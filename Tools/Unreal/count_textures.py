"""Cuenta las texturas de /Game (Content) y falla si hay alguna fuera de la lista permitida.

GDD v3 §11: el estilo no usa texturas salvo la hoja de paleta (T_Paleta) y la máscara de trazos del
terreno (T_TerrainStrokes, excepción del terreno de F6 en el GDD §11). El océano y el cielo
usan texturas del motor (/Engine), que no cuentan; la hoja de iconos de Slate
(Content/Slate/T_IconosUI.png) es un PNG suelto, no un asset de /Game. Escribe el resultado en
Saved/Logs/count_textures.json.

Uso: Tools/unreal_python.ps1 -Script Tools/Unreal/count_textures.py
"""

import json
import os

import unreal

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
RESULT_PATH = os.path.join(REPO_ROOT, "Saved", "Logs", "count_textures.json")
ALLOWED = {"/Game/Art/Materials/T_Paleta", "/Game/Art/Materials/T_TerrainStrokes"}
TEXTURE_CLASSES = ("Texture2D", "TextureCube", "TextureRenderTarget2D", "Texture2DArray", "VolumeTexture",
                   "TextureCubeArray", "TextureLightProfile", "MediaTexture")


def main():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.search_all_assets(True)
    found = []
    for asset in registry.get_assets_by_path("/Game", recursive=True):
        class_name = str(asset.asset_class_path.asset_name)
        if class_name in TEXTURE_CLASSES:
            found.append(str(asset.package_name))
    extra = sorted(p for p in found if p not in ALLOWED)
    result = {"textures": sorted(found), "not_allowed": extra}
    os.makedirs(os.path.dirname(RESULT_PATH), exist_ok=True)
    with open(RESULT_PATH, "w", encoding="utf-8") as f:
        json.dump(result, f, ensure_ascii=False, indent=1)
    unreal.log(f"[count_textures] {len(found)} texturas en /Game; fuera de la lista permitida: {len(extra)}")
    for path in extra:
        unreal.log_warning(f"[count_textures] textura no permitida: {path}")
    return 1 if extra else 0


if __name__ == "__main__":
    if main() != 0:
        raise RuntimeError("Hay texturas en Content fuera de la lista permitida (ver Saved/Logs/count_textures.json)")
