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

from . import photobash
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


def _facet_plane(vo: dict, seed_a: float, seed_b: float) -> np.ndarray:
    """Plano inclinado por celda (pendiente aleatoria): caras planas tipo low-poly."""
    a = np.mod(vo["id"] * seed_a, 1.0) - 0.5
    b = np.mod(vo["id2"] * seed_b + vo["id"] * 3.1, 1.0) - 0.5
    return vo["dx"] * a + vo["dy"] * b


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
    """Arena seca cartoon: crema cálida en pinceladas amplias de ondulación eólica, con
    grano y conchas sueltas como acento sutil (no pimienta fotográfica), sin grietas."""
    u, v, height, rip, zones, fine, grain = _sand_base(size, seed, 0.55)
    pebbles = scatter_dots(size, 17, seed + 6, radius=0.22, keep=0.05)
    shells = scatter_dots(size, 9, seed + 7, radius=0.17, keep=0.14)
    height = height + 0.14 * pebbles["mask"] + 0.1 * shells["mask"]
    t = 0.2 + 0.55 * rip * (0.4 + 0.6 * zones) + 0.25 * fine
    albedo = ramp(t, [(0.0, "#c79d62"), (0.35, "#ddb97f"), (0.65, "#ecd29f"), (1.0, "#f8e8c4")])
    # Grano suelto: un acento discreto, no una pimienta uniforme por todo el tile.
    albedo = mix_color(albedo, "#9d7a4f", smoothstep(2.6, 3.4, grain) * 0.28)
    albedo = mix_color(albedo, "#fff6e2", smoothstep(2.7, 3.6, -grain) * 0.3)
    peb_col = lerp(hex_rgb("#a58e74"), hex_rgb("#6f6660"), pebbles["id"])
    albedo = albedo + (peb_col - albedo) * pebbles["mask"][..., None]
    shell_col = lerp(hex_rgb("#f6dccf"), hex_rgb("#fdf3e6"), shells["id"])
    albedo = albedo + (shell_col - albedo) * shells["mask"][..., None]
    albedo = macro_variation(albedo, seed + 20, warm="#ffd9a0", cool="#e8e2cf", amount=0.18, value=0.07)
    rough = 0.9 - 0.05 * fine
    return Material(albedo, height, rough, depth=0.010, ao_strength=0.9)


def sand_wet(size: int, seed: int) -> Material:
    """Arena mojada de orilla: caramelo saturado (no gris), más oscura y fría que la seca;
    rizos lavados, marcas de resaca, agujeritos de cangrejo y alguna concha. El agua se
    insinúa como humedad difusa (sin manchas grandes que se repitan a la vista) y algún
    reflejo pintado pequeño, irregular y suave — nunca un charco-mancha uniforme."""
    u, v, height, rip, zones, fine, grain = _sand_base(size, seed, 0.3)
    height = blur(height, 0.0015)
    # Humedad: campo de frecuencia media (no 1-2 manchas grandes) que oscurece y enfría.
    puddle_n = unit(spectral_noise(size, seed + 8, 3, 12, 2.2), 2.0)
    soak = smoothstep(0.55, 0.78, puddle_n + 0.1 * (0.5 - height))
    height = height * (1.0 - 0.15 * smoothstep(0.72, 0.88, puddle_n))
    t = 0.2 + 0.5 * rip * (0.4 + 0.6 * zones) + 0.3 * fine
    albedo = ramp(t, [(0.0, "#78543a"), (0.4, "#8f6c48"), (0.75, "#a3835c"), (1.0, "#b4926c")])
    # Tinte frío uniforme (toda la textura, no solo la humedad) para que se note el
    # contraste con la arena seca cálida.
    albedo = mix_color(albedo, "#748a86", np.full_like(rip, 0.16))
    albedo = mix_color(albedo, "#5e4230", soak * 0.38)
    # Reflejos pintados: brillos pequeños, irregulares y dispersos (varios por tile, no una
    # mancha grande que se note al repetir 2x2), solo donde ya hay humedad.
    gu = u + 0.05 * spectral_noise(size, seed + 16, 2, 8, 2.0)
    gv = v + 0.05 * spectral_noise(size, seed + 17, 2, 8, 2.0)
    glints = scatter_dots(size, 15, seed + 13, radius=0.11, keep=0.4, vary=0.7, u=gu, v=gv)
    glint = glints["mask"] * smoothstep(0.4, 0.78, puddle_n)
    sky = ramp(unit(spectral_noise(size, seed + 18, 1, 4, 2.0)), [(0.0, "#a8c2ba"), (1.0, "#d8ece6")])
    albedo = albedo + (sky - albedo) * (glint * 0.6)[..., None]
    height = height - 0.02 * glint
    # Marcas de resaca: líneas finas y onduladas que dejó la espuma al retirarse.
    swash_warp = spectral_noise(size, seed + 9, 1, 5, 2.6)
    swash_phase = 3 * v + 0.45 * swash_warp + 0.06 * spectral_noise(size, seed + 11, 3, 12, 2.0)
    line = np.abs(np.mod(swash_phase, 1.0) - 0.5) * 2.0
    froth = (1.0 - smoothstep(0.0, 0.035, 1.0 - line)) * smoothstep(0.4, 0.7, unit(
        spectral_noise(size, seed + 10, 2, 10, 2.0), 2.0))
    froth = froth * (0.6 + 0.4 * unit(spectral_noise(size, seed + 12, 30, 300, 1.0)))
    # Por detrás de cada marca, una banda algo más clara donde la arena ya escurrió.
    drained = smoothstep(0.55, 0.95, np.mod(swash_phase, 1.0)) * (1.0 - soak)
    albedo = mix_color(albedo, "#dfbd88", drained * 0.25)
    albedo = mix_color(albedo, "#f4efe2", froth * 0.75)
    height = height + 0.05 * froth
    # Agujeritos de cangrejo/pulga de mar con su anillo de bolitas, y conchas sueltas.
    holes = scatter_dots(size, 11, seed + 14, radius=0.07, keep=0.3)
    ring = scatter_dots(size, 11, seed + 14, radius=0.2, keep=0.3)["mask"] - holes["mask"]
    shells = scatter_dots(size, 8, seed + 15, radius=0.11, keep=0.12, vary=0.3)
    height = height - 0.2 * holes["mask"] + 0.06 * ring + 0.1 * shells["mask"]
    albedo = mix_color(albedo, "#6b4a2b", holes["mask"] * 0.8)
    albedo = mix_color(albedo, "#c69a62", np.clip(ring, 0, 1) * 0.35)
    shell_col = lerp(hex_rgb("#eec3ad"), hex_rgb("#f6e2c8"), shells["id"])
    albedo = albedo + (shell_col - albedo) * shells["mask"][..., None]
    albedo = mix_color(albedo, "#7d5a3a", smoothstep(2.4, 3.3, grain) * 0.3)
    albedo = macro_variation(albedo, seed + 20, warm="#f0b878", cool="#c9c2ab", amount=0.14, value=0.06)
    rough = np.clip(0.6 - 0.2 * soak - 0.35 * glint + 0.2 * froth + 0.08 * (fine - 0.5)
                    + 0.3 * shells["mask"], 0.08, 1.0)
    return Material(albedo, height, rough, depth=0.006, ao_strength=0.8)


def grass(size: int, seed: int) -> Material:
    """Césped pintado visto desde arriba: pinceladas direccionales blandas de 2-4 cm que
    siguen una dirección de flujo coherente localmente (nunca celdas ni contornos cerrados),
    con poco contraste entre matas y una deriva de tono a gran escala entre amarillento
    cálido y azulado frío. Estilo Sea of Thieves: legible como césped a 3-5 m, no como
    escamas ni briznas realistas."""
    u, v = uv_grid(size)
    # Dirección de la brocha: campo de muy baja frecuencia, coherente en cada zona.
    flow = 0.6 + 1.3 * spectral_noise(size, seed + 1, 1, 3, 2.6)
    ca, sa = np.cos(flow), np.sin(flow)
    # Integración tipo LIC a lo largo del flujo: un grano fino se difumina siguiendo la
    # dirección local, lo que dibuja vetas de pincelada blandas sin geometría de celda.
    grain = spectral_noise(size, seed + 2, 10, 90, 1.4)
    step = 0.009  # ~1.3 cm en un tile de 1.5 m: pinceladas de 2-4 cm de largo total
    acc = grain.copy()
    wsum = 1.0
    for k in range(1, 7):
        du, dv = ca * step * k, sa * step * k
        acc = acc + sample(grain, np.mod(u + du, 1.0), np.mod(v + dv, 1.0))
        acc = acc + sample(grain, np.mod(u - du, 1.0), np.mod(v - dv, 1.0))
        wsum += 2.0
    brush = unit(acc / wsum, 1.4)
    # Bulto blando de mata: variación grande y suave, sin bordes duros.
    clump = unit(spectral_noise(size, seed + 3, 2, 7, 2.0), 1.8)
    height = np.clip(0.5 + 0.3 * (brush - 0.5) + 0.25 * (clump - 0.5), 0.0, 1.0)

    albedo = ramp(height, [(0.0, "#2c4c24"), (0.35, "#3f7530"), (0.65, "#5a9c43"), (1.0, "#82bd58")])
    # Deriva de tono a gran escala: zonas cálidas amarillentas y zonas frías azuladas, sin
    # bordes (mismo campo de baja frecuencia en las dos direcciones, no una celda).
    hue = unit(spectral_noise(size, seed + 4, 1, 3, 2.4), 2.2)
    warm = ramp(height, [(0.0, "#3a5a1c"), (0.5, "#8aa22e"), (1.0, "#d6d968")])
    cool = ramp(height, [(0.0, "#173c34"), (0.5, "#2f7a63"), (1.0, "#79c4a6")])
    tint = lerp(cool, warm, hue)
    albedo = albedo * 0.55 + tint * 0.45
    # Pinceladas: variación suave de brillo/sombra siguiendo la brocha, poco contraste.
    albedo = mix_color(albedo, "#cfe08a", smoothstep(0.66, 0.92, brush) * 0.16)
    albedo = mix_color(albedo, "#1f3a1a", smoothstep(0.34, 0.08, brush) * 0.14)
    # Florecillas: pocas, pequeñas, blancas o amarillas con centro amarillo.
    fl = scatter_dots(size, 11, seed + 5, radius=0.09, keep=0.1)
    petal = np.where(fl["id"] > 0.5, 1.0, 0.0)[..., None]
    fl_col = petal * hex_rgb("#fbf6e6") + (1 - petal) * hex_rgb("#ffd84a")
    core = (1.0 - smoothstep(0.02, 0.04, fl["f1"])) * (fl["mask"] > 0)
    albedo = albedo + (fl_col - albedo) * fl["mask"][..., None]
    albedo = mix_color(albedo, "#e8a326", core * 1.0)
    height = np.maximum(height, fl["mask"] * 0.98)
    albedo = macro_variation(albedo, seed + 20, warm="#c8d65a", cool="#4d9a6a", amount=0.2, value=0.08)
    height = blur(height, 0.0009)
    rough = 0.78 - 0.12 * height
    return Material(albedo, height, rough, depth=0.011, ao_strength=0.65)


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
    """Tierra de huerto labrada: camellones anchos y redondeados (hechos a mano, algo
    ondulados) entre surcos estrechos y húmedos; terrones redondos que ruedan por las laderas,
    migas finas, pocas pajas y guijarros y algún brote. Cacao cálido, nunca negro ni gris."""
    u, v = uv_grid(size)
    rows = 5
    # Hileras casi rectas: ondulación lenta y leve (labrado a mano), anchura distinta por hilera.
    warp = 0.022 * spectral_noise(size, seed + 1, 1, 3, 2.5) + 0.006 * spectral_noise(size, seed + 11, 4, 12, 2.0)
    rv = rows * (v + warp) + 0.31
    k = np.floor(rv).astype(np.int64) % rows
    f = rv - np.floor(rv)
    width = 0.72 + 0.12 * rng_table(seed + 12, rows)[k]
    # Perfil del camellón: meseta redondeada (coseno ensanchado), surco en V suave.
    d = np.abs(f - 0.5) / (0.5 * width)
    ridge = np.sqrt(np.clip(1.0 - d ** 2.2, 0.0, 1.0))
    furrow = 1.0 - smoothstep(0.0, 0.35, ridge)
    # Terrones: bultos grandes redondeados, más en las laderas del camellón.
    cl = voronoi(size, 22, 22, seed + 2, jitter=0.9)
    clod_r = 0.62 + 0.25 * cl["id"]
    clod = np.sqrt(np.clip(1.0 - (cl["f1"] / clod_r) ** 2, 0.0, 1.0))
    slope = smoothstep(0.1, 0.5, ridge) * (1.0 - smoothstep(0.85, 1.0, ridge))
    clod_on = (cl["id2"] < 0.35 + 0.45 * slope).astype(np.float64)
    clod = blur(clod * clod_on, 0.0025)
    crumbs = voronoi(size, 70, 70, seed + 3, jitter=0.9)
    crumb = np.sqrt(np.clip(1.0 - (crumbs["f1"] / 0.8) ** 2, 0.0, 1.0)) * (0.4 + 0.6 * crumbs["id"])
    fine = unit(spectral_noise(size, seed + 13, 20, 120, 1.4))
    height = 0.62 * ridge + 0.2 * clod + 0.1 * crumb + 0.08 * fine
    pebbles = scatter_dots(size, 11, seed + 4, radius=0.22, keep=0.12, vary=0.4)
    peb_dome = np.sqrt(np.clip(1.0 - (pebbles["f1"] / (0.22 * (1 + 0.4 * (2 * pebbles["id"] - 1)))) ** 2, 0, 1))
    height = np.maximum(height, pebbles["mask"] * (0.45 + 0.35 * peb_dome + 0.2 * ridge))
    # Color: surco húmedo cacao oscuro -> ladera chocolate -> cresta seca canela.
    t = 0.18 + 0.62 * ridge + 0.14 * clod + 0.06 * (crumbs["id"] - 0.5) + 0.05 * (fine - 0.5)
    albedo = ramp(t, [(0.0, "#46291c"), (0.3, "#5c3620"), (0.55, "#774628"),
                      (0.8, "#935d37"), (1.0, "#aa7648")])
    # Terrón a terrón cambia un poco (más rojizo / más ocre): cartoon con vida, no barro plano.
    clod_tint = lerp(hex_rgb("#8e4f30"), hex_rgb("#9a6a3a"), cl["id"])
    albedo = albedo + (clod_tint - albedo) * (clod * 0.25)[..., None]
    # Surco con el brillo de la humedad (algo más saturado, no gris).
    albedo = mix_color(albedo, "#55301c", furrow * 0.35)
    peb_col = lerp(hex_rgb("#7d7064"), hex_rgb("#a39179"), pebbles["id"]) * (0.78 + 0.25 * peb_dome)[..., None]
    albedo = albedo + (peb_col - albedo) * pebbles["mask"][..., None]
    # Pajas cortas y algo gruesas, pocas, sobre todo caídas en los surcos.
    straw = strokes(size, 16, seed + 5, length=0.5, width=0.05, keep=0.22, angle_bias=0.0, spread=1.6)
    straw_col = lerp(hex_rgb("#c79a4e"), hex_rgb("#e0bd6e"), fine)
    albedo = albedo + (straw_col - albedo) * (straw * 0.9)[..., None]
    height = np.maximum(height, straw * (0.3 + 0.45 * ridge))
    # Algún brote de mala hierba en lo alto del camellón (dos hojitas): da vida sin ser cultivo.
    sp = voronoi(size, 7, 7, seed + 6, jitter=0.7)
    ang = sp["id"] * np.pi
    ca, sa = np.cos(ang), np.sin(ang)
    a = sp["dx"] * ca + sp["dy"] * sa
    b = -sp["dx"] * sa + sp["dy"] * ca
    leaf = sum(1.0 - smoothstep(0.7, 1.0, np.hypot((a - s_ * 0.11) / 0.12, b / 0.055)) for s_ in (-1, 1))
    sprout = np.clip(leaf, 0, 1) * (sp["id2"] < 0.3) * smoothstep(0.6, 0.9, sample(ridge, u, v))
    albedo = mix_color(albedo, "#79b943", sprout * 0.95)
    height = np.maximum(height, sprout * 0.85)
    albedo = macro_variation(albedo, seed + 20, warm="#b0683a", cool="#6e5646", amount=0.16, value=0.1)
    rough = 0.93 - 0.18 * furrow - 0.1 * pebbles["mask"] - 0.25 * sprout
    return Material(albedo, height, rough, depth=0.02, ao_strength=1.0)


def leaves(size: int, n: int, seed: int, length: float, width: float, keep: float) -> dict:
    """Hojas caídas vistas desde arriba: una por celda, orientación libre, silueta de lanza
    (ancha en el centro, punta afilada). t = 0 peciolo, 1 punta; across = 0 nervio, 1 borde."""
    vo = voronoi(size, n, n, seed, jitter=0.9)
    ang = vo["id"] * 2.0 * np.pi
    ca, sa = np.cos(ang), np.sin(ang)
    a = vo["dx"] * ca + vo["dy"] * sa
    b = -vo["dx"] * sa + vo["dy"] * ca
    t = np.clip(a / length + 0.5, 0.0, 1.0)
    half = width * np.sin(np.pi * t) ** 0.75 * (1.0 - 0.35 * t) + 1e-4
    inside = (np.abs(a) < length * 0.5) & (np.abs(b) < half) & (vo["id2"] < keep)
    across = np.clip(np.abs(b) / half, 0, 1)
    return {"mask": inside.astype(np.float64), "t": t, "across": across, "id": vo["id2"] / max(keep, 1e-6),
            "side": np.sign(b)}


def forest_floor(size: int, seed: int) -> Material:
    """Hojarasca del suelo de selva: capas de hojas caídas (ocre, teja, marrón y alguna aún
    verde) con nervio central y bordes algo curvados, ramitas y tierra oscura en los huecos."""
    soil_n = unit(spectral_noise(size, seed + 1, 3, 60, 1.8))
    height = 0.1 * soil_n
    col_t = np.zeros_like(height)
    hue = np.zeros_like(height)
    rib = np.zeros_like(height)
    layer_of = np.full(height.shape, -1.0)
    layers = 5
    for j in range(layers):
        lf = leaves(size, 5 + j, seed + 10 + j, length=1.05, width=0.34, keep=0.85)
        curl = 0.06 * lf["across"] ** 2 - 0.03 * (lf["t"] - 0.5) ** 2
        h = 0.18 + 0.12 * j + curl
        take = (lf["mask"] > 0) & (h > height)
        height = np.where(take, h, height)
        hue = np.where(take, lf["id"], hue)
        col_t = np.where(take, lf["t"], col_t)
        rib = np.where(take, 1.0 - smoothstep(0.0, 0.12, lf["across"]), rib)
        layer_of = np.where(take, j, layer_of)
    covered = layer_of >= 0
    # Paleta de hojarasca: la mayoría ocre/teja/marrón cálido, alguna verde reciente.
    dry = ramp(hue, [(0.0, "#7a4a28"), (0.3, "#a3612f"), (0.55, "#c98a3c"), (0.75, "#d9a94e"),
                     (0.9, "#8a7a3a"), (1.0, "#6a8a34")])
    # Las capas de abajo quedan más oscuras y húmedas: da profundidad sin negro.
    depth_k = np.where(covered, (layer_of + 1) / layers, 0.0)
    leaf_c = dry * (0.62 + 0.38 * depth_k)[..., None]
    leaf_c = leaf_c * (1.0 + 0.1 * (col_t - 0.5))[..., None]
    leaf_c = mix_color(leaf_c, "#e8c77a", rib * 0.4 * depth_k)
    soil = ramp(soil_n, [(0.0, "#3a2819"), (0.6, "#523a24"), (1.0, "#6a4c2e")])
    albedo = np.where(covered[..., None], leaf_c, soil)
    # Ramitas finas por encima de algunas hojas.
    tw = strokes(size, 14, seed + 5, length=0.9, width=0.03, keep=0.22)
    tw_c = ramp(soil_n, [(0.0, "#4a3322"), (1.0, "#6e4d30")])
    albedo = albedo + (tw_c - albedo) * (tw * 0.95)[..., None]
    height = np.maximum(height, tw * (0.2 + 0.12 * layers))
    # Brotes y musgo en los huecos: pinceladas verdes vivas pequeñas.
    height = blur(height, 0.0008)
    height = height / (0.18 + 0.12 * layers + 0.1)
    albedo = macro_variation(albedo, seed + 20, warm="#c8864a", cool="#6a7a48", amount=0.22, value=0.08)
    rough = 0.72 + 0.15 * (1.0 - depth_k) - 0.1 * rib
    return Material(albedo, height, rough, depth=0.008, ao_strength=1.1)


def ash(size: int, seed: int) -> Material:
    """Ceniza del Humo: mantos suaves modelados por el viento con rizos finos, grumos
    redondeados donde la lluvia apelmazó la ceniza (nunca una red de grietas ni losetas),
    pómez con volumen y carbones angulosos. Paleta gris lavanda cálida y clara, no sucia."""
    u, v = uv_grid(size)
    drift = unit(spectral_noise(size, seed + 1, 1, 6, 2.8), 2.2)
    warp = spectral_noise(size, seed + 2, 1, 4, 3.0)
    phase = 9 * v - 2 * u + 0.3 * warp
    s = np.mod(phase, 1.0)
    rip = smoothstep(0.0, 1.0, np.where(s < 0.7, s / 0.7, (1.0 - s) / 0.3))
    rip = rip * smoothstep(0.3, 0.75, drift)
    powder = unit(spectral_noise(size, seed + 3, 30, 300, 1.2))
    # Grumos: bultos redondeados y sueltos (disco suave, sin bordes ni juntas marcadas),
    # muy deformados para que no se lean como una rejilla de placas.
    wu = u + 0.06 * spectral_noise(size, seed + 8, 2, 8, 2.0)
    wv = v + 0.06 * spectral_noise(size, seed + 9, 2, 8, 2.0)
    crumb = scatter_dots(size, 9, seed + 10, radius=0.4, jitter=1.0, keep=0.8, vary=0.4, u=wu, v=wv)
    crumb_r = 0.4 * (1.0 + 0.4 * (2.0 * crumb["id"] - 1.0))
    crumb_dome = np.sqrt(np.clip(1.0 - (crumb["f1"] / crumb_r) ** 2, 0.0, 1.0)) * crumb["alive"]
    crumb_dome = blur(crumb_dome, 0.008)
    crust_h = 0.16 * crumb_dome
    height = 0.42 * drift + 0.14 * rip + 0.04 * powder + crust_h
    height = np.clip(height + 0.2, 0, 1)
    albedo = ramp(0.3 + 0.35 * drift + 0.26 * rip + 0.1 * (powder - 0.5),
                  [(0.0, "#8a8490"), (0.35, "#a09aa0"), (0.65, "#b8b1ae"), (1.0, "#d6cec4")])
    # Los grumos aclaran suave hacia la cresta; nunca una línea oscura en la junta.
    crumb_c = ramp(crumb["id"], [(0.0, "#b7afa6"), (1.0, "#c7bfb4")])
    albedo = albedo + (crumb_c - albedo) * (crumb_dome * 0.55)[..., None]
    # Carbones: trozos angulosos (celda de Voronoi recortada), con canto claro arriba.
    ch = voronoi(size, 30, 30, seed + 4, jitter=0.9)
    ang = ch["id"] * np.pi
    ca, sa = np.cos(ang), np.sin(ang)
    ra = np.abs(ch["dx"] * ca + ch["dy"] * sa) / (0.28 + 0.12 * ch["id"])
    rb = np.abs(-ch["dx"] * sa + ch["dy"] * ca) / (0.16 + 0.08 * np.mod(ch["id2"] * 10.0, 1.0))
    char = (np.maximum(ra, rb) + 0.35 * np.minimum(ra, rb) < 1.0) * (ch["id2"] < 0.03)
    char = blur(char.astype(np.float64), 0.0008)
    char_c = ramp(ch["id"], [(0.0, "#2c2830"), (1.0, "#3f3740")])
    albedo = albedo + (char_c - albedo) * (char * 0.9)[..., None]
    # Pómez: guijarros claros con volumen (sombra propia hacia abajo-derecha) y poros.
    pum = scatter_dots(size, 13, seed + 5, radius=0.26, keep=0.16, vary=0.45)
    rad = 0.26 * (1.0 + 0.45 * (2.0 * pum["id"] - 1.0))
    dome = np.sqrt(np.clip(1.0 - (pum["f1"] / rad) ** 2, 0, 1)) * pum["alive"]
    # Luz desde arriba-izquierda: el lado del píxel con dx, dy > 0 (punto abajo-derecha) brilla.
    shade = np.clip(0.88 + 0.35 * (pum["dx"] + pum["dy"]) / rad, 0.55, 1.12)
    pum_c = ramp(pum["id"], [(0.0, "#cdbfa9"), (1.0, "#e4d8c3")]) * shade[..., None]
    pores = scatter_dots(size, 160, seed + 11, radius=0.25, keep=0.5)["mask"]
    pum_c = mix_color(pum_c, "#a39381", pores * 0.5)
    albedo = albedo + (pum_c - albedo) * (pum["mask"] * 0.95)[..., None]
    height = np.maximum(height, pum["mask"] * (0.55 + 0.4 * dome) - pores * 0.04 * pum["mask"])
    height = np.maximum(height, char * (0.5 + 0.2 * drift))
    # Brasas casi apagadas: muy pocas, un guiño de color cálido con halo.
    ember = scatter_dots(size, 17, seed + 6, radius=0.12, keep=0.035)
    halo = scatter_dots(size, 17, seed + 6, radius=0.4, keep=0.035)["mask"]
    albedo = mix_color(albedo, "#7a4a44", halo * 0.35)
    albedo = mix_color(albedo, "#ff7a32", ember["mask"] * 0.9)
    albedo = macro_variation(albedo, seed + 20, warm="#c09c86", cool="#98a0b4", amount=0.16, value=0.07)
    rough = 0.94 - 0.1 * char - 0.06 * crumb_dome
    return Material(albedo, height, rough, depth=0.016, ao_strength=1.0)


# ---------------------------------------------------------------------------
# Rocas — fotobasheadas (ver texgen/photobash.py): a la escala de tile de Explored
# (3 m), cualquier rejilla procedural —por irregular que sea— delataba una rejilla.
# ---------------------------------------------------------------------------

def volcanic_rock(size: int, seed: int) -> dict[str, np.ndarray]:
    """Basalto gris violáceo: fotografía CC0 de Poly Haven ('Dark Rock', Amal Kumar)
    estilizada a óleo cartoon. Ver `texgen/photobash.py` y la atribución en
    `docs/art/texturas.md`. `seed` no se usa (la fuente no es aleatoria)."""
    return photobash.photobash_rock("dark_rock", size, photobash.VOLCANIC_STYLE, seed)


def limestone(size: int, seed: int) -> dict[str, np.ndarray]:
    """Caliza gris kárstica con vetas oscuras y toques ocres (referencia: El Nido / Ha
    Long, no crema): fotografía CC0 de Poly Haven ('Marble Cliff 04', Amal Kumar)
    estilizada a óleo cartoon. Ver `texgen/photobash.py` y la atribución en
    `docs/art/texturas.md`. `seed` no se usa (la fuente no es aleatoria)."""
    return photobash.photobash_rock("marble_cliff_04", size, photobash.LIMESTONE_STYLE, seed)


# ---------------------------------------------------------------------------
# Construcción
# ---------------------------------------------------------------------------

def _thatch_strands(u, rv, rows, n, seed, t_off, shift, width_k, lift=0.0):
    """Una capa de hebras de la hilera `floor(rv) - t_off` (t = distancia bajo su atadura, en
    hileras; `lift` lo sube sin cambiar de hilera): devuelve (dentro, t, largo, tabla, across)."""
    r_idx = (np.floor(rv).astype(np.int64) - t_off) % rows
    t = rv - np.floor(rv) + t_off - lift
    tab = rng_table(seed, rows, n, 6)
    row_off = rng_table(seed + 1, rows)[r_idx]
    lean = (rng_table(seed + 2, rows)[r_idx] - 0.5) * 0.9
    # Hebra: se inclina un poco con la hilera y se curva al colgar (más cuanto más baja).
    cu = u * n + row_off * n + shift + lean * t
    col0 = np.floor(cu).astype(np.int64) % n
    r = tab[r_idx, col0]
    cu = cu + (r[..., 4] - 0.5) * 0.9 * t * t
    col = np.floor(cu).astype(np.int64) % n
    r = tab[r_idx, col]
    x = cu - np.floor(cu)
    length = 1.38 + 0.42 * r[..., 0]
    s = np.clip(t / length, 0.0, 1.0)
    width = width_k * (0.8 + 0.35 * r[..., 1]) * (1.0 - 0.3 * s)
    # Punta afilada en el último 25 % del largo, a veces rasgada en dos.
    width = width * np.sqrt(np.clip((length - t) / 0.3, 0.0, 1.0))
    off = x - 0.5 - (r[..., 2] - 0.5) * 0.2
    split = (r[..., 5] < 0.3) & (t > length - 0.28)
    off = np.where(split, np.abs(off) - width * 0.45, off)
    width = np.where(split, width * 0.5, width)
    across = np.abs(off) / np.maximum(width, 1e-4)
    inside = (across < 1.0) & (t < length) & (t >= 0.0)
    return inside, t, length, r, across


def palm_thatch(size: int, seed: int) -> Material:
    """Techo de hoja de palma: hileras solapadas de hebras largas y estrechas que cuelgan
    (cada hilera tapa la parte alta de la de abajo), puntas desiguales y a veces rasgadas,
    tono por hebra (paja dorada, alguna aún verde, alguna tostada); v = pendiente abajo."""
    u, v = uv_grid(size)
    rows, n = 6, 34
    # Las hileras no son reglas: el borde de cada una ondula (atado a mano) a lo largo de u.
    sag = spectral_noise(size, seed + 4, 1, 3, 3.0, stretch=(1.0, 4.0))
    rv = v * rows + 0.37 + 0.09 * sag
    fibers = unit(spectral_noise(size, seed + 3, 10, 360, 1.1, stretch=(1.0, 9.0)))
    fine = unit(spectral_noise(size, seed + 5, 40, 480, 0.8, stretch=(1.0, 14.0)))

    # Capas de delante a atrás: hilera de arriba (cuelga sobre esta), hilera propia
    # (dos capas desfasadas media hebra para que no queden huecos).
    layers = [
        _thatch_strands(u, rv, rows, n, seed + 10, 1, 0.0, 0.40),
        _thatch_strands(u, rv, rows, n, seed + 11, 1, 0.5, 0.40),
        _thatch_strands(u, rv, rows, n, seed + 10, 0, 0.0, 0.40),
        _thatch_strands(u, rv, rows, n, seed + 11, 0, 0.5, 0.40),
    ]
    height = 0.05 + 0.12 * (rv - np.floor(rv))
    albedo = ramp(rv - np.floor(rv), [(0.0, "#2e2213"), (1.0, "#4d3a1f")])
    covered = np.zeros((size, size), dtype=bool)
    own = np.zeros((size, size), dtype=bool)
    rough = np.full((size, size), 0.95)
    for li, (inside, t, length, r, across) in enumerate(layers):
        vis = inside & ~covered
        s = np.clip(t / length, 0.0, 1.0)
        prof = np.sqrt(np.clip(1.0 - across ** 2, 0.0, 1.0))
        midrib = np.exp(-(across / 0.18) ** 2)
        back = li % 2  # capa trasera de cada hilera, algo más hundida y oscura
        h = 0.2 + 0.55 * (t / 2.0) + 0.1 * prof + 0.04 * midrib + 0.04 * fibers - 0.05 * back
        h = h + 0.06 * smoothstep(length - 0.35, length, t)  # la punta monta sobre la hilera de abajo
        tone = r[..., 3]
        k = 0.2 + 0.35 * s + 0.3 * fibers + 0.15 * fine + 0.15 * (r[..., 1] - 0.5)
        straw = ramp(k, [(0.0, "#9a7433"), (0.35, "#c49a45"), (0.7, "#e0bb62"), (1.0, "#f0d893")])
        gold = ramp(k, [(0.0, "#a0702a"), (0.4, "#cf9738"), (0.8, "#e8b454"), (1.0, "#f3cf7c")])
        green = ramp(k, [(0.0, "#6f7c2e"), (0.5, "#9fac45"), (1.0, "#c9cd6e")])
        brown = ramp(k, [(0.0, "#6c4a26"), (0.5, "#936a39"), (1.0, "#b98e57")])
        c = np.where((tone < 0.45)[..., None], straw, gold)
        c = np.where((tone > 0.9)[..., None], green, c)
        c = np.where((tone < 0.1)[..., None], brown, c)
        c = mix_color(c, "#f6e6ae", midrib * 0.22)
        # Borde de la hebra algo más oscuro (se lee cada hebra) y punta seca más gris-tostada.
        c = c * (0.8 + 0.2 * prof)[..., None]
        c = mix_color(c, "#a58a5c", smoothstep(length - 0.4, length, t) * 0.35)
        c = c * (0.93 - 0.1 * back)
        albedo = np.where(vis[..., None], c, albedo)
        height = np.where(vis, h, height)
        rough = np.where(vis, 0.62 + 0.12 * (1.0 - fibers) - 0.06 * midrib, rough)
        if li >= 2:
            own |= vis
        covered |= inside
    # Lo que asoma de cada hilera queda bajo las puntas de la de arriba: sombra proyectada
    # (las puntas de arriba, un poco más arriba, tapan la luz) y penumbra junto a la atadura.
    cast = np.zeros((size, size))
    for lift, w in ((0.05, 0.5), (0.12, 0.3), (0.22, 0.2)):
        for sd, sh in ((seed + 10, 0.0), (seed + 11, 0.5)):
            cast = cast + w * 0.5 * _thatch_strands(u, rv, rows, n, sd, 1, sh, 0.40, lift)[0]
    # Sin desenfoque: difuminaría la sombra hacia la hilera de arriba y marcaría una raya.
    t0 = rv - np.floor(rv)
    lit = (0.7 + 0.3 * smoothstep(0.0, 0.8, t0)) * (1.0 - 0.3 * np.clip(cast, 0.0, 1.0))
    albedo = albedo * np.where(own, lit, 1.0)[..., None]
    albedo = macro_variation(albedo, seed + 20, warm="#e8b85c", cool="#a2a878", amount=0.18, value=0.08)
    height = blur(height, 0.0007)
    return Material(albedo, height, rough, depth=0.024, ao_strength=1.2)


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
    """Tablones horizontales largos (1 o 2 juntas por hilera, escalonadas); u = a lo largo.
    Veta de corte plano en arcos («catedral») que rodea los nudos, o recta en las tablas de
    corte radial; tono por tabla (miel, caramelo, rojizo, alguna gastada por el sol), cantos
    redondeados que atrapan la luz y juntas marrón oscuro, nunca negras."""
    u, v = uv_grid(size)
    rows = 6
    rv = v * rows + 0.29
    row = np.floor(rv).astype(np.int64) % rows
    y = rv - np.floor(rv)
    tab = rng_table(seed, rows, 8)
    s0 = tab[row, 0]
    two = tab[row, 2] < 0.6
    s1 = np.where(two, np.mod(s0 + 0.35 + 0.3 * tab[row, 1], 1.0), s0)
    lu = np.mod(u - s0, 1.0)
    seg = np.where(two, np.mod(s1 - s0, 1.0), 1.0)
    in_second = two & (lu >= seg)
    start = np.where(in_second, seg, 0.0)
    length = np.where(in_second, 1.0 - seg, seg)
    ul = (lu - start) / length  # 0..1 a lo largo de la tabla
    board = row * 2 + in_second.astype(np.int64)
    br = rng_table(seed + 1, rows * 2, 8)[board]

    def wrap_dist(a, b):
        d = np.abs(a - b)
        return np.minimum(d, 1.0 - d)

    seam_d = np.minimum(wrap_dist(u, s0), np.where(two, wrap_dist(u, s1), 1.0))
    rnd = smoothstep(0.0, 0.12, y) * smoothstep(0.0, 0.12, 1.0 - y) * smoothstep(0.0, 0.01, seam_d)
    bevel = np.sqrt(rnd)
    knots = scatter_dots(size, 6, seed + 3, radius=0.3, keep=0.4)
    kd = knots["f1"]
    knot_pull = np.exp(-(kd / 0.16) ** 2) * knots["alive"]
    long_n = spectral_noise(size, seed + 2, 1, 40, 1.8, stretch=(8.0, 1.0))
    # Arcos de catedral: parábola centrada en la tabla; plana en las de corte radial.
    world_len = length * rows  # largo de la tabla en anchos de tabla
    bend = np.where(br[..., 5] < 0.3, 0.0, 0.15 + 0.35 * br[..., 6])
    xc = (ul - (0.3 + 0.4 * br[..., 7])) * world_len
    t = (y - 0.2 - 0.6 * br[..., 0]) + bend * 0.12 * xc ** 2 / (1.0 + 0.05 * xc ** 2)
    grain_phase = t * (3.5 + 2.5 * br[..., 1]) + 0.25 * long_n + 1.2 * knot_pull + br[..., 2] * 5.0
    rings = np.mod(grain_phase, 1.0)
    late = smoothstep(0.62, 0.8, rings) * smoothstep(1.0, 0.86, rings)
    fibre = unit(spectral_noise(size, seed + 4, 10, 400, 1.0, stretch=(14.0, 1.0)))
    height = 0.8 * bevel + 0.05 * (1.0 - late) + 0.04 * fibre + 0.06 * br[..., 3] * bevel
    # Tono por tabla: miel, caramelo, rojizo; ~15 % gastadas por el sol (más gris plata).
    tone = br[..., 4]
    honey = ramp(0.2 + 0.5 * fibre + 0.3 * (1.0 - late), [(0.0, "#9a6536"), (0.5, "#c08a4e"), (1.0, "#dcad6c")])
    caramel = ramp(0.2 + 0.5 * fibre + 0.3 * (1.0 - late), [(0.0, "#80492a"), (0.5, "#a4653a"), (1.0, "#c4884f")])
    red = ramp(0.2 + 0.5 * fibre + 0.3 * (1.0 - late), [(0.0, "#7a3f2a"), (0.5, "#9c5638"), (1.0, "#b8744c")])
    albedo = lerp(honey, caramel, smoothstep(0.3, 0.45, tone))
    albedo = lerp(albedo, red, smoothstep(0.7, 0.85, tone))
    weathered = smoothstep(0.85, 0.9, br[..., 3])
    albedo = lerp(albedo, albedo.mean(axis=-1, keepdims=True) * np.array([1.05, 1.0, 0.95]) + 0.08,
                  weathered * 0.55)
    albedo = mix_color(albedo, "#5e3620", late * 0.45)
    ring_d = kd / 0.07
    knot_core = (1.0 - smoothstep(0.35, 0.6, ring_d)) * knots["alive"]
    knot_ring = np.exp(-((ring_d - 0.75) / 0.12) ** 2) * knots["alive"]
    albedo = mix_color(albedo, "#5a321b", knot_core * 0.85)
    albedo = mix_color(albedo, "#6e4125", knot_ring * 0.5)
    height = height - 0.05 * knot_core
    # Clavos a ambos lados de cada junta.
    nails = np.zeros_like(u)
    for s, on in ((s0, np.ones_like(two)), (s1, two)):
        for side in (-1.0, 1.0):
            nu = s + side * 0.02
            for ny in (0.3, 0.7):
                d = np.hypot(wrap_dist(u, nu) * rows, (y - ny))
                nails = np.maximum(nails, (1.0 - smoothstep(0.022, 0.034, d)) * on)
    albedo = mix_color(albedo, "#4a4447", nails * 0.9)
    height = height - 0.06 * nails
    albedo = mix_color(albedo, "#3a2414", (1.0 - rnd) ** 1.5 * 0.8)
    edge = np.clip(cavity(height, 0.004) * 6.0, 0, 1)
    albedo = albedo * (1.0 + 0.22 * edge)[..., None]
    albedo = macro_variation(albedo, seed + 20, warm="#d08a4c", cool="#8a7560", amount=0.14, value=0.07)
    rough = 0.66 + 0.1 * late + 0.2 * (1.0 - rnd) + 0.12 * weathered - 0.35 * nails
    return Material(albedo, np.clip(height, 0, 1), rough, depth=0.014, ao_strength=1.0)


def stone_wall(size: int, seed: int) -> Material:
    """Muro de piedra seca polinesio: basalto facetado encajado, algún bloque de coral y
    musgo que crece en las juntas (más por abajo de cada piedra, donde queda la humedad)."""
    u, v = uv_grid(size)
    wu = u + 0.02 * spectral_noise(size, seed + 1, 2, 8, 2.2)
    wv = v + 0.02 * spectral_noise(size, seed + 2, 2, 8, 2.2)
    vo = voronoi(size, 8, 6, seed, jitter=0.8, u=wu, v=wv)
    chips = voronoi(size, 26, 20, seed + 11, jitter=0.9, u=wu, v=wv)
    bevel = smoothstep(0.0, 0.22, vo["edge"]) ** 0.55
    surf = unit(spectral_noise(size, seed + 3, 8, 160, 1.9))
    facets = 0.5 * _facet_plane(vo, 17.3, 11.7) + 0.35 * _facet_plane(chips, 13.1, 29.7) / 3.0
    height = bevel * (0.62 + 0.18 * vo["id"] + facets + 0.06 * surf)
    height = np.clip(height, 0, 1)
    coral = (vo["id2"] > 0.92)[..., None]
    basalt = lerp(ramp(vo["id"], [(0.0, "#474a55"), (0.5, "#5a5658"), (1.0, "#6e6158")]),
                  hex_rgb("#8a8580"), 0.25 * surf + 0.2 * np.clip(facets * 3.0, 0, 1))
    coral_c = ramp(surf, [(0.0, "#8f8272"), (0.6, "#a8997f"), (1.0, "#bcae94")])
    pores = scatter_dots(size, 90, seed + 12, radius=0.22, keep=0.6)["mask"]
    coral_c = mix_color(coral_c, "#7a6b5c", pores * 0.45)
    albedo = np.where(coral, coral_c, basalt)
    height = height - pores * 0.04 * coral[..., 0]
    edge = np.clip(cavity(height, 0.006) * 7.0, -1, 1)
    albedo = albedo * (1.0 + 0.35 * np.clip(edge, 0, 1) - 0.15 * np.clip(-edge, 0, 1))[..., None]
    joint = 1.0 - smoothstep(0.02, 0.14, vo["edge"])
    albedo = mix_color(albedo, "#2b231d", joint * 0.9)
    # El musgo prefiere la mitad baja de cada piedra (dy < 0: el punto queda por encima).
    below = smoothstep(-0.1, 0.35, -vo["dy"])
    moss_n = unit(spectral_noise(size, seed + 4, 1, 12, 2.0), 2.0)
    moss_fine = unit(spectral_noise(size, seed + 5, 25, 250, 1.2))
    grow = joint * 0.9 + below * (1.0 - bevel) * 0.6 + (moss_n - 0.5) * 1.0 + (moss_fine - 0.5) * 0.35
    moss = smoothstep(0.45, 0.7, grow) * smoothstep(0.35, 0.55, moss_n)
    moss_col = ramp(0.5 * moss_n + 0.5 * moss_fine, [(0.0, "#2f5a22"), (0.5, "#4d8a2e"), (0.8, "#72a83f"), (1.0, "#a3c85c")])
    albedo = albedo + (moss_col - albedo) * (moss * 0.92)[..., None]
    height = np.maximum(height, moss * (0.25 + 0.25 * moss_fine))
    albedo = macro_variation(albedo, seed + 20, warm="#8e705a", cool="#5a6878", amount=0.2, value=0.1)
    rough = 0.82 - 0.12 * np.clip(edge, 0, 1) + 0.1 * moss - 0.05 * coral[..., 0]
    return Material(albedo, height, rough, depth=0.036, ao_strength=1.3)


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
    """Lona de vela/toldo: tafetán legible (urdimbre algo más clara que la trama), hilos con
    grosor irregular, una costura de paño (solape con doble pespunte) a lo largo de u,
    descolorido suave por el sol y motas de sal pequeñas (nada de manchas que se repitan)."""
    weave, warp_top, u, v = _plain_weave(size, seed, 44, amp=0.32)
    fuzz = unit(spectral_noise(size, seed + 5, 80, 600, 0.8))
    # Tono por hilo: cada hilo de urdimbre/trama tiene su matiz (lo que da vida al tejido).
    ix = np.floor(u * 44 + 0.5).astype(np.int64) % 44
    iy = np.floor(v * 44 + 0.5).astype(np.int64) % 44
    thread = np.where(warp_top, rng_table(seed + 8, 44)[ix], rng_table(seed + 9, 44)[iy])
    # Costura de paño: el paño de arriba solapa al de abajo (banda algo elevada con canto
    # redondeado que hace sombra) y dos filas de pespunte. Fase fraccionaria: no cae en el borde.
    sv = v + 0.004 * spectral_noise(size, seed + 12, 1, 4, 2.0)
    d = np.abs(np.mod(sv - 0.37 + 0.5, 1.0) - 0.5)
    band_w = 0.034
    band = 1.0 - smoothstep(band_w - 0.006, band_w, d)
    lip = np.exp(-((d - band_w + 0.004) / 0.004) ** 2)
    stitch_rows = np.exp(-((d - 0.022) / 0.0035) ** 2)
    su = np.mod(u * 40 + 0.25 * (np.mod(sv - 0.37, 1.0) > 0.5), 1.0)
    dash = smoothstep(0.1, 0.22, su) * (1.0 - smoothstep(0.62, 0.74, su))
    stitch = stitch_rows * dash
    height = 0.62 * weave + 0.1 * fuzz + 0.14 * band + 0.08 * lip + 0.18 * stitch
    t = 0.5 * weave + 0.15 * fuzz + 0.2 * warp_top + 0.15 * thread
    albedo = ramp(t, [(0.0, "#b89c6e"), (0.35, "#d8c193"), (0.7, "#ecdbb1"), (1.0, "#f7ecca")])
    albedo = mix_color(albedo, "#937a52", smoothstep(0.35, 0.0, weave) * 0.45)
    # Sombra suave bajo el solape y pespunte en hilo encerado algo más oscuro.
    shade = np.exp(-((d - band_w - 0.006) / 0.006) ** 2)
    albedo = albedo * (1.0 - 0.16 * shade)[..., None]
    albedo = mix_color(albedo, "#9c7641", stitch * 0.9)
    # Descolorido por el sol: bandas lentas y alargadas a lo largo de u, poco contraste.
    bleach = unit(spectral_noise(size, seed + 7, 1, 5, 2.2, stretch=(1.0, 2.5)), 2.0)
    albedo = mix_color(albedo, "#f4efe0", smoothstep(0.45, 0.9, bleach) * 0.22)
    # Motas de sal: puntitos pálidos dispersos, pequeños.
    salt = scatter_dots(size, 18, seed + 6, radius=0.18, keep=0.3, vary=0.5)
    albedo = mix_color(albedo, "#fbf7ec", salt["mask"] * 0.35)
    albedo = macro_variation(albedo, seed + 20, warm="#f0cf90", cool="#d9d6c4", amount=0.12, value=0.05)
    rough = 0.9 + 0.05 * fuzz - 0.1 * stitch
    return Material(albedo, height, rough, depth=0.006, ao_strength=0.8)


def rope(size: int, seed: int) -> Material:
    """Cuerda de 3 cabos; u = alrededor de la cuerda, v = a lo largo (UV de cilindro).
    Cada cabo es un cilindro que asoma en diagonal; sus hilos giran al revés (torsión
    contraria), dibujando arcos en «S» que son lo que hace que se lea como cuerda."""
    u, v = uv_grid(size)
    strands = 3
    phase = strands * u + 4 * v + 0.13
    x = phase - np.floor(phase)
    c = 2.0 * x - 1.0
    prof = np.sqrt(np.clip(1.0 - c * c, 0, 1))
    # Hilos: fase que cruza el cabo (frecuencias enteras) + curvatura por la sección redonda.
    yarn_phase = 10 * u - 18 * v + 0.35 * c * c
    yarn_x = yarn_phase - np.floor(yarn_phase)
    yarn = np.sqrt(np.clip(1.0 - (2.0 * yarn_x - 1.0) ** 2, 0, 1))
    fibre = unit(spectral_noise(size, seed + 1, 20, 400, 1.2, stretch=(1.0, 3.0), angle=-1.0))
    fuzz = unit(spectral_noise(size, seed + 3, 60, 500, 0.9))
    height = 0.7 * prof ** 0.8 + 0.2 * yarn * prof + 0.06 * fibre + 0.04 * fuzz
    idx = np.floor(phase).astype(np.int64) % strands
    tone = rng_table(seed + 2, strands)[idx]
    albedo = ramp(0.35 * prof + 0.3 * yarn * prof + 0.2 * fibre + 0.15 * tone,
                  [(0.0, "#6a4b28"), (0.35, "#a27b44"), (0.7, "#c9a462"), (1.0, "#e6cc8e")])
    groove = smoothstep(0.45, 0.0, prof)
    albedo = mix_color(albedo, "#3d2a14", groove * 0.85)
    albedo = mix_color(albedo, "#5d4221", (1.0 - yarn) * prof * 0.35)
    albedo = mix_color(albedo, "#f1dfae", smoothstep(0.75, 1.0, fuzz) * 0.25)
    albedo = macro_variation(albedo, seed + 20, warm="#d6a860", cool="#a09a84", amount=0.12, value=0.06)
    rough = 0.86 + 0.08 * fuzz
    return Material(albedo, height, rough, depth=0.03, ao_strength=1.4)


def map_paper(size: int, seed: int) -> Material:
    """Papel viejo del mapa: fibras largas visibles, pulpa, ondulación (cockling), pocas
    manchas de agua de forma irregular con cerco, motas de óxido (foxing)."""
    u, v = uv_grid(size)
    blotch = unit(spectral_noise(size, seed + 1, 1, 6, 2.6), 2.4)
    cockle = unit(spectral_noise(size, seed + 2, 2, 12, 2.6))
    fib_l = strokes(size, 70, seed + 3, length=0.45, width=0.05, keep=0.7)
    fib_d = strokes(size, 45, seed + 4, length=0.45, width=0.04, keep=0.5)
    fib_long = strokes(size, 24, seed + 11, length=0.48, width=0.025, keep=0.35)
    pulp = unit(spectral_noise(size, seed + 5, 40, 500, 1.1))
    albedo = ramp(0.45 * blotch + 0.3 * pulp + 0.25 * cockle,
                  [(0.0, "#d0b17c"), (0.35, "#e0c998"), (0.7, "#ecdfbb"), (1.0, "#f6eed6")])
    albedo = mix_color(albedo, "#fbf5e2", fib_l * 0.4)
    albedo = mix_color(albedo, "#a88a5a", fib_d * 0.3)
    albedo = mix_color(albedo, "#9c7c4c", fib_long * 0.45)
    # Manchas de agua: borde deformado por ruido (no círculos), relleno suave y cerco oscuro.
    wu = u + 0.06 * spectral_noise(size, seed + 6, 2, 10, 2.0)
    wv = v + 0.06 * spectral_noise(size, seed + 7, 2, 10, 2.0)
    st = voronoi(size, 2, 2, seed + 8, jitter=0.8, u=wu, v=wv)
    r0 = 0.2 + 0.14 * st["id"] + 0.05 * spectral_noise(size, seed + 12, 3, 14, 1.8)
    alive = st["id2"] < 0.55
    ring = np.exp(-((st["f1"] - r0) / 0.02) ** 2) * alive
    fill = smoothstep(r0, r0 - 0.1, st["f1"]) * alive
    albedo = mix_color(albedo, "#c9a66d", fill * 0.2)
    albedo = mix_color(albedo, "#9b7141", ring * 0.4)
    fox = scatter_dots(size, 24, seed + 9, radius=0.16, keep=0.22, vary=0.5)
    fox_soft = scatter_dots(size, 11, seed + 10, radius=0.3, keep=0.2, vary=0.4)
    albedo = mix_color(albedo, "#8f5a2e", fox["mask"] * 0.5)
    albedo = mix_color(albedo, "#b88b55", fox_soft["mask"] * 0.25)
    albedo = macro_variation(albedo, seed + 20, warm="#e4b777", cool="#d8d2bb", amount=0.16, value=0.06)
    height = 0.5 * cockle + 0.22 * pulp + 0.13 * fib_l + 0.08 * fib_long + 0.07 * ring
    rough = 0.88 - 0.1 * fill
    return Material(albedo, height, rough, depth=0.003, ao_strength=0.6)


def bark(size: int, seed: int) -> Material:
    """Corteza fisurada vertical (troncos, postes); v = a lo largo del tronco.
    Placas alargadas (Voronoi anisótropo, 4:1) separadas por fisuras en V, con grietas
    finas secundarias, fibra vertical, crestas curtidas más grises y liquen en manchas."""
    u, v = uv_grid(size)
    wu = u + 0.018 * spectral_noise(size, seed + 1, 1, 10, 2.2, stretch=(1.0, 4.0))
    wv = v + 0.01 * spectral_noise(size, seed + 2, 1, 8, 2.2)
    vo = voronoi(size, 12, 3, seed, jitter=0.9, u=wu, v=wv, isotropic=False)
    fine = voronoi(size, 30, 6, seed + 5, jitter=0.9, u=wu, v=wv, isotropic=False)
    plate = smoothstep(0.0, 0.4, vo["edge"]) ** 0.6
    sub = smoothstep(0.0, 0.12, fine["edge"])
    fibre = unit(spectral_noise(size, seed + 3, 6, 300, 1.3, stretch=(1.0, 10.0)))
    dome = np.sqrt(np.clip(1.0 - (vo["dx"] * 1.6) ** 2, 0, 1))
    height = plate * (0.55 + 0.15 * vo["id"] + 0.15 * dome) * (0.85 + 0.15 * sub) + 0.12 * fibre * plate
    height = np.clip(height + 0.04 * fibre, 0, 1)
    albedo = ramp(0.5 * plate + 0.25 * fibre + 0.15 * vo["id"] + 0.1 * sub,
                  [(0.0, "#2e1d12"), (0.3, "#5b3b25"), (0.6, "#825a3a"), (0.85, "#a07a56"), (1.0, "#b39a7c")])
    edge = np.clip(cavity(height, 0.005) * 6.0, 0, 1)
    albedo = albedo * (1.0 + 0.22 * edge)[..., None]
    # Crestas curtidas: lo más alto se agrisa (sol y lluvia).
    albedo = mix_color(albedo, "#a59784", smoothstep(0.72, 0.9, height) * 0.45)
    lichen_n = unit(spectral_noise(size, seed + 4, 2, 24, 1.8), 2.0)
    lichen = smoothstep(0.66, 0.76, lichen_n) * plate
    albedo = mix_color(albedo, "#8fae5c", lichen * 0.6)
    dots = scatter_dots(size, 40, seed + 6, radius=0.3, keep=0.35)["mask"] * smoothstep(0.5, 0.65, lichen_n)
    albedo = mix_color(albedo, "#d99a45", dots * plate * 0.7)
    albedo = macro_variation(albedo, seed + 20, warm="#9a6a44", cool="#6a6458", amount=0.16, value=0.08)
    rough = 0.86 - 0.1 * edge + 0.05 * lichen
    return Material(albedo, height, rough, depth=0.035, ao_strength=1.4)


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
        Spec("ForestFloor", forest_floor, 1.5, "Hojarasca del suelo de selva con ramitas y brotes."),
        Spec("Ash", ash, 2.0, "Ceniza volcánica (isla del Humo), con carbones y pómez."),
        Spec("VolcanicRock", volcanic_rock, 3.0, "Basalto en losas facetadas con vesículas y óxido."),
        Spec("Limestone", limestone, 3.0, "Caliza clara estratificada con líquenes (Dientes)."),
        Spec("PalmThatch", palm_thatch, 1.0, "Techo de hebras de palma en hileras solapadas; v = pendiente abajo."),
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
