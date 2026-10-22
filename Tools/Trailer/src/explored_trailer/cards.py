"""Rótulos del tráiler: PNG con transparencia, tipografía Lora (Content/Slate/Fonts) y paleta
del juego (Tools/Style/paleta.json).

Cada rótulo se recorta a su caja y se guarda con su posición, para que ffmpeg solo componga
el trozo que ocupa. La legibilidad la dan una sombra difusa en tinta y una banda suave.
"""

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import Literal

from PIL import Image, ImageDraw, ImageFilter, ImageFont

from .timeline import Timeline

WIDTH, HEIGHT = 1920, 1080
FADE_SECONDS = 0.5
WORD_MARGIN = 0.3
Language = Literal["es", "en"]
LANGUAGES: tuple[Language, ...] = ("es", "en")


@dataclass(frozen=True)
class Bilingual:
    es: str
    en: str

    def get(self, lang: Language) -> str:
        return self.es if lang == "es" else self.en


INTRO = Bilingual("Iria Valdés apaga faros para ganarse la vida.", "Iria Valdés switches off lighthouses for a living.")
STRANDED = Bilingual("Esta vez es ella quien naufraga.", "This time, she is the one who is stranded.")
WORDS = (
    Bilingual("Fuego.", "Fire."),
    Bilingual("Pesca.", "Fish."),
    Bilingual("Cuerda.", "Rope."),
    Bilingual("Aceite.", "Oil."),
)
CYCLONE = Bilingual("El ciclón llega la segunda noche.", "The cyclone arrives on the second night.")
FREIGHTER = Bilingual("Una lente. Una llama. Una oportunidad.", "One lens. One flame. One chance.")
TITLE = Bilingual("BALIZA SIETE", "BALIZA SIETE")
TAGLINE = Bilingual("Enciende la luz.", "Light the beacon.")
GENRE = Bilingual("Supervivencia en primera persona · PC (Windows)", "First-person survival · PC (Windows)")
ENGINE = Bilingual("Hecho con Unreal Engine", "Made with Unreal Engine")


@dataclass(frozen=True)
class Palette:
    paper: str
    ink: str
    brass: str
    fire: str

    @staticmethod
    def load(path: Path) -> Palette:
        data = json.loads(path.read_text(encoding="utf-8"))
        colors = {c["nombre"]: c["hex"] for c in data["colores"]}
        return Palette(colors["papel"], colors["tinta"], colors["laton"], colors["fuego"])


@dataclass(frozen=True)
class Fonts:
    regular: Path
    bold: Path
    italic: Path

    @staticmethod
    def from_dir(folder: Path) -> Fonts:
        fonts = Fonts(folder / "Lora-Regular.ttf", folder / "Lora-Bold.ttf", folder / "Lora-Italic.ttf")
        for path in (fonts.regular, fonts.bold, fonts.italic):
            if not path.is_file():
                raise FileNotFoundError(f"falta la fuente del juego: {path}")
        return fonts


@dataclass(frozen=True)
class Card:
    card_id: str
    kind: Literal["line", "word", "title"]
    start: float
    end: float
    text: Bilingual


@dataclass(frozen=True)
class Layout:
    """Posiciones en fracciones del fotograma; se ajustan a la composición real."""

    line_y: float = 0.80
    word_x: float = 0.085
    word_y: float = 0.40
    title_y: float = 0.40
    title_x: float = 0.50


@dataclass(frozen=True)
class RenderedCard:
    card: Card
    path: Path
    x: int
    y: int


def card_plan(tl: Timeline) -> tuple[Card, ...]:
    s1, s2, s3, s5, s6, s8 = (tl.shot(i) for i in (1, 2, 3, 5, 6, 8))
    parts = tl.parts_of(3)
    if len(parts) == len(WORDS):
        # Una palabra por subplano del montaje, dentro del corte para que no salte con la imagen.
        spans = [(p.start + WORD_MARGIN, p.end - WORD_MARGIN) for p in parts]
    else:
        slot = (s3.duration - 2.0) / len(WORDS)
        spans = [(s3.start + 1.0 + i * slot, s3.start + 1.0 + (i + 1) * slot) for i in range(len(WORDS))]
    words = tuple(Card(f"03_palabra_{i + 1}", "word", a, b, w) for i, (w, (a, b)) in enumerate(zip(WORDS, spans,
                                                                                                    strict=True)))
    return (
        Card("01_intro", "line", 0.8, min(3.6, s1.end - 0.4), INTRO),
        Card("02_naufraga", "line", s2.start + 2.5, s2.end - 1.0, STRANDED),
        *words,
        Card("05_ciclon", "line", s5.start + 1.0, s5.end - 2.0, CYCLONE),
        Card("06_carguero", "line", s6.start + 1.0, s6.end - 1.0, FREIGHTER),
        Card("08_titulo", "title", s8.start + 1.0, tl.duration - 1.0, TITLE),
    )


def _hex(color: str, alpha: int = 255) -> tuple[int, int, int, int]:
    color = color.lstrip("#")
    return int(color[0:2], 16), int(color[2:4], 16), int(color[4:6], 16), alpha


def tracked_width(text: str, font: ImageFont.FreeTypeFont, tracking: float) -> float:
    if not text:
        return 0.0
    return sum(font.getlength(ch) for ch in text) + tracking * (len(text) - 1)


def draw_text(draw: ImageDraw.ImageDraw, x: float, baseline: float, text: str, font: ImageFont.FreeTypeFont,
              fill: int | tuple[int, int, int, int], tracking: float = 0.0) -> None:
    """Texto con espaciado entre letras, anclado a la línea base por la izquierda."""
    if tracking == 0.0:
        draw.text((x, baseline), text, font=font, fill=fill, anchor="ls")
        return
    for ch in text:
        draw.text((x, baseline), ch, font=font, fill=fill, anchor="ls")
        x += font.getlength(ch) + tracking


@dataclass(frozen=True)
class TextRun:
    text: str
    font: ImageFont.FreeTypeFont
    x: float
    baseline: float
    color: tuple[int, int, int, int]
    tracking: float = 0.0


def compose(runs: list[TextRun], palette: Palette, band: tuple[int, int, int, int] | None,
            shadow_radius: float = 5.0, band_alpha: int = 96) -> Image.Image:
    """Capa RGBA de fotograma completo: banda suave + sombra difusa + texto."""
    ink = _hex(palette.ink)
    canvas = Image.new("RGBA", (WIDTH, HEIGHT), ink[:3] + (0,))
    if band is not None:
        mask = Image.new("L", (WIDTH, HEIGHT), 0)
        ImageDraw.Draw(mask).rounded_rectangle(band, radius=(band[3] - band[1]) // 2, fill=band_alpha)
        canvas.putalpha(mask.filter(ImageFilter.GaussianBlur(28)))
    text_mask = Image.new("L", (WIDTH, HEIGHT), 0)
    mask_draw = ImageDraw.Draw(text_mask)
    for run in runs:
        draw_text(mask_draw, run.x, run.baseline, run.text, run.font, 255, run.tracking)
    shadow = text_mask.filter(ImageFilter.GaussianBlur(shadow_radius)).point(lambda v: int(v * 0.85))
    shadow_layer = Image.new("RGBA", (WIDTH, HEIGHT), ink[:3] + (0,))
    shadow_layer.putalpha(shadow)
    shadow_layer = shadow_layer.transform((WIDTH, HEIGHT), Image.Transform.AFFINE, (1, 0, 0, 0, 1, -3))
    canvas = Image.alpha_composite(canvas, shadow_layer)
    text_layer = Image.new("RGBA", (WIDTH, HEIGHT), (0, 0, 0, 0))
    text_draw = ImageDraw.Draw(text_layer)
    for run in runs:
        draw_text(text_draw, run.x, run.baseline, run.text, run.font, run.color, run.tracking)
    return Image.alpha_composite(canvas, text_layer)


def _band_around(runs: list[TextRun], pad_x: int, pad_y: int) -> tuple[int, int, int, int]:
    left = min(r.x for r in runs) - pad_x
    right = max(r.x + tracked_width(r.text, r.font, r.tracking) for r in runs) + pad_x
    top = min(r.baseline - r.font.size for r in runs) - pad_y
    bottom = max(r.baseline + r.font.size * 0.3 for r in runs) + pad_y
    return int(left), int(top), int(right), int(bottom)


def line_runs(text: str, fonts: Fonts, palette: Palette, layout: Layout) -> list[TextRun]:
    font = ImageFont.truetype(str(fonts.italic), 56)
    width = tracked_width(text, font, 0.0)
    return [TextRun(text, font, (WIDTH - width) / 2, HEIGHT * layout.line_y, _hex(palette.paper))]


def word_runs(text: str, fonts: Fonts, palette: Palette, layout: Layout) -> list[TextRun]:
    font = ImageFont.truetype(str(fonts.bold), 104)
    return [TextRun(text, font, WIDTH * layout.word_x, HEIGHT * layout.word_y, _hex(palette.paper), 2.0)]


def title_runs(lang: Language, fonts: Fonts, palette: Palette, layout: Layout) -> list[TextRun]:
    title_font = ImageFont.truetype(str(fonts.bold), 150)
    tag_font = ImageFont.truetype(str(fonts.italic), 60)
    small_font = ImageFont.truetype(str(fonts.regular), 30)
    corner_font = ImageFont.truetype(str(fonts.regular), 24)
    title, tag, genre, engine = TITLE.get(lang), TAGLINE.get(lang), GENRE.get(lang), ENGINE.get(lang)
    title_track = 18.0
    title_w = tracked_width(title, title_font, title_track)
    tag_w = tracked_width(tag, tag_font, 0.5)
    genre_w = tracked_width(genre, small_font, 1.0)
    engine_w = tracked_width(engine, corner_font, 0.5)
    base = HEIGHT * layout.title_y
    paper = _hex(palette.paper)
    return [
        TextRun(title, title_font, WIDTH * layout.title_x - title_w / 2, base, paper, title_track),
        TextRun(tag, tag_font, WIDTH * layout.title_x - tag_w / 2, base + 104, _hex(palette.brass), 0.5),
        TextRun(genre, small_font, (WIDTH - genre_w) / 2, HEIGHT - 92, _hex(palette.paper, 230), 1.0),
        TextRun(engine, corner_font, WIDTH - 56 - engine_w, HEIGHT - 44, _hex(palette.paper, 235), 0.5),
    ]


def render_card(card: Card, lang: Language, fonts: Fonts, palette: Palette, layout: Layout) -> tuple[Image.Image, int, int]:
    """Imagen recortada del rótulo y su esquina superior izquierda en el fotograma."""
    if card.kind == "title":
        runs = title_runs(lang, fonts, palette, layout)
        image = compose(runs, palette, _band_around(runs[:2], 160, 60), shadow_radius=7.0, band_alpha=88)
    elif card.kind == "word":
        runs = word_runs(card.text.get(lang), fonts, palette, layout)
        image = compose(runs, palette, _band_around(runs, 70, 34), shadow_radius=6.0)
    else:
        runs = line_runs(card.text.get(lang), fonts, palette, layout)
        image = compose(runs, palette, _band_around(runs, 90, 30))
    bbox = image.getbbox() or (0, 0, 2, 2)
    left, top = bbox[0] & ~1, bbox[1] & ~1
    return image.crop((left, top, bbox[2], bbox[3])), left, top


def render_cards(tl: Timeline, lang: Language, out_dir: Path, fonts: Fonts, palette: Palette,
                 layout: Layout | None = None) -> tuple[RenderedCard, ...]:
    out_dir.mkdir(parents=True, exist_ok=True)
    rendered = []
    for card in card_plan(tl):
        image, x, y = render_card(card, lang, fonts, palette, layout or Layout())
        path = out_dir / f"{card.card_id}.png"
        image.save(path)
        rendered.append(RenderedCard(card, path, x, y))
    return tuple(rendered)
