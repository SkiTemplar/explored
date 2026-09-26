"""
albatros.py — El hidroavion Albatros: version completa (prologo jugable) y
cuatro restos esparcidos (fuselaje, ala, cola, motor) que el jugador
recolecta para construir la baliza de rescate.
"""
import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import common as C  # noqa: E402
import _materials as M  # noqa: E402

VARIANTS = [
    dict(name='Albatros_Full', seed=1001, builder='full',
         tri_budget=(200, 700), needs_collision=True, collision_complex=True),
    dict(name='Albatros_Fuselage_Wreck', seed=1002, builder='fuselage_wreck',
         tri_budget=(80, 400), needs_collision=True, collision_complex=True),
    dict(name='Albatros_Wing_Wreck', seed=1003, builder='wing_wreck',
         tri_budget=(30, 200), needs_collision=True),
    dict(name='Albatros_Tail_Wreck', seed=1004, builder='tail_wreck',
         tri_budget=(40, 250), needs_collision=True),
    dict(name='Albatros_Engine_Wreck', seed=1005, builder='engine_wreck',
         tri_budget=(150, 600), needs_collision=True),
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


def _drop_to_ground(obj):
    """Traslada el objeto en Z para que su punto mas bajo quede en z=0
    (regla de pivote en la base). Red de seguridad para restos con bordes
    rotos, jitter o piezas dobladas donde el calculo manual del minimo es
    fragil."""
    obj.data.update()
    min_z = min(v[2] for v in obj.bound_box)
    if abs(min_z) > 1e-6:
        C.select_only(obj)
        import bpy
        bpy.ops.transform.translate(value=(0.0, 0.0, -min_z))
        bpy.ops.object.transform_apply(location=True)
    return obj


def _finish_grounded(parts, name):
    obj = C.join_objects(parts, name) if len(parts) > 1 else parts[0]
    _drop_to_ground(obj)
    C.shade_smooth_auto(obj, angle_deg=35.0)
    C.add_basic_uv(obj)
    return obj


_METAL = (0.40, 0.43, 0.47)
_METAL_DARK = (0.22, 0.24, 0.27)
_RUST = (0.42, 0.20, 0.08)
_GLASS = (0.55, 0.74, 0.80)


def _h_cylinder(name, y0, y1, r_at_y0, r_at_y1, x=0.0, z=0.0, segments=12, cap_ends=True):
    """Cilindro (o tronco de cono) horizontal a lo largo de Y: radio
    «r_at_y0» en y=y0 y «r_at_y1» en y=y1, con el eje a altura «z» y
    desplazamiento lateral «x». make_cylinder crece nativamente en Z, asi
    que se construye a lo largo de Z y se rota +90 grados sobre X (mismo
    patron que _build_flaregun/_build_camera en small_items.py), con
    center=(x, z, -y_center) para que el radio1 quede en y1 y el radio2 en
    y0 tras la rotacion."""
    length = y1 - y0
    y_center = (y0 + y1) * 0.5
    obj = C.make_cylinder(name, radius=r_at_y1, depth=length, segments=segments,
                           center=(x, z, -y_center), cap_ends=cap_ends, radius2=r_at_y0)
    C.select_only(obj)
    import bpy
    bpy.ops.transform.rotate(value=math.radians(90), orient_axis='X')
    bpy.ops.object.transform_apply(rotation=True)
    return obj


# ---------------------------------------------------------------------------
# 1. Hidroavion Albatros completo (prologo) — ~9 m de largo, ~2.5 m de alto,
#    envergadura ~11 m. Eje del fuselaje a lo largo de Y.
# ---------------------------------------------------------------------------
@_register('full')
def _build_full(variant, rnd):
    fuselage_r = 0.55
    axis_z = 1.35  # altura del eje del fuselaje sobre la base de los flotadores

    nose = _h_cylinder('Nose', 2.6, 4.3, fuselage_r, 0.05, z=axis_z, segments=14)
    mid = _h_cylinder('MidFuselage', -2.3, 2.6, fuselage_r, fuselage_r,
                       z=axis_z, segments=14, cap_ends=False)
    tail = _h_cylinder('TailCone', -4.4, -2.3, 0.10, fuselage_r, z=axis_z, segments=14)
    fuselage = C.join_objects([nose, mid, tail], 'Fuselage')
    C.merge_by_distance(fuselage, dist=0.01)
    M.assign(fuselage, ['M_Metal'])
    C.set_vertex_colors(fuselage, C.constant_tint(_METAL, alpha=0.0, jitter=0.03, rnd=rnd))

    cabin = C.make_box('Cabin', (0.85, 1.15, 0.42), center=(0.0, 3.15, axis_z + 0.42))
    M.assign(cabin, ['M_Glass'])
    C.set_vertex_colors(cabin, C.constant_tint(_GLASS, alpha=0.25, jitter=0.02, rnd=rnd))

    engine = _h_cylinder('EngineCowl', 4.3, 4.85, 0.05, 0.34, z=axis_z, segments=14)
    M.assign(engine, ['M_Metal'])
    C.set_vertex_colors(engine, C.constant_tint(_METAL_DARK, alpha=0.0, jitter=0.03, rnd=rnd))

    hub = _h_cylinder('PropHub', 4.85, 4.98, 0.12, 0.12, z=axis_z, segments=10)
    blades = []
    for i in range(3):
        blade = C.make_box(f'Blade{i}', (0.10, 0.02, 1.05), center=(0.0, 0.0, 0.55))
        C.select_only(blade)
        import bpy
        bpy.ops.transform.rotate(value=math.radians(120 * i), orient_axis='Y')
        bpy.ops.object.transform_apply(rotation=True)
        bpy.ops.transform.translate(value=(0.0, 4.92, axis_z))
        bpy.ops.object.transform_apply(location=True)
        blades.append(blade)
    prop = C.join_objects([hub] + blades, 'Propeller')
    M.assign(prop, ['M_Metal'])
    C.set_vertex_colors(prop, C.constant_tint(_METAL_DARK, alpha=0.0, jitter=0.02, rnd=rnd))

    wing_lower = C.make_box('WingLower', (11.0, 1.7, 0.14),
                             center=(0.0, 0.2, axis_z + fuselage_r + 0.55))
    wing_upper = C.make_box('WingUpper', (9.2, 1.1, 0.08),
                             center=(0.0, 0.35, axis_z + fuselage_r + 0.62))
    wing = C.join_objects([wing_lower, wing_upper], 'Wing')
    M.assign(wing, ['M_Metal'])
    C.set_vertex_colors(wing, C.constant_tint(_METAL, alpha=0.0, jitter=0.03, rnd=rnd))

    float_r = 0.19
    floats = []
    for side in (-1, 1):
        fl = _h_cylinder(f'Float{side}', -2.1, 2.3, float_r * 0.75, float_r,
                          x=side * 1.15, z=float_r, segments=10)
        floats.append(fl)
    float_grp = C.join_objects(floats, 'Floats')
    M.assign(float_grp, ['M_Metal'])
    C.set_vertex_colors(float_grp, C.constant_tint((0.35, 0.36, 0.34), alpha=0.0, jitter=0.03, rnd=rnd))

    strut_h = float_r + axis_z - fuselage_r * 0.4
    strut_a = C.make_box('StrutA', (0.06, 0.06, strut_h), center=(0.9, 0.5, strut_h * 0.5))
    strut_b = C.make_box('StrutB', (0.06, 0.06, strut_h), center=(-0.9, 0.5, strut_h * 0.5))
    strut_grp = C.join_objects([strut_a, strut_b], 'Struts')
    M.assign(strut_grp, ['M_Metal'])
    C.set_vertex_colors(strut_grp, C.constant_tint(_METAL_DARK, alpha=0.0, jitter=0.02, rnd=rnd))

    vert_stab = C.make_box('VertStab', (0.10, 0.95, 1.15),
                            center=(0.0, -4.05, axis_z + fuselage_r * 0.5 + 0.575))
    horiz_stab = C.make_box('HorizStab', (2.6, 0.85, 0.08),
                             center=(0.0, -3.95, axis_z + fuselage_r * 0.5 + 0.30))
    tail_grp = C.join_objects([vert_stab, horiz_stab], 'TailPlanes')
    M.assign(tail_grp, ['M_Metal'])
    C.set_vertex_colors(tail_grp, C.constant_tint(_METAL, alpha=0.0, jitter=0.03, rnd=rnd))

    return _finish([fuselage, cabin, prop, wing, float_grp, strut_grp, tail_grp],
                    'SM_' + variant['name'])


# ---------------------------------------------------------------------------
# 2. Resto de fuselaje (~3.5 m), extremo desgarrado y oxidado
# ---------------------------------------------------------------------------
@_register('fuselage_wreck')
def _build_fuselage_wreck(variant, rnd):
    fuselage_r = 0.55
    length = 3.5
    body = _h_cylinder('Body', 0.0, length, fuselage_r, fuselage_r * 0.90,
                        z=fuselage_r, segments=12, cap_ends=False)
    M.assign(body, ['M_Metal'])
    C.set_vertex_colors(body, C.gradient_along_axis(
        _METAL, _RUST, 'y', 0.0, length, curve=1.4, jitter=0.04, rnd=rnd))

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

    return _finish_grounded([body, shard_grp], 'SM_' + variant['name'])


# ---------------------------------------------------------------------------
# 3. Resto de ala (~4 m), punta doblada hacia abajo
# ---------------------------------------------------------------------------
@_register('wing_wreck')
def _build_wing_wreck(variant, rnd):
    seg1_len = 2.6
    seg2_len = 1.4

    lower1 = C.make_box('WingLower1', (seg1_len, 1.5, 0.12), center=(seg1_len * 0.5, 0.0, 0.75))
    upper1 = C.make_box('WingUpper1', (seg1_len, 1.0, 0.07), center=(seg1_len * 0.5, 0.1, 0.80))
    seg1 = C.join_objects([lower1, upper1], 'WingSeg1')
    M.assign(seg1, ['M_Metal'])
    C.set_vertex_colors(seg1, C.constant_tint(_METAL, alpha=0.0, jitter=0.04, rnd=rnd))

    lower2 = C.make_box('WingLower2', (seg2_len, 1.3, 0.10), center=(seg2_len * 0.5, 0.0, 0.0))
    upper2 = C.make_box('WingUpper2', (seg2_len, 0.85, 0.06), center=(seg2_len * 0.5, 0.1, 0.03))
    seg2 = C.join_objects([lower2, upper2], 'WingSeg2')
    M.assign(seg2, ['M_Metal'])
    C.set_vertex_colors(seg2, C.gradient_along_axis(
        _METAL, _RUST, 'x', 0.0, seg2_len, curve=1.2, jitter=0.05, rnd=rnd))
    C.select_only(seg2)
    import bpy
    bpy.ops.transform.rotate(value=math.radians(-38.0), orient_axis='Y')
    bpy.ops.object.transform_apply(rotation=True)
    bpy.ops.transform.translate(value=(seg1_len, 0.0, 0.75))
    bpy.ops.object.transform_apply(location=True)

    return _finish_grounded([seg1, seg2], 'SM_' + variant['name'])


# ---------------------------------------------------------------------------
# 4. Resto de cola: tramo trasero de fuselaje + estabilizadores, torcido
# ---------------------------------------------------------------------------
@_register('tail_wreck')
def _build_tail_wreck(variant, rnd):
    fuselage_r = 0.5
    length = 2.6
    body = _h_cylinder('TailSection', 0.0, length, fuselage_r * 0.15, fuselage_r,
                        z=fuselage_r, segments=12)
    M.assign(body, ['M_Metal'])
    C.set_vertex_colors(body, C.gradient_along_axis(
        _RUST, _METAL, 'y', 0.0, length, curve=1.3, jitter=0.04, rnd=rnd))

    vert_stab = C.make_box('VertStab', (0.09, 0.8, 1.0), center=(0.0, length * 0.15, fuselage_r + 0.5))
    horiz_stab = C.make_box('HorizStab', (2.2, 0.7, 0.07), center=(0.0, length * 0.10, fuselage_r + 0.25))
    stabs = C.join_objects([vert_stab, horiz_stab], 'Stabilizers')
    M.assign(stabs, ['M_Metal'])
    C.set_vertex_colors(stabs, C.constant_tint(_METAL, alpha=0.0, jitter=0.04, rnd=rnd))
    C.select_only(stabs)
    import bpy
    bpy.ops.transform.rotate(value=math.radians(9.0), orient_axis='Y')
    bpy.ops.object.transform_apply(rotation=True)

    return _finish_grounded([body, stabs], 'SM_' + variant['name'])


# ---------------------------------------------------------------------------
# 5. Resto de motor: bloque radial + helice de 2 palas dobladas, muy oxidado
# ---------------------------------------------------------------------------
@_register('engine_wreck')
def _build_engine_wreck(variant, rnd):
    hub_r = 0.22
    hub = C.make_cylinder('Crankcase', radius=hub_r, depth=0.30, segments=14,
                           center=(0.0, 0.0, 0.15))
    M.assign(hub, ['M_Metal'])
    C.set_vertex_colors(hub, C.constant_tint(_METAL_DARK, alpha=0.0, jitter=0.03, rnd=rnd))

    n_cyl = rnd.randint(6, 8)
    cylinders = []
    for i in range(n_cyl):
        ang = 2.0 * math.pi * i / n_cyl
        cx = math.cos(ang) * (hub_r + 0.14)
        cy = math.sin(ang) * (hub_r + 0.14)
        cyl = C.make_cylinder(f'Cyl{i}', radius=0.075, depth=0.30, segments=8,
                               center=(cx, cy, 0.15))
        cylinders.append(cyl)
    cyl_grp = C.join_objects(cylinders, 'RadialCylinders')
    M.assign(cyl_grp, ['M_Metal'])
    C.set_vertex_colors(cyl_grp, C.gradient_along_axis(
        _RUST, _METAL_DARK, 'z', 0.0, 0.30, curve=1.0, jitter=0.05, rnd=rnd))

    blades = []
    for i, bend in enumerate((26.0, -18.0)):
        blade = C.make_box(f'PropBlade{i}', (0.10, 0.9, 0.02), center=(0.0, 0.45, 0.0))
        C.select_only(blade)
        import bpy
        bpy.ops.transform.rotate(value=math.radians(180.0 * i + 6.0), orient_axis='Z')
        bpy.ops.object.transform_apply(rotation=True)
        bpy.ops.transform.rotate(value=math.radians(bend), orient_axis='X')
        bpy.ops.object.transform_apply(rotation=True)
        bpy.ops.transform.translate(value=(0.0, 0.0, hub_r + 0.15))
        bpy.ops.object.transform_apply(location=True)
        blades.append(blade)
    blade_grp = C.join_objects(blades, 'BentBlades')
    M.assign(blade_grp, ['M_Metal'])
    C.set_vertex_colors(blade_grp, C.constant_tint(_RUST, alpha=0.0, jitter=0.05, rnd=rnd))

    return _finish_grounded([hub, cyl_grp, blade_grp], 'SM_' + variant['name'])
