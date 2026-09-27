"""Intención de paleta: tonos que definen el material y que una iteración no debe perder
(la roca volcánica se volvía marrón barro; la hojarasca tiene que leerse cálida)."""

import numpy as np


def mean_rgb(generated, name):
    return generated[name]["BC"].reshape(-1, 3).mean(axis=0)


def test_volcanic_rock_is_cool_slate_not_mud(generated):
    r, g, b = mean_rgb(generated, "VolcanicRock")
    assert b > r and b > g, f"basalto no azulado: {r:.3f} {g:.3f} {b:.3f}"
    assert 0.25 < (r + g + b) / 3 < 0.5, "basalto demasiado oscuro o lavado"


def test_forest_floor_is_warm_leaf_litter(generated):
    r, g, b = mean_rgb(generated, "ForestFloor")
    assert r > g > b, f"hojarasca no cálida: {r:.3f} {g:.3f} {b:.3f}"
    # Las hojas cubren casi todo: la tierra oscura solo asoma en huecos.
    lum = generated["ForestFloor"]["BC"] @ np.array([0.2126, 0.7152, 0.0722])
    assert (lum < 0.22).mean() < 0.25


def test_ash_is_light_and_nearly_neutral(generated):
    rgb = mean_rgb(generated, "Ash")
    assert rgb.mean() > 0.5, "ceniza demasiado oscura (se ve sucia)"
    assert rgb.max() - rgb.min() < 0.08, "ceniza con dominante de color"


def test_limestone_is_light_and_clean(generated):
    bc = generated["Limestone"]["BC"]
    r, g, b = mean_rgb(generated, "Limestone")
    assert (r + g + b) / 3 > 0.6, "caliza demasiado oscura (se ve sucia)"
    assert r >= b, "caliza fría: tiene que ser crema, no gris azulado"
    # Juntas y alveolos oscurecen poco: casi nada por debajo de un gris medio.
    lum = bc @ np.array([0.2126, 0.7152, 0.0722])
    assert (lum < 0.45).mean() < 0.05


def test_wood_planks_are_warm_with_board_variety(generated):
    r, g, b = mean_rgb(generated, "WoodPlanks")
    assert r > g > b, f"madera no cálida: {r:.3f} {g:.3f} {b:.3f}"
    # Tono distinto por tabla: la mediana de cada hilera no puede ser la misma.
    bc = generated["WoodPlanks"]["BC"]
    size = bc.shape[0]
    rows = [np.median(bc[int((k + 0.5 - 0.29) / 6 * size) % size].mean(axis=-1)) for k in range(6)]
    assert np.ptp(rows) > 0.03, "todas las tablas del mismo tono"
