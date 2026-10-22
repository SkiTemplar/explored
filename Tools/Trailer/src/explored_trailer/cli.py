"""CLI del tráiler: `uv run python assemble.py <orden>` (o `uv run explored-trailer <orden>`).

Órdenes:
  assemble   (por defecto) montaje final con los fotogramas del motor (Saved/Trailer/frames)
  rehearsal  ensayo completo con capturas de docs/v3/capturas como fondo
  mix        solo la mezcla (Saved/Trailer/audio/mix.wav + informe de niveles)
  cards      solo los rótulos PNG
  credits    créditos (docs/creditos-trailer.md) y Saved/Trailer/descripcion.md
  analyze    envolvente de una pieza para elegir cortes
"""

from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass
from pathlib import Path

from .analyze import analyze, format_envelope
from .cards import LANGUAGES, Fonts, Language, Layout, Palette, render_cards
from .credits import write_credits
from .loudness import ffmpeg_loudnorm
from .mix import TARGET_LUFS, build_mix, make_loader, write_mix
from .music import music_plan
from .report import build_report, format_report, plot_levels, write_report
from .timeline import Timeline, finale_events, load_timeline, with_events
from .video import (
    DEFAULT_THREADS,
    compose,
    count_frames,
    extract_frame,
    frames_input,
    probe,
    render_rehearsal_background,
)

REPO_ROOT = Path(__file__).resolve().parents[4]
CALIBRATION_TOLERANCE = 0.1


@dataclass(frozen=True)
class Paths:
    root: Path
    timeline: Path
    out: Path
    frames: Path

    @property
    def audio_root(self) -> Path:
        return self.root / "Art/Export/Audio"

    @property
    def music_root(self) -> Path:
        return self.audio_root / "Grabada"

    @property
    def mix(self) -> Path:
        return self.out / "audio/mix.wav"

    @property
    def fonts(self) -> Path:
        return self.root / "Content/Slate/Fonts"

    @property
    def palette(self) -> Path:
        return self.root / "Tools/Style/paleta.json"

    @property
    def captures(self) -> Path:
        return self.root / "docs/v3/capturas"

    @property
    def music_sources(self) -> Path:
        return self.root / "Tools/Audio/music_sources.json"


def parse_args(argv: list[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(prog="assemble.py", description="Tráiler de Baliza Siete")
    parser.add_argument("command", nargs="?", default="assemble",
                        choices=["assemble", "rehearsal", "mix", "cards", "credits", "analyze"])
    parser.add_argument("wav", nargs="?", help="pieza a analizar (orden analyze)")
    parser.add_argument("--root", type=Path, default=REPO_ROOT, help="raíz del repositorio")
    parser.add_argument("--timeline", type=Path, help="trailer.json (por defecto Tools/Trailer/trailer.json)")
    parser.add_argument("--out", type=Path, help="carpeta de salida (por defecto Saved/Trailer)")
    parser.add_argument("--frames", type=Path, help="fotogramas f_%%05d.png (por defecto Saved/Trailer/frames)")
    parser.add_argument("--langs", default="es,en", help="idiomas: es, en o es,en")
    parser.add_argument("--threads", type=int, default=DEFAULT_THREADS)
    parser.add_argument("--line-y", type=float, default=Layout.line_y, help="línea base de las frases (0-1)")
    parser.add_argument("--word-x", type=float, default=Layout.word_x)
    parser.add_argument("--word-y", type=float, default=Layout.word_y)
    parser.add_argument("--title-y", type=float, default=Layout.title_y)
    parser.add_argument("--title-x", type=float, default=Layout.title_x, help="centro horizontal del título (0-1)")
    parser.add_argument("--hop", type=float, default=0.5, help="ventana del análisis (s)")
    parser.add_argument("--horn-at", type=float, help="fuerza el instante de la bocina (s), p. ej. sacado del log")
    parser.add_argument("--mistral-at", type=float, help="fuerza la aparición de las luces del Mistral (s)")
    return parser.parse_args(argv)


def resolve_paths(args: argparse.Namespace) -> Paths:
    root = args.root.resolve()
    out = args.out or root / "Saved/Trailer"
    return Paths(root, args.timeline or root / "Tools/Trailer/trailer.json", out, args.frames or out / "frames")


def languages(text: str) -> tuple[Language, ...]:
    wanted = {t.strip() for t in text.split(",")}
    picked: tuple[Language, ...] = tuple(lang for lang in LANGUAGES if lang in wanted)
    if not picked:
        raise SystemExit(f"idiomas no válidos: {text!r} (es, en)")
    return picked


def with_overrides(tl: Timeline, args: argparse.Namespace) -> Timeline:
    """Eventos dados a mano en la CLI (mandan sobre trailer.json)."""
    events = dict(tl.events)
    if args.mistral_at is not None:
        events["mistral"] = args.mistral_at
    if args.horn_at is not None:
        events["horn"] = args.horn_at
    return with_events(tl, events)


def run_mix(tl: Timeline, paths: Paths) -> dict:
    """Mezcla, mide con ffmpeg y recalibra una vez si se aparta más de 0,1 LU del objetivo."""
    load = make_loader(paths.audio_root, paths.music_root)
    result = build_mix(tl, load)
    write_mix(result, paths.mix)
    measured = ffmpeg_loudnorm(paths.mix)
    trim = TARGET_LUFS - measured.integrated_lufs
    if abs(trim) > CALIBRATION_TOLERANCE:
        result = build_mix(tl, load, trim_db=trim)
        write_mix(result, paths.mix)
        measured = ffmpeg_loudnorm(paths.mix)
    report = build_report(result, tl, measured)
    write_report(report, paths.mix.parent)
    plot_levels(result, tl, paths.mix.parent / "levels.png")
    print(format_report(report))
    return report


def run_cards(tl: Timeline, paths: Paths, args: argparse.Namespace, lang: Language):
    layout = Layout(args.line_y, args.word_x, args.word_y, args.title_y, args.title_x)
    return render_cards(tl, lang, paths.out / "cards" / lang, Fonts.from_dir(paths.fonts),
                        Palette.load(paths.palette), layout)


def run_credits(tl: Timeline, paths: Paths) -> None:
    docs, desc = write_credits(paths.music_sources, music_plan(tl), paths.root / "docs/creditos-trailer.md",
                               paths.out / "descripcion.md", tl.duration)
    print(f"créditos: {docs}\ndescripción: {desc}")


def verify(video: Path, tl: Timeline, stills: Path) -> list[str]:
    result = probe(video)
    problems = result.problems(tl.duration)
    measured = ffmpeg_loudnorm(video)
    print(f"{video.name}: {result.duration:.3f} s, {result.width}x{result.height}, {result.fps:g} fps, "
          f"{result.video_codec}/{result.pix_fmt}, {result.audio_codec} {result.channels} canales "
          f"{result.sample_rate} Hz; {measured.integrated_lufs} LUFS, {measured.true_peak_dbtp} dBTP")
    if measured.true_peak_dbtp >= -1.0:
        problems.append(f"pico verdadero del AAC {measured.true_peak_dbtp} dBTP")
    for t in (2.0, 9.0, 14.0, 37.0, 46.0, 60.0, 71.5):
        extract_frame(video, min(t, tl.duration - 0.1), stills / f"{video.stem}_{t:05.1f}s.png")
    return problems


def run_video(tl: Timeline, paths: Paths, args: argparse.Namespace, base_input: list[str], suffix: str) -> int:
    run_mix(tl, paths)
    run_credits(tl, paths)
    failures: list[str] = []
    for lang in languages(args.langs):
        cards = run_cards(tl, paths, args, lang)
        out = paths.out / f"baliza-siete-trailer-{lang}{suffix}.mp4"
        compose(base_input, cards, paths.mix, out, tl, args.threads)
        failures += [f"{out.name}: {p}" for p in verify(out, tl, paths.out / "stills")]
    for failure in failures:
        print(f"FALLO {failure}", file=sys.stderr)
    return 1 if failures else 0


def utf8_console() -> None:
    """La consola de Windows usa cp1252: sin esto, una flecha o una tilde rompen el informe."""
    for stream in (sys.stdout, sys.stderr):
        reconfigure = getattr(stream, "reconfigure", None)
        if callable(reconfigure):
            reconfigure(encoding="utf-8", errors="replace")


def main(argv: list[str] | None = None) -> int:
    utf8_console()
    args = parse_args(argv)
    if args.command == "analyze":
        if not args.wav:
            raise SystemExit("analyze necesita la ruta de un WAV")
        print(format_envelope(analyze(args.wav, args.hop)))
        return 0
    paths = resolve_paths(args)
    base = load_timeline(paths.timeline)
    tl = with_overrides(with_events(base, finale_events(paths.frames, paths.root / "Saved/Logs", base.fps)), args)
    print(f"línea de tiempo: {tl.source} ({tl.frames} fotogramas a {tl.fps} fps = {tl.duration:.2f} s); "
          f"plano 7 a {tl.shot(7).start:.2f} s, bocina a {tl.horn_time():.2f} s")
    if args.command == "mix":
        return 0 if run_mix(tl, paths)["ok"] else 1
    if args.command == "cards":
        for lang in languages(args.langs):
            for rc in run_cards(tl, paths, args, lang):
                print(f"{lang} {rc.card.card_id} {rc.card.start:.2f}-{rc.card.end:.2f} s en ({rc.x}, {rc.y})")
        return 0
    if args.command == "credits":
        run_credits(tl, paths)
        return 0
    if args.command == "rehearsal":
        background = render_rehearsal_background(tl, paths.captures, paths.out / "tmp/rehearsal_bg.mp4", args.threads)
        return run_video(tl, paths, args, ["-i", str(background)], "-ensayo")
    found = count_frames(paths.frames)
    if tl.source == "nominal":
        raise SystemExit(f"falta {paths.timeline}: el montaje final necesita los tiempos reales del motor")
    if found != tl.frames:
        raise SystemExit(f"hay {found} fotogramas en {paths.frames}; trailer.json declara {tl.frames}")
    return run_video(tl, paths, args, frames_input(paths.frames, tl.fps), "")
