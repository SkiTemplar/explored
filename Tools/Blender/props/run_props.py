"""
run_props.py — genera el kit de props narrativos y de puntos de interés de
«Explored» (avión Albatros, campamentos Halden, faro, brújula estelar,
petroglifos, marae, pecio, baliza, objetos pequeños, embarcaciones y piezas
de construcción del jugador).

Se ejecuta dentro de Blender 5.2 en modo headless:

    "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" ^
        -b --factory-startup --python Tools\\Blender\\props\\run_props.py

Por cada variante declarada en los módulos de Tools/Blender/props/*.py
construye la malla, la exporta a Art/Export/Props/<Grupo>/SM_<Nombre>.fbx y
anota sus metadatos en Art/Export/Props/manifest.json (triángulos,
dimensiones en cm, slots de material, si necesita colisión y si es
interactuable). Idempotente: las semillas son fijas.

Para regenerar solo algunos grupos durante iteración, se puede pasar un
filtro tras «--»:

    blender.exe -b --python run_props.py -- --modules=albatros,lighthouse
"""

import importlib
import json
import os
import sys

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
BLENDER_DIR = os.path.join(REPO_ROOT, 'Tools', 'Blender')
LIB_DIR = os.path.join(BLENDER_DIR, 'lib')
PROPS_DIR = os.path.join(BLENDER_DIR, 'props')
EXPORT_DIR = os.path.join(REPO_ROOT, 'Art', 'Export', 'Props')

for _p in (LIB_DIR, PROPS_DIR):
    if _p not in sys.path:
        sys.path.insert(0, _p)

import common as C  # noqa: E402

# módulo -> carpeta de grupo bajo Art/Export/Props/ (nombres del catálogo
# del encargo, sin acentos ni espacios para que sean rutas limpias).
MODULE_GROUPS = [
    ('albatros', 'Albatros'),
    ('halden_camp', 'Halden'),
    ('lighthouse', 'Faro'),
    ('star_compass', 'BrujulaEstelar'),
    ('petroglyphs', 'Petroglifos'),
    ('marae', 'Marae'),
    ('shipwreck', 'Pecio'),
    ('beacon', 'Baliza'),
    ('small_items', 'ObjetosPequenos'),
    ('boats', 'Embarcaciones'),
    ('building_kit', 'Construccion'),
    # kit modular de la base: una carpeta por material (KitPalma, KitBambu,
    # KitMadera, KitPiedra), que cada variante declara en su clave 'group'.
    ('kit_construccion', 'KitConstruccion'),
    ('mobiliario_base', 'MobiliarioBase'),
    ('produccion_base', 'ProduccionBase'),
    ('ruinas_polinesias', 'Ruinas'),  # grupos RuinasMarae / RuinasTallas por variante
    ('tesoros', 'Tesoros'),
    # objetos de inventario de items.json (SM_Item_<Id>): herramientas y materiales
    ('items_herramientas', 'Items'),
    ('items_materiales', 'Items'),
    ('items_contenedores', 'Items'),
    ('items_naturales', 'Items'),
    ('items_rescatados', 'Items'),
    ('items_recursos', 'Items'),
    ('items_armas', 'Items'),
    ('items_frutas', 'Items'),
    ('items_pescados', 'Items'),
    ('items_despojos', 'Items'),
    ('items_orilla', 'Items'),
]


def _import_or_reload(name):
    if name in sys.modules:
        return importlib.reload(sys.modules[name])
    return importlib.import_module(name)


def _parse_module_filter():
    argv = sys.argv
    if '--' not in argv:
        return None
    after = argv[argv.index('--') + 1:]
    for a in after:
        if a.startswith('--modules='):
            return set(a.split('=', 1)[1].split(','))
    return None


def main():
    os.makedirs(EXPORT_DIR, exist_ok=True)
    module_filter = _parse_module_filter()
    manifest = []

    for mod_name, default_group in MODULE_GROUPS:
        if module_filter is not None and mod_name not in module_filter:
            continue
        mod = _import_or_reload(mod_name)

        for variant in mod.VARIANTS:
            # un módulo puede repartir sus variantes en varios grupos
            # (p.ej. el kit modular, un grupo por material)
            group_folder = variant.get('group', default_group)
            out_dir = os.path.join(EXPORT_DIR, group_folder)
            os.makedirs(out_dir, exist_ok=True)
            C.reset_scene()
            obj = mod.build(variant)

            tris = C.triangle_count(obj)
            dims = C.dimensions_cm(obj)
            slots = [m.name for m in obj.data.materials]
            mesh_name = f"SM_{variant['name']}"
            fpath = os.path.join(out_dir, mesh_name + '.fbx')
            C.export_fbx(fpath, [obj])

            lo, hi = variant.get('tri_budget', (0, 999999))
            in_budget = lo <= tris <= hi

            manifest.append(dict(
                name=mesh_name,
                group=group_folder,
                file=os.path.relpath(fpath, EXPORT_DIR).replace('\\', '/'),
                seed=variant.get('seed', 0),
                triangles=tris,
                triangle_budget=dict(min=lo, max=hi, in_budget=in_budget),
                dimensions_cm=dict(x=round(dims[0], 1), y=round(dims[1], 1), z=round(dims[2], 1)),
                material_slots=slots,
                needs_collision=bool(variant.get('needs_collision', True)),
                collision_complex=bool(variant.get('collision_complex', False)),
                interactable=bool(variant.get('interactable', False)),
            ))
            flag = 'OK' if in_budget else 'FUERA DE PRESUPUESTO'
            print(f"[run_props] {mesh_name}: {tris} tris ({flag}), "
                  f"dims_cm={tuple(round(d, 1) for d in dims)}")

    if module_filter is not None:
        # actualización parcial: conserva las entradas de grupos no
        # regenerados en esta pasada en vez de truncar el manifest entero.
        manifest_path = os.path.join(EXPORT_DIR, 'manifest.json')
        prev = []
        if os.path.isfile(manifest_path):
            with open(manifest_path, 'r', encoding='utf-8') as f:
                prev = json.load(f).get('meshes', [])
        touched_groups = {g for m, g in MODULE_GROUPS if m in module_filter}
        touched_groups |= {e['group'] for e in manifest}
        prev_kept = [e for e in prev if e['group'] not in touched_groups]
        manifest = prev_kept + manifest

    manifest_path = os.path.join(EXPORT_DIR, 'manifest.json')
    manifest_doc = dict(generated_by='Tools/Blender/props/run_props.py',
                         mesh_count=len(manifest), meshes=manifest)
    with open(manifest_path, 'w', encoding='utf-8') as f:
        json.dump(manifest_doc, f, indent=2, ensure_ascii=False)
    print(f"[run_props] manifest escrito en {manifest_path} ({len(manifest)} props)")

    # catálogo del encargo (id, fichero, tamaño, categoría): mismo contenido
    # que manifest.json, con nombre propio para quien solo quiera consultar
    # el catálogo de props sin conocer la convención manifest.json del resto
    # del pipeline (Art/Export/Meshes/manifest.json).
    props_catalog_path = os.path.join(EXPORT_DIR, 'props.json')
    catalog = [dict(id=m['name'], file=m['file'], group=m['group'],
                     dimensions_cm=m['dimensions_cm'], triangles=m['triangles'])
               for m in manifest]
    with open(props_catalog_path, 'w', encoding='utf-8') as f:
        json.dump(dict(generated_by='Tools/Blender/props/run_props.py',
                        prop_count=len(catalog), props=catalog),
                  f, indent=2, ensure_ascii=False)
    print(f"[run_props] catálogo escrito en {props_catalog_path} ({len(catalog)} props)")


if __name__ == '__main__':
    main()
