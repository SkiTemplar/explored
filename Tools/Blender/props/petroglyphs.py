"""
petroglyphs.py — losas y piedra vertical talladas del antiguo pueblo de
navegantes.

30 petroglifos repartidos en cuevas y la Meseta reutilizan solo 3 losas
planas (cada una con un patron de grabado distinto) mas una piedra vertical
(menhir), con distinta rotacion/escala aplicada en Unreal. El "grabado" no
es geometria: se sugiere solo con vertex color, oscureciendo la piedra base
en un patron calculado a partir de la posicion local del vertice.
"""

import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import common as C  # noqa: E402
import _materials as M  # noqa: E402

VARIANTS = [
    dict(name='Petroglyph_Slab_A', seed=1501, builder='slab_a',
         tri_budget=(200, 900), needs_collision=True),
    dict(name='Petroglyph_Slab_B', seed=1502, builder='slab_b',
         tri_budget=(200, 900), needs_collision=True),
    dict(name='Petroglyph_Slab_C', seed=1503, builder='slab_c',
         tri_budget=(200, 900), needs_collision=True),
    dict(name='Petroglyph_Standing', seed=1504, builder='standing',
         tri_budget=(300, 1000), needs_collision=True),
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


_STONE_BASE = (0.42, 0.40, 0.36)
_STONE_CARVED = (0.16, 0.15, 0.13)


def _clamp01(x):
    return max(0.0, min(1.0, x))


def _subdivide(obj, cuts=5):
    """Subdivide todas las caras de la caja: sin esto solo hay 4 vertices
    por cara (las esquinas) y un patron con frecuencia alta no se llega a
    leer — el color de vertice interpola linealmente entre esquinas."""
    import bpy
    C.select_only(obj)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.subdivide(number_cuts=cuts)
    bpy.ops.object.mode_set(mode='OBJECT')


def _lean_slab(obj, tilt_deg, thickness):
    """Inclina una losa construida plana (grosor en Z, cara tallada hacia
    arriba) unos pocos grados sobre el eje X, como si descansara apoyada
    contra la pendiente del terreno, y vuelve a apoyar el punto mas bajo en
    z=0 (regla del pivote en la base)."""
    import bpy
    C.select_only(obj)
    bpy.ops.transform.rotate(value=math.radians(tilt_deg), orient_axis='X')
    bpy.ops.object.transform_apply(rotation=True)
    lift = (thickness / 2.0) * math.sin(math.radians(tilt_deg))
    bpy.ops.transform.translate(value=(0.0, 0.0, lift))
    bpy.ops.object.transform_apply(location=True)


def _make_pattern_a(thickness, rnd):
    """Patron A: lineas paralelas en la cara superior de la losa."""
    cache = {}

    def fn(v):
        if v.index not in cache:
            cache[v.index] = rnd.uniform(-0.03, 0.03)
        j = cache[v.index]
        is_top = v.co.z > thickness * 0.95
        carved = is_top and math.sin(v.co.x * 18.0) > 0.6
        base = _STONE_CARVED if carved else _STONE_BASE
        return (_clamp01(base[0] + j), _clamp01(base[1] + j), _clamp01(base[2] + j), 0.0)
    return fn


def _make_pattern_b(thickness, rnd):
    """Patron B: espiral/concentrico en la cara superior de la losa."""
    cache = {}

    def fn(v):
        if v.index not in cache:
            cache[v.index] = rnd.uniform(-0.03, 0.03)
        j = cache[v.index]
        is_top = v.co.z > thickness * 0.95
        radius = math.hypot(v.co.x, v.co.y)
        angle = math.atan2(v.co.y, v.co.x)
        carved = is_top and math.sin(radius * 14.0 + angle * 3.0) > 0.5
        base = _STONE_CARVED if carved else _STONE_BASE
        return (_clamp01(base[0] + j), _clamp01(base[1] + j), _clamp01(base[2] + j), 0.0)
    return fn


def _make_pattern_c(thickness, rnd):
    """Patron C: puntos/cuadricula en la cara superior de la losa."""
    cache = {}

    def fn(v):
        if v.index not in cache:
            cache[v.index] = rnd.uniform(-0.03, 0.03)
        j = cache[v.index]
        is_top = v.co.z > thickness * 0.95
        carved = is_top and (math.sin(v.co.x * 22.0) * math.sin(v.co.y * 22.0)) > 0.5
        base = _STONE_CARVED if carved else _STONE_BASE
        return (_clamp01(base[0] + j), _clamp01(base[1] + j), _clamp01(base[2] + j), 0.0)
    return fn


def _make_pattern_front(half_thick, rnd):
    """Patron para la piedra vertical: usa z/x (cara frontal) en vez de x/y,
    ya que aqui la cara tallada es la frontal, no la superior."""
    cache = {}

    def fn(v):
        if v.index not in cache:
            cache[v.index] = rnd.uniform(-0.03, 0.03)
        j = cache[v.index]
        is_front = v.co.y < -half_thick * 0.9
        carved = is_front and math.sin(v.co.z * 16.0 + v.co.x * 6.0) > 0.5
        base = _STONE_CARVED if carved else _STONE_BASE
        return (_clamp01(base[0] + j), _clamp01(base[1] + j), _clamp01(base[2] + j), 0.0)
    return fn


def _build_slab(variant, rnd, width, depth, thickness, pattern_fn):
    slab = C.make_box('Slab', (width, depth, thickness), center=(0.0, 0.0, thickness / 2.0))
    _subdivide(slab, cuts=5)
    M.assign(slab, ['M_Stone'])
    C.set_vertex_colors(slab, pattern_fn(thickness, rnd))

    tilt = rnd.uniform(10.0, 15.0)
    _lean_slab(slab, tilt, thickness)

    return _finish([slab], 'SM_' + variant['name'])


# ---------------------------------------------------------------------------
# 1. Losa A (lineas paralelas)
# ---------------------------------------------------------------------------
@_register('slab_a')
def _build_slab_a(variant, rnd):
    return _build_slab(variant, rnd, width=1.2, depth=0.9, thickness=0.15, pattern_fn=_make_pattern_a)


# ---------------------------------------------------------------------------
# 2. Losa B (espiral/concentrico)
# ---------------------------------------------------------------------------
@_register('slab_b')
def _build_slab_b(variant, rnd):
    return _build_slab(variant, rnd, width=1.0, depth=1.0, thickness=0.14, pattern_fn=_make_pattern_b)


# ---------------------------------------------------------------------------
# 3. Losa C (puntos/cuadricula)
# ---------------------------------------------------------------------------
@_register('slab_c')
def _build_slab_c(variant, rnd):
    return _build_slab(variant, rnd, width=1.3, depth=0.8, thickness=0.16, pattern_fn=_make_pattern_c)


# ---------------------------------------------------------------------------
# 4. Menhir pequeno (piedra vertical, dos tramos)
# ---------------------------------------------------------------------------
@_register('standing')
def _build_standing(variant, rnd):
    base_h, base_w, base_t = 0.55, 0.40, 0.20
    tip_h, tip_w, tip_t = 0.45, 0.28, 0.14

    base = C.make_box('Base', (base_w, base_t, base_h), center=(0.0, 0.0, base_h / 2.0))
    _subdivide(base, cuts=5)
    M.assign(base, ['M_Stone'])
    C.set_vertex_colors(base, _make_pattern_front(base_t / 2.0, rnd))

    tip = C.make_box('Tip', (tip_w, tip_t, tip_h), center=(0.0, 0.0, base_h + tip_h / 2.0))
    _subdivide(tip, cuts=4)
    M.assign(tip, ['M_Stone'])
    C.set_vertex_colors(tip, _make_pattern_front(tip_t / 2.0, rnd))

    return _finish([base, tip], 'SM_' + variant['name'])
