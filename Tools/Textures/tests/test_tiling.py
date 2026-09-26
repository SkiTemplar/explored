"""Tileado sin costura: el salto entre la última y la primera fila/columna debe ser como
cualquier otro salto entre píxeles vecinos del interior de la textura."""

import numpy as np
import pytest

from texgen.materials import MATERIALS
from texgen.noise import blur, spectral_noise, voronoi

# Umbral: salto medio en el borde / percentil 99.5 de los saltos interiores. En las texturas
# periódicas sale ≤ ~1.1; una textura sin tilear da 10-30 (ver test_metric_detects_seams).
MAX_SEAM_RATIO = 1.5


def seam_ratio(channel: np.ndarray, axis: int) -> float:
    inner = np.abs(np.diff(channel, axis=axis)).mean(axis=1 - axis)
    wrap = np.abs(np.take(channel, 0, axis=axis) - np.take(channel, -1, axis=axis)).mean()
    return float(wrap / (np.percentile(inner, 99.5) + 1e-9))


def channels(arr: np.ndarray):
    arr = arr if arr.ndim == 3 else arr[..., None]
    for c in range(arr.shape[-1]):
        yield c, arr[..., c]


@pytest.mark.parametrize("name", list(MATERIALS))
def test_material_maps_tile_without_seams(generated, name):
    for suffix, arr in generated[name].items():
        for c, ch in channels(arr):
            for axis in (0, 1):
                r = seam_ratio(ch, axis)
                assert r < MAX_SEAM_RATIO, f"{name}_{suffix} canal {c} eje {axis}: costura {r:.2f}"


@pytest.mark.parametrize("name", list(MATERIALS))
def test_encoded_png_tiles_without_seams(generated, name):
    """Lo mismo tras cuantizar a 8 bits, que es lo que llega a Unreal."""
    from texgen.output import encode

    for suffix, arr in generated[name].items():
        img = np.asarray(encode(suffix, arr), dtype=np.float64) / 255.0
        for c, ch in channels(img):
            for axis in (0, 1):
                assert seam_ratio(ch, axis) < MAX_SEAM_RATIO, f"{name}_{suffix} canal {c}"


def test_metric_detects_seams():
    """Control negativo: recortar un ruido periódico o integrar ruido blanco crea costura."""
    cropped = spectral_noise(256, 1, 1, 8)[:, :200]
    walk = np.cumsum(np.random.default_rng(0).standard_normal((256, 256)), axis=1)
    assert seam_ratio(cropped, 1) > 5 * MAX_SEAM_RATIO
    assert seam_ratio(walk, 1) > 5 * MAX_SEAM_RATIO


def test_primitives_are_periodic():
    """Las primitivas son periódicas exactas: evaluar en u+1 / v+1 da lo mismo."""
    size = 64
    c = (np.arange(size) + 0.5) / size
    u, v = np.meshgrid(c, c)
    a = voronoi(size, 5, 3, 7, u=u, v=v)
    b = voronoi(size, 5, 3, 7, u=u + 1.0, v=v - 1.0)
    for key in ("f1", "f2", "id"):
        np.testing.assert_allclose(a[key], b[key], atol=1e-9)
    n = spectral_noise(size, 3, 1, 16)
    # Periódico ⇔ el desenfoque (convolución circular) conmuta con el desplazamiento.
    np.testing.assert_allclose(np.roll(blur(n, 0.05), 17, axis=1), blur(np.roll(n, 17, axis=1), 0.05), atol=1e-9)
