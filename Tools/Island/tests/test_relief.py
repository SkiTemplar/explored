import numpy as np
import pytest

from explored_island import paths, relief
from explored_island.noise import gradient_noise, hash01, value_noise


def test_ruido_determinista_y_acotado():
    x, y = np.meshgrid(np.linspace(-50, 50, 120), np.linspace(-50, 50, 120))
    g = gradient_noise(x, y, 8.0, 3)
    assert np.array_equal(g, gradient_noise(x, y, 8.0, 3))
    assert g.min() >= -1.0 and g.max() <= 1.0 and abs(float(g.mean())) < 0.1 and g.std() > 0.15
    v = value_noise(x, y, 8.0, 3)
    assert v.min() >= 0.0 and v.max() < 1.0
    h = hash01(np.arange(1000), np.arange(1000), 1, 2)
    assert 0.45 < h.mean() < 0.55


def test_proteccion_de_puntos(layout, props):
    spots = relief.guard_spots(layout, props)
    assert spots.shape[1] == 4 and len(spots) > len(layout.points)
    assert len(relief.guard_spots(layout, None)) == len(layout.points)
    x, y = layout.points['lighthouse']
    w = relief.guard_weight(spots, np.array([x, x + 50.0]), np.array([y, y]))
    assert w[0] == pytest.approx(1.0) and w[1] < 1.0


def test_surcos_radiales_de_la_colina(layout):
    zone = layout.zone(relief.GULLY_ZONE)
    angles = np.linspace(0.0, 2.0 * np.pi, 720, endpoint=False)
    ring = relief.gullies(layout, zone.center[0] + 25.0 * np.cos(angles), zone.center[1] + 25.0 * np.sin(angles))
    peaks = int(((ring > 0.3) & (np.roll(ring, 1) <= 0.3)).sum())
    assert relief.GULLY_COUNT - 3 <= peaks <= relief.GULLY_COUNT + 3
    assert relief.gullies(layout, np.array([zone.center[0]]), np.array([zone.center[1]]))[0] == 0.0


def test_rellanos_solo_en_ladera(layout):
    flat_x, flat_y = np.array([-80.0]), np.array([0.0])
    shift, weight = relief.terraces(layout, flat_x, flat_y, np.array([3.0]))
    assert weight[0] == 0.0 and shift[0] == 0.0
    # En una ladera, el escalonado deja la media casi intacta (rellano y contrahuella).
    base = np.linspace(5.0, 25.0, 4001)
    f = base / relief.TERRACE_STEP
    profile = (relief.smoothstep(0.5, 1.0, f - np.floor(f)) - (f - np.floor(f)) + 0.25) * relief.TERRACE_STEP
    assert abs(float(profile.mean())) < 0.02


def test_playa_dunas_y_berma_sin_tocar_la_orilla(layout):
    x = np.full(5, -120.0)
    y = np.linspace(0.0, 4.0, 5)
    dz, _ = relief.beach(layout.seed, x, y, np.array([-0.5, 0.0, 0.8, 3.5, 6.0]))
    assert abs(dz[0]) < 1e-6 and abs(dz[1]) < 0.01 and dz[2] > 0.1 and dz[4] == pytest.approx(0.0, abs=1e-6)


def test_perfil_del_camino():
    d = np.array([-1.0, 0.6, 5.0])
    out = relief.path_profile(d, np.full(3, 5.0))
    assert out[0] == pytest.approx(-relief.PATH_SINK) and out[1] > 0.0 and abs(out[2]) < 1e-6
    assert relief.path_profile(np.array([-1.0]), np.array([0.0]))[0] == 0.0  # no hunde la orilla
    level = relief.path_level(np.array([-1.0, 10.0]), np.array([5.0, 5.0]), np.array([4.0, 4.0]))
    assert level[0] == pytest.approx(-1.0) and level[1] == 0.0


def test_detalle_nulo_sin_tierra(layout):
    x, y = np.array([-170.0]), np.array([0.0])
    fields = relief.detail(layout, x, y, np.array([-4.0]), np.array([50.0]), relief.guard_spots(layout, None))
    assert fields.dz[0] == 0.0 and fields.gully[0] == 0.0


def test_limita_la_pendiente_del_perfil():
    h = np.array([0.0, 0.0, 2.0, 2.0, 2.0, 0.0])
    out = paths.limit_slope(h, 1.0, 45.0)
    assert np.abs(np.diff(out)).max() <= 1.0 + 1e-9
    assert out.mean() == pytest.approx(h.mean(), abs=0.4)
    assert np.array_equal(paths.limit_slope(h, 1.0, 89.0), h)


def test_altura_del_eje_continua_en_las_curvas(layout):
    profiles = paths.build_profiles(layout, lambda x, y: 0.5 * np.asarray(x, dtype=float))
    assert len(profiles) == len(layout.paths)
    for profile in profiles:
        slope = np.degrees(np.arctan(np.abs(np.diff(profile.heights)) / paths.STEP))
        assert slope.max() <= paths.MAX_ALONG + 1e-6
    # Alrededor de una curva del camino de la colina, la altura del eje no salta.
    bend = np.array(layout.paths[2].points[9])
    angles = np.linspace(0.0, 2.0 * np.pi, 361)
    ring_x, ring_y = bend[0] + 1.2 * np.cos(angles), bend[1] + 1.2 * np.sin(angles)
    heights = paths.axis_height(profiles, ring_x, ring_y)
    assert np.abs(np.diff(heights)).max() < 0.1
