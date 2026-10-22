from __future__ import annotations

import json
from pathlib import Path

import pytest

from explored_trailer.timeline import (
    HORN_AFTER_MISTRAL,
    TimelineError,
    load_timeline,
    nominal_timeline,
    parse_timeline,
)


def test_nominal_timeline_is_75_seconds_at_30_fps() -> None:
    tl = nominal_timeline()
    assert tl.frames == 2250 and tl.fps == 30
    assert tl.duration == pytest.approx(75.0)
    assert [s.index for s in tl.shots] == list(range(1, 9))
    assert tl.shot(7).start == 50.0


def test_missing_file_falls_back_to_nominal(tmp_path: Path) -> None:
    assert load_timeline(tmp_path / "nope.json").source == "nominal"
    assert load_timeline(None).source == "nominal"


def test_trailer_json_with_seconds_and_spanish_event_names(trailer_json: Path) -> None:
    tl = load_timeline(trailer_json)
    assert tl.shot(7).start == pytest.approx(50.4)
    assert tl.horn_time() == pytest.approx(63.5)
    assert tl.mistral_time() == pytest.approx(53.9)
    assert tl.ignite_time() == pytest.approx(50.9)


def test_horn_falls_back_to_finale_model_timing() -> None:
    tl = parse_timeline({"fps": 30, "frames": 2250, "shots": _shots(), "events": {"mistral_lights": 54.0}})
    assert tl.horn_time() == pytest.approx(54.0 + HORN_AFTER_MISTRAL)
    assert HORN_AFTER_MISTRAL == pytest.approx(9.6)


def test_default_events_derive_from_shot_7() -> None:
    tl = nominal_timeline()
    assert tl.ignite_time() == pytest.approx(50.0)
    assert tl.lit_time() == pytest.approx(53.0)
    assert tl.horn_time() == pytest.approx(62.6)


def test_frames_instead_of_seconds_and_event_list() -> None:
    shots = [{"startFrame": int(s * 30), "endFrame": int(e * 30)} for s, e in _bounds()]
    data = {"fps": 30, "shots": shots, "events": [{"name": "horn", "frame": 1890}, {"name": "otro", "t": 1.0}]}
    tl = parse_timeline(data)
    assert tl.frames == 2250
    assert tl.shot(3).start == pytest.approx(12.0)
    assert tl.horn_time() == pytest.approx(63.0)


def test_subshots_are_grouped_by_plano_number() -> None:
    shots = [
        {"plano": i, "inicio": s, "fin": e} for i, (s, e) in enumerate(_bounds(), start=1) if i != 3
    ] + [{"plano": 3, "inicio": 12.0, "fin": 15.0}, {"plano": 3, "inicio": 15.0, "fin": 22.0,
                                                   "eventos": {"bocina": 63.0}}]
    tl = parse_timeline({"fps": 30, "duration": 75.0, "planos": shots})
    assert tl.shot(3).start == 12.0 and tl.shot(3).end == 22.0
    assert tl.horn_time() == 63.0


@pytest.mark.parametrize(
    "data, message",
    [
        ({"fps": 0, "shots": []}, "fps"),
        ({"fps": 30}, "planos"),
        ({"fps": 30, "shots": ["x"]}, "objeto"),
        ({"fps": 30, "shots": [{"start": 2.0, "end": 1.0}]}, "inicio y fin"),
        ({"fps": 30, "shots": [{"start": 0.0, "end": 1.0}] * 3}, "sin número"),
        ({"fps": 30, "shots": [{"index": 1, "start": 0.0, "end": 1.0}]}, "faltan"),
    ],
)
def test_bad_structures_fail_clearly(data: dict, message: str) -> None:
    with pytest.raises(TimelineError, match=message):
        parse_timeline(data)


def test_non_object_json_is_rejected(tmp_path: Path) -> None:
    path = tmp_path / "trailer.json"
    path.write_text(json.dumps([1, 2]), encoding="utf-8")
    with pytest.raises(TimelineError):
        load_timeline(path)


def test_unknown_shot_raises_key_error() -> None:
    with pytest.raises(KeyError):
        nominal_timeline().shot(9)


def _bounds() -> list[tuple[float, float]]:
    return [(0, 4), (4, 12), (12, 22), (22, 32), (32, 42), (42, 50), (50, 68), (68, 75)]


def _shots() -> list[dict]:
    return [{"start": s, "end": e} for s, e in _bounds()]


def test_engine_format_groups_subshots_by_name_prefix() -> None:
    names = ["01-mar", "02-cala", "03a-palmar", "03b-rocas", "03c-interfaz", "03d-cenital", "04a-camino",
             "04b-galeria", "05a-orilla", "05b-refugio", "06-carguero", "07-final", "08-titulo"]
    bounds = [0, 4, 12, 14.7, 17, 20, 22, 27, 32, 37, 42, 50, 68, 75]
    shots = [{"name": n, "start": bounds[i], "end": bounds[i + 1], "frames": 1} for i, n in enumerate(names)]
    tl = parse_timeline({"fps": 30, "width": 1920, "shots": shots})
    assert tl.frames == 2250
    assert (tl.shot(3).start, tl.shot(3).end) == (12, 22)
    assert [p.name for p in tl.parts_of(3)] == ["03a-palmar", "03b-rocas", "03c-interfaz", "03d-cenital"]
    assert tl.parts_of(1) == ()
    assert tl.ignite_time() == 50.0 and tl.horn_time() == pytest.approx(62.6)


def test_finale_events_from_engine_reports_and_logs(tmp_path: Path) -> None:
    from explored_trailer.timeline import finale_events, with_events

    frames = tmp_path / "frames"
    frames.mkdir()
    (frames / "report_07-final.json").write_text(json.dumps({"finaleFrames": {"ignite": 1500, "mistral": 1590}}),
                                                 encoding="utf-8")
    (frames / "report_x.json").write_text(json.dumps([1]), encoding="utf-8")
    logs = tmp_path / "Logs"
    logs.mkdir()
    (logs / "Trailer_7.log").write_text("LogExplored: Display: [Trailer] final: horn en el fotograma 1878\n"
                                        "[Trailer] final: mistral en el fotograma 9999\n", encoding="utf-8")
    events = finale_events(frames, logs, 30)
    assert events == {"ignite": 50.0, "mistral": 53.0, "horn": pytest.approx(62.6)}
    assert finale_events(tmp_path / "none", None, 30) == {}
    tl = with_events(nominal_timeline(), events)
    assert tl.horn_time() == pytest.approx(62.6) and tl.lit_time() == pytest.approx(53.0)
