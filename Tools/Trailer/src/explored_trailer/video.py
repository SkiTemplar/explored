"""Montaje con ffmpeg: fondo (fotogramas del motor o ensayo con capturas), rótulos y mezcla.

Salida: H.264 yuv420p (CRF 17, preset slow, +faststart, BT.709) a 1920×1080 y 30 fps, con
AAC 256 kbps estéreo a 48 kHz. ffmpeg corre con prioridad baja y un tope de hilos para no
estorbar a quien esté usando el PC.
"""

from __future__ import annotations

import json
import os
import subprocess
from dataclasses import dataclass
from pathlib import Path

from .cards import FADE_SECONDS, HEIGHT, WIDTH, RenderedCard
from .timeline import Timeline

CRF = 17
PRESET = "slow"
AUDIO_BITRATE = "256k"
DEFAULT_THREADS = 8
FRAME_PATTERN = "f_%05d.png"

# Ensayo: capturas de docs/v3/capturas por plano (el tiempo del plano se reparte entre ellas).
REHEARSAL_SHOTS: dict[int, tuple[str, ...]] = {
    1: ("f7-cesped-cala.png",),
    2: ("f8-arena-orilla.png", "f8-arena-sol.png"),
    3: ("f8-cesped-palmar.png", "f8-cesped-colina.png", "f5-hud.png", "f8-cesped-lejos.png"),
    4: ("f7-cesped-faro.png", "f6-relieve-atardecer.png"),
    5: ("f6-lluvia-ciclon.png", "f4b-ciclon.png"),
    6: ("f6-relieve-islote.png",),
    7: ("f6-final-encender.png", "f6-final-haz.png", "f6-final-mistral.png"),
    8: ("f6-final-haz.png",),
}


@dataclass(frozen=True)
class Segment:
    image: Path
    frames: int
    zoom: float
    pan: float


def run_ffmpeg(args: list[str], log_path: Path | None = None) -> None:
    """ffmpeg con prioridad baja; si falla, lanza con la cola del log."""
    cmd = ["ffmpeg", "-hide_banner", "-nostats", "-loglevel", "warning", "-y", *args]
    flags = getattr(subprocess, "BELOW_NORMAL_PRIORITY_CLASS", 0) if os.name == "nt" else 0
    result = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace",
                            check=False, creationflags=flags)
    if log_path is not None:
        log_path.parent.mkdir(parents=True, exist_ok=True)
        log_path.write_text(" ".join(cmd) + "\n\n" + result.stderr, encoding="utf-8")
    if result.returncode != 0:
        raise RuntimeError(f"ffmpeg falló ({result.returncode}): {result.stderr[-800:]}")


def split_frames(boundaries: list[float], fps: int, total: int) -> list[int]:
    """Fotogramas por tramo redondeando los cortes acumulados: la suma es exactamente `total`."""
    cuts = [0] + [min(total, int(round(b * fps))) for b in boundaries[1:-1]] + [total]
    return [b - a for a, b in zip(cuts, cuts[1:], strict=False)]


def rehearsal_segments(tl: Timeline, captures: Path) -> list[Segment]:
    times: list[float] = [0.0]
    images: list[Path] = []
    for shot in tl.shots:
        names = REHEARSAL_SHOTS[shot.index]
        step = shot.duration / len(names)
        for k, name in enumerate(names):
            images.append(captures / name)
            times.append(shot.start + (k + 1) * step)
    times[-1] = tl.duration
    frames = split_frames(times, tl.fps, tl.frames)
    return [Segment(img, n, 0.06 + 0.02 * (i % 3), (-1) ** i * 0.03) for i, (img, n) in enumerate(zip(images, frames,
                                                                                                    strict=True))]


def rehearsal_filter(segments: list[Segment], fps: int) -> str:
    """zoompan lento sobre cada captura (ampliada a 4K para que el encuadre no tiemble)."""
    parts = []
    for i, seg in enumerate(segments):
        n = max(seg.frames, 1)
        zoom = f"1+{seg.zoom:.3f}*on/{n}"
        x = f"(iw-iw/zoom)/2+{seg.pan:.3f}*iw*on/{n}"
        parts.append(
            f"[{i}:v]scale={2 * WIDTH}:{2 * HEIGHT}:flags=lanczos,format=yuv444p,"
            f"zoompan=z='{zoom}':x='{x}':y='(ih-ih/zoom)/2':d={n}:s={WIDTH}x{HEIGHT}:fps={fps},"
            f"trim=end_frame={n},setpts=PTS-STARTPTS,setsar=1[s{i}]"
        )
    labels = "".join(f"[s{i}]" for i in range(len(segments)))
    parts.append(f"{labels}concat=n={len(segments)}:v=1:a=0,format=yuv420p[bg]")
    return ";".join(parts)


def render_rehearsal_background(tl: Timeline, captures: Path, out: Path, threads: int = DEFAULT_THREADS) -> Path:
    segments = rehearsal_segments(tl, captures)
    missing = [str(s.image) for s in segments if not s.image.is_file()]
    if missing:
        raise FileNotFoundError(f"faltan capturas para el ensayo: {missing}")
    args: list[str] = []
    for seg in segments:
        args += ["-i", str(seg.image)]
    args += ["-filter_complex", rehearsal_filter(segments, tl.fps), "-map", "[bg]", "-frames:v", str(tl.frames),
             "-r", str(tl.fps), "-c:v", "libx264", "-preset", "veryfast", "-crf", "12", "-threads", str(threads),
             str(out)]
    out.parent.mkdir(parents=True, exist_ok=True)
    run_ffmpeg(args, out.with_suffix(".log"))
    return out


def frames_input(frames_dir: Path, fps: int) -> list[str]:
    return ["-framerate", str(fps), "-start_number", "0", "-i", str(frames_dir / FRAME_PATTERN)]


def count_frames(frames_dir: Path) -> int:
    return len(list(frames_dir.glob("f_*.png"))) if frames_dir.is_dir() else 0


def card_filters(cards: tuple[RenderedCard, ...], first_input: int) -> tuple[str, str]:
    """Cadena de overlays con fundidos de 0,5 s; devuelve (filtros, etiqueta de salida)."""
    parts = []
    current = "base"
    for k, rc in enumerate(cards):
        idx = first_input + k
        start, end = rc.card.start, rc.card.end
        parts.append(
            f"[{idx}:v]format=rgba,fade=t=in:st={start:.3f}:d={FADE_SECONDS}:alpha=1,"
            f"fade=t=out:st={end - FADE_SECONDS:.3f}:d={FADE_SECONDS}:alpha=1[c{k}]"
        )
        nxt = f"o{k}"
        parts.append(
            f"[{current}][c{k}]overlay=x={rc.x}:y={rc.y}:eof_action=pass:"
            f"enable='between(t,{start:.3f},{end:.3f})'[{nxt}]"
        )
        current = nxt
    return ";".join(parts), current


def compose_args(base_input: list[str], cards: tuple[RenderedCard, ...], audio: Path, out: Path, tl: Timeline,
                 threads: int = DEFAULT_THREADS) -> list[str]:
    args = list(base_input)
    for rc in cards:
        args += ["-loop", "1", "-framerate", str(tl.fps), "-t", f"{rc.card.end + 0.2:.3f}", "-i", str(rc.path)]
    audio_index = 1 + len(cards)
    args += ["-i", str(audio)]
    duration = tl.frames / tl.fps
    overlays, last = card_filters(cards, 1)
    graph = [f"[0:v]setpts=PTS-STARTPTS,fps={tl.fps},scale={WIDTH}:{HEIGHT}:flags=lanczos,setsar=1[base]"]
    if overlays:
        graph.append(overlays)
    graph.append(
        f"[{last}]fade=t=out:st={duration - 1.0:.3f}:d=1,"
        f"scale=out_color_matrix=bt709:out_range=tv,format=yuv420p[vout]"
    )
    args += [
        "-filter_complex", ";".join(graph), "-map", "[vout]", "-map", f"{audio_index}:a",
        "-c:v", "libx264", "-preset", PRESET, "-crf", str(CRF), "-pix_fmt", "yuv420p", "-r", str(tl.fps),
        "-colorspace", "bt709", "-color_primaries", "bt709", "-color_trc", "bt709", "-color_range", "tv",
        "-c:a", "aac", "-b:a", AUDIO_BITRATE, "-ar", "48000", "-ac", "2",
        "-frames:v", str(tl.frames), "-t", f"{duration:.3f}", "-movflags", "+faststart",
        "-threads", str(threads), str(out),
    ]
    return args


def compose(base_input: list[str], cards: tuple[RenderedCard, ...], audio: Path, out: Path, tl: Timeline,
            threads: int = DEFAULT_THREADS) -> Path:
    out.parent.mkdir(parents=True, exist_ok=True)
    run_ffmpeg(compose_args(base_input, cards, audio, out, tl, threads), out.with_suffix(".log"))
    return out


@dataclass(frozen=True)
class ProbeResult:
    duration: float
    width: int
    height: int
    fps: float
    video_codec: str
    pix_fmt: str
    audio_codec: str
    channels: int
    sample_rate: int

    def problems(self, expected_duration: float, tolerance: float = 0.1) -> list[str]:
        issues = []
        if abs(self.duration - expected_duration) > tolerance:
            issues.append(f"duración {self.duration:.3f} s, se esperaba {expected_duration:.3f} ± {tolerance}")
        if (self.width, self.height) != (WIDTH, HEIGHT):
            issues.append(f"resolución {self.width}x{self.height}")
        if abs(self.fps - 30.0) > 1e-3:
            issues.append(f"{self.fps} fps")
        if self.video_codec != "h264" or self.pix_fmt != "yuv420p":
            issues.append(f"vídeo {self.video_codec}/{self.pix_fmt}")
        if self.audio_codec != "aac" or self.channels != 2 or self.sample_rate != 48000:
            issues.append(f"audio {self.audio_codec} {self.channels} canales {self.sample_rate} Hz")
        return issues


def _rate(text: str) -> float:
    num, _, den = text.partition("/")
    return float(num) / float(den or 1)


def parse_probe(data: dict) -> ProbeResult:
    streams = data.get("streams", [])
    video = next((s for s in streams if s.get("codec_type") == "video"), None)
    audio = next((s for s in streams if s.get("codec_type") == "audio"), None)
    if video is None or audio is None:
        raise ValueError("el vídeo no tiene pista de imagen y de audio")
    return ProbeResult(
        duration=float(data.get("format", {}).get("duration", 0.0)),
        width=int(video["width"]), height=int(video["height"]), fps=_rate(video.get("r_frame_rate", "0/1")),
        video_codec=str(video.get("codec_name")), pix_fmt=str(video.get("pix_fmt")),
        audio_codec=str(audio.get("codec_name")), channels=int(audio.get("channels", 0)),
        sample_rate=int(audio.get("sample_rate", 0)),
    )


def probe(path: Path) -> ProbeResult:
    cmd = ["ffprobe", "-v", "error", "-show_format", "-show_streams", "-of", "json", str(path)]
    result = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", check=False)
    if result.returncode != 0:
        raise RuntimeError(f"ffprobe falló: {result.stderr[-400:]}")
    return parse_probe(json.loads(result.stdout))


def extract_frame(video: Path, seconds: float, out: Path) -> Path:
    out.parent.mkdir(parents=True, exist_ok=True)
    run_ffmpeg(["-ss", f"{seconds:.3f}", "-i", str(video), "-frames:v", "1", str(out)])
    return out
