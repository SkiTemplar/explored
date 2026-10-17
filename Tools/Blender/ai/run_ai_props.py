"""
run_ai_props.py — importa, estiliza y exporta los props generados con el
pipeline de IA (Canva -> Hunyuan3D-2 local -> stylize_prop), y anota el
resultado en ai_overrides.json para que run_props.py los use en vez de
reconstruir la version procedural.

Se ejecuta dentro de Blender 5.2 en modo headless:

    "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" ^
        -b --factory-startup --python Tools\\Blender\\ai\\run_ai_props.py

Requisitos por prop (ver ai_props_config.py): la referencia ya ampliada a
512px en Art/Reference/props/<name>_512.png (no se usa aqui, solo por
trazabilidad) y la malla cruda de Hunyuan3D en
Art/Generated/Props3D/<name>.glb. Un prop sin .glb todavia se salta con un
aviso: permite ir generando el kit de a poco sin romper una pasada previa.

Para regenerar solo algunos: blender.exe -b --python run_ai_props.py -- --only=Boat_Limon,ItemCoconut
"""

import json
import os
import sys

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
BLENDER_DIR = os.path.join(REPO_ROOT, 'Tools', 'Blender')
LIB_DIR = os.path.join(BLENDER_DIR, 'lib')
PROPS_DIR = os.path.join(BLENDER_DIR, 'props')
AI_DIR = os.path.join(BLENDER_DIR, 'ai')
GENERATED_DIR = os.path.join(REPO_ROOT, 'Art', 'Generated', 'Props3D')
EXPORT_DIR = os.path.join(REPO_ROOT, 'Art', 'Export', 'Props')
OVERRIDES_PATH = os.path.join(AI_DIR, 'ai_overrides.json')

for _p in (LIB_DIR, PROPS_DIR, AI_DIR):
    if _p not in sys.path:
        sys.path.insert(0, _p)

import bpy  # noqa: E402
import common as C  # noqa: E402
import _materials as M  # noqa: E402
import stylize  # noqa: E402
from ai_props_config import AI_PROPS  # noqa: E402


def _parse_only_filter():
    argv = sys.argv
    if '--' not in argv:
        return None
    after = argv[argv.index('--') + 1:]
    for a in after:
        if a.startswith('--only='):
            return set(a.split('=', 1)[1].split(','))
    return None


def _import_glb(path):
    before = set(bpy.data.objects.keys())
    bpy.ops.import_scene.gltf(filepath=path)
    after = set(bpy.data.objects.keys())
    new_objs = [bpy.data.objects[n] for n in (after - before)]
    meshes = [o for o in new_objs if o.type == 'MESH']
    if not meshes:
        raise RuntimeError(f'sin mallas al importar {path}')
    for o in new_objs:
        if o.type != 'MESH':
            bpy.data.objects.remove(o, do_unlink=True)
    return C.join_objects(meshes, 'AIImport') if len(meshes) > 1 else meshes[0]


def _rescale_and_ground(obj, target_size_m):
    """Escala uniforme por la dimension mayor del bounding box y mueve el
    pivote a la base (min Z = 0, centrado en X/Y): mismas convenciones que
    los props procedurales (base apoyada en el suelo del juego)."""
    bpy.context.view_layer.update()
    verts = obj.data.vertices
    xs = [v.co.x for v in verts]
    ys = [v.co.y for v in verts]
    zs = [v.co.z for v in verts]
    size = max(max(xs) - min(xs), max(ys) - min(ys), max(zs) - min(zs), 1e-5)
    scale = target_size_m / size
    for v in verts:
        v.co.x = (v.co.x - (max(xs) + min(xs)) / 2.0) * scale
        v.co.y = (v.co.y - (max(ys) + min(ys)) / 2.0) * scale
        v.co.z = (v.co.z - min(zs)) * scale
    obj.data.update()


def main():
    only = _parse_only_filter()
    if os.path.isfile(OVERRIDES_PATH):
        with open(OVERRIDES_PATH, 'r', encoding='utf-8') as f:
            overrides = json.load(f).get('overrides', {})
    else:
        overrides = {}

    for cfg in AI_PROPS:
        name = cfg['name']
        if only is not None and name not in only:
            continue
        glb_path = os.path.join(GENERATED_DIR, f'{name}.glb')
        if not os.path.isfile(glb_path):
            print(f'[run_ai_props] {name}: sin {glb_path}, se salta (aun no generado)')
            continue

        C.reset_scene()
        obj = _import_glb(glb_path)
        _rescale_and_ground(obj, cfg['target_size_m'])

        rnd = C.seeded_rng(sum(map(ord, name)))
        stats = stylize.stylize_prop(
            obj, cfg['base_color'], cfg['rules'],
            target_faces=cfg['target_faces'], rnd=rnd)
        M.assign(obj, [cfg['material_slot']])
        C.shade_smooth_auto(obj, angle_deg=35.0)
        C.add_basic_uv(obj)

        out_dir = os.path.join(EXPORT_DIR, cfg['group'])
        os.makedirs(out_dir, exist_ok=True)
        mesh_name = f'SM_{name}'
        fpath = os.path.join(out_dir, mesh_name + '.fbx')
        C.export_fbx(fpath, [obj])

        tris = C.triangle_count(obj)
        dims = C.dimensions_cm(obj)
        overrides[name] = dict(
            file=os.path.relpath(fpath, EXPORT_DIR).replace('\\', '/'),
            triangles=tris,
            dimensions_cm=dict(x=round(dims[0], 1), y=round(dims[1], 1), z=round(dims[2], 1)),
            material_slots=[cfg['material_slot']],
        )
        print(f"[run_ai_props] {name}: {tris} tris (removed_verts={stats['removed_verts']}), "
              f"dims_cm={tuple(round(d, 1) for d in dims)} -> {fpath}")

    os.makedirs(AI_DIR, exist_ok=True)
    with open(OVERRIDES_PATH, 'w', encoding='utf-8') as f:
        json.dump(dict(generated_by='Tools/Blender/ai/run_ai_props.py', overrides=overrides),
                  f, indent=2, ensure_ascii=False)
    print(f'[run_ai_props] overrides escritos en {OVERRIDES_PATH} ({len(overrides)} props)')


if __name__ == '__main__':
    main()
