"""Filtros del fotobasheado de roca (sin red: entradas sintéticas)."""

import numpy as np
import pytest

from texgen.photobash import kuwahara_periodic, soft_bands


@pytest.fixture(scope="module")
def image():
    rng = np.random.default_rng(7)
    img = rng.random((64, 64, 3))
    img[16:40, 20:50] += 0.6  # un bloque con borde vivo
    return np.clip(img, 0.0, 1.0)


def test_kuwahara_es_periodico(image):
    """Desplazar la entrada desplaza la salida igual: el filtro no rompe el tileado."""
    out = kuwahara_periodic(image, 4)
    shifted = kuwahara_periodic(np.roll(image, (11, -23), axis=(0, 1)), 4)
    assert np.allclose(np.roll(out, (11, -23), axis=(0, 1)), shifted, atol=1e-9)


def test_kuwahara_conserva_rango_y_color_plano(image):
    out = kuwahara_periodic(image, 4)
    assert out.shape == image.shape
    assert out.min() >= image.min() - 1e-9 and out.max() <= image.max() + 1e-9
    flat = np.full((32, 32, 3), 0.37)
    assert np.allclose(kuwahara_periodic(flat, 3), 0.37)


def test_kuwahara_alisa_sin_emborronar_el_borde(image):
    """Óleo: el interior del bloque queda mucho más liso y el salto del borde se conserva."""
    out = kuwahara_periodic(image, 4)
    lum_in, lum_out = image.mean(-1), out.mean(-1)
    assert lum_out[22:34, 26:44].std() < 0.5 * lum_in[22:34, 26:44].std()
    assert lum_out[28, 30:40].mean() - lum_out[28, 55:62].mean() > 0.35


def test_soft_bands_monotono_y_en_rango():
    t = np.linspace(0.0, 1.0, 1001)
    b = soft_bands(t, 5)
    assert b.min() >= 0.0 and b.max() <= 1.0
    assert np.all(np.diff(b) >= -1e-12)
    # Mesetas: la mayor parte del recorrido cambia poco (planos de tono).
    assert (np.abs(np.diff(b)) < 0.2 / 1000).mean() > 0.5
