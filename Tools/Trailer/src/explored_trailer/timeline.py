"""Línea de tiempo del tráiler: planos y eventos, leídos de `Tools/Trailer/trailer.json`.

El agente de motor escribe ese JSON con los tiempos reales. Mientras no exista se usa la
línea de tiempo nominal del encargo (75 s a 30 fps). El lector es tolerante con los nombres
de campo (inglés o español, segundos o fotogramas) y falla con un mensaje claro si no
reconoce la estructura, en lugar de inventarse tiempos.

Formato preferido:

    {"fps": 30, "frames": 2250,
     "shots": [{"index": 1, "name": "mar_amanecer", "start": 0.0, "end": 4.0}, ...],
     "events": {"ignite": 50.4, "lit": 53.4, "mistral": 53.4, "horn": 63.0}}

Si hay más de 8 planos, cada uno debe llevar su número de plano (`index`/`plano`, o el prefijo
numérico del nombre, como en `03b-rocas-pescador`) y los subplanos con el mismo número se agrupan.

Los instantes del final los marca el motor (UExploredTrailerSubsystem::TrackFinale) en
`Saved/Trailer/frames/report_*.json` («finaleFrames»: ignite, mistral, horn, fade, en
fotogramas globales) y en el log (`[Trailer] final: horn en el fotograma N`);
`finale_events` los lee de ahí.
"""

from __future__ import annotations

import json
import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

SHOT_COUNT = 8
DEFAULT_FPS = 30
DEFAULT_FRAMES = 2250

# Final del juego (Source/Explored/Narrative/FinaleSequenceModel.h, FFinaleTiming por defecto):
# la llama prende en 3 s, el Mistral aparece al quedar encendida (de noche) y la bocina suena
# al 80 % de una llegada de 12 s.
IGNITE_SECONDS = 3.0
MISTRAL_APPROACH_SECONDS = 12.0
HORN_AT_APPROACH_FRACTION = 0.8
HORN_AFTER_MISTRAL = MISTRAL_APPROACH_SECONDS * HORN_AT_APPROACH_FRACTION

NOMINAL_SHOTS: tuple[tuple[int, str, float, float], ...] = (
    (1, "mar_amanecer", 0.0, 4.0),
    (2, "cala_varada", 4.0, 12.0),
    (3, "montaje_supervivencia", 12.0, 22.0),
    (4, "faro_atardecer", 22.0, 32.0),
    (5, "ciclon", 32.0, 42.0),
    (6, "carguero_sahara", 42.0, 50.0),
    (7, "final_faro", 50.0, 68.0),
    (8, "titulo", 68.0, 75.0),
)

_START_KEYS = ("start", "startSeconds", "start_s", "inicio", "t0", "begin")
_END_KEYS = ("end", "endSeconds", "end_s", "fin", "t1")
_START_FRAME_KEYS = ("startFrame", "frameStart", "start_frame", "firstFrame", "frame_start")
_END_FRAME_KEYS = ("endFrame", "frameEnd", "end_frame", "lastFrame", "frame_end")
_INDEX_KEYS = ("index", "plano", "shot", "number", "id")
_NAME_KEYS = ("name", "nombre", "label", "id")
_EVENT_ALIASES: dict[str, tuple[str, ...]] = {
    "ignite": ("ignite", "encender", "enciende", "flame", "llama", "ignition"),
    "lit": ("lit", "encendida", "beacon_lit", "beam"),
    "mistral": ("mistral", "mistral_lights", "luces_mistral", "mistral_start"),
    "horn": ("horn", "bocina", "sfx_ship_horn", "ship_horn"),
}


_NAME_PREFIX = re.compile(r"^(\d{1,2})(?:[a-z])?[-_ ]")
_LOG_EVENT = re.compile(r"\[Trailer\] final: (\w+) en el fotograma (\d+)")


class TimelineError(ValueError):
    """trailer.json no tiene una estructura reconocible."""


@dataclass(frozen=True)
class Shot:
    index: int
    name: str
    start: float
    end: float

    @property
    def duration(self) -> float:
        return self.end - self.start


@dataclass(frozen=True)
class Timeline:
    fps: int
    frames: int
    shots: tuple[Shot, ...]
    events: dict[str, float] = field(default_factory=dict)
    source: str = "nominal"
    parts: tuple[Shot, ...] = ()

    @property
    def duration(self) -> float:
        return self.frames / self.fps

    def shot(self, index: int) -> Shot:
        for shot in self.shots:
            if shot.index == index:
                return shot
        raise KeyError(f"no hay plano {index}")

    def parts_of(self, index: int) -> tuple[Shot, ...]:
        """Subplanos del plano `index` (vacío si el plano no se dividió)."""
        found = tuple(p for p in self.parts if p.index == index)
        return found if len(found) > 1 else ()

    def ignite_time(self) -> float:
        return self.events.get("ignite", self.shot(7).start)

    def lit_time(self) -> float:
        return self.events.get("lit", self.ignite_time() + IGNITE_SECONDS)

    def mistral_time(self) -> float:
        return self.events.get("mistral", self.lit_time())

    def horn_time(self) -> float:
        """Bocina: la del JSON; si no, FFinaleSequenceModel desde las luces del Mistral."""
        return self.events.get("horn", self.mistral_time() + HORN_AFTER_MISTRAL)


def nominal_timeline() -> Timeline:
    shots = tuple(Shot(i, n, s, e) for i, n, s, e in NOMINAL_SHOTS)
    return Timeline(DEFAULT_FPS, DEFAULT_FRAMES, shots, {}, "nominal")


def _first(data: dict[str, Any], keys: tuple[str, ...]) -> Any:
    for key in keys:
        if key in data and data[key] is not None:
            return data[key]
    return None


def _seconds(data: dict[str, Any], second_keys: tuple[str, ...], frame_keys: tuple[str, ...], fps: int) -> float | None:
    value = _first(data, second_keys)
    if isinstance(value, (int, float)):
        return float(value)
    frame = _first(data, frame_keys)
    if isinstance(frame, (int, float)):
        return float(frame) / fps
    return None


def _parse_shot(raw: Any, order: int, fps: int) -> tuple[int | None, Shot]:
    if not isinstance(raw, dict):
        raise TimelineError(f"el plano {order} no es un objeto")
    start = _seconds(raw, _START_KEYS, _START_FRAME_KEYS, fps)
    end = _seconds(raw, _END_KEYS, _END_FRAME_KEYS, fps)
    if end is None:
        frame_end = _first(raw, _END_FRAME_KEYS)
        end = None if frame_end is None else (float(frame_end) + 1) / fps
    if start is None or end is None or end <= start:
        raise TimelineError(f"el plano {order} no tiene inicio y fin válidos: {raw}")
    explicit = _first(raw, _INDEX_KEYS)
    name = _first(raw, _NAME_KEYS)
    if not isinstance(explicit, int) and isinstance(name, str):
        prefix = _NAME_PREFIX.match(name)
        explicit = int(prefix.group(1)) if prefix else None
    index = explicit if isinstance(explicit, int) and 1 <= explicit <= SHOT_COUNT else None
    return index, Shot(index or order, str(name) if name is not None else f"plano_{order}", start, end)


def _group_shots(parsed: list[tuple[int | None, Shot]]) -> tuple[Shot, ...]:
    if len(parsed) == SHOT_COUNT and any(index is None for index, _ in parsed):
        ordered = sorted((shot for _, shot in parsed), key=lambda s: s.start)
        return tuple(Shot(i + 1, s.name, s.start, s.end) for i, s in enumerate(ordered))
    if any(index is None for index, _ in parsed):
        raise TimelineError(f"hay {len(parsed)} planos sin número de plano 1-{SHOT_COUNT}: no se pueden agrupar")
    groups: dict[int, list[Shot]] = {}
    for index, shot in parsed:
        groups.setdefault(index or 0, []).append(shot)
    if sorted(groups) != list(range(1, SHOT_COUNT + 1)):
        raise TimelineError(f"faltan planos: hay {sorted(groups)}")
    return tuple(
        Shot(i, groups[i][0].name, min(s.start for s in groups[i]), max(s.end for s in groups[i]))
        for i in range(1, SHOT_COUNT + 1)
    )


def _canonical_event(name: str) -> str | None:
    lowered = name.strip().lower()
    for canonical, aliases in _EVENT_ALIASES.items():
        if lowered in aliases:
            return canonical
    return None


def _parse_events(raw: Any, fps: int) -> dict[str, float]:
    events: dict[str, float] = {}
    if isinstance(raw, dict):
        items = [(str(k), v) for k, v in raw.items()]
    elif isinstance(raw, list):
        items = []
        for entry in raw:
            if isinstance(entry, dict) and _first(entry, ("name", "nombre", "event", "id")) is not None:
                items.append((str(_first(entry, ("name", "nombre", "event", "id"))), entry))
    else:
        return events
    for name, value in items:
        canonical = _canonical_event(name)
        if canonical is None:
            continue
        seconds: float | None = None
        if isinstance(value, (int, float)):
            seconds = float(value)
        elif isinstance(value, dict):
            seconds = _seconds(value, ("t", "time", "seconds", "at", "start"), ("frame", "startFrame"), fps)
        if seconds is not None:
            events[canonical] = seconds
    return events


def parse_timeline(data: dict[str, Any], source: str = "trailer.json") -> Timeline:
    fps_raw = data.get("fps", DEFAULT_FPS)
    if not isinstance(fps_raw, (int, float)) or fps_raw <= 0:
        raise TimelineError(f"fps no válido: {fps_raw!r}")
    fps = int(round(fps_raw))
    raw_shots = _first(data, ("shots", "planos"))
    if not isinstance(raw_shots, list) or not raw_shots:
        raise TimelineError("trailer.json no tiene lista de planos ('shots' o 'planos')")
    parsed = [_parse_shot(raw, i + 1, fps) for i, raw in enumerate(raw_shots)]
    shots = _group_shots(parsed)
    frames_raw = _first(data, ("frames", "frameCount", "totalFrames", "frame_count"))
    if isinstance(frames_raw, (int, float)) and frames_raw > 0:
        frames = int(frames_raw)
    else:
        duration = _first(data, ("duration", "durationSeconds", "duracion"))
        end = float(duration) if isinstance(duration, (int, float)) else shots[-1].end
        frames = int(round(end * fps))
    events = _parse_events(_first(data, ("events", "eventos", "marks")), fps)
    for raw in raw_shots:
        if isinstance(raw, dict):
            for name, value in _parse_events(_first(raw, ("events", "eventos")), fps).items():
                events.setdefault(name, value)
    parts = tuple(sorted((Shot(i or 0, sh.name, sh.start, sh.end) for i, sh in parsed if i is not None),
                         key=lambda sh: sh.start))
    return Timeline(fps, frames, shots, events, source, parts)


def load_timeline(path: str | Path | None) -> Timeline:
    """trailer.json si existe; si no, la línea de tiempo nominal."""
    if path is None or not Path(path).is_file():
        return nominal_timeline()
    data = json.loads(Path(path).read_text(encoding="utf-8"))
    if not isinstance(data, dict):
        raise TimelineError("trailer.json debe ser un objeto JSON")
    return parse_timeline(data, str(path))


def finale_events(frames_dir: Path, logs_dir: Path | None, fps: int) -> dict[str, float]:
    """Eventos del final grabados por el motor: report_*.json y, si faltan, los logs del tráiler."""
    frames: dict[str, int] = {}
    for report in sorted(frames_dir.glob("report_*.json")) if frames_dir.is_dir() else []:
        data = json.loads(report.read_text(encoding="utf-8"))
        marks = data.get("finaleFrames") if isinstance(data, dict) else None
        if isinstance(marks, dict):
            frames.update({str(k): int(v) for k, v in marks.items() if isinstance(v, (int, float))})
    if logs_dir is not None and logs_dir.is_dir():
        for log in sorted(logs_dir.glob("Trailer_*.log")):
            for name, frame in _LOG_EVENT.findall(log.read_text(encoding="utf-8", errors="replace")):
                frames.setdefault(name, int(frame))
    events: dict[str, float] = {}
    for name, frame in frames.items():
        canonical = _canonical_event(name)
        if canonical is not None:
            events[canonical] = frame / fps
    return events


def with_events(tl: Timeline, events: dict[str, float]) -> Timeline:
    """Copia con `events` encima de los que ya tenía la línea de tiempo."""
    return Timeline(tl.fps, tl.frames, tl.shots, {**tl.events, **events}, tl.source, tl.parts)
