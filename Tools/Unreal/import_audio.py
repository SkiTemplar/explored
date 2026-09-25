"""Importa el catalogo de audio generado por Tools/Audio en el editor de UE 5.6.

Lee `Art/Export/Audio/manifest.json` y, por cada sonido, importa el WAV
correspondiente a `/Game/Generated/Audio/<Categoria>/<Nombre>`, marca
`looping = True` en los que el manifiesto señala como bucle y guarda el
asset. Es Python del EDITOR (usa el modulo `unreal`), pensado para
ejecutarse con:

    UnrealEditor-Cmd.exe <Explored.uproject> -run=pythonscript -script="Tools/Unreal/import_audio.py"

o desde la consola de Python del editor. Este repositorio NO ejecuta este
script como parte de la tarea que lo crea: se deja listo para cuando se
integre en el pipeline de importacion (`Tools/build_content.ps1`).
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

try:
    import unreal
except ImportError:  # pragma: no cover - solo se ejecuta dentro del editor de UE
    print(
        "Este script requiere el modulo 'unreal': ejecutalo dentro del editor "
        "(UnrealEditor-Cmd -run=pythonscript) o desde su consola de Python."
    )
    sys.exit(1)


# Raiz de contenido donde aterriza todo el audio generado. Coincide con el
# pipeline descrito en la seccion 12.3 del diseño (Tools/Audio -> Art/Export
# -> Tools/Unreal/import_* -> Content/).
CONTENT_ROOT = "/Game/Generated/Audio"

# Nombres de propiedad candidatos para "es un bucle" segun version del motor:
# se prueban en orden y se usa el primero que exista en el SoundWave importado.
LOOP_PROPERTY_CANDIDATES = ("looping", "bLooping", "loop")


def _repo_root() -> Path:
    # Tools/Unreal/import_audio.py -> raiz del repo (dos niveles arriba).
    return Path(__file__).resolve().parents[2]


def _manifest_path() -> Path:
    return _repo_root() / "Art" / "Export" / "Audio" / "manifest.json"


def _load_manifest(path: Path) -> list[dict]:
    if not path.exists():
        raise FileNotFoundError(
            f"No existe {path}. Genera el catalogo primero con "
            f"'uv run explored-audio build' (o 'uv run python -m explored_audio.build') "
            f"desde Tools/Audio."
        )
    payload = json.loads(path.read_text(encoding="utf-8"))
    return payload["sounds"]


def _set_looping(sound_wave: "unreal.SoundWave", is_loop: bool) -> bool:
    """Marca el flag de bucle probando los nombres de propiedad conocidos."""
    for prop_name in LOOP_PROPERTY_CANDIDATES:
        try:
            sound_wave.set_editor_property(prop_name, is_loop)
            return True
        except Exception:
            continue
    return False


def _import_one(entry: dict, source_root: Path, asset_tools: "unreal.AssetTools") -> "unreal.Object | None":
    wav_path = source_root / entry["file"]
    if not wav_path.exists():
        unreal.log_warning(f"[import_audio] falta el WAV en disco, se omite: {wav_path}")
        return None

    destination_path = f"{CONTENT_ROOT}/{entry['category']}"

    task = unreal.AssetImportTask()
    task.filename = str(wav_path)
    task.destination_path = destination_path
    task.destination_name = entry["name"]
    task.automated = True
    task.replace_existing = True
    task.replace_existing_settings = True
    task.save = True

    asset_tools.import_asset_tasks([task])

    imported_paths = list(task.get_editor_property("imported_object_paths"))
    if not imported_paths:
        unreal.log_error(f"[import_audio] no se pudo importar: {wav_path}")
        return None

    asset = unreal.load_asset(imported_paths[0])
    if asset is None:
        unreal.log_error(f"[import_audio] importado pero no se pudo cargar: {imported_paths[0]}")
        return None

    if entry.get("loop"):
        if not _set_looping(asset, True):
            unreal.log_warning(
                f"[import_audio] {entry['name']}: no se encontro una propiedad de bucle "
                f"reconocida en SoundWave (revisar LOOP_PROPERTY_CANDIDATES)."
            )

    unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
    return asset


def main() -> int:
    manifest_path = _manifest_path()
    entries = _load_manifest(manifest_path)
    source_root = manifest_path.parent

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

    imported = 0
    skipped = 0
    for entry in entries:
        asset = _import_one(entry, source_root, asset_tools)
        if asset is not None:
            imported += 1
        else:
            skipped += 1

    unreal.log(f"[import_audio] importados: {imported}, omitidos: {skipped}, total manifiesto: {len(entries)}")
    return 0 if skipped == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
