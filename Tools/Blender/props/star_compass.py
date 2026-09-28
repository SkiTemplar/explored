"""
star_compass.py — la brújula estelar de los Navegantes, tallada en la cima de
la Isla del Humo. Monumento circular de piedra con surcos radiales alineados
con las estrellas, más las piedras de alineación individuales repartidas a su
alrededor. Clave del final «La Travesía».

2 props: el monumento (plataforma + gnomon central) y la piedra de
alineación individual, genérica, para repetir con distinta rotación/escala.
Los surcos tallados NO son geometría: se sugieren con vertex color.
"""
import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import _materials as M  # noqa: E402
import common as C  # noqa: E402

CATEGORY = 'star_compass'

VARIANTS = [
    dict(name='StarCompass_Monument', seed=1401, builder='monument',
         tri_budget=(60, 400), needs_collision=True, collision_complex=True),
    dict(name='StarCompass_Marker', seed=1402, builder='marker',
         tri_budget=(10, 150), needs_collision=True),
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


def _radial_groove_tint(base_rgb, n_grooves, groove_half_width, darken, rnd):
    """color_fn que oscurece franjas radiales periódicas: sugiere los 12
    surcos tallados (uno cada 30°) solo con vertex color, sin tocar la
    geometría de la plataforma."""
    step = (2.0 * math.pi) / n_grooves

    def color_fn(v):
        angle = math.atan2(v.co.y, v.co.x)
        if angle < 0.0:
            angle += 2.0 * math.pi
        local = angle % step
        dist = min(local, step - local)
        shade = (1.0 - darken) if dist < groove_half_width else 1.0
        jitter = rnd.uniform(-0.03, 0.03)
        r = max(0.0, min(1.0, base_rgb[0] * shade + jitter))
        g = max(0.0, min(1.0, base_rgb[1] * shade + jitter))
        b = max(0.0, min(1.0, base_rgb[2] * shade + jitter))
        return (r, g, b, 0.0)

    return color_fn


# ---------------------------------------------------------------------------
# 1. Monumento (plataforma circular + gnomon central)
# ---------------------------------------------------------------------------
@_register('monument')
def _build_monument(variant, rnd):
    platform_radius = 2.5
    platform_height = 0.3
    platform = C.make_cylinder('Platform', radius=platform_radius, depth=platform_height,
                                segments=24, center=(0.0, 0.0, platform_height / 2.0))
    M.assign(platform, ['M_Stone'])
    C.set_vertex_colors(platform, _radial_groove_tint(
        (0.40, 0.38, 0.35), n_grooves=12, groove_half_width=0.045, darken=0.4, rnd=rnd))

    gnomon_height = 1.2
    gnomon = C.make_cylinder('Gnomon', radius=0.13, depth=gnomon_height, segments=8,
                              center=(0.0, 0.0, platform_height + gnomon_height / 2.0),
                              radius2=0.09)
    M.assign(gnomon, ['M_Stone'])
    C.set_vertex_colors(gnomon, C.constant_tint((0.33, 0.31, 0.29), alpha=0.0, jitter=0.03, rnd=rnd))

    obj = C.join_objects([platform, gnomon], 'Monument')
    C.merge_by_distance(obj, dist=0.003)
    C.shade_smooth_auto(obj, angle_deg=35.0)
    C.add_basic_uv(obj)
    return obj


# ---------------------------------------------------------------------------
# 2. Piedra de alineación individual (losa vertical genérica)
# ---------------------------------------------------------------------------
@_register('marker')
def _build_marker(variant, rnd):
    slab = C.make_box('Slab', (0.18, 0.05, 0.5), center=(0.0, 0.0, 0.25))
    M.assign(slab, ['M_Stone'])
    C.set_vertex_colors(slab, C.constant_tint((0.36, 0.35, 0.33), alpha=0.0, jitter=0.04, rnd=rnd))

    # surco tallado sugerido por color: tira estrecha ligeramente saliente
    # en el frente, más oscura, sin relieve profundo en la geometría.
    groove = C.make_box('Groove', (0.022, 0.052, 0.40), center=(0.0, 0.0, 0.25))
    M.assign(groove, ['M_Stone'])
    C.set_vertex_colors(groove, C.constant_tint((0.20, 0.19, 0.18), alpha=0.0, jitter=0.03, rnd=rnd))

    return _finish([slab, groove], 'SM_' + variant['name'])
