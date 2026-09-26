"""Texturas «legado» que ya usa Tools/Unreal/build_materials.py (M_Terrain, M_Ocean, follaje).

Se conservan tal cual (mismos algoritmos y semillas): a 1024 px el resultado es idéntico
byte a byte al generador original, así los materiales ajustados sobre ellas no cambian.
Nota: T_TerrainNormal y T_WaterRipple salen con verde «hacia arriba» (convención OpenGL);
los materiales los leen con HLSL propio (.rg * 2 - 1) y ya están ajustados así.
"""

from __future__ import annotations

import numpy as np
from PIL import Image

LEGACY_NAMES = ("T_TerrainDetail", "T_TerrainNormal", "T_LeafNoise", "T_WaterFoam", "T_WaterRipple")
# Nombre -> (clase, sRGB) para el manifiesto de importación.
LEGACY_KINDS = {
    "T_TerrainDetail": ("masks", False),
    "T_TerrainNormal": ("normal", False),
    "T_LeafNoise": ("masks", False),
    "T_WaterFoam": ("masks", False),
    "T_WaterRipple": ("normal", False),
}


def periodic_value_noise(size: int, cells: int, rng: np.random.Generator) -> np.ndarray:
    """Ruido de valor con interpolación suave que se repite exactamente cada `size` píxeles."""
    grid = rng.random((cells, cells))
    coords = np.arange(size) * cells / size
    i0 = np.floor(coords).astype(int)
    frac = coords - i0
    t = frac * frac * (3.0 - 2.0 * frac)
    i1 = (i0 + 1) % cells
    a = grid[np.ix_(i0, i0)]
    b = grid[np.ix_(i0, i1)]
    c = grid[np.ix_(i1, i0)]
    d = grid[np.ix_(i1, i1)]
    tx = t[None, :]
    ty = t[:, None]
    return (a * (1 - tx) + b * tx) * (1 - ty) + (c * (1 - tx) + d * tx) * ty


def fbm(size: int, base_cells: int, octaves: int, seed: int) -> np.ndarray:
    rng = np.random.default_rng(seed)
    total = np.zeros((size, size))
    amplitude = 1.0
    norm = 0.0
    cells = base_cells
    for _ in range(octaves):
        total += periodic_value_noise(size, cells, rng) * amplitude
        norm += amplitude
        amplitude *= 0.5
        cells *= 2
    return total / norm


def cellular(size: int, points: int, seed: int) -> np.ndarray:
    """Distancia al punto más cercano en un toro (guijarros o celdas)."""
    rng = np.random.default_rng(seed)
    pts = rng.random((points, 2)) * size
    ys, xs = np.mgrid[0:size, 0:size].astype(np.float32)
    best = np.full((size, size), np.inf, dtype=np.float32)
    for px, py in pts:
        dx = np.abs(xs - px)
        dy = np.abs(ys - py)
        dx = np.minimum(dx, size - dx)
        dy = np.minimum(dy, size - dy)
        best = np.minimum(best, np.sqrt(dx * dx + dy * dy))
    best /= best.max()
    return best


def normalize01(a: np.ndarray) -> np.ndarray:
    lo, hi = a.min(), a.max()
    return (a - lo) / (hi - lo + 1e-9)


def to_u8(a: np.ndarray) -> np.ndarray:
    return np.clip(a * 255.0 + 0.5, 0, 255).astype(np.uint8)


def tileable_waves(size: int, count: int, fmin: int, fmax: int, falloff: float, seed: int) -> np.ndarray:
    """Suma de ondas con vectores de frecuencia enteros: se repite exacta cada `size` píxeles.
    Cresta algo afilada (como el oleaje real) y amplitud que baja con la frecuencia."""
    rng = np.random.default_rng(seed)
    y, x = np.mgrid[0:size, 0:size] / size
    h = np.zeros((size, size))
    for _ in range(count):
        f = int(rng.integers(fmin, fmax + 1))
        angle = rng.uniform(0.0, 2.0 * np.pi)
        kx, ky = int(round(f * np.cos(angle))), int(round(f * np.sin(angle)))
        if kx == 0 and ky == 0:
            continue
        phase = rng.uniform(0.0, 2.0 * np.pi)
        wave = np.sin(2.0 * np.pi * (kx * x + ky * y) + phase)
        crest = 1.0 - np.abs(wave)
        amp = (np.hypot(kx, ky)) ** (-falloff)
        h += amp * (0.6 * wave + 0.4 * (crest * 2.0 - 1.0))
    return normalize01(h)


def height_to_normal(height: np.ndarray, strength: float) -> np.ndarray:
    """Normal con diferencias centrales periódicas (verde = +dh/dv: convención OpenGL; ver nota arriba)."""
    dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * strength
    dy = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * strength
    n = np.stack([-dx, dy, np.ones_like(height)], axis=-1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    return n * 0.5 + 0.5


def generate_legacy(SIZE: int = 1024) -> dict[str, Image.Image]:
    images: dict[str, Image.Image] = {}

    fine = normalize01(fbm(SIZE, 32, 4, seed=11))
    medium = normalize01(fbm(SIZE, 8, 5, seed=23))
    pebbles = 1.0 - normalize01(cellular(SIZE, 420, seed=37))
    streak_base = fbm(SIZE, 4, 4, seed=51)
    streaks = normalize01(np.sin((np.arange(SIZE)[:, None] / SIZE * 48.0 + streak_base * 6.0) * np.pi))

    detail = np.stack([fine, medium, pebbles, streaks], axis=-1)
    images["T_TerrainDetail"] = Image.fromarray(to_u8(detail), "RGBA")

    height = fine * 0.35 + medium * 0.35 + np.power(pebbles, 3.0) * 0.3
    images["T_TerrainNormal"] = Image.fromarray(to_u8(height_to_normal(height, strength=6.0)), "RGB")

    leaf_noise = normalize01(fbm(SIZE, 16, 4, seed=71))
    veins = normalize01(np.abs(np.sin((np.arange(SIZE)[None, :] / SIZE * 20.0 + fbm(SIZE, 6, 3, seed=73) * 3.0) * np.pi)))
    mottled = normalize01(fbm(SIZE, 48, 3, seed=79))
    edge = normalize01(1.0 - cellular(SIZE, 160, seed=83))
    leaf = np.stack([leaf_noise, veins, mottled, edge], axis=-1)
    images["T_LeafNoise"] = Image.fromarray(to_u8(leaf), "RGBA")

    # Espuma del océano: celdas irregulares (dos capas de ondas cruzadas) con agujeros, en gris.
    foam_a = tileable_waves(SIZE, 48, 3, 20, 0.6, seed=101)
    foam_b = tileable_waves(SIZE, 48, 8, 40, 0.4, seed=103)
    foam = np.clip((foam_a * 0.6 + foam_b * 0.4 - 0.45) * 3.0, 0.0, 1.0)
    images["T_WaterFoam"] = Image.fromarray(to_u8(foam), "L")

    # Oleaje fino del agua: ondas de frecuencia entera (se repiten sin costura) convertidas a normal.
    ripple_height = tileable_waves(SIZE, 90, 2, 48, 1.1, seed=107)
    images["T_WaterRipple"] = Image.fromarray(to_u8(height_to_normal(ripple_height, strength=4.5)), "RGB")

    return images
