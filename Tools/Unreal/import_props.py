"""Importa los kits de arte de Tools/Style (Art/Export/K*/) a /Game/Art/<Kit>/ (GDD v3 §11.1).

Lee el manifest.json de cada kit (lo escribe `uv run style-kit-kN`) e importa cada FBX como
/Game/Art/<Kit>/SM_<Nombre>_01, la ruta definitiva que ya usan island_props.json, items.json y el
C++. Por malla:

- escala real (1 m = 100 uu, el actor a escala 1) y normales importadas (planas por cara), sin materiales ni texturas, sin UV de
  lightmap (la iluminación es dinámica) y sin Nanite (las mallas están muy por debajo de su coste);
- M_Toon (/Game/Art/Materials, build_materials.py) en su único slot;
- colisión simple (caja) salvo las piezas que se pisan o se rodean de cerca (casco, puente, torre,
  linterna, rocas grandes, acantilado, palmeras y bambú), que usan su geometría como colisión; las
  manos y la fauna, sin colisión;
- comprueba la caja y los triángulos contra el manifest: si no coinciden, el script falla en vez
  de dejar una malla a otra escala.

Escribe un resumen en Saved/Logs/import_props.json.

Uso: Tools/unreal_python.ps1 -Script Tools/Unreal/import_props.py
"""

import glob
import json
import os
import sys

import unreal

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
EXPORT_ROOT = os.path.join(REPO_ROOT, "Art", "Export")
ART_ROOT = "/Game/Art"
MATERIAL_PATH = "/Game/Art/Materials/M_Toon"
SUMMARY_PATH = os.path.join(REPO_ROOT, "Saved", "Logs", "import_props.json")
# Tolerancia de la caja frente a dimensiones_m del manifest (cm): el exportador redondea a mm.
BOUNDS_TOLERANCE_CM = 1.5
FBX_METERS_TO_CM = 100.0

COMPLEX_COLLISION = {
    "SM_CargueroCasco_01", "SM_CargueroPuente_01", "SM_FaroTorre_01", "SM_FaroLinterna_01",
    "SM_RockBig_01", "SM_CliffRock_01", "SM_PalmA_01", "SM_PalmB_01", "SM_Bamboo_01",
}
NO_COLLISION = {"SM_ManoIria_01", "SM_FaunaPez_01", "SM_FaunaGaviota_01", "SM_GrassClump_01"}


def kit_manifests():
    paths = sorted(glob.glob(os.path.join(EXPORT_ROOT, "K*", "manifest.json")))
    if not paths:
        raise RuntimeError(f"No hay kits en {EXPORT_ROOT} (uv run style-kit-k1/k2/k3 en Tools/Style)")
    for path in paths:
        with open(path, "r", encoding="utf-8") as f:
            yield os.path.basename(os.path.dirname(path)), os.path.dirname(path), json.load(f)


def import_fbx(fbx_path, dest_path, name):
    full_path = f"{dest_path}/{name}"
    # Reimportar sobre un asset existente reutiliza sus ajustes guardados: se borra antes.
    if unreal.EditorAssetLibrary.does_asset_exist(full_path):
        unreal.EditorAssetLibrary.delete_asset(full_path)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", fbx_path)
    task.set_editor_property("destination_path", dest_path)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)

    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("import_animations", False)
    smi = options.static_mesh_import_data
    smi.set_editor_property("combine_meshes", True)
    smi.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    smi.set_editor_property("generate_lightmap_u_vs", False)
    smi.set_editor_property("auto_generate_collision", False)
    smi.set_editor_property("convert_scene", True)
    # El importador de UE 5.6 ignora la unidad del FBX (metros con apply_unit_scale), igual que en
    # import_island.py: el factor 100 deja la malla a escala real (1 m = 100 uu) y el actor a escala 1.
    # check_mesh lo comprueba contra dimensiones_m.
    smi.set_editor_property("import_uniform_scale", FBX_METERS_TO_CM)
    task.set_editor_property("options", options)

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported = task.get_editor_property("imported_object_paths")
    if not imported:
        raise RuntimeError(f"No se pudo importar {fbx_path}")
    mesh = unreal.load_asset(full_path)
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError(f"{fbx_path} no produjo una StaticMesh en {full_path}")
    return mesh


def configure_material(mesh, material):
    slots = mesh.get_editor_property("static_materials")
    if len(slots) != 1:
        raise RuntimeError(f"{mesh.get_name()} tiene {len(slots)} materiales (el estilo exige uno)")
    mesh.set_material(0, material)


def add_box_collision(mesh, body_setup):
    """Caja simple que envuelve la malla. Se escribe en AggGeom directamente: en modo commandlet no
    existe StaticMeshEditorSubsystem y EditorStaticMeshLibrary.add_simple_collisions devuelve -1."""
    box = mesh.get_bounding_box()
    elem = unreal.KBoxElem()
    elem.set_editor_property("center", (box.min + box.max) * 0.5)
    elem.set_editor_property("x", box.max.x - box.min.x)
    elem.set_editor_property("y", box.max.y - box.min.y)
    elem.set_editor_property("z", box.max.z - box.min.z)
    geom = body_setup.get_editor_property("agg_geom")
    geom.set_editor_property("box_elems", [elem])
    body_setup.set_editor_property("agg_geom", geom)


def configure_collision(mesh):
    """La importación no genera colisión (auto_generate_collision = False): aquí se pone la que toca."""
    name = mesh.get_name()
    body_setup = mesh.get_editor_property("body_setup")
    if body_setup is None:
        raise RuntimeError(f"{name} no tiene BodySetup")
    if name in NO_COLLISION:
        body_setup.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
        return "ninguna"
    if name in COMPLEX_COLLISION:
        body_setup.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        return "compleja"
    add_box_collision(mesh, body_setup)
    body_setup.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_DEFAULT)
    return "caja"


def check_mesh(mesh, entry):
    box = mesh.get_bounding_box()
    got = [box.max.x - box.min.x, box.max.y - box.min.y, box.max.z - box.min.z]
    want = [v * 100.0 for v in entry["dimensiones_m"]]
    for g, w in zip(got, want, strict=True):
        if abs(g - w) > BOUNDS_TOLERANCE_CM:
            raise RuntimeError(f"{mesh.get_name()}: caja {got} cm frente a {want} cm del manifest")
    tris = mesh.get_num_triangles(0)
    if tris != entry["tris"]:
        raise RuntimeError(f"{mesh.get_name()}: {tris} triángulos frente a {entry['tris']} del manifest")
    return got, tris


def main():
    material = unreal.load_asset(MATERIAL_PATH)
    if material is None:
        raise RuntimeError(f"Falta {MATERIAL_PATH}: ejecuta antes build_materials.py")
    summary = []
    for kit_dir, export_dir, manifest in kit_manifests():
        dest = f"{ART_ROOT}/{kit_dir}"
        unreal.EditorAssetLibrary.make_directory(dest)
        for entry in manifest["mallas"]:
            name = entry["malla"]
            mesh = import_fbx(os.path.join(export_dir, entry["fbx"]), dest, name)
            configure_material(mesh, material)
            nanite = mesh.get_editor_property("nanite_settings")
            nanite.set_editor_property("enabled", False)
            mesh.set_editor_property("nanite_settings", nanite)
            collision = configure_collision(mesh)
            size_cm, tris = check_mesh(mesh, entry)
            unreal.EditorAssetLibrary.save_loaded_asset(mesh)
            summary.append({"path": mesh.get_path_name(), "tris": tris, "size_cm": [round(v, 1) for v in size_cm],
                            "collision": collision})
            unreal.log(f"[import_props] {mesh.get_path_name()}: {tris} tris, colisión {collision}")
    os.makedirs(os.path.dirname(SUMMARY_PATH), exist_ok=True)
    with open(SUMMARY_PATH, "w", encoding="utf-8") as f:
        json.dump(summary, f, ensure_ascii=False, indent=1)
    unreal.log(f"[import_props] {len(summary)} mallas importadas en {ART_ROOT}/K*")
    return 0


if __name__ == "__main__":
    sys.exit(main())
