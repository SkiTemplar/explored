"""Comprobaciones de Content/Data/soundtrack.json (GDD v3 §10) contra el director musical.

Lo lee el C++ (`UExploredMusicSubsystem::ParseSoundtrack` y `FSoundtrackModel`): un estado
mal escrito o una pieza sin licencia documentada harían que el catálogo no cargara (sin
música) o que sonara algo sin crédito. Aquí se comprueba el JSON frente a las claves de
estado de `SoundtrackModel.cpp` y frente a `Tools/Audio/music_sources.json` (procedencia y
licencia de cada grabación).
"""

from __future__ import annotations

import json
import math
import re
from pathlib import Path
from typing import Any, TypeGuard

MODEL_CPP = Path("Source") / "Explored" / "Audio" / "SoundtrackModel.cpp"
SOURCES = Path("Tools") / "Audio" / "music_sources.json"
ID_RE = re.compile(r"^mus_[a-z0-9_]+$")
# Estados que exige el GDD v3 §10 (más menú y créditos, que también usa el juego).
REQUIRED_STATES = ("menu", "day", "night", "cyclone", "eye", "finale", "credits")
# El ojo del ciclón es «casi silencio».
EYE_MAX_GAIN = 0.3


def _cpp_keys(text: str, array: str) -> list[str]:
    """Claves de ``const TCHAR* const <array>[...] = { TEXT("a"), ... };``."""
    m = re.search(rf"{array}\[[^\]]*\]\s*=\s*\{{(.*?)\}};", text, re.S)
    return re.findall(r'TEXT\("([^"]+)"\)', m.group(1)) if m else []


def _num(v: Any) -> TypeGuard[float]:
    return isinstance(v, (int, float)) and not isinstance(v, bool) and math.isfinite(v)


def _source_ids(root: Path) -> set[str] | None:
    path = root / SOURCES
    if not path.exists():
        return None
    try:
        doc = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return None
    return {str(p.get("id")) for p in doc.get("pieces", []) if isinstance(p, dict)}


def _check_pieces(doc: dict, where: str, sources: set[str] | None, r) -> set[str]:
    pieces = doc.get("pieces")
    if not isinstance(pieces, list) or not pieces:
        r.error(f"{where}: «pieces» vacío o ausente")
        return set()
    ids: list[str] = []
    for p in pieces:
        if not isinstance(p, dict):
            r.error(f"{where}: pieza {p!r} no es un objeto")
            continue
        pid = p.get("id")
        if not isinstance(pid, str) or not ID_RE.match(pid):
            r.error(f"{where}: id de pieza inválido {pid!r} (mus_ + minúsculas ASCII)")
            continue
        ids.append(pid)
        if not _num(p.get("seconds")) or p["seconds"] <= 0:
            r.error(f"{where} «{pid}»: «seconds» {p.get('seconds')!r} debe ser > 0")
        if sources is not None and pid not in sources:
            r.error(f"{where} «{pid}»: no está en {SOURCES.as_posix()} (sin procedencia ni licencia)")
    for dup in sorted({i for i in ids if ids.count(i) > 1}):
        r.error(f"{where}: id de pieza duplicado «{dup}»")
    return set(ids)


def _check_state(key: str, st: Any, known: set[str], where: str, r) -> None:
    tag = f"{where} estado «{key}»"
    if not isinstance(st, dict):
        r.error(f"{tag}: no es un objeto")
        return
    lst = st.get("pieces")
    if not isinstance(lst, list) or not lst:
        r.error(f"{tag}: sin piezas")
    else:
        for pid in lst:
            if pid not in known:
                r.error(f"{tag}: nombra «{pid}», que no está en «pieces»")
    gain = st.get("gain")
    if not _num(gain) or not 0.0 <= gain <= 1.0:
        r.error(f"{tag}: «gain» {gain!r} fuera de 0-1")
    elif key == "eye" and gain > EYE_MAX_GAIN:
        r.error(f"{tag}: «gain» {gain} demasiado alto; el ojo es casi silencio (≤ {EYE_MAX_GAIN})")
    for fade in ("fadeIn", "fadeOut"):
        if not _num(st.get(fade)) or st[fade] < 0:
            r.error(f"{tag}: «{fade}» {st.get(fade)!r} debe ser ≥ 0")
    rest = st.get("rest")
    if not (isinstance(rest, list) and len(rest) == 2 and all(_num(v) for v in rest) and 0 <= rest[0] <= rest[1]):
        r.error(f"{tag}: «rest» {rest!r} debe ser [mínimo, máximo] con 0 ≤ mínimo ≤ máximo")
    if not isinstance(st.get("loop"), bool):
        r.error(f"{tag}: «loop» debe ser booleano")


def check_music(ds, r) -> None:
    doc = ds.data.get("soundtrack.json")
    if not doc:
        return
    where = "soundtrack.json"
    root = ds.repo_root
    cpp = (root / MODEL_CPP).read_text(encoding="utf-8") if (root / MODEL_CPP).exists() else ""
    state_keys = _cpp_keys(cpp, "StateKeys")
    if not state_keys:
        r.warn(f"{where}: no se encuentra StateKeys en {MODEL_CPP.as_posix()}; no se compara con el C++")
    if not isinstance(doc.get("version"), int) or isinstance(doc.get("version"), bool):
        r.error(f"{where}: «version» debe ser un entero")
    known = _check_pieces(doc, where, _source_ids(root), r)
    states = doc.get("states")
    if not isinstance(states, dict):
        r.error(f"{where}: «states» debe ser un objeto estado → configuración")
        return
    for key, st in states.items():
        if state_keys and (key not in state_keys or key == "silence"):
            r.error(f"{where}: estado «{key}» desconocido para ESoundtrackState {state_keys}; el catálogo no cargaría")
            continue
        _check_state(key, st, known, where, r)
    for key in REQUIRED_STATES:
        if key not in states:
            r.error(f"{where}: falta el estado «{key}» (GDD v3 §10)")
