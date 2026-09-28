"""Juego estilizado del terreno (texgen/stylized.py): paleta por isla, rugosidad de suelo
mate, normales suaves, sin repetición a 50 m y sin depender de fotos ni de la red.

El tileado sin costura, el determinismo y las normales unitarias de estos materiales ya
los cubren test_tiling.py, test_determinism.py y test_ranges.py (parametrizados sobre
todo MATERIALS); aquí van los criterios propios del encargo del director (2026-09-28)."""

import numpy as np
import pytest

from texgen.materials import MATERIALS, default_seed, generate
from texgen.noise import blur
from texgen.palette import (
    ISLANDS,
    TERRAIN_TARGETS,
    linear_to_oklab,
    oklab_to_linear,
    srgb_to_linear,
    srgb_to_oklab,
    vertex_tint,
)
from texgen.stylized import PALETTES, REFERENCE_ISLAND, ROUGH_MAX, ROUGH_MIN, STYLIZED, swatch_ramp, tone

LUMA = np.array([0.2126, 0.7152, 0.0722])
# ΔE Oklab: 0.02 apenas visible, 0.1 diferencia clara. El 98 % de los píxeles tiene que
# caer a menos de la tolerancia de algún tramo de su paleta (los bordes antialias de una
# máscara mezclan dos muestras y quedan entre ambas: de ahí el margen del 2 % restante).
# En la isla de referencia (Landing, grado neutro) la tolerancia es estricta; en las demás
# se suma el grado Oklab de la isla (≤ 0.03), porque una misma textura sirve a todas y solo
# el terreno se tiñe por isla en M_Terrain.
TOL_REF, TOL_ISLAND, TOL_MAX = 0.025, 0.045, 0.12


def _segment_distance(lab: np.ndarray, ramp: np.ndarray) -> np.ndarray:
    """Distancia de cada color (N, 3) a la polilínea abajo → medio → arriba de una muestra."""
    best = np.full(lab.shape[0], np.inf)
    for a, b in ((ramp[0], ramp[1]), (ramp[1], ramp[2])):
        ab = b - a
        t = np.clip(((lab - a) @ ab) / max(float(ab @ ab), 1e-12), 0.0, 1.0)
        best = np.minimum(best, np.linalg.norm(lab - (a + t[:, None] * ab), axis=1))
    return best


def _tinted(ramp: np.ndarray, vertex_rgb) -> np.ndarray:
    return np.stack([linear_to_oklab(vertex_tint(oklab_to_linear(c), vertex_rgb)) for c in ramp])


def palette_distance(name: str, bc: np.ndarray, island) -> np.ndarray:
    """ΔE de cada píxel a la paleta del material en `island`, tal como se ve en juego: el
    terreno lo tiñe M_Terrain con el color de vértice de la isla (`vertex_tint`); el resto
    se compara con las muestras de la isla sin teñir."""
    px = bc.reshape(-1, 3)
    terrain = name in TERRAIN_TARGETS
    vertex = island.vertex[TERRAIN_TARGETS[name][4]] if terrain else None
    if terrain:
        lab = linear_to_oklab(vertex_tint(srgb_to_linear(px), vertex))
    else:
        lab = srgb_to_oklab(px)
    dist = np.full(lab.shape[0], np.inf)
    for sw in PALETTES[name]:
        ramp = swatch_ramp(sw, island.key)
        if terrain and not sw.startswith("terreno."):
            ramp = _tinted(ramp, vertex)  # la muestra `terreno.*` de la isla ya viene teñida
        dist = np.minimum(dist, _segment_distance(lab, ramp))
    return dist



def test_stylized_set_is_registered():
    assert set(STYLIZED) == {"Grass", "SandDry", "SandWet", "Dirt", "VolcanicRock", "Limestone", "ForestFloor"}
    for name in STYLIZED:
        assert MATERIALS[name].outputs == ("BC", "N", "ARH")
        assert MATERIALS[name].tileable


@pytest.mark.parametrize("island", ISLANDS, ids=lambda i: i.key)
@pytest.mark.parametrize("name", STYLIZED)
def test_albedo_stays_in_the_island_palette(generated, name, island):
    d = palette_distance(name, generated[name]["BC"], island)
    tol = TOL_REF if island.key == REFERENCE_ISLAND.key else TOL_ISLAND
    assert np.percentile(d, 98) < tol, f"{name}/{island.key}: p98 ΔE {np.percentile(d, 98):.3f}"
    assert d.max() < TOL_MAX, f"{name}/{island.key}: ΔE máx {d.max():.3f}"


def test_palette_metric_rejects_off_palette_colors(generated):
    """Control negativo: la hierba teñida de azul o la arena gris salen de la paleta."""
    grass = generated["Grass"]["BC"]
    blue = grass[..., [0, 2, 1]]  # verde → violeta
    sand = generated["SandDry"]["BC"]
    grey = np.repeat(sand.mean(axis=-1, keepdims=True), 3, axis=-1)
    for name, bc in (("Grass", blue), ("SandDry", grey)):
        assert np.percentile(palette_distance(name, bc, ISLANDS[0]), 98) > 1.3 * TOL_REF, name


def test_tone_follows_the_swatch_ramp():
    ramp = swatch_ramp("terreno.hierba")
    np.testing.assert_allclose(tone(ramp, np.array([-1.0, 0.0, 1.0])), ramp, atol=1e-12)
    # Fuera de [-1, 1] se queda en el extremo: el detalle nunca extrapola fuera de la paleta.
    np.testing.assert_allclose(tone(ramp, np.array([-5.0, 5.0])), ramp[[0, 2]], atol=1e-12)


@pytest.mark.parametrize("name", STYLIZED)
def test_ground_roughness_is_matte(generated, name):
    rough = generated[name]["ARH"][..., 1]
    assert rough.min() >= ROUGH_MIN - 1e-9 and rough.max() <= ROUGH_MAX + 1e-9
    assert rough.std() > 0.005, "rugosidad plana: sin variación pintada"
    # Tras cuantizar a 8 bits (lo que llega a Unreal) sigue en rango.
    q = np.round(rough * 255.0) / 255.0
    assert q.min() >= ROUGH_MIN - 0.5 / 255 and q.max() <= ROUGH_MAX + 0.5 / 255


def test_wet_sand_is_smoothest_in_the_swash_band(generated):
    """La película de agua de la resaca es la zona menos rugosa (y la más oscura)."""
    arh = generated["SandWet"]["ARH"]
    lum = generated["SandWet"]["BC"] @ LUMA
    rough = arh[..., 1]
    dark = lum < np.percentile(lum, 20)
    assert rough[dark].mean() < rough[~dark].mean() - 0.01


@pytest.mark.parametrize("name", STYLIZED)
def test_normals_are_soft_but_present(generated, name):
    n = generated[name]["N"]
    np.testing.assert_allclose(np.linalg.norm(n, axis=-1), 1.0, atol=1e-6)
    z = n[..., 2]
    # Relieve suave: ninguna zona amplia muy inclinada (nada de la roca «esculpida» de foto)…
    assert z.mean() > 0.965, f"normal demasiado fuerte: z medio {z.mean():.4f}"
    assert np.percentile(z, 1) > 0.8, f"normal demasiado fuerte: z p1 {np.percentile(z, 1):.3f}"
    # …pero hay normal: no es la «foto plana sin normales» que pidió quitar el director.
    assert z.mean() < 0.9985, f"sin relieve: z medio {z.mean():.4f}"


@pytest.mark.parametrize("name", STYLIZED)
def test_never_flat_matte_color(generated, name):
    """Siempre un poco de textura pintada: variación de valor a escala media y fina."""
    lum = generated[name]["BC"] @ LUMA
    local = (lum - blur(lum, 0.03)).std() / lum.mean()
    assert local > 0.02, f"{name}: color mate liso ({local:.3f})"


@pytest.mark.parametrize("name", STYLIZED)
def test_no_blob_per_tile_repeated_at_50m(generated, name):
    """A 50 m un tile de 2 m ocupa unos 40 px de pantalla (1080p, 60° de campo) y se ven
    decenas a la vez: la repetición se lee por la frecuencia más baja de la textura. Si esa
    energía se concentra en 1 ciclo por tile (una mancha por tile), el terreno se ve como
    una rejilla de manchas repetidas. La macro-variación tiene que vivir en 2–4 ciclos por
    tile (y la de mayor escala la pone M_Terrain en coordenadas de mundo)."""
    lum = generated[name]["BC"] @ LUMA
    size = lum.shape[0]
    power = np.abs(np.fft.fft2(lum - lum.mean())) ** 2
    f = np.fft.fftfreq(size) * size
    fx, fy = np.meshgrid(f, f)
    r = np.hypot(fx, fy)
    low = power[(r > 0) & (r <= 4.5)].sum()
    fundamental = power[(r > 0) & (r < 1.5)].sum() / low
    assert fundamental < 0.25, f"{name}: {fundamental:.0%} de la macro en 1 ciclo por tile"


def test_macro_variation_breaks_mid_distance_repetition(generated):
    """La macro en 2–4 ciclos por tile está (no es un plano a media distancia)."""
    for name in STYLIZED:
        lum = generated[name]["BC"] @ LUMA
        mid = blur(lum, 0.04).std() / lum.mean()
        assert mid > 0.015, f"{name}: sin macro-variación ({mid:.3f})"


@pytest.mark.parametrize("name", STYLIZED)
def test_resolution_independent_design(generated, name):
    """Los rasgos van en unidades de tile: a 128 px sale la misma textura que a 256 px,
    reducida (misma media y mismas formas grandes)."""
    small = generate(name, 128)["BC"]
    big = generated[name]["BC"]
    down = big.reshape(128, 2, 128, 2, 3).mean(axis=(1, 3))
    assert np.abs(small.mean(axis=(0, 1)) - down.mean(axis=(0, 1))).max() < 0.02
    corr = np.corrcoef(blur(small @ LUMA, 0.03).ravel(), blur(down @ LUMA, 0.03).ravel())[0, 1]
    assert corr > 0.8, f"{name}: la forma cambia con la resolución (r = {corr:.2f})"


@pytest.mark.parametrize("name", STYLIZED)
def test_independent_of_global_random_state(generated, name):
    """Determinismo real: la salida depende solo de la semilla, no del estado global de
    NumPy ni de lo que se haya generado antes."""
    np.random.seed(12345)
    _ = np.random.random(1000)
    again = generate(name, 256, seed=default_seed(name))
    for suffix, arr in generated[name].items():
        np.testing.assert_array_equal(arr, again[suffix])


def test_rocks_need_neither_photos_nor_network(monkeypatch):
    """Fuera las fotos: la roca ya no descarga ni lee nada de Poly Haven."""
    import urllib.request

    def no_network(*_a, **_k):
        raise AssertionError("la generación ha intentado usar la red")

    monkeypatch.setattr(urllib.request, "urlopen", no_network)
    for name in ("VolcanicRock", "Limestone"):
        maps = generate(name, 64)
        assert set(maps) == {"BC", "N", "ARH"}


def test_islands_get_their_own_terrain_tone(generated):
    """El mismo T_Grass_BC teñido por M_Terrain da el tono medio de hierba de cada isla."""
    from texgen.palette import island_swatches, hex_to_srgb

    bc = generated["Grass"]["BC"].reshape(-1, 3)
    for island in ISLANDS:
        lab = linear_to_oklab(vertex_tint(srgb_to_linear(bc), island.vertex["Grass"])).mean(axis=0)
        mid = next(s for s in island_swatches(island) if s["id"] == "terreno.hierba")["mid"]
        mid = srgb_to_oklab(hex_to_srgb(mid) if isinstance(mid, str) else mid)
        assert np.linalg.norm(lab - mid) < 0.02, island.key


def test_band_noise_is_periodic_resolution_independent_and_seeded():
    from texgen.noise import band_noise

    a = band_noise(256, 7, 2.0, 12.0)
    b = band_noise(128, 7, 2.0, 12.0)
    down = a.reshape(128, 2, 128, 2).mean(axis=(1, 3))
    assert np.corrcoef(down.ravel(), b.ravel())[0, 1] > 0.99
    np.testing.assert_array_equal(a, band_noise(256, 7, 2.0, 12.0))
    assert np.corrcoef(a.ravel(), band_noise(256, 8, 2.0, 12.0).ravel())[0, 1] < 0.3
    # Periódico: la convolución circular conmuta con el desplazamiento y no hay salto de borde.
    np.testing.assert_allclose(np.roll(blur(a, 0.05), 31, axis=0), blur(np.roll(a, 31, axis=0), 0.05), atol=1e-9)
    assert np.isfinite(a).all() and abs(a.mean()) < 1e-9 and abs(a.std() - 1.0) < 1e-9
    # Por encima del tamaño canónico no se pierde nada de la banda útil.
    big = band_noise(1024, 7, 2.0, 12.0)
    assert np.corrcoef(big.reshape(256, 4, 256, 4).mean(axis=(1, 3)).ravel(), a.ravel())[0, 1] > 0.99
