"""Lámina comparativa «antes / después» de capturas del juego (pasada visual de F4).

Pone en filas las parejas `<antes>-<vista>.png` y `<después>-<vista>.png` de
`docs/v3/capturas`, reducidas a la mitad, con el nombre de cada columna y vista.
Las vistas que no tienen captura «antes» (atardecer, ciclón) salen solas en la
columna de la derecha.

Uso: `uv run style-comparativa [--antes f4] [--despues f4b] [--out RUTA]`.
"""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
from PIL import Image

from explored_style import raster
from explored_style.palette import REPO_ROOT, load_palette

CAPTURAS = REPO_ROOT / 'docs' / 'v3' / 'capturas'
OUT = REPO_ROOT / 'docs' / 'v3' / 'laminas' / 'f4b-antes-despues.png'
VISTAS = ('cenital', 'cala', 'palmar', 'faro', 'islote', 'mano', 'atardecer', 'ciclon')
PAD, LABEL_H, SCALE = 16, 36, 3


def half(image: np.ndarray) -> np.ndarray:
    """Media de bloques de 2 x 2 (recorta una fila o columna impar)."""
    h, w = image.shape[0] // 2 * 2, image.shape[1] // 2 * 2
    blocks = image[:h, :w].astype(np.float32).reshape(h // 2, 2, w // 2, 2, 3)
    return blocks.mean(axis=(1, 3)).round().astype(np.uint8)


def load(path: Path) -> np.ndarray | None:
    """Captura del motor (PNG RGBA con filtros, por eso Pillow) reducida a la mitad, o None si no existe."""
    if not path.is_file():
        return None
    with Image.open(path) as image:
        return half(np.asarray(image.convert('RGB')))


def build(antes: str, despues: str, carpeta: Path = CAPTURAS, vistas: tuple[str, ...] = VISTAS) -> np.ndarray:
    pal = load_palette()
    paper, ink = pal.by_name('papel').rgb, pal.by_name('tinta').rgb
    filas = [(v, load(carpeta / f'{antes}-{v}.png'), load(carpeta / f'{despues}-{v}.png')) for v in vistas]
    filas = [f for f in filas if f[2] is not None]
    if not filas:
        raise FileNotFoundError(f'no hay capturas {despues}-*.png en {carpeta}')
    cell_h, cell_w = filas[0][2].shape[:2]
    width = 2 * cell_w + 3 * PAD
    height = LABEL_H + PAD + len(filas) * (cell_h + LABEL_H + PAD)
    canvas = raster.new_canvas(width, height, paper)
    canvas = raster.draw_text(canvas, f'ANTES ({antes.upper()})', PAD, PAD, SCALE, ink)
    canvas = raster.draw_text(canvas, f'DESPUES ({despues.upper()})', 2 * PAD + cell_w, PAD, SCALE, ink)
    y = LABEL_H + PAD
    for vista, img_antes, img_despues in filas:
        canvas = raster.draw_text(canvas, vista.upper(), PAD, y + 8, SCALE, ink)
        y += LABEL_H
        if img_antes is not None:
            canvas = raster.paste(canvas, img_antes, PAD, y)
        canvas = raster.paste(canvas, img_despues, 2 * PAD + cell_w, y)
        y += cell_h + PAD
    return canvas


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog='style-comparativa', description='Lámina antes/después de capturas.')
    parser.add_argument('--antes', default='f4')
    parser.add_argument('--despues', default='f4b')
    parser.add_argument('--capturas', type=Path, default=CAPTURAS)
    parser.add_argument('--out', type=Path, default=OUT)
    args = parser.parse_args(argv)
    try:
        canvas = build(args.antes, args.despues, args.capturas)
    except FileNotFoundError as exc:
        print(f'[comparativa] ERROR: {exc}')
        return 2
    raster.write_png(args.out, canvas)
    print(f'[comparativa] {args.out} ({canvas.shape[1]} x {canvas.shape[0]})')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
