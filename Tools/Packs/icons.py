"""Normaliza los iconos de UI de los packs (``icons`` de packs_catalogo.json) y compone su hoja.

Uso (desde la raíz del repositorio, con los packs ya en la caché de fetch_packs.py):
    uv run --with pillow python Tools/Packs/icons.py lote9-iconos

Cada icono del pack es una silueta blanca con alfa. Se recorta a su caja, se centra en un
cuadrado con el mismo margen para todos (``MARGIN``) y se reescala a ``SIZE`` px; el color se
tira a blanco puro para que Slate lo tiña con el color de ``ExploredUIStyle`` que diga
``tint`` (la UI no cambia por isla). Salida: ``Art/Export/Packs/<lote>/<texture>.png``
(ignorada en git) y la hoja ``docs/art/packs/<lote>.png`` (< 1 MB) con el original a la
izquierda y el normalizado sobre ``ColorPaper`` en cada estado de ``tint``.
"""

from __future__ import annotations

import json
import os
import re
import sys
import textwrap
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

REPO = Path(__file__).resolve().parent.parent.parent
CATALOG = REPO / "Content" / "Data" / "packs_catalogo.json"
STYLE = REPO / "Source" / "Explored" / "UI" / "ExploredUIStyle.h"
EXPORT = REPO / "Art" / "Export" / "Packs"
SIZE = 128
MARGIN = 0.10
MAX_BYTES = 1_000_000
COLS = 2
BG = (38, 41, 46)
FG = (235, 232, 224)
DIM = (160, 164, 170)
STYLE_COLOR = re.compile(
    r"FLinearColor (Color[A-Za-z]+)\(\) const \{ return FLinearColor\(([\d.]+)f, ([\d.]+)f, ([\d.]+)f, ([\d.]+)f\)"
)


def cache_dir() -> Path:
    env = os.environ.get("EXPLORED_PACKS_CACHE")
    return Path(env) if env else REPO / "Art" / "Packs"


def style_colors(path: Path = STYLE) -> dict[str, tuple[int, int, int, int]]:
    """Colores de ExploredUIStyle.h en sRGB de 8 bits (FLinearColor es lineal)."""

    def srgb(c: float) -> int:
        c = min(max(c, 0.0), 1.0)
        s = 12.92 * c if c <= 0.0031308 else 1.055 * c ** (1 / 2.4) - 0.055
        return round(s * 255)

    out = {}
    for name, r, g, b, a in STYLE_COLOR.findall(path.read_text(encoding="utf-8")):
        out[name] = (srgb(float(r)), srgb(float(g)), srgb(float(b)), round(float(a) * 255))
    return out


def normalize(src: Image.Image, size: int = SIZE, margin: float = MARGIN) -> Image.Image:
    """Silueta blanca centrada en un cuadrado ``size`` con ``margin`` a cada lado."""
    alpha = src.convert("RGBA").getchannel("A")
    box = alpha.getbbox()
    if box is None:
        raise ValueError("icono vacío (alfa a cero)")
    alpha = alpha.crop(box)
    inner = round(size * (1 - 2 * margin))
    w, h = alpha.size
    scale = inner / max(w, h)
    alpha = alpha.resize((max(1, round(w * scale)), max(1, round(h * scale))), Image.Resampling.LANCZOS)
    out = Image.new("RGBA", (size, size), (255, 255, 255, 0))
    mask = Image.new("L", (size, size), 0)
    mask.paste(alpha, ((size - alpha.width) // 2, (size - alpha.height) // 2))
    out.putalpha(mask)
    return out


def tinted(icon: Image.Image, rgba: tuple[int, int, int, int], bg: tuple[int, int, int]) -> Image.Image:
    """Lo que pinta Slate: el blanco del icono multiplicado por el tinte, sobre el fondo."""
    layer = Image.new("RGBA", icon.size, rgba[:3] + (255,))
    alpha_scale = rgba[3]
    a = icon.getchannel("A").point([v * alpha_scale // 255 for v in range(256)])
    layer.putalpha(a)
    base = Image.new("RGBA", icon.size, bg + (255,))
    return Image.alpha_composite(base, layer).convert("RGB")


def font(size: int) -> ImageFont.ImageFont | ImageFont.FreeTypeFont:
    for name in ("DejaVuSans.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    return ImageFont.load_default()


def build(lote: str) -> Path:
    catalog = json.loads(CATALOG.read_text(encoding="utf-8"))
    info = next(entry for entry in catalog["lotes"] if entry["id"] == lote)
    icons = [i for i in catalog.get("icons", []) if i["lote"] == lote]
    colors = style_colors()
    paper = colors["ColorPaper"][:3]
    out_dir = EXPORT / lote
    out_dir.mkdir(parents=True, exist_ok=True)

    cell_w, cell_h, pad = 700, SIZE + 16 + 58, 16
    header = 84
    rows = (len(icons) + COLS - 1) // COLS
    sheet = Image.new("RGB", (COLS * cell_w, header + rows * cell_h), BG)
    draw = ImageDraw.Draw(sheet)
    draw.text((16, 12), f"{lote} · {info['date']} · original del pack (izq.) / normalizado y teñido sobre ColorPaper (dcha.)",
              font=font(20), fill=FG)
    for k, line in enumerate(textwrap.wrap(info["scope"], 150)):
        draw.text((16, 38 + 18 * k), line, font=font(14), fill=DIM)
    for n, icon in enumerate(icons):
        src = Image.open(cache_dir() / icon["pack"] / icon["file"]).convert("RGBA")
        norm = normalize(src)
        norm.save(out_dir / f"{icon['texture']}.png", optimize=True)
        x = (n % COLS) * cell_w + pad
        y = header + (n // COLS) * cell_h + 8
        orig = Image.new("RGBA", (SIZE, SIZE), (90, 96, 104, 255))
        thumb = src.copy()
        thumb.thumbnail((SIZE, SIZE))
        orig.alpha_composite(thumb, ((SIZE - thumb.width) // 2, (SIZE - thumb.height) // 2))
        sheet.paste(orig.convert("RGB"), (x, y))
        for k, color in enumerate(icon["tint"].values()):
            sheet.paste(tinted(norm, colors[color], paper), (x + (k + 1) * (SIZE + 16), y))
        slots = ", ".join(s.get("achievementIcon") or f"{s['widget']}.{s['role']}" for s in icon["slots"])
        states = " / ".join(f"{k}: {v}" for k, v in icon["tint"].items())
        draw.text((x, y + SIZE + 6), f"{icon['texture']} ← {slots}", font=font(14), fill=FG)
        draw.text((x, y + SIZE + 26), f"{icon['pack']}: {Path(icon['file']).name} · {states}", font=font(11), fill=DIM)
    out = REPO / "docs" / "art" / "packs" / f"{lote}.png"
    out.parent.mkdir(parents=True, exist_ok=True)
    sheet.quantize(colors=128, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).save(out, optimize=True)
    size = out.stat().st_size
    if size > MAX_BYTES:
        raise SystemExit(f"{out} pesa {size} bytes (> 1 MB)")
    return out


def main(argv: list[str] | None = None) -> None:
    args = sys.argv[1:] if argv is None else argv
    if len(args) != 1:
        print(__doc__)
        raise SystemExit(1)
    out = build(args[0])
    print(f"Escrito {out.relative_to(REPO)} ({out.stat().st_size // 1024} KB)")


if __name__ == "__main__":
    main()
