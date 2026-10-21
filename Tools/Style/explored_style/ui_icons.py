"""Iconos planos de la interfaz de Baliza Siete (GDD v3 §9 y §11).

Cada icono es una silueta de un solo color: se dibuja en blanco sobre alfa y el juego la tiñe con
un color de `paleta.json` (tinta en el papel, el color de la necesidad en las barras). Se dibuja a
4x con Pillow y se reduce con Lanczos para que los bordes salgan suavizados.

Todo en una sola hoja (`Content/Slate/T_IconosUI.png`, la única textura de interfaz permitida):
8 columnas x 4 filas de celdas de 128 px. El orden de `ICONS` es el del enum `EExploredIcon` que
genera `ui_kit.py` en `Source/Explored/UI/ExploredUIPalette.h`.

Coordenadas de dibujo: 0-100 en cada eje de la celda (y hacia abajo).
"""

from __future__ import annotations

import math
from collections.abc import Callable

from PIL import Image, ImageDraw

CELL = 128
COLUMNS = 8
ROWS = 4
SUPERSAMPLE = 4
_S = CELL * SUPERSAMPLE / 100.0

ON = 255
OFF = 0

Pt = tuple[float, float]


def _p(points: list[Pt]) -> list[tuple[float, float]]:
    return [(x * _S, y * _S) for x, y in points]


def _box(x0: float, y0: float, x1: float, y1: float) -> tuple[float, float, float, float]:
    return (x0 * _S, y0 * _S, x1 * _S, y1 * _S)


def _line(d: ImageDraw.ImageDraw, points: list[Pt], width: float, fill: int = ON) -> None:
    d.line(_p(points), fill=fill, width=max(1, round(width * _S)), joint='curve')
    r = width / 2.0
    for x, y in (points[0], points[-1]):
        d.ellipse(_box(x - r, y - r, x + r, y + r), fill=fill)


def _rotated(points: list[Pt], deg: float, cx: float = 50.0, cy: float = 50.0) -> list[Pt]:
    a = math.radians(deg)
    c, s = math.cos(a), math.sin(a)
    return [(cx + (x - cx) * c - (y - cy) * s, cy + (x - cx) * s + (y - cy) * c) for x, y in points]


# --- Necesidades ---------------------------------------------------------------------------------

def sed(d: ImageDraw.ImageDraw) -> None:
    d.ellipse(_box(24, 40, 76, 92), fill=ON)
    d.polygon(_p([(50, 6), (26.5, 56), (73.5, 56)]), fill=ON)
    d.ellipse(_box(34, 58, 44, 74), fill=OFF)


def hambre(d: ImageDraw.ImageDraw) -> None:
    # Espina de pez: lo que queda cuando hay hambre.
    _line(d, [(14, 50), (78, 50)], 6)
    d.polygon(_p([(76, 50), (94, 32), (94, 68)]), fill=ON)
    for x in (26, 40, 54, 66):
        _line(d, [(x, 50), (x - 8, 30)], 4.5)
        _line(d, [(x, 50), (x - 8, 70)], 4.5)
    d.ellipse(_box(4, 40, 22, 60), fill=ON)
    d.ellipse(_box(9, 45, 14, 50), fill=OFF)


def energia(d: ImageDraw.ImageDraw) -> None:
    d.ellipse(_box(14, 10, 90, 86), fill=ON)
    d.ellipse(_box(34, 2, 104, 72), fill=OFF)
    d.polygon(_p([(74, 70), (77, 78), (85, 79), (79, 84), (81, 92), (74, 87), (67, 92), (69, 84), (63, 79), (71, 78)]),
              fill=ON)


def calor(d: ImageDraw.ImageDraw) -> None:
    d.polygon(_p([(50, 4), (66, 30), (78, 52), (78, 70), (22, 70), (22, 52), (32, 38), (40, 46), (42, 26)]), fill=ON)
    d.ellipse(_box(22, 44, 78, 94), fill=ON)
    d.polygon(_p([(52, 46), (62, 62), (60, 76), (40, 76), (40, 64)]), fill=OFF)
    d.ellipse(_box(38, 62, 62, 86), fill=OFF)


# --- Objetos (ids de items.json) ---------------------------------------------------------------

def cuchillo(d: ImageDraw.ImageDraw) -> None:
    d.polygon(_p([(6, 94), (52, 38), (64, 50), (26, 84)]), fill=ON)
    _line(d, [(66, 34), (88, 12)], 14)
    _line(d, [(52, 30), (72, 50)], 6)
    d.ellipse(_box(80, 14, 88, 22), fill=OFF)


def cantimplora(d: ImageDraw.ImageDraw) -> None:
    d.ellipse(_box(14, 30, 86, 98), fill=ON)
    d.rounded_rectangle(_box(40, 12, 60, 34), radius=3 * _S, fill=ON)
    d.rounded_rectangle(_box(36, 4, 64, 14), radius=3 * _S, fill=ON)
    d.ellipse(_box(28, 44, 72, 88), outline=OFF, width=round(4 * _S))
    d.arc(_box(4, 14, 96, 70), 200, 340, fill=ON, width=round(4 * _S))


def barometro(d: ImageDraw.ImageDraw) -> None:
    d.ellipse(_box(8, 12, 92, 96), fill=ON)
    d.ellipse(_box(16, 20, 84, 88), fill=OFF)
    d.rounded_rectangle(_box(42, 2, 58, 14), radius=3 * _S, fill=ON)
    for k in range(7):
        a = math.radians(210 - k * 40)
        x0, y0 = 50 + math.cos(a) * 26, 54 - math.sin(a) * 26
        x1, y1 = 50 + math.cos(a) * 32, 54 - math.sin(a) * 32
        _line(d, [(x0, y0), (x1, y1)], 3.5)
    _line(d, [(50, 54), (66, 36)], 5)
    d.ellipse(_box(44, 48, 56, 60), fill=ON)


def coco(d: ImageDraw.ImageDraw) -> None:
    d.ellipse(_box(12, 14, 88, 92), fill=ON)
    for x, y in ((38, 32), (50, 26), (62, 32)):
        d.ellipse(_box(x - 4.5, y - 4.5, x + 4.5, y + 4.5), fill=OFF)
    d.arc(_box(22, 24, 78, 84), 110, 170, fill=OFF, width=round(3 * _S))


def coco_vacio(d: ImageDraw.ImageDraw) -> None:
    d.chord(_box(8, 18, 92, 102), 0, 180, fill=ON)
    d.ellipse(_box(8, 46, 92, 74), fill=ON)
    d.ellipse(_box(16, 50, 84, 70), fill=OFF)


def lena(d: ImageDraw.ImageDraw) -> None:
    for y0, x0 in ((54, 6), (54, 46), (24, 26)):
        d.rounded_rectangle(_box(x0, y0, x0 + 48, y0 + 26), radius=12 * _S, fill=ON)
        d.ellipse(_box(x0 + 30, y0 + 4, x0 + 48, y0 + 22), fill=OFF)
        d.ellipse(_box(x0 + 35, y0 + 9, x0 + 43, y0 + 17), fill=ON)
        d.rectangle(_box(x0 - 1, y0 + 26, x0 + 49, y0 + 29), fill=OFF)


def fibra(d: ImageDraw.ImageDraw) -> None:
    for k, dx in enumerate((-18, -6, 6, 18)):
        top = 10 + abs(k - 1.5) * 8
        _line(d, [(50 + dx * 0.2, 92), (50 + dx * 0.6, 60), (50 + dx, top)], 5)
    d.rounded_rectangle(_box(36, 66, 64, 76), radius=3 * _S, fill=ON)


def cuerda(d: ImageDraw.ImageDraw) -> None:
    d.ellipse(_box(10, 14, 90, 94), fill=ON)
    d.ellipse(_box(20, 24, 80, 84), fill=OFF)
    d.ellipse(_box(26, 30, 74, 78), fill=ON)
    d.ellipse(_box(36, 40, 64, 68), fill=OFF)
    _line(d, [(72, 78), (86, 94), (96, 92)], 6)


def cana_bambu(d: ImageDraw.ImageDraw) -> None:
    pts: list[Pt] = [(18, 92), (82, 8)]
    _line(d, pts, 13)
    for t in (0.28, 0.55, 0.8):
        x, y = 18 + 64 * t, 92 - 84 * t
        _line(d, [(x - 7, y - 5), (x + 7, y + 5)], 3, fill=OFF)
    _line(d, [(60, 38), (74, 44), (88, 40)], 4)


def cana_pescar(d: ImageDraw.ImageDraw) -> None:
    _line(d, [(10, 94), (82, 8)], 7)
    _line(d, [(82, 8), (90, 30), (90, 64)], 2.5)
    d.arc(_box(80, 58, 96, 78), 0, 200, fill=ON, width=round(3.5 * _S))
    d.ellipse(_box(16, 72, 34, 90), fill=ON)
    d.ellipse(_box(21, 77, 29, 85), fill=OFF)


def tronco(d: ImageDraw.ImageDraw) -> None:
    d.rounded_rectangle(_box(6, 30, 90, 74), radius=22 * _S, fill=ON)
    d.ellipse(_box(64, 30, 94, 74), fill=OFF)
    d.ellipse(_box(68, 34, 90, 70), fill=ON)
    d.ellipse(_box(74, 44, 84, 60), fill=OFF)
    _line(d, [(18, 44), (52, 44)], 3, fill=OFF)
    _line(d, [(26, 58), (58, 58)], 3, fill=OFF)


def remo(d: ImageDraw.ImageDraw) -> None:
    _line(d, [(10, 92), (56, 46)], 7)
    blade: list[Pt] = [(50, 2), (60, 6), (64, 30), (56, 52), (50, 54), (44, 52), (36, 30), (40, 6)]
    d.polygon(_p(_rotated(blade, 45, 50, 40)), fill=ON)
    _line(d, [(6, 96), (16, 86)], 9)


def _fish(d: ImageDraw.ImageDraw) -> None:
    d.ellipse(_box(8, 26, 76, 74), fill=ON)
    d.polygon(_p([(68, 50), (94, 26), (88, 50), (94, 74)]), fill=ON)
    d.ellipse(_box(20, 40, 28, 48), fill=OFF)


def pez_crudo(d: ImageDraw.ImageDraw) -> None:
    _fish(d)
    d.arc(_box(30, 30, 50, 70), 300, 60, fill=OFF, width=round(3 * _S))


def pez_asado(d: ImageDraw.ImageDraw) -> None:
    _fish(d)
    for x in (36, 48, 60):
        _line(d, [(x - 4, 36), (x + 4, 64)], 3.5, fill=OFF)
    _line(d, [(2, 50), (98, 50)], 3)


def tarro_aceite(d: ImageDraw.ImageDraw) -> None:
    d.rounded_rectangle(_box(20, 26, 80, 94), radius=12 * _S, fill=ON)
    d.rounded_rectangle(_box(30, 12, 70, 26), radius=3 * _S, fill=ON)
    d.rectangle(_box(24, 24, 76, 28), fill=OFF)
    d.ellipse(_box(36, 50, 64, 82), fill=OFF)
    d.polygon(_p([(50, 42), (38, 64), (62, 64)]), fill=OFF)
    d.ellipse(_box(42, 58, 58, 76), fill=ON)


def mecha(d: ImageDraw.ImageDraw) -> None:
    pts = [(50 + 14 * math.sin(t / 3.0), 92 - t * 4.6) for t in range(0, 15)]
    _line(d, pts, 9)
    d.polygon(_p([(50, 2), (58, 16), (56, 26), (44, 26), (42, 16)]), fill=ON)


def lente(d: ImageDraw.ImageDraw) -> None:
    d.ellipse(_box(8, 8, 92, 92), fill=ON)
    d.ellipse(_box(16, 16, 84, 84), fill=OFF)
    for r in (26, 14):
        d.ellipse(_box(50 - r, 50 - r, 50 + r, 50 + r), outline=ON, width=round(5 * _S))
    d.ellipse(_box(46, 46, 54, 54), fill=ON)


# --- Interfaz ------------------------------------------------------------------------------------

def flecha_sube(d: ImageDraw.ImageDraw) -> None:
    d.polygon(_p([(50, 8), (88, 50), (62, 50), (62, 92), (38, 92), (38, 50), (12, 50)]), fill=ON)


def flecha_baja(d: ImageDraw.ImageDraw) -> None:
    d.polygon(_p([(50, 92), (88, 50), (62, 50), (62, 8), (38, 8), (38, 50), (12, 50)]), fill=ON)


def estable(d: ImageDraw.ImageDraw) -> None:
    d.polygon(_p([(92, 50), (54, 16), (54, 38), (8, 38), (8, 62), (54, 62), (54, 84)]), fill=ON)


def sol(d: ImageDraw.ImageDraw) -> None:
    d.ellipse(_box(28, 28, 72, 72), fill=ON)
    for k in range(8):
        a = math.radians(k * 45)
        _line(d, [(50 + math.cos(a) * 32, 50 + math.sin(a) * 32), (50 + math.cos(a) * 44, 50 + math.sin(a) * 44)], 6)


def luna(d: ImageDraw.ImageDraw) -> None:
    d.ellipse(_box(12, 12, 88, 88), fill=ON)
    d.ellipse(_box(30, 2, 100, 72), fill=OFF)


def cuaderno(d: ImageDraw.ImageDraw) -> None:
    d.rounded_rectangle(_box(16, 6, 86, 94), radius=6 * _S, fill=ON)
    d.rectangle(_box(16, 6, 28, 94), fill=ON)
    for y in (30, 44, 58, 72):
        _line(d, [(38, y), (76, y)], 3.5, fill=OFF)
    for y in (16, 50, 84):
        d.ellipse(_box(10, y - 4, 22, y + 4), fill=OFF)
    _line(d, [(30, 6), (30, 94)], 2.5, fill=OFF)


def faro(d: ImageDraw.ImageDraw) -> None:
    d.polygon(_p([(38, 28), (62, 28), (70, 92), (30, 92)]), fill=ON)
    d.rectangle(_box(36, 14, 64, 28), fill=ON)
    d.polygon(_p([(34, 14), (50, 2), (66, 14)]), fill=ON)
    d.rectangle(_box(42, 17, 58, 25), fill=OFF)
    d.polygon(_p([(36, 52), (64, 52), (65.5, 62), (34.5, 62)]), fill=OFF)
    d.polygon(_p([(68, 18), (98, 8), (98, 34)]), fill=ON)
    d.polygon(_p([(32, 18), (2, 8), (2, 34)]), fill=ON)


def candado(d: ImageDraw.ImageDraw) -> None:
    d.rounded_rectangle(_box(20, 44, 80, 94), radius=8 * _S, fill=ON)
    d.arc(_box(30, 12, 70, 62), 180, 360, fill=ON, width=round(9 * _S))
    d.rectangle(_box(30, 36, 39, 46), fill=ON)
    d.rectangle(_box(61, 36, 70, 46), fill=ON)
    d.ellipse(_box(44, 58, 56, 70), fill=OFF)
    d.rectangle(_box(47.5, 66, 52.5, 80), fill=OFF)


def pagina(d: ImageDraw.ImageDraw) -> None:
    d.polygon(_p([(18, 4), (62, 4), (84, 26), (84, 96), (18, 96)]), fill=ON)
    _line(d, [(62, 2), (62, 26), (86, 26)], 4, fill=OFF)
    d.polygon(_p([(66, 6), (66, 22), (82, 22)]), fill=ON)
    for y in (42, 56, 70, 84):
        _line(d, [(30, y), (72, y)], 3.5, fill=OFF)


def anillo(d: ImageDraw.ImageDraw) -> None:
    d.ellipse(_box(14, 14, 86, 86), outline=ON, width=round(7 * _S))


ICONS: tuple[tuple[str, Callable[[ImageDraw.ImageDraw], None]], ...] = (
    ('sed', sed), ('hambre', hambre), ('energia', energia), ('calor', calor),
    ('cuchillo', cuchillo), ('cantimplora', cantimplora), ('barometro', barometro), ('coco', coco),
    ('coco_vacio', coco_vacio), ('lena', lena), ('fibra', fibra), ('cuerda', cuerda),
    ('cana_bambu', cana_bambu), ('cana_pescar', cana_pescar), ('tronco', tronco), ('remo', remo),
    ('pez_crudo', pez_crudo), ('pez_asado', pez_asado), ('tarro_aceite', tarro_aceite), ('mecha', mecha),
    ('lente', lente), ('flecha_sube', flecha_sube), ('flecha_baja', flecha_baja), ('estable', estable),
    ('sol', sol), ('luna', luna), ('cuaderno', cuaderno), ('faro', faro),
    ('candado', candado), ('pagina', pagina), ('anillo', anillo),
)


def draw_cell(painter: Callable[[ImageDraw.ImageDraw], None]) -> Image.Image:
    big = Image.new('L', (CELL * SUPERSAMPLE, CELL * SUPERSAMPLE), OFF)
    painter(ImageDraw.Draw(big))
    return big.resize((CELL, CELL), Image.Resampling.LANCZOS)


def icon_sheet() -> Image.Image:
    """Hoja RGBA: blanco puro con la silueta en el canal alfa (el juego tiñe el color)."""
    if len(ICONS) > COLUMNS * ROWS:
        raise ValueError(f'{len(ICONS)} iconos no caben en {COLUMNS} x {ROWS}')
    alpha = Image.new('L', (CELL * COLUMNS, CELL * ROWS), OFF)
    for index, (_name, painter) in enumerate(ICONS):
        alpha.paste(draw_cell(painter), ((index % COLUMNS) * CELL, (index // COLUMNS) * CELL))
    sheet = Image.new('RGBA', alpha.size, (255, 255, 255, 0))
    sheet.putalpha(alpha)
    return sheet


def coverage(sheet: Image.Image, index: int) -> float:
    """Fracción de la celda cubierta por la silueta (alfa > 50 %)."""
    x, y = (index % COLUMNS) * CELL, (index // COLUMNS) * CELL
    cell = sheet.getchannel('A').crop((x, y, x + CELL, y + CELL))
    histogram = cell.histogram()
    return sum(histogram[128:]) / float(CELL * CELL)
