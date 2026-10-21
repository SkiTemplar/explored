import json

import numpy as np
import pytest
from explored_style.palette import PALETTE_PATH

from explored_island.layout import height_at, zone_at
from explored_island.terrain import (
    PALETTE,
    ROCK_SLOPE,
    build_terrain,
    surface_height,
)

ROCK = {PALETTE['roca_media'], PALETTE['roca_clara'], PALETTE['roca_oscura']}
GRASS = {PALETTE[n] for n in ('hierba_clara', 'hierba_oscura', 'hierba_salvia', 'hierba_seca', 'verde_selva',
                              'verde_lima', 'tierra')}


def face_at(mesh, x, y):
    d = np.hypot(mesh.centroids[:, 0] - x, mesh.centroids[:, 1] - y)
    return int(np.argmin(d))


def test_indices_de_paleta_coinciden_con_tools_style():
    colores = {c['nombre']: c['indice'] for c in json.loads(PALETTE_PATH.read_text(encoding='utf-8'))['colores']}
    for nombre, indice in PALETTE.items():
        assert colores[nombre] == indice, nombre


def test_rejilla_completa_y_normales_hacia_arriba(layout, terreno):
    t = layout.terrain
    nx = int(np.ceil((t.max_xy[0] - t.min_xy[0]) / t.cell)) + 1
    ny = int(np.ceil((t.max_xy[1] - t.min_xy[1]) / t.cell)) + 1
    assert terreno.triangle_count == 2 * (nx - 1) * (ny - 1)
    assert terreno.slopes.max() < 90.0  # ningún triángulo mira hacia abajo
    assert set(np.unique(terreno.colors)) <= set(PALETTE.values())


def test_triangulos_de_unos_tres_metros(terreno):
    v = terreno.verts
    edges = np.concatenate([
        np.linalg.norm(v[terreno.tris[:, a], :2] - v[terreno.tris[:, b], :2], axis=1)
        for a, b in ((0, 1), (1, 2), (2, 0))])
    assert 2.4 <= float(np.median(edges)) <= 3.2


def test_presupuesto_de_triangulos(terreno):
    assert terreno.triangle_count < 80_000


def test_fondo_somero_y_falda(layout, terreno):
    z = terreno.verts[:, 2]
    assert z.min() == pytest.approx(layout.terrain.skirt)
    interior = z[z > layout.terrain.skirt]
    assert interior.min() >= layout.terrain.seabed - 1e-6


def test_pendientes_fuertes_son_roca(terreno):
    dry = terreno.centroids[:, 2] > 0.0
    steep = terreno.slopes > ROCK_SLOPE
    assert steep[dry].sum() > 50
    assert set(np.unique(terreno.colors[dry & steep])) <= ROCK


def test_colores_por_lugar(layout, terreno):
    start = layout.points['player_start']
    assert terreno.colors[face_at(terreno, *start)] == PALETTE['arena_clara']
    assert terreno.colors[face_at(terreno, 30.0, -10.0)] in GRASS
    shore = terreno.colors[(terreno.centroids[:, 2] > -0.5) & (terreno.centroids[:, 2] < 0.3)]
    assert (shore == PALETTE['arena_humeda']).mean() > 0.8
    assert (terreno.colors == PALETTE['arena_oscura']).sum() > 100


def test_rocas_del_pescador_rocosas(layout, terreno):
    c = terreno.centroids
    in_zone = np.array([zone_at(layout, x, y) == 'rocas_pescador' for x, y in c[:, :2]]) & (c[:, 2] > 1.5)
    assert np.isin(terreno.colors[in_zone], list(ROCK)).mean() > 0.6


def test_caminos_transitables(layout, terreno):
    from explored_island.export import path_max_slope
    assert path_max_slope(terreno, layout) <= 30.0


def test_superficie_sigue_al_relieve(layout, terreno):
    for p in (layout.points['player_start'], layout.points['lighthouse'], (-30.0, -5.0)):
        assert surface_height(terreno, layout, *p) == pytest.approx(float(height_at(layout, *p)), abs=0.4)
    with pytest.raises(ValueError):
        surface_height(terreno, layout, 5000.0, 5000.0)


def test_determinista(layout, terreno):
    again = build_terrain(layout)
    assert np.array_equal(again.verts, terreno.verts)
    assert np.array_equal(again.colors, terreno.colors)


def _zone_colors(layout, terreno, zone_id):
    zone = layout.zone(zone_id)
    c = terreno.centroids
    inside = np.hypot(c[:, 0] - zone.center[0], c[:, 1] - zone.center[1]) <= zone.radius * 0.8
    land = inside & (c[:, 2] > 2.0) & np.isin(terreno.colors, list(GRASS))
    return terreno.colors[land]


def test_hierba_por_zona_y_tierra(layout, terreno):
    colina = _zone_colors(layout, terreno, 'colina_faro')
    palmar = _zone_colors(layout, terreno, 'palmar')
    assert (colina == PALETTE['hierba_seca']).mean() > 0.3          # colina clara y seca
    assert (palmar == PALETTE['hierba_seca']).sum() == 0
    oscuras = {PALETTE['hierba_oscura'], PALETTE['verde_selva']}
    assert np.isin(palmar, list(oscuras)).mean() > np.isin(colina, list(oscuras)).mean() + 0.3
    assert (terreno.colors == PALETTE['hierba_salvia']).sum() > (terreno.colors == PALETTE['hierba_clara']).sum()
    assert (terreno.colors == PALETTE['tierra']).sum() > 200          # caminos y manchas de tierra
