"""
run_animals.py — genera el kit de fauna completo de «Explored» (biblia §6).

Se ejecuta dentro de Blender 5.2 en modo headless:

    "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" ^
        -b --factory-startup --python Tools\\Blender\\run_animals.py

A diferencia de run_all.py (vegetación: una malla = un FBX), cada especie de
fauna es un KIT DE PIEZAS: tantas mallas independientes como huesos del rig
(cuerpo/caparazón, cabeza, patas, cola, alas, aletas...), cada una con el
origen de objeto en el pivote de su articulación y SIN NINGUNA rotación de
objeto (la nota de Tools/Blender/animals/rig.py). Por cada variante:

    1. Construye las piezas (mod.build(variant)).
    2. Genera un LOD1 opcional (decimate) para las piezas de bulto pesadas.
    3. Exporta cada pieza (y su LOD1 si existe) a su propio FBX en
       Art/Export/Meshes/Fauna/<Especie>/SM_<Especie>_<Pieza>[_LOD1].fbx.
    4. Anota en animals.json, por pieza: nombre, «role» (bone lógico:
       body/root, head, neck, jaw, leg, tail, wing, fin, flipper, tentacle,
       spine, ear, eye, beak, shell, bell), padre y «local_offset_cm» — el
       offset de traslación PURA respecto al padre (sin rotación posible en
       este rig), tal y como pide la nota de rig.py: pivot_cm que build()
       devuelve es absoluto respecto a la raíz de la especie, y aquí se
       resta el pivote del padre para dejar el offset local que el C++
       necesita para reconstruir la jerarquía. «pivot_cm» (absoluto) se deja
       también, solo como referencia de depuración.

Fuera de alcance a propósito: luciérnagas, mariposas, abejas, libélulas y
peces voladores (biblia §6, "fauna ambiental") — el diseño los resuelve con
sistemas de partículas/bandadas (boids), no con mallas rigged individuales.
"""

import importlib
import json
import os
import sys

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
BLENDER_DIR = os.path.join(REPO_ROOT, 'Tools', 'Blender')
LIB_DIR = os.path.join(BLENDER_DIR, 'lib')
ANIMALS_DIR = os.path.join(BLENDER_DIR, 'animals')
EXPORT_DIR = os.path.join(REPO_ROOT, 'Art', 'Export', 'Meshes', 'Fauna')

for _p in (LIB_DIR, ANIMALS_DIR):
    if _p not in sys.path:
        sys.path.insert(0, _p)

import common as C  # noqa: E402
import rig  # noqa: E402

MODULE_NAMES = [
    'quadrupeds', 'arthropods', 'reptiles', 'serpent', 'birds', 'bat',
    'turtle', 'fish', 'cephalopod', 'jellyfish',
]

# Presupuesto orientativo de triángulos TOTAL por especie (suma de todas sus
# piezas, LOD0). Orientativo aquí (solo se avisa); validate.py es quien lo
# hace cumplir de verdad antes de dar el kit por bueno.
TRIANGLE_BUDGET_BY_CATEGORY = {
    'quadruped': (450, 7000),
    'arthropod': (500, 4000),
    'cephalopod': (500, 4000),
    'jellyfish': (150, 2500),
    'reptile': (400, 7000),
    'serpent': (300, 3000),
    'bird': (400, 3500),
    'bat': (300, 2200),
    'turtle': (400, 4500),
    'fish': (100, 6000),
}

LOD1_MIN_TRIS = 140
LOD1_RATIO = 0.5


def _import_or_reload(name):
    if name in sys.modules:
        return importlib.reload(sys.modules[name])
    return importlib.import_module(name)


def _species_bbox_cm(pieces, abs_by_name):
    xs, ys, zs = [], [], []
    for p in pieces:
        piv = abs_by_name[p['name']]
        for v in p['obj'].data.vertices:
            xs.append(v.co.x * 100.0 + piv[0])
            ys.append(v.co.y * 100.0 + piv[1])
            zs.append(v.co.z * 100.0 + piv[2])
    if not xs:
        return (0.0, 0.0, 0.0)
    return (max(xs) - min(xs), max(ys) - min(ys), max(zs) - min(zs))


def _export_piece(species, out_dir, piece):
    fname = f"SM_{species}_{piece['name']}.fbx"
    fpath = os.path.join(out_dir, fname)
    C.export_fbx(fpath, [piece['obj']])
    return dict(
        file=os.path.relpath(fpath, EXPORT_DIR).replace('\\', '/'),
        triangles=C.triangle_count(piece['obj']),
    )


def main():
    os.makedirs(EXPORT_DIR, exist_ok=True)
    species_list = []

    for mod_name in MODULE_NAMES:
        mod = _import_or_reload(mod_name)

        for variant in mod.VARIANTS:
            C.reset_scene()
            result = mod.build(variant)
            species = result['species']
            pieces = result['pieces']
            locomotion = result['locomotion']

            out_dir = os.path.join(EXPORT_DIR, species)
            os.makedirs(out_dir, exist_ok=True)

            abs_by_name = {p['name']: p['pivot_cm'] for p in pieces}
            dims_cm = _species_bbox_cm(pieces, abs_by_name)

            piece_entries = []
            total_tris = 0
            for p in pieces:
                exported = _export_piece(species, out_dir, p)
                total_tris += exported['triangles']

                parent = p['parent']
                if parent is None:
                    local_offset = p['pivot_cm']
                else:
                    parent_abs = abs_by_name[parent]
                    local_offset = tuple(round(a - b, 4) for a, b in
                                          zip(p['pivot_cm'], parent_abs))

                entry = dict(
                    name=p['name'], parent=parent, role=p['role'],
                    local_offset_cm=list(local_offset),
                    pivot_cm=list(p['pivot_cm']),
                    file=exported['file'], triangles=exported['triangles'],
                )

                lod = rig.make_lod1(p, ratio=LOD1_RATIO, min_tris=LOD1_MIN_TRIS)
                if lod is not None:
                    lod_exported = _export_piece(species, out_dir, lod)
                    entry['lod1_file'] = lod_exported['file']
                    entry['lod1_triangles'] = lod_exported['triangles']

                piece_entries.append(entry)

            lo, hi = TRIANGLE_BUDGET_BY_CATEGORY.get(mod.CATEGORY, (0, 999999))
            in_budget = lo <= total_tris <= hi
            species_list.append(dict(
                species=species, category=mod.CATEGORY, locomotion=locomotion,
                habitat=variant.get('habitat', ''), behavior=variant.get('behavior', ''),
                use=variant.get('use', ''), diet=variant.get('diet', ''),
                material=f'M_Fauna_{species}',
                triangles_total=total_tris,
                triangle_budget=dict(min=lo, max=hi, in_budget=in_budget),
                dimensions_cm=dict(x=round(dims_cm[0], 1), y=round(dims_cm[1], 1),
                                    z=round(dims_cm[2], 1)),
                piece_count=len(piece_entries),
                pieces=piece_entries,
            ))
            flag = 'OK' if in_budget else 'FUERA DE PRESUPUESTO'
            print(f"[run_animals] {species}: {total_tris} tris en {len(piece_entries)} "
                  f"piezas ({flag}), dims_cm={tuple(round(d, 1) for d in dims_cm)}")

    manifest_path = os.path.join(EXPORT_DIR, 'animals.json')
    with open(manifest_path, 'w', encoding='utf-8') as f:
        json.dump(dict(generated_by='Tools/Blender/run_animals.py',
                        species_count=len(species_list), species=species_list),
                  f, indent=2, ensure_ascii=False)
    print(f"[run_animals] manifest escrito en {manifest_path} ({len(species_list)} especies)")


if __name__ == '__main__':
    main()
