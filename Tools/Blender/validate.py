"""
validate.py — valida el kit de mallas ya exportado.

Se ejecuta dentro de Blender 5.2 en modo headless, DESPUÉS de run_all.py:

    "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" ^
        -b --factory-startup --python Tools\\Blender\\validate.py

Lee Art/Export/Meshes/manifest.json, reimporta cada FBX en una escena vacía
y comprueba:
    1. Que el número de triángulos está dentro del presupuesto anotado en
       el manifest (encargo §4).
    2. Que no hay geometría degenerada (triángulos de área ~0).
    3. Que las dimensiones del bounding box son plausibles para su
       categoría (encargo §5: p.ej. palmera 6-14 m de altura).
    4. Que la malla tiene el atributo de color de vértice «Col».
    5. Que los slots de material solo usan los 4 nombres estables del kit.
    6. Para rocas (únicas mallas pensadas como volumen cerrado): que la
       malla es 2-manifold, es decir, «watertight» cuando corresponde.

Termina con sys.exit(1) si hay algún FALLO, para que se pueda encadenar en
un script de CI; imprime un resumen legible en cualquier caso.
"""

import json
import math
import os
import sys

import bmesh
import bpy

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
EXPORT_DIR = os.path.join(REPO_ROOT, 'Art', 'Export', 'Meshes')
MANIFEST_PATH = os.path.join(EXPORT_DIR, 'manifest.json')

ALLOWED_MATERIALS = {'M_Bark', 'M_Leaf', 'M_Rock', 'M_Grass'}

# Rangos plausibles de dimensiones por categoría, en cm. El eje relevante es
# Z (altura) para todo salvo la roca, donde se usa la dimensión mayor.
PLAUSIBLE_RANGES_CM = {
    'palm':  ('z', 550.0, 1450.0),
    'tree':  ('z', 450.0, 2500.0),
    'shrub': ('z', 15.0, 420.0),
    'rock':  ('max', 12.0, 170.0),
    'grass': ('z', 4.0, 70.0),
}

DEGENERATE_AREA_EPS = 1e-8  # m^2


def _import_fbx(filepath):
    before = set(bpy.data.objects.keys())
    bpy.ops.import_scene.fbx(filepath=filepath)
    after = set(bpy.data.objects.keys())
    new_names = after - before
    meshes = [bpy.data.objects[n] for n in new_names if bpy.data.objects[n].type == 'MESH']
    return meshes


def _reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def _check_degenerate(obj):
    me = obj.data
    me.calc_loop_triangles()
    bad = 0
    for poly in me.polygons:
        if poly.area < DEGENERATE_AREA_EPS:
            bad += 1
    return bad


def _check_manifold(obj):
    """Devuelve (n_bordes_no_manifold, n_vertices_no_manifold)."""
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.edges.ensure_lookup_table()
    non_manifold_edges = sum(1 for e in bm.edges if not e.is_manifold)
    non_manifold_verts = sum(1 for v in bm.verts if not v.is_manifold)
    bm.free()
    return non_manifold_edges, non_manifold_verts


def main():
    if not os.path.isfile(MANIFEST_PATH):
        print(f"[validate] ERROR: no existe {MANIFEST_PATH}. Ejecuta run_all.py primero.")
        sys.exit(1)

    with open(MANIFEST_PATH, 'r', encoding='utf-8') as f:
        manifest = json.load(f)

    results = []
    for entry in manifest['meshes']:
        _reset_scene()
        fpath = os.path.join(EXPORT_DIR, entry['file'])
        problems = []

        if not os.path.isfile(fpath):
            problems.append('fichero FBX no encontrado')
            results.append((entry['name'], problems))
            continue

        meshes = _import_fbx(fpath)
        if len(meshes) != 1:
            problems.append(f'se esperaba 1 objeto de malla al reimportar, hay {len(meshes)}')
            results.append((entry['name'], problems))
            continue
        obj = meshes[0]
        me = obj.data

        # 1. presupuesto de triángulos
        me.calc_loop_triangles()
        tris = len(me.loop_triangles)
        lo, hi = entry['triangle_budget']['min'], entry['triangle_budget']['max']
        if hi > 0 and not (lo <= tris <= hi):
            problems.append(f'triángulos {tris} fuera de presupuesto [{lo}, {hi}]')

        # 2. geometría degenerada
        bad_faces = _check_degenerate(obj)
        if bad_faces:
            problems.append(f'{bad_faces} caras con área ~0 (geometría degenerada)')

        # 3. dimensiones plausibles
        axis, v_min, v_max = PLAUSIBLE_RANGES_CM[entry['category']]
        # obj.dimensions viene en metros: el FBX se exportó con
        # apply_unit_scale, y al reimportar Blender reconvierte a metros.
        dims_cm = (obj.dimensions.x * 100.0, obj.dimensions.y * 100.0, obj.dimensions.z * 100.0)
        value = max(dims_cm) if axis == 'max' else dims_cm['xyz'.index(axis)]
        if not (v_min <= value <= v_max):
            problems.append(f'dimensión {axis}={value:.1f} cm fuera de rango plausible [{v_min}, {v_max}]')

        # 4. color de vértice
        if 'Col' not in me.color_attributes:
            problems.append('falta el atributo de color de vértice «Col»')

        # 5. materiales estables
        slot_names = {m.name.split('.')[0] for m in me.materials if m is not None}
        extra = slot_names - ALLOWED_MATERIALS
        if extra:
            problems.append(f'slots de material fuera del kit estable: {sorted(extra)}')

        # 6. watertight solo exigido a rocas (volumen cerrado)
        if entry['category'] == 'rock':
            nm_edges, nm_verts = _check_manifold(obj)
            if nm_edges or nm_verts:
                problems.append(f'no es watertight: {nm_edges} aristas y {nm_verts} vértices no-manifold')

        results.append((entry['name'], problems, tris, dims_cm))

    print('\n[validate] ==== Resultado ====')
    n_fail = 0
    for row in results:
        name = row[0]
        problems = row[1]
        if problems:
            n_fail += 1
            print(f'  FALLO {name}: ' + '; '.join(problems))
        else:
            tris, dims_cm = row[2], row[3]
            print(f'  OK    {name}: {tris} tris, dims_cm=({dims_cm[0]:.1f}, {dims_cm[1]:.1f}, {dims_cm[2]:.1f})')

    total = len(results)
    print(f'[validate] {total - n_fail}/{total} mallas en verde.')
    if n_fail:
        print('[validate] VALIDACIÓN FALLIDA')
        sys.exit(1)
    print('[validate] VALIDACIÓN OK')


if __name__ == '__main__':
    main()
