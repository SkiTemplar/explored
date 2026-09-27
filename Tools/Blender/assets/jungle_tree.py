"""
Árboles de selva — Tools/Blender/assets/jungle_tree.py

4 especies distintas con ramificación real (no «piruletas»): un gigante de
dosel con contrafuertes, un mediano de copa ancha, un manglar con raíces
aéreas en arco que lo levantan del suelo, y un delgado de sotobosque con
hojas grandes.

5ª pasada de arte (encargo 2026-09-27, técnica «follaje estilizado cartoon»
de Sea of Thieves/Genshin/Tchia — rechazo explícito de la 4ª pasada, «nube
de esquirlas en punta» y «troncos palillo»):
  - Copa = 4-9 MASAS (make_canopy_mass en common.py: esferoide con ruido
    suave + normales esféricas transferidas desde un proxy + tarjetas de
    hoja ancladas como escamas sobre su superficie), en pisos distintos con
    huecos entre ellas — nunca cards sueltas flotando sin superficie que
    las sostenga (eso era la 4ª pasada).
  - Tronco mucho más grueso (0,3-1,0 m de radio según especie, antes
    0,07-0,48 m) con curva en S real (make_curved_trunk: s_curve) y base
    ensanchada (base_flare), además de las raíces tabulares/zancudas que
    ya había.
  - Ramas principales gruesas SIN subramas (las masas ya dan el volumen
    que antes daban las subramas con más puntas de card) para que cada
    una se vea entera desde el tronco hasta su masa.
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

_CANOPY_CELLS = ('leaf_a', 'leaf_b', 'leaf_small_round', 'leaf_serrated')


def _mass_palette(rnd):
    """Degradado abajo/dentro (verde azulado, frío) -> arriba/fuera (verde
    amarillento vivo, cálido) de UNA masa de copa — 4ª pasada de arte,
    encargo 2026-09-27: «la parte superior en verde amarillento vivo, la
    inferior en verde azulado, y menos negro en el conjunto» (la 3ª pasada
    alternaba masas enteras cálidas U frías, y se iba a un R/B cercanos a
    0 en la base, demasiado oscuro bajo el cielo gris de revisión)."""
    # R se mantiene siempre bien por detrás de G en las dos paradas del
    # degradado: R alto (la versión anterior llegaba a 0.46-0.51) es lo
    # que hacía leer khaki/amarillo reseco en vez de verde vivo bajo un
    # sol fuerte — R solo sube para dar CALIDEZ, nunca para competir con G.
    cool = rnd.uniform(-0.01, 0.02)
    dark = (0.055 + cool * 0.3, 0.20 + rnd.uniform(-0.015, 0.02), 0.16 + cool)
    warm = rnd.uniform(0.0, 0.05)
    light = (0.30 + warm, 0.66 + rnd.uniform(-0.02, 0.03), 0.13 + warm * 0.3)
    return dark, light


def _build_canopy(tips, rnd, seed, radius_range, target_tris=1800, max_masses=9,
                   inward_pull=0.24, aspect_xy=1.6):
    """Construye UNA masa elipsoide por punta de rama (tips: lista de
    (pos, dirección)), hasta `max_masses`, con oclusión acumulada entre
    masas ya colocadas (`neighbors`, ver make_canopy_mass/_mass_color_fn).

    4ª pasada de arte (encargo 2026-09-27: «bolas sueltas en un palo, casi
    perfectas» en vez de una nube con lóbulos): el centro de cada masa se
    acerca `inward_pull` hacia el centroide de TODAS las puntas de rama de
    la copa — así las masas vecinas se solapan un 30-50% en vez de solo
    tocarse (una copa es una nube única con lóbulos, no un racimo), y de
    paso la punta de la rama (que se queda en `pos`, sin mover) acaba más
    enterrada dentro de su masa en vez de asomar el corte. Una masa
    -normalmente la de la rama más gruesa- sale un 25-45% más grande que
    las demás para romper la simetría de «racimo de uvas de tamaño
    uniforme»."""
    tips = tips[:max_masses]
    if not tips:
        return []
    centroid = C.Vector((0.0, 0.0, 0.0))
    for pos, _d in tips:
        centroid += pos
    centroid /= len(tips)

    dominant_i = rnd.randrange(len(tips))
    masses = []
    records = []  # (centro, radio_efectivo) de las masas ya construidas, para el AO
    for k, (pos, direction) in enumerate(tips):
        center = pos.lerp(centroid, inward_pull)
        radius = rnd.uniform(*radius_range)
        if k == dominant_i:
            radius *= rnd.uniform(1.25, 1.45)
        out_hint = C.Vector((direction.x, direction.y, 0.0))
        dark, light = _mass_palette(rnd)
        mass = C.make_canopy_mass(
            f'Mass_{k:02d}', center=center, radius=radius, seed=seed * 37 + k,
            up_hint=(0.0, 0.0, 1.0), out_hint=out_hint,
            dark_cool=dark, light_warm=light, cell_names=_CANOPY_CELLS,
            target_tris=target_tris, neighbors=list(records), aspect_xy=aspect_xy,
        )
        records.append((center, radius * aspect_xy))
        masses.append(mass)
    return masses


def _branch_tier(attach, base_radius, rnd, seed, n, len_range, elevation_range,
                  radius_ratio=(0.36, 0.52), curvature_ratio=(0.25, 0.45),
                  ang_offset=0.0, ang_jitter=0.4):
    """Ramas principales GRUESAS desde `attach`, SIN subramas (las masas de
    copa dan ahora el volumen que antes daban las puntas de subrama): cada
    una llega entera y visible hasta la masa que cuelga de su punta.
    Devuelve (parts, tips) con tips=[(posición_mundo, dirección), ...]."""
    parts = []
    tips = []
    for i in range(n):
        ang = ang_offset + (2.0 * math.pi * i / n) + rnd.uniform(-ang_jitter, ang_jitter)
        elevation = math.radians(rnd.uniform(*elevation_range))
        b_len = rnd.uniform(*len_range)
        b_radius = base_radius * rnd.uniform(*radius_ratio)
        direction = C.Vector((math.cos(ang), math.sin(ang), math.sin(elevation)))

        branch, _ = C.make_curved_trunk(
            f'Branch_{seed}_{i:02d}', height=b_len, base_radius=b_radius,
            tip_radius=b_radius * 0.42, curvature=b_len * rnd.uniform(*curvature_ratio),
            n_points=6, bevel_resolution=4, wobble=b_len * 0.012, rnd=rnd,
        )
        C.orient_and_place_zaxis(branch, attach, direction, C.Vector((0, 0, 1)))
        C.assign_materials(branch, ['M_Bark'])
        C.set_vertex_colors(branch, C.bark_streaks_tint(
            (0.15, 0.10, 0.07), (0.27, 0.20, 0.13), b_len, seed * 10 + i, streak_count=6))
        parts.append(branch)
        tips.append((attach + direction.normalized() * b_len, direction))
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


def build(variant):
    rnd = C.seeded_rng(variant['seed'])
    kind = variant['kind']
    parts = []       # madera: tronco, raíces, ramas — se sombrea por ángulo
    all_masses = []  # masas de copa: normales esféricas ya asignadas, NUNCA
                      # pasar por shade_smooth_auto (sobrescribiría esas
                      # normales con las recalculadas por ángulo)

    if kind == 'giant':
        # Emergente: tronco de dosel, ensanchado en la base, con copa en
        # parasol de dos pisos (rama baja + rama alta, más plana y abierta).
        height = rnd.uniform(25.0, 35.0)
        base_radius = rnd.uniform(0.70, 1.0)
        tip_radius = base_radius * 0.38
        curvature = height * rnd.uniform(0.02, 0.04)
        s_curve = height * rnd.uniform(0.018, 0.03)
        trunk, lean_dir = C.make_curved_trunk(
            'Trunk', height, base_radius, tip_radius, curvature,
            n_points=10, bevel_resolution=7, wobble=height * 0.004, rnd=rnd,
            s_curve=s_curve, base_flare=1.7,
        )
        C.assign_materials(trunk, ['M_Bark'])
        C.set_vertex_colors(trunk, C.bark_streaks_tint(
            (0.16, 0.11, 0.08), (0.29, 0.22, 0.15), height, variant['seed'], streak_count=12))
        parts.append(trunk)

        parts += _buttress_roots(base_radius, rnd, variant['seed'] + 5,
                                  count=rnd.randint(6, 8), height_ratio=(4.4, 5.8))

        lower_attach = C.spline_point(height, 0.58, curvature, lean_dir, s_curve=s_curve)
        upper_attach = C.spline_point(height, 0.92, curvature, lean_dir, s_curve=s_curve)
        lower_parts, lower_tips = _branch_tier(
            lower_attach, base_radius, rnd, variant['seed'] + 1, n=3,
            len_range=(height * 0.17, height * 0.23), elevation_range=(10, 22),
            radius_ratio=(0.42, 0.56),
        )
        upper_parts, upper_tips = _branch_tier(
            upper_attach, base_radius, rnd, variant['seed'] + 2, n=4,
            len_range=(height * 0.10, height * 0.15), elevation_range=(2, 12),
            radius_ratio=(0.30, 0.44), ang_offset=math.pi / 4.0,
        )
        parts += lower_parts + upper_parts

        masses = _build_canopy(lower_tips + upper_tips, rnd, variant['seed'],
                                radius_range=(2.8, 4.1), target_tris=4200, max_masses=9)
        all_masses += masses

    elif kind == 'wide':
        # Copa ancha y baja: ramas largas y casi horizontales desde media
        # altura, masas grandes muy separadas en horizontal.
        height = rnd.uniform(12.0, 18.0)
        base_radius = rnd.uniform(0.50, 0.75)
        tip_radius = base_radius * 0.45
        curvature = height * rnd.uniform(0.04, 0.07)
        s_curve = height * rnd.uniform(0.03, 0.05)
        trunk, lean_dir = C.make_curved_trunk(
            'Trunk', height, base_radius, tip_radius, curvature,
            n_points=8, bevel_resolution=6, wobble=height * 0.006, rnd=rnd,
            s_curve=s_curve, base_flare=1.55,
        )
        C.assign_materials(trunk, ['M_Bark'])
        C.set_vertex_colors(trunk, C.bark_streaks_tint(
            (0.17, 0.12, 0.08), (0.30, 0.23, 0.15), height, variant['seed'], streak_count=9))
        parts.append(trunk)

        parts += _buttress_roots(base_radius, rnd, variant['seed'] + 5,
                                  count=rnd.randint(3, 5), height_ratio=(2.8, 3.8))

        attach = C.spline_point(height, 0.52, curvature, lean_dir, s_curve=s_curve)
        branch_parts, tips = _branch_tier(
            attach, base_radius, rnd, variant['seed'] + 1, n=6,
            len_range=(height * 0.34, height * 0.48), elevation_range=(8, 24),
            radius_ratio=(0.38, 0.52),
        )
        parts += branch_parts

        masses = _build_canopy(tips, rnd, variant['seed'],
                                radius_range=(2.3, 3.3), target_tris=3400, max_masses=9)
        all_masses += masses

    elif kind == 'mangrove':
        # Copa baja sobre un tronco levantado por raíces zancudas.
        lift = rnd.uniform(1.2, 2.2)
        height = rnd.uniform(9.0, 13.0)
        base_radius = rnd.uniform(0.32, 0.46)
        tip_radius = base_radius * 0.5
        curvature = height * rnd.uniform(0.03, 0.06)
        s_curve = height * rnd.uniform(0.03, 0.05)
        trunk, lean_dir = C.make_curved_trunk(
            'Trunk', height, base_radius, tip_radius, curvature,
            n_points=7, bevel_resolution=5, wobble=height * 0.006, rnd=rnd,
            z_offset=lift, s_curve=s_curve, base_flare=1.25,
        )
        C.assign_materials(trunk, ['M_Bark'])
        C.set_vertex_colors(trunk, C.bark_streaks_tint(
            (0.14, 0.11, 0.09), (0.26, 0.22, 0.17), height + lift, variant['seed'], streak_count=7))
        parts.append(trunk)

        parts += _mangrove_stilt_roots(lift, spread=lift * rnd.uniform(1.3, 1.8),
                                        base_radius=base_radius, rnd=rnd,
                                        seed=variant['seed'] + 5, count=rnd.randint(5, 7))

        attach = C.spline_point(height, 0.72, curvature, lean_dir, z_offset=lift, s_curve=s_curve)
        branch_parts, tips = _branch_tier(
            attach, base_radius, rnd, variant['seed'] + 1, n=5,
            len_range=(height * 0.26, height * 0.37), elevation_range=(14, 36),
            radius_ratio=(0.36, 0.50),
        )
        parts += branch_parts

        masses = _build_canopy(tips, rnd, variant['seed'],
                                radius_range=(1.7, 2.4), target_tris=2600, max_masses=9)
        all_masses += masses

    else:  # understory
        # Pequeño y redondeado: pocas masas apretadas cerca de la copa.
        height = rnd.uniform(6.0, 9.0)
        base_radius = rnd.uniform(0.17, 0.24)
        tip_radius = base_radius * 0.5
        curvature = height * rnd.uniform(0.04, 0.08)
        s_curve = height * rnd.uniform(0.03, 0.05)
        trunk, lean_dir = C.make_curved_trunk(
            'Trunk', height, base_radius, tip_radius, curvature,
            n_points=7, bevel_resolution=5, wobble=height * 0.008, rnd=rnd,
            s_curve=s_curve, base_flare=1.35,
        )
        C.assign_materials(trunk, ['M_Bark'])
        C.set_vertex_colors(trunk, C.bark_streaks_tint(
            (0.18, 0.13, 0.09), (0.31, 0.24, 0.16), height, variant['seed'], streak_count=6))
        parts.append(trunk)

        top = C.spline_point(height, 1.0, curvature, lean_dir, s_curve=s_curve)
        branch_parts, tips = _branch_tier(
            top, base_radius, rnd, variant['seed'] + 1, n=4,
            len_range=(height * 0.18, height * 0.26), elevation_range=(26, 55),
            radius_ratio=(0.34, 0.48),
        )
        parts += branch_parts
        # una masa central en la punta del tronco, además de las 4 de rama,
        # para un penacho redondeado y lleno (5 masas en total).
        tips_with_center = [(top, C.Vector((0.0, 0.0, 1.0)))] + tips

        masses = _build_canopy(tips_with_center, rnd, variant['seed'],
                                radius_range=(1.05, 1.6), target_tris=1650, max_masses=9)
        all_masses += masses

    wood = C.join_objects(parts, 'SM_' + variant['name'] + '_Wood')
    C.shade_smooth_auto(wood, angle_deg=55.0)
    obj = C.join_objects([wood] + all_masses, 'SM_' + variant['name'])
    return obj
