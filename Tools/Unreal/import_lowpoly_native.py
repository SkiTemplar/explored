"""
Importa el set low poly de vegetación (Art/Export/Meshes/LowPoly/<Slot>/*.fbx, generado por
Tools/Packs/process_landing_set.py) a /Game/Generated/Meshes/LowPoly/<Slot>/ conservando el
material y la textura incrustados en cada FBX (material_slots ["NATIVE"], ver PR #111). El
horneado (WorldGenCommandlet::ApplyLowPolySet) usa estas mallas en lugar de las procedurales.

Uso: Tools/unreal_python.ps1 -Script Tools/Unreal/import_lowpoly_native.py
"""
import json
import os

import unreal

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
LOWPOLY_DIR = os.path.join(REPO_ROOT, 'Art', 'Export', 'Meshes', 'LowPoly')
MANIFEST = os.path.join(LOWPOLY_DIR, 'lowpoly_manifest.json')
DEST_ROOT = '/Game/Generated/Meshes/LowPoly'
# Rocas y restos se pisan: colisión simple. El follaje se atraviesa.
COLLISION_SLOTS = {'Rock', 'Debris'}


def build_task(fbx_path: str, dest: str) -> unreal.AssetImportTask:
    options = unreal.FbxImportUI()
    options.set_editor_property('import_mesh', True)
    options.set_editor_property('import_as_skeletal', False)
    options.set_editor_property('import_materials', True)
    options.set_editor_property('import_textures', True)
    options.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
    data = options.get_editor_property('static_mesh_import_data')
    data.set_editor_property('combine_meshes', True)
    data.set_editor_property('auto_generate_collision', False)
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', fbx_path)
    task.set_editor_property('destination_path', dest)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', True)
    task.set_editor_property('options', options)
    return task


def main() -> None:
    with open(MANIFEST, encoding='utf-8') as f:
        entries = json.load(f)['meshes']
    tasks = []
    for entry in entries:
        fbx = os.path.join(LOWPOLY_DIR, entry['file'].replace('/', os.sep))
        if not os.path.isfile(fbx):
            unreal.log_error(f'[lowpoly] Falta {fbx}')
            continue
        tasks.append((entry, build_task(fbx, f"{DEST_ROOT}/{entry['slot']}")))
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t for _, t in tasks])

    imported = 0
    for entry, _ in tasks:
        path = f"{DEST_ROOT}/{entry['slot']}/{entry['name']}"
        mesh = unreal.EditorAssetLibrary.load_asset(path)
        if mesh is None:
            unreal.log_error(f'[lowpoly] No se importó {path}')
            continue
        if entry['slot'] in COLLISION_SLOTS:
            unreal.EditorStaticMeshLibrary.add_simple_collisions(mesh, unreal.ScriptingCollisionShapeType.NDOP18)
            unreal.EditorAssetLibrary.save_loaded_asset(mesh)
        imported += 1
    unreal.log(f'[lowpoly] {imported}/{len(entries)} mallas importadas en {DEST_ROOT}')


main()
