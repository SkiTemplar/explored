"""Juegos de texturas PBR estilizadas (cartoon suave) para Explored.

Cada generador recibe (size, seed) y devuelve un `Material` con:
  albedo  (H, W, 3) sRGB en [0, 1]  -> T_<Nombre>_BC.png  (sRGB)
  height  (H, W)    en [0, 1]        -> canal B de T_<Nombre>_ARH.png y origen de la normal
  rough   (H, W)    en [0, 1]        -> canal G de T_<Nombre>_ARH.png
  ao      (H, W)    en [0, 1]        -> canal R de T_<Nombre>_ARH.png
  depth: relieve en unidades de tile para la normal (T_<Nombre>_N.png, DirectX)
Los rasgos se definen en coordenadas de tile, así que el aspecto no cambia con la resolución.
Ninguna junta estructural (tablas, cañas, filas) cae en el borde del tile: se desplazan con
una fase fraccionaria para que el borde sea «un sitio cualquiera» de la textura.
"""

from __future__ import annotations

from dataclasses import dataclass, field

import numpy as np

from .noise import (
    ambient_occlusion,
    blur,
    height_to_normal,
    hex_rgb,
    lerp,
    macro_variation,
    mix_color,
    ramp,
    sample,
    scatter_dots,
    smoothstep,
    spectral_noise,
    unit,
    uv_grid,
    voronoi,
)


@dataclass
class Material:
    albedo: np.ndarray
    height: np.ndarray
    rough: np.ndarray
    depth: float
    ao: np.ndarray | None = None
    ao_strength: float = 1.0

    def finish(self) -> dict[str, np.ndarray]:
        h = np.clip(self.height, 0.0, 1.0)
        ao = self.ao if self.ao is not None else ambient_occlusion(h, strength=self.ao_strength)
        # Suelo de AO: las grietas se oscurecen, pero nunca a negro (look limpio, no sucio).
        ao = 0.3 + 0.7 * np.clip(ao, 0.0, 1.0)
        # Ni negro ni blanco puros: albedo cartoon pero dentro de rangos físicos creíbles.
        albedo = np.clip(self.albedo, 0.035, 0.95)
        return {
            "BC": albedo,
            "N": height_to_normal(h, self.depth),
            "ARH": np.stack([np.clip(ao, 0.0, 1.0), np.clip(self.rough, 0.04, 1.0), h], axis=-1),
        }


@dataclass
class Spec:
    name: str
    fn: object
    tile_m: float
    use: str
    outputs: tuple[str, ...] = ("BC", "N", "ARH")
    extra: dict = field(default_factory=dict)


def cavity(height: np.ndarray, radius: float) -> np.ndarray:
    """Convexidad local (+ en aristas y crestas, − en huecos), útil para realzar bordes."""
    return height - blur(height, radius)


def rng_table(seed: int, *shape: int) -> np.ndarray:
    return np.random.default_rng(seed).random(shape)


def strokes(size: int, n: int, seed: int, length: float, width: float, keep: float,
            angle_bias: float | None = None, spread: float = np.pi) -> np.ndarray:
    """Trazos cortos orientados (fibras, pajas, carbones): elipse por celda de un Voronoi."""
    vo = voronoi(size, n, n, seed, jitter=0.8)
    base = 0.0 if angle_bias is None else angle_bias
    ang = base + (vo["id"] - 0.5) * spread if angle_bias is not None else vo["id"] * np.pi
    ca, sa = np.cos(ang), np.sin(ang)
    a = vo["dx"] * ca + vo["dy"] * sa
    b = -vo["dx"] * sa + vo["dy"] * ca
    e = np.hypot(a / length, b / width)
    mask = 1.0 - smoothstep(0.6, 1.0, e)
    return mask * (vo["id2"] < keep)


# ---------------------------------------------------------------------------
# Suelos
# ---------------------------------------------------------------------------

def _sand_base(size: int, seed: int, ripples: float):
    u, v = uv_grid(size)
    warp = spectral_noise(size, seed + 1, 1, 4, 3.0)
    warp2 = spectral_noise(size, seed + 2, 3, 10, 2.5)
    phase = 7 * v + 1 * u + 0.32 * warp + 0.05 * warp2
    s = np.mod(phase, 1.0)
    # Rizo eólico asimétrico: ladera suave a barlovento, cara corta a sotavento.
    rip = np.where(s < 0.72, s / 0.72, (1.0 - s) / 0.28)
    rip = smoothstep(0.0, 1.0, rip)
    zones = smoothstep(0.25, 0.8, unit(spectral_noise(size, seed + 3, 1, 3, 2.0), 2.0))
    fine = unit(spectral_noise(size, seed + 4, 8, 60, 2.0))
    grain = spectral_noise(size, seed + 5, 90, 600, 0.5)
    height = ripples * rip * (0.3 + 0.7 * zones) + 0.22 * fine + 0.04 * unit(grain)
    return u, v, height, rip, zones, fine, grain


def sand_dry(size: int, seed: int) -> Material:
    u, v, height, rip, zones, fine, grain = _sand_base(size, seed, 0.55)
    pebbles = scatter_dots(size, 17, seed + 6, radius=0.2, keep=0.16)
    shells = scatter_dots(size, 9, seed + 7, radius=0.16, keep=0.3)
    height = height + 0.18 * pebbles["mask"] + 0.12 * shells["mask"]
    t = 0.2 + 0.55 * rip * (0.4 + 0.6 * zones) + 0.25 * fine
    albedo = ramp(t, [(0.0, "#c79d62"), (0.35, "#ddb97f"), (0.65, "#ecd29f"), (1.0, "#f8e8c4")])
    # Granos sueltos: unos más oscuros (minerales) y otros claros (coral molido).
    albedo = mix_color(albedo, "#9d7a4f", smoothstep(2.2, 3.2, grain) * 0.55)
    albedo = mix_color(albedo, "#fff6e2", smoothstep(2.3, 3.4, -grain) * 0.6)
    peb_col = lerp(hex_rgb("#a58e74"), hex_rgb("#6f6660"), pebbles["id"])
    albedo = albedo + (peb_col - albedo) * pebbles["mask"][..., None]
    shell_col = lerp(hex_rgb("#f6dccf"), hex_rgb("#fdf3e6"), shells["id"])
    albedo = albedo + (shell_col - albedo) * shells["mask"][..., None]
    albedo = macro_variation(albedo, seed + 20, warm="#ffd9a0", cool="#e8e2cf", amount=0.18, value=0.07)
    rough = 0.9 - 0.05 * fine
    return Material(albedo, height, rough, depth=0.010, ao_strength=0.9)


def sand_wet(size: int, seed: int) -> Material:
    u, v, height, rip, zones, fine, grain = _sand_base(size, seed, 0.3)
    height = blur(height, 0.0015)
    # Charcos y láminas de agua: zonas bajas y lisas que brillan.
    puddle_n = unit(spectral_noise(size, seed + 8, 1, 6, 2.2), 2.0)
    puddle = smoothstep(0.58, 0.72, puddle_n + 0.12 * (0.5 - height))
    height = height * (1.0 - 0.6 * puddle)
    t = 0.2 + 0.5 * rip * (0.4 + 0.6 * zones) + 0.3 * fine
    albedo = ramp(t, [(0.0, "#8f6c44"), (0.4, "#a88457"), (0.75, "#bd9a69"), (1.0, "#cfae7c")])
    albedo = mix_color(albedo, "#7f6a55", puddle * 0.35)
    # Marcas de resaca: líneas finas y onduladas que dejó la espuma al retirarse.
    swash_warp = spectral_noise(size, seed + 9, 1, 5, 2.6)
    swash_phase = 3 * v + 0.45 * swash_warp + 0.3 * spectral_noise(size, seed + 11, 3, 12, 2.0) * 0.2
    line = np.abs(np.mod(swash_phase, 1.0) - 0.5) * 2.0
    froth = (1.0 - smoothstep(0.0, 0.035, 1.0 - line)) * smoothstep(0.4, 0.7, unit(
        spectral_noise(size, seed + 10, 2, 10, 2.0), 2.0))
    froth = froth * (0.6 + 0.4 * unit(spectral_noise(size, seed + 12, 30, 300, 1.0)))
    albedo = mix_color(albedo, "#ece6d6", froth * 0.7)
    albedo = mix_color(albedo, "#5e4a36", smoothstep(2.2, 3.2, grain) * 0.4)
    albedo = macro_variation(albedo, seed + 20, warm="#e6b27a", cool="#b7b3a6", amount=0.16, value=0.07)
    rough = np.clip(0.55 - 0.4 * puddle + 0.15 * froth + 0.08 * (fine - 0.5), 0.08, 1.0)
    return Material(albedo, height, rough, depth=0.006, ao_strength=0.8)


def blades(size: int, n: int, seed: int, length: float, width: float, flow: np.ndarray,
           spread: float) -> dict:
    """Hojas de hierba afiladas vistas desde arriba: una por celda de Voronoi, centrada en su
    punto, orientada según el campo `flow` (radianes) ± `spread`. t = 0 en la base, 1 en la punta."""
    vo = voronoi(size, n, n, seed, jitter=0.9)
    ang = flow + (vo["id"] - 0.5) * spread
    ca, sa = np.cos(ang), np.sin(ang)
    a = -(vo["dx"] * ca + vo["dy"] * sa)
    b = -vo["dx"] * sa + vo["dy"] * ca
    t = np.clip(a / length + 0.5, 0.0, 1.0)
    half = width * (1.0 - t ** 1.5) * np.sqrt(np.clip(t * 6.0, 0, 1)) + 1e-4
    inside = (np.abs(a) < length * 0.5) & (np.abs(b) < half)
    across = np.clip(np.abs(b) / half, 0, 1)
    return {"mask": inside.astype(np.float64), "t": t, "across": across, "id": vo["id2"]}


def grass(size: int, seed: int) -> Material:
    """Césped cartoon: capas de hojas afiladas que se inclinan con un flujo suave (viento, pisadas)."""
    flow = 0.6 + 1.1 * spectral_noise(size, seed + 1, 1, 4, 2.5)
    soil = unit(spectral_noise(size, seed + 2, 4, 60, 1.8))
    height = 0.08 * soil
    tip = np.zeros_like(height)
    blade_id = np.zeros_like(height)
    across = np.ones_like(height)
    layers = 7
    for j in range(layers):
        bl = blades(size, 15 + 2 * j, seed + 10 + j, length=0.95, width=0.11, flow=flow, spread=1.6)
        h = 0.14 + 0.12 * j + 0.12 * bl["t"] - 0.05 * bl["across"] ** 2
        take = (bl["mask"] > 0) & (h > height)
        height = np.where(take, h, height)
        tip = np.where(take, bl["t"], tip)
        blade_id = np.where(take, bl["id"], blade_id)
        across = np.where(take, bl["across"], across)
    top = 0.14 + 0.12 * (layers - 1) + 0.12
    height = height / top
    albedo = ramp(0.75 * height + 0.25 * tip, [(0.0, "#27451d"), (0.3, "#3a6f28"), (0.55, "#5b9a35"),
                                               (0.8, "#8cc147"), (1.0, "#bcdb68")])
    yellow = smoothstep(0.78, 1.0, blade_id) * smoothstep(0.3, 0.6, height)
    blue = smoothstep(0.22, 0.0, blade_id) * smoothstep(0.3, 0.6, height)
    albedo = mix_color(albedo, "#c2c050", yellow * 0.45)
    albedo = mix_color(albedo, "#3e9160", blue * 0.35)
    albedo = mix_color(albedo, "#d6e98a", (1.0 - across) * smoothstep(0.5, 1.0, height) * 0.18)
    # Florecillas: pocas, pequeñas, blancas o amarillas con centro amarillo.
    fl = scatter_dots(size, 11, seed + 5, radius=0.09, keep=0.1)
    petal = np.where(fl["id"] > 0.5, 1.0, 0.0)[..., None]
    fl_col = petal * hex_rgb("#fbf6e6") + (1 - petal) * hex_rgb("#ffd84a")
    core = (1.0 - smoothstep(0.02, 0.04, fl["f1"])) * (fl["mask"] > 0)
    albedo = albedo + (fl_col - albedo) * fl["mask"][..., None]
    albedo = mix_color(albedo, "#e8a326", core * 1.0)
    height = np.maximum(height, fl["mask"] * 0.98)
    albedo = macro_variation(albedo, seed + 20, warm="#c8d65a", cool="#4d9a6a", amount=0.22, value=0.09)
    height = blur(height, 0.0012)
    rough = 0.8 - 0.12 * height
    return Material(albedo, height, rough, depth=0.005, ao_strength=0.6)


def moss(size: int, seed: int) -> Material:
    u, v = uv_grid(size)
    wu = u + 0.012 * spectral_noise(size, seed + 1, 3, 20, 2.0)
    wv = v + 0.012 * spectral_noise(size, seed + 2, 3, 20, 2.0)
    c1 = voronoi(size, 11, 11, seed, jitter=0.9, u=wu, v=wv)
    c2 = voronoi(size, 27, 27, seed + 3, jitter=0.9, u=wu, v=wv)
    d1 = np.clip(1.0 - c1["f1"] / 0.8, 0, 1) ** 0.6
    d2 = np.clip(1.0 - c2["f1"] / 0.75, 0, 1) ** 0.7
    fuzz = unit(spectral_noise(size, seed + 4, 60, 400, 1.0))
    height = 0.6 * d1 + 0.3 * d2 + 0.1 * fuzz
    albedo = ramp(height + 0.1 * (c1["id"] - 0.5), [(0.0, "#23421b"), (0.3, "#3d6e25"), (0.6, "#63a232"),
                                                    (0.85, "#8fc245"), (1.0, "#c3dd66")])
    sporo = scatter_dots(size, 40, seed + 5, radius=0.18, keep=0.2)
    albedo = mix_color(albedo, "#d8d069", sporo["mask"] * smoothstep(0.4, 0.7, height) * 0.8)
    albedo = macro_variation(albedo, seed + 20, warm="#b6c64c", cool="#3f8a60", amount=0.2, value=0.08)
    rough = 0.92 - 0.05 * height
    return Material(albedo, height, rough, depth=0.010, ao_strength=1.3)


def garden_soil(size: int, seed: int) -> Material:
    u, v = uv_grid(size)
    warp = spectral_noise(size, seed + 1, 1, 5, 2.5)
    furrow_phase = 5 * v + 0.13 * warp + 0.31
    ridge = (0.5 + 0.5 * np.cos(2 * np.pi * furrow_phase)) ** 0.8
    clods = voronoi(size, 34, 34, seed + 2, jitter=0.9)
    clod = np.clip(1.0 - clods["f1"] / 0.75, 0, 1) ** 0.5 * (0.5 + 0.5 * clods["id"])
    crumb = unit(spectral_noise(size, seed + 3, 40, 300, 1.2))
    height = 0.55 * ridge + 0.3 * clod + 0.15 * crumb
    pebbles = scatter_dots(size, 21, seed + 4, radius=0.2, keep=0.12)
    height = np.maximum(height, pebbles["mask"] * (0.55 + 0.4 * ridge))
    albedo = ramp(0.25 + 0.55 * height + 0.2 * (clods["id"] - 0.5),
                  [(0.0, "#2c1b11"), (0.35, "#452c1c"), (0.65, "#6a4629"), (1.0, "#8d653f")])
    # Surcos más húmedos (oscuros) y crestas algo más secas.
    moist = smoothstep(0.45, 0.0, ridge)
    albedo = albedo * (1.0 - 0.18 * moist[..., None])
    albedo = albedo + (lerp(hex_rgb("#8a7a6a"), hex_rgb("#b3a28b"), pebbles["id"]) - albedo) * pebbles["mask"][..., None]
    straw = strokes(size, 26, seed + 5, length=0.42, width=0.035, keep=0.3)
    albedo = mix_color(albedo, "#d0ab5c", straw * 0.9)
    height = np.maximum(height, straw * (0.25 + 0.6 * ridge))
    albedo = macro_variation(albedo, seed + 20, warm="#a8683c", cool="#5a5048", amount=0.18, value=0.08)
    rough = 0.9 - 0.2 * moist
    return Material(albedo, height, rough, depth=0.016, ao_strength=1.1)


def ash(size: int, seed: int) -> Material:
    u, v = uv_grid(size)
    drift = unit(spectral_noise(size, seed + 1, 1, 6, 2.8), 2.2)
    warp = spectral_noise(size, seed + 2, 1, 4, 3.0)
    rip = 0.5 + 0.5 * np.sin(2 * np.pi * (5 * v - 2 * u + 0.4 * warp))
    powder = unit(spectral_noise(size, seed + 3, 20, 200, 1.4))
    height = 0.6 * drift + 0.22 * rip * drift + 0.18 * powder
    albedo = ramp(0.3 + 0.5 * height, [(0.0, "#4f4b4c"), (0.35, "#6c6766"), (0.65, "#8d8783"), (1.0, "#b6afa8")])
    char = strokes(size, 30, seed + 4, length=0.3, width=0.12, keep=0.25)
    albedo = mix_color(albedo, "#262223", char * 0.85)
    pumice = scatter_dots(size, 22, seed + 5, radius=0.2, keep=0.15)
    albedo = mix_color(albedo, "#c9c0b3", pumice["mask"] * 0.8)
    height = np.maximum(height, pumice["mask"] * 0.8)
    # Brasas casi apagadas: muy pocas y pequeñas, un guiño de color.
    ember = scatter_dots(size, 19, seed + 6, radius=0.1, keep=0.05)
    albedo = mix_color(albedo, "#c8542c", ember["mask"] * 0.8)
    albedo = macro_variation(albedo, seed + 20, warm="#9b8378", cool="#7d8590", amount=0.18, value=0.09)
    rough = 0.95 - 0.1 * char
    return Material(albedo, height, rough, depth=0.016, ao_strength=0.9)


# ---------------------------------------------------------------------------
# Rocas
# ---------------------------------------------------------------------------

def volcanic_rock(size: int, seed: int) -> Material:
    u, v = uv_grid(size)
    wu = u + 0.02 * spectral_noise(size, seed + 1, 2, 10, 2.0)
    wv = v + 0.02 * spectral_noise(size, seed + 2, 2, 10, 2.0)
    vo = voronoi(size, 6, 6, seed, jitter=0.9, u=wu, v=wv)
    # Facetas planas por celda (low-poly suave): cada losa con su propia inclinación.
    tilt_a = (vo["id2"] - 0.5) * 0.5
    tilt_b = (vo["id"] - 0.5) * 0.5
    facet = 0.55 + 0.2 * vo["id"] + vo["dx"] * tilt_a + vo["dy"] * tilt_b
    crack = smoothstep(0.0, 0.09, vo["edge"])
    small = voronoi(size, 17, 17, seed + 3, jitter=0.9, u=wu, v=wv)
    sub = smoothstep(0.0, 0.07, small["edge"])
    rough_n = unit(spectral_noise(size, seed + 4, 10, 120, 1.6))
    height = facet * (0.3 + 0.7 * crack) * (0.85 + 0.15 * sub) + 0.12 * rough_n
    ves = scatter_dots(size, 44, seed + 5, radius=0.22, keep=0.4, vary=0.6)
    ves2 = scatter_dots(size, 17, seed + 6, radius=0.18, keep=0.18, vary=0.5)
    pits = np.maximum(ves["mask"], ves2["mask"])
    height = height - 0.18 * pits
    height = (height - height.min()) / (height.max() - height.min())
    albedo = ramp(0.35 * rough_n + 0.4 * vo["id"] + 0.25 * crack,
                  [(0.0, "#221e1d"), (0.4, "#36302d"), (0.75, "#4b433f"), (1.0, "#5f5650")])
    rust = smoothstep(0.58, 0.8, unit(spectral_noise(size, seed + 7, 1, 6, 2.2), 2.0))
    albedo = mix_color(albedo, "#6e4430", rust * 0.4)
    edge = np.clip(cavity(height, 0.006) * 9.0, -1, 1)
    albedo = albedo * (1.0 + 0.45 * np.clip(edge, 0, 1))[..., None] * (1.0 - 0.3 * np.clip(-edge, 0, 1))[..., None]
    albedo = mix_color(albedo, "#1c1716", pits * 0.35)
    albedo = macro_variation(albedo, seed + 20, warm="#6e5040", cool="#454a52", amount=0.18, value=0.08)
    rough = 0.82 - 0.12 * np.clip(edge, 0, 1) + 0.1 * pits
    return Material(albedo, height, rough, depth=0.03, ao_strength=1.2)


def limestone(size: int, seed: int) -> Material:
    u, v = uv_grid(size)
    warp = spectral_noise(size, seed + 1, 1, 6, 2.5)
    strata_phase = 6 * v + 0.25 * warp + 0.17
    s = np.mod(strata_phase, 1.0)
    terrace = smoothstep(0.0, 0.18, s) * (1.0 - 0.35 * s)
    wu = u + 0.015 * spectral_noise(size, seed + 2, 2, 10, 2.0)
    wv = v + 0.015 * spectral_noise(size, seed + 3, 2, 10, 2.0)
    blocks = voronoi(size, 4, 3, seed + 4, jitter=0.85, u=wu, v=wv)
    crack = smoothstep(0.0, 0.04, blocks["edge"])
    body = unit(spectral_noise(size, seed + 5, 3, 80, 2.0))
    pits = scatter_dots(size, 46, seed + 6, radius=0.26, keep=0.22, vary=0.7)["mask"]
    height = 0.35 * terrace + 0.35 * body + 0.2 * blocks["id"] + 0.1
    height = height * (0.35 + 0.65 * crack) - 0.12 * pits
    height = np.clip(height, 0, 1)
    albedo = ramp(0.5 * body + 0.3 * terrace + 0.2 * blocks["id"],
                  [(0.0, "#a99f89"), (0.35, "#c6bca3"), (0.7, "#ddd5bf"), (1.0, "#f0eadb")])
    band = smoothstep(0.85, 1.0, np.cos(2 * np.pi * strata_phase * 2.0))
    albedo = mix_color(albedo, "#b49e7d", band * 0.3)
    albedo = mix_color(albedo, "#857a67", (1.0 - crack) * 0.5)
    albedo = mix_color(albedo, "#9c917c", pits * 0.3)
    lichen_n = unit(spectral_noise(size, seed + 7, 3, 20, 1.8), 2.0)
    lichen_fine = unit(spectral_noise(size, seed + 8, 30, 200, 1.0))
    lichen = smoothstep(0.74, 0.79, lichen_n + 0.08 * (lichen_fine - 0.5)) * crack
    lich2 = smoothstep(0.76, 0.8, 1.0 - lichen_n + 0.08 * (lichen_fine - 0.5)) * crack
    albedo = mix_color(albedo, "#dca544", lichen * 0.75)
    albedo = mix_color(albedo, "#9fb07a", lich2 * 0.6)
    edge = np.clip(cavity(height, 0.006) * 8.0, -1, 1)
    albedo = albedo * (1.0 + 0.12 * edge)[..., None]
    albedo = macro_variation(albedo, seed + 20, warm="#e8cf9e", cool="#b9c0c2", amount=0.18, value=0.07)
    rough = 0.86 - 0.08 * np.clip(edge, 0, 1) - 0.05 * lichen
    return Material(albedo, height, rough, depth=0.022, ao_strength=1.1)


# ---------------------------------------------------------------------------
# Construcción
# ---------------------------------------------------------------------------

def palm_thatch(size: int, seed: int) -> Material:
    """Techo de hojas de palma en hileras solapadas (tejas vegetales); v = pendiente abajo."""
    u, v = uv_grid(size)
    rows, per_row = 7, 22
    rv = v * rows + 0.37
    row = np.floor(rv).astype(np.int64) % rows
    t = rv - np.floor(rv)
    tab = rng_table(seed, rows, per_row, 4)
    row_off = rng_table(seed + 1, rows)[row]
    slant = (rng_table(seed + 2, rows)[row] - 0.5) * 0.5
    cu = u * per_row + row_off * per_row + slant * t
    col = np.floor(cu).astype(np.int64) % per_row
    x = cu - np.floor(cu)
    r = tab[row, col]
    width = 0.52 * (1.0 - 0.4 * t ** 2) * (0.85 + 0.15 * r[..., 0])
    length = 0.88 + 0.12 * r[..., 2]
    # Punta redondeada-afilada: la hoja se estrecha en el último 20 % de su largo.
    width = width * np.sqrt(np.clip((length - t) / 0.2, 0.02, 1.0))
    across = np.abs(x - 0.5 - (r[..., 1] - 0.5) * 0.08) / width
    inside = (across < 1.0) & (t < length)
    tip = smoothstep(length, length - 0.1, t)
    prof = np.sqrt(np.clip(1.0 - across ** 2, 0, 1))
    midrib = np.exp(-(across / 0.12) ** 2)
    fibers = unit(spectral_noise(size, seed + 3, 8, 300, 1.2, stretch=(1.0, 7.0)))
    leaf_h = 0.35 + 0.45 * t + 0.12 * prof + 0.06 * midrib + 0.05 * fibers
    under_h = 0.1 + 0.25 * t
    height = np.where(inside, leaf_h * (0.6 + 0.4 * tip), under_h)
    tone = r[..., 3]
    straw = ramp(0.25 + 0.45 * t + 0.3 * fibers,
                 [(0.0, "#8d6a30"), (0.35, "#b28b3f"), (0.7, "#d3ad5c"), (1.0, "#e7cd86")])
    green = ramp(0.25 + 0.45 * t + 0.3 * fibers, [(0.0, "#6f7a31"), (0.5, "#9aa447"), (1.0, "#c3c36a")])
    brown = ramp(0.25 + 0.45 * t + 0.3 * fibers, [(0.0, "#6b4a28"), (0.5, "#8d6638"), (1.0, "#b08650")])
    col_leaf = np.where((tone > 0.82)[..., None], green, np.where((tone < 0.14)[..., None], brown, straw))
    col_leaf = mix_color(col_leaf, "#f1dea0", midrib * 0.3)
    col_leaf = mix_color(col_leaf, "#6e5028", (1.0 - tip) * 0.5)
    under = ramp(t, [(0.0, "#2c2112"), (1.0, "#5a4424")])
    albedo = np.where(inside[..., None], col_leaf, under)
    # Sombra bajo la hilera superior (la parte alta de cada hilera queda tapada).
    shade = smoothstep(0.0, 0.3, t)
    albedo = albedo * (0.72 + 0.28 * shade)[..., None]
    albedo = macro_variation(albedo, seed + 20, warm="#e0b060", cool="#9aa070", amount=0.18, value=0.08)
    height = blur(height, 0.0008)
    rough = np.where(inside, 0.72 - 0.08 * midrib, 0.95)
    return Material(albedo, height, rough, depth=0.02, ao_strength=1.2)


def palm_weave(size: int, seed: int) -> Material:
    """Estera de hoja de palma trenzada en diagonal (paredes, techos interiores, cestos)."""
    u, v = uv_grid(size)
    k = 6
    pa = (u + v) * k + 0.23
    pb = (u - v) * k + 0.41
    ia = np.floor(pa).astype(np.int64)
    ib = np.floor(pb).astype(np.int64)
    fa = pa - ia
    fb = pb - ib
    s = np.where((ia + ib) % 2 == 0, 1.0, -1.0)
    # Cada tira ondula por encima y por debajo de las que cruza.
    ha = 0.5 + 0.32 * s * np.sin(np.pi * fb)
    hb = 0.5 - 0.32 * s * np.sin(np.pi * fa)
    gap_a = smoothstep(0.0, 0.08, fa) * smoothstep(0.0, 0.08, 1.0 - fa)
    gap_b = smoothstep(0.0, 0.08, fb) * smoothstep(0.0, 0.08, 1.0 - fb)
    ha = ha * (0.55 + 0.45 * gap_a) + 0.08 * np.sin(np.pi * fa)
    hb = hb * (0.55 + 0.45 * gap_b) + 0.08 * np.sin(np.pi * fb)
    top_a = ha >= hb
    height = np.maximum(ha, hb)
    fib_a = unit(spectral_noise(size, seed + 1, 6, 300, 1.2, stretch=(6.0, 1.0), angle=np.pi / 4))
    fib_b = unit(spectral_noise(size, seed + 2, 6, 300, 1.2, stretch=(6.0, 1.0), angle=-np.pi / 4))
    fib = np.where(top_a, fib_a, fib_b)
    height = height + 0.05 * fib
    strip_id = np.where(top_a, rng_table(seed + 3, 2 * k)[ia % (2 * k)], rng_table(seed + 4, 2 * k)[ib % (2 * k)])
    t = 0.3 * fib + 0.5 * (height - 0.2) + 0.2 * strip_id
    col_a = ramp(t, [(0.0, "#8e6a33"), (0.4, "#b99147"), (0.8, "#d8b56a"), (1.0, "#ead095")])
    col_b = ramp(t, [(0.0, "#7c6030"), (0.4, "#a48543"), (0.8, "#c6aa66"), (1.0, "#ddc790")])
    albedo = np.where(top_a[..., None], col_a, col_b)
    albedo = albedo * (0.55 + 0.6 * np.clip(height, 0, 1))[..., None]
    gaps = 1.0 - np.where(top_a, gap_a, gap_b)
    albedo = mix_color(albedo, "#3a2a14", gaps * 0.8)
    albedo = mix_color(albedo, "#9da04e", smoothstep(0.85, 0.97, strip_id) * 0.45)
    albedo = macro_variation(albedo, seed + 20, warm="#e5b664", cool="#a6a07a", amount=0.16, value=0.07)
    rough = 0.7 + 0.15 * (1.0 - fib)
    return Material(albedo, height, rough, depth=0.03, ao_strength=1.4)


def bamboo(size: int, seed: int) -> Material:
    """Cañas verticales juntas (paredes, vallas, balsa); u = a lo ancho, v = a lo largo."""
    u, v = uv_grid(size)
    n = 6
    cu = u * n + 0.21
    col = np.floor(cu).astype(np.int64) % n
    x = cu - np.floor(cu)
    tab = rng_table(seed, n, 6)
    r = tab[col]
    prof = np.sqrt(np.clip(1.0 - (2.0 * x - 1.0) ** 2, 0.0, 1.0))
    height = 0.2 + 0.7 * prof ** 0.8
    node_ring = np.zeros_like(u)
    node_dark = np.zeros_like(u)
    for j in range(2):
        pos = r[..., j] * 0.5 + 0.5 * j
        d = np.abs(v - pos)
        d = np.minimum(d, 1.0 - d)
        node_ring += np.exp(-(d / 0.008) ** 2)
        above = np.mod(pos - v, 1.0)
        node_dark += np.exp(-(np.minimum(above, 1.0 - above) / 0.02) ** 2)
    height = height + 0.1 * node_ring * prof
    streak = unit(spectral_noise(size, seed + 1, 6, 300, 1.2, stretch=(1.0, 10.0)))
    height = height + 0.03 * streak
    fresh = r[..., 2]
    t = 0.3 * streak + 0.55 * prof + 0.15 * r[..., 3]
    dry_col = ramp(t, [(0.0, "#8f7536"), (0.45, "#bea056"), (0.8, "#d8bd72"), (1.0, "#ecd699")])
    green_col = ramp(t, [(0.0, "#667a2f"), (0.45, "#8fa743"), (0.8, "#b4c563"), (1.0, "#d3dc8c")])
    albedo = lerp(dry_col, green_col, smoothstep(0.55, 0.9, fresh) * 0.9)
    albedo = mix_color(albedo, "#6b5328", node_ring * 0.55)
    albedo = mix_color(albedo, "#8a6f38", node_dark * 0.3)
    # Brillo vertical de la caña (cartoon): franja clara algo desplazada del centro.
    shine = np.exp(-((x - 0.38) / 0.09) ** 2)
    albedo = albedo * (1.0 + 0.12 * shine)[..., None]
    gap = smoothstep(0.35, 0.0, prof)
    albedo = mix_color(albedo, "#2b2213", gap * 0.85)
    albedo = macro_variation(albedo, seed + 20, warm="#d9b45e", cool="#98aa66", amount=0.12, value=0.06)
    rough = 0.38 + 0.4 * gap + 0.2 * node_ring - 0.05 * shine
    return Material(albedo, height, rough, depth=0.03, ao_strength=1.0)


def wood_planks(size: int, seed: int) -> Material:
    """Tablones horizontales con juntas escalonadas; u = a lo largo de la tabla."""
    u, v = uv_grid(size)
    rows = 5
    rv = v * rows + 0.29
    row = np.floor(rv).astype(np.int64) % rows
    y = rv - np.floor(rv)
    tab = rng_table(seed, rows, 8)
    s0 = tab[row, 0]
    s1 = np.mod(s0 + 0.38 + 0.24 * tab[row, 1], 1.0)
    in_second = np.mod(u - s0, 1.0) >= np.mod(s1 - s0, 1.0)
    board = row * 2 + in_second.astype(np.int64)
    btab = rng_table(seed + 1, rows * 2, 6)
    br = btab[board]

    def wrap_dist(a, b):
        d = np.abs(a - b)
        return np.minimum(d, 1.0 - d)

    seam_d = np.minimum(wrap_dist(u, s0), wrap_dist(u, s1))
    bevel = smoothstep(0.0, 0.07, y) * smoothstep(0.0, 0.07, 1.0 - y) * smoothstep(0.0, 0.006, seam_d)
    long_n = spectral_noise(size, seed + 2, 1, 60, 1.8, stretch=(10.0, 1.0))
    knots = scatter_dots(size, 7, seed + 3, radius=0.35, keep=0.35)
    knot_pull = np.exp(-(knots["f1"] / 0.18) ** 2) * knots["alive"]
    grain_phase = y * (1.4 + br[..., 0]) + br[..., 1] * 7.0 + 0.35 * long_n + 0.9 * knot_pull
    rings = np.mod(grain_phase * 5.0, 1.0)
    late = smoothstep(0.55, 0.9, rings) * smoothstep(1.0, 0.92, rings)
    fibre = unit(spectral_noise(size, seed + 4, 10, 400, 1.0, stretch=(12.0, 1.0)))
    height = 0.75 * bevel + 0.08 * (1.0 - late) + 0.06 * fibre + 0.08 * br[..., 2] * bevel
    base_t = 0.15 + 0.55 * br[..., 3] + 0.2 * fibre + 0.1 * (1.0 - rings)
    albedo = ramp(base_t, [(0.0, "#6a4228"), (0.35, "#8d5e3a"), (0.7, "#b0804f"), (1.0, "#caa06a")])
    albedo = lerp(albedo, albedo * np.array([0.92, 0.95, 1.0]), br[..., 4])
    albedo = mix_color(albedo, "#553520", late * 0.55)
    knot_core = (1.0 - smoothstep(0.03, 0.07, knots["f1"])) * knots["alive"]
    albedo = mix_color(albedo, "#4a2a15", knot_core * 0.9)
    # Clavos junto a las juntas.
    nails = np.zeros_like(u)
    for s in (s0, s1):
        for side in (-1.0, 1.0):
            nu = s + side * 0.018
            for ny in (0.28, 0.72):
                d = np.hypot(wrap_dist(u, nu) * rows, (y - ny) * 1.0) * 1.0
                nails = np.maximum(nails, 1.0 - smoothstep(0.018, 0.03, d))
    albedo = mix_color(albedo, "#3d3432", nails * 0.9)
    height = height - 0.08 * nails
    albedo = mix_color(albedo, "#2a1a10", (1.0 - bevel) * 0.8)
    edge = np.clip(cavity(height, 0.004) * 6.0, 0, 1)
    albedo = albedo * (1.0 + 0.15 * edge)[..., None]
    albedo = macro_variation(albedo, seed + 20, warm="#d08a4c", cool="#8a7560", amount=0.16, value=0.08)
    rough = 0.72 + 0.1 * late + 0.15 * (1.0 - bevel) - 0.35 * nails
    return Material(albedo, np.clip(height, 0, 1), rough, depth=0.012, ao_strength=1.0)


def stone_wall(size: int, seed: int) -> Material:
    """Muro de piedra seca polinesio: basalto encajado, algún bloque de coral, musgo en juntas."""
    u, v = uv_grid(size)
    wu = u + 0.025 * spectral_noise(size, seed + 1, 2, 8, 2.2)
    wv = v + 0.025 * spectral_noise(size, seed + 2, 2, 8, 2.2)
    vo = voronoi(size, 6, 4, seed, jitter=0.75, u=wu, v=wv)
    pillow = smoothstep(0.0, 0.32, vo["edge"]) ** 0.7
    surf = unit(spectral_noise(size, seed + 3, 6, 150, 1.8))
    tilt = vo["dx"] * (vo["id2"] - 0.5) * 0.3
    height = pillow * (0.6 + 0.2 * vo["id"] + 0.15 * surf + tilt) + 0.05 * surf
    height = np.clip(height, 0, 1)
    coral = vo["id2"] > 0.84
    basalt = ramp(0.6 * vo["id"] + 0.4 * surf, [(0.0, "#3c3838"), (0.4, "#524c4b"), (0.75, "#696260"), (1.0, "#7f7671")])
    coral_c = ramp(surf, [(0.0, "#a39880"), (0.6, "#c6bb9f"), (1.0, "#ddd3ba")])
    albedo = np.where(coral[..., None], coral_c, basalt)
    edge = np.clip(cavity(height, 0.008) * 6.0, -1, 1)
    albedo = albedo * (1.0 + 0.3 * np.clip(edge, 0, 1))[..., None]
    joint = 1.0 - smoothstep(0.02, 0.16, vo["edge"])
    albedo = mix_color(albedo, "#2e251e", joint * 0.9)
    moss_n = unit(spectral_noise(size, seed + 4, 3, 30, 1.8), 2.0)
    moss_fine = unit(spectral_noise(size, seed + 5, 25, 250, 1.2))
    moss = smoothstep(0.35, 0.6, joint + (moss_n - 0.5) * 0.9 + (moss_fine - 0.5) * 0.35) * smoothstep(0.4, 0.6, moss_n)
    moss_col = ramp(0.5 * moss_n + 0.5 * moss_fine, [(0.0, "#2f5421"), (0.5, "#4b7f2d"), (0.8, "#6f9f3f"), (1.0, "#9cc05a")])
    albedo = albedo + (moss_col - albedo) * (moss * 0.9)[..., None]
    height = np.maximum(height, moss * (0.2 + 0.25 * moss_fine))
    albedo = macro_variation(albedo, seed + 20, warm="#8a6e5a", cool="#586470", amount=0.16, value=0.08)
    rough = 0.8 - 0.1 * np.clip(edge, 0, 1) + 0.12 * moss
    return Material(albedo, height, rough, depth=0.04, ao_strength=1.3)


# ---------------------------------------------------------------------------
# Tejidos y papel
# ---------------------------------------------------------------------------

def _plain_weave(size: int, seed: int, threads: int, amp: float = 0.28):
    u, v = uv_grid(size)
    pu = u * threads + 0.5
    pv = v * threads + 0.5
    ix = np.floor(pu).astype(np.int64)
    iy = np.floor(pv).astype(np.int64)
    fx = pu - ix
    fy = pv - iy
    s = np.where((ix + iy) % 2 == 0, 1.0, -1.0)
    slub_u = spectral_noise(size, seed + 1, 2, 60, 1.5, stretch=(1.0, 8.0))
    slub_v = spectral_noise(size, seed + 2, 2, 60, 1.5, stretch=(8.0, 1.0))
    thick_warp = 0.42 + 0.05 * rng_table(seed + 3, threads)[ix % threads] + 0.03 * slub_u
    thick_weft = 0.42 + 0.05 * rng_table(seed + 4, threads)[iy % threads] + 0.03 * slub_v
    prof_warp = np.sqrt(np.clip(1.0 - ((fx - 0.5) / thick_warp) ** 2, 0, 1))
    prof_weft = np.sqrt(np.clip(1.0 - ((fy - 0.5) / thick_weft) ** 2, 0, 1))
    warp_h = (0.5 + amp * s * np.sin(np.pi * fy)) * prof_warp
    weft_h = (0.5 - amp * s * np.sin(np.pi * fx)) * prof_weft
    return np.maximum(warp_h, weft_h), warp_h >= weft_h, u, v


def canvas(size: int, seed: int) -> Material:
    weave, warp_top, u, v = _plain_weave(size, seed, 56)
    fuzz = unit(spectral_noise(size, seed + 5, 80, 600, 0.8))
    height = 0.85 * weave + 0.15 * fuzz
    albedo = ramp(0.55 * weave + 0.25 * fuzz + 0.2 * warp_top,
                  [(0.0, "#a8987a"), (0.35, "#cbbd9a"), (0.7, "#e0d4b4"), (1.0, "#efe6cc")])
    stains = smoothstep(0.62, 0.8, unit(spectral_noise(size, seed + 6, 1, 8, 2.2), 2.0))
    albedo = mix_color(albedo, "#b49a6c", stains * 0.35)
    bleach = smoothstep(0.6, 0.85, unit(spectral_noise(size, seed + 7, 1, 5, 2.5), 2.0))
    albedo = mix_color(albedo, "#f5efdf", bleach * 0.3)
    albedo = macro_variation(albedo, seed + 20, warm="#e8cf9a", cool="#c9ccc2", amount=0.14, value=0.06)
    rough = 0.9 + 0.05 * fuzz
    return Material(albedo, height, rough, depth=0.004, ao_strength=1.0)


def rope(size: int, seed: int) -> Material:
    """Cuerda de 3 cabos; u = alrededor de la cuerda, v = a lo largo (UV de cilindro)."""
    u, v = uv_grid(size)
    strands = 3
    phase = strands * u + 5 * v + 0.13
    x = phase - np.floor(phase)
    prof = np.sqrt(np.clip(1.0 - (2.0 * x - 1.0) ** 2, 0, 1)) ** 0.7
    # Hilos de cada cabo, retorcidos en sentido contrario.
    yarn_phase = 14 * u - 22 * v
    yarn = 0.5 + 0.5 * np.cos(2 * np.pi * (yarn_phase + 0.2 * x))
    fuzz = unit(spectral_noise(size, seed + 1, 30, 500, 1.0))
    height = 0.75 * prof + 0.15 * yarn * prof + 0.1 * fuzz
    idx = np.floor(phase).astype(np.int64) % strands
    tone = rng_table(seed + 2, strands)[idx]
    albedo = ramp(0.45 * prof + 0.25 * yarn + 0.2 * fuzz + 0.1 * tone,
                  [(0.0, "#5a4226"), (0.35, "#9a7746"), (0.7, "#c2a064"), (1.0, "#dfc38a")])
    gap = smoothstep(0.3, 0.0, prof)
    albedo = mix_color(albedo, "#3a2a17", gap * 0.8)
    albedo = macro_variation(albedo, seed + 20, warm="#d6a860", cool="#a09a84", amount=0.12, value=0.06)
    rough = 0.88 + 0.07 * fuzz
    return Material(albedo, height, rough, depth=0.025, ao_strength=1.3)


def map_paper(size: int, seed: int) -> Material:
    """Papel viejo del mapa: fibras, manchas de agua con cerco, motas de óxido (foxing)."""
    u, v = uv_grid(size)
    blotch = unit(spectral_noise(size, seed + 1, 1, 10, 2.4), 2.2)
    cockle = unit(spectral_noise(size, seed + 2, 2, 12, 2.6))
    fib_l = strokes(size, 70, seed + 3, length=0.45, width=0.05, keep=0.7)
    fib_d = strokes(size, 45, seed + 4, length=0.45, width=0.04, keep=0.5)
    pulp = unit(spectral_noise(size, seed + 5, 40, 500, 1.1))
    albedo = ramp(0.6 * blotch + 0.25 * pulp + 0.15 * cockle,
                  [(0.0, "#c8ab78"), (0.35, "#dbc596"), (0.7, "#e9dcb7"), (1.0, "#f5ecd2")])
    albedo = mix_color(albedo, "#fbf4de", fib_l * 0.35)
    albedo = mix_color(albedo, "#a88a5a", fib_d * 0.3)
    # Manchas de agua: relleno suave y cerco más oscuro en el borde.
    wu = u + 0.03 * spectral_noise(size, seed + 6, 2, 10, 2.0)
    wv = v + 0.03 * spectral_noise(size, seed + 7, 2, 10, 2.0)
    st = voronoi(size, 3, 3, seed + 8, jitter=0.8, u=wu, v=wv)
    r0 = 0.25 + 0.2 * st["id"]
    alive = st["id2"] < 0.6
    ring = np.exp(-((st["f1"] - r0) / 0.018) ** 2) * alive
    fill = smoothstep(r0, r0 - 0.08, st["f1"]) * alive
    albedo = mix_color(albedo, "#c29a5e", fill * 0.22)
    albedo = mix_color(albedo, "#9b7141", ring * 0.45)
    fox = scatter_dots(size, 24, seed + 9, radius=0.16, keep=0.22)
    fox_soft = scatter_dots(size, 11, seed + 10, radius=0.3, keep=0.2)
    albedo = mix_color(albedo, "#8f5a2e", fox["mask"] * 0.55)
    albedo = mix_color(albedo, "#b88b55", fox_soft["mask"] * 0.3)
    albedo = macro_variation(albedo, seed + 20, warm="#e4b777", cool="#d8d2bb", amount=0.16, value=0.06)
    height = 0.5 * cockle + 0.25 * pulp + 0.15 * fib_l + 0.1 * ring
    rough = 0.88 - 0.1 * fill
    return Material(albedo, height, rough, depth=0.003, ao_strength=0.6)


def bark(size: int, seed: int) -> Material:
    """Corteza fisurada vertical (troncos, postes); v = a lo largo del tronco."""
    u, v = uv_grid(size)
    wu = u + 0.02 * spectral_noise(size, seed + 1, 1, 8, 2.2, stretch=(1.0, 4.0))
    wv = v + 0.01 * spectral_noise(size, seed + 2, 1, 8, 2.2)
    vo = voronoi(size, 8, 2, seed, jitter=0.85, u=wu, v=wv)
    plate = smoothstep(0.0, 0.3, vo["edge"]) ** 0.7
    fibre = unit(spectral_noise(size, seed + 3, 6, 300, 1.3, stretch=(1.0, 9.0)))
    height = plate * (0.7 + 0.15 * vo["id"]) + 0.15 * fibre * plate + 0.05 * fibre
    height = np.clip(height, 0, 1)
    albedo = ramp(0.55 * plate + 0.3 * fibre + 0.15 * vo["id"],
                  [(0.0, "#2f2117"), (0.3, "#5a3f2b"), (0.65, "#7e5c40"), (1.0, "#a2805d")])
    edge = np.clip(cavity(height, 0.006) * 6.0, 0, 1)
    albedo = albedo * (1.0 + 0.2 * edge)[..., None]
    lichen = smoothstep(0.62, 0.75, unit(spectral_noise(size, seed + 4, 3, 30, 1.8), 2.0)) * plate
    albedo = mix_color(albedo, "#7f9a52", lichen * 0.55 * (vo["id2"] > 0.45))
    albedo = macro_variation(albedo, seed + 20, warm="#9a6a44", cool="#6a6458", amount=0.16, value=0.08)
    rough = 0.86 - 0.08 * edge
    return Material(albedo, height, rough, depth=0.03, ao_strength=1.3)


# ---------------------------------------------------------------------------
# Agua
# ---------------------------------------------------------------------------

def water_waves(size: int, seed: int) -> dict[str, np.ndarray]:
    """Normal de oleaje fino con dirección de viento (espectro direccional) y crestas algo afiladas."""
    rng = np.random.default_rng(seed)
    white = rng.standard_normal((size, size))
    f = np.fft.fftfreq(size) * size
    fx, fy = np.meshgrid(f, f)
    r = np.hypot(fx, fy)
    r[0, 0] = 1.0
    wind = np.array([np.cos(0.5), np.sin(0.5)])
    cosang = (fx * wind[0] + fy * wind[1]) / r
    spread = 0.25 + 0.75 * np.abs(cosang) ** 3
    amp = r ** -2.0 * spread * np.exp(-(r / 40.0) ** 2) / (1.0 + (2.0 / r) ** 6)
    amp[0, 0] = 0.0
    swell = np.fft.ifft2(np.fft.fft2(white) * amp).real
    swell /= swell.std()
    chop = spectral_noise(size, seed + 1, 12, 90, 2.0)
    ridge = 1.0 - np.abs(spectral_noise(size, seed + 2, 3, 20, 2.5))
    height = unit(swell) * 0.7 + unit(chop) * 0.08 + np.clip(ridge, 0, 1) ** 2 * 0.22
    return {"N": height_to_normal(height, 0.06), "H": height}


def water_foam(size: int, seed: int) -> dict[str, np.ndarray]:
    """Máscaras de espuma: R encaje (red de celdas), G burbujas, B masa suave, A estelas."""
    u, v = uv_grid(size)
    wu = u + 0.04 * spectral_noise(size, seed + 1, 1, 8, 2.2)
    wv = v + 0.04 * spectral_noise(size, seed + 2, 1, 8, 2.2)
    cells = voronoi(size, 10, 10, seed, jitter=0.95, u=wu, v=wv)
    density = unit(spectral_noise(size, seed + 3, 1, 6, 2.2), 2.0)
    width = 0.03 + 0.18 * density
    lace = 1.0 - smoothstep(width * 0.4, width, cells["edge"])
    lace = lace * (0.55 + 0.45 * unit(spectral_noise(size, seed + 4, 20, 200, 1.2)))
    bub = voronoi(size, 46, 46, seed + 5, jitter=0.9)
    rad = 0.18 + 0.2 * bub["id"]
    bubbles = np.exp(-((bub["f1"] - rad) / 0.05) ** 2) * (bub["id2"] < 0.6)
    bubbles = bubbles * smoothstep(0.3, 0.7, density)
    mass = smoothstep(0.35, 0.85, density) * (0.6 + 0.4 * lace)
    streak = unit(spectral_noise(size, seed + 6, 2, 120, 1.4, stretch=(1.0, 10.0), angle=0.5))
    streak = smoothstep(0.55, 0.8, streak)
    return {"M": np.stack([lace, bubbles, mass, streak], axis=-1)}


MATERIALS: dict[str, Spec] = {
    s.name: s
    for s in [
        Spec("SandDry", sand_dry, 2.0, "Arena seca de playa y dunas (Amaraje, Arenas Blancas)."),
        Spec("SandWet", sand_wet, 2.0, "Arena mojada de la franja de orilla, con charcos brillantes."),
        Spec("Grass", grass, 1.5, "Suelo de hierba corta con matas y florecillas."),
        Spec("Moss", moss, 1.0, "Musgo en cojines para ruinas, rocas y suelo de selva."),
        Spec("GardenSoil", garden_soil, 2.0, "Tierra de huerto labrada en surcos (eje u)."),
        Spec("Ash", ash, 2.0, "Ceniza volcánica (isla del Humo), con carbones y pómez."),
        Spec("VolcanicRock", volcanic_rock, 3.0, "Basalto en losas facetadas con vesículas y óxido."),
        Spec("Limestone", limestone, 3.0, "Caliza clara estratificada con líquenes (Dientes)."),
        Spec("PalmThatch", palm_thatch, 1.0, "Techo de hojas de palma en hileras; v = pendiente abajo."),
        Spec("PalmWeave", palm_weave, 0.6, "Estera trenzada de palma en diagonal (paredes, techos)."),
        Spec("Bamboo", bamboo, 1.0, "Cañas de bambú juntas; v = a lo largo de la caña."),
        Spec("WoodPlanks", wood_planks, 2.0, "Tablones con juntas escalonadas y clavos; u = a lo largo."),
        Spec("StoneWall", stone_wall, 2.5, "Muro polinesio de piedra seca (basalto y coral)."),
        Spec("Canvas", canvas, 0.5, "Lona de vela y toldos (tafetán)."),
        Spec("Rope", rope, 0.25, "Cuerda de 3 cabos; u alrededor, v a lo largo."),
        Spec("MapPaper", map_paper, 0.6, "Papel envejecido del mapa y la UI."),
        Spec("Bark", bark, 1.5, "Corteza fisurada para troncos y postes; v = a lo largo."),
        Spec("WaterWaves", water_waves, 6.0, "Normal de oleaje fino direccional.", outputs=("N",)),
        Spec("SeaFoam", water_foam, 6.0, "Espuma: R encaje, G burbujas, B masa, A estelas.", outputs=("M",)),
    ]
}


def generate(name: str, size: int, seed: int | None = None) -> dict[str, np.ndarray]:
    """Genera los mapas de un material. Semilla por defecto estable por nombre."""
    spec = MATERIALS[name]
    if seed is None:
        seed = default_seed(name)
    out = spec.fn(size, seed)
    if isinstance(out, Material):
        return out.finish()
    return {k: out[k] for k in spec.outputs}


def default_seed(name: str) -> int:
    return sum((i + 1) * ord(c) for i, c in enumerate(name)) * 7919 % 100000
