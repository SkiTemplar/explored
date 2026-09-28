"""Hoja de contacto por material del juego estilizado (docs/art/texturas-estilizadas-*.png).

Cada hoja enseña lo que hay que mirar antes de dar un material por bueno:
  1. Iluminada 3 × 3 tiles: forma, trazo, relieve suave y que no haya costura.
  2. Campo de 30 m a 5 cm/px, la escala de pantalla de un suelo a unos 50 m (1080p, 60°):
     si se ve una rejilla de manchas, la macro-variación no funciona.
  3. El mismo tile teñido como lo tiñe M_Terrain en cada isla (solo capas del terreno).
  4. Mapas sueltos: BC, N, AO, rugosidad (0,85–0,95) y altura.
  5. Las muestras de paleta que usa, con su tramo abajo → medio → arriba.
"""

from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageOps

from .output import _font, lit_preview, linear_to_srgb, srgb_to_linear, to_u8
from .palette import ISLANDS, TERRAIN_TARGETS, oklab_to_srgb, vertex_tint
from .stylized import PALETTES, swatch_ramp

BG = (32, 34, 40)
FG = (240, 236, 226)
DIM = (170, 172, 180)
PAD = 16
BIG = 600
FAR_M = 30.0  # lado del campo lejano en metros (600 px a 5 cm/px)
MAX_BYTES = 1_500_000


def _img(arr: np.ndarray) -> Image.Image:
    if arr.ndim == 2:
        arr = np.repeat(arr[..., None], 3, axis=-1)
    return Image.fromarray(to_u8(arr[..., :3]), "RGB")


def _lit(maps: dict, albedo: np.ndarray | None = None) -> np.ndarray:
    arh = maps["ARH"]
    bc = maps["BC"] if albedo is None else albedo
    return lit_preview(bc, maps["N"], arh[..., 0], arh[..., 1])


def _tinted(bc: np.ndarray, vertex_rgb) -> np.ndarray:
    return linear_to_srgb(vertex_tint(srgb_to_linear(bc), vertex_rgb))


def material_sheet(name: str, maps: dict, seed: int, tile_m: float, use: str, path: Path) -> Path:
    lit = _lit(maps)
    size = lit.shape[0]
    W = PAD * 3 + BIG * 2
    island_px = (W - PAD * 5) // 4
    map_px = (W - PAD * 6) // 5
    head = 92
    H = head + BIG + 34 + island_px + 40 + map_px + 40 + 70 + PAD
    sheet = Image.new("RGB", (W, H), BG)
    draw = ImageDraw.Draw(sheet)
    draw.text((PAD, 12), f"{name} · estilizada", fill=FG, font=_font(28))
    draw.text((PAD, 50), f"{use}  ·  {tile_m:g} m/tile  ·  semilla {seed}  ·  {size} px", fill=DIM, font=_font(15))
    f_lab = _font(15)

    # 1. Iluminada 3 × 3.
    y = head
    sheet.paste(_img(np.tile(lit, (3, 3, 1))).resize((BIG, BIG), Image.LANCZOS), (PAD, y))
    draw.text((PAD, y + BIG + 6), f"iluminada, 3 × 3 tiles ({3 * tile_m:g} m)", fill=DIM, font=f_lab)

    # 2. Campo lejano: se repite el tile hasta cubrir FAR_M y se reduce por caja (como un mip).
    reps = max(2, int(round(FAR_M / tile_m)))
    small = _img(lit).resize((max(8, BIG // reps + 1),) * 2, Image.BOX)
    far = Image.new("RGB", (small.width * reps, small.height * reps))
    for i in range(reps):
        for j in range(reps):
            far.paste(small, (i * small.width, j * small.height))
    sheet.paste(far.resize((BIG, BIG), Image.BOX), (PAD * 2 + BIG, y))
    draw.text((PAD * 2 + BIG, y + BIG + 6), f"{FAR_M:g} m de lado, {reps} × {reps} tiles: escala de pantalla a ~50 m",
              fill=DIM, font=f_lab)

    # 3. Por isla.
    y += BIG + 34
    terrain = name in TERRAIN_TARGETS
    for k, island in enumerate(ISLANDS):
        x = PAD + k * (island_px + PAD)
        if terrain:
            bc = _tinted(maps["BC"], island.vertex[TERRAIN_TARGETS[name][4]])
        else:
            bc = maps["BC"]
        tile = _img(_lit(maps, bc)).resize((island_px, island_px), Image.LANCZOS)
        sheet.paste(tile, (x, y))
        label = f"{island.key}" + ("  (teñida por M_Terrain)" if terrain else "  (sin tinte)")
        draw.text((x, y + island_px + 6), label, fill=DIM, font=f_lab)

    # 4. Mapas.
    y += island_px + 40
    arh = maps["ARH"]
    thumbs = [("BC", maps["BC"]), ("N", maps["N"] * 0.5 + 0.5), ("AO", arh[..., 0]),
              (f"rugosidad {arh[..., 1].min():.2f}–{arh[..., 1].max():.2f}", arh[..., 1]), ("altura", arh[..., 2])]
    for k, (label, arr) in enumerate(thumbs):
        x = PAD + k * (map_px + PAD)
        sheet.paste(_img(arr).resize((map_px, map_px), Image.LANCZOS), (x, y))
        draw.text((x, y + map_px + 6), label, fill=DIM, font=f_lab)

    # 5. Paleta: una tira por muestra con su tramo abajo → medio → arriba.
    y += map_px + 40
    draw.text((PAD, y), "paleta (paleta.json, isla de referencia):", fill=DIM, font=f_lab)
    y += 24
    chip_w = (W - PAD * (len(PALETTES[name]) + 1)) // len(PALETTES[name])
    for k, sw in enumerate(PALETTES[name]):
        ramp = swatch_ramp(sw)
        t = np.linspace(-1.0, 1.0, chip_w)
        lab = np.where((t < 0)[:, None], ramp[1] + (ramp[1] - ramp[0]) * t[:, None],
                       ramp[1] + (ramp[2] - ramp[1]) * t[:, None])
        strip = np.repeat(np.clip(oklab_to_srgb(lab), 0, 1)[None], 20, axis=0)
        x = PAD + k * (chip_w + PAD)
        sheet.paste(_img(strip), (x, y))
        draw.text((x, y + 24), sw, fill=FG, font=f_lab)

    path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(path, optimize=True)
    for bits in (6, 5):
        if path.stat().st_size <= MAX_BYTES:
            break
        ImageOps.posterize(sheet, bits).save(path, optimize=True)
    return path


def sheet_path(directory: Path, name: str) -> Path:
    return directory / f"texturas-estilizadas-{name}.png"
