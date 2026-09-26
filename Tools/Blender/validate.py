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
    'palm':   ('z', 550.0, 1450.0),
    'tree':   ('z', 450.0, 3900.0),   # el gigante de dosel llega a 25-35 m (+ copa)
    'shrub':  ('z', 15.0, 900.0),     # bambú denso hasta 8 m
    'rock':   ('max', 12.0, 170.0),
    'grass':  ('z', 4.0, 170.0),      # hierba alta hasta 1,5 m
    'debris': ('max', 15.0, 650.0),   # troncos caídos tumbados, cocos sueltos
}

DEGENERATE_AREA_EPS = 1e-8  # m^2

# ---------------------------------------------------------------------------
# Kit de props narrativos (Tools/Blender/props/run_props.py), validado con las
# mismas comprobaciones 1/2/4/5 de arriba (presupuesto, geometría degenerada,
# color de vértice, materiales estables); el rango plausible se agrupa por
# `entry['group']` en vez de `entry['category']` y no se exige watertight
# (no hay ninguna categoría de prop pensada como volumen cerrado).
# ---------------------------------------------------------------------------
EXPORT_DIR_PROPS = os.path.join(REPO_ROOT, 'Art', 'Export', 'Props')
MANIFEST_PATH_PROPS = os.path.join(EXPORT_DIR_PROPS, 'manifest.json')

ALLOWED_MATERIALS_PROPS = {'M_Wood', 'M_Metal', 'M_Fabric', 'M_Stone', 'M_Glass', 'M_Paper', 'M_Leaf'}

# Rango plausible por grupo narrativo, dimensión mayor del bounding box en cm
# (los grupos mezclan piezas sueltas pequeñas con estructuras grandes, así
# que el rango es deliberadamente ancho por grupo).
PLAUSIBLE_RANGES_CM_PROPS = {
    'Albatros':        (100.0, 1200.0),   # restos sueltos ~1 m .. avión entero ~11-12 m
    'Faro':            (30.0, 1400.0),    # escombro suelto .. torre entera
    'Halden':          (30.0, 900.0),     # herramienta de mano .. caseta con mástil
    'BrujulaEstelar':  (40.0, 600.0),
    'Petroglifos':     (20.0, 200.0),
    'Marae':           (40.0, 700.0),
    'Pecio':           (30.0, 800.0),
    'Baliza':          (30.0, 300.0),
    'ObjetosPequenos': (2.0, 90.0),
    'Embarcaciones':   (60.0, 500.0),
    'Construccion':    (2.0, 300.0),
    # kit modular (rejilla de 2 m): pilote de 0.6 m .. escalera de 4 m y
    # tejados de 2 m + alero
    'KitPalma':        (40.0, 450.0),
    'KitBambu':        (40.0, 450.0),
    'KitMadera':       (40.0, 450.0),
    'KitPiedra':       (40.0, 450.0),
    'MobiliarioBase':  (60.0, 450.0),    # vitrina ~1 m .. muelle 4 m
    'RuinasMarae':     (80.0, 900.0),    # enlosado 2 m .. marae grande ~8,5 m
    'RuinasTallas':    (80.0, 700.0),    # losa grabada 1,4 m .. canoa doble 6,5 m
    'Tesoros':         (5.0, 200.0),     # anzuelo ~10 cm .. remo ceremonial ~1,7 m
}


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


def main_props():
    if not os.path.isfile(MANIFEST_PATH_PROPS):
        print(f"[validate] ERROR: no existe {MANIFEST_PATH_PROPS}. Ejecuta run_props.py primero.")
        sys.exit(1)

    with open(MANIFEST_PATH_PROPS, 'r', encoding='utf-8') as f:
        manifest = json.load(f)

    results = []
    for entry in manifest['meshes']:
        _reset_scene()
        fpath = os.path.join(EXPORT_DIR_PROPS, entry['file'])
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

        me.calc_loop_triangles()
        tris = len(me.loop_triangles)
        lo, hi = entry['triangle_budget']['min'], entry['triangle_budget']['max']
        if hi > 0 and not (lo <= tris <= hi):
            problems.append(f'triángulos {tris} fuera de presupuesto [{lo}, {hi}]')

        bad_faces = _check_degenerate(obj)
        if bad_faces:
            problems.append(f'{bad_faces} caras con área ~0 (geometría degenerada)')

        v_min, v_max = PLAUSIBLE_RANGES_CM_PROPS[entry['group']]
        dims_cm = (obj.dimensions.x * 100.0, obj.dimensions.y * 100.0, obj.dimensions.z * 100.0)
        value = max(dims_cm)
        if not (v_min <= value <= v_max):
            problems.append(f'dimensión máxima={value:.1f} cm fuera de rango plausible [{v_min}, {v_max}]')

        if 'Col' not in me.color_attributes:
            problems.append('falta el atributo de color de vértice «Col»')

        slot_names = {m.name.split('.')[0] for m in me.materials if m is not None}
        extra = slot_names - ALLOWED_MATERIALS_PROPS
        if extra:
            problems.append(f'slots de material fuera del kit estable de props: {sorted(extra)}')

        results.append((entry['name'], problems, tris, dims_cm))

    print('\n[validate] ==== Resultado (props) ====')
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
    print(f'[validate] {total - n_fail}/{total} props en verde.')
    if n_fail:
        print('[validate] VALIDACIÓN DE PROPS FALLIDA')
        sys.exit(1)
    print('[validate] VALIDACIÓN DE PROPS OK')


# ---------------------------------------------------------------------------
# Kit de fauna (Tools/Blender/run_animals.py). A diferencia de vegetación y
# props (una malla = un fichero), cada especie es un kit de piezas: aquí no
# se valida «una malla» sino la especie entera, reimportando TODAS sus
# piezas y comprobando además la propia coherencia del rig (padres que
# existen, una única raíz, roles reconocidos) antes de las comprobaciones
# de geometría 1/2/4/5 de siempre, ya por pieza. La dimensión total se
# RECALCULA desde las piezas reimportadas (no se confía en el
# «dimensions_cm» que anotó run_animals.py) para que un bug de export o de
# pivote (como el eje X invertido que se coló en el primer intento del kit
# de peces) lo detecte esta validación en vez de colarse silencioso.
# ---------------------------------------------------------------------------
FAUNA_DIR = os.path.join(EXPORT_DIR, 'Fauna')
FAUNA_MANIFEST_PATH = os.path.join(FAUNA_DIR, 'animals.json')

FAUNA_ALLOWED_ROLES = {
    'body', 'shell', 'bell', 'head', 'neck', 'jaw', 'beak', 'leg', 'tail',
    'spine', 'wing', 'fin', 'flipper', 'tentacle', 'claw', 'ear', 'eye',
}

# Tolerancia de la dimensión total recalculada frente a la anotada en el
# manifest: el máximo entre 2 cm y un 5% (los bichos pequeños necesitan un
# suelo absoluto; los grandes, uno relativo).
DIMENSION_TOLERANCE_CM = 2.0
DIMENSION_TOLERANCE_PCT = 0.05


def _accumulate_bbox(bbox_min, bbox_max, obj, piv):
    """Actualiza (in-place) los acumuladores min/max con los vértices de
    «obj» desplazados por su pivote absoluto «piv» (cm). Se llama pieza a
    pieza, ANTES del siguiente _reset_scene(): guardar el bpy.types.Object
    para leerlo más tarde no vale — read_factory_settings libera sus datos y
    cualquier referencia Python queda con un StructRNA muerto."""
    for v in obj.data.vertices:
        x = v.co.x * 100.0 + piv[0]
        y = v.co.y * 100.0 + piv[1]
        z = v.co.z * 100.0 + piv[2]
        bbox_min[0] = min(bbox_min[0], x)
        bbox_min[1] = min(bbox_min[1], y)
        bbox_min[2] = min(bbox_min[2], z)
        bbox_max[0] = max(bbox_max[0], x)
        bbox_max[1] = max(bbox_max[1], y)
        bbox_max[2] = max(bbox_max[2], z)


def main_animals():
    if not os.path.isfile(FAUNA_MANIFEST_PATH):
        print(f"[validate] ERROR: no existe {FAUNA_MANIFEST_PATH}. Ejecuta run_animals.py primero.")
        sys.exit(1)

    with open(FAUNA_MANIFEST_PATH, 'r', encoding='utf-8') as f:
        manifest = json.load(f)

    results = []
    for entry in manifest['species']:
        species = entry['species']
        pieces = entry['pieces']
        names = {p['name'] for p in pieces}
        problems = []

        # 1. coherencia del rig: exactamente una raíz, todo padre existe
        roots = sum(1 for p in pieces if p['parent'] is None)
        if roots != 1:
            problems.append(f'{roots} piezas raíz (se esperaba exactamente 1)')
        dangling = [p['name'] for p in pieces
                    if p['parent'] is not None and p['parent'] not in names]
        if dangling:
            problems.append(f'piezas con padre inexistente: {dangling}')

        bad_roles = {p['role'] for p in pieces} - FAUNA_ALLOWED_ROLES
        if bad_roles:
            problems.append(f'roles desconocidos: {sorted(bad_roles)}')

        # 2. por pieza: reimporta y comprueba geometría/color/material, y
        # acumula la posición absoluta (local_offset_cm de la raíz al hijo)
        # para recalcular la caja del bounding box de la especie entera.
        abs_by_name = {}
        bbox_min = [float('inf')] * 3
        bbox_max = [float('-inf')] * 3
        total_tris = 0
        expected_material = f'M_Fauna_{species}'
        for p in pieces:
            parent = p['parent']
            local = p['local_offset_cm']
            if parent is None:
                abs_by_name[p['name']] = tuple(local)
            elif parent in abs_by_name:
                pa = abs_by_name[parent]
                abs_by_name[p['name']] = tuple(pa[i] + local[i] for i in range(3))

            _reset_scene()
            fpath = os.path.join(FAUNA_DIR, p['file'])
            if not os.path.isfile(fpath):
                problems.append(f"pieza «{p['name']}»: fichero FBX no encontrado")
                continue
            meshes = _import_fbx(fpath)
            if len(meshes) != 1:
                problems.append(f"pieza «{p['name']}»: se esperaba 1 malla, hay {len(meshes)}")
                continue
            obj = meshes[0]
            me = obj.data
            me.calc_loop_triangles()
            total_tris += len(me.loop_triangles)

            bad_faces = _check_degenerate(obj)
            if bad_faces:
                problems.append(f"pieza «{p['name']}»: {bad_faces} caras degeneradas")
            if 'Col' not in me.color_attributes:
                problems.append(f"pieza «{p['name']}»: falta el atributo de color «Col»")
            slot_names = {m.name.split('.')[0] for m in me.materials if m is not None}
            if slot_names != {expected_material}:
                problems.append(f"pieza «{p['name']}»: material {sorted(slot_names)} "
                                 f"(se esperaba solo {{'{expected_material}'}})")

            piv = abs_by_name.get(p['name'], (0.0, 0.0, 0.0))
            _accumulate_bbox(bbox_min, bbox_max, obj, piv)

        # 3. presupuesto de triángulos total de la especie
        lo, hi = entry['triangle_budget']['min'], entry['triangle_budget']['max']
        if hi > 0 and not (lo <= total_tris <= hi):
            problems.append(f'triángulos totales {total_tris} fuera de presupuesto [{lo}, {hi}]')

        # 4. dimensión recalculada frente a la anotada por run_animals.py
        if bbox_min[0] == float('inf'):
            dims_cm = (0.0, 0.0, 0.0)
        else:
            dims_cm = tuple(bbox_max[i] - bbox_min[i] for i in range(3))
        recorded = entry['dimensions_cm']
        for axis, value in zip('xyz', dims_cm):
            rec = recorded[axis]
            tol = max(DIMENSION_TOLERANCE_CM, rec * DIMENSION_TOLERANCE_PCT)
            if abs(value - rec) > tol:
                problems.append(f'dimensión {axis} recalculada {value:.1f} cm '
                                 f'difiere de la anotada {rec} cm (tolerancia {tol:.1f})')

        results.append((species, problems, total_tris, dims_cm, len(pieces)))

    print('\n[validate] ==== Resultado (fauna) ====')
    n_fail = 0
    for species, problems, total_tris, dims_cm, piece_count in results:
        if problems:
            n_fail += 1
            print(f'  FALLO {species}: ' + '; '.join(problems))
        else:
            print(f'  OK    {species}: {total_tris} tris en {piece_count} piezas, '
                  f'dims_cm=({dims_cm[0]:.1f}, {dims_cm[1]:.1f}, {dims_cm[2]:.1f})')

    total = len(results)
    print(f'[validate] {total - n_fail}/{total} especies en verde.')
    if n_fail:
        print('[validate] VALIDACIÓN DE FAUNA FALLIDA')
        sys.exit(1)
    print('[validate] VALIDACIÓN DE FAUNA OK')


if __name__ == '__main__':
    main()
    main_props()
    main_animals()
