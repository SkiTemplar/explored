"""Rangos y convenciones de cada mapa."""

import numpy as np
import pytest

from texgen.materials import MATERIALS
from texgen.noise import height_to_normal

PBR = [n for n, s in MATERIALS.items() if s.outputs == ("BC", "N", "ARH")]


def luminance(rgb):
    lin = np.where(rgb <= 0.04045, rgb / 12.92, ((rgb + 0.055) / 1.055) ** 2.4)
    return lin @ np.array([0.2126, 0.7152, 0.0722])


@pytest.mark.parametrize("name", list(MATERIALS))
def test_outputs_declared_and_finite(generated, name):
    maps = generated[name]
    assert tuple(maps) == MATERIALS[name].outputs
    for suffix, arr in maps.items():
        assert arr.shape[:2] == (256, 256), suffix
        assert np.isfinite(arr).all(), suffix


@pytest.mark.parametrize("name", PBR)
def test_albedo_is_plausible_and_alive(generated, name):
    bc = generated[name]["BC"]
    assert bc.shape[-1] == 3
    # Ni negro ni blanco puros (rango físico del color base).
    assert bc.min() >= 0.03 and bc.max() <= 0.96
    lum = luminance(bc)
    assert 0.02 < lum.mean() < 0.75, f"luminancia media {lum.mean():.3f}"
    # «Con vida»: hay variación de valor (no es un plano) pero sin ruido sucio extremo.
    assert 0.01 < lum.std() < 0.25, f"desviación {lum.std():.3f}"
    # Paleta cartoon: algo de saturación media (no gris), salvo ceniza.
    sat = (bc.max(-1) - bc.min(-1)) / bc.max(-1)   # saturación HSV (vale para tonos oscuros)
    if name != "Ash":
        assert sat.mean() > 0.12, f"saturación media {sat.mean():.3f}"


@pytest.mark.parametrize("name", PBR)
def test_arh_channels(generated, name):
    arh = generated[name]["ARH"]
    ao, rough, height = arh[..., 0], arh[..., 1], arh[..., 2]
    assert 0.0 <= ao.min() and ao.max() <= 1.0
    assert ao.min() >= 0.3 - 1e-9, "AO a negro ensucia el color"
    assert ao.mean() > 0.75, "AO demasiado oscura: ensuciaría el color"
    assert 0.04 <= rough.min() and rough.max() <= 1.0
    assert 0.0 <= height.min() and height.max() <= 1.0
    assert height.std() > 0.02, "altura plana"


@pytest.mark.parametrize("name", PBR + ["WaterWaves"])
def test_normals_are_unit_and_facing_up(generated, name):
    n = generated[name]["N"]
    np.testing.assert_allclose(np.linalg.norm(n, axis=-1), 1.0, atol=1e-6)
    assert n[..., 2].min() > 0.2
    # Hay relieve, pero la normal media apunta hacia fuera (sin sesgo de inclinación).
    assert n[..., 2].mean() < 0.9995
    assert abs(n[..., 0].mean()) < 0.05 and abs(n[..., 1].mean()) < 0.05


def test_normal_convention_is_directx():
    """Un bulto: por encima del centro (v menor) la normal mira hacia -V (verde < 0.5)."""
    size = 64
    c = (np.arange(size) + 0.5) / size
    u, v = np.meshgrid(c, c)
    h = np.exp(-(((u - 0.5) ** 2 + (v - 0.5) ** 2) / 0.02))
    n = height_to_normal(h, 0.05)
    assert n[20, 32, 1] < 0 and n[44, 32, 1] > 0   # arriba / abajo
    assert n[32, 20, 0] < 0 and n[32, 44, 0] > 0   # izquierda / derecha


def test_foam_masks(generated):
    m = generated["SeaFoam"]["M"]
    assert m.shape[-1] == 4
    assert m.min() >= 0.0 and m.max() <= 1.0
    for c in range(4):
        assert 0.02 < m[..., c].mean() < 0.7, f"canal {c}"
