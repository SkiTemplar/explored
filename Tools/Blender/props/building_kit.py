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
import _shapes as S  # noqa: E402

CATEGORY = 'building_kit'

VARIANTS = [
    dict(name='Floor_Wood', seed=1901, builder='floor',
         tri_budget=(50, 900), needs_collision=True),
    dict(name='Wall_Wood', seed=1902, builder='wall',
         tri_budget=(100, 1300), needs_collision=True),
    dict(name='Roof_PalmThatch', seed=1903, builder='roof_thatch',
         tri_budget=(300, 3600), needs_collision=True),
    dict(name='Stake', seed=1904, builder='stake',
         tri_budget=(30, 180), needs_collision=True, interactable=True),
    dict(name='Shelter_LeanTo', seed=1905, builder='shelter_lean_to',
         tri_budget=(500, 5200), needs_collision=True, collision_complex=True),
    dict(name='Bed_Leaves', seed=1906, builder='bed_leaves',
         tri_budget=(200, 900), needs_collision=True),
    dict(name='Campfire', seed=1907, builder='campfire',
         tri_budget=(150, 3200), needs_collision=False),
    dict(name='Bonfire_Signal', seed=1908, builder='bonfire_signal',
         tri_budget=(200, 2200), needs_collision=False),
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


def _finish(parts, name, bevel_width=0.01, bevel_segments=2):
    """Une, bisela (redondea las aristas duras de las cajas: el look
    "low-poly pero smooth, como animado" pedido en la revisión de arte) y
    solo entonces sombrea suave + UV. El bisel debe ir ANTES de
    shade_smooth_auto para que el ángulo de sombreado suave vea las caras
    nuevas del bisel."""
    obj = C.join_objects(parts, name) if len(parts) > 1 else parts[0]
    S.bevel_obj(obj, width=bevel_width, segments=bevel_segments, limit_angle_deg=35.0)
    C.shade_smooth_auto(obj, angle_deg=35.0)
    C.add_basic_uv(obj)
    return obj


# Paleta viva: madera miel/caramelo cálida (nunca marrón-negro apagado),
# hoja de palma verde-dorada tropical y el carbón (ese sí oscuro de
# verdad) de la base de las hogueras.
_WOOD_HONEY = (0.58, 0.38, 0.18)
_WOOD_CARAMEL = (0.50, 0.32, 0.15)
_WOOD_HONEY_DARK = (0.46, 0.30, 0.15)
_WOOD_CARAMEL_LIGHT = (0.62, 0.42, 0.20)
_LEAF_GREENS = [
    (0.42, 0.56, 0.18),
    (0.55, 0.62, 0.20),
    (0.35, 0.49, 0.15),
    (0.48, 0.58, 0.22),
]
_SOOT = (0.05, 0.04, 0.04)
_STONE_WARM = (0.46, 0.42, 0.36)


# ---------------------------------------------------------------------------
# Helpers internos de tejado inclinado (compartidos por Roof_PalmThatch y
# Shelter_LeanTo)
# ---------------------------------------------------------------------------

def _incline_lift(width_along_slope, thickness, tilt_rad):
    """Distancia que hay que subir en Z una losa ya inclinada (rotada sobre
    X) para que su punto más bajo quede justo en z=0."""
    return (width_along_slope / 2.0) * math.sin(tilt_rad) + (thickness / 2.0) * math.cos(tilt_rad)


def _thatch_layer(prefix, roof_w, roof_len, strip_count, thickness, tilt_rad,
                   z_extra, colors, rnd, jitter=0.03):
    """Una capa de hojas de palma: varias tiras finas y anchas superpuestas
    a lo ancho del tejado, todas inclinadas al mismo ángulo. `colors` es una
    lista de 3-4 tonos verde-dorado repartidos en bandas por posición (en
    vez de un tinte casi uniforme) para que la capa se lea como hojas de
    verdad, no como una tabla verde."""
    strips = []
    step = roof_w / strip_count
    strip_w = step * 1.15  # solape entre tiras contiguas
    color_fn = S.banded_tint(colors, 'x', -roof_w / 2.0, roof_w / 2.0, jitter=jitter, rnd=rnd)
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
        C.set_vertex_colors(strip, color_fn)
        strips.append(strip)
    grp = C.join_objects(strips, prefix.rstrip('_'))
    M.assign(grp, ['M_Leaf'])
    return grp


# ---------------------------------------------------------------------------
# 1. Módulo de suelo de tablones (2m x 2m x 0.09m)
# ---------------------------------------------------------------------------
@_register('floor')
def _build_floor(variant, rnd):
    n_planks = 6
    length, width, thickness = 2.0, 2.0, 0.09
    plank_w = length / n_planks

    planks = []
    for i in range(n_planks):
        x = -length / 2.0 + plank_w * (i + 0.5)
        p = C.make_box(f'Plank{i}', (plank_w * 0.96, width, thickness),
                        center=(x, 0.0, thickness / 2.0))
        planks.append(p)

    floor = C.join_objects(planks, 'Floor')
    M.assign(floor, ['M_Wood'])
    C.set_vertex_colors(floor, S.banded_tint(
        [_WOOD_HONEY, _WOOD_CARAMEL, _WOOD_HONEY_DARK, _WOOD_CARAMEL_LIGHT],
        'x', -length / 2.0, length / 2.0, jitter=0.03, rnd=rnd))
    return _finish([floor], 'SM_' + variant['name'], bevel_width=0.01)


# ---------------------------------------------------------------------------
# 2. Módulo de pared (2m ancho x 0.1m grosor x 2.4m alto)
# ---------------------------------------------------------------------------
@_register('wall')
def _build_wall(variant, rnd):
    n_boards = 8
    width, thickness, height = 2.0, 0.1, 2.4
    board_w = width / n_boards

    boards = []
    for i in range(n_boards):
        x = -width / 2.0 + board_w * (i + 0.5)
        b = C.make_box(f'Board{i}', (board_w * 0.96, thickness, height),
                        center=(x, 0.0, height / 2.0))
        boards.append(b)
    panel = C.join_objects(boards, 'Panel')
    M.assign(panel, ['M_Wood'])
    C.set_vertex_colors(panel, S.banded_tint(
        [_WOOD_CARAMEL, _WOOD_HONEY, _WOOD_HONEY_DARK, _WOOD_CARAMEL_LIGHT],
        'x', -width / 2.0, width / 2.0, jitter=0.03, rnd=rnd))

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
    C.set_vertex_colors(brace, C.constant_tint(_WOOD_CARAMEL, alpha=0.0, jitter=0.02, rnd=rnd))

    return _finish([panel, brace], 'SM_' + variant['name'], bevel_width=0.012)


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
    C.set_vertex_colors(base, C.constant_tint(_WOOD_HONEY_DARK, alpha=0.0, jitter=0.02, rnd=rnd))

    parts = [base]
    n_layers = 4
    strips_per_layer = 7
    stack = 0.03
    for i in range(n_layers):
        layer = _thatch_layer(f'Thatch{i}_', roof_w * (1.0 - 0.05 * i),
                               roof_len * (1.0 - 0.10 * i), strips_per_layer, 0.02, tilt,
                               stack, _LEAF_GREENS, rnd)
        parts.append(layer)
        stack += 0.035

    return _finish(parts, 'SM_' + variant['name'], bevel_width=0.008)


# ---------------------------------------------------------------------------
# 4. Estaca de madera puntiaguda (~0.6m)
# ---------------------------------------------------------------------------
@_register('stake')
def _build_stake(variant, rnd):
    height = 0.6
    stake = C.make_cylinder('Stake', radius=0.024, depth=height, segments=12,
                             center=(0.0, 0.0, height / 2.0), radius2=0.003)
    M.assign(stake, ['M_Wood'])
    C.set_vertex_colors(stake, S.banded_tint(
        [_WOOD_CARAMEL, _WOOD_HONEY, _WOOD_HONEY_DARK], 'z', 0.0, height,
        jitter=0.03, rnd=rnd))
    return _finish([stake], 'SM_' + variant['name'], bevel_width=0.005)


# ---------------------------------------------------------------------------
# 5. Refugio inclinado de nivel 0 (~2.5m x 2m base, ~1.8m alto)
# ---------------------------------------------------------------------------
@_register('shelter_lean_to')
def _build_shelter_lean_to(variant, rnd):
    post_positions = [(-0.9, -0.85), (0.9, -0.85), (0.0, 0.85)]
    post_height = 1.8

    posts = []
    for i, (x, y) in enumerate(post_positions):
        p = C.make_cylinder(f'Post{i}', radius=0.06, depth=post_height, segments=14,
                             center=(x, y, post_height / 2.0), radius2=0.054)
        posts.append(p)
    post_grp = C.join_objects(posts, 'Posts')
    M.assign(post_grp, ['M_Wood'])
    C.set_vertex_colors(post_grp, C.constant_tint(_WOOD_CARAMEL, alpha=0.0, jitter=0.03, rnd=rnd))

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
    C.set_vertex_colors(base, C.constant_tint(_WOOD_HONEY_DARK, alpha=0.0, jitter=0.02, rnd=rnd))

    parts = [post_grp, base]
    n_layers = 4
    strips_per_layer = 9
    stack = 0.03
    for i in range(n_layers):
        layer = _thatch_layer(f'RoofThatch{i}_', roof_w * (1.0 - 0.04 * i),
                               roof_len * (1.0 - 0.08 * i), strips_per_layer, 0.02, tilt,
                               roof_rest + stack, _LEAF_GREENS, rnd)
        parts.append(layer)
        stack += 0.03

    return _finish(parts, 'SM_' + variant['name'], bevel_width=0.014)


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
    C.set_vertex_colors(frame, S.banded_tint(
        [_WOOD_CARAMEL, _WOOD_HONEY, _WOOD_HONEY_DARK], 'x', -length / 2.0, length / 2.0,
        jitter=0.03, rnd=rnd))

    mattress_half_h = 0.18
    mattress = C.make_blob('Mattress', (0.0, 0.0, rail_h + mattress_half_h), radius=1.0,
                            seed=variant['seed'], subdivisions=3, noise_scale=1.6,
                            noise_strength=0.12, scale=(0.95, 0.42, mattress_half_h),
                            relax_iterations=2)
    M.assign(mattress, ['M_Leaf'])
    C.set_vertex_colors(mattress, S.banded_tint(
        _LEAF_GREENS, 'y', -0.5, 0.5, jitter=0.04, rnd=rnd))

    return _finish([frame, mattress], 'SM_' + variant['name'], bevel_width=0.01)


# ---------------------------------------------------------------------------
# 7. Fogata (anillo de piedras + troncos en tepee, ~0.9m diámetro)
# ---------------------------------------------------------------------------
def _build_log_teepee(rnd, seed, n_logs, log_len, log_r, segments, tilt_deg):
    tilt = math.radians(tilt_deg)
    half_len = log_len / 2.0
    lift = half_len * math.cos(tilt)

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
    # hollín en parches orgánicos sobre madera cálida en vez de un
    # degradado uniforme de negro a marrón (se leía como un anillo
    # perfecto, no como suciedad real).
    C.set_vertex_colors(log_grp, S.weathered_tint(
        _WOOD_HONEY, _SOOT, seed=seed, patchiness=2.4, wear_amount=0.4,
        jitter=0.03, rnd=rnd))
    return log_grp


@_register('campfire')
def _build_campfire(variant, rnd):
    stones = []
    n_stones = rnd.randint(7, 9)
    ring_r = 0.42
    for i in range(n_stones):
        ang = 2.0 * math.pi * i / n_stones + rnd.uniform(-0.08, 0.08)
        cx, cy = math.cos(ang) * ring_r, math.sin(ang) * ring_r
        r = rnd.uniform(0.11, 0.15)
        s = C.make_blob(f'Stone{i}', (cx, cy, r * 0.7), radius=r,
                         seed=variant['seed'] * 13 + i, subdivisions=1, noise_scale=2.2,
                         noise_strength=0.25, scale=(1.0, 1.0, 0.8), relax_iterations=1)
        stones.append(s)
    stone_grp = C.join_objects(stones, 'Stones')
    M.assign(stone_grp, ['M_Stone'])
    C.set_vertex_colors(stone_grp, C.constant_tint(_STONE_WARM, alpha=0.0, jitter=0.05, rnd=rnd))

    # troncos más gruesos y más leña: se lee generosa y acogedora, no un
    # cruce de palillos.
    log_grp = _build_log_teepee(rnd, variant['seed'], n_logs=rnd.randint(6, 8),
                                 log_len=0.65, log_r=0.03, segments=12, tilt_deg=58.0)

    return _finish([stone_grp, log_grp], 'SM_' + variant['name'], bevel_width=0.01)


# ---------------------------------------------------------------------------
# 8. Hoguera de señal (evento del barco en el horizonte, ~1.8m diámetro)
# ---------------------------------------------------------------------------
@_register('bonfire_signal')
def _build_bonfire_signal(variant, rnd):
    log_grp = _build_log_teepee(rnd, variant['seed'], n_logs=rnd.randint(10, 13),
                                 log_len=2.0, log_r=0.06, segments=14, tilt_deg=62.0)
    return _finish([log_grp], 'SM_' + variant['name'], bevel_width=0.02)
