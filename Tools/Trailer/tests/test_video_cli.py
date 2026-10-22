from __future__ import annotations

import shutil
from pathlib import Path

import numpy as np
import pytest
from conftest import REPO, tone, write_wav

from explored_trailer import cli
from explored_trailer.cards import Bilingual, Card, RenderedCard
from explored_trailer.timeline import nominal_timeline
from explored_trailer.video import (
    REHEARSAL_SHOTS,
    ProbeResult,
    card_filters,
    compose_args,
    count_frames,
    frames_input,
    parse_probe,
    rehearsal_filter,
    rehearsal_segments,
    run_ffmpeg,
    split_frames,
)


def test_split_frames_sums_exactly() -> None:
    frames = split_frames([0.0, 1.01, 2.52, 3.0], 30, 90)
    assert sum(frames) == 90 and frames == [30, 46, 14]


def test_rehearsal_segments_cover_every_frame() -> None:
    tl = nominal_timeline()
    segments = rehearsal_segments(tl, REPO / "docs/v3/capturas")
    assert sum(s.frames for s in segments) == tl.frames
    assert len(segments) == sum(len(v) for v in REHEARSAL_SHOTS.values())
    assert all(s.image.is_file() for s in segments)
    graph = rehearsal_filter(segments, 30)
    assert graph.count("zoompan") == len(segments) and graph.endswith("[bg]")


def _card(start: float, end: float, path: Path) -> RenderedCard:
    return RenderedCard(Card("x", "line", start, end, Bilingual("a", "b")), path, 100, 800)


def test_card_filters_fade_and_enable_window(tmp_path: Path) -> None:
    graph, last = card_filters((_card(1.0, 3.0, tmp_path / "a.png"), _card(5.0, 7.0, tmp_path / "b.png")), 1)
    assert last == "o1"
    assert "fade=t=in:st=1.000:d=0.5:alpha=1" in graph and "fade=t=out:st=6.500:d=0.5:alpha=1" in graph
    assert "enable='between(t,5.000,7.000)'" in graph and "overlay=x=100:y=800" in graph
    assert card_filters((), 1) == ("", "base")


def test_compose_args_match_the_delivery_spec(tmp_path: Path) -> None:
    tl = nominal_timeline()
    args = compose_args(frames_input(tmp_path, 30), (_card(1.0, 3.0, tmp_path / "a.png"),), tmp_path / "m.wav",
                        tmp_path / "o.mp4", tl, threads=4)
    joined = " ".join(args)
    for fragment in ("-c:v libx264", "-preset slow", "-crf 17", "-pix_fmt yuv420p", "-r 30", "-c:a aac",
                     "-b:a 256k", "-ar 48000", "-ac 2", "-movflags +faststart", "-frames:v 2250", "-t 75.000",
                     "f_%05d.png", "fade=t=out:st=74.000:d=1", "-map 2:a", "-threads 4"):
        assert fragment in joined
    assert count_frames(tmp_path) == 0 and count_frames(tmp_path / "no") == 0


def _probe_data(duration: str = "75.0", channels: int = 2) -> dict:
    return {
        "format": {"duration": duration},
        "streams": [
            {"codec_type": "video", "codec_name": "h264", "pix_fmt": "yuv420p", "width": 1920, "height": 1080,
             "r_frame_rate": "30/1"},
            {"codec_type": "audio", "codec_name": "aac", "channels": channels, "sample_rate": "48000"},
        ],
    }


def test_probe_parsing_and_problems() -> None:
    good = parse_probe(_probe_data())
    assert isinstance(good, ProbeResult) and good.problems(75.0) == []
    bad = parse_probe(_probe_data("74.5", 1))
    problems = bad.problems(75.0)
    assert any("duración" in p for p in problems) and any("audio" in p for p in problems)
    with pytest.raises(ValueError):
        parse_probe({"streams": []})


def test_run_ffmpeg_reports_failures(tmp_path: Path) -> None:
    with pytest.raises(RuntimeError):
        run_ffmpeg(["-i", str(tmp_path / "missing.wav"), str(tmp_path / "x.wav")], tmp_path / "log.txt")
    assert (tmp_path / "log.txt").is_file()


@pytest.fixture()
def fake_repo(tmp_path: Path, fake_audio_root: Path) -> Path:
    root = tmp_path / "repo"
    shutil.copytree(fake_audio_root, root / "Art/Export/Audio")
    shutil.copytree(REPO / "Content/Slate/Fonts", root / "Content/Slate/Fonts")
    (root / "Tools/Style").mkdir(parents=True)
    shutil.copy(REPO / "Tools/Style/paleta.json", root / "Tools/Style/paleta.json")
    (root / "Tools/Audio").mkdir(parents=True)
    shutil.copy(REPO / "Tools/Audio/music_sources.json", root / "Tools/Audio/music_sources.json")
    return root


def test_cli_mix_cards_and_credits(fake_repo: Path, capsys: pytest.CaptureFixture[str]) -> None:
    assert cli.main(["mix", "--root", str(fake_repo)]) == 0
    out = fake_repo / "Saved/Trailer"
    assert (out / "audio/mix.wav").is_file() and (out / "audio/levels.json").is_file()
    assert (out / "audio/levels.png").is_file()
    assert cli.main(["cards", "--root", str(fake_repo), "--langs", "en"]) == 0
    assert len(list((out / "cards/en").glob("*.png"))) == 9
    assert cli.main(["credits", "--root", str(fake_repo)]) == 0
    assert (fake_repo / "docs/creditos-trailer.md").is_file() and (out / "descripcion.md").is_file()
    printed = capsys.readouterr().out
    assert "LUFS" in printed and "Comprobaciones" in printed


def test_cli_assemble_refuses_without_real_timeline_or_frames(fake_repo: Path, trailer_json: Path) -> None:
    with pytest.raises(SystemExit, match="trailer.json"):
        cli.main(["assemble", "--root", str(fake_repo)])
    with pytest.raises(SystemExit, match="fotogramas"):
        cli.main(["--root", str(fake_repo), "--timeline", str(trailer_json)])


def test_cli_analyze_and_bad_languages(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    wav = write_wav(tmp_path / "p.wav", tone(2.0))
    assert cli.main(["analyze", str(wav), "--hop", "1.0"]) == 0
    assert "dB" in capsys.readouterr().out
    with pytest.raises(SystemExit):
        cli.main(["analyze"])
    with pytest.raises(SystemExit):
        cli.languages("fr")
    assert cli.languages("en, es") == ("es", "en")


def test_verify_extracts_stills_and_reports(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    from explored_trailer.loudness import LoudnormReport

    tl = nominal_timeline()
    monkeypatch.setattr(cli, "probe", lambda _p: parse_probe(_probe_data("70.0")))
    monkeypatch.setattr(cli, "ffmpeg_loudnorm", lambda _p: LoudnormReport(-16.0, -0.5, 8.0, -26.0))
    grabbed: list[Path] = []
    monkeypatch.setattr(cli, "extract_frame", lambda _v, _t, out: grabbed.append(out) or out)
    problems = cli.verify(tmp_path / "v.mp4", tl, tmp_path / "stills")
    assert any("duración" in p for p in problems) and any("pico" in p for p in problems)
    assert len(grabbed) == 7


def test_run_video_composes_each_language(fake_repo: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    calls: list[Path] = []
    monkeypatch.setattr(cli, "compose", lambda _b, _c, _a, out, _tl, _th: calls.append(out) or out)
    monkeypatch.setattr(cli, "verify", lambda _v, _tl, _s: [])
    args = cli.parse_args(["rehearsal", "--root", str(fake_repo)])
    paths = cli.resolve_paths(args)
    assert cli.run_video(nominal_timeline(), paths, args, ["-i", "bg.mp4"], "-ensayo") == 0
    assert [p.name for p in calls] == ["baliza-siete-trailer-es-ensayo.mp4", "baliza-siete-trailer-en-ensayo.mp4"]
    monkeypatch.setattr(cli, "verify", lambda _v, _tl, _s: ["mal"])
    assert cli.run_video(nominal_timeline(), paths, cli.parse_args(["--root", str(fake_repo), "--langs", "es"]),
                         ["-i", "bg.mp4"], "") == 1
    assert np.isfinite(len(calls))


def test_cli_event_overrides() -> None:
    tl = nominal_timeline()
    args = cli.parse_args(["mix", "--mistral-at", "54.0"])
    assert cli.with_overrides(tl, args).horn_time() == pytest.approx(63.6)
    args = cli.parse_args(["mix", "--horn-at", "62.0"])
    assert cli.with_overrides(tl, args).horn_time() == 62.0
    assert cli.with_overrides(tl, cli.parse_args([])).events == {}
