"""Informe de niveles de la mezcla (JSON + texto) y gráfica de sonoridad por plano (PNG)."""

from __future__ import annotations

import json
from dataclasses import asdict
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

from .audio_io import SAMPLE_RATE, seconds_to_samples
from .loudness import LoudnormReport, window_lufs
from .mix import CEILING_DBTP, TARGET_LUFS, MixResult, discontinuity_ratio
from .timeline import Timeline

PEAK_LIMIT_DBTP = -1.5
LUFS_TOLERANCE = 0.5
HORN_MIN_MARGIN_DB = 4.0
CLICK_RATIO = 3.0


def short_term(audio: np.ndarray, hop_s: float = 0.25, window_s: float = 3.0) -> np.ndarray:
    hop, half = seconds_to_samples(hop_s), seconds_to_samples(window_s / 2)
    centers = range(0, len(audio), hop)
    return np.array([window_lufs(audio, max(0, c - half), min(len(audio), c + half)) for c in centers])


def per_shot(audio: np.ndarray, tl: Timeline) -> dict[str, float]:
    return {
        f"{s.index}_{s.name}": round(window_lufs(audio, seconds_to_samples(s.start), seconds_to_samples(s.end)), 1)
        for s in tl.shots
    }


def build_report(result: MixResult, tl: Timeline, measured: LoudnormReport) -> dict:
    splices = {f"{t:.3f}": round(discontinuity_ratio(result.mix, t), 2) for t in result.splices if t > 0.0}
    checks = {
        "integrated_ok": abs(measured.integrated_lufs - TARGET_LUFS) <= LUFS_TOLERANCE,
        "true_peak_ok": measured.true_peak_dbtp < PEAK_LIMIT_DBTP,
        "horn_margin_ok": result.horn_margin_db >= HORN_MIN_MARGIN_DB,
        "splices_ok": all(v < CLICK_RATIO for v in splices.values()),
    }
    return {
        "timeline": tl.source,
        "duration_s": round(len(result.mix) / SAMPLE_RATE, 3),
        "ffmpeg_loudnorm": asdict(measured),
        "target": {"integrated_lufs": TARGET_LUFS, "true_peak_max_dbtp": PEAK_LIMIT_DBTP,
                   "limiter_ceiling_dbtp": CEILING_DBTP},
        "own_meter": {"integrated_lufs": round(result.own_lufs, 2), "true_peak_dbtp": round(result.own_true_peak_db, 2)},
        "master_gain_db": round(result.master_gain_db, 2),
        "horn": {"at_s": round(result.horn_at, 3), "window_s": [round(t, 2) for t in result.horn_window],
                 "margin_over_music_lu": round(result.horn_margin_db, 2)},
        "music_cuts": [
            {"piece": c.piece_id, "src": [round(c.src_start, 3), round(c.src_end, 3)],
             "dst": [round(c.dst_start, 3), round(c.dst_end, 3)], "fade_in": c.fade_in, "fade_out": c.fade_out,
             "gain_db": c.gain_db, "note": c.note}
            for c in result.cuts
        ],
        "splice_discontinuity_ratio": splices,
        "per_shot_lufs": per_shot(result.mix, tl),
        "checks": checks,
        "ok": all(checks.values()),
    }


def format_report(report: dict) -> str:
    m = report["ffmpeg_loudnorm"]
    lines = [
        f"Mezcla del tráiler ({report['timeline']}), {report['duration_s']} s",
        f"ffmpeg loudnorm: {m['integrated_lufs']} LUFS integrados, {m['true_peak_dbtp']} dBTP, LRA {m['lra']} LU",
        f"Medidor propio: {report['own_meter']['integrated_lufs']} LUFS, {report['own_meter']['true_peak_dbtp']} dBTP",
        f"Bocina a {report['horn']['at_s']} s: {report['horn']['margin_over_music_lu']} LU sobre la música",
        "Cortes de música:",
        *[f"  {c['dst'][0]:6.2f}-{c['dst'][1]:6.2f} s  {c['piece']} [{c['src'][0]}-{c['src'][1]}]  {c['note']}"
          for c in report["music_cuts"]],
        "Empalmes (salto / p99,9 del entorno; < 3 = sin clic): "
        + ", ".join(f"{k} s → {v}" for k, v in report["splice_discontinuity_ratio"].items()),
        "Sonoridad por plano (LUFS): " + ", ".join(f"{k} {v}" for k, v in report["per_shot_lufs"].items()),
        "Comprobaciones: " + ", ".join(f"{k}={'sí' if v else 'NO'}" for k, v in report["checks"].items()),
    ]
    return "\n".join(lines)


def write_report(report: dict, folder: Path) -> tuple[Path, Path]:
    folder.mkdir(parents=True, exist_ok=True)
    json_path = folder / "levels.json"
    text_path = folder / "levels.txt"
    json_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    text_path.write_text(format_report(report) + "\n", encoding="utf-8")
    return json_path, text_path


def plot_levels(result: MixResult, tl: Timeline, out: Path, size: tuple[int, int] = (1600, 500)) -> Path:
    """Sonoridad a corto plazo (3 s) de la mezcla, la música y la bocina, con los planos marcados."""
    width, height = size
    image = Image.new("RGB", size, (24, 28, 40))
    draw = ImageDraw.Draw(image)
    floor, top = -50.0, -6.0
    duration = len(result.mix) / SAMPLE_RATE

    def xy(t: float, db: float) -> tuple[float, float]:
        level = min(max(db, floor), top)
        return 40 + (width - 60) * t / duration, height - 30 - (height - 50) * (level - floor) / (top - floor)

    for db in range(-48, -5, 6):
        draw.line([xy(0, db), xy(duration, db)], fill=(50, 56, 70))
        draw.text((4, xy(0, db)[1] - 6), str(db), fill=(140, 140, 150))
    for shot in tl.shots:
        draw.line([xy(shot.start, floor), xy(shot.start, top)], fill=(90, 90, 110))
        draw.text((xy(shot.start, top)[0] + 3, 6), str(shot.index), fill=(200, 200, 210))
    draw.line([xy(result.horn_at, floor), xy(result.horn_at, top)], fill=(240, 140, 46), width=2)
    for audio, color in ((result.music, (159, 214, 224)), (result.horn, (240, 140, 46)), (result.mix, (246, 238, 219))):
        curve = short_term(audio)
        points = [xy(i * 0.25, float(v)) for i, v in enumerate(curve) if np.isfinite(v)]
        if len(points) > 1:
            draw.line(points, fill=color, width=2)
    out.parent.mkdir(parents=True, exist_ok=True)
    image.save(out)
    return out
