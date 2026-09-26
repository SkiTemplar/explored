"""
building_kit.py — piezas de construcción modulares del jugador y fuego.

8 props: módulo de suelo, módulo de pared, módulo de tejado de hoja de
palma, estaca, refugio inclinado de nivel 0, cama de hojas, fogata y
hoguera de señal. Los módulos de suelo/pared/tejado comparten una huella
de referencia de 2m x 2m para encajar entre sí en Unreal.
"""

import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import common as C  # noqa: E402
import _materials as M  # noqa: E402

CATEGORY = 'building_kit'

VARIANTS = [
    dict(name='Floor_Wood', seed=1901, builder='floor',
         tri_budget=(50, 300), needs_collision=True),
    dict(name='Wall_Wood', seed=1902, builder='wall',
         tri_budget=(100, 400), needs_collision=True),
    dict(name='Roof_PalmThatch', seed=1903, builder='roof_thatch',
         tri_budget=(300, 1200), needs_collision=True),
    dict(name='Stake', seed=1904, builder='stake',
         tri_budget=(30, 150), needs_collision=True, interactable=True),
    dict(name='Shelter_LeanTo', seed=1905, builder='shelter_lean_to',
         tri_budget=(500, 2000), needs_collision=True, collision_complex=True),
    dict(name='Bed_Leaves', seed=1906, builder='bed_leaves',
         tri_budget=(200, 800), needs_collision=True),
    dict(name='Campfire', seed=1907, builder='campfire',
         tri_budget=(150, 600), needs_collision=False),
    dict(name='Bonfire_Signal', seed=1908, builder='bonfire_signal',
         tri_budget=(200, 700), needs_collision=False),
]

_BUILDERS = {}


def _register(key):
    def deco(fn):
        _BUILDERS[key] = fn
        return fn
    return deco


def build(variant):
    rnd = C.seeded_rng(variant['seed'])
    return _BUILDERS[variant['builder']](variant, rnd)


def _finish(parts, name):
    obj = C.join_objects(parts, name) if len(parts) > 1 else parts[0]
    C.shade_smooth_auto(obj, angle_deg=35.0)
    C.add_basic_uv(obj)
    return obj


# ---------------------------------------------------------------------------
# Helpers internos de tejado inclinado (compartidos por Roof_PalmThatch y
# Shelter_LeanTo)
# ---------------------------------------------------------------------------

def _incline_lift(width_along_slope, thickness, tilt_rad):
    """Distancia que hay que subir en Z una losa ya inclinada (rotada sobre
    X) para que su punto más bajo quede justo en z=0."""
    return (width_along_slope / 2.0) * math.sin(tilt_rad) + (thickness / 2.0) * math.cos(tilt_rad)


def _thatch_layer(prefix, roof_w, roof_len, strip_count, thickness, tilt_rad,
                   z_extra, color, rnd, jitter=0.04):
    """Una capa de hojas de palma: varias tiras finas y anchas superpuestas
    a lo ancho del tejado, todas inclinadas al mismo ángulo."""
    strips = []
    step = roof_w / strip_count
    strip_w = step * 1.15  # solape entre tiras contiguas
    for i in range(strip_count):
        x = -roof_w / 2.0 + step * (i + 0.5)
        strip = C.make_box(f'{prefix}{i}', (strip_w, roof_len, thickness), center=(0.0, 0.0, 0.0))
        C.select_only(strip)
        import bpy
        bpy.ops.transform.rotate(value=tilt_rad, orient_axis='X')
        bpy.ops.object.transform_apply(rotation=True)
        lift = _incline_lift(roof_len, thickness, tilt_rad) + z_extra
        bpy.ops.transform.translate(value=(x, 0.0, lift))
        bpy.ops.object.transform_apply(location=True)
        C.set_vertex_colors(strip, C.constant_tint(color, alpha=0.0, jitter=jitter, rnd=rnd))
        strips.append(strip)
    grp = C.join_objects(strips, prefix.rstrip('_'))
    M.assign(grp, ['M_Leaf'])
    return grp


# ---------------------------------------------------------------------------
# 1. Módulo de suelo de tablones (2m x 2m x 0.08m)
# ---------------------------------------------------------------------------
@_register('floor')
def _build_floor(variant, rnd):
    n_planks = 6
    length, width, thickness = 2.0, 2.0, 0.08
    plank_w = length / n_planks
    base = (0.36, 0.25, 0.15)
    dark = (0.27, 0.18, 0.10)

    planks = []
    for i in range(n_planks):
        x = -length / 2.0 + plank_w * (i + 0.5)
        p = C.make_box(f'Plank{i}', (plank_w * 0.96, width, thickness),
                        center=(x, 0.0, thickness / 2.0))
        color = dark if i % 2 == 0 else base
        C.set_vertex_colors(p, C.constant_tint(color, alpha=0.0, jitter=0.02, rnd=rnd))
        planks.append(p)

    floor = C.join_objects(planks, 'Floor')
    M.assign(floor, ['M_Wood'])
    return _finish([floor], 'SM_' + variant['name'])


# ---------------------------------------------------------------------------
# 2. Módulo de pared (2m ancho x 0.1m grosor x 2.4m alto)
# ---------------------------------------------------------------------------
@_register('wall')
def _build_wall(variant, rnd):
    n_boards = 8
    width, thickness, height = 2.0, 0.1, 2.4
    board_w = width / n_boards
    base = (0.34, 0.23, 0.13)
    dark = (0.26, 0.17, 0.09)

    boards = []
    for i in range(n_boards):
        x = -width / 2.0 + board_w * (i + 0.5)
        b = C.make_box(f'Board{i}', (board_w * 0.96, thickness, height),
                        center=(x, 0.0, height / 2.0))
        color = dark if i % 2 == 0 else base
        C.set_vertex_colors(b, C.constant_tint(color, alpha=0.0, jitter=0.02, rnd=rnd))
        boards.append(b)
    panel = C.join_objects(boards, 'Panel')
    M.assign(panel, ['M_Wood'])

    brace_len = math.hypot(width * 0.85, height * 0.85)
    brace_ang = math.atan2(height * 0.85, width * 0.85)
    brace = C.make_box('Brace', (brace_len, thickness * 1.05, 0.06), center=(0.0, 0.0, 0.0))
    C.select_only(brace)
    import bpy
    bpy.ops.transform.rotate(value=brace_ang, orient_axis='Y')
    bpy.ops.object.transform_apply(rotation=True)
    bpy.ops.transform.translate(value=(0.0, 0.0, height / 2.0))
    bpy.ops.object.transform_apply(location=True)
    M.assign(brace, ['M_Wood'])
    C.set_vertex_colors(brace, C.constant_tint((0.18, 0.12, 0.06), alpha=0.0, jitter=0.02, rnd=rnd))

    return _finish([panel, brace], 'SM_' + variant['name'])


# ---------------------------------------------------------------------------
# 3. Módulo de tejado de hoja de palma (base ~2.2m x 2.2m, inclinado 25°)
# ---------------------------------------------------------------------------
@_register('roof_thatch')
def _build_roof_thatch(variant, rnd):
    tilt = math.radians(25.0)
    roof_w, roof_len = 2.2, 2.2

    base = C.make_box('Base', (roof_w, roof_len, 0.05), center=(0.0, 0.0, 0.0))
    C.select_only(base)
    import bpy
    bpy.ops.transform.rotate(value=tilt, orient_axis='X')
    bpy.ops.object.transform_apply(rotation=True)
    base_lift = _incline_lift(roof_len, 0.05, tilt)
    bpy.ops.transform.translate(value=(0.0, 0.0, base_lift))
    bpy.ops.object.transform_apply(location=True)
    M.assign(base, ['M_Wood'])
    C.set_vertex_colors(base, C.constant_tint((0.22, 0.15, 0.08), alpha=0.0, jitter=0.02, rnd=rnd))

    greens = [(0.16, 0.34, 0.14), (0.19, 0.38, 0.16), (0.15, 0.30, 0.12), (0.20, 0.40, 0.18)]
    parts = [base]
    n_layers = 4
    strips_per_layer = 7
    stack = 0.03
    for i in range(n_layers):
        layer = _thatch_layer(f'Thatch{i}_', roof_w * (1.0 - 0.05 * i),
                               roof_len * (1.0 - 0.10 * i), strips_per_layer, 0.02, tilt,
                               stack, greens[i % len(greens)], rnd)
        parts.append(layer)
        stack += 0.035

    return _finish(parts, 'SM_' + variant['name'])


# ---------------------------------------------------------------------------
# 4. Estaca de madera puntiaguda (~0.6m)
# ---------------------------------------------------------------------------
@_register('stake')
def _build_stake(variant, rnd):
    height = 0.6
    stake = C.make_cylinder('Stake', radius=0.02, depth=height, segments=10,
                             center=(0.0, 0.0, height / 2.0), radius2=0.002)
    M.assign(stake, ['M_Wood'])
    C.set_vertex_colors(stake, C.gradient_along_axis(
        (0.30, 0.19, 0.10), (0.42, 0.30, 0.18), 'z', 0.0, height, jitter=0.03, rnd=rnd))
    return _finish([stake], 'SM_' + variant['name'])


# ---------------------------------------------------------------------------
# 5. Refugio inclinado de nivel 0 (~2.5m x 2m base, ~1.8m alto)
# ---------------------------------------------------------------------------
@_register('shelter_lean_to')
def _build_shelter_lean_to(variant, rnd):
    post_positions = [(-0.9, -0.85), (0.9, -0.85), (0.0, 0.85)]
    post_height = 1.8

    posts = []
    for i, (x, y) in enumerate(post_positions):
        p = C.make_cylinder(f'Post{i}', radius=0.05, depth=post_height, segments=8,
                             center=(x, y, post_height / 2.0), radius2=0.045)
        posts.append(p)
    post_grp = C.join_objects(posts, 'Posts')
    M.assign(post_grp, ['M_Wood'])
    C.set_vertex_colors(post_grp, C.constant_tint((0.26, 0.17, 0.09), alpha=0.0, jitter=0.03, rnd=rnd))

    tilt = math.radians(20.0)
    roof_w, roof_len = 2.6, 2.2
    roof_rest = post_height - 0.35

    base = C.make_box('RoofBase', (roof_w, roof_len, 0.05), center=(0.0, 0.0, 0.0))
    C.select_only(base)
    import bpy
    bpy.ops.transform.rotate(value=tilt, orient_axis='X')
    bpy.ops.object.transform_apply(rotation=True)
    base_lift = _incline_lift(roof_len, 0.05, tilt) + roof_rest
    bpy.ops.transform.translate(value=(0.0, 0.0, base_lift))
    bpy.ops.object.transform_apply(location=True)
    M.assign(base, ['M_Wood'])
    C.set_vertex_colors(base, C.constant_tint((0.20, 0.13, 0.07), alpha=0.0, jitter=0.02, rnd=rnd))

    greens = [(0.16, 0.34, 0.14), (0.19, 0.38, 0.16), (0.15, 0.30, 0.12)]
    parts = [post_grp, base]
    n_layers = 4
    strips_per_layer = 9
    stack = 0.03
    for i in range(n_layers):
        layer = _thatch_layer(f'RoofThatch{i}_', roof_w * (1.0 - 0.04 * i),
                               roof_len * (1.0 - 0.08 * i), strips_per_layer, 0.02, tilt,
                               roof_rest + stack, greens[i % len(greens)], rnd)
        parts.append(layer)
        stack += 0.03

    return _finish(parts, 'SM_' + variant['name'])


# ---------------------------------------------------------------------------
# 6. Cama de hojas (~1.9m x 0.9m x 0.35m alto)
# ---------------------------------------------------------------------------
@_register('bed_leaves')
def _build_bed_leaves(variant, rnd):
    length, width = 1.9, 0.9
    rail_h, rail_t = 0.06, 0.05

    rails = [
        C.make_box('RailFront', (length, rail_t, rail_h),
                   center=(0.0, -width / 2.0 + rail_t / 2.0, rail_h / 2.0)),
        C.make_box('RailBack', (length, rail_t, rail_h),
                   center=(0.0, width / 2.0 - rail_t / 2.0, rail_h / 2.0)),
        C.make_box('RailLeft', (rail_t, width, rail_h),
                   center=(-length / 2.0 + rail_t / 2.0, 0.0, rail_h / 2.0)),
        C.make_box('RailRight', (rail_t, width, rail_h),
                   center=(length / 2.0 - rail_t / 2.0, 0.0, rail_h / 2.0)),
    ]
    frame = C.join_objects(rails, 'Frame')
    C.merge_by_distance(frame, dist=0.001)
    M.assign(frame, ['M_Wood'])
    C.set_vertex_colors(frame, C.constant_tint((0.30, 0.19, 0.10), alpha=0.0, jitter=0.03, rnd=rnd))

    mattress_half_h = 0.18
    mattress = C.make_blob('Mattress', (0.0, 0.0, rail_h + mattress_half_h), radius=1.0,
                            seed=variant['seed'], subdivisions=3, noise_scale=1.6,
                            noise_strength=0.12, scale=(0.95, 0.42, mattress_half_h),
                            relax_iterations=2)
    M.assign(mattress, ['M_Leaf'])
    C.set_vertex_colors(mattress, C.constant_tint((0.42, 0.55, 0.20), alpha=0.0, jitter=0.04, rnd=rnd))

    return _finish([frame, mattress], 'SM_' + variant['name'])


# ---------------------------------------------------------------------------
# 7. Fogata (anillo de piedras + troncos en tepee, ~0.9m diámetro)
# ---------------------------------------------------------------------------
def _build_log_teepee(rnd, seed, n_logs, log_len, log_r, segments, tilt_deg):
    tilt = math.radians(tilt_deg)
    half_len = log_len / 2.0
    lift = half_len * math.cos(tilt)
    total_height = 2.0 * lift

    logs = []
    for i in range(n_logs):
        ang = 2.0 * math.pi * i / n_logs + rnd.uniform(-0.12, 0.12)
        log = C.make_cylinder(f'Log{i}', radius=log_r, depth=log_len, segments=segments,
                               center=(0.0, 0.0, 0.0), radius2=log_r * 0.65)
        C.select_only(log)
        import bpy
        bpy.ops.transform.rotate(value=tilt, orient_axis='X')
        bpy.ops.object.transform_apply(rotation=True)
        bpy.ops.transform.rotate(value=ang, orient_axis='Z')
        bpy.ops.object.transform_apply(rotation=True)
        bpy.ops.transform.translate(value=(0.0, 0.0, lift))
        bpy.ops.object.transform_apply(location=True)
        logs.append(log)
    log_grp = C.join_objects(logs, 'Logs')
    C.merge_by_distance(log_grp, dist=0.01)
    M.assign(log_grp, ['M_Wood'])
    C.set_vertex_colors(log_grp, C.gradient_along_axis(
        (0.04, 0.03, 0.03), (0.32, 0.22, 0.13), 'z', 0.0, total_height, jitter=0.03, rnd=rnd))
    return log_grp


@_register('campfire')
def _build_campfire(variant, rnd):
    stones = []
    n_stones = rnd.randint(7, 9)
    ring_r = 0.42
    for i in range(n_stones):
        ang = 2.0 * math.pi * i / n_stones + rnd.uniform(-0.08, 0.08)
        cx, cy = math.cos(ang) * ring_r, math.sin(ang) * ring_r
        r = rnd.uniform(0.10, 0.14)
        s = C.make_blob(f'Stone{i}', (cx, cy, r * 0.7), radius=r,
                         seed=variant['seed'] * 13 + i, subdivisions=1, noise_scale=2.2,
                         noise_strength=0.25, scale=(1.0, 1.0, 0.8), relax_iterations=1)
        stones.append(s)
    stone_grp = C.join_objects(stones, 'Stones')
    M.assign(stone_grp, ['M_Stone'])
    C.set_vertex_colors(stone_grp, C.constant_tint((0.38, 0.37, 0.35), alpha=0.0, jitter=0.05, rnd=rnd))

    log_grp = _build_log_teepee(rnd, variant['seed'], n_logs=rnd.randint(5, 6),
                                 log_len=0.55, log_r=0.025, segments=8, tilt_deg=58.0)

    return _finish([stone_grp, log_grp], 'SM_' + variant['name'])


# ---------------------------------------------------------------------------
# 8. Hoguera de señal (evento del barco en el horizonte, ~1.8m diámetro)
# ---------------------------------------------------------------------------
@_register('bonfire_signal')
def _build_bonfire_signal(variant, rnd):
    log_grp = _build_log_teepee(rnd, variant['seed'], n_logs=rnd.randint(9, 11),
                                 log_len=1.7, log_r=0.05, segments=12, tilt_deg=62.0)
    return _finish([log_grp], 'SM_' + variant['name'])
