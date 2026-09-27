"""
Sotobosque — Tools/Blender/assets/shrub.py

Segunda pasada de arte (2026-09-26): 6 plantas — helecho arborescente,
helecho de suelo (más grande y denso), platanera (con alguna hoja
rasgada), monstera/oreja de elefante, arbusto con flores (heliconia/
hibisco) y bambú en mata (más alto y denso). Presupuesto orientativo:
1 000-9 000 triángulos.

3ª pasada (encargo del autor, mejora de vegetación): añade pandano
(Pandanus, roseta de hojas en espiral filotáctica sobre un tallo corto) y
sube la densidad de folíolos/hojas del resto para que ninguna especie del
kit se lea como «palo con pocas hojas sueltas».
"""

import os
import sys
import math

import bpy

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import common as C  # noqa: E402

CATEGORY = 'shrub'

VARIANTS = [
    dict(name='ShrubFernTree', index=1, seed=3001, kind='fern_tree'),
    dict(name='ShrubFernGround', index=1, seed=3002, kind='fern_ground'),
    dict(name='ShrubBanana', index=1, seed=3003, kind='banana'),
    dict(name='ShrubMonstera', index=1, seed=3004, kind='monstera'),
    dict(name='ShrubFlowering', index=1, seed=3005, kind='flowering'),
    dict(name='ShrubBamboo', index=1, seed=3006, kind='bamboo'),
    dict(name='ShrubPandanus', index=1, seed=3007, kind='pandanus'),
]


def _tear_leaf_edges(obj, segments, rnd, tear_count=2, depth_ratio=0.5):
    """Pellizca 1-2 vértices del borde hacia la nervadura central para
    simular un desgarro de viento en una hoja grande (platanera): barato
    (no añade triángulos) y evita geometría degenerada porque nunca cierra
    del todo el hueco."""
    verts = obj.data.vertices
    rows = segments + 1
    for _ in range(tear_count):
        row = rnd.randint(1, max(1, rows - 2))
        side = rnd.choice([0, 1])
        idx = 2 * row + side
        if idx < len(verts):
            verts[idx].co.x *= (1.0 - depth_ratio)
    obj.data.update()


def _build_fern_tree(rnd, seed):
    """Helecho arborescente: tronco fibroso de 2-4 m rematado por un
    penacho de frondas, igual que una palmera pequeña pero con frondas más
    finas, numerosas y menos rígidas."""
    parts = []
    height = rnd.uniform(2.0, 4.0)
    base_radius = rnd.uniform(0.09, 0.14)
    trunk, lean_dir = C.make_curved_trunk(
        'Trunk', height, base_radius, base_radius * 0.85, curvature=height * 0.04,
        n_points=7, bevel_resolution=3, wobble=height * 0.006, rnd=rnd,
    )
    C.add_ring_bumps(trunk, spacing=height / rnd.uniform(10, 14),
                      amplitude=base_radius * 0.12, rnd=rnd, sharpness=4)
    C.assign_materials(trunk, ['M_Bark'])
    C.set_vertex_colors(trunk, C.bark_streaks_tint(
        (0.14, 0.11, 0.08), (0.24, 0.19, 0.13), height, seed, streak_count=14))
    parts.append(trunk)

    crown = C.spline_point(height, 1.0, height * 0.04, lean_dir)
    n_fronds = rnd.randint(12, 17)
    frond_len = rnd.uniform(1.0, 1.7)
    for i in range(n_fronds):
        ang = (2.0 * math.pi * i / n_fronds) + rnd.uniform(-0.25, 0.25)
        elevation = rnd.uniform(math.radians(10), math.radians(45))
        frond = C.make_frond_object(
            f'Frond_{i:02d}', length=frond_len * rnd.uniform(0.85, 1.1),
            width=frond_len * 0.30, leaflet_count=rnd.randint(22, 30),
            droop=frond_len * rnd.uniform(0.30, 0.5), seed=seed * 10 + i,
            curl=0.2, rachis_width_ratio=0.014, leaflet_uv_cell='fern_leaflet',
        )
        forward = C.Vector((math.cos(ang), math.sin(ang), math.sin(elevation) * 1.3))
        C.orient_and_place(frond, crown, forward, C.Vector((0, 0, 1)))
        dark = (0.075 + rnd.uniform(-0.01, 0.02), 0.20 + rnd.uniform(-0.02, 0.02), 0.07)
        light = (0.20 + rnd.uniform(-0.02, 0.03), 0.36, 0.14)
        C.assign_materials(frond, ['M_Leaf'])
        C.set_vertex_colors(frond, C.gradient_along_axis(
            dark, light, 'y', 0.0, frond_len, curve=0.8, jitter=0.03, rnd=rnd))
        parts.append(frond)
    return parts


def _build_fern_ground(rnd, seed):
    """Helecho de suelo, ahora más grande y denso: 10-16 frondas de
    0,8-1,5 m abriéndose en abanico desde un rizoma."""
    parts = []
    n_fronds = rnd.randint(12, 16)
    length = rnd.uniform(0.8, 1.5)
    for i in range(n_fronds):
        ang = (2.0 * math.pi * i / n_fronds) + rnd.uniform(-0.35, 0.35)
        frond = C.make_frond_object(
            f'Frond_{i:02d}', length=length * rnd.uniform(0.8, 1.15),
            width=length * 0.40, leaflet_count=rnd.randint(20, 26),
            droop=length * rnd.uniform(0.05, 0.20), seed=seed * 10 + i,
            curl=0.25, rachis_width_ratio=0.014, leaflet_uv_cell='fern_leaflet',
        )
        elevation = rnd.uniform(math.radians(15), math.radians(55))
        forward = C.Vector((math.cos(ang), math.sin(ang), math.sin(elevation) * 1.6))
        C.orient_and_place(frond, C.Vector((0, 0, 0.02)), forward, C.Vector((0, 0, 1)))
        dark = (0.08, 0.19 + rnd.uniform(-0.02, 0.02), 0.07)
        light = (0.22, 0.37, 0.15)
        C.assign_materials(frond, ['M_Leaf'])
        C.set_vertex_colors(frond, C.gradient_along_axis(
            dark, light, 'y', 0.0, length, curve=0.8, jitter=0.04, rnd=rnd))
        parts.append(frond)
    return parts


def _build_banana(rnd, seed):
    """Platanera: pseudotallo grueso y corto con una mata REDONDEADA de
    10-14 hojas anchas en abanico (algunas rasgadas por el viento).

    2ª pasada de arte (encargo 2026-09-27: el «arbusto en V» rechazado):
    con 6-9 hojas de anchura moderada (0.34x su longitud) repartidas en
    círculo completo, desde CUALQUIER cámara fija la mayoría quedan casi
    de canto (tarjetas planas de un solo lado, double_sided=False) y se
    vuelven casi invisibles — solo se leían 1-2 hojas de frente, en forma
    de V. Ahora las hojas son mucho más anchas (0.62x), van a dos alturas
    del pseudotallo (rosetón, no un único punto) y son de dos caras, para
    que la silueta se lea llena y redondeada desde cualquier ángulo."""
    parts = []
    height = rnd.uniform(1.6, 2.4)
    base_radius = rnd.uniform(0.14, 0.20)
    trunk, lean_dir = C.make_curved_trunk(
        'Pseudostem', height, base_radius, base_radius * 0.7, curvature=height * 0.03,
        n_points=6, bevel_resolution=4, rnd=rnd,
    )
    C.assign_materials(trunk, ['M_Bark'])
    C.set_vertex_colors(trunk, C.bark_streaks_tint(
        (0.16, 0.20, 0.10), (0.30, 0.38, 0.20), height, seed, streak_count=16))
    parts.append(trunk)

    n_leaves = rnd.randint(10, 14)
    for i in range(n_leaves):
        leaf_len = rnd.uniform(1.4, 2.3)
        segments = 8
        leaf = C.make_leaf_blade(
            f'Leaf_{i:02d}', length=leaf_len, width_base=leaf_len * 0.62,
            width_tip=leaf_len * 0.08, curve_amount=leaf_len * 0.34,
            segments=segments, double_sided=True, uv_cell='banana_leaf',
        )
        if rnd.random() < 0.4:
            _tear_leaf_edges(leaf, segments, rnd, tear_count=1,
                              depth_ratio=rnd.uniform(0.25, 0.4))
        t_stem = rnd.uniform(0.75, 1.0)
        origin = C.spline_point(height, t_stem, height * 0.03, lean_dir)
        ang = (2.0 * math.pi * i / n_leaves) + rnd.uniform(-0.2, 0.2)
        # elevación con caída: la mitad más nueva del rosetón se abre hacia
        # arriba, la otra mitad ya cuelga hacia fuera/abajo por su propio
        # peso -silueta redondeada tipo fuente, no un candelabro apuntando
        # todo hacia arriba.
        elevation = rnd.uniform(math.radians(-18), math.radians(38))
        forward = C.Vector((math.cos(ang), math.sin(ang), math.sin(elevation)))
        C.orient_and_place(leaf, origin, forward, C.Vector((0, 0, 1)))
        dark = (0.07, 0.22 + rnd.uniform(-0.02, 0.02), 0.08)
        light = (0.26, 0.56, 0.16)
        C.assign_materials(leaf, ['M_Leaf'])
        C.set_vertex_colors(leaf, C.gradient_along_axis(
            dark, light, 'y', 0.0, leaf_len, curve=1.0, jitter=0.03, rnd=rnd))
        parts.append(leaf)
    return parts


def _build_monstera(rnd, seed):
    """Monstera / oreja de elefante: 6-10 hojas grandes acorazonadas sobre
    pecíolos largos (sin perforaciones reales: sería un booleano, se deja
    como simplificación conocida — ver informe)."""
    parts = []
    n_leaves = rnd.randint(6, 10)
    for i in range(n_leaves):
        petiole_h = rnd.uniform(0.45, 0.75)
        petiole, _ = C.make_curved_trunk(
            f'Petiole_{i:02d}', height=petiole_h, base_radius=rnd.uniform(0.015, 0.024),
            tip_radius=rnd.uniform(0.010, 0.016), curvature=petiole_h * rnd.uniform(0.15, 0.3),
            n_points=5, bevel_resolution=2, rnd=rnd,
            uv_rect=C.atlas_uv_rect('stem_swatch'),  # material M_Grass: ver nota de uv_rect
        )
        C.assign_materials(petiole, ['M_Grass'])
        C.set_vertex_colors(petiole, C.constant_tint((0.22, 0.38, 0.15), alpha=0.0, jitter=0.02, rnd=rnd))
        ang = (2.0 * math.pi * i / n_leaves) + rnd.uniform(-0.3, 0.3)
        C.orient_and_place(petiole, C.Vector((0, 0, 0)),
                            C.Vector((math.cos(ang) * 0.3, math.sin(ang) * 0.3, 1.0)),
                            C.Vector((0, 0, 1)))
        parts.append(petiole)

        leaf_len = rnd.uniform(0.6, 1.0)
        leaf = C.make_leaf_blade(
            f'Blade_{i:02d}', length=leaf_len, width_base=leaf_len * 0.72,
            width_tip=leaf_len * 0.5, curve_amount=leaf_len * 0.22,
            segments=5, double_sided=False, uv_cell='monstera_leaf',
        )
        top = C.Vector((math.cos(ang) * petiole_h * 0.3, math.sin(ang) * petiole_h * 0.3, petiole_h))
        tilt = rnd.uniform(math.radians(30), math.radians(60))
        forward = C.Vector((math.cos(ang), math.sin(ang), -math.sin(tilt) * 0.3))
        C.orient_and_place(leaf, top, forward, C.Vector((0, 0, 1)))
        dark = (0.075, 0.22 + rnd.uniform(-0.02, 0.02), 0.09)
        light = (0.20, 0.40, 0.17)
        C.assign_materials(leaf, ['M_Leaf'])
        C.set_vertex_colors(leaf, C.gradient_along_axis(
            dark, light, 'y', 0.0, leaf_len, curve=1.0, jitter=0.04, rnd=rnd))
        parts.append(leaf)
    return parts


def _build_flowering(rnd, seed):
    """Arbusto con flores tipo heliconia/hibisco: base de hojas anchas y
    una o dos espigas de brácteas rojas/amarillas apiladas en zigzag."""
    parts = []
    trunk, lean_dir = C.make_curved_trunk(
        'Stem', height=rnd.uniform(0.6, 0.9), base_radius=rnd.uniform(0.022, 0.032),
        tip_radius=rnd.uniform(0.010, 0.016), curvature=rnd.uniform(0.06, 0.12),
        n_points=5, bevel_resolution=3, rnd=rnd,
    )
    C.assign_materials(trunk, ['M_Bark'])
    C.set_vertex_colors(trunk, C.constant_tint((0.20, 0.15, 0.10), alpha=0.0, jitter=0.02, rnd=rnd))
    stem_top = trunk.dimensions.z
    parts.append(trunk)

    n_leaves = rnd.randint(9, 13)
    for i in range(n_leaves):
        leaf_len = rnd.uniform(0.30, 0.5)
        leaf = C.make_leaf_blade(
            f'Leaf_{i:02d}', length=leaf_len, width_base=leaf_len * 0.30,
            width_tip=leaf_len * 0.06, curve_amount=leaf_len * 0.25,
            segments=6, double_sided=True, uv_cell='shrub_flower_leaf',
        )
        ang = rnd.uniform(0, 2 * math.pi)
        elevation = rnd.uniform(math.radians(20), math.radians(60))
        origin = C.Vector((0, 0, rnd.uniform(stem_top * 0.3, stem_top * 0.9)))
        forward = C.Vector((math.cos(ang), math.sin(ang), math.sin(elevation)))
        C.orient_and_place(leaf, origin, forward, C.Vector((0, 0, 1)))
        dark = (0.07, 0.22, 0.09)
        light = (0.18, 0.36, 0.15)
        C.assign_materials(leaf, ['M_Leaf'])
        C.set_vertex_colors(leaf, C.gradient_along_axis(
            dark, light, 'y', 0.0, leaf_len, curve=0.9, jitter=0.03, rnd=rnd))
        parts.append(leaf)

    hue = rnd.choice([
        ((0.42, 0.03, 0.02), (0.95, 0.55, 0.08)),   # rojo -> naranja/amarillo (heliconia)
        ((0.55, 0.05, 0.10), (0.92, 0.20, 0.30)),   # rojo oscuro -> rojo vivo (hibisco)
    ])
    n_spikes = rnd.randint(2, 3)
    for s in range(n_spikes):
        spike_ang = rnd.uniform(0, 2 * math.pi)
        spike_h = rnd.uniform(0.35, 0.55)
        n_bracts = rnd.randint(5, 7)
        for b in range(n_bracts):
            t = b / max(1, n_bracts - 1)
            bract_len = rnd.uniform(0.14, 0.22) * (1.0 - 0.25 * t)
            bract = C.make_leaf_blade(
                f'Bract_{s}_{b:02d}', length=bract_len, width_base=bract_len * 0.5,
                width_tip=bract_len * 0.1, curve_amount=bract_len * 0.6, segments=3,
                double_sided=True, uv_cell='flower_petal',
            )
            side = 1 if b % 2 == 0 else -1
            origin = C.Vector((math.cos(spike_ang) * 0.02 * side, math.sin(spike_ang) * 0.02 * side,
                                stem_top + spike_h * t))
            forward = C.Vector((math.cos(spike_ang) * side, math.sin(spike_ang) * side, 0.35))
            C.orient_and_place(bract, origin, forward, C.Vector((0, 0, 1)))
            C.assign_materials(bract, ['M_Leaf'])
            C.set_vertex_colors(bract, C.gradient_along_axis(
                hue[0], hue[1], 'y', 0.0, bract_len, curve=1.0, jitter=0.02, rnd=rnd))
            parts.append(bract)
    return parts


def _build_bamboo(rnd, seed):
    """Mata de bambú más alta y densa: 12-20 cañas de 5-8 m."""
    parts = []
    n_culms = rnd.randint(12, 16)
    for i in range(n_culms):
        h = rnd.uniform(5.0, 8.0)
        base_r = rnd.uniform(0.035, 0.06)
        culm, lean_dir = C.make_curved_trunk(
            f'Culm_{i:02d}', height=h, base_radius=base_r, tip_radius=base_r * 0.6,
            curvature=h * rnd.uniform(0.02, 0.06), n_points=5, bevel_resolution=2,
            wobble=h * 0.004, rnd=rnd,
            uv_rect=C.atlas_uv_rect('stem_swatch'),  # material M_Grass: ver nota de uv_rect
        )
        C.add_ring_bumps(culm, spacing=h / rnd.uniform(9, 13), amplitude=base_r * 0.30,
                          rnd=rnd, sharpness=8)
        ang = rnd.uniform(0, 2 * math.pi)
        r = rnd.uniform(0.0, 0.55)
        offset = C.Vector((math.cos(ang) * r, math.sin(ang) * r, 0.0))
        culm.location = offset
        C.select_only(culm)
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        C.assign_materials(culm, ['M_Grass'])
        C.set_vertex_colors(culm, C.gradient_along_axis(
            (0.42, 0.50, 0.20), (0.68, 0.72, 0.38), 'z', 0.0, h, curve=1.0, jitter=0.03, rnd=rnd))
        parts.append(culm)

        top = C.spline_point(h, 1.0, h * 0.04, lean_dir) + offset
        n_leaves = rnd.randint(3, 5)
        for j in range(n_leaves):
            leaf_len = rnd.uniform(0.22, 0.38)
            leaf = C.make_leaf_blade(
                f'BambooLeaf_{i:02d}_{j:02d}', length=leaf_len, width_base=leaf_len * 0.16,
                width_tip=leaf_len * 0.01, curve_amount=leaf_len * 0.35, segments=3,
                double_sided=False, uv_cell='bamboo_leaf',
            )
            lang = rnd.uniform(0, 2 * math.pi)
            forward = C.Vector((math.cos(lang), math.sin(lang), rnd.uniform(0.1, 0.5)))
            C.orient_and_place(leaf, top, forward, C.Vector((0, 0, 1)))
            C.assign_materials(leaf, ['M_Leaf'])
            C.set_vertex_colors(leaf, C.gradient_along_axis(
                (0.16, 0.32, 0.12), (0.34, 0.50, 0.20), 'y', 0.0, leaf_len,
                curve=0.9, jitter=0.03, rnd=rnd))
            parts.append(leaf)
    return parts


def _build_pandanus(rnd, seed):
    """Pandano (Pandanus, «palmera de tornillo»): roseta en espiral
    filotáctica de hojas largas en espada sobre un tallo corto y grueso —
    reconocible por la espiral, a diferencia del abanico plano de la
    platanera o el penacho radial de la palmera."""
    parts = []
    stem_h = rnd.uniform(0.5, 0.9)
    stem, lean_dir = C.make_curved_trunk(
        'Stem', stem_h, rnd.uniform(0.10, 0.14), rnd.uniform(0.08, 0.11),
        curvature=stem_h * 0.05, n_points=5, bevel_resolution=3, rnd=rnd,
    )
    C.assign_materials(stem, ['M_Bark'])
    C.set_vertex_colors(stem, C.bark_streaks_tint(
        (0.15, 0.12, 0.08), (0.27, 0.22, 0.14), stem_h, seed, streak_count=10))
    parts.append(stem)

    top = C.spline_point(stem_h, 1.0, stem_h * 0.05, lean_dir)
    n_leaves = rnd.randint(22, 28)
    golden_angle = math.pi * (3.0 - math.sqrt(5.0))  # ángulo áureo: espiral filotáctica
    for i in range(n_leaves):
        leaf_len = rnd.uniform(0.9, 1.5)
        leaf = C.make_leaf_blade(
            f'Leaf_{i:02d}', length=leaf_len, width_base=leaf_len * 0.09,
            width_tip=leaf_len * 0.015, curve_amount=leaf_len * 0.22,
            segments=7, double_sided=True, uv_cell='pandanus_leaf',
        )
        ang = i * golden_angle
        elevation = rnd.uniform(math.radians(20), math.radians(50))
        forward = C.Vector((math.cos(ang), math.sin(ang), math.sin(elevation)))
        C.orient_and_place(leaf, top, forward, C.Vector((0, 0, 1)))
        C.assign_materials(leaf, ['M_Leaf'])
        C.set_vertex_colors(leaf, C.gradient_along_axis(
            (0.09, 0.28, 0.11), (0.22, 0.46, 0.18), 'y', 0.0, leaf_len,
            curve=0.85, jitter=0.03, rnd=rnd))
        parts.append(leaf)
    return parts


_BUILDERS = {
    'fern_tree': _build_fern_tree,
    'fern_ground': _build_fern_ground,
    'banana': _build_banana,
    'monstera': _build_monstera,
    'flowering': _build_flowering,
    'bamboo': _build_bamboo,
    'pandanus': _build_pandanus,
}


def build(variant):
    rnd = C.seeded_rng(variant['seed'])
    parts = _BUILDERS[variant['kind']](rnd, variant['seed'])
    obj = C.join_objects(parts, 'SM_' + variant['name'])
    C.shade_smooth_auto(obj, angle_deg=45.0)
    return obj
