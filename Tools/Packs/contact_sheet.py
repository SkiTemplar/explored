"""Compone la hoja de contacto de un lote a partir de las viñetas de ``normalize.py --tiles``.

Uso (desde la raíz del repositorio):
    uv run --with pillow python Tools/Packs/contact_sheet.py lote1-herramientas

Cada celda: a la izquierda el modelo con el color original del pack, a la derecha el
normalizado (escala, pivote y paleta de Landing), con el id de juego, el fichero de origen,
las medidas en metros y los triángulos. Se guarda en ``docs/art/packs/<lote>.png`` con
paleta indexada para no pasar de 1 MB.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path
from typing import Any

from PIL import Image, ImageDraw, ImageFont

REPO = Path(__file__).resolve().parent.parent.parent
CATALOG = REPO / "Content" / "Data" / "packs_catalogo.json"
TILES = REPO / "Art" / "Export" / "Packs"
MAX_BYTES = 1_000_000
COLS = 3
LABEL_H = 54
BG = (38, 41, 46)
FG = (235, 232, 224)
DIM = (160, 164, 170)


def font(size: int) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    for name in ("DejaVuSans.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    return ImageFont.load_default()


def build(lote: str) -> Path:
    catalog = json.loads(CATALOG.read_text(encoding="utf-8"))
    info = next((lt for lt in catalog["lotes"] if lt["id"] == lote), None)
    if info is None:
        raise SystemExit(f"lote desconocido: {lote}")
    entries = [e for e in catalog["entries"] if e["lote"] == lote]
    if not entries:
        raise SystemExit(f"el lote {lote} no tiene entradas en el catálogo")
    tiles_dir = TILES / lote / "_tiles"
    report_path = tiles_dir / "report.json"
    if not report_path.exists():
        raise SystemExit(f"falta {report_path}: ejecuta antes normalize.py --tiles")
    report = json.loads(report_path.read_text(encoding="utf-8"))
    # Fauna con rig: además de <id>.png, una viñeta por pose extra (<id>@<acción>.png).
    cells: list[tuple[dict[str, Any], str | None]] = []
    for e in entries:
        cells.append((e, None))
        poses = (e.get("rig") or {}).get("tilePoses", [])[1:]
        cells.extend((e, p["action"]) for p in poses)
    tiles = [Image.open(tiles_dir / (f"{e['gameId']}.png" if a is None else f"{e['gameId']}@{a}.png")).convert("RGB")
             for e, a in cells]
    tw, th = tiles[0].size
    rows = (len(tiles) + COLS - 1) // COLS
    header = 64
    sheet = Image.new("RGB", (COLS * tw, header + rows * (th + LABEL_H)), BG)
    draw = ImageDraw.Draw(sheet)
    draw.text((16, 12), f"{lote} · {info['date']} · original del pack (izq.) / normalizado a paleta Landing (dcha.)",
              font=font(20), fill=FG)
    draw.text((16, 38), info["scope"], font=font(14), fill=DIM)
    for i, ((e, action), tile) in enumerate(zip(cells, tiles, strict=True)):
        x = (i % COLS) * tw
        y = header + (i // COLS) * (th + LABEL_H)
        sheet.paste(tile, (x, y))
        r = report.get(e["gameId"])
        if r is None:
            raise SystemExit(f"{e['gameId']}: no está en {report_path} (vuelve a normalizar el lote)")
        dims = " × ".join(f"{d:.2f}" for d in r["dims"])
        tile_poses = (e.get("rig") or {}).get("tilePoses")
        pose = f" · {action or tile_poses[0]['action']}" if tile_poses else ""
        draw.text((x + 10, y + th + 4), f"{e['gameId']} → {e['mesh']}{pose}", font=font(16), fill=FG)
        draw.text((x + 10, y + th + 26), f"{e['pack']}: {Path(e['file']).name} · {dims} m · {r['tris']} tris",
                  font=font(12), fill=DIM)
    out = REPO / "docs" / "art" / "packs" / f"{lote}.png"
    out.parent.mkdir(parents=True, exist_ok=True)
    sheet.quantize(colors=256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).save(out, optimize=True)
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
