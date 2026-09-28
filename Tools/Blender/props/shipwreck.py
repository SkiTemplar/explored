"""
shipwreck.py — pecio de un velero de los años 60 encallado en el arrecife de
Arenas Blancas.

4 props: casco partido, mástil quebrado tumbado, vela de lona caída y ancla
tipo almirantazgo.
"""
import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import _materials as M  # noqa: E402
import common as C  # noqa: E402

VARIANTS = [
    dict(name='Shipwreck_Hull', seed=1701, builder='hull',
         tri_budget=(60, 400), needs_collision=True, collision_complex=True),
    dict(name='Shipwreck_Mast', seed=1702, builder='mast',
         tri_budget=(200, 1400), needs_collision=True),
    dict(name='Shipwreck_Sail', seed=1703, builder='sail',
         tri_budget=(300, 1200), needs_collision=False),
    dict(name='Shipwreck_Anchor', seed=1704, builder='anchor',
         tri_budget=(50, 300), needs_collision=True),
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


def _settle_to_ground(obj):
    """Traslada el objeto en Z para que su vertice mas bajo quede en z=0.
    Se usa tras rotaciones manuales (mastil tumbado) o ruido organico
    (vela) donde la cota final mas baja no es trivial de calcular a priori;
    para geometria construida directamente con las cotas correctas (casco,
    ancla ya asentada) no hace falta."""
    min_z = min(v.co.z for v in obj.data.vertices)
    if abs(min_z) > 1e-6:
        for v in obj.data.vertices:
            v.co.z -= min_z
        obj.data.update()
    return obj


def _plank_bands(base_rgb, dark_rgb, band_height=0.15, jitter=0.06, rnd=None):
    """Color_fn: bandas horizontales periodicas segun la altura local (Z),
    sugiriendo tablones de casco sin modelar geometria adicional, con
    jitter fuerte para dar aspecto de madera muy desgastada y grisacea."""
    cache = {}

    def fn(v):
        if v.index not in cache:
            cache[v.index] = rnd.uniform(-jitter, jitter) if (jitter and rnd is not None) else 0.0
        j = cache[v.index]
        band = (math.sin(v.co.z / band_height * math.pi * 2.0) + 1.0) * 0.5
        r = max(0.0, min(1.0, base_rgb[0] + (dark_rgb[0] - base_rgb[0]) * (1.0 - band) + j))
        g = max(0.0, min(1.0, base_rgb[1] + (dark_rgb[1] - base_rgb[1]) * (1.0 - band) + j))
        b = max(0.0, min(1.0, base_rgb[2] + (dark_rgb[2] - base_rgb[2]) * (1.0 - band) + j))
        return (r, g, b, 0.0)
    return fn


def _distance_tint(base_rgb, tip_rgb, max_dist, jitter=0.0, rnd=None):
    """Color_fn tipo gradient_along_axis pero basado en la distancia al
    origen en vez de un eje local fijo: para piezas tumbadas/rotadas a mano
    (mastil) donde el eje de crecimiento original ya no coincide con
    ningun eje local tras aplicar la rotacion."""
    cache = {}

    def fn(v):
        if v.index not in cache:
            cache[v.index] = rnd.uniform(-jitter, jitter) if (jitter and rnd is not None) else 0.0
        j = cache[v.index]
        dist = math.sqrt(v.co.x ** 2 + v.co.y ** 2 + v.co.z ** 2)
        t = max(0.0, min(1.0, dist / max_dist))
        r = max(0.0, min(1.0, base_rgb[0] + (tip_rgb[0] - base_rgb[0]) * t + j))
        g = max(0.0, min(1.0, base_rgb[1] + (tip_rgb[1] - base_rgb[1]) * t + j))
        b = max(0.0, min(1.0, base_rgb[2] + (tip_rgb[2] - base_rgb[2]) * t + j))
        return (r, g, b, 0.0)
    return fn


def _bone(name, start, angle_deg, length, thickness):
    """Caja fina construida colgando hacia -Z desde el origen (pivote en la
    base), rotada `angle_deg` alrededor de X global para inclinarla hacia
    un lado y hacia abajo, y despues trasladada a `start`. Mismo patron de
    rotate+transform_apply que small_items.py, reutilizado aqui para
    aproximar las unas curvas del ancla con un par de segmentos rectos."""
    obj = C.make_box(name, (thickness, thickness, length), center=(0.0, 0.0, -length / 2.0))
    C.select_only(obj)
    import bpy
    bpy.ops.transform.rotate(value=math.radians(angle_deg), orient_axis='Y')
    bpy.ops.object.transform_apply(rotation=True)
    for v in obj.data.vertices:
        v.co.x += start[0]
        v.co.y += start[1]
        v.co.z += start[2]
    obj.data.update()
    return obj


# ---------------------------------------------------------------------------
# 1. Casco partido (tramos ahusados hacia la proa, con una fractura central)
# ---------------------------------------------------------------------------
@_register('hull')
def _build_hull(variant, rnd):
    # (y_inicio, longitud, anchura, altura) de popa (y=0) a proa (y=7)
    segments_def = [
        (0.0, 1.8, 2.4, 1.0),
        (1.8, 1.6, 2.2, 0.95),
        # hueco central 3.4 -> 4.6: seccion arrancada por el impacto
        (4.6, 1.4, 1.6, 0.8),
        (6.0, 1.0, 0.8, 0.55),
    ]
    keel_h = 0.15

    parts = []
    for i, (y0, length, width, height) in enumerate(segments_def):
        yc = y0 + length / 2.0
        keel = C.make_box(f'Keel{i}', (width * 0.18, length, keel_h),
                           center=(0.0, yc, keel_h / 2.0))
        hull = C.make_box(f'Hull{i}', (width, length * 0.96, height),
                           center=(0.0, yc, keel_h + height / 2.0))
        parts.append(keel)
        parts.append(hull)

    # costillas rotas expuestas en el borde de la fractura
    for i, rx in enumerate((-0.5, 0.0, 0.5)):
        rib = C.make_box(f'Rib{i}', (0.05, 0.05, 0.55),
                          center=(rx, 3.55 + i * 0.18, keel_h + 0.55))
        parts.append(rib)

    plank_fn = _plank_bands((0.42, 0.35, 0.27), (0.20, 0.19, 0.19), jitter=0.06, rnd=rnd)
    for p in parts:
        M.assign(p, ['M_Wood'])
        C.set_vertex_colors(p, plank_fn)

    return _finish(parts, 'SM_' + variant['name'])


# ---------------------------------------------------------------------------
# 2. Mástil partido, tumbado casi horizontal
# ---------------------------------------------------------------------------
@_register('mast')
def _build_mast(variant, rnd):
    trunk, _ = C.make_curved_trunk(
        'Mast', height=4.0, base_radius=0.09, tip_radius=0.02, curvature=0.25,
        n_points=8, bevel_resolution=3, rnd=rnd, z_offset=0.0)

    C.select_only(trunk)
    import bpy
    bpy.ops.transform.rotate(value=math.radians(88), orient_axis='X')
    bpy.ops.object.transform_apply(rotation=True)
    _settle_to_ground(trunk)

    M.assign(trunk, ['M_Wood'])
    C.set_vertex_colors(trunk, _distance_tint(
        (0.32, 0.30, 0.27), (0.62, 0.52, 0.36), max_dist=4.0, jitter=0.04, rnd=rnd))

    return _finish([trunk], 'SM_' + variant['name'])


# ---------------------------------------------------------------------------
# 3. Vela de lona rasgada, caída y arrugada en el suelo
# ---------------------------------------------------------------------------
@_register('sail')
def _build_sail(variant, rnd):
    sail = C.make_blob(
        'Sail', center=(0.0, 0.0, 0.12), radius=1.6, seed=variant['seed'],
        subdivisions=3, noise_scale=1.4, noise_strength=0.30,
        scale=(1.4, 1.0, 0.12), relax_iterations=1)
    _settle_to_ground(sail)

    M.assign(sail, ['M_Fabric'])
    C.set_vertex_colors(sail, C.constant_tint((0.78, 0.74, 0.60), alpha=0.0, jitter=0.06, rnd=rnd))

    return _finish([sail], 'SM_' + variant['name'])


# ---------------------------------------------------------------------------
# 4. Ancla tipo almirantazgo (caña, cepo cruzado y dos uñas curvas)
# ---------------------------------------------------------------------------
@_register('anchor')
def _build_anchor(variant, rnd):
    shank = C.make_cylinder('Shank', radius=0.028, depth=0.50, segments=10,
                             center=(0.0, 0.0, 0.40))
    cepo = C.make_box('Cepo', (0.50, 0.045, 0.045), center=(0.0, 0.0, 0.58))

    arms = []
    for side in (-1, 1):
        a1_angle = 35 * side
        a1_len = 0.30
        a1 = _bone('Arm', (0.0, 0.0, 0.15), a1_angle, a1_len, 0.035)
        theta1 = math.radians(a1_angle)
        tip1 = (-a1_len * math.sin(theta1), 0.0, 0.15 - a1_len * math.cos(theta1))

        a2_angle = 75 * side
        a2_len = 0.20
        a2 = _bone('Fluke', tip1, a2_angle, a2_len, 0.045)

        arms.append(a1)
        arms.append(a2)

    parts = [shank, cepo] + arms
    obj = C.join_objects(parts, 'SM_' + variant['name'])
    C.merge_by_distance(obj, dist=0.002)
    _settle_to_ground(obj)

    M.assign(obj, ['M_Metal'])
    top_z = max(v.co.z for v in obj.data.vertices)
    C.set_vertex_colors(obj, C.gradient_along_axis(
        (0.55, 0.30, 0.12), (0.45, 0.45, 0.47), 'z', 0.0, top_z, jitter=0.03, rnd=rnd))

    C.shade_smooth_auto(obj, angle_deg=30.0)
    C.add_basic_uv(obj)
    return obj
