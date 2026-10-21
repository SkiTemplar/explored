import json

import numpy as np
import pytest
from explored_style.palette import PALETTE_PATH

from explored_island.layout import height_at, zone_at
from explored_island.props import STRUCTURE_TYPES
from explored_island.terrain import (
    FACET_MIX,
    PALETTE,
    ROCK_SLOPE,
    SMOOTH_RADIUS,
    build_terrain,
    corner_normals,
    smoothed_height,
    surface_height,
    terrain_height,
)

ROCK = {PALETTE['roca_media'], PALETTE['roca_clara'], PALETTE['roca_oscura']}
GRASS = {PALETTE[n] for n in ('hierba_clara', 'hierba_oscura', 'hierba_salvia', 'hierba_seca', 'verde_selva',
                              'verde_lima', 'tierra')}


def face_at(mesh, x, y):
    d = np.hypot(mesh.centroids[:, 0] - x, mesh.centroids[:, 1] - y)
    return int(np.argmin(d))


def grid_shape(layout):
    t = layout.terrain
    nx = int(np.ceil((t.max_xy[0] - t.min_xy[0]) / t.cell)) + 1
    ny = int(np.ceil((t.max_xy[1] - t.min_xy[1]) / t.cell)) + 1
    return nx, ny


def base_height(layout, x, y):
    """Altura del terreno de 8d3fa23b (relieve base filtrado, sin detalle)."""
    return float(smoothed_height(layout, np.array([x]), np.array([y]), SMOOTH_RADIUS * layout.terrain.cell)[0])


def test_indices_de_paleta_coinciden_con_tools_style():
    colores = {c['nombre']: c['indice'] for c in json.loads(PALETTE_PATH.read_text(encoding='utf-8'))['colores']}
    for nombre, indice in PALETTE.items():
        assert colores[nombre] == indice, nombre


def test_rejilla_completa_y_normales_hacia_arriba(layout, terreno):
    nx, ny = grid_shape(layout)
    assert terreno.triangle_count == 2 * (nx - 1) * (ny - 1)
    assert terreno.slopes.max() < 90.0  # ningún triángulo mira hacia abajo
    assert set(np.unique(terreno.colors)) <= set(PALETTE.values())


def test_triangulos_de_unos_dos_metros(terreno):
    v = terreno.verts
    edges = np.concatenate([
        np.linalg.norm(v[terreno.tris[:, a], :2] - v[terreno.tris[:, b], :2], axis=1)
        for a, b in ((0, 1), (1, 2), (2, 0))])
    assert 1.6 <= float(np.median(edges)) <= 2.3


def test_presupuesto_de_triangulos(terreno):
    assert terreno.triangle_count <= 200_000


def test_normales_por_esquina_mezclan_suave_y_cara(layout, props):
    from explored_island.export import linear_palette
    mesh = build_terrain(layout, props, linear_palette())
    assert np.allclose(np.linalg.norm(mesh.normals, axis=1), 1.0)
    assert (mesh.normals[:, 2] > 0.0).all()
    assert np.allclose(np.linalg.norm(mesh.corner_normals, axis=2), 1.0)
    assert 0.25 <= FACET_MIX <= 0.4
    # La normal de esquina queda entre la suave y la de cara: más cerca de la suave con FACET_MIX < 0,5.
    smooth = mesh.normals[mesh.tris]
    faces = np.cross(mesh.verts[mesh.tris[:, 1]] - mesh.verts[mesh.tris[:, 0]],
                     mesh.verts[mesh.tris[:, 2]] - mesh.verts[mesh.tris[:, 0]])
    faces = faces / np.linalg.norm(faces, axis=1, keepdims=True)
    to_smooth = np.einsum('ijk,ijk->ij', mesh.corner_normals, smooth)
    to_face = np.einsum('ijk,ik->ij', mesh.corner_normals, faces)
    assert (to_smooth >= to_face - 1e-9).mean() > 0.99
    # Fundido: hay colores por vértice que no son ninguno de la paleta (transiciones)...
    pal = linear_palette()
    nearest = np.min(np.linalg.norm(mesh.vertex_colors[:, None, :] - pal[None, :, :], axis=2), axis=1)
    assert (nearest > 0.01).mean() > 0.05
    # ...y todos quedan dentro de la envolvente de la paleta.
    assert mesh.vertex_colors.min() >= pal.min() - 1e-9 and mesh.vertex_colors.max() <= pal.max() + 1e-9


def test_mezcla_de_normales_en_los_extremos():
    smooth = np.array([[0.0, 0.0, 1.0], [0.0, 0.0, 1.0], [0.0, 0.0, 1.0]])
    tris = np.array([[0, 1, 2]])
    face = np.array([[0.6, 0.0, 0.8]])
    assert np.allclose(corner_normals(smooth, face, tris, 0.0)[0], smooth)
    assert np.allclose(corner_normals(smooth, face, tris, 1.0)[0], np.repeat(face, 3, axis=0))


def test_relieve_con_detalle_de_media_escala(layout, terreno):
    # Sobre el relieve base de 8d3fa23b hay ondulaciones de 0,2-1,2 m (más en la colina que en la cala).
    land = terreno.base > 2.0
    dz = terreno.verts[:, 2] - terreno.base
    assert 0.25 <= float(dz[land].std()) <= 0.8
    assert float(np.percentile(np.abs(dz[land]), 99)) <= 1.6

    def spread(zone_id):
        zone = layout.zone(zone_id)
        inside = np.hypot(terreno.verts[:, 0] - zone.center[0], terreno.verts[:, 1] - zone.center[1]) < zone.radius
        return float(dz[inside & land].std())

    assert spread('colina_faro') > spread('cala_varada')


def test_relieve_sin_escalones(layout, terreno):
    _, ny = grid_shape(layout)
    z = terreno.verts[:, 2].reshape(-1, ny)
    dry = z[1:-1, 1:-1] > 0.5
    step = np.abs(np.diff(z, axis=0))[1:, 1:-1]
    assert 0.1 < float(np.median(step[dry])) < 0.6   # más relieve que 8d3fa23b (< 0,3) sin facetas bruscas
    assert step[dry].max() < 3.0


def test_fondo_somero_y_falda(layout, terreno):
    z = terreno.verts[:, 2]
    assert z.min() == pytest.approx(layout.terrain.skirt)
    interior = z[z > layout.terrain.skirt]
    assert interior.min() >= layout.terrain.seabed - 1e-6


def test_la_costa_no_se_mueve(layout, props):
    # Por rayos desde el centro de cada tierra: la última orilla con tierra se desplaza ≤ 1 m.
    worst = 0.0
    for island in layout.islands:
        for angle in np.radians(np.arange(0.0, 360.0, 4.0)):
            r = np.arange(island.minor_radius * 0.8, island.radius * 1.35, 0.1)
            x = island.center[0] + r * np.cos(angle)
            y = island.center[1] + r * np.sin(angle)
            old = smoothed_height(layout, x, y, SMOOTH_RADIUS * layout.terrain.cell)
            new = terrain_height(layout, props, x, y)
            if (old > 0).any() and (new > 0).any():
                worst = max(worst, abs(r[np.flatnonzero(old > 0).max()] - r[np.flatnonzero(new > 0).max()]))
    assert worst <= 1.0


def test_puntos_de_autor_y_estructuras_a_la_misma_altura(layout, props, terreno):
    spots = list(layout.points.values())
    spots += [tuple(p['at']) for p in props['props'] if 'anchor' not in p and p['type'] in STRUCTURE_TYPES]
    assert len(spots) > 10
    for x, y in spots:
        assert surface_height(terreno, layout, x, y) == pytest.approx(base_height(layout, x, y), abs=0.3), (x, y)


def test_pendientes_fuertes_son_roca(terreno):
    dry = terreno.centroids[:, 2] > 0.0
    steep = terreno.slopes > ROCK_SLOPE
    assert steep[dry].sum() > 50
    assert set(np.unique(terreno.colors[dry & steep])) <= ROCK


def test_colores_por_lugar(layout, terreno):
    start = layout.points['player_start']
    assert terreno.colors[face_at(terreno, *start)] in {PALETTE['arena_clara'], PALETTE['arena_oscura']}
    assert terreno.colors[face_at(terreno, -45.0, -20.0)] in GRASS
    shore = terreno.colors[(terreno.centroids[:, 2] > -0.5) & (terreno.centroids[:, 2] < 0.3)]
    assert (shore == PALETTE['arena_humeda']).mean() > 0.8
    assert (terreno.colors == PALETTE['arena_oscura']).sum() > 100


def test_rocas_del_pescador_rocosas(layout, terreno):
    c = terreno.centroids
    in_zone = np.array([zone_at(layout, x, y) == 'rocas_pescador' for x, y in c[:, :2]]) & (c[:, 2] > 1.5)
    assert np.isin(terreno.colors[in_zone], list(ROCK)).mean() > 0.6


def test_caminos_transitables_y_hundidos(layout, terreno):
    from explored_island.export import path_max_slope
    from explored_island.layout import distance_to_paths
    assert path_max_slope(terreno, layout) <= 28.0
    v = terreno.verts
    d = distance_to_paths(layout, v[:, 0], v[:, 1])
    inland = terreno.base > 3.0
    # Hundidos respecto al relieve base y de tierra en el centro.
    assert float(np.median((v[:, 2] - terreno.base)[(d < -0.5) & inland])) < -0.05
    c = terreno.centroids
    on = distance_to_paths(layout, c[:, 0], c[:, 1]) < -0.4
    assert (terreno.colors[on & (c[:, 2] > 3.0)] == PALETTE['tierra']).mean() > 0.8


def test_superficie_sigue_al_relieve(layout, terreno):
    for p in (layout.points['player_start'], layout.points['lighthouse'], (-30.0, -5.0)):
        assert surface_height(terreno, layout, *p) == pytest.approx(float(height_at(layout, *p)), abs=0.4)
    with pytest.raises(ValueError):
        surface_height(terreno, layout, 5000.0, 5000.0)


def test_determinista(layout, props, terreno):
    again = build_terrain(layout, props)
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


def test_variacion_de_color_de_alta_frecuencia(layout, terreno):
    # En la hierba, el tono cambia cada pocas celdas (racha media ≤ 4 celdas = 7,2 m), no en manchas de 20 m.
    nx, ny = grid_shape(layout)
    centroid_rows = terreno.colors[:terreno.triangle_count // 2].reshape(nx - 1, ny - 1)
    z = terreno.centroids[:terreno.triangle_count // 2, 2].reshape(nx - 1, ny - 1)
    grass = np.isin(centroid_rows, list(GRASS)) & (z > 3.0)
    changes = (centroid_rows[:, 1:] != centroid_rows[:, :-1]) & grass[:, 1:] & grass[:, :-1]
    pairs = (grass[:, 1:] & grass[:, :-1]).sum()
    assert pairs / max(int(changes.sum()), 1) <= 4.0


def test_mancha_de_tierra_bajo_las_palmeras(layout, props):
    palms = np.array([[-40.0, -20.0]])
    with_palm = build_terrain(layout, props, palms=palms)
    c = with_palm.centroids
    near = np.hypot(c[:, 0] + 40.0, c[:, 1] + 20.0) < 1.5
    ring = (np.hypot(c[:, 0] + 40.0, c[:, 1] + 20.0) > 3.5) & (np.hypot(c[:, 0] + 40.0, c[:, 1] + 20.0) < 4.5)
    assert (with_palm.colors[near] == PALETTE['tierra']).all()
    assert (with_palm.colors[ring] == PALETTE['tierra']).mean() < 0.5
