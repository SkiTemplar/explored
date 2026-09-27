"""Genera la textura atlas de paleta compartida (256x256, 4x4 celdas de 64 px).

Un solo atlas para que las mallas de packs distintos (Kenney Nature Kit,
Kenney Survival Kit) lean como del mismo juego: verdes tropicales saturados
pero no fosforitos, corteza calida, gris de roca neutro, arena calida y un
puñado de colores de flor.

Uso: uv run --with pillow python Tools/Packs/make_palette.py
Escribe Tools/Packs/palette_lowpoly.png (se copia despues junto al FBX final
o se referencia por material; ver process_landing_set.py).
"""
from __future__ import annotations

from pathlib import Path

CELL = 64
COLS = 4
ROWS = 4
OUT = Path(__file__).resolve().parent / "palette_lowpoly.png"

# (fila, columna) -> nombre, RGB. Fila 0 = verdes de follaje, fila 1 = madera/
# roca/tierra, fila 2 = flores, fila 3 = reservado (sin usar de momento).
SWATCHES: dict[tuple[int, int], tuple[str, tuple[int, int, int]]] = {
    (0, 0): ("LeafDark", (34, 90, 56)),
    (0, 1): ("LeafMid", (58, 140, 76)),
    (0, 2): ("LeafLight", (122, 178, 86)),
    (0, 3): ("GrassGreen", (96, 160, 72)),
    (1, 0): ("BarkWarm", (122, 79, 48)),
    (1, 1): ("BarkPale", (198, 162, 120)),
    (1, 2): ("RockGrey", (142, 140, 134)),
    (1, 3): ("SandTan", (216, 188, 142)),
    (2, 0): ("FlowerRed", (214, 78, 74)),
    (2, 1): ("FlowerYellow", (238, 178, 74)),
    (2, 2): ("FlowerPurple", (150, 126, 214)),
    (2, 3): ("FlowerWhite", (238, 232, 214)),
}


def cell_uv_center(row: int, col: int) -> tuple[float, float]:
    """UV (Blender: V=0 abajo) del centro de la celda, coherente con como se pinta el PNG."""
    u = (col + 0.5) / COLS
    v = 1.0 - (row + 0.5) / ROWS
    return u, v


def main() -> None:
    from PIL import Image  # import diferido: este modulo tambien se importa desde Blender

    img = Image.new("RGB", (COLS * CELL, ROWS * CELL), (255, 0, 255))  # magenta = celda sin usar, visible si hay bug
    for (row, col), (_name, rgb) in SWATCHES.items():
        x0, y0 = col * CELL, row * CELL
        for x in range(x0, x0 + CELL):
            for y in range(y0, y0 + CELL):
                img.putpixel((x, y), rgb)
    img.save(OUT)
    print(f"[ok] paleta escrita en {OUT} ({img.size[0]}x{img.size[1]})")
    for (row, col), (name, rgb) in sorted(SWATCHES.items()):
        u, v = cell_uv_center(row, col)
        print(f"  {name:12s} fila={row} col={col} rgb={rgb} uv_centro=({u:.3f},{v:.3f})")


if __name__ == "__main__":
    main()
