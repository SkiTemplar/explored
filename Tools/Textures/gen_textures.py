"""Genera las texturas procedurales periódicas (sin costuras) de Explored.

Uso (desde la raíz del repositorio):
    uv run --with numpy --with pillow python Tools/Textures/gen_textures.py
    uv run --with numpy --with pillow python Tools/Textures/gen_textures.py --size 2048 --only SandDry Grass
    uv run --with numpy --with pillow python Tools/Textures/gen_textures.py --sheet docs/art/texturas-AAAA-MM-DD.png

Salida en Art/Export/Textures/ (no se versiona):
  Legado (las usa hoy build_materials.py; no cambian):
    T_TerrainDetail.png  RGBA: R ruido fino, G ruido medio, B guijarros (celular), A vetas.
    T_TerrainNormal.png  normales de las alturas combinadas (verde OpenGL, leído por HLSL propio).
    T_LeafNoise.png      variación para hojas (R ruido, G venas, B moteado, A máscara de borde).
    T_WaterFoam.png      patrón de espuma del océano (gris).
    T_WaterRipple.png    normales de oleaje fino del océano.
  Juegos PBR estilizados (ver docs/art/texturas.md):
    T_<Material>_BC.png  color base sRGB.
    T_<Material>_N.png   normal en espacio tangente, convención DirectX (la de Unreal).
    T_<Material>_ARH.png R oclusión, G rugosidad, B altura (lineal).
    T_WaterWaves_N.png, T_SeaFoam_M.png (RGBA de máscaras de espuma).
  textures.json          manifiesto nombre -> {kind, srgb} que usa import_textures.py.
"""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from texgen.legacy import LEGACY_KINDS, generate_legacy  # noqa: E402
from texgen.materials import MATERIALS, default_seed, generate  # noqa: E402
from texgen.output import KINDS, contact_sheet, lit_preview, texture_name, write_manifest, write_maps  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "Art" / "Export" / "Textures"


def preview_card(name: str, maps: dict, seed: int) -> dict:
    spec = MATERIALS[name]
    info = f"{spec.tile_m:g} m/tile · semilla {seed}"
    if "BC" in maps:
        arh = maps["ARH"]
        lit = lit_preview(maps["BC"], maps["N"], arh[..., 0], arh[..., 1])
        thumbs = [maps["BC"], maps["N"] * 0.5 + 0.5, arh]
    elif "N" in maps:
        import numpy as np

        water = np.broadcast_to(np.array([0.10, 0.42, 0.52]), maps["N"].shape).copy()
        lit = lit_preview(water, maps["N"], None, np.full(maps["N"].shape[:2], 0.08))
        thumbs = [maps["N"] * 0.5 + 0.5]
    else:
        m = maps["M"]
        import numpy as np

        sea = np.array([0.12, 0.45, 0.55])
        foam = np.clip(m[..., 0] * 0.8 + m[..., 1] * 0.5 + m[..., 2] * 0.5, 0, 1)[..., None]
        lit = sea + (np.array([0.95, 0.97, 0.96]) - sea) * foam
        thumbs = [m[..., :3], np.repeat(m[..., 3:4], 3, axis=-1)]
    return {"name": name, "info": info, "lit": lit, "thumbs": thumbs}


def main(argv: list[str] | None = None) -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--size", type=int, default=1024, help="resolución (potencia de 2: 1024 o 2048)")
    ap.add_argument("--only", nargs="*", help="solo estos materiales (p. ej. SandDry Grass)")
    ap.add_argument("--out", type=Path, default=OUT)
    ap.add_argument("--no-legacy", action="store_true", help="no regenerar las texturas legado")
    ap.add_argument("--sheet", type=Path, help="escribe una hoja de contacto PNG en esta ruta")
    ap.add_argument("--seed", type=int, help="semilla global (por defecto, una estable por material)")
    args = ap.parse_args(argv)

    if args.size & (args.size - 1) or args.size < 64:
        ap.error("--size debe ser potencia de 2 (>= 64)")
    names = args.only or list(MATERIALS)
    unknown = [n for n in names if n not in MATERIALS]
    if unknown:
        ap.error(f"materiales desconocidos: {unknown}; hay {list(MATERIALS)}")

    args.out.mkdir(parents=True, exist_ok=True)
    manifest: dict[str, dict] = {}
    if not args.no_legacy:
        for name, img in generate_legacy().items():
            img.save(args.out / f"{name}.png")
            print(f"[texturas] {args.out / name}.png")
    for name, (kind, srgb) in LEGACY_KINDS.items():
        manifest[name] = {"kind": kind, "srgb": srgb}

    cards = []
    for name in MATERIALS:
        for suffix in MATERIALS[name].outputs:
            kind, srgb = KINDS[suffix]
            manifest[texture_name(name, suffix)] = {"kind": kind, "srgb": srgb}
    for name in names:
        seed = args.seed if args.seed is not None else default_seed(name)
        t0 = time.perf_counter()
        maps = generate(name, args.size, seed)
        written = write_maps(args.out, name, maps)
        print(f"[texturas] {name}: {', '.join(written)} ({time.perf_counter() - t0:.1f} s)")
        if args.sheet:
            cards.append(preview_card(name, maps, seed))
    write_manifest(args.out, manifest)

    if args.sheet:
        title = f"Explored · texturas procedurales · {args.size}px · vista iluminada en 2×2 (tileado) · BC / N / ARH"
        path = contact_sheet(cards, args.sheet, title)
        print(f"[texturas] hoja de contacto: {path} ({path.stat().st_size / 1e6:.2f} MB)")


if __name__ == "__main__":
    main()
