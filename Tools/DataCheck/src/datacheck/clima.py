"""storm_curve.json: barómetro, chubasco y ciclón de Baliza Siete (GDD v3 §3 y §5.8).

Curva fija sin azar. Comprueba la forma, el orden de los puntos, que las ventanas no se solapen,
los hitos del GDD (chubasco D1 15:00, ciclón D2 19:00 hasta D3 02:00, ojo de 65 s reales) y que
la duración del día y del ojo coincidan con las constantes de FStormClockConfig en C++.
"""

from __future__ import annotations

import re
from collections.abc import Callable
from pathlib import Path
from typing import Any

STORM_CPP = "Weather/StormClockModel.h"
STATES = {"Squall", "HighClouds", "SeaClosed", "CycloneWall1", "CycloneEye", "CycloneWall2"}
# Hitos del GDD: (estado, inicio en horas absolutas desde D1 00:00 o None, fin o None).
GDD_MARKS = {
    "Squall": (15.0, 17.0),
    "CycloneWall1": (43.0, 46.0),
    "CycloneEye": (46.0, None),
    "CycloneWall2": (None, 50.0),
}
TOLERANCE_HOURS = 1.0 / 3600.0

Report = Callable[[str], None]


def _num(value: Any) -> float | None:
    """El número como float, o None si no es un número (los bool no cuentan)."""
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return None
    return float(value)


def _cpp_double(root: Path, name: str) -> float | None:
    path = root / "Source" / "Explored" / STORM_CPP
    if not path.exists():
        return None
    m = re.search(rf"constexpr\s+double\s+{name}\s*=\s*(-?[0-9.]+)\s*;", path.read_text(encoding="utf-8"))
    return float(m.group(1)) if m else None


def _abs_hour(entry: Any) -> float | None:
    if not isinstance(entry, dict):
        return None
    day, hour = _num(entry.get("day")), _num(entry.get("hour"))
    if day is None or hour is None or not 1 <= day <= 3 or not 0 <= hour < 24:
        return None
    return (day - 1) * 24.0 + hour


def _check_points(points: Any, error: Report) -> None:
    if not isinstance(points, list) or not points:
        error("storm_curve.json points: debe ser una lista no vacía")
        return
    last = None
    for i, point in enumerate(points):
        at = _abs_hour(point)
        hpa = _num(point.get("hpa")) if isinstance(point, dict) else None
        if at is None or hpa is None or not 900 <= hpa <= 1100:
            error(f"storm_curve.json points[{i}]: día 1-3, hora 0-24 y hpa 900-1100")
            continue
        if last is not None and at <= last:
            error(f"storm_curve.json points[{i}]: las horas deben ir en orden creciente")
        last = at


def _check_windows(windows: Any, seconds_per_hour: float, eye_seconds: float, error: Report) -> None:
    if not isinstance(windows, list) or not windows:
        error("storm_curve.json windows: debe ser una lista no vacía")
        return
    spans: list[tuple[str, float, float]] = []
    for i, window in enumerate(windows):
        state = window.get("state") if isinstance(window, dict) else None
        start = _abs_hour(window)
        hours = _num(window.get("hours")) if isinstance(window, dict) else None
        if not isinstance(state, str) or state not in STATES or start is None or hours is None or hours <= 0:
            error(f"storm_curve.json windows[{i}]: estado de {sorted(STATES)}, día/hora válidos y hours > 0")
            continue
        spans.append((state, start, start + hours))
    for i, (state, start, end) in enumerate(spans):
        for other, ostart, oend in spans[:i]:
            if start < oend - TOLERANCE_HOURS and ostart < end - TOLERANCE_HOURS:
                error(f"storm_curve.json windows: «{state}» se solapa con «{other}»")
    by_state = {state: (start, end) for state, start, end in spans}
    for state, (want_start, want_end) in GDD_MARKS.items():
        got = by_state.get(state)
        if got is None:
            error(f"storm_curve.json windows: falta «{state}» (GDD §5.8)")
            continue
        if want_start is not None and abs(got[0] - want_start) > TOLERANCE_HOURS:
            error(f"storm_curve.json «{state}»: debe empezar a las {want_start:g} h absolutas (GDD §5.8)")
        if want_end is not None and abs(got[1] - want_end) > TOLERANCE_HOURS:
            error(f"storm_curve.json «{state}»: debe terminar a las {want_end:g} h absolutas (GDD §5.8)")
    eye = by_state.get("CycloneEye")
    if eye is not None and abs((eye[1] - eye[0]) * seconds_per_hour - eye_seconds) > 0.5:
        error(f"storm_curve.json «CycloneEye»: dura {(eye[1] - eye[0]) * seconds_per_hour:.1f} s reales, no {eye_seconds:g}")


def _check_rates(doc: dict[str, Any], error: Report) -> None:
    for key, low, high in (("rain", 0.0, 1.0), ("chargesPerHour", 0.0, 10.0)):
        block = doc.get(key)
        values = [_num(block.get(k)) for k in ("squall", "cyclone")] if isinstance(block, dict) else [None]
        if not all(v is not None and low <= v <= high for v in values):
            error(f"storm_curve.json {key}: squall y cyclone entre {low:g} y {high:g}")
    raft, back = _abs_hour(doc.get("raftLast")), _abs_hour(doc.get("forcedReturn"))
    if raft is None or back is None or back < raft:
        error("storm_curve.json: raftLast y forcedReturn deben ser día/hora válidos, con forcedReturn ≥ raftLast")


def check_storm(data: dict[str, Any], repo_root: Path, error: Report) -> None:
    doc = data.get("storm_curve.json")
    if not isinstance(doc, dict):
        return
    day = _num(doc.get("dayLengthMinutes"))
    eye = _num(doc.get("eyeRealSeconds"))
    for key, value, constant in (("dayLengthMinutes", day, "DayLengthMinutes"), ("eyeRealSeconds", eye, "EyeRealSeconds")):
        cpp = _cpp_double(repo_root, constant)
        if cpp is None:
            error(f"storm_curve.json {key}: no existe la constante {constant} en {STORM_CPP}")
        elif value != cpp:
            error(f"storm_curve.json {key}={doc.get(key)} pero {constant}={cpp:g} en {STORM_CPP}")
    _check_points(doc.get("points"), error)
    if day is not None and day > 0 and eye is not None:
        _check_windows(doc.get("windows"), day * 60.0 / 24.0, eye, error)
    _check_rates(doc, error)
