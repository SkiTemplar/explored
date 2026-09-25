"""
run_all.py — genera todo el kit de vegetación y rocas de «Explored».

Se ejecuta dentro de Blender 5.2 en modo headless:

    "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" ^
        -b --factory-startup --python Tools\\Blender\\run_all.py

Por cada variante declarada en los scripts de Tools/Blender/assets/*.py
construye la malla, la exporta a
Art/Export/Meshes/<Familia>/SM_<Nombre>_<NN>.fbx y anota sus metadatos en
Art/Export/Meshes/manifest.json (triángulos, dimensiones en cm, slots de
material y categoría). Idempotente: las semillas son fijas, así que cada
ejecución reescribe los mismos ficheros sin acumular basura ni duplicados.
"""

import importlib
import json
import os
import sys

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
BLENDER_DIR = os.path.join(REPO_ROOT, 'Tools', 'Blender')
LIB_DIR = os.path.join(BLENDER_DIR, 'lib')
ASSETS_DIR = os.path.join(BLENDER_DIR, 'assets')
EXPORT_DIR = os.path.join(REPO_ROOT, 'Art', 'Export', 'Meshes')

for _p in (LIB_DIR, ASSETS_DIR):
    if _p not in sys.path:
        sys.path.insert(0, _p)

import common as C  # noqa: E402

FAMILY_FOLDERS = {
    'palm': 'Palm',
    'tree': 'JungleTree',
    'shrub': 'Shrub',
    'rock': 'Rock',
    'grass': 'Grass',
}

MODULE_NAMES = ['palm', 'jungle_tree', 'shrub', 'rock', 'grass']

# Presupuestos orientativos de triángulos (GDD §8 + encargo): se registran
# en el manifest pero no bloquean la generación; validate.py es quien los
# hace cumplir de verdad antes de dar el kit por bueno.
TRIANGLE_BUDGETS = {
    'palm': (3000, 6000),
    'tree': (6000, 15000),
    'shrub': (1000, 4000),
    'rock': (1000, 3000),
    'grass': (0, 600),
}


def _import_or_reload(name):
    if name in sys.modules:
        return importlib.reload(sys.modules[name])
    return importlib.import_module(name)


def main():
    os.makedirs(EXPORT_DIR, exist_ok=True)
    manifest = []

    for mod_name in MODULE_NAMES:
        mod = _import_or_reload(mod_name)
        folder = FAMILY_FOLDERS[mod.CATEGORY]
        out_dir = os.path.join(EXPORT_DIR, folder)
        os.makedirs(out_dir, exist_ok=True)
        lo, hi = TRIANGLE_BUDGETS[mod.CATEGORY]

        for variant in mod.VARIANTS:
            C.reset_scene()
            obj = mod.build(variant)

            tris = C.triangle_count(obj)
            dims = C.dimensions_cm(obj)
            slots = [m.name for m in obj.data.materials]
            mesh_name = f"SM_{variant['name']}_{variant['index']:02d}"
            fname = mesh_name + '.fbx'
            fpath = os.path.join(out_dir, fname)
            C.export_fbx(fpath, [obj])

            in_budget = lo <= tris <= hi
            manifest.append(dict(
                name=mesh_name,
                file=os.path.relpath(fpath, EXPORT_DIR).replace('\\', '/'),
                category=mod.CATEGORY,
                seed=variant['seed'],
                triangles=tris,
                triangle_budget=dict(min=lo, max=hi, in_budget=in_budget),
                dimensions_cm=dict(x=round(dims[0], 1), y=round(dims[1], 1), z=round(dims[2], 1)),
                material_slots=slots,
            ))
            flag = 'OK' if in_budget else 'FUERA DE PRESUPUESTO'
            print(f"[run_all] {fname}: {tris} tris ({flag}), dims_cm={tuple(round(d,1) for d in dims)}")

    manifest_path = os.path.join(EXPORT_DIR, 'manifest.json')
    with open(manifest_path, 'w', encoding='utf-8') as f:
        json.dump(dict(generated_by='Tools/Blender/run_all.py', mesh_count=len(manifest),
                        meshes=manifest), f, indent=2, ensure_ascii=False)
    print(f"[run_all] manifest escrito en {manifest_path} ({len(manifest)} mallas)")


if __name__ == '__main__':
    main()
