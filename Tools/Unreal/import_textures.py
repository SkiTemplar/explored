"""Importa las texturas procedurales de Art/Export/Textures a /Game/Generated/Textures (idempotente).

    UnrealEditor-Cmd Explored.uproject -run=pythonscript -script="Tools/Unreal/import_textures.py"
"""

import os

import unreal

SOURCE_DIR = os.path.join(unreal.Paths.project_dir(), "Art", "Export", "Textures")
DEST = "/Game/Generated/Textures"

# Nombre -> (sRGB, compresión)
SETTINGS = {
    "T_TerrainDetail": (False, unreal.TextureCompressionSettings.TC_MASKS),
    "T_TerrainNormal": (False, unreal.TextureCompressionSettings.TC_NORMALMAP),
    "T_LeafNoise": (False, unreal.TextureCompressionSettings.TC_MASKS),
    "T_WaterFoam": (False, unreal.TextureCompressionSettings.TC_MASKS),
    "T_WaterRipple": (False, unreal.TextureCompressionSettings.TC_NORMALMAP),
}


def import_texture(path: str) -> None:
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
    srgb, compression = SETTINGS.get(name, (True, unreal.TextureCompressionSettings.TC_DEFAULT))
    texture.set_editor_property("srgb", srgb)
    texture.set_editor_property("compression_settings", compression)
    if compression == unreal.TextureCompressionSettings.TC_NORMALMAP:
        # El generador ya escribe la convención de Unreal (verde invertido).
        texture.set_editor_property("flip_green_channel", False)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    unreal.log(f"[Explored] Textura importada: {DEST}/{name}")


def main() -> None:
    if not os.path.isdir(SOURCE_DIR):
        raise RuntimeError(f"No existe {SOURCE_DIR}; ejecuta antes Tools/Textures/gen_textures.py")
    for file in sorted(os.listdir(SOURCE_DIR)):
        if file.lower().endswith(".png"):
            import_texture(os.path.join(SOURCE_DIR, file))


main()
