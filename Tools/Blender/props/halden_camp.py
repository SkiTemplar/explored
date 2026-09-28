"""
halden_camp.py — campamentos abandonados de la expedicion cientifica Halden
de 1974.

Cinco campamentos de campo repartidos por el archipielago mas una estacion
de radio y un observatorio de mareas. Aspecto: equipo de los anos 70
desgastado por el clima tropical (lona con color y caracter, madera calida
con vetas, metal claro con oxido en parches).
"""

import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import _materials as M  # noqa: E402
import _shapes as S  # noqa: E402
import common as C  # noqa: E402

VARIANTS = [
    dict(name='Halden_Tent', seed=1301, builder='tent',
         tri_budget=(20, 500), needs_collision=True),
    dict(name='Halden_Crate', seed=1302, builder='crate',
         tri_budget=(100, 1200), needs_collision=True),
    dict(name='Halden_Table', seed=1303, builder='table',
         tri_budget=(100, 900), needs_collision=True),
    dict(name='Halden_FieldRadio', seed=1304, builder='field_radio',
         tri_budget=(60, 1500), needs_collision=True, interactable=True),
    dict(name='Halden_RadioStation', seed=1305, builder='radio_station',
         tri_budget=(900, 3800), needs_collision=True, collision_complex=True),
    dict(name='Halden_TideObservatory', seed=1306, builder='tide_observatory',
         tri_budget=(300, 3200), needs_collision=True, collision_complex=True),
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


def _finish(parts, name, bevel_width=0.02):
    obj = C.join_objects(parts, name) if len(parts) > 1 else parts[0]
    S.bevel_obj(obj, width=bevel_width, segments=2)
    C.shade_smooth_auto(obj, angle_deg=35.0)
    C.add_basic_uv(obj)
    return obj


def _build_gable_end(y_pos, half_w, height, thickness, name):
    """Pared triangular (extremo de tienda tipo cuna): una caja plana cuyos
    dos vertices superiores se colapsan al centro (x=0), dejando una
    silueta de gablete en vez de un rectangulo."""
    wall = C.make_box(name, (half_w * 2.0, thickness, height), center=(0.0, y_pos, height / 2.0))
    me = wall.data
    for v in me.vertices:
        if v.co.z > height * 0.9:
            v.co.x = 0.0
    me.update()
    return wall


# Paleta calida compartida del campamento: madera caramelo/miel, metal
# claro con oxido en parches, lona viva con caracter (nunca marron-negro
# apagado ni gris plano).
_WOOD_LIGHT = (0.62, 0.42, 0.20)
_WOOD_MID = (0.56, 0.36, 0.17)
_WOOD_DARK = (0.50, 0.32, 0.14)
_METAL = (0.55, 0.54, 0.52)
_METAL_RUST = (0.48, 0.28, 0.15)
_CANVAS_OLIVE = (0.42, 0.46, 0.24)
_CANVAS_OLIVE_DARK = (0.22, 0.24, 0.13)


# ---------------------------------------------------------------------------
# 1. Tienda de campana tipo cuna (A-frame)
# ---------------------------------------------------------------------------
@_register('tent')
def _build_tent(variant, rnd):
    import bpy

    tent_len = 2.0
    half_w = 0.75
    height = 1.3
    thickness = 0.03

    slope_len = math.hypot(half_w, height)
    theta = math.pi / 2.0 - math.atan2(half_w, height)

    right = C.make_box('PanelRight', (slope_len, tent_len, thickness),
                        center=(slope_len / 2.0, 0.0, 0.0))
    C.select_only(right)
    bpy.ops.transform.rotate(value=theta, orient_axis='Y')
    bpy.ops.object.transform_apply(rotation=True)
    bpy.ops.transform.translate(value=(0.0, 0.0, height))
    bpy.ops.object.transform_apply(location=True)

    left = C.make_box('PanelLeft', (slope_len, tent_len, thickness),
                       center=(-slope_len / 2.0, 0.0, 0.0))
    C.select_only(left)
    bpy.ops.transform.rotate(value=-theta, orient_axis='Y')
    bpy.ops.object.transform_apply(rotation=True)
    bpy.ops.transform.translate(value=(0.0, 0.0, height))
    bpy.ops.object.transform_apply(location=True)

    front_end = _build_gable_end(-(tent_len / 2.0 - 0.01), half_w, height, 0.02, 'EndFront')
    back_end = _build_gable_end(tent_len / 2.0 - 0.01, half_w, height, 0.02, 'EndBack')

    canvas = C.join_objects([right, left, front_end, back_end], 'Canvas')
    C.merge_by_distance(canvas, dist=0.002)
    M.assign(canvas, ['M_Fabric'])
    # lona viva en oliva claro desvaido, con una franja mas oscura a lo
    # largo del borde inferior (remate sucio de la tela contra el suelo)
    C.set_vertex_colors(canvas, S.stripe_tint(
        _CANVAS_OLIVE, _CANVAS_OLIVE_DARK, 'z', 0.0, height * 0.16,
        jitter=0.04, rnd=rnd))

    return _finish([canvas], 'SM_' + variant['name'], bevel_width=0.012)


# ---------------------------------------------------------------------------
# 2. Caja de suministros con refuerzos metalicos
# ---------------------------------------------------------------------------
@_register('crate')
def _build_crate(variant, rnd):
    # proporciones ligeramente no cubicas (rompe la silueta de "caja
    # perfecta") + vetas de madera en bandas en vez de un tinte plano.
    size_x, size_y, size_z = 0.64, 0.58, 0.56
    body = C.make_box('Body', (size_x, size_y, size_z), center=(0.0, 0.0, size_z / 2.0))
    M.assign(body, ['M_Wood'])
    C.set_vertex_colors(body, S.banded_tint(
        [_WOOD_LIGHT, _WOOD_MID, _WOOD_DARK, _WOOD_MID, _WOOD_LIGHT],
        'z', 0.0, size_z, jitter=0.03, rnd=rnd))

    inset_x = size_x / 2.0 - 0.03
    inset_y = size_y / 2.0 - 0.03
    braces = []
    for xi in (-inset_x, inset_x):
        for yi in (-inset_y, inset_y):
            for zi in (0.03, size_z - 0.03):
                b = C.make_box(f'Brace{len(braces)}', (0.05, 0.05, 0.05), center=(xi, yi, zi))
                braces.append(b)
    brace_grp = C.join_objects(braces, 'Braces')
    M.assign(brace_grp, ['M_Metal'])
    C.set_vertex_colors(brace_grp, S.weathered_tint(
        _METAL, _METAL_RUST, seed=variant['seed'], patchiness=4.0,
        wear_amount=0.4, jitter=0.03, rnd=rnd))

    return _finish([body, brace_grp], 'SM_' + variant['name'], bevel_width=0.02)


# ---------------------------------------------------------------------------
# 3. Mesa plegable de campamento
# ---------------------------------------------------------------------------
@_register('table')
def _build_table(variant, rnd):
    top_w, top_d, top_h = 1.2, 0.7, 0.75
    top_thick = 0.03
    leg_radius = 0.026  # +30% sobre el original: patas menos "palillo"
    leg_height = top_h - top_thick

    top = C.make_box('Top', (top_w, top_d, top_thick), center=(0.0, 0.0, top_h - top_thick / 2.0))
    M.assign(top, ['M_Wood'])
    C.set_vertex_colors(top, S.banded_tint(
        [_WOOD_LIGHT, _WOOD_MID, _WOOD_DARK, _WOOD_MID, _WOOD_LIGHT],
        'x', -top_w / 2.0, top_w / 2.0, jitter=0.03, rnd=rnd))

    legs = []
    for xi in (-(top_w / 2.0 - 0.05), (top_w / 2.0 - 0.05)):
        for yi in (-(top_d / 2.0 - 0.05), (top_d / 2.0 - 0.05)):
            leg = C.make_cylinder(f'Leg{len(legs)}', radius=leg_radius, depth=leg_height,
                                   segments=12, center=(xi, yi, leg_height / 2.0))
            legs.append(leg)
    leg_grp = C.join_objects(legs, 'Legs')
    M.assign(leg_grp, ['M_Wood'])
    C.set_vertex_colors(leg_grp, C.constant_tint(_WOOD_DARK, alpha=0.0, jitter=0.03, rnd=rnd))

    return _finish([top, leg_grp], 'SM_' + variant['name'], bevel_width=0.015)


# ---------------------------------------------------------------------------
# 4. Radio de campamento robusta con manivela lateral
# ---------------------------------------------------------------------------
@_register('field_radio')
def _build_field_radio(variant, rnd):
    import bpy

    body_size = (0.32, 0.16, 0.22)
    body = C.make_box('Body', body_size, center=(0.0, 0.0, body_size[2] / 2.0))
    M.assign(body, ['M_Metal'])
    C.set_vertex_colors(body, S.weathered_tint(
        _METAL, _METAL_RUST, seed=variant['seed'], patchiness=5.0,
        wear_amount=0.35, jitter=0.03, rnd=rnd))

    grille = C.make_box('Grille', (0.14, 0.006, 0.14),
                         center=(0.0, -body_size[1] / 2.0 - 0.003, 0.13))
    M.assign(grille, ['M_Fabric'])
    C.set_vertex_colors(grille, C.constant_tint((0.14, 0.12, 0.10), alpha=0.0, jitter=0.02, rnd=rnd))

    knobs = []
    for i, x in enumerate((-0.10, -0.05)):
        k = C.make_cylinder(f'Knob{i}', radius=0.014, depth=0.018, segments=10, center=(x, 0.0, 0.0))
        C.select_only(k)
        bpy.ops.transform.rotate(value=math.radians(90), orient_axis='X')
        bpy.ops.object.transform_apply(rotation=True)
        bpy.ops.transform.translate(value=(0.0, -body_size[1] / 2.0 - 0.009, 0.05))
        bpy.ops.object.transform_apply(location=True)
        knobs.append(k)
    knob_grp = C.join_objects(knobs, 'Knobs')
    M.assign(knob_grp, ['M_Metal'])
    C.set_vertex_colors(knob_grp, C.constant_tint((0.65, 0.50, 0.18), alpha=0.0, jitter=0.02, rnd=rnd))

    # manivela lateral, algo mas gruesa que el original para que no parezca
    # alambre tecnico: eje corto saliendo del costado + palanca perpendicular.
    shaft = C.make_cylinder('CrankShaft', radius=0.015, depth=0.06, segments=10, center=(0.0, 0.0, 0.03))
    C.select_only(shaft)
    bpy.ops.transform.rotate(value=math.radians(90), orient_axis='Y')
    bpy.ops.object.transform_apply(rotation=True)
    bpy.ops.transform.translate(value=(body_size[0] / 2.0, 0.0, 0.11))
    bpy.ops.object.transform_apply(location=True)

    handle = C.make_cylinder('CrankHandle', radius=0.013, depth=0.05, segments=10, center=(0.0, 0.0, 0.025))
    C.select_only(handle)
    bpy.ops.transform.rotate(value=math.radians(90), orient_axis='X')
    bpy.ops.object.transform_apply(rotation=True)
    bpy.ops.transform.translate(value=(body_size[0] / 2.0 + 0.06, -0.03, 0.11))
    bpy.ops.object.transform_apply(location=True)

    crank = C.join_objects([shaft, handle], 'Crank')
    M.assign(crank, ['M_Metal'])
    C.set_vertex_colors(crank, C.constant_tint(_METAL, alpha=0.0, jitter=0.03, rnd=rnd))

    return _finish([body, grille, knob_grp, crank], 'SM_' + variant['name'], bevel_width=0.006)


# ---------------------------------------------------------------------------
# 5. Estacion de radio (caseta de madera + mastil + cables tensores)
# ---------------------------------------------------------------------------
@_register('radio_station')
def _build_radio_station(variant, rnd):
    import bpy

    hut_w, hut_d, eave_h = 2.5, 2.0, 1.6
    roof_rise = 0.5
    roof_overhang = 0.2
    roof_thick = 0.04
    mast_height = 6.0
    mast_curvature = 0.3

    walls = C.make_box('Walls', (hut_w, hut_d, eave_h), center=(0.0, 0.0, eave_h / 2.0))
    M.assign(walls, ['M_Wood'])
    C.set_vertex_colors(walls, S.banded_tint(
        [_WOOD_LIGHT, _WOOD_MID, _WOOD_DARK, _WOOD_MID],
        'z', 0.0, eave_h, jitter=0.03, rnd=rnd))

    roof_half_w = hut_w / 2.0 + roof_overhang
    roof_len = hut_d + roof_overhang * 2.0
    slope_len = math.hypot(roof_half_w, roof_rise)
    theta = math.pi / 2.0 - math.atan2(roof_half_w, roof_rise)
    ridge_z = eave_h + roof_rise

    roof_right = C.make_box('RoofRight', (slope_len, roof_len, roof_thick),
                             center=(slope_len / 2.0, 0.0, 0.0))
    C.select_only(roof_right)
    bpy.ops.transform.rotate(value=theta, orient_axis='Y')
    bpy.ops.object.transform_apply(rotation=True)
    bpy.ops.transform.translate(value=(0.0, 0.0, ridge_z))
    bpy.ops.object.transform_apply(location=True)

    roof_left = C.make_box('RoofLeft', (slope_len, roof_len, roof_thick),
                            center=(-slope_len / 2.0, 0.0, 0.0))
    C.select_only(roof_left)
    bpy.ops.transform.rotate(value=-theta, orient_axis='Y')
    bpy.ops.object.transform_apply(rotation=True)
    bpy.ops.transform.translate(value=(0.0, 0.0, ridge_z))
    bpy.ops.object.transform_apply(location=True)

    roof = C.join_objects([roof_right, roof_left], 'Roof')
    C.merge_by_distance(roof, dist=0.002)
    M.assign(roof, ['M_Wood'])
    C.set_vertex_colors(roof, S.banded_tint(
        [_WOOD_DARK, _WOOD_MID, _WOOD_DARK],
        'x', -roof_half_w, roof_half_w, jitter=0.03, rnd=rnd))

    # mastil y cables mas gruesos (proporcion caricaturesca: no deben leerse
    # como alambre tecnico) + acabado metalico claro con oxido en parches.
    mast, _lean = C.make_curved_trunk(
        'Mast', height=mast_height, base_radius=0.076, tip_radius=0.026,
        curvature=mast_curvature, n_points=8, bevel_resolution=8,
        lean_dir=0.0, rnd=rnd, z_offset=ridge_z)
    M.assign(mast, ['M_Metal'])
    C.set_vertex_colors(mast, S.weathered_tint(
        _METAL, _METAL_RUST, seed=variant['seed'] * 2, patchiness=3.0,
        wear_amount=0.4, jitter=0.03, rnd=rnd))

    # cables tensores: parten de un punto cerca de la punta del mastil hacia
    # 3 anclajes en el suelo repartidos alrededor de la caseta.
    anchor_t = 0.85
    bend = mast_curvature * (anchor_t ** 1.6)
    wire_top = C.Vector((bend, 0.0, mast_height * anchor_t + ridge_z))
    wire_radius = 2.6
    wires = []
    for i, ang_deg in enumerate((20.0, 140.0, 260.0)):
        ang = math.radians(ang_deg)
        anchor = C.Vector((wire_radius * math.cos(ang), wire_radius * math.sin(ang), 0.0))
        direction = anchor - wire_top
        wlen = direction.length
        wire = C.make_cylinder(f'Wire{i}', radius=0.0085, depth=wlen, segments=10,
                                center=(0.0, 0.0, wlen / 2.0))
        C.orient_and_place_zaxis(wire, wire_top, direction)
        wires.append(wire)
    wire_grp = C.join_objects(wires, 'Wires')
    M.assign(wire_grp, ['M_Metal'])
    C.set_vertex_colors(wire_grp, C.constant_tint(_METAL, alpha=0.0, jitter=0.02, rnd=rnd))

    return _finish([walls, roof, mast, wire_grp], 'SM_' + variant['name'], bevel_width=0.03)


# ---------------------------------------------------------------------------
# 6. Observatorio de mareas (plataforma elevada sobre pilotes)
# ---------------------------------------------------------------------------
@_register('tide_observatory')
def _build_tide_observatory(variant, rnd):
    deck_size = 3.2
    post_height = 2.5
    post_radius = 0.092  # +15% sobre el original: pilotes menos "palillo"
    deck_thick = 0.08
    rail_height = 0.9

    posts = []
    for xi in (-1.3, 0.0, 1.3):
        for yi in (-1.3, 1.3):
            p = C.make_cylinder(f'Post{len(posts)}', radius=post_radius, depth=post_height,
                                 segments=14, center=(xi, yi, post_height / 2.0))
            posts.append(p)
    post_grp = C.join_objects(posts, 'Posts')
    M.assign(post_grp, ['M_Wood'])
    C.set_vertex_colors(post_grp, C.constant_tint(_WOOD_MID, alpha=0.0, jitter=0.04, rnd=rnd))

    deck_z = post_height + deck_thick / 2.0
    deck = C.make_box('Deck', (deck_size, deck_size, deck_thick), center=(0.0, 0.0, deck_z))
    M.assign(deck, ['M_Wood'])
    C.set_vertex_colors(deck, S.banded_tint(
        [_WOOD_LIGHT, _WOOD_MID, _WOOD_DARK, _WOOD_MID, _WOOD_LIGHT, _WOOD_MID],
        'y', -deck_size / 2.0, deck_size / 2.0, jitter=0.03, rnd=rnd))

    deck_top = post_height + deck_thick
    rail_positions = [(-1.5, -1.5), (1.5, -1.5), (-1.5, 1.5), (1.5, 1.5),
                      (-1.5, 0.0), (1.5, 0.0), (0.0, -1.5), (0.0, 1.5)]
    balusters = []
    for xi, yi in rail_positions:
        b = C.make_cylinder(f'Baluster{len(balusters)}', radius=0.026, depth=rail_height,
                             segments=12, center=(xi, yi, deck_top + rail_height / 2.0))
        balusters.append(b)

    rail_z = deck_top + rail_height * 0.85
    rail_span = deck_size - 0.2
    rail_n = C.make_box('RailN', (rail_span, 0.03, 0.03), center=(0.0, 1.5, rail_z))
    rail_s = C.make_box('RailS', (rail_span, 0.03, 0.03), center=(0.0, -1.5, rail_z))
    rail_e = C.make_box('RailE', (0.03, rail_span, 0.03), center=(1.5, 0.0, rail_z))
    rail_w = C.make_box('RailW', (0.03, rail_span, 0.03), center=(-1.5, 0.0, rail_z))
    railing = C.join_objects(balusters + [rail_n, rail_s, rail_e, rail_w], 'Railing')
    M.assign(railing, ['M_Wood'])
    C.set_vertex_colors(railing, C.constant_tint(_WOOD_MID, alpha=0.0, jitter=0.03, rnd=rnd))

    board_h = 1.2
    board = C.make_box('TideBoard', (0.9, 0.04, board_h),
                        center=(0.0, 1.52, deck_top + board_h / 2.0))

    def board_tint(v):
        # fondo de madera clara con franjas de anotacion oscuras pero
        # calidas (nunca negro puro)
        step = 0.10
        striped = math.fmod(v.co.z - deck_top, step) < step * 0.3
        rgb = (0.30, 0.20, 0.12) if striped else (0.66, 0.50, 0.24)
        return (rgb[0], rgb[1], rgb[2], 0.0)
    C.set_vertex_colors(board, board_tint)
    M.assign(board, ['M_Wood'])

    return _finish([post_grp, deck, railing, board], 'SM_' + variant['name'], bevel_width=0.025)
