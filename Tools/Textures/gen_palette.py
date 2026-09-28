"""Genera la paleta low poly de Explored: paleta.json (versionado), los atlas
T_Palette_<Isla>.png y, opcionalmente, la hoja de contacto.

Uso (desde la raíz del repositorio; dependencias en Tools/Textures/pyproject.toml):
    uv run --project Tools/Textures python Tools/Textures/gen_palette.py
    uv run --with numpy --with pillow python Tools/Textures/gen_palette.py   (forma antigua, sigue valiendo)
    uv run --with numpy --with pillow python Tools/Textures/gen_palette.py --sheet docs/art/paleta-AAAA-MM-DD.png

Salida:
    Tools/Textures/paleta.json           paleta por isla y familia (sRGB + lineal, celdas y UV).
    Art/Export/Textures/T_Palette_*.png  atlas 512×512 (no se versiona; gen_textures.py también los escribe).
La definición está en texgen/palette.py; ver docs/art/paleta.md.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))

from texgen.output import to_u8
from texgen.palette import (
    ISLANDS,
    TERRAIN_TARGETS,
    build_atlas,
    island_swatches,
    palette_texture_name,
    to_json,
)

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
JSON_PATH = HERE / "paleta.json"
OUT = ROOT / "Art" / "Export" / "Textures"


def palette_json_text() -> str:
    return json.dumps(to_json(), indent=2, ensure_ascii=False) + "\n"


def write_atlases(out: Path) -> list[Path]:
    out.mkdir(parents=True, exist_ok=True)
    paths = []
    for isl in ISLANDS:
        path = out / f"{palette_texture_name(isl)}.png"
        Image.fromarray(to_u8(build_atlas(island_swatches(isl))), "RGB").save(path, compress_level=6)
        paths.append(path)
    return paths


def main(argv: list[str] | None = None) -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", type=Path, default=OUT, help="carpeta de los atlas PNG")
    ap.add_argument("--json", type=Path, default=JSON_PATH, help="ruta de paleta.json")
    ap.add_argument("--sheet", type=Path, help="escribe la hoja de contacto PNG en esta ruta")
    ap.add_argument("--sheet-size", type=int, default=512, help="resolución del terreno en la hoja")
    args = ap.parse_args(argv)

    # Como --out y --sheet, --json crea su carpeta si falta (antes fallaba con FileNotFoundError).
    args.json.parent.mkdir(parents=True, exist_ok=True)
    args.json.write_text(palette_json_text(), encoding="utf-8")
    print(f"[paleta] {args.json}")
    for p in write_atlases(args.out):
        print(f"[paleta] {p}")

    if args.sheet:
        from texgen.materials import MATERIALS, Material, default_seed, generate
        from texgen.palette_sheet import contact_sheet

        terrain, raw = {}, {}
        for name in TERRAIN_TARGETS:
            terrain[name] = generate(name, args.sheet_size)["BC"]
            out = MATERIALS[name].fn(args.sheet_size, default_seed(name))
            raw[name] = out.finish()["BC"] if isinstance(out, Material) else out["BC"]
        path = contact_sheet(args.sheet, terrain, raw,
                             "Explored · paleta low poly por isla · atlas 512 px + muestra a 1-3 m")
        print(f"[paleta] hoja de contacto: {path} ({path.stat().st_size / 1e6:.2f} MB)")


if __name__ == "__main__":
    main()
