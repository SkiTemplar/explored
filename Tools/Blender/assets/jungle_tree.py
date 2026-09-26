"""
Árboles de selva — Tools/Blender/assets/jungle_tree.py

4 especies distintas con ramificación real (no «piruletas»): un gigante de
dosel con contrafuertes, un mediano de copa ancha, un manglar con raíces
aéreas en arco que lo levantan del suelo, y un delgado de sotobosque con
hojas grandes. Las copas son cúmulos de «blobs» de hoja irregulares y algo
aplanados repartidos en las puntas de las ramas — nunca una esfera lisa.

Segunda pasada de arte (2026-09-26): la primera versión colocaba las ramas
con orient_and_place (pensado para geometría que crece en +Y, como hojas y
frondas) sobre troncos de make_curved_trunk, que crecen en +Z. Eso hacía
que las ramas apuntaran hacia «up» en vez de abrirse hacia los lados: el
árbol quedaba con un tronco recto y una bola encima («piruleta»). Se usa
ahora orient_and_place_zaxis, pensado para ese caso.

Presupuesto orientativo: 6 000-25 000 triángulos (el gigante, con Nanite,
puede llegar al máximo).
"""

import os
import sys
import math

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import common as C  # noqa: E402

CATEGORY = 'tree'

VARIANTS = [
    dict(name='JungleTreeGiant', index=1, seed=2001, kind='giant'),
    dict(name='JungleTreeWide', index=1, seed=2002, kind='wide'),
    dict(name='JungleTreeMangrove', index=1, seed=2003, kind='mangrove'),
    dict(name='JungleTreeUnderstory', index=1, seed=2004, kind='understory'),
]


def _leaf_palette(rnd):
    """Paleta natural y ligeramente cálida (nunca verde saturado plano):
    base oscura y olivácea, punta más clara y algo más cálida, con
    variación aleatoria pequeña para que cada cúmulo no sea idéntico."""
    warm = rnd.uniform(-0.02, 0.05)
    dark = (0.085 + warm * 0.6, 0.185 + rnd.uniform(-0.02, 0.015), 0.065 + warm * 0.25)
    light = (0.23 + warm, 0.38 + rnd.uniform(-0.03, 0.03), 0.14 + warm * 0.4)
    return dark, light


def _tint_canopy_part(obj, rnd):
    dark, light = _leaf_palette(rnd)
    zs = [v.co.z for v in obj.data.vertices]
    z0, z1 = (min(zs), max(zs)) if zs else (0.0, 1.0)
    C.set_vertex_colors(obj, C.gradient_along_axis(dark, light, 'z', z0, z1,
                                                     curve=0.85, jitter=0.03, rnd=rnd))


def _canopy_lobe(name, center, radius_xy, radius_z, seed, rnd):
    """Un cúmulo de hoja irregular y algo aplanado (2-4 blobs fundidos),
    con degradado propio de tono. Es la unidad que se reparte en las
    puntas de las ramas para dar una copa en capas, no una esfera única."""
    lobe = C.make_canopy_blobs(
        name, center=center, radius_xy=radius_xy, radius_z=radius_z * 0.6,
        count=rnd.randint(2, 4), seed=seed,
        blob_scale_range=(0.5, 0.85), subdivisions=2, relax_iterations=2,
    )
    C.assign_materials(lobe, ['M_Leaf'])
    _tint_canopy_part(lobe, rnd)
    return lobe


def _branch_system(top, base_radius, ref_height, rnd, seed,
                    n_main=(3, 5), main_len_ratio=(0.30, 0.50),
                    elevation_range=(20, 55), sub_prob=0.6, sub_count=(1, 2),
                    sub_len_ratio=(0.40, 0.65), sub_elevation_range=(10, 45)):
    """Ramas principales desde el tercio superior del tronco, cada una con
    posibles subramas. Devuelve (partes_de_madera, lista_de_puntas) donde
    cada punta es (posición_mundo, dirección) lista para colgar una copa.
    """
    parts = []
    tips = []
    n = rnd.randint(*n_main)
    for i in range(n):
        ang = (2.0 * math.pi * i / n) + rnd.uniform(-0.35, 0.35)
        elevation = math.radians(rnd.uniform(*elevation_range))
        b_len = ref_height * rnd.uniform(*main_len_ratio)
        b_radius = base_radius * rnd.uniform(0.30, 0.48)
        direction = C.Vector((math.cos(ang), math.sin(ang), math.sin(elevation)))

        branch, _ = C.make_curved_trunk(
            f'Branch_{i:02d}', height=b_len, base_radius=b_radius,
            tip_radius=b_radius * 0.35, curvature=b_len * rnd.uniform(0.25, 0.45),
            n_points=6, bevel_resolution=3, wobble=b_len * 0.01, rnd=rnd,
        )
        C.orient_and_place_zaxis(branch, top, direction, C.Vector((0, 0, 1)))
        C.assign_materials(branch, ['M_Bark'])
        C.set_vertex_colors(branch, C.bark_streaks_tint(
            (0.15, 0.10, 0.07), (0.27, 0.20, 0.13), b_len, seed * 10 + i, streak_count=6))
        parts.append(branch)

        tip_pos = top + direction.normalized() * b_len

        if rnd.random() < sub_prob:
            n_sub = rnd.randint(*sub_count)
            for j in range(n_sub):
                s_ang = ang + rnd.uniform(-1.1, 1.1)
                s_elev = math.radians(rnd.uniform(*sub_elevation_range))
                s_len = b_len * rnd.uniform(*sub_len_ratio)
                s_radius = b_radius * 0.55
                s_dir = C.Vector((math.cos(s_ang), math.sin(s_ang), math.sin(s_elev)))
                s_dir = (direction.normalized() * 0.5 + s_dir.normalized() * 0.85)
                subbranch, _ = C.make_curved_trunk(
                    f'SubBranch_{i:02d}_{j:02d}', height=s_len, base_radius=s_radius,
                    tip_radius=s_radius * 0.4, curvature=s_len * rnd.uniform(0.2, 0.4),
                    n_points=5, bevel_resolution=3, rnd=rnd,
                )
                C.orient_and_place_zaxis(subbranch, tip_pos, s_dir, C.Vector((0, 0, 1)))
                C.assign_materials(subbranch, ['M_Bark'])
                C.set_vertex_colors(subbranch, C.bark_streaks_tint(
                    (0.15, 0.10, 0.07), (0.27, 0.20, 0.13), s_len,
                    seed * 100 + i * 10 + j, streak_count=5))
                parts.append(subbranch)
                tips.append((tip_pos + s_dir.normalized() * s_len, s_dir))
        else:
            tips.append((tip_pos, direction))

    return parts, tips


def _buttress_roots(trunk_radius, rnd, seed, count, height_ratio=(3.6, 5.2)):
    """Raíces tabulares: aletas planas y curvadas que suben desde el suelo
    y se estrechan hacia el tronco (dosel alto de selva). height_ratio se
    aplica sobre el radio del tronco: cuanto más grueso, más altas."""
    fins = []
    fin_height = trunk_radius * rnd.uniform(*height_ratio)
    for i in range(count):
        ang = (2.0 * math.pi * i / count) + rnd.uniform(-0.15, 0.15)
        fin_width_ground = trunk_radius * rnd.uniform(1.7, 2.6)
        fin = C.make_leaf_blade(
            f'Root_{i:02d}', length=fin_height, width_base=fin_width_ground,
            width_tip=trunk_radius * 0.12, curve_amount=fin_height * 0.30,
            segments=5, double_sided=False,
        )
        forward = C.Vector((0.0, 0.0, 1.0))
        up = C.Vector((math.cos(ang), math.sin(ang), 0.12))
        C.orient_and_place(fin, C.Vector((0, 0, 0)), forward, up)
        C.assign_materials(fin, ['M_Bark'])
        C.set_vertex_colors(fin, C.bark_streaks_tint(
            (0.13, 0.09, 0.06), (0.24, 0.17, 0.10), fin_height, seed + i, streak_count=4))
        fins.append(fin)
    return fins


def _mangrove_stilt_roots(lift_height, spread, base_radius, rnd, seed, count):
    """Raíces zancudas en arco: piernas curvadas que salen de la parte
    alta del tocón y bajan y se abren hasta el suelo, levantando el tronco
    visible del suelo (característica de Rhizophora)."""
    legs = []
    for i in range(count):
        ang = (2.0 * math.pi * i / count) + rnd.uniform(-0.2, 0.2)
        attach_h = lift_height * rnd.uniform(0.6, 0.95)
        attach = C.Vector((math.cos(ang) * base_radius * 0.55,
                            math.sin(ang) * base_radius * 0.55, attach_h))
        ground = C.Vector((math.cos(ang) * spread, math.sin(ang) * spread, 0.0))
        direction = ground - attach
        leg_len = direction.length * rnd.uniform(1.08, 1.25)
        leg_radius = base_radius * rnd.uniform(0.16, 0.24)

        leg, _ = C.make_curved_trunk(
            f'StiltRoot_{i:02d}', height=leg_len, base_radius=leg_radius,
            tip_radius=leg_radius * 0.55, curvature=leg_len * rnd.uniform(0.30, 0.45),
            n_points=7, bevel_resolution=3, wobble=leg_len * 0.01, rnd=rnd,
        )
        C.orient_and_place_zaxis(leg, attach, direction, C.Vector((0, 0, 1)))
        C.assign_materials(leg, ['M_Bark'])
        C.set_vertex_colors(leg, C.bark_streaks_tint(
            (0.12, 0.10, 0.08), (0.23, 0.20, 0.15), leg_len, seed * 10 + i, streak_count=5))
        legs.append(leg)
    return legs


def _big_leaf_crown(top, rnd, seed, n_leaves, leaf_len_range):
    """Corona de hojas grandes simples (no cúmulos de blobs) para el árbol
    delgado de sotobosque: hojas anchas y alargadas que se abren desde lo
    alto del tronco, cada una con su propio pecíolo corto."""
    parts = []
    for i in range(n_leaves):
        leaf_len = rnd.uniform(*leaf_len_range)
        leaf = C.make_leaf_blade(
            f'BigLeaf_{i:02d}', length=leaf_len, width_base=leaf_len * 0.34,
            width_tip=leaf_len * 0.10, curve_amount=leaf_len * 0.28,
            segments=7, double_sided=True,
        )
        ang = (2.0 * math.pi * i / n_leaves) + rnd.uniform(-0.3, 0.3)
        elevation = rnd.uniform(math.radians(15), math.radians(55))
        forward = C.Vector((math.cos(ang), math.sin(ang), math.sin(elevation)))
        C.orient_and_place(leaf, top, forward, C.Vector((0, 0, 1)))
        dark, light = _leaf_palette(rnd)
        C.assign_materials(leaf, ['M_Leaf'])
        C.set_vertex_colors(leaf, C.gradient_along_axis(dark, light, 'y', 0.0, leaf_len,
                                                          curve=0.9, jitter=0.03, rnd=rnd))
        parts.append(leaf)
    return parts


def build(variant):
    rnd = C.seeded_rng(variant['seed'])
    kind = variant['kind']
    parts = []

    if kind == 'giant':
        height = rnd.uniform(25.0, 35.0)
        base_radius = rnd.uniform(0.32, 0.48)          # ~64-96 cm de diámetro
        tip_radius = base_radius * 0.42
        curvature = height * rnd.uniform(0.015, 0.04)
        trunk, lean_dir = C.make_curved_trunk(
            'Trunk', height, base_radius, tip_radius, curvature,
            n_points=10, bevel_resolution=6, wobble=height * 0.006, rnd=rnd,
        )
        C.assign_materials(trunk, ['M_Bark'])
        C.set_vertex_colors(trunk, C.bark_streaks_tint(
            (0.16, 0.11, 0.08), (0.29, 0.22, 0.15), height, variant['seed'], streak_count=10))
        parts.append(trunk)

        parts += _buttress_roots(base_radius, rnd, variant['seed'] + 5,
                                  count=rnd.randint(6, 8), height_ratio=(4.2, 5.6))

        top = C.spline_point(height, 1.0, curvature, lean_dir)
        branch_parts, tips = _branch_system(
            top, base_radius, height, rnd, variant['seed'],
            n_main=(4, 5), main_len_ratio=(0.22, 0.32), elevation_range=(4, 16),
            sub_prob=0.85, sub_count=(2, 3), sub_len_ratio=(0.35, 0.5),
            sub_elevation_range=(4, 14),
        )
        parts += branch_parts

        for k, (pos, _direction) in enumerate(tips):
            lobe = _canopy_lobe(f'Lobe_{k:02d}', pos, radius_xy=height * 0.11,
                                 radius_z=height * 0.09, seed=variant['seed'] * 37 + k, rnd=rnd)
            parts.append(lobe)

    elif kind == 'wide':
        height = rnd.uniform(12.0, 18.0)
        base_radius = rnd.uniform(0.22, 0.34)
        tip_radius = base_radius * 0.5
        curvature = height * rnd.uniform(0.03, 0.07)
        trunk, lean_dir = C.make_curved_trunk(
            'Trunk', height, base_radius, tip_radius, curvature,
            n_points=8, bevel_resolution=5, wobble=height * 0.008, rnd=rnd,
        )
        C.assign_materials(trunk, ['M_Bark'])
        C.set_vertex_colors(trunk, C.bark_streaks_tint(
            (0.17, 0.12, 0.08), (0.30, 0.23, 0.15), height, variant['seed'], streak_count=8))
        parts.append(trunk)

        parts += _buttress_roots(base_radius, rnd, variant['seed'] + 5,
                                  count=rnd.randint(3, 5), height_ratio=(2.6, 3.6))

        top = C.spline_point(height, 1.0, curvature, lean_dir)
        branch_parts, tips = _branch_system(
            top, base_radius, height, rnd, variant['seed'],
            n_main=(4, 6), main_len_ratio=(0.45, 0.65), elevation_range=(10, 35),
            sub_prob=0.7, sub_count=(1, 2), sub_len_ratio=(0.5, 0.75),
        )
        parts += branch_parts

        for k, (pos, _direction) in enumerate(tips):
            lobe = _canopy_lobe(f'Lobe_{k:02d}', pos, radius_xy=height * 0.20,
                                 radius_z=height * 0.13, seed=variant['seed'] * 37 + k, rnd=rnd)
            parts.append(lobe)

    elif kind == 'mangrove':
        lift = rnd.uniform(1.2, 2.2)
        height = rnd.uniform(9.0, 13.0)
        base_radius = rnd.uniform(0.16, 0.24)
        tip_radius = base_radius * 0.5
        curvature = height * rnd.uniform(0.04, 0.09)
        trunk, lean_dir = C.make_curved_trunk(
            'Trunk', height, base_radius, tip_radius, curvature,
            n_points=7, bevel_resolution=4, wobble=height * 0.008, rnd=rnd,
            z_offset=lift,
        )
        C.assign_materials(trunk, ['M_Bark'])
        C.set_vertex_colors(trunk, C.bark_streaks_tint(
            (0.14, 0.11, 0.09), (0.26, 0.22, 0.17), height + lift, variant['seed'], streak_count=7))
        parts.append(trunk)

        parts += _mangrove_stilt_roots(lift, spread=lift * rnd.uniform(1.3, 1.8),
                                        base_radius=base_radius, rnd=rnd,
                                        seed=variant['seed'] + 5, count=rnd.randint(5, 7))

        top = C.spline_point(height, 1.0, curvature, lean_dir, z_offset=lift)
        branch_parts, tips = _branch_system(
            top, base_radius, height, rnd, variant['seed'],
            n_main=(3, 4), main_len_ratio=(0.35, 0.55), elevation_range=(15, 45),
            sub_prob=0.55, sub_count=(1, 2), sub_len_ratio=(0.45, 0.65),
        )
        parts += branch_parts

        for k, (pos, _direction) in enumerate(tips):
            lobe = _canopy_lobe(f'Lobe_{k:02d}', pos, radius_xy=height * 0.16,
                                 radius_z=height * 0.12, seed=variant['seed'] * 37 + k, rnd=rnd)
            parts.append(lobe)

    else:  # understory
        height = rnd.uniform(6.0, 9.0)
        base_radius = rnd.uniform(0.07, 0.11)
        tip_radius = base_radius * 0.55
        curvature = height * rnd.uniform(0.05, 0.11)
        trunk, lean_dir = C.make_curved_trunk(
            'Trunk', height, base_radius, tip_radius, curvature,
            n_points=6, bevel_resolution=3, wobble=height * 0.01, rnd=rnd,
        )
        C.assign_materials(trunk, ['M_Bark'])
        C.set_vertex_colors(trunk, C.bark_streaks_tint(
            (0.18, 0.13, 0.09), (0.31, 0.24, 0.16), height, variant['seed'], streak_count=6))
        parts.append(trunk)

        top = C.spline_point(height, 1.0, curvature, lean_dir)
        branch_parts, tips = _branch_system(
            top, base_radius, height, rnd, variant['seed'],
            n_main=(3, 4), main_len_ratio=(0.25, 0.40), elevation_range=(20, 50),
            sub_prob=0.6, sub_count=(1, 2), sub_len_ratio=(0.5, 0.65),
        )
        parts += branch_parts

        parts += _big_leaf_crown(top, rnd, variant['seed'] + 9,
                                  n_leaves=rnd.randint(7, 10), leaf_len_range=(0.75, 1.15))
        for pos, _direction in tips:
            parts += _big_leaf_crown(pos, rnd, variant['seed'] + 19 + int(pos.z * 100),
                                      n_leaves=rnd.randint(3, 5), leaf_len_range=(0.6, 0.9))

    obj = C.join_objects(parts, 'SM_' + variant['name'])
    C.shade_smooth_auto(obj, angle_deg=55.0)
    C.add_basic_uv(obj)
    return obj
