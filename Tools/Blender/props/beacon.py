"""
beacon.py — baliza de rescate del Acto III, montada en lo alto del faro con
piezas recuperadas del Albatros (tubos de aluminio, chapa, cable).

4 props: armazón trípode, reflector parabólico, bidón de combustible/señales
y la versión final montada (armazón + reflector + bidón + bandera + antena
improvisada). Los tres primeros son las piezas sueltas que el jugador
recupera; Beacon_Complete es el resultado narrativo de montarlas.
"""
import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import _materials as M  # noqa: E402
import common as C  # noqa: E402

CATEGORY = 'beacon'

VARIANTS = [
    dict(name='Beacon_Frame', seed=1201, builder='frame',
         tri_budget=(150, 600), needs_collision=True),
    dict(name='Beacon_Reflector', seed=1202, builder='reflector',
         tri_budget=(50, 300), needs_collision=True),
    dict(name='Beacon_FuelDrum', seed=1203, builder='fuel_drum',
         tri_budget=(80, 400), needs_collision=True),
    dict(name='Beacon_Complete', seed=1204, builder='complete',
         tri_budget=(250, 900), needs_collision=True, collision_complex=True),
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


def _strut(name, base, tip, radius, segments=10, radius2=None):
    """Tubo recto entre dos puntos cualesquiera: construye un cilindro que
    crece en +Z desde el origen y lo reorienta con orient_and_place_zaxis
    (el mismo patrón que usan los troncos curvados para mástiles/patas)."""
    base_v = C.Vector(base)
    tip_v = C.Vector(tip)
    length = (tip_v - base_v).length
    obj = C.make_cylinder(name, radius=radius, depth=length, segments=segments,
                           center=(0.0, 0.0, length / 2.0), radius2=radius2)
    C.orient_and_place_zaxis(obj, base_v, (tip_v - base_v).normalized())
    return obj


# ---------------------------------------------------------------------------
# Geometría compartida (piezas sueltas y versión reducida para Complete)
# ---------------------------------------------------------------------------
def _build_frame(rnd, height=1.9, base_radius=0.55, leg_r=0.035, leg_r2=0.024,
                  brace_r=0.016, z0=0.0, segments=12):
    top_z = z0 + height
    top_radius = height * 0.045  # las 3 patas convergen cerca del centro,
    legs = []                    # no en un único punto (evita geometría degenerada)
    for i in range(3):
        ang = math.radians(90.0 + i * 120.0)
        base_pt = (base_radius * math.cos(ang), base_radius * math.sin(ang), z0)
        tip_pt = (top_radius * math.cos(ang), top_radius * math.sin(ang), top_z)
        legs.append(_strut(f'Leg{i}', base_pt, tip_pt, leg_r, segments=segments, radius2=leg_r2))

    braces = []
    brace_z = z0 + height * 0.38
    brace_radius = base_radius * 0.62
    brace_segments = max(8, segments - 2)
    for i in range(3):
        ang_a = math.radians(90.0 + i * 120.0)
        ang_b = math.radians(90.0 + (i + 1) * 120.0)
        pa = (brace_radius * math.cos(ang_a), brace_radius * math.sin(ang_a), brace_z)
        pb = (brace_radius * math.cos(ang_b), brace_radius * math.sin(ang_b), brace_z)
        braces.append(_strut(f'Brace{i}', pa, pb, brace_r, segments=brace_segments))

    platform_size = top_radius * 3.2
    platform = C.make_box('Platform', (platform_size, platform_size, height * 0.02),
                           center=(0.0, 0.0, top_z + height * 0.01))

    obj = C.join_objects(legs + braces + [platform], 'Frame')
    C.merge_by_distance(obj, dist=0.004)
    M.assign(obj, ['M_Metal'])
    C.set_vertex_colors(obj, C.constant_tint((0.58, 0.58, 0.60), alpha=0.0, jitter=0.04, rnd=rnd))
    return obj


def _build_reflector(rnd, radius=0.5, depth=0.15, tip_radius=0.06, z0=0.0, segments=16):
    dish = C.make_cylinder('Dish', radius=radius, depth=depth, segments=segments,
                            center=(0.0, 0.0, z0 + depth / 2.0), radius2=tip_radius)
    rim_depth = depth * 0.18
    rim = C.make_cylinder('Rim', radius=radius * 1.02, depth=rim_depth, segments=segments,
                           center=(0.0, 0.0, z0 + rim_depth / 2.0), cap_ends=False)
    obj = C.join_objects([dish, rim], 'Reflector')
    C.merge_by_distance(obj, dist=0.003)
    M.assign(obj, ['M_Metal'])
    C.set_vertex_colors(obj, C.gradient_along_axis(
        (0.72, 0.73, 0.75), (0.40, 0.40, 0.43), 'z', z0, z0 + depth,
        curve=1.0, jitter=0.02, rnd=rnd))
    return obj


def _build_fuel_drum(rnd, radius=0.22, height=0.6, z0=0.0, segments=14):
    body = C.make_cylinder('Body', radius=radius, depth=height, segments=segments,
                            center=(0.0, 0.0, z0 + height / 2.0))
    ring_depth = height * 0.035
    ring_a = C.make_cylinder('RingA', radius=radius * 1.06, depth=ring_depth, segments=segments,
                              center=(0.0, 0.0, z0 + height * 0.32), cap_ends=False)
    ring_b = C.make_cylinder('RingB', radius=radius * 1.06, depth=ring_depth, segments=segments,
                              center=(0.0, 0.0, z0 + height * 0.68), cap_ends=False)
    lid = C.make_cylinder('Lid', radius=radius * 0.92, depth=height * 0.02, segments=segments,
                           center=(0.0, 0.0, z0 + height + height * 0.01))
    obj = C.join_objects([body, ring_a, ring_b, lid], 'FuelDrum')
    C.merge_by_distance(obj, dist=0.003)
    M.assign(obj, ['M_Metal'])
    C.set_vertex_colors(obj, C.gradient_along_axis(
        (0.42, 0.22, 0.09), (0.34, 0.34, 0.36), 'z', z0, z0 + height,
        curve=1.4, jitter=0.03, rnd=rnd))
    return obj


# ---------------------------------------------------------------------------
# 1. Armazón trípode (aluminio reciclado del Albatros)
# ---------------------------------------------------------------------------
@_register('frame')
def _build_beacon_frame(variant, rnd):
    frame = _build_frame(rnd)
    return _finish([frame], 'SM_' + variant['name'])


# ---------------------------------------------------------------------------
# 2. Reflector parabólico (chapa pulida recuperada)
# ---------------------------------------------------------------------------
@_register('reflector')
def _build_beacon_reflector(variant, rnd):
    reflector = _build_reflector(rnd)
    return _finish([reflector], 'SM_' + variant['name'])


# ---------------------------------------------------------------------------
# 3. Bidón de combustible/señales (oxidado en la base)
# ---------------------------------------------------------------------------
@_register('fuel_drum')
def _build_beacon_fuel_drum(variant, rnd):
    drum = _build_fuel_drum(rnd)
    return _finish([drum], 'SM_' + variant['name'])


# ---------------------------------------------------------------------------
# 4. Versión montada (armazón + reflector + bidón + antena + bandera)
# ---------------------------------------------------------------------------
@_register('complete')
def _build_beacon_complete(variant, rnd):
    drum_h = 0.5
    drum = _build_fuel_drum(rnd, radius=0.20, height=drum_h, z0=0.0, segments=12)

    frame_z0 = drum_h * 0.85  # el trípode se apoya sobre el bidón, con solape
    frame_h = 1.5
    frame = _build_frame(rnd, height=frame_h, base_radius=0.34, leg_r=0.028,
                          leg_r2=0.019, brace_r=0.014, z0=frame_z0, segments=10)
    platform_z = frame_z0 + frame_h

    reflector = _build_reflector(rnd, radius=0.28, depth=0.09, tip_radius=0.032,
                                  z0=platform_z, segments=12)

    mast_base_z = platform_z + 0.03
    mast_top_z = mast_base_z + 0.6
    mast = C.make_cylinder('Mast', radius=0.009, depth=mast_top_z - mast_base_z, segments=8,
                            center=(0.0, 0.0, (mast_base_z + mast_top_z) / 2.0), radius2=0.005)
    M.assign(mast, ['M_Metal'])
    C.set_vertex_colors(mast, C.constant_tint((0.5, 0.5, 0.52), alpha=0.0, jitter=0.02, rnd=rnd))

    flag = C.make_box('Flag', (0.16, 0.014, 0.11), center=(0.09, 0.0, mast_top_z - 0.1))
    M.assign(flag, ['M_Fabric'])
    C.set_vertex_colors(flag, C.constant_tint((0.52, 0.16, 0.14), alpha=0.0, jitter=0.05, rnd=rnd))

    return _finish([drum, frame, reflector, mast, flag], 'SM_' + variant['name'])
