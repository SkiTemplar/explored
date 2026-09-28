"""
boats.py — embarcaciones del jugador (balsa, canoa, canoa con balancín y vela).

4 props: balsa de troncos atados, canoa excavada de un tronco, la misma
canoa con flotador lateral y mástil, y una vela triangular suelta pensada
para acoplarse al mástil de la canoa con balancín en Unreal.
"""

import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import _materials as M  # noqa: E402
import _shapes as S  # noqa: E402
import common as C  # noqa: E402

CATEGORY = 'boats'

VARIANTS = [
    dict(name='Raft', seed=1801, builder='raft',
         tri_budget=(500, 2600), needs_collision=True),
    dict(name='Canoe', seed=1802, builder='canoe',
         tri_budget=(150, 900), needs_collision=True),
    dict(name='Canoe_Outrigger', seed=1803, builder='canoe_outrigger',
         tri_budget=(1000, 4500), needs_collision=True, collision_complex=True),
    dict(name='Sail', seed=1804, builder='sail',
         tri_budget=(5, 150), needs_collision=False),
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


def _finish(parts, name, bevel_width=0.015, bevel_segments=2):
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


# Paleta viva: madera miel/caramelo cálida (nunca marrón-negro apagado) y
# fibra/tela con color de verdad en vez de gris o beige plano.
_WOOD_LIGHT = (0.62, 0.42, 0.20)
_WOOD_MID = (0.54, 0.36, 0.17)
_WOOD_DARK = (0.44, 0.28, 0.14)
_ROPE_FIBER = (0.62, 0.42, 0.14)
_HULL_TRIBAL = (0.68, 0.28, 0.12)
_SAIL_CREAM = (0.85, 0.76, 0.55)
_SAIL_STRIPE = (0.80, 0.58, 0.16)


# ---------------------------------------------------------------------------
# Helpers internos de casco (compartidos por canoa y canoa con balancín)
# ---------------------------------------------------------------------------

def _build_canoe_hull(rnd, length=4.0):
    """Casco excavado de un tronco: 5 tramos de make_cylinder (puntas
    ahusadas + cuerpo cilíndrico) unidos y reorientados en horizontal, con
    el punto más bajo (quilla) en z=0. Proporciones ~20% más rechonchas que
    un diseño técnico realista para que se lea bonito y divertido de cerca."""
    segments = 16
    tip_r = 0.06
    mid_r = 0.20
    hull_r = 0.34
    pieces_spec = [
        ('SternTip', tip_r, mid_r, 0.5),
        ('SternMid', mid_r, hull_r, 0.4),
        ('Body', hull_r, None, length - 1.8),
        ('BowMid', hull_r, mid_r, 0.4),
        ('BowTip', mid_r, tip_r, 0.5),
    ]
    pieces = []
    z = 0.0
    for pname, r0, r1, seg_len in pieces_spec:
        pieces.append(C.make_cylinder(pname, radius=r0, depth=seg_len, segments=segments,
                                       center=(0.0, 0.0, z + seg_len / 2.0), radius2=r1))
        z += seg_len

    hull = C.join_objects(pieces, 'Hull')
    C.merge_by_distance(hull, dist=0.003)

    C.select_only(hull)
    import bpy
    bpy.ops.transform.rotate(value=math.radians(90), orient_axis='X')
    bpy.ops.object.transform_apply(rotation=True)
    bpy.ops.transform.translate(value=(0.0, length / 2.0, hull_r))
    bpy.ops.object.transform_apply(location=True)

    M.assign(hull, ['M_Wood'])
    # madera oscura tallada pero CÁLIDA + una franja de pintura tribal
    # simple cerca de la proa (extremo "y" negativo tras la reorientación de
    # arriba), para darle carácter de embarcación propia del jugador.
    C.set_vertex_colors(hull, S.stripe_tint(
        _WOOD_DARK, _HULL_TRIBAL, 'y', -length / 2.0, -length / 2.0 + 0.45,
        jitter=0.03, rnd=rnd))
    return hull, hull_r


# ---------------------------------------------------------------------------
# 1. Balsa de troncos atados
# ---------------------------------------------------------------------------
@_register('raft')
def _build_raft(variant, rnd):
    n_logs = 8
    log_r = 0.10
    log_len = 2.2
    width = 1.6
    spacing = width / n_logs

    logs = []
    for i in range(n_logs):
        x = -width / 2.0 + spacing * (i + 0.5)
        log = C.make_cylinder(f'Log{i}', radius=log_r, depth=log_len, segments=16,
                               center=(0.0, 0.0, 0.0))
        C.select_only(log)
        import bpy
        bpy.ops.transform.rotate(value=math.radians(90), orient_axis='X')
        bpy.ops.object.transform_apply(rotation=True)
        bpy.ops.transform.translate(value=(x, 0.0, log_r))
        bpy.ops.object.transform_apply(location=True)
        logs.append(log)
    log_grp = C.join_objects(logs, 'Logs')
    C.merge_by_distance(log_grp, dist=0.001)
    M.assign(log_grp, ['M_Wood'])
    C.set_vertex_colors(log_grp, C.gradient_along_axis(
        _WOOD_DARK, _WOOD_LIGHT, 'z', 0.0, log_r * 2.0, jitter=0.03, rnd=rnd))

    ropes = []
    rope_r = 0.024
    for i, y in enumerate((-0.75, 0.75)):
        rope = C.make_cylinder(f'Rope{i}', radius=rope_r, depth=width, segments=12,
                                center=(0.0, 0.0, 0.0))
        C.select_only(rope)
        import bpy
        bpy.ops.transform.rotate(value=math.radians(90), orient_axis='Y')
        bpy.ops.object.transform_apply(rotation=True)
        bpy.ops.transform.translate(value=(0.0, y, log_r * 2.0 * 0.55))
        bpy.ops.object.transform_apply(location=True)
        ropes.append(rope)
    rope_grp = C.join_objects(ropes, 'Ropes')
    M.assign(rope_grp, ['M_Fabric'])
    C.set_vertex_colors(rope_grp, C.constant_tint(_ROPE_FIBER, alpha=0.0, jitter=0.03, rnd=rnd))

    return _finish([log_grp, rope_grp], 'SM_' + variant['name'], bevel_width=0.012)


# ---------------------------------------------------------------------------
# 2. Canoa excavada de un tronco
# ---------------------------------------------------------------------------
@_register('canoe')
def _build_canoe(variant, rnd):
    hull, hull_r = _build_canoe_hull(rnd)

    thwarts = []
    for i, y in enumerate((-0.6, 0.6)):
        t = C.make_box(f'Thwart{i}', (0.5, 0.05, 0.036), center=(0.0, y, hull_r * 2.0 - 0.02))
        thwarts.append(t)
    thwart_grp = C.join_objects(thwarts, 'Thwarts')
    M.assign(thwart_grp, ['M_Wood'])
    C.set_vertex_colors(thwart_grp, C.constant_tint(_WOOD_MID, alpha=0.0, jitter=0.03, rnd=rnd))

    return _finish([hull, thwart_grp], 'SM_' + variant['name'], bevel_width=0.014)


# ---------------------------------------------------------------------------
# 3. Canoa con balancín (flotador lateral, brazos y mástil)
# ---------------------------------------------------------------------------
@_register('canoe_outrigger')
def _build_canoe_outrigger(variant, rnd):
    hull, hull_r = _build_canoe_hull(rnd)

    float_len, float_r, float_x = 3.0, 0.072, 1.2
    float_obj = C.make_cylinder('Float', radius=float_r, depth=float_len, segments=14,
                                 center=(0.0, 0.0, 0.0))
    C.select_only(float_obj)
    import bpy
    bpy.ops.transform.rotate(value=math.radians(90), orient_axis='X')
    bpy.ops.object.transform_apply(rotation=True)
    bpy.ops.transform.translate(value=(float_x, 0.0, float_r))
    bpy.ops.object.transform_apply(location=True)
    M.assign(float_obj, ['M_Wood'])
    C.set_vertex_colors(float_obj, C.constant_tint(_WOOD_MID, alpha=0.0, jitter=0.03, rnd=rnd))

    arm_len = float_x + 0.05
    arms = []
    for i, y in enumerate((-1.0, 1.0)):
        arm = C.make_cylinder(f'Arm{i}', radius=0.024, depth=arm_len, segments=12,
                               center=(0.0, 0.0, 0.0))
        C.select_only(arm)
        import bpy
        bpy.ops.transform.rotate(value=math.radians(90), orient_axis='Y')
        bpy.ops.object.transform_apply(rotation=True)
        bpy.ops.transform.translate(value=(float_x / 2.0, y, hull_r + 0.06))
        bpy.ops.object.transform_apply(location=True)
        arms.append(arm)
    arm_grp = C.join_objects(arms, 'Arms')
    M.assign(arm_grp, ['M_Wood'])
    C.set_vertex_colors(arm_grp, C.constant_tint(_WOOD_DARK, alpha=0.0, jitter=0.03, rnd=rnd))

    mast, _lean = C.make_curved_trunk('Mast', height=2.5, base_radius=0.036, tip_radius=0.018,
                                       curvature=0.04, n_points=6, bevel_resolution=4, rnd=rnd,
                                       lean_dir=0.0)
    C.select_only(mast)
    import bpy
    bpy.ops.transform.translate(value=(0.0, 0.0, hull_r + 0.08))
    bpy.ops.object.transform_apply(location=True)
    M.assign(mast, ['M_Wood'])
    mast_z0 = hull_r + 0.08
    C.set_vertex_colors(mast, C.gradient_along_axis(
        _WOOD_MID, _WOOD_LIGHT, 'z', mast_z0, mast_z0 + 2.5, jitter=0.02, rnd=rnd))

    fittings = []
    for i, y in enumerate((-1.0, 1.0)):
        f = C.make_box(f'Fitting{i}', (0.03, 0.03, 0.03),
                        center=(float_x * 0.9, y, hull_r + 0.06))
        fittings.append(f)
    fitting_grp = C.join_objects(fittings, 'Fittings')
    M.assign(fitting_grp, ['M_Metal'])
    C.set_vertex_colors(fitting_grp, C.constant_tint((0.52, 0.49, 0.43), alpha=0.0, jitter=0.02, rnd=rnd))

    return _finish([hull, float_obj, arm_grp, mast, fitting_grp], 'SM_' + variant['name'],
                    bevel_width=0.02)


# ---------------------------------------------------------------------------
# 4. Vela triangular (para acoplar al mástil de Canoe_Outrigger en Unreal)
# ---------------------------------------------------------------------------
@_register('sail')
def _build_sail(variant, rnd):
    height, width, thickness = 2.3, 1.1, 0.012
    sail = C.make_box('Sail', (thickness, width, height), center=(0.0, 0.0, height / 2.0))

    me = sail.data
    z_max = max(v.co.z for v in me.vertices)
    for v in me.vertices:
        if v.co.z >= z_max - 1e-6:
            v.co.x = 0.0
            v.co.y = 0.0
    me.update()
    # los 4 vértices superiores colapsan al mismo punto (vértice de la vela):
    # sin fusionarlos, la cara superior y las dos caras laterales adyacentes
    # quedan con vértices duplicados y área ~0 (geometría degenerada que
    # validate.py rechaza). merge_by_distance los suelda en 1 solo vértice,
    # dejando un triángulo real en cada lado.
    C.merge_by_distance(sail, dist=0.001)

    M.assign(sail, ['M_Fabric'])
    # crudo cálido con una franja de mostaza vertical: color de verdad, no
    # un beige apagado.
    C.set_vertex_colors(sail, S.stripe_tint(
        _SAIL_CREAM, _SAIL_STRIPE, 'y', -0.20, 0.20, jitter=0.03, rnd=rnd))
    return _finish([sail], 'SM_' + variant['name'], bevel_width=0.004)
