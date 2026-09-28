"""
albatros.py — hidroavion Albatros (chárter que se estrella en el prólogo)
y sus 4 restos narrativos: fuselaje, ala, cola y motor, esparcidos por el
archipiélago para la baliza de rescate.

Pasada de arte 2026-09-26 (revisión visual): silueta tipo De Havilland
Beaver (ala alta arriostrada con tirantes en V, fuselaje panzudo y
redondeado, capó de motor radial romo, flotadores gemelos), librea pintada
(carena crema + franja de color) en vez de metal gris liso, y bisel suave
en las piezas finales para que las cajas no se lean como "un plano en
cruz" — la silueta cuadrada y el gris apagado eran justo la queja de la
primera pasada.
"""
import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import _materials as M  # noqa: E402
import _shapes as S  # noqa: E402
import common as C  # noqa: E402

VARIANTS = [
    dict(name='Albatros_Full', seed=1001, builder='full',
         tri_budget=(300, 3600), needs_collision=True, collision_complex=True),
    dict(name='Albatros_Fuselage_Wreck', seed=1002, builder='fuselage_wreck',
         tri_budget=(100, 900), needs_collision=True, collision_complex=True),
    dict(name='Albatros_Wing_Wreck', seed=1003, builder='wing_wreck',
         tri_budget=(60, 700), needs_collision=True),
    dict(name='Albatros_Tail_Wreck', seed=1004, builder='tail_wreck',
         tri_budget=(60, 700), needs_collision=True),
    dict(name='Albatros_Engine_Wreck', seed=1005, builder='engine_wreck',
         tri_budget=(150, 1500), needs_collision=True),
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


def _drop_to_ground(obj):
    """Traslada el objeto en Z para que su punto más bajo quede en z=0
    (regla de pivote en la base). Red de seguridad para restos con bordes
    rotos, jitter o piezas dobladas donde el cálculo manual del mínimo es
    frágil."""
    obj.data.update()
    min_z = min(v[2] for v in obj.bound_box)
    if abs(min_z) > 1e-6:
        C.select_only(obj)
        import bpy
        bpy.ops.transform.translate(value=(0.0, 0.0, -min_z))
        bpy.ops.object.transform_apply(location=True)
    return obj


def _finish(parts, name, bevel_width=0.014, bevel_segments=2, grounded=True):
    """Une, bisela (redondea las aristas duras de las cajas: la parte del
    look "animado" que más se notaba) y solo entonces sombrea suave + UV.
    El bisel debe ir ANTES de shade_smooth_auto para que el ángulo de
    sombreado suave vea las caras nuevas del bisel."""
    obj = C.join_objects(parts, name) if len(parts) > 1 else parts[0]
    if grounded:
        _drop_to_ground(obj)
    S.bevel_obj(obj, width=bevel_width, segments=bevel_segments, limit_angle_deg=40.0)
    C.shade_smooth_auto(obj, angle_deg=32.0)
    C.add_basic_uv(obj)
    return obj


# Librea: carena clara cálida + franja de color vivo + filete de acento.
_HULL_CREAM = (0.90, 0.86, 0.74)
_STRIPE_RED = (0.78, 0.24, 0.11)
_STRIPE_GOLD = (0.86, 0.64, 0.16)
_METAL_WARM = (0.62, 0.60, 0.55)
_METAL_DARK = (0.30, 0.29, 0.27)
_RUST = (0.55, 0.26, 0.10)
_GLASS = (0.55, 0.80, 0.86)
_WOOD_PROP = (0.62, 0.42, 0.20)


def _h_cylinder(name, y0, y1, r_at_y0, r_at_y1, x=0.0, z=0.0, segments=14, cap_ends=True):
    """Cilindro (o tronco de cono) horizontal a lo largo de Y: radio
    «r_at_y0» en y=y0 y «r_at_y1» en y=y1, con el eje a altura «z» y
    desplazamiento lateral «x». make_cylinder crece nativamente en Z, así
    que se construye a lo largo de Z y se rota +90 grados sobre X."""
    length = y1 - y0
    y_center = (y0 + y1) * 0.5
    obj = C.make_cylinder(name, radius=r_at_y1, depth=length, segments=segments,
                           center=(x, z, -y_center), cap_ends=cap_ends, radius2=r_at_y0)
    C.select_only(obj)
    import bpy
    bpy.ops.transform.rotate(value=math.radians(90), orient_axis='X')
    bpy.ops.object.transform_apply(rotation=True)
    return obj


def _paint_fuselage(rnd, axis_z, fuselage_r):
    """Franja de color a la altura del eje del fuselaje (cheatline clásica
    de hidroavión de los años 30-50), con un filete dorado fino encima."""
    return S.stripe_tint(
        _HULL_CREAM, _STRIPE_RED, 'z',
        axis_z - fuselage_r * 0.30, axis_z + fuselage_r * 0.30,
        accent_rgb=_STRIPE_GOLD, accent_at=axis_z + fuselage_r * 0.34, accent_width=0.04,
        jitter=0.015, rnd=rnd)


# ---------------------------------------------------------------------------
# 1. Hidroavión Albatros completo (prólogo) — silueta De Havilland Beaver:
#    ala alta arriostrada, fuselaje panzudo, flotadores gemelos, capó
#    redondo con hélice de 3 palas. ~9 m de largo, envergadura ~11 m.
# ---------------------------------------------------------------------------
@_register('full')
def _build_full(variant, rnd):
    import bpy
    fuselage_r = 0.66
    axis_z = 1.55  # altura del eje del fuselaje sobre la base de los flotadores

    nose = _h_cylinder('Nose', 2.7, 4.0, fuselage_r, fuselage_r * 0.55, z=axis_z, segments=16)
    mid = _h_cylinder('MidFuselage', -2.2, 2.7, fuselage_r, fuselage_r,
                       z=axis_z, segments=16, cap_ends=False)
    tail = _h_cylinder('TailCone', -4.3, -2.2, 0.09, fuselage_r, z=axis_z, segments=16)
    fuselage = C.join_objects([nose, mid, tail], 'Fuselage')
    C.merge_by_distance(fuselage, dist=0.01)
    M.assign(fuselage, ['M_Metal'])
    C.set_vertex_colors(fuselage, _paint_fuselage(rnd, axis_z, fuselage_r))

    # capó de motor redondo (romo, tipo radial) + pequeño cono de morro
    cowl = _h_cylinder('EngineCowl', 4.0, 4.55, fuselage_r * 0.55, fuselage_r * 0.42,
                        z=axis_z, segments=16)
    spinner = _h_cylinder('Spinner', 4.55, 4.80, fuselage_r * 0.42, 0.04,
                           z=axis_z, segments=12)
    cowl_grp = C.join_objects([cowl, spinner], 'Cowling')
    M.assign(cowl_grp, ['M_Metal'])
    C.set_vertex_colors(cowl_grp, C.constant_tint(_METAL_DARK, alpha=0.0, jitter=0.03, rnd=rnd))

    # cabina: dos ventanas enmarcadas en vez de un único cristal liso, para
    # que se lea como cabina de verdad y no como un bloque de vidrio.
    win_frame = C.make_box('WinFrame', (0.92, 1.30, 0.50), center=(0.0, 3.05, axis_z + 0.46))
    M.assign(win_frame, ['M_Metal'])
    C.set_vertex_colors(win_frame, C.constant_tint(_METAL_DARK, alpha=0.0, jitter=0.02, rnd=rnd))
    win_l = C.make_box('WinL', (0.94, 0.52, 0.32), center=(0.0, 2.78, axis_z + 0.47))
    win_r = C.make_box('WinR', (0.94, 0.52, 0.32), center=(0.0, 3.32, axis_z + 0.47))
    glass_grp = C.join_objects([win_l, win_r], 'CabinGlass')
    M.assign(glass_grp, ['M_Glass'])
    C.set_vertex_colors(glass_grp, C.constant_tint(_GLASS, alpha=0.30, jitter=0.02, rnd=rnd))

    hub = _h_cylinder('PropHub', 4.80, 4.94, 0.13, 0.13, z=axis_z, segments=10)
    blades = []
    for i in range(3):
        blade = C.make_box(f'Blade{i}', (0.13, 0.025, 1.0), center=(0.0, 0.0, 0.52))
        me = blade.data
        z_max = max(v.co.z for v in me.vertices)
        for v in me.vertices:  # ahusa la punta de la pala (menos "tabla", más pala)
            if v.co.z >= z_max - 1e-6:
                v.co.x *= 0.35
        me.update()
        C.select_only(blade)
        bpy.ops.transform.rotate(value=math.radians(120 * i), orient_axis='Y')
        bpy.ops.object.transform_apply(rotation=True)
        bpy.ops.transform.translate(value=(0.0, 4.90, axis_z))
        bpy.ops.object.transform_apply(location=True)
        blades.append(blade)
    prop = C.join_objects([hub] + blades, 'Propeller')
    C.merge_by_distance(prop, dist=0.002)
    M.assign(prop, ['M_Wood'])
    C.set_vertex_colors(prop, C.constant_tint(_WOOD_PROP, alpha=0.0, jitter=0.03, rnd=rnd))

    # ala alta: caja central + puntas ahusadas, con un filete de color en
    # el borde de ataque (la franja que "explica" la librea en vista de
    # planta) en vez de un rectángulo gris liso.
    wing_z = axis_z + fuselage_r + 0.62
    wing = C.make_box('Wing', (11.0, 1.55, 0.16), center=(0.0, 0.15, wing_z))
    me = wing.data
    x_max = max(v.co.x for v in me.vertices)
    for v in me.vertices:  # puntas de ala ahusadas en cuerda (menos "tablón")
        if abs(v.co.x) >= x_max - 1e-6:
            v.co.y *= 0.45
    me.update()
    M.assign(wing, ['M_Fabric'])
    C.set_vertex_colors(wing, S.stripe_tint(
        _HULL_CREAM, _STRIPE_RED, 'y', -0.55, -0.15, jitter=0.02, rnd=rnd))

    # tirantes en V (arriostrado clásico del ala alta): del fuselaje al
    # ala, dos por lado — sin esto el ala "flota" y se lee como una cruz.
    # orient_and_place_zaxis coloca el extremo base del tirante (local
    # z=0) exactamente en «base» y lo extiende «length» a lo largo de
    # «direction»: a diferencia de rotar con bpy.ops.transform (que gira
    # sobre el punto medio de la selección, no sobre el extremo), esto no
    # depende del pivote de la escena y coloca el tirante donde se pide.
    struts = []
    for side in (-1, 1):
        base = C.Vector((side * 0.55, 0.6, axis_z - fuselage_r * 0.2))
        top = C.Vector((side * 3.0, 0.15, wing_z))
        direction = top - base
        length = direction.length
        strut, _ld = C.make_curved_trunk(
            f'WingStrut{side}', height=length, base_radius=0.035, tip_radius=0.03,
            curvature=0.0, n_points=3, bevel_resolution=4, rnd=rnd, lean_dir=0.0)
        C.orient_and_place_zaxis(strut, base, direction.normalized())
        struts.append(strut)
    strut_grp = C.join_objects(struts, 'WingStruts')
    M.assign(strut_grp, ['M_Metal'])
    C.set_vertex_colors(strut_grp, C.constant_tint(_METAL_WARM, alpha=0.0, jitter=0.02, rnd=rnd))

    float_r = 0.22
    floats = []
    for side in (-1, 1):
        fl = _h_cylinder(f'Float{side}', -2.15, 2.35, float_r * 0.55, float_r,
                          x=side * 1.20, z=float_r, segments=12)
        floats.append(fl)
    float_grp = C.join_objects(floats, 'Floats')
    M.assign(float_grp, ['M_Metal'])
    C.set_vertex_colors(float_grp, S.stripe_tint(
        _HULL_CREAM, _METAL_DARK, 'z', 0.0, float_r * 0.35, jitter=0.02, rnd=rnd))

    strut_h = float_r + axis_z - fuselage_r * 0.35
    float_struts = []
    for side in (-1, 1):
        for yoff in (-1.3, 1.3):
            st = C.make_box(f'FloatStrut{side}_{yoff}', (0.05, 0.05, strut_h),
                             center=(side * 1.0, yoff, strut_h * 0.5 + float_r * 0.4))
            float_struts.append(st)
    fstrut_grp = C.join_objects(float_struts, 'FloatStruts')
    M.assign(fstrut_grp, ['M_Metal'])
    C.set_vertex_colors(fstrut_grp, C.constant_tint(_METAL_DARK, alpha=0.0, jitter=0.02, rnd=rnd))

    vert_stab = C.make_box('VertStab', (0.11, 0.95, 1.05),
                            center=(0.0, -4.0, axis_z + fuselage_r * 0.5 + 0.525))
    horiz_stab = C.make_box('HorizStab', (2.7, 0.85, 0.09),
                             center=(0.0, -3.9, axis_z + fuselage_r * 0.5 + 0.28))
    tail_grp = C.join_objects([vert_stab, horiz_stab], 'TailPlanes')
    M.assign(tail_grp, ['M_Fabric'])
    C.set_vertex_colors(tail_grp, S.stripe_tint(
        _HULL_CREAM, _STRIPE_RED, 'z', axis_z + fuselage_r * 0.5, axis_z + fuselage_r * 0.5 + 0.5,
        jitter=0.02, rnd=rnd))

    return _finish([fuselage, cowl_grp, win_frame, glass_grp, prop, wing, strut_grp,
                     float_grp, fstrut_grp, tail_grp], 'SM_' + variant['name'],
                    bevel_width=0.02, grounded=False)


# ---------------------------------------------------------------------------
# 2. Resto de fuselaje (~3.5 m): tramo pintado con parches de óxido
#    orgánicos (no un degradado uniforme) y borde desgarrado.
# ---------------------------------------------------------------------------
@_register('fuselage_wreck')
def _build_fuselage_wreck(variant, rnd):
    fuselage_r = 0.62
    length = 3.5
    body = _h_cylinder('Body', 0.0, length, fuselage_r, fuselage_r * 0.90,
                        z=fuselage_r, segments=14, cap_ends=False)
    M.assign(body, ['M_Metal'])
    C.set_vertex_colors(body, S.weathered_tint(
        _HULL_CREAM, _RUST, seed=variant['seed'], patchiness=2.6, wear_amount=0.55,
        jitter=0.03, rnd=rnd))

    shards = []
    n_shards = rnd.randint(7, 10)
    for i in range(n_shards):
        ang = (2.0 * math.pi * i / n_shards) + rnd.uniform(-0.25, 0.25)
        sx = fuselage_r * 0.95 * math.cos(ang)
        sz = fuselage_r + fuselage_r * 0.95 * math.sin(ang)
        w = rnd.uniform(0.08, 0.22)
        h = rnd.uniform(0.10, 0.30)
        shard = C.make_box(f'Shard{i}', (w, 0.02, h),
                            center=(sx, length + rnd.uniform(-0.05, 0.12), sz))
        shards.append(shard)
    shard_grp = C.join_objects(shards, 'BrokenEdge')
    M.assign(shard_grp, ['M_Metal'])
    C.set_vertex_colors(shard_grp, C.constant_tint(_RUST, alpha=0.0, jitter=0.06, rnd=rnd))

    return _finish([body, shard_grp], 'SM_' + variant['name'], bevel_width=0.01)


# ---------------------------------------------------------------------------
# 3. Resto de ala (~4 m): punta doblada, franja de color aún visible bajo
#    el óxido — cuenta una historia, no solo "caja gris rota".
# ---------------------------------------------------------------------------
@_register('wing_wreck')
def _build_wing_wreck(variant, rnd):
    import bpy
    span = 4.0
    root = C.make_box('WingRoot', (span * 0.62, 1.2, 0.16), center=(-span * 0.19, 0.0, 0.08))
    tip = C.make_box('WingTip', (span * 0.42, 0.95, 0.13), center=(span * 0.29, -0.05, 0.30))
    C.select_only(tip)
    bpy.ops.transform.rotate(value=math.radians(-22.0), orient_axis='Y')
    bpy.ops.object.transform_apply(rotation=True)
    wing = C.join_objects([root, tip], 'Wing')
    C.merge_by_distance(wing, dist=0.02)
    M.assign(wing, ['M_Fabric'])
    C.set_vertex_colors(wing, S.weathered_tint(
        _HULL_CREAM, _RUST, seed=variant['seed'] + 1, patchiness=2.2, wear_amount=0.45,
        jitter=0.03, rnd=rnd))

    stripe = C.make_box('StripeRemnant', (span * 0.5, 0.30, 0.02), center=(-span * 0.1, 0.35, 0.17))
    M.assign(stripe, ['M_Fabric'])
    C.set_vertex_colors(stripe, C.constant_tint(_STRIPE_RED, alpha=0.0, jitter=0.03, rnd=rnd))

    return _finish([wing, stripe], 'SM_' + variant['name'], bevel_width=0.012)


# ---------------------------------------------------------------------------
# 4. Resto de cola: tramo trasero de fuselaje + estabilizadores, torcido
# ---------------------------------------------------------------------------
@_register('tail_wreck')
def _build_tail_wreck(variant, rnd):
    import bpy
    fuselage_r = 0.55
    length = 2.6
    body = _h_cylinder('TailSection', 0.0, length, fuselage_r * 0.15, fuselage_r,
                        z=fuselage_r, segments=14)
    M.assign(body, ['M_Metal'])
    C.set_vertex_colors(body, S.weathered_tint(
        _HULL_CREAM, _RUST, seed=variant['seed'] + 2, patchiness=2.4, wear_amount=0.5,
        jitter=0.03, rnd=rnd))

    vert_stab = C.make_box('VertStab', (0.10, 0.8, 1.0), center=(0.0, length * 0.15, fuselage_r + 0.5))
    horiz_stab = C.make_box('HorizStab', (2.2, 0.7, 0.08), center=(0.0, length * 0.10, fuselage_r + 0.25))
    stabs = C.join_objects([vert_stab, horiz_stab], 'Stabilizers')
    M.assign(stabs, ['M_Fabric'])
    C.set_vertex_colors(stabs, S.stripe_tint(
        _HULL_CREAM, _STRIPE_RED, 'z', fuselage_r + 0.5, fuselage_r + 0.9, jitter=0.02, rnd=rnd))
    C.select_only(stabs)
    bpy.ops.transform.rotate(value=math.radians(9.0), orient_axis='Y')
    bpy.ops.object.transform_apply(rotation=True)

    return _finish([body, stabs], 'SM_' + variant['name'], bevel_width=0.012)


# ---------------------------------------------------------------------------
# 5. Resto de motor: bloque radial + 7 cilindros + hélice doblada
# ---------------------------------------------------------------------------
@_register('engine_wreck')
def _build_engine_wreck(variant, rnd):
    import bpy
    core_r = 0.32
    core = C.make_cylinder('Core', radius=core_r, depth=0.30, segments=16,
                            center=(0.0, 0.0, 0.15))
    M.assign(core, ['M_Metal'])
    C.set_vertex_colors(core, S.weathered_tint(
        _METAL_WARM, _RUST, seed=variant['seed'] + 3, patchiness=3.2, wear_amount=0.5,
        jitter=0.03, rnd=rnd))

    cyls = []
    n = 7
    for i in range(n):
        ang = 2.0 * math.pi * i / n
        cx = math.cos(ang) * core_r * 1.15
        cy = math.sin(ang) * core_r * 1.15
        cyl = C.make_cylinder(f'Cyl{i}', radius=0.10, depth=0.34, segments=10,
                               center=(0.0, 0.0, 0.17))
        C.select_only(cyl)
        bpy.ops.transform.translate(value=(cx, cy, 0.0))
        bpy.ops.object.transform_apply(location=True)
        cyls.append(cyl)
    cyl_grp = C.join_objects(cyls, 'Cylinders')
    M.assign(cyl_grp, ['M_Metal'])
    C.set_vertex_colors(cyl_grp, S.weathered_tint(
        _METAL_WARM, _RUST, seed=variant['seed'] + 4, patchiness=3.0, wear_amount=0.55,
        jitter=0.03, rnd=rnd))

    hub = C.make_cylinder('Hub', radius=0.10, depth=0.10, segments=10, center=(0.0, 0.0, 0.35))
    blades = []
    for i in range(2):  # una pala arrancada: solo quedan 2
        blade = C.make_box(f'Blade{i}', (0.13, 0.02, 0.75), center=(0.0, 0.0, 0.40))
        C.select_only(blade)
        bpy.ops.transform.rotate(value=math.radians(180 * i + 25), orient_axis='Y')
        bpy.ops.object.transform_apply(rotation=True)
        blades.append(blade)
    prop = C.join_objects([hub] + blades, 'BentProp')
    M.assign(prop, ['M_Wood'])
    C.set_vertex_colors(prop, C.constant_tint(_WOOD_PROP, alpha=0.0, jitter=0.03, rnd=rnd))

    return _finish([core, cyl_grp, prop], 'SM_' + variant['name'], bevel_width=0.012)
