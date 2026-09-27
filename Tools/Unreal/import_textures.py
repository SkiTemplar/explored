"""Importa las texturas procedurales de Art/Export/Textures a /Game/Generated/Textures (idempotente).

    UnrealEditor-Cmd Explored.uproject -run=pythonscript -script="Tools/Unreal/import_textures.py"
"""

import json
import os

import unreal

SOURCE_DIR = os.path.join(unreal.Paths.project_dir(), "Art", "Export", "Textures")
DEST = "/Game/Generated/Textures"

# Nombre -> (sRGB, compresión). Respaldo si falta textures.json (lo escribe gen_textures.py).
SETTINGS = {
    "T_TerrainDetail": (False, unreal.TextureCompressionSettings.TC_MASKS),
    "T_TerrainNormal": (False, unreal.TextureCompressionSettings.TC_NORMALMAP),
    "T_LeafNoise": (False, unreal.TextureCompressionSettings.TC_MASKS),
    "T_WaterFoam": (False, unreal.TextureCompressionSettings.TC_MASKS),
    "T_WaterRipple": (False, unreal.TextureCompressionSettings.TC_NORMALMAP),
}


KIND_COMPRESSION = {
    "color": unreal.TextureCompressionSettings.TC_DEFAULT,
    "normal": unreal.TextureCompressionSettings.TC_NORMALMAP,
    "masks": unreal.TextureCompressionSettings.TC_MASKS,
    # Atlas de paleta (T_Palette_<Isla>): RGBA8 sin compresión por bloques. BC1 comprime en
    # bloques de 4×4 y en los mips con celdas de menos de 4 px mezclaría colores de celdas
    # vecinas. Ver docs/art/paleta.md.
    "palette": unreal.TextureCompressionSettings.TC_EDITOR_ICON,
}


def load_manifest() -> dict:
    """textures.json: nombre -> {kind: color|normal|masks|palette, srgb}. Cubre T_<Material>_BC/_N/_ARH
    y T_Palette_<Isla>."""
    path = os.path.join(SOURCE_DIR, "textures.json")
    if not os.path.isfile(path):
        return {}
    with open(path, encoding="utf-8") as f:
        data = json.load(f)
    return {name: (entry["srgb"], KIND_COMPRESSION[entry["kind"]]) for name, entry in data.items()}


def import_texture(path: str, settings: dict) -> None:
    name = os.path.splitext(os.path.basename(path))[0]
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", DEST)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    texture = unreal.EditorAssetLibrary.load_asset(f"{DEST}/{name}")
    if texture is None:
        raise RuntimeError(f"No se importó {path}")
    srgb, compression = settings.get(name, (True, unreal.TextureCompressionSettings.TC_DEFAULT))
    texture.set_editor_property("srgb", srgb)
    texture.set_editor_property("compression_settings", compression)
    if name.startswith("T_Palette_"):
        # Mips por promedio 2×2: con celdas de 32 px alineadas, ningún mip hasta el 5 mezcla
        # celdas. Bilineal (no trilineal-aniso de más) y sin streaming: es 1 MB y lo usa todo.
        texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE)
        texture.set_editor_property("filter", unreal.TextureFilter.TF_BILINEAR)
        texture.set_editor_property("never_stream", True)
    if compression == unreal.TextureCompressionSettings.TC_NORMALMAP:
        # Los juegos nuevos (_N) ya salen en convención DirectX; los legado los lee HLSL propio.
        texture.set_editor_property("flip_green_channel", False)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    unreal.log(f"[Explored] Textura importada: {DEST}/{name}")


def main() -> None:
    if not os.path.isdir(SOURCE_DIR):
        raise RuntimeError(f"No existe {SOURCE_DIR}; ejecuta antes Tools/Textures/gen_textures.py")
    settings = {**SETTINGS, **load_manifest()}
    for file in sorted(os.listdir(SOURCE_DIR)):
        if file.lower().endswith(".png"):
            import_texture(os.path.join(SOURCE_DIR, file), settings)


main()
