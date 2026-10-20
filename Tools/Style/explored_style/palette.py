"""Paleta única de Baliza Siete (GDD v3 §11).

`paleta.json` es la fuente de verdad. Este módulo la carga y valida, define
el método de color de las mallas (una celda de la hoja por cara, ver
README) y genera:

- `Art/Export/Style/T_Paleta.png`: hoja de 32 x 1 px que muestrea `M_Toon`.
- `docs/v3/laminas/paleta-ampliada.png`: versión rotulada para documentación.

Uso: `uv run style-paleta [--sheet RUTA] [--doc RUTA]`.
"""

from __future__ import annotations

import argparse
import json
from dataclasses import dataclass
from pathlib import Path

import numpy as np

from explored_style import raster

STYLE_DIR = Path(__file__).resolve().parents[1]
REPO_ROOT = STYLE_DIR.parents[1]
PALETTE_PATH = STYLE_DIR / 'paleta.json'
SHEET_PATH = REPO_ROOT / 'Art' / 'Export' / 'Style' / 'T_Paleta.png'
DOC_PATH = REPO_ROOT / 'docs' / 'v3' / 'laminas' / 'paleta-ampliada.png'

PALETTE_SIZE = 32
# Los 12 colores fijados en el GDD §11, en este orden, en los índices 0-11.
GDD_COLORS: tuple[tuple[str, str], ...] = (
    ('arena_clara', '#E8D8A8'), ('arena_oscura', '#C9B27C'), ('hierba_clara', '#8DBB5A'),
    ('hierba_oscura', '#4F8A3C'), ('verde_palma', '#3E7D3A'), ('madera_clara', '#B98A5A'),
    ('madera_oscura', '#7A5233'), ('roca_media', '#7C7C78'), ('oxido', '#B5532F'),
    ('rojo_faro', '#C8402F'), ('laton', '#D9A441'), ('cristal', '#9FD6E0'),
)
# Margen dentro de la celda (en fracción de celda) que deben respetar las UV:
# con filtro Nearest basta con no tocar el borde; el cuarto de texel cubre
# además el filtrado bilineal si alguien olvida poner Nearest.
CELL_MARGIN = 0.25


@dataclass(frozen=True)
class PaletteColor:
    index: int
    name: str
    hex: str
    family: str
    use: str

    @property
    def rgb(self) -> raster.Rgb:
        return raster.hex_to_rgb(self.hex)


@dataclass(frozen=True)
class Palette:
    colors: tuple[PaletteColor, ...]

    def __len__(self) -> int:
        return len(self.colors)

    def by_name(self, name: str) -> PaletteColor:
        for color in self.colors:
            if color.name == name:
                return color
        raise KeyError(f'color «{name}» no está en la paleta')

    def index_of(self, name: str) -> int:
        return self.by_name(name).index


def parse_palette(data: dict) -> Palette:
    """Valida el contenido de paleta.json y devuelve la paleta ordenada por
    índice. Lanza ValueError con el motivo si no cumple el GDD."""
    entries = data.get('colores')
    if not isinstance(entries, list):
        raise ValueError('paleta.json: falta la lista «colores»')
    colors = tuple(sorted(
        (PaletteColor(int(e['indice']), str(e['nombre']), str(e['hex']).upper(), str(e.get('familia', '')),
                      str(e.get('uso', '')))
         for e in entries),
        key=lambda c: c.index))
    if len(colors) != PALETTE_SIZE:
        raise ValueError(f'paleta.json: {len(colors)} colores; el GDD fija {PALETTE_SIZE}')
    if [c.index for c in colors] != list(range(PALETTE_SIZE)):
        raise ValueError('paleta.json: los índices deben ser 0..31 sin huecos ni repetidos')
    for color in colors:
        raster.hex_to_rgb(color.hex)
    for kind, values in (('nombres', [c.name for c in colors]), ('colores', [c.hex for c in colors])):
        if len(set(values)) != len(values):
            raise ValueError(f'paleta.json: hay {kind} repetidos')
    for index, (name, hex_value) in enumerate(GDD_COLORS):
        if (colors[index].name, colors[index].hex) != (name, hex_value):
            raise ValueError(f'paleta.json: el índice {index} debe ser {name} {hex_value} (GDD §11)')
    return Palette(colors)


def load_palette(path: Path = PALETTE_PATH) -> Palette:
    return parse_palette(json.loads(path.read_text(encoding='utf-8')))


def cell_uv(index: int, size: int = PALETTE_SIZE) -> tuple[float, float]:
    """UV del centro de la celda «index» de la hoja (u, v)."""
    if not 0 <= index < size:
        raise ValueError(f'índice de paleta fuera de rango: {index}')
    return ((index + 0.5) / size, 0.5)


def cell_of_u(u: float, size: int = PALETTE_SIZE, margin: float = CELL_MARGIN) -> int | None:
    """Celda que contiene la coordenada «u» con el margen exigido, o None si
    cae fuera de la hoja o demasiado cerca del borde de una celda."""
    scaled = u * size
    cell = int(np.floor(scaled))
    if not 0 <= cell < size:
        return None
    frac = scaled - cell
    if frac < margin or frac > 1.0 - margin:
        return None
    return cell


def sheet_canvas(palette: Palette) -> np.ndarray:
    """Hoja 32 x 1: el píxel i es el color de índice i."""
    canvas = raster.new_canvas(len(palette), 1, (0, 0, 0))
    for color in palette.colors:
        canvas = raster.fill_rect(canvas, color.index, 0, 1, 1, color.rgb)
    return canvas


def strip_canvas(palette: Palette, width: int, height: int, label_scale: int = 2) -> np.ndarray:
    """Tira horizontal con las 32 muestras y su índice (para pies de lámina)."""
    canvas = raster.new_canvas(width, height, palette.by_name('tinta').rgb)
    ink, paper = palette.by_name('tinta').rgb, palette.by_name('papel').rgb
    for color in palette.colors:
        x0 = color.index * width // len(palette)
        x1 = (color.index + 1) * width // len(palette)
        canvas = raster.fill_rect(canvas, x0, 0, x1 - x0, height, color.rgb)
        label = f'{color.index:02d}'
        tx = x0 + (x1 - x0 - raster.text_width(label, label_scale)) // 2
        text_color = ink if raster.luminance(color.rgb) > 0.5 else paper
        canvas = raster.draw_text(canvas, label, tx, height - 7 * label_scale, label_scale, text_color)
    return canvas


def documentation_canvas(palette: Palette, columns: int = 2, swatch: int = 72, scale: int = 3) -> np.ndarray:
    """Hoja ampliada: muestra grande, índice, nombre y hex de cada color."""
    rows = -(-len(palette) // columns)
    col_w, row_h, pad = 560, swatch + 12, 24
    paper, ink = palette.by_name('papel').rgb, palette.by_name('tinta').rgb
    title_h = 9 * scale + 2 * pad
    canvas = raster.new_canvas(columns * col_w + pad, title_h + rows * row_h + pad, paper)
    canvas = raster.draw_text(canvas, 'BALIZA SIETE - PALETA UNICA (32) - T_PALETA 32X1', pad, pad, scale, ink)
    for color in palette.colors:
        col, row = divmod(color.index, rows)
        x, y = pad + col * col_w, title_h + row * row_h
        canvas = raster.fill_rect(canvas, x - 2, y - 2, swatch + 4, swatch + 4, ink)
        canvas = raster.fill_rect(canvas, x, y, swatch, swatch, color.rgb)
        text_y = y + (swatch - 5 * scale) // 2
        label = f'{color.index:02d} {color.name} {color.hex}'
        canvas = raster.draw_text(canvas, label, x + swatch + 16, text_y, scale, ink)
    return canvas


def write_sheets(palette: Palette, sheet_path: Path = SHEET_PATH, doc_path: Path = DOC_PATH) -> tuple[Path, Path]:
    return (raster.write_png(sheet_path, sheet_canvas(palette)),
            raster.write_png(doc_path, documentation_canvas(palette)))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog='style-paleta', description='Genera las hojas PNG de la paleta.')
    parser.add_argument('--palette', type=Path, default=PALETTE_PATH)
    parser.add_argument('--sheet', type=Path, default=SHEET_PATH, help='hoja 32 x 1 para el motor')
    parser.add_argument('--doc', type=Path, default=DOC_PATH, help='versión ampliada para documentación')
    args = parser.parse_args(argv)
    try:
        palette = load_palette(args.palette)
    except (OSError, ValueError, KeyError) as exc:
        print(f'[paleta] ERROR: {exc}')
        return 2
    for path in write_sheets(palette, args.sheet, args.doc):
        print(f'[paleta] escrito {path}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
