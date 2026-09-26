"""Variación macro: cada albedo debe tener algo de variación a gran escala (rompe la
repetición a media distancia) pero sin manchas que dominen el tile (se verían repetidas)."""

import numpy as np
import pytest

from texgen.materials import MATERIALS
from texgen.noise import blur, uv_grid, voronoi

LUMA = np.array([0.2126, 0.7152, 0.0722])
# Desviación relativa de la luminancia desenfocada (σ = 8 % del tile). Hoy: 0.022–0.089.
MACRO_MIN, MACRO_MAX = 0.015, 0.12


@pytest.mark.parametrize("name", [n for n, s in MATERIALS.items() if "BC" in s.outputs])
def test_albedo_has_moderate_macro_variation(generated, name):
    lum = generated[name]["BC"] @ LUMA
    macro = blur(lum, 0.08).std() / lum.mean()
    assert MACRO_MIN < macro < MACRO_MAX, f"{name}: variación macro {macro:.3f}"


def test_anisotropic_voronoi_is_periodic_and_elongated():
    """isotropic=False: celdas alargadas como la rejilla (corteza) y sigue siendo periódico."""
    size = 128
    u, v = uv_grid(size)
    a = voronoi(size, 12, 3, 5, u=u, v=v, isotropic=False)
    b = voronoi(size, 12, 3, 5, u=u - 1.0, v=v + 2.0, isotropic=False)
    np.testing.assert_allclose(a["edge"], b["edge"], atol=1e-9)
    # Cambios de celda a lo largo de una fila (u) frente a una columna (v): ~4:1.
    along_u = (np.diff(a["id"], axis=1) != 0).sum()
    along_v = (np.diff(a["id"], axis=0) != 0).sum()
    assert along_u > 2.5 * along_v
