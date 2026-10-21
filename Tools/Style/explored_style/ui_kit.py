"""Kit de interfaz de Baliza Siete: colores de la paleta para C++ y hoja de iconos.

Genera, a partir de `paleta.json` y de `ui_icons.ICONS`:

- `Source/Explored/UI/ExploredUIPalette.h`: los 41 colores de la paleta (sRGB) y el enum
  `EExploredIcon` con la celda de cada icono. La interfaz no define colores propios: todos salen de aquí.
- `Content/Slate/T_IconosUI.png`: la hoja de iconos (única textura de interfaz; ver ui_icons).
- `docs/v3/laminas/f5-iconos.png`: lámina de revisión con los iconos teñidos sobre el papel.

Uso: `uv run style-ui [--check]`. Con `--check` no escribe: falla si lo generado no coincide con lo
que hay en el repositorio (lo usa la batería de tests).
"""

from __future__ import annotations

import argparse
import io

from PIL import Image

from explored_style import ui_icons
from explored_style.palette import REPO_ROOT, Palette, load_palette

HEADER_PATH = REPO_ROOT / 'Source' / 'Explored' / 'UI' / 'ExploredUIPalette.h'
SHEET_PATH = REPO_ROOT / 'Content' / 'Slate' / 'T_IconosUI.png'
LAMINA_PATH = REPO_ROOT / 'docs' / 'v3' / 'laminas' / 'f5-iconos.png'


def camel(name: str) -> str:
    return ''.join(part[:1].upper() + part[1:] for part in name.split('_'))


def header_text(palette: Palette) -> str:
    lines = [
        '// Generado por Tools/Style (`uv run style-ui`). No editar a mano: la fuente es',
        '// Tools/Style/paleta.json (colores) y Tools/Style/explored_style/ui_icons.py (iconos).',
        '#pragma once',
        '',
        '#include "CoreMinimal.h"',
        '',
        '/** Colores de la paleta única (sRGB, GDD v3 §11). La interfaz los convierte con ExploredPalette::Linear. */',
        'namespace ExploredPalette',
        '{',
    ]
    for color in palette.colors:
        r, g, b = color.rgb
        lines.append(f'\tinline const FColor {camel(color.name)}({r}, {g}, {b}); // {color.index} {color.name}')
    lines += [
        '',
        '\t/** Color lineal para Slate y Canvas, con la opacidad dada. */',
        '\tinline FLinearColor Linear(const FColor& Srgb, float Alpha = 1.0f)',
        '\t{',
        '\t\tFLinearColor Out = FLinearColor::FromSRGBColor(Srgb);',
        '\t\tOut.A = Alpha;',
        '\t\treturn Out;',
        '\t}',
        '}',
        '',
        '/** Celdas de Content/Slate/T_IconosUI.png, en orden de lectura. */',
        'enum class EExploredIcon : uint8',
        '{',
    ]
    for index, (name, _painter) in enumerate(ui_icons.ICONS):
        lines.append(f'\t{camel(name)} = {index},')
    lines += [
        '\tCount',
        '};',
        '',
        'namespace ExploredIconSheet',
        '{',
        f'\tconstexpr int32 Columns = {ui_icons.COLUMNS};',
        f'\tconstexpr int32 Rows = {ui_icons.ROWS};',
        f'\tconstexpr int32 CellPixels = {ui_icons.CELL};',
        '',
        '\t/** Id de items.json (o de necesidad) a icono; Count si no tiene. */',
        '\tinline EExploredIcon FromId(FName Id)',
        '\t{',
    ]
    for name, _painter in ui_icons.ICONS:
        lines.append(f'\t\tif (Id == FName(TEXT("{name}"))) {{ return EExploredIcon::{camel(name)}; }}')
    lines += [
        '\t\treturn EExploredIcon::Count;',
        '\t}',
        '',
        '\t/** Región UV (u0, v0, u1, v1) de la celda del icono. */',
        '\tinline FBox2f UVRegion(EExploredIcon Icon)',
        '\t{',
        '\t\tconst int32 Index = FMath::Clamp(static_cast<int32>(Icon), 0, Columns * Rows - 1);',
        '\t\tconst float U = static_cast<float>(Index % Columns) / Columns;',
        '\t\tconst float V = static_cast<float>(Index / Columns) / Rows;',
        '\t\treturn FBox2f(FVector2f(U, V), FVector2f(U + 1.0f / Columns, V + 1.0f / Rows));',
        '\t}',
        '}',
        '',
    ]
    return '\n'.join(lines)


def png_bytes(image: Image.Image) -> bytes:
    buffer = io.BytesIO()
    image.save(buffer, format='PNG', optimize=False)
    return buffer.getvalue()


def lamina(palette: Palette, sheet: Image.Image) -> Image.Image:
    """Iconos teñidos de tinta sobre papel, a 2x, con una muestra de cada color de necesidad."""
    paper = palette.by_name('papel').rgb
    ink = palette.by_name('tinta').rgb
    cell = ui_icons.CELL
    out = Image.new('RGB', (sheet.width + 64, sheet.height + 64), paper)
    tinted = Image.new('RGBA', sheet.size, (*ink, 255))
    tinted.putalpha(sheet.getchannel('A'))
    out.paste(tinted, (32, 32), tinted)
    needs = ('agua_media', 'madera_oscura', 'hierba_oscura', 'fuego')
    for index, name in enumerate(needs):
        color = Image.new('RGBA', (cell, cell), (*palette.by_name(name).rgb, 255))
        alpha = sheet.getchannel('A').crop((index * cell, 0, (index + 1) * cell, cell))
        color.putalpha(alpha)
        out.paste(color, (32 + index * cell, 32), color)
    return out


def generate(palette: Palette) -> tuple[str, bytes, Image.Image]:
    sheet = ui_icons.icon_sheet()
    return header_text(palette), png_bytes(sheet), lamina(palette, sheet)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0] if __doc__ else None)
    parser.add_argument('--check', action='store_true', help='solo comprueba que lo generado está al día')
    args = parser.parse_args(argv)
    header, sheet, preview = generate(load_palette())
    if args.check:
        stale = []
        if not HEADER_PATH.exists() or HEADER_PATH.read_text(encoding='utf-8') != header:
            stale.append(str(HEADER_PATH))
        if not SHEET_PATH.exists() or SHEET_PATH.read_bytes() != sheet:
            stale.append(str(SHEET_PATH))
        for path in stale:
            print(f'Desfasado: {path} (ejecuta `uv run style-ui`)')
        return 1 if stale else 0
    HEADER_PATH.write_text(header, encoding='utf-8', newline='\n')
    SHEET_PATH.parent.mkdir(parents=True, exist_ok=True)
    SHEET_PATH.write_bytes(sheet)
    LAMINA_PATH.parent.mkdir(parents=True, exist_ok=True)
    preview.save(LAMINA_PATH)
    for path in (HEADER_PATH, SHEET_PATH, LAMINA_PATH):
        print(path)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
