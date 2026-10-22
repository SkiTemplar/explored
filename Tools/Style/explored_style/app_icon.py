"""Icono del ejecutable de Baliza Siete: el faro encendido de noche sobre el mar.

Se dibuja con colores de `paleta.json` (cielo de noche, mar, faro blanco con franjas rojas, linterna
y haz de latón) a 4x y se reduce con Lanczos. Escribe `Build/Windows/Application.ico` (16 a 256 px),
que UnrealBuildTool incrusta en el .exe, y una lámina PNG para revisarlo.

    uv run style-icon            # escribe el .ico y la lámina
    uv run style-icon --check    # falla si el .ico versionado no está al día
"""

from __future__ import annotations

import argparse
import io
from pathlib import Path

from PIL import Image, ImageDraw

from explored_style.palette import REPO_ROOT, Palette, load_palette

ICON_PATH = REPO_ROOT / 'Build' / 'Windows' / 'Application.ico'
LAMINA_PATH = REPO_ROOT / 'docs' / 'v3' / 'laminas' / 'icono.png'
SIZE = 256
SUPERSAMPLE = 4
ICO_SIZES = ((16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256))

Rgba = tuple[int, int, int, int]


def _rgba(palette: Palette, name: str, alpha: int = 255) -> Rgba:
    r, g, b = palette.by_name(name).rgb
    return (r, g, b, alpha)


def _s(*values: float) -> tuple[float, ...]:
    """Coordenadas 0-100 a píxeles del lienzo grande."""
    return tuple(v * SIZE * SUPERSAMPLE / 100.0 for v in values)


def _pts(points: list[tuple[float, float]]) -> list[tuple[float, float]]:
    scale = SIZE * SUPERSAMPLE / 100.0
    return [(x * scale, y * scale) for x, y in points]


def draw_icon(palette: Palette) -> Image.Image:
    """Icono RGBA de SIZE x SIZE con las esquinas transparentes."""
    big = SIZE * SUPERSAMPLE
    image = Image.new('RGBA', (big, big), (0, 0, 0, 0))
    d = ImageDraw.Draw(image, 'RGBA')
    d.rounded_rectangle(_s(2, 2, 98, 98), radius=18 * big / 100.0, fill=_rgba(palette, 'noche_cenit'))
    # Haz hacia la izquierda y hacia la derecha desde la linterna.
    beam = _rgba(palette, 'laton', 110)
    d.polygon(_pts([(50, 27), (6, 12), (6, 38)]), fill=beam)
    d.polygon(_pts([(50, 27), (94, 14), (94, 36)]), fill=beam)
    # Mar con una ola clara.
    d.rounded_rectangle(_s(2, 74, 98, 98), radius=18 * big / 100.0, fill=_rgba(palette, 'agua_profunda'))
    d.rectangle(_s(2, 74, 98, 84), fill=_rgba(palette, 'agua_profunda'))
    d.line(_pts([(14, 82), (26, 79), (38, 82)]), fill=_rgba(palette, 'agua_somera'), width=round(2.5 * big / 100))
    d.line(_pts([(62, 86), (74, 83), (86, 86)]), fill=_rgba(palette, 'agua_somera'), width=round(2.5 * big / 100))
    # Roca y torre (troncocónica) con dos franjas rojas.
    d.polygon(_pts([(28, 80), (36, 70), (64, 70), (72, 80)]), fill=_rgba(palette, 'roca_oscura'))
    d.polygon(_pts([(41, 34), (59, 34), (64, 72), (36, 72)]), fill=_rgba(palette, 'blanco_nube'))
    d.polygon(_pts([(39.7, 44), (60.3, 44), (61.4, 52), (38.6, 52)]), fill=_rgba(palette, 'rojo_faro'))
    d.polygon(_pts([(37.9, 58), (62.1, 58), (63.2, 66), (36.8, 66)]), fill=_rgba(palette, 'rojo_faro'))
    # Galería, linterna encendida y cúpula.
    d.rectangle(_s(37, 31, 63, 35), fill=_rgba(palette, 'tinta'))
    d.rectangle(_s(42, 21, 58, 31), fill=_rgba(palette, 'laton'))
    d.rectangle(_s(45, 23, 55, 29), fill=_rgba(palette, 'fuego'))
    d.polygon(_pts([(40, 21), (50, 12), (60, 21)]), fill=_rgba(palette, 'rojo_faro'))
    return image.resize((SIZE, SIZE), Image.Resampling.LANCZOS)


def ico_bytes(icon: Image.Image) -> bytes:
    buffer = io.BytesIO()
    icon.save(buffer, format='ICO', sizes=list(ICO_SIZES))
    return buffer.getvalue()


def lamina(icon: Image.Image) -> Image.Image:
    """Lámina de revisión: el icono a 256, 64, 32 y 16 px sobre papel."""
    sheet = Image.new('RGBA', (SIZE + 3 * 72 + 64, SIZE + 32), (233, 222, 196, 255))
    sheet.alpha_composite(icon, (16, 16))
    x = SIZE + 32
    for size in (64, 32, 16):
        sheet.alpha_composite(icon.resize((size, size), Image.Resampling.LANCZOS), (x, 16))
        x += 72
    return sheet


def main(argv: list[str] | None = None, icon_path: Path = ICON_PATH, lamina_path: Path = LAMINA_PATH) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0] if __doc__ else None)
    parser.add_argument('--check', action='store_true', help='solo comprueba que el .ico está al día')
    args = parser.parse_args(argv)
    icon = draw_icon(load_palette())
    data = ico_bytes(icon)
    if args.check:
        if not icon_path.exists() or icon_path.read_bytes() != data:
            print(f'Desfasado: {icon_path} (ejecuta `uv run style-icon`)')
            return 1
        return 0
    icon_path.parent.mkdir(parents=True, exist_ok=True)
    icon_path.write_bytes(data)
    lamina_path.parent.mkdir(parents=True, exist_ok=True)
    lamina(icon).save(lamina_path)
    print(icon_path)
    print(lamina_path)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
