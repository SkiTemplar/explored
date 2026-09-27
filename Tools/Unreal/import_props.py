"""Importa el kit de props de Tools/Blender/props a /Game/Generated/Meshes/<grupo>/.

Lee Art/Export/Props/manifest.json (lo escribe run_props.py) e importa cada FBX
en la carpeta de su grupo, que es la ruta que esperan los datos y el C++
(p. ej. items.json -> /Game/Generated/Meshes/Items/SM_Item_*, BuildingModel ->
/Game/Generated/Meshes/Construccion/ y /Game/Generated/Meshes/Kit<Material>/).

Reutiliza la importación, los materiales y la colisión de import_meshes.py:
- materiales por slot según `material_slots` (color de vértice x tinte),
- `needs_collision` + `collision_complex`: la malla usa su geometría como colisión,
- `needs_collision` sin `collision_complex`: envolvente NDOP18.

Uso: Tools/unreal_python.ps1 -Script Tools/Unreal/import_props.py
"""

import json
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import import_meshes as kit  # noqa: E402 - necesita el sys.path de arriba

EXPORT_DIR = os.path.join(kit.REPO_ROOT, 'Art', 'Export', 'Props')
MANIFEST_PATH = os.path.join(EXPORT_DIR, 'manifest.json')

# Nanite conviene en la geometria mas pesada del kit de rocas y acantilados
# (paredes/espolones/farallones/arco son piezas de mundo grandes con
# densidad razonable, ver Tools/Blender/props/rocks_cliffs.py); mismo
# criterio que NANITE_CATEGORIES en import_meshes.py para rock/tree. Los
# bloques/cantos/losas sueltas (AcantiladoBloques) son demasiado ligeros
# para que compense.
NANITE_GROUPS = {'AcantiladoFormaciones'}


def _use_complex_collision(static_mesh):
    body_setup = static_mesh.get_editor_property('body_setup')
    if body_setup is None:
        unreal.log_warning(f'[import_props] {static_mesh.get_name()} sin BodySetup: no se ajusta la colisión')
        return
    body_setup.set_editor_property('collision_trace_flag',
                                   unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)


def _import_entry(entry):
    dest_path = f"{kit.MESH_DEST_ROOT}/{entry['group']}"
    fbx_path = os.path.join(EXPORT_DIR, entry['file'])
    static_mesh = kit._import_mesh(fbx_path, dest_path, entry['name'])
    if static_mesh is None:
        return False

    kit._assign_materials(static_mesh, entry['material_slots'])
    if entry.get('needs_collision'):
        if entry.get('collision_complex'):
            _use_complex_collision(static_mesh)
        else:
            kit._add_simple_collision(static_mesh)

    if entry['group'] in NANITE_GROUPS:
        kit._enable_nanite(static_mesh)

    unreal.EditorAssetLibrary.save_loaded_asset(static_mesh)
    return True


def main():
    with open(MANIFEST_PATH, 'r', encoding='utf-8') as f:
        manifest = json.load(f)

    ok, failed = 0, 0
    for entry in manifest['meshes']:
        if _import_entry(entry):
            ok += 1
        else:
            failed += 1

    unreal.log(f'[import_props] importación completa: {ok} OK, {failed} fallidas.')
    return 0 if failed == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
