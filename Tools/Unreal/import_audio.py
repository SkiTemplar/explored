"""Importa el catalogo de audio generado por Tools/Audio en el editor de UE 5.6.

Lee `Art/Export/Audio/manifest.json` y, por cada sonido, importa el WAV
correspondiente a `/Game/Generated/Audio/<Categoria>/<Nombre>`, marca
`looping = True` en los que el manifiesto señala como bucle, le asigna su
SoundClass y guarda el asset.

SoundClass del proyecto (las lee `UExploredGameUserSettings` por ruta para
aplicar los volúmenes de Ajustes > Audio; ver `ExploredGameUserSettings.cpp`):

    /Game/Audio/Classes/SC_Master      madre de todas
    /Game/Audio/Classes/SC_Music       categoria "Musica"
    /Game/Audio/Classes/SC_Effects     categoria "Efectos" (salvo sfx_ui_*)
    /Game/Audio/Classes/SC_Ambient     categoria "Ambiente"
    /Game/Audio/Classes/SC_Interface   sonidos sfx_ui_*

Si ya existen se reutilizan (el script es idempotente). Es Python del EDITOR (usa el modulo `unreal`), pensado para
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

# SoundClass del proyecto. Las rutas deben coincidir con
# ExploredGameUserSettingsDetail::SoundClassPath (Source/Explored/UI/ExploredGameUserSettings.cpp).
SOUND_CLASS_ROOT = "/Game/Audio/Classes"
MASTER_CLASS = "SC_Master"
CHILD_CLASSES = ("SC_Music", "SC_Effects", "SC_Ambient", "SC_Interface")
CATEGORY_TO_CLASS = {"Musica": "SC_Music", "Efectos": "SC_Effects", "Ambiente": "SC_Ambient"}
INTERFACE_PREFIX = "sfx_ui_"
INTERFACE_CLASS = "SC_Interface"

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


def sound_class_name_for(entry: dict) -> str:
    """SoundClass que corresponde a una entrada del manifiesto (sin tocar el editor)."""
    if entry["name"].startswith(INTERFACE_PREFIX):
        return INTERFACE_CLASS
    return CATEGORY_TO_CLASS.get(entry["category"], MASTER_CLASS)


def _load_or_create_sound_class(name: str, asset_tools: "unreal.AssetTools") -> "unreal.SoundClass":
    path = f"{SOUND_CLASS_ROOT}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    return asset_tools.create_asset(name, SOUND_CLASS_ROOT, unreal.SoundClass, unreal.SoundClassFactory())


def _ensure_sound_classes(asset_tools: "unreal.AssetTools") -> dict:
    """Crea (o reutiliza) SC_Master y sus hijas y enlaza la jerarquia en ambos sentidos."""
    master = _load_or_create_sound_class(MASTER_CLASS, asset_tools)
    classes = {MASTER_CLASS: master}
    children = list(master.get_editor_property("child_classes") or [])
    for name in CHILD_CLASSES:
        child = _load_or_create_sound_class(name, asset_tools)
        if child not in children:
            children.append(child)
        classes[name] = child
    # El ajuste del maestro (bApplyToChildren) recorre ChildClasses. Al editar
    # ChildClasses, USoundClass::PostEditChangeProperty fija ParentClass en
    # cada hija (ParentClass es VisibleAnywhere y no se puede escribir directo).
    master.set_editor_property("child_classes", children)
    for name in CHILD_CLASSES:
        child = classes[name]
        if child.get_editor_property("parent_class") != master:
            unreal.log_warning(f"[import_audio] {name}: ParentClass no quedo en {MASTER_CLASS}; revisalo en el editor.")
        unreal.EditorAssetLibrary.save_loaded_asset(child, only_if_is_dirty=False)
    unreal.EditorAssetLibrary.save_loaded_asset(master, only_if_is_dirty=False)
    return classes


def _import_one(entry: dict, source_root: Path, asset_tools: "unreal.AssetTools", sound_classes: dict) -> "unreal.Object | None":
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

    sound_class = sound_classes.get(sound_class_name_for(entry))
    if sound_class is not None:
        try:
            asset.set_editor_property("sound_class_object", sound_class)
        except Exception as error:  # pragma: no cover - depende de la version del motor
            unreal.log_warning(f"[import_audio] {entry['name']}: no se pudo asignar la SoundClass ({error}).")

    unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
    return asset


def main() -> int:
    manifest_path = _manifest_path()
    entries = _load_manifest(manifest_path)
    source_root = manifest_path.parent

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    sound_classes = _ensure_sound_classes(asset_tools)

    imported = 0
    skipped = 0
    for entry in entries:
        asset = _import_one(entry, source_root, asset_tools, sound_classes)
        if asset is not None:
            imported += 1
        else:
            skipped += 1

    unreal.log(f"[import_audio] importados: {imported}, omitidos: {skipped}, total manifiesto: {len(entries)}")
    return 0 if skipped == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
