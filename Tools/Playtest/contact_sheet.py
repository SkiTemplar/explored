"""Monta una hoja de contacto (rejilla de miniaturas con su nombre de fichero) a partir de
las capturas del set «playtest» de UExploredShotSubsystem.

Uso (desde la raíz del repositorio, vía Tools/playtest.ps1 o a mano):
    uv run --with pillow python Tools/Playtest/contact_sheet.py --shots-dir Saved/Shots --out Saved/Shots/playtest_contact.png

No usa pip ni "python" directo (norma del proyecto): siempre `uv run --with pillow python ...`.
"""

from __future__ import annotations

import argparse
import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

THUMB_WIDTH = 320
CAPTION_HEIGHT = 22
PADDING = 6
COLUMNS = 6
BACKGROUND = (24, 24, 28)
CAPTION_COLOR = (230, 230, 230)


def load_thumbnails(shots_dir: Path, exclude: set[str]) -> list[tuple[str, Image.Image]]:
    """Carga y reduce a miniatura cada PNG de shots_dir (orden alfabético), saltando `exclude`."""
    thumbnails: list[tuple[str, Image.Image]] = []
    for path in sorted(shots_dir.glob("*.png")):
        if path.name in exclude:
            continue
        with Image.open(path) as image:
            image = image.convert("RGB")
            ratio = THUMB_WIDTH / image.width
            thumb_height = max(1, round(image.height * ratio))
            thumbnails.append((path.stem, image.resize((THUMB_WIDTH, thumb_height))))
    return thumbnails


def build_contact_sheet(thumbnails: list[tuple[str, Image.Image]], columns: int) -> Image.Image:
    """Coloca las miniaturas en una rejilla con el nombre de cada captura debajo."""
    if not thumbnails:
        return Image.new("RGB", (THUMB_WIDTH, THUMB_WIDTH), BACKGROUND)

    cell_height = max(thumb.height for _, thumb in thumbnails) + CAPTION_HEIGHT
    rows = math.ceil(len(thumbnails) / columns)
    sheet_width = columns * (THUMB_WIDTH + PADDING) + PADDING
    sheet_height = rows * (cell_height + PADDING) + PADDING

    sheet = Image.new("RGB", (sheet_width, sheet_height), BACKGROUND)
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.load_default()

    for index, (name, thumb) in enumerate(thumbnails):
        col = index % columns
        row = index // columns
        x = PADDING + col * (THUMB_WIDTH + PADDING)
        y = PADDING + row * (cell_height + PADDING)
        sheet.paste(thumb, (x, y))
        draw.text((x, y + thumb.height + 4), name, fill=CAPTION_COLOR, font=font)

    return sheet


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--shots-dir", type=Path, required=True, help="Carpeta con las capturas .png")
    parser.add_argument("--out", type=Path, required=True, help="Ruta del PNG de salida")
    parser.add_argument("--columns", type=int, default=COLUMNS)
    args = parser.parse_args()

    thumbnails = load_thumbnails(args.shots_dir, exclude={args.out.name})
    sheet = build_contact_sheet(thumbnails, args.columns)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(args.out)
    print(f"Hoja de contacto: {args.out} ({len(thumbnails)} capturas)")


if __name__ == "__main__":
    main()
