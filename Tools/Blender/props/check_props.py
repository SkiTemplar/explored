"""
check_props.py — criterio de aceptación por script de uno o varios módulos
de props YA exportados por run_props.py, y lámina de revisión de su grupo.

    blender -b --factory-startup --python Tools/Blender/props/check_props.py -- \
        --modules=albatros [--render] [--review-dir=<carpeta>]

Por cada variante del módulo reimporta su FBX (Art/Export/Props/<Grupo>/) y
comprueba:
    - triángulos dentro de `tri_budget`,
    - bounding box dentro de `bbox_cm` (x, y, z) y, si se declara,
      `z_range_cm` (z mínima y máxima: posición del origen),
    - normales coherentes: ninguna arista con dos caras de giro igual y
      ninguna cara invertida respecto a recalc_face_normals (por islas),
    - sin geometría suelta ni degenerada: vértices/aristas sin cara, caras
      de área ~0,
    - atributo «Col» y slots del kit estable de props.
Sale con código 1 si algo falla. Con --render escribe además la lámina
Art/Export/Props/preview_<Grupo>.png (la misma de render_preview.py) y, con
--review-dir, tres vistas cercanas por malla para la revisión visual.
"""

import importlib
import json
import math
import os
import sys

import bpy

import bmesh
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
BLENDER_DIR = os.path.dirname(HERE)
for _p in (os.path.join(BLENDER_DIR, 'lib'), HERE, BLENDER_DIR):
    if _p not in sys.path:
        sys.path.insert(0, _p)

import run_props as RUN  # noqa: E402

import render_preview as RP  # noqa: E402
import validate as V  # noqa: E402

DEGENERATE_AREA_EPS = 1e-8


def _args():
    opts = dict(modules='', render=False, review_dir='')
    if '--' in sys.argv:
        for a in sys.argv[sys.argv.index('--') + 1:]:
            if a == '--render':
                opts['render'] = True
            elif a.startswith('--') and '=' in a:
                k, v = a[2:].split('=', 1)
                opts[k.replace('-', '_')] = v
    return opts


def _bounds(obj):
    ws = [obj.matrix_world @ v.co for v in obj.data.vertices]
    lo = Vector((min(w.x for w in ws), min(w.y for w in ws), min(w.z for w in ws)))
    hi = Vector((max(w.x for w in ws), max(w.y for w in ws), max(w.z for w in ws)))
    return lo * 100.0, hi * 100.0


def _winding_problems(obj):
    """(aristas con giro incoherente, caras invertidas tras recalc)."""
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bad_edges = 0
    for e in bm.edges:
        if len(e.link_faces) != 2:
            continue
        f0, f1 = e.link_faces
        l0 = next(lp for lp in f0.loops if lp.edge == e)
        l1 = next(lp for lp in f1.loops if lp.edge == e)
        if l0.vert == l1.vert:
            bad_edges += 1
    before = [f.normal.copy() for f in bm.faces]
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.normal_update()
    flipped = sum(1 for f, n in zip(bm.faces, before, strict=True) if f.normal.dot(n) < 0.0)
    bm.free()
    return bad_edges, flipped


def _check(entry, variant):
    problems = []
    V._reset_scene()
    meshes = V._import_fbx(os.path.join(RUN.EXPORT_DIR, entry['file']))
    if len(meshes) != 1:
        return [f'{len(meshes)} mallas al reimportar'], {}
    obj = meshes[0]
    me = obj.data
    me.calc_loop_triangles()
    tris = len(me.loop_triangles)
    lo, hi = variant.get('tri_budget', (0, 10 ** 9))
    if not lo <= tris <= hi:
        problems.append(f'{tris} tris fuera de [{lo}, {hi}]')
    bmin, bmax = _bounds(obj)
    dims = bmax - bmin
    for axis, (a, b) in variant.get('bbox_cm', {}).items():
        v = dims['xyz'.index(axis)]
        if not a <= v <= b:
            problems.append(f'bbox {axis}={v:.1f} cm fuera de [{a}, {b}]')
    zr = variant.get('z_range_cm')
    if zr and not (zr[0] - 1.0 <= bmin.z and bmax.z <= zr[1] + 1.0):
        problems.append(f'z [{bmin.z:.1f}, {bmax.z:.1f}] fuera de {zr}')
    used = {v for p in me.polygons for v in p.vertices}
    loose_v = len(me.vertices) - len(used)
    face_edges = {k for p in me.polygons for k in p.edge_keys}
    loose_e = sum(1 for e in me.edges if e.key not in face_edges)
    degenerate = sum(1 for p in me.polygons if p.area < DEGENERATE_AREA_EPS)
    bad_edges, flipped = _winding_problems(obj)
    for n, label in ((loose_v, 'vértices sueltos'), (loose_e, 'aristas sueltas'),
                     (degenerate, 'caras degeneradas'), (bad_edges, 'aristas con giro incoherente'),
                     (flipped, 'caras invertidas')):
        if n:
            problems.append(f'{n} {label}')
    if 'Col' not in me.color_attributes:
        problems.append('sin «Col»')
    slots = {m.name.split('.')[0] for m in me.materials if m is not None}
    if slots - V.ALLOWED_MATERIALS_PROPS:
        problems.append(f'slots fuera del kit: {sorted(slots - V.ALLOWED_MATERIALS_PROPS)}')
    info = dict(tris=tris, dims=tuple(round(d, 1) for d in dims),
                zmin=round(bmin.z, 1), zmax=round(bmax.z, 1), slots=entry['material_slots'])
    return problems, info


def _review_views(entry, out_dir):
    """Tres vistas cercanas (3/4 proa, 3/4 popa alta, perfil) de una malla."""
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    obj = RP._import_and_fix_materials_props(entry)
    bpy.context.view_layer.update()
    x0, x1, y0, y1, z0, z1 = RP._bounds_world(obj)
    centre = Vector(((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2))
    size = max(x1 - x0, y1 - y0, z1 - z0)
    bpy.ops.mesh.primitive_plane_add(size=size * 4, location=(centre.x, centre.y, min(z0, 0.0)))
    world = bpy.data.worlds.new('W')
    scene.world = world
    world.use_nodes = True
    bg = next(n for n in world.node_tree.nodes if n.type == 'BACKGROUND')
    bg.inputs['Color'].default_value = (0.68, 0.78, 0.88, 1.0)
    bg.inputs['Strength'].default_value = 0.6
    bpy.ops.object.light_add(type='SUN', location=(0, 0, 10))
    sun = bpy.context.object
    sun.data.energy = 2.0
    sun.rotation_euler = (math.radians(45), math.radians(10), math.radians(-35))
    bpy.ops.object.camera_add()
    cam = bpy.context.object
    scene.camera = cam
    cam.data.lens = 40
    views = dict(bow=(35.0, 18.0), stern=(215.0, 30.0), side=(90.0, 4.0))
    for tag, (az, el) in views.items():
        a, e = math.radians(az), math.radians(el)
        d = size * 1.25
        cam.location = centre + Vector((math.cos(a) * math.cos(e), -math.sin(a) * math.cos(e),
                                        math.sin(e))) * d
        RP._point_camera(cam, centre)
        out = os.path.join(out_dir, f"{entry['name']}_{tag}.png")
        RP._render_still(scene, out, 1000, 750, samples=32, raytracing=False)


def main():
    opts = _args()
    mods = [m for m in opts['modules'].split(',') if m]
    with open(os.path.join(RUN.EXPORT_DIR, 'manifest.json'), encoding='utf-8') as f:
        by_name = {e['name']: e for e in json.load(f)['meshes']}
    groups = dict(RUN.MODULE_GROUPS)
    n_fail, touched = 0, []
    for mod_name in mods:
        mod = importlib.import_module(mod_name)
        for variant in mod.VARIANTS:
            name = 'SM_' + variant['name']
            entry = by_name.get(name)
            if entry is None:
                print(f'[check_props] FALLO {name}: no está en el manifest')
                n_fail += 1
                continue
            touched.append(entry)
            problems, info = _check(entry, variant)
            if problems:
                n_fail += 1
                print(f'[check_props] FALLO {name}: ' + '; '.join(problems) + f' {info}')
            else:
                print(f'[check_props] OK    {name}: {info}')
        groups.setdefault(mod_name, mod_name)
    if opts['render']:
        for group in sorted({e['group'] for e in touched}):
            entries = [e for e in touched if e['group'] == group]
            height = RP.GROUP_TARGET_HEIGHT_PROPS.get(group, 2.2)
            RP._render_props_group(entries, height,
                                   os.path.join(RUN.EXPORT_DIR, f'preview_{group}.png'))
    if opts['review_dir']:
        os.makedirs(opts['review_dir'], exist_ok=True)
        for entry in touched:
            _review_views(entry, opts['review_dir'])
    print(f'[check_props] {len(touched) - n_fail}/{len(touched)} en verde')
    sys.exit(1 if n_fail else 0)


if __name__ == '__main__':
    main()
