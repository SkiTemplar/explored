"""Juego estilizado del terreno, coherente con el low poly: hierba, arena seca y mojada,
tierra, roca volcánica, caliza y hojarasca.

Sustituye a las texturas pseudo-realistas (hierba de césped fotográfico, roca fotobasheada
de Poly Haven) por pintura hecha a mano por código:

- **Color solo de la paleta.** Cada material pinta con unas pocas muestras de
  `paleta.json` (ver `PALETTES`). Una muestra es un tramo Oklab abajo → medio → arriba
  (la misma rampa que el atlas `T_Palette_<Isla>`); cada píxel elige una muestra y un
  punto `t ∈ [-1, 1]` de ese tramo. El detalle, el trazo y la variación macro mueven `t`,
  nunca se suman como ruido de color, así que el albedo no sale de la paleta.
- **Formas grandes y planos.** Manchas escalonadas en 3–4 valores (`soft_bands`), matas
  de hojas en abanico, rizos de viento, terrones, facetas de roca con plano inclinado
  por cara (el sombreado plano del low poly): poca frecuencia fina, nada de grano
  fotográfico.
- **Un poco de textura siempre.** Trazo anisótropo suave encima de todo, y normal
  derivada de una altura suave (relieve de pocos milímetros: se ve la forma, no el poro).
- **Rugosidad de suelo mate**: 0,85–0,95 en todos (la banda de resaca, en el extremo bajo).
- **Macro-variación** a 2–4 ciclos por tile, sin componente de 1 ciclo: rompe la
  repetición del detalle sin crear una mancha por tile que se repita a 50 m.

Todo en coordenadas de tile y con primitivas periódicas (`texgen/noise.py`); el ruido es
`band_noise`, que no depende de la resolución: la misma textura a 256, 1024 o 2048 px (los
tests a 256 px validan lo que se publica), tileado exacto y determinista por semilla.

Los materiales del terreno (`TERRAIN_TARGETS`) usan la muestra `terreno.*` **sin teñir**
(el objetivo de la paleta): M_Terrain la tiñe luego con el color de vértice de cada isla y
el resultado cae sobre la muestra `terreno.*` de esa isla (lo comprueba
`tests/test_stylized.py`).
"""

from __future__ import annotations

from functools import lru_cache
from typing import TYPE_CHECKING

import numpy as np

from .noise import (
    band_noise,
    blur,
    scatter_dots,
    smoothstep,
    unit,
    uv_grid,
    voronoi,
)
from .palette import (
    ISLANDS,
    TERRAIN_TARGETS,
    _ends,
    hex_to_srgb,
    island_swatches,
    oklab_to_srgb,
    srgb_to_oklab,
    terrain_target_lab,
)

if TYPE_CHECKING:
    from .materials import Material  # solo tipos: materials importa este módulo

# Muestras de paleta que puede usar cada material (la primera es la base).
PALETTES: dict[str, tuple[str, ...]] = {
    "Grass": ("terreno.hierba", "vegetacion.hoja_clara", "vegetacion.hierba_seca", "vegetacion.hoja_oscura"),
    "SandDry": ("terreno.arena_seca", "recurso.concha", "piedra.coral"),
    "SandWet": ("terreno.arena_mojada", "terreno.arena_seca", "entorno.espuma"),
    "Dirt": ("fauna.pardo", "madera.corteza", "mineral.arcilla"),
    "VolcanicRock": ("terreno.basalto", "piedra.obsidiana", "piedra.basalto_claro"),
    "Limestone": ("terreno.caliza", "piedra.caliza_clara", "piedra.canto", "vegetacion.musgo"),
    "ForestFloor": ("madera.corteza", "palma.seca", "palma.dorada", "mineral.terracota", "vegetacion.hoja"),
}
STYLIZED = tuple(PALETTES)

# Rugosidad de suelo mate (encargo del director, 2026-09-28).
ROUGH_MIN, ROUGH_MAX = 0.85, 0.95

REFERENCE_ISLAND = ISLANDS[0]  # Landing: grado neutro, la paleta de referencia.
_TERRAIN_BY_KEY = {key: mat for mat, (key, *_r) in TERRAIN_TARGETS.items()}


# ---------------------------------------------------------------------------
# Paleta: tramos Oklab
# ---------------------------------------------------------------------------

@lru_cache(maxsize=None)
def _island_ramps(island_key: str) -> dict[str, np.ndarray]:
    island = next(i for i in ISLANDS if i.key == island_key)
    out = {}
    for sw in island_swatches(island):
        out[sw["id"]] = np.stack([srgb_to_oklab(hex_to_srgb(sw[k]) if isinstance(sw[k], str) else sw[k])
                                  for k in ("bottom", "mid", "top")])
    return out


def swatch_ramp(swatch_id: str, island_key: str | None = None) -> np.ndarray:
    """(3, 3) Oklab: abajo, medio, arriba. Sin isla, las `terreno.*` son el objetivo sin
    teñir (lo que va en la textura) y el resto, la muestra de la isla de referencia."""
    family, key = swatch_id.split(".", 1)
    if island_key is None and family == "terreno":
        mid = terrain_target_lab(_TERRAIN_BY_KEY[key])
        top, bottom = _ends(mid)
        return np.stack([bottom, mid, top])
    return _island_ramps(island_key or REFERENCE_ISLAND.key)[swatch_id].copy()


def tone(ramp: np.ndarray, t: np.ndarray | float) -> np.ndarray:
    """Color Oklab (…, 3) en el tramo de una muestra: t = -1 abajo, 0 medio, 1 arriba."""
    t = np.clip(np.asarray(t, dtype=np.float64), -1.0, 1.0)[..., None]
    bottom, mid, top = ramp
    return np.where(t < 0.0, mid + (mid - bottom) * t, mid + (top - mid) * t)


def over(lab: np.ndarray, color: np.ndarray, mask: np.ndarray) -> np.ndarray:
    m = np.clip(mask, 0.0, 1.0)[..., None]
    return lab + (color - lab) * m


def to_srgb(lab: np.ndarray) -> np.ndarray:
    return np.clip(oklab_to_srgb(lab), 0.0, 1.0)


# ---------------------------------------------------------------------------
# Primitivas de pintura
# ---------------------------------------------------------------------------

def soft_bands(t: np.ndarray, n: int, softness: float = 0.3) -> np.ndarray:
    """Escalonado suave de `t` (0-1) en `n` bandas: planos de valor casi constante unidos
    por transiciones cortas (manchas pintadas, caras planas de low poly)."""
    x = np.clip(t, 0.0, 1.0) * n
    k = np.floor(x)
    f = x - k
    edge = 0.5 * softness
    step = np.clip((f - (0.5 - edge)) / (2.0 * edge), 0.0, 1.0)
    step = step * step * (3.0 - 2.0 * step)
    return np.clip((k + step) / n, 0.0, 1.0)


def patches(size: int, seed: int, fmin: float = 2.0, fmax: float = 7.0, bands: int = 4) -> np.ndarray:
    """Manchas pintadas escalonadas en `bands` valores, en [-1, 1]."""
    return soft_bands(unit(band_noise(size, seed, fmin, fmax, 2.5), 2.2), bands) * 2.0 - 1.0


def macro(size: int, seed: int) -> np.ndarray:
    """Variación macro de 2 a 4 ciclos por tile (sin el de 1 ciclo, que se repetiría como
    una mancha por tile a 50 m), en [-1, 1]."""
    return np.clip(band_noise(size, seed, 2.0, 4.0, 2.0) * 0.5, -1.0, 1.0)


def brush(size: int, seed: int, along: float, fmin: float = 10.0, fmax: float = 48.0) -> np.ndarray:
    """Trazo de pincel: ruido alargado ~5:1 en la dirección `along` (radianes, 0 = u), en
    [-1, 1]. Es la «textura pintada» que evita el color mate liso."""
    n = band_noise(size, seed, fmin, fmax, 1.6, stretch=(5.0, 1.0), angle=along)
    return np.clip(n / 2.2, -1.0, 1.0)


def warped_uv(size: int, seed: int, amount: float, fmax: float = 5.0):
    """Coordenadas de tile deformadas por un ruido periódico (rompe cualquier rejilla)."""
    u, v = uv_grid(size)
    wu = band_noise(size, seed, 1.0, fmax, 2.5)
    wv = band_noise(size, seed + 1, 1.0, fmax, 2.5)
    return u + amount * wu, v + amount * wv


def fan_tufts(size: int, n: int, seed: int, length: float, width: float, keep: float,
              blades: int = 3, fan: float = 0.5, lean: float = 0.0):
    """Matas de hojas en abanico (hierba pintada): `blades` hojas en lanza que nacen del
    punto de cada celda de un Voronoi n × n y se abren `fan` rad alrededor de «arriba»
    (−v) inclinadas `lean`. Devuelve máscara, `s` (0 base → 1 punta) e id por mata.
    Cada mata se desvanece antes de la junta de celda (nunca se ve el polígono)."""
    vo = voronoi(size, n, n, seed, jitter=0.9)
    mask = np.zeros_like(vo["f1"])
    s_out = np.zeros_like(vo["f1"])
    # Vector del punto de la mata al píxel (dx, dy apuntan del píxel al punto).
    px, py = -vo["dx"], -vo["dy"]
    base_ang = -np.pi / 2 + lean + (vo["id"] - 0.5) * 0.5
    for k in range(blades):
        off = (k - (blades - 1) / 2) * fan / max(blades - 1, 1)
        ang = base_ang + off + (np.mod(vo["id2"] * (7.3 + k * 3.1), 1.0) - 0.5) * 0.25
        ca, sa = np.cos(ang), np.sin(ang)
        a = px * ca + py * sa
        b = -px * sa + py * ca
        blade_len = length * (0.75 + 0.35 * np.mod(vo["id"] * (5.7 + k), 1.0))
        s = a / blade_len
        half = width * np.clip(1.0 - s, 0.0, 1.0) ** 0.8 * smoothstep(-0.05, 0.12, s)
        m = (1.0 - smoothstep(0.7, 1.0, np.abs(b) / (half + 1e-6))) * (s >= 0.0) * (s <= 1.0)
        take = m > mask
        s_out = np.where(take, s, s_out)
        mask = np.maximum(mask, m)
    alive = (vo["id2"] < keep).astype(np.float64)
    fade = smoothstep(0.02, 0.18, vo["edge"])
    return mask * alive * fade, np.clip(s_out, 0.0, 1.0), vo["id"]


def leaf_shapes(size: int, n: int, seed: int, length: float, width: float, keep: float) -> dict:
    """Hojas caídas grandes vistas desde arriba (almendra con punta): máscara, `s` a lo
    largo (0 peciolo → 1 punta), `across` (0 nervio → 1 borde), lado del nervio e id."""
    vo = voronoi(size, n, n, seed, jitter=0.9)
    ang = vo["id"] * 2.0 * np.pi
    ca, sa = np.cos(ang), np.sin(ang)
    a = -(vo["dx"] * ca + vo["dy"] * sa)
    b = -(-vo["dx"] * sa + vo["dy"] * ca)
    s = a / length + 0.5
    half = width * np.clip(np.sin(np.pi * np.clip(s, 0.0, 1.0)), 0.0, 1.0) ** 0.7 + 1e-4
    across = np.abs(b) / half
    mask = (1.0 - smoothstep(0.85, 1.0, across)) * (s > 0.0) * (s < 1.0) * (vo["id2"] < keep)
    mask = mask * smoothstep(0.01, 0.1, vo["edge"])
    return {"mask": mask, "s": np.clip(s, 0.0, 1.0), "across": np.clip(across, 0.0, 1.0),
            "side": np.sign(b), "id": vo["id2"] / max(keep, 1e-6), "id2": vo["id"]}


def facet_shade(vo: dict, k1: float, k2: float) -> np.ndarray:
    """Plano inclinado por celda (pendiente aleatoria): sombreado plano de cara low poly."""
    a = np.mod(vo["id"] * k1, 1.0) - 0.5
    b = np.mod(vo["id2"] * k2 + vo["id"] * 3.1, 1.0) - 0.5
    return vo["dx"] * a + vo["dy"] * b


def rough_field(size: int, seed: int, lo: float = ROUGH_MIN + 0.02, hi: float = ROUGH_MAX) -> np.ndarray:
    return lo + (hi - lo) * unit(band_noise(size, seed, 2.0, 16.0, 2.0), 2.5)


def _result(lab: np.ndarray, height: np.ndarray, rough: np.ndarray, depth: float, soften: float) -> Material:
    """Empaqueta un material: la normal sale de la altura algo desenfocada (relieve suave)."""
    from .materials import Material  # evita el ciclo materials ↔ stylized

    h = np.clip(height, 0.0, 1.0)
    return Material(albedo=to_srgb(lab), height=blur(h, soften) if soften > 0 else h,
                    rough=np.clip(rough, ROUGH_MIN, ROUGH_MAX), depth=depth, ao_strength=0.8)


# ---------------------------------------------------------------------------
# Materiales
# ---------------------------------------------------------------------------

def grass(size: int, seed: int):
    """Hierba pintada: manchas de verde en 4 valores, trazo inclinado por el viento y matas
    en abanico de 3 hojas en lanza (base honda, punta clara); lima y paja como acento."""
    base, light, dry, dark = (swatch_ramp(s) for s in PALETTES["Grass"])
    mac = macro(size, seed + 1)
    t = 0.4 * patches(size, seed, 2.0, 6.0, 4) + 0.6 * mac + 0.15 * brush(size, seed + 2, along=-1.2)
    lab = tone(base, t - 0.35)
    # Hueco entre manchas: verde hondo, nunca negro.
    hollow = smoothstep(0.2, 0.6, -patches(size, seed + 3, 2.0, 5.0, 3))
    lab = over(lab, tone(dark, 0.6 + 0.3 * t), hollow * 0.3)
    height = 0.25 + 0.15 * t
    for j, (n, length, width, keep) in enumerate([(6, 0.9, 0.15, 0.95), (9, 0.9, 0.14, 0.9),
                                                  (13, 0.85, 0.13, 0.9), (19, 0.8, 0.13, 0.85),
                                                  (27, 0.8, 0.13, 0.8)]):
        m, s, cid = fan_tufts(size, n, seed + 10 + j, length, width, keep, blades=3, fan=0.9, lean=-0.25)
        blade_t = -0.6 + 1.9 * s + 0.3 * (cid - 0.5) + 0.5 * mac
        kind = np.mod(cid * 13.7, 1.0)
        col = tone(base, blade_t)
        # Base de la hoja en verde hondo y punta en lima: más valor sin salir de la paleta.
        col = over(col, tone(dark, 0.8), (1.0 - smoothstep(0.0, 0.3, s)) * 0.4)
        col = over(col, tone(light, 0.2 * mac), smoothstep(0.6, 1.0, s) * 0.55)
        col = np.where((kind < 0.15)[..., None], tone(light, blade_t - 0.2), col)
        col = np.where((kind > 0.97)[..., None], tone(dry, blade_t - 0.5), col)
        lab = over(lab, col, m)
        height = np.maximum(height, m * (0.45 + 0.1 * j + 0.3 * s))
    return _result(lab, height, rough_field(size, seed + 40, 0.9, 0.95), depth=0.006, soften=0.0015)


def _ripples(size: int, seed: int, cycles: int):
    u, v = uv_grid(size)
    warp = band_noise(size, seed, 1.0, 4.0, 3.0)
    warp2 = band_noise(size, seed + 1, 3.0, 10.0, 2.5)
    phase = cycles * v + u + 0.3 * warp + 0.05 * warp2
    s = np.mod(phase, 1.0)
    # Rizo eólico asimétrico: ladera suave a barlovento, cara corta a sotavento.
    rip = np.where(s < 0.72, s / 0.72, (1.0 - s) / 0.28)
    return smoothstep(0.0, 1.0, rip)


def sand_dry(size: int, seed: int):
    """Arena seca pintada: crema en 3 valores siguiendo rizos de viento anchos (cresta
    clara, sotavento algo más tostado), trazo a lo largo del rizo y alguna concha rosada."""
    base, shell, coral = (swatch_ramp(s) for s in PALETTES["SandDry"])
    rip = _ripples(size, seed, 6)
    zones = smoothstep(0.2, 0.8, unit(band_noise(size, seed + 3, 2.0, 5.0, 2.0), 2.0))
    t = 0.5 * (soft_bands(rip, 3) * 2.0 - 1.0) * (0.45 + 0.55 * zones) \
        + 0.3 * patches(size, seed + 4, 2.0, 6.0, 3) + 0.7 * macro(size, seed + 5) \
        + 0.12 * brush(size, seed + 6, along=0.15)
    lab = tone(base, t)
    height = 0.55 * rip * (0.4 + 0.6 * zones) + 0.15 * (t * 0.5 + 0.5)
    for j, (n, ramp, keep) in enumerate([(11, shell, 0.07), (17, coral, 0.05)]):
        d = scatter_dots(size, n, seed + 20 + j, radius=0.16, keep=keep, vary=0.4)
        disc = smoothstep(0.35, 0.65, d["mask"])
        lab = over(lab, tone(ramp, 0.4 - 0.9 * d["dy"] / 0.16), disc)
        height = np.maximum(height, disc * 0.75)
    return _result(lab, height, rough_field(size, seed + 40, 0.91, 0.95), depth=0.012, soften=0.002)


def sand_wet(size: int, seed: int):
    """Arena mojada de orilla en bandas paralelas a la costa (a lo largo de u): línea fina
    de espuma, película oscura de agua (la más lisa, rugosidad 0,85) que se va secando
    hacia una franja escurrida más clara; trazo a lo largo de la orilla."""
    base, dry, foam = (swatch_ramp(s) for s in PALETTES["SandWet"])
    u, v = uv_grid(size)
    warp = band_noise(size, seed, 2.0, 6.0, 2.5, stretch=(1.0, 3.0))
    phase = 2.0 * v + 0.07 * warp + 0.03 * band_noise(size, seed + 1, 4.0, 14.0, 2.0)
    s = np.mod(phase, 1.0)
    film = smoothstep(0.03, 0.08, s) * (1.0 - smoothstep(0.35, 0.6, s))
    drying = smoothstep(0.35, 0.95, s)
    t = -0.4 * film + 0.4 * drying + 0.35 * macro(size, seed + 2) \
        + 0.15 * brush(size, seed + 3, along=0.0) + 0.2 * patches(size, seed + 4, 2.0, 6.0, 3)
    lab = tone(base, t)
    # Franja escurrida: asoma el color de arena seca (su tramo bajo), en manchas.
    dry_mask = drying * smoothstep(0.1, 0.5, patches(size, seed + 5, 3.0, 8.0, 3))
    lab = over(lab, tone(dry, -0.9 + 0.2 * t), dry_mask * 0.35)
    # Frente de espuma: línea fina e irregular al inicio de cada banda.
    lace = unit(band_noise(size, seed + 6, 6.0, 30.0, 1.8, stretch=(4.0, 1.0)), 2.0)
    front = (1.0 - smoothstep(0.0, 0.03, s)) * smoothstep(0.4, 0.6, lace)
    lab = over(lab, tone(foam, -0.2), front * 0.8)
    height = 0.45 + 0.25 * drying - 0.2 * film + 0.1 * patches(size, seed + 7, 4.0, 12.0, 3) + 0.2 * front
    rough = ROUGH_MIN + 0.005 + 0.08 * (1.0 - film) * (0.6 + 0.4 * unit(band_noise(size, seed + 8, 2.0, 12.0)))
    return _result(lab, height, rough, depth=0.012, soften=0.002)


def dirt(size: int, seed: int):
    """Tierra pintada: pardo cálido en terrones redondeados con cara plana (sombreado low
    poly), juntas de corteza oscura, vetas de arcilla roja y migas sueltas."""
    base, dark, clay = (swatch_ramp(s) for s in PALETTES["Dirt"])
    uu, vv = warped_uv(size, seed, 0.02)
    mac = macro(size, seed + 2)
    t = 0.25 * patches(size, seed + 1, 2.0, 6.0, 3) + 0.6 * mac \
        + 0.12 * brush(size, seed + 3, along=0.7)
    lab = tone(base, t)
    height = 0.3 + 0.1 * t
    red = smoothstep(0.25, 0.6, patches(size, seed + 4, 2.0, 5.0, 3))
    lab = over(lab, tone(clay, -0.4 + 0.5 * t), red * 0.45)
    for j, (n, keep, lift) in enumerate([(7, 1.0, 0.0), (13, 0.7, 0.15)]):
        vo = voronoi(size, n, n, seed + 10 + j, jitter=1.0, u=uu, v=vv)
        dome = 1.0 - smoothstep(0.0, 0.62, vo["f1"])
        alive = (vo["id2"] < keep) * smoothstep(0.02, 0.1, vo["edge"])
        shade = soft_bands(np.clip(0.5 + 1.6 * facet_shade(vo, 17.3, 11.9) + 0.4 * (vo["id"] - 0.5), 0, 1), 3)
        clod_t = -0.5 + 1.2 * shade + 0.35 * dome + 0.5 * mac
        lab = over(lab, tone(base, clod_t), alive * smoothstep(0.1, 0.35, dome))
        height = np.maximum(height, alive * (0.35 + lift + 0.45 * dome))
        if j == 0:
            gap = 1.0 - smoothstep(0.0, 0.07, vo["edge"])
            lab = over(lab, tone(dark, -0.2 + 0.4 * t), gap * 0.8)
    # Migas sueltas: bultitos con la cara de arriba iluminada y la de abajo en sombra.
    d = scatter_dots(size, 23, seed + 20, radius=0.2, keep=0.25, vary=0.5, u=uu, v=vv)
    crumb = smoothstep(0.35, 0.6, d["mask"])
    lab = over(lab, tone(base, 0.3 + 3.0 * d["dy"] + 0.4 * mac), crumb * 0.9)
    height = np.maximum(height, crumb * 0.7)
    return _result(lab, height, rough_field(size, seed + 40), depth=0.006, soften=0.002)


def volcanic_rock(size: int, seed: int):
    """Basalto low poly: caras planas grandes (Voronoi deformado con un plano inclinado
    por cara, escalonado en 4 valores) y una segunda talla más pequeña encima; grietas de
    obsidiana finas y cantos altos en basalto claro. Gris violáceo frío."""
    base, crack, light = (swatch_ramp(s) for s in PALETTES["VolcanicRock"])
    uu, vv = warped_uv(size, seed, 0.035, fmax=4.0)
    big = voronoi(size, 6, 6, seed + 1, jitter=1.0, u=uu, v=vv)
    small = voronoi(size, 11, 11, seed + 2, jitter=1.0, u=uu * 1.0 + 0.37, v=vv + 0.21)
    plane = 1.1 * facet_shade(big, 13.1, 7.7) + 0.15 * (big["id"] - 0.5) + 0.6 * facet_shade(small, 5.3, 9.1)
    face = soft_bands(np.clip(0.5 + plane, 0.0, 1.0), 4) * 2.0 - 1.0
    t = 0.75 * face + 0.2 * macro(size, seed + 3) + 0.12 * brush(size, seed + 4, along=-0.8)
    lab = tone(base, t)
    lab = over(lab, tone(light, -0.6 + 0.5 * face), smoothstep(0.6, 0.85, face) * 0.3)
    edge = 1.0 - smoothstep(0.0, 0.045, big["edge"])
    lab = over(lab, tone(crack, 0.2), edge * 0.85)
    edge2 = 1.0 - smoothstep(0.0, 0.04, small["edge"])
    lab = over(lab, tone(base, -0.9), edge2 * 0.35)
    height = 0.5 + 0.3 * np.clip(plane, -1, 1) + 0.15 * (big["id"] - 0.5) - 0.25 * edge - 0.08 * edge2
    return _result(lab, height, rough_field(size, seed + 40, 0.87, 0.95), depth=0.014, soften=0.003)


def limestone(size: int, seed: int):
    """Caliza en estratos: 5 bancos ondulados por tile partidos en bloques, con la repisa
    de arriba iluminada y la base en sombra, chorreones verticales de canto gris bajo
    cada repisa y un toque de musgo en algunas repisas. Clara y cálida."""
    base, light, drip, moss = (swatch_ramp(s) for s in PALETTES["Limestone"])
    u, v = uv_grid(size)
    strata = 5
    warp = band_noise(size, seed, 1.0, 3.0, 2.5, stretch=(1.0, 2.0))
    phase = strata * v + 0.12 * warp + 0.04 * band_noise(size, seed + 7, 3.0, 9.0, 2.0)
    k = np.floor(phase)
    f = phase - k
    row = np.mod(k, strata)
    rnd = np.random.default_rng(seed + 1).random((strata, 3))
    ki = row.astype(np.int64)
    blocks = 5
    bu = u * blocks + rnd[ki, 0] * blocks + 0.15 * band_noise(size, seed + 2, 1.0, 6.0, 2.5)
    bid = np.mod(np.floor(bu), blocks)
    bf = bu - np.floor(bu)
    block_tone = np.mod(np.sin((bid + 1.0) * 12.9898 + (row + 1.0) * 78.233) * 43758.5453, 1.0) - 0.5
    ledge = 1.0 - smoothstep(0.0, 0.22, f)       # repisa iluminada arriba del banco
    shadow = smoothstep(0.72, 1.0, f)             # base del banco en sombra
    joint = np.clip((1.0 - smoothstep(0.0, 0.03, bf)) + smoothstep(0.97, 1.0, bf), 0.0, 1.0) * (1.0 - ledge)
    t = 0.5 * soft_bands(block_tone + 0.5, 3) - 0.25 + 0.45 * ledge - 0.55 * shadow \
        + 0.5 * macro(size, seed + 3) + 0.1 * brush(size, seed + 4, along=np.pi / 2)
    lab = tone(base, t)
    lab = over(lab, tone(light, -0.2 + 0.6 * ledge), ledge * 0.55 * (block_tone > -0.15))
    streak = unit(band_noise(size, seed + 5, 4.0, 24.0, 2.0, stretch=(1.0, 6.0)), 2.0)
    drips = smoothstep(0.62, 0.8, streak) * smoothstep(0.15, 0.6, f) * (1.0 - shadow)
    lab = over(lab, tone(drip, 0.6), drips * 0.5)
    lab = over(lab, tone(drip, -0.2), joint * 0.7)
    moss_on = (np.mod(rnd[ki, 1] + bid * 0.37, 1.0) < 0.3)
    moss_m = ledge * moss_on * smoothstep(0.55, 0.75, unit(band_noise(size, seed + 6, 6.0, 20.0, 2.0), 2.0))
    lab = over(lab, tone(moss, 0.2), moss_m * 0.7)
    height = 0.75 - 0.45 * f + 0.1 * block_tone - 0.2 * joint
    return _result(lab, height, rough_field(size, seed + 40, 0.87, 0.95), depth=0.016, soften=0.005)


def forest_floor(size: int, seed: int):
    """Hojarasca pintada: hojas grandes en almendra, en cuatro capas (las de abajo más
    en sombra), de dos tonos a cada lado del nervio como una hoja plegada; paja seca,
    dorada, teja y ~12 % aún verdes sobre tierra de corteza."""
    ground, dry, gold, tile, green = (swatch_ramp(s) for s in PALETTES["ForestFloor"])
    mac = macro(size, seed + 2)
    t0 = 0.3 * patches(size, seed + 1, 2.0, 7.0, 3) + 0.3 * mac
    lab = tone(ground, t0 - 0.2)
    height = 0.2 + 0.05 * t0
    layers = [(5, 1.0, 0.36, 0.9), (6, 1.0, 0.34, 0.9), (7, 1.0, 0.34, 0.9), (8, 0.95, 0.32, 0.9)]
    stroke = brush(size, seed + 3, along=0.4, fmin=16.0, fmax=60.0)
    for j, (n, length, width, keep) in enumerate(layers):
        lf = leaf_shapes(size, n, seed + 10 + j, length, width, keep)
        depth_shade = -0.45 * (len(layers) - 1 - j) / (len(layers) - 1)
        fold = np.where(lf["side"] > 0, 0.35, -0.25)
        rib = 1.0 - smoothstep(0.0, 0.1, lf["across"])
        lt = depth_shade + fold + 0.25 * (lf["s"] - 0.5) + 0.12 * stroke + 0.3 * rib + 0.4 * mac
        pick = np.mod(lf["id2"] * 7.1, 1.0)
        col = tone(dry, lt)
        col = np.where((pick < 0.28)[..., None], tone(gold, lt - 0.1), col)
        col = np.where(((pick >= 0.28) & (pick < 0.5))[..., None], tone(tile, lt - 0.2), col)
        col = np.where((pick > 0.92)[..., None], tone(green, lt - 0.2), col)
        lab = over(lab, col, lf["mask"])
        dome = 1.0 - lf["across"] ** 2
        height = np.where(lf["mask"] > 0.5, 0.3 + 0.15 * j + 0.1 * dome, height)
    return _result(lab, height, rough_field(size, seed + 40), depth=0.004, soften=0.003)


GENERATORS = {
    "Grass": grass,
    "SandDry": sand_dry,
    "SandWet": sand_wet,
    "Dirt": dirt,
    "VolcanicRock": volcanic_rock,
    "Limestone": limestone,
    "ForestFloor": forest_floor,
}
assert set(GENERATORS) == set(PALETTES)

__all__ = ["GENERATORS", "PALETTES", "ROUGH_MAX", "ROUGH_MIN", "STYLIZED", "swatch_ramp", "tone"]
