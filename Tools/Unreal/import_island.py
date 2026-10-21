"""Importa el terreno de la Isla de Sal (Art/Island/SM_IslaSal_Terrain.fbx, lo genera Tools/Island).

Destino: /Game/Art/Island/SM_IslaSal_Terrain, con M_TerrainToon (/Game/Art/Materials, ver
materials_f6.py), normales suaves y color por vértice importados, sin lightmap UV (la iluminación es
dinámica) y colisión compleja como simple (el personaje camina sobre los triángulos reales).

Comprueba al final que la caja de la malla coincide con las cifras de Art/Island/SM_IslaSal_Terrain.json
(metros -> cm, eje Y conservado): si no, el script falla en vez de dejar un terreno a otra escala.

Uso: Tools/unreal_python.ps1 -Script Tools/Unreal/import_island.py
"""

import json
import os
import sys

import unreal

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
FBX_PATH = os.path.join(REPO_ROOT, "Art", "Island", "SM_IslaSal_Terrain.fbx")
STATS_PATH = os.path.join(REPO_ROOT, "Art", "Island", "SM_IslaSal_Terrain.json")
DEST_PATH = "/Game/Art/Island"
MESH_NAME = "SM_IslaSal_Terrain"
MATERIAL_PATH = "/Game/Art/Materials/M_TerrainToon"
# Tolerancia de la comprobación de la caja (cm).
BOUNDS_TOLERANCE_CM = 5.0


def import_mesh():
    full_path = f"{DEST_PATH}/{MESH_NAME}"
    # Reimportar sobre un asset existente reutiliza sus ajustes guardados: se borra antes.
    if unreal.EditorAssetLibrary.does_asset_exist(full_path):
        unreal.EditorAssetLibrary.delete_asset(full_path)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", FBX_PATH)
    task.set_editor_property("destination_path", DEST_PATH)
    task.set_editor_property("destination_name", MESH_NAME)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)

    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    smi = options.static_mesh_import_data
    smi.set_editor_property("combine_meshes", True)
    smi.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    # Colores fundidos por vértice (M_TerrainToon) y normales suaves por vértice desde F6.
    smi.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
    smi.set_editor_property("generate_lightmap_u_vs", False)
    smi.set_editor_property("auto_generate_collision", False)
    smi.set_editor_property("convert_scene", True)
    # El importador de UE 5.6 ignora la unidad del FBX (metros con apply_unit_scale): sin el factor
    # 100 la isla queda en centímetros numéricamente iguales a los metros (comprobado por check_bounds).
    smi.set_editor_property("import_uniform_scale", 100.0)
    task.set_editor_property("options", options)

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported = task.get_editor_property("imported_object_paths")
    if not imported:
        raise RuntimeError(f"No se pudo importar {FBX_PATH}")
    return unreal.load_asset(imported[0])


def configure(mesh):
    material = unreal.load_asset(MATERIAL_PATH)
    if material is None:
        raise RuntimeError(f"Falta {MATERIAL_PATH}: ejecuta antes materials_f6.py")
    for index in range(len(mesh.get_editor_property("static_materials"))):
        mesh.set_material(index, material)
    body_setup = mesh.get_editor_property("body_setup")
    if body_setup is None:
        raise RuntimeError("La malla del terreno no tiene BodySetup")
    body_setup.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    mesh.set_editor_property("light_map_resolution", 4)


def check_bounds(mesh):
    with open(STATS_PATH, "r", encoding="utf-8") as f:
        stats = json.load(f)
    box = mesh.get_bounding_box()
    got_min = [box.min.x, box.min.y, box.min.z]
    got_max = [box.max.x, box.max.y, box.max.z]
    want_min = [v * 100.0 for v in stats["bounds_min_m"]]
    want_max = [v * 100.0 for v in stats["bounds_max_m"]]
    unreal.log(f"[import_island] caja {got_min} .. {got_max} (esperada {want_min} .. {want_max})")
    for got, want in zip(got_min + got_max, want_min + want_max):
        if abs(got - want) > BOUNDS_TOLERANCE_CM:
            raise RuntimeError(f"La caja del terreno no coincide con {STATS_PATH}: {got} frente a {want}")


def main():
    unreal.EditorAssetLibrary.make_directory(DEST_PATH)
    mesh = import_mesh()
    configure(mesh)
    check_bounds(mesh)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    unreal.log(f"[import_island] {mesh.get_path_name()} importado (M_TerrainToon, colisión compleja como simple)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
