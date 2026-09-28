"""
small_items.py — objetos pequeños recogibles del inventario diegético.

12 props: cuaderno de vuelo de Inés, página suelta del diario Halden,
botella con mensaje, radio del Albatros, batería, antena plegable, pistola
de bengalas, brújula del Albatros, mochila, cámara desechable, termo y
botiquín. Presupuesto: <800 triángulos cada uno (todos se quedan muy por
debajo: son props de mano, no hace falta gastar el presupuesto completo).
"""

import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import _materials as M  # noqa: E402
import _shapes as S  # noqa: E402
import common as C  # noqa: E402

CATEGORY = 'small_items'

VARIANTS = [
    dict(name='ItemFlightLog', seed=3001, builder='notebook',
         tri_budget=(20, 800), needs_collision=True, interactable=True),
    dict(name='ItemHaldenPage', seed=3002, builder='page',
         tri_budget=(10, 400), needs_collision=False, interactable=True),
    dict(name='ItemMessageBottle', seed=3003, builder='bottle',
         tri_budget=(40, 800), needs_collision=True, interactable=True),
    dict(name='ItemAlbatrosRadio', seed=3004, builder='radio',
         tri_budget=(40, 900), needs_collision=True, interactable=True),
    dict(name='ItemBattery', seed=3005, builder='battery',
         tri_budget=(20, 400), needs_collision=True, interactable=True),
    dict(name='ItemFoldingAntenna', seed=3006, builder='antenna',
         tri_budget=(20, 1100), needs_collision=True, interactable=True),
    dict(name='ItemFlareGun', seed=3007, builder='flaregun',
         tri_budget=(40, 800), needs_collision=True, interactable=True),
    dict(name='ItemAlbatrosCompass', seed=3008, builder='compass',
         tri_budget=(30, 650), needs_collision=True, interactable=True),
    dict(name='ItemBackpack', seed=3009, builder='backpack',
         tri_budget=(80, 5200), needs_collision=True, interactable=True),
    dict(name='ItemDisposableCamera', seed=3010, builder='camera',
         tri_budget=(30, 600), needs_collision=True, interactable=True),
    dict(name='ItemThermos', seed=3011, builder='thermos',
         tri_budget=(30, 650), needs_collision=True, interactable=True),
    dict(name='ItemMedkit', seed=3012, builder='medkit',
         tri_budget=(10, 700), needs_collision=True, interactable=True),
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
    # bisel suave antes de sombrear: quita el aspecto "de caja" de los
    # props de mano sin perder su forma (width pequeño: son objetos de
    # pocos centimetros a decimetros).
    S.bevel_obj(obj, width=0.004, segments=2, limit_angle_deg=40.0)
    C.shade_smooth_auto(obj, angle_deg=35.0)
    C.add_basic_uv(obj)
    return obj


# ---------------------------------------------------------------------------
# 1. Cuaderno de vuelo de Inés (cuero marrón cálido)
# ---------------------------------------------------------------------------
@_register('notebook')
def _build_notebook(variant, rnd):
    cover = C.make_box('Cover', (0.14, 0.19, 0.018), center=(0.0, 0.0, 0.009))
    M.assign(cover, ['M_Fabric'])
    C.set_vertex_colors(cover, C.constant_tint((0.56, 0.34, 0.16), alpha=0.0, jitter=0.03, rnd=rnd))

    pages = C.make_box('Pages', (0.132, 0.178, 0.012), center=(0.0, -0.002, 0.021))
    M.assign(pages, ['M_Paper'])
    C.set_vertex_colors(pages, C.constant_tint((0.86, 0.80, 0.64), alpha=0.0, jitter=0.02, rnd=rnd))

    strap = C.make_box('Strap', (0.145, 0.015, 0.006), center=(0.0, 0.0, 0.031))
    M.assign(strap, ['M_Fabric'])
    C.set_vertex_colors(strap, C.constant_tint((0.40, 0.24, 0.12), alpha=0.0, jitter=0.02, rnd=rnd))

    return _finish([cover, pages, strap], 'SM_' + variant['name'])


# ---------------------------------------------------------------------------
# 2. Página suelta del diario Halden (papel amarillento, doblada)
# ---------------------------------------------------------------------------
@_register('page')
def _build_page(variant, rnd):
    bm_obj = C.make_box('Sheet', (0.148, 0.21, 0.002), center=(0.0, 0.0, 0.001))
    me = bm_obj.data
    # doblez suave: desplaza una esquina hacia arriba a lo largo de una
    # diagonal para que no quede una hoja perfectamente plana.
    fold_axis = C.Vector((1.0, 1.0, 0.0)).normalized()
    for v in me.vertices:
        d = C.Vector((v.co.x, v.co.y, 0.0)).dot(fold_axis)
        v.co.z += max(0.0, d) * 0.35 * rnd.uniform(0.7, 1.0)
    me.update()
    M.assign(bm_obj, ['M_Paper'])
    C.set_vertex_colors(bm_obj, C.constant_tint((0.80, 0.72, 0.52), alpha=0.0, jitter=0.03, rnd=rnd))
    return _finish([bm_obj], 'SM_' + variant['name'])


# ---------------------------------------------------------------------------
# 3. Botella con mensaje (vidrio verde translúcido, papel dentro, corcho)
# ---------------------------------------------------------------------------
@_register('bottle')
def _build_bottle(variant, rnd):
    body = C.make_cylinder('Body', radius=0.035, depth=0.16, segments=10,
                            center=(0.0, 0.0, 0.08), cap_ends=False)
    neck = C.make_cylinder('Neck', radius=0.014, depth=0.06, segments=10,
                            center=(0.0, 0.0, 0.19), cap_ends=False, radius2=0.011)
    shoulder = C.make_cylinder('Shoulder', radius=0.035, depth=0.02, segments=10,
                                center=(0.0, 0.0, 0.17), cap_ends=True, radius2=0.014)
    base_cap = C.make_cylinder('BaseCap', radius=0.035, depth=0.004, segments=10,
                                center=(0.0, 0.0, 0.002), cap_ends=True)
    glass = C.join_objects([body, neck, shoulder, base_cap], 'Glass')
    C.merge_by_distance(glass, dist=0.002)
    M.assign(glass, ['M_Glass'])
    C.set_vertex_colors(glass, C.constant_tint((0.35, 0.55, 0.32), alpha=0.35, jitter=0.02, rnd=rnd))

    cork = C.make_cylinder('Cork', radius=0.012, depth=0.03, segments=8,
                            center=(0.0, 0.0, 0.235))
    M.assign(cork, ['M_Wood'])
    C.set_vertex_colors(cork, C.constant_tint((0.55, 0.42, 0.26), alpha=0.0, jitter=0.03, rnd=rnd))

    scroll = C.make_cylinder('Scroll', radius=0.009, depth=0.1, segments=6,
                              center=(0.0, 0.0, 0.09))
    M.assign(scroll, ['M_Paper'])
    C.set_vertex_colors(scroll, C.constant_tint((0.83, 0.78, 0.60), alpha=0.0, jitter=0.02, rnd=rnd))

    return _finish([glass, cork, scroll], 'SM_' + variant['name'])


# ---------------------------------------------------------------------------
# 4. Radio del Albatros (arrancada del panel, con perilla y rejilla)
# ---------------------------------------------------------------------------
@_register('radio')
def _build_radio(variant, rnd):
    body = C.make_box('Body', (0.20, 0.09, 0.13), center=(0.0, 0.0, 0.065))
    M.assign(body, ['M_Metal'])
    C.set_vertex_colors(body, C.constant_tint((0.48, 0.49, 0.50), alpha=0.0, jitter=0.03, rnd=rnd))

    grille = C.make_box('Grille', (0.09, 0.005, 0.09), center=(0.045, -0.048, 0.075))
    M.assign(grille, ['M_Fabric'])
    C.set_vertex_colors(grille, C.constant_tint((0.22, 0.21, 0.20), alpha=0.0, jitter=0.02, rnd=rnd))

    knobs = []
    for i, x in enumerate((-0.06, -0.02)):
        k = C.make_cylinder(f'Knob{i}', radius=0.014, depth=0.02, segments=8,
                             center=(x, -0.05, 0.02))
        C.select_only(k)
        import bpy
        bpy.ops.transform.rotate(value=math.radians(90), orient_axis='X')
        bpy.ops.object.transform_apply(rotation=True)
        k.location = (x, -0.05, 0.02)
        knobs.append(k)
    knob_grp = C.join_objects(knobs, 'Knobs')
    M.assign(knob_grp, ['M_Metal'])
    C.set_vertex_colors(knob_grp, C.constant_tint((0.55, 0.42, 0.14), alpha=0.0, jitter=0.02, rnd=rnd))

    antenna = C.make_cylinder('StubAntenna', radius=0.004, depth=0.09, segments=6,
                               center=(0.08, 0.0, 0.13), radius2=0.002)
    M.assign(antenna, ['M_Metal'])
    C.set_vertex_colors(antenna, C.constant_tint((0.5, 0.5, 0.52), alpha=0.0, jitter=0.02, rnd=rnd))

    return _finish([body, grille, knob_grp, antenna], 'SM_' + variant['name'])


# ---------------------------------------------------------------------------
# 5. Batería (pila tipo D, con casquillo metálico y banda de etiqueta)
# ---------------------------------------------------------------------------
@_register('battery')
def _build_battery(variant, rnd):
    body = C.make_cylinder('Cell', radius=0.017, depth=0.06, segments=10,
                            center=(0.0, 0.0, 0.03))
    M.assign(body, ['M_Metal'])
    C.set_vertex_colors(body, C.gradient_along_axis(
        (0.62, 0.14, 0.10), (0.10, 0.10, 0.11), 'z', 0.012, 0.048, jitter=0.02, rnd=rnd))

    cap = C.make_cylinder('Terminal', radius=0.006, depth=0.004, segments=8,
                           center=(0.0, 0.0, 0.062))
    M.assign(cap, ['M_Metal'])
    C.set_vertex_colors(cap, C.constant_tint((0.65, 0.55, 0.20), alpha=0.0, jitter=0.02, rnd=rnd))

    return _finish([body, cap], 'SM_' + variant['name'])


# ---------------------------------------------------------------------------
# 6. Antena plegable (telescópica, 4 segmentos)
# ---------------------------------------------------------------------------
@_register('antenna')
def _build_antenna(variant, rnd):
    segs = []
    z = 0.0
    radii = (0.008, 0.0055, 0.0038, 0.0022)
    lengths = (0.14, 0.12, 0.11, 0.09)
    for i, (r, ln) in enumerate(zip(radii, lengths, strict=True)):
        seg = C.make_cylinder(f'Seg{i}', radius=r, depth=ln, segments=7,
                               center=(0.0, 0.0, z + ln / 2.0), radius2=r * 0.85)
        segs.append(seg)
        z += ln * 0.94  # ligero solape telescópico
    base = C.make_cylinder('Base', radius=0.013, depth=0.02, segments=8, center=(0.0, 0.0, 0.01))
    obj = C.join_objects(segs + [base], 'SM_' + variant['name'])
    C.merge_by_distance(obj, dist=0.001)
    M.assign(obj, ['M_Metal'])
    C.set_vertex_colors(obj, C.tint_along_axis((0.68, 0.68, 0.70), 'z', 0.0, z, jitter=0.03, rnd=rnd))
    S.bevel_obj(obj, width=0.0015, segments=2, limit_angle_deg=40.0)
    C.shade_smooth_auto(obj, angle_deg=25.0)
    C.add_basic_uv(obj)
    return obj


# ---------------------------------------------------------------------------
# 7. Pistola de bengalas
# ---------------------------------------------------------------------------
@_register('flaregun')
def _build_flaregun(variant, rnd):
    grip = C.make_box('Grip', (0.03, 0.045, 0.11), center=(0.0, 0.0, -0.05))
    M.assign(grip, ['M_Wood'])
    C.set_vertex_colors(grip, C.constant_tint((0.56, 0.36, 0.18), alpha=0.0, jitter=0.03, rnd=rnd))

    frame = C.make_box('Frame', (0.032, 0.05, 0.05), center=(0.0, 0.0, 0.0))
    barrel = C.make_cylinder('Barrel', radius=0.017, depth=0.14, segments=10,
                              center=(0.0, 0.09, 0.012))
    C.select_only(barrel)
    import bpy
    bpy.ops.transform.rotate(value=math.radians(90), orient_axis='X')
    bpy.ops.object.transform_apply(rotation=True)

    trigger = C.make_box('Trigger', (0.006, 0.012, 0.025), center=(0.0, -0.005, -0.012))

    metal = C.join_objects([frame, barrel, trigger], 'Metal')
    M.assign(metal, ['M_Metal'])
    C.set_vertex_colors(metal, C.constant_tint((0.38, 0.37, 0.36), alpha=0.0, jitter=0.03, rnd=rnd))

    return _finish([grip, metal], 'SM_' + variant['name'])


# ---------------------------------------------------------------------------
# 8. Brújula del Albatros (de mano, latón + cristal)
# ---------------------------------------------------------------------------
@_register('compass')
def _build_compass(variant, rnd):
    body = C.make_cylinder('Body', radius=0.04, depth=0.018, segments=14,
                            center=(0.0, 0.0, 0.009))
    M.assign(body, ['M_Metal'])
    C.set_vertex_colors(body, C.constant_tint((0.62, 0.50, 0.20), alpha=0.0, jitter=0.02, rnd=rnd))

    glass = C.make_cylinder('Glass', radius=0.036, depth=0.004, segments=14,
                             center=(0.0, 0.0, 0.02))
    M.assign(glass, ['M_Glass'])
    C.set_vertex_colors(glass, C.constant_tint((0.85, 0.90, 0.85), alpha=0.25, jitter=0.0, rnd=rnd))

    needle = C.make_box('Needle', (0.058, 0.006, 0.003), center=(0.0, 0.0, 0.019))
    M.assign(needle, ['M_Metal'])
    C.set_vertex_colors(needle, C.gradient_along_axis(
        (0.75, 0.10, 0.08), (0.85, 0.85, 0.88), 'x', -0.029, 0.029, jitter=0.0, rnd=rnd))

    ring = C.make_cylinder('Ring', radius=0.045, depth=0.006, segments=14,
                            center=(0.0, 0.0, 0.005), radius2=0.041)
    M.assign(ring, ['M_Metal'])
    C.set_vertex_colors(ring, C.constant_tint((0.60, 0.48, 0.18), alpha=0.0, jitter=0.02, rnd=rnd))

    return _finish([body, glass, needle, ring], 'SM_' + variant['name'])


# ---------------------------------------------------------------------------
# 9. Mochila (lona verde oliva, correas y hebillas) — prop muy visible
# ---------------------------------------------------------------------------
@_register('backpack')
def _build_backpack(variant, rnd):
    body = C.make_blob('Body', (0.0, 0.0, 0.2), radius=0.16, seed=variant['seed'],
                        subdivisions=2, noise_scale=1.6, noise_strength=0.10,
                        scale=(0.72, 0.52, 1.0), relax_iterations=2)
    M.assign(body, ['M_Fabric'])
    C.set_vertex_colors(body, C.constant_tint((0.44, 0.50, 0.26), alpha=0.0, jitter=0.03, rnd=rnd))

    pouches = []
    for side in (-1, 1):
        p = C.make_blob(f'Pouch{side}', (side * 0.135, -0.02, 0.15), radius=0.055,
                         seed=variant['seed'] * 7 + side, subdivisions=1,
                         noise_scale=2.0, noise_strength=0.12, scale=(0.75, 0.7, 1.1))
        pouches.append(p)
    pouch_grp = C.join_objects(pouches, 'Pouches')
    M.assign(pouch_grp, ['M_Fabric'])
    C.set_vertex_colors(pouch_grp, C.constant_tint((0.40, 0.46, 0.24), alpha=0.0, jitter=0.03, rnd=rnd))

    flap = C.make_box('Flap', (0.20, 0.16, 0.05), center=(0.0, -0.11, 0.28))
    M.assign(flap, ['M_Fabric'])
    C.set_vertex_colors(flap, C.constant_tint((0.40, 0.24, 0.14), alpha=0.0, jitter=0.02, rnd=rnd))

    straps = []
    for side in (-1, 1):
        s, _ = C.make_curved_trunk(
            f'Strap{side}', height=0.34, base_radius=0.014, tip_radius=0.012,
            curvature=0.10 * side, n_points=6, bevel_resolution=2, rnd=rnd, lean_dir=0.0)
        C.select_only(s)
        import bpy
        bpy.ops.transform.translate(value=(side * 0.08, 0.05, 0.06))
        bpy.ops.object.transform_apply(location=True)
        straps.append(s)
    strap_grp = C.join_objects(straps, 'Straps')
    M.assign(strap_grp, ['M_Fabric'])
    C.set_vertex_colors(strap_grp, C.constant_tint((0.42, 0.28, 0.15), alpha=0.0, jitter=0.03, rnd=rnd))

    buckles = []
    for i, z in enumerate((0.10, 0.20)):
        for side in (-1, 1):
            b = C.make_box(f'Buckle{i}{side}', (0.024, 0.008, 0.03), center=(side * 0.08, 0.06, z))
            buckles.append(b)
    buckle_grp = C.join_objects(buckles, 'Buckles')
    M.assign(buckle_grp, ['M_Metal'])
    C.set_vertex_colors(buckle_grp, C.constant_tint((0.55, 0.53, 0.45), alpha=0.0, jitter=0.02, rnd=rnd))

    return _finish([body, pouch_grp, flap, strap_grp, buckle_grp], 'SM_' + variant['name'])


# ---------------------------------------------------------------------------
# 10. Cámara desechable
# ---------------------------------------------------------------------------
@_register('camera')
def _build_camera(variant, rnd):
    body = C.make_box('Body', (0.11, 0.055, 0.06), center=(0.0, 0.0, 0.0))
    M.assign(body, ['M_Paper'])
    C.set_vertex_colors(body, C.constant_tint((0.80, 0.66, 0.20), alpha=0.0, jitter=0.03, rnd=rnd))

    lens = C.make_cylinder('Lens', radius=0.016, depth=0.02, segments=10,
                            center=(0.0, -0.037, 0.005))
    C.select_only(lens)
    import bpy
    bpy.ops.transform.rotate(value=math.radians(90), orient_axis='X')
    bpy.ops.object.transform_apply(rotation=True)
    M.assign(lens, ['M_Glass'])
    C.set_vertex_colors(lens, C.constant_tint((0.1, 0.12, 0.15), alpha=0.15, jitter=0.0, rnd=rnd))

    finder = C.make_box('Finder', (0.02, 0.015, 0.015), center=(0.03, -0.035, 0.038))
    flash = C.make_box('Flash', (0.022, 0.01, 0.02), center=(-0.035, -0.033, 0.02))
    small = C.join_objects([finder, flash], 'Small')
    M.assign(small, ['M_Metal'])
    C.set_vertex_colors(small, C.constant_tint((0.7, 0.7, 0.72), alpha=0.0, jitter=0.02, rnd=rnd))

    return _finish([body, lens, small], 'SM_' + variant['name'])


# ---------------------------------------------------------------------------
# 11. Termo
# ---------------------------------------------------------------------------
@_register('thermos')
def _build_thermos(variant, rnd):
    body = C.make_cylinder('Body', radius=0.035, depth=0.19, segments=12,
                            center=(0.0, 0.0, 0.095))
    M.assign(body, ['M_Metal'])
    C.set_vertex_colors(body, C.constant_tint((0.50, 0.14, 0.12), alpha=0.0, jitter=0.03, rnd=rnd))

    cap = C.make_cylinder('Cap', radius=0.028, depth=0.03, segments=12,
                           center=(0.0, 0.0, 0.205), radius2=0.024)
    M.assign(cap, ['M_Metal'])
    C.set_vertex_colors(cap, C.constant_tint((0.75, 0.75, 0.78), alpha=0.0, jitter=0.02, rnd=rnd))

    base = C.make_cylinder('Base', radius=0.037, depth=0.008, segments=12,
                            center=(0.0, 0.0, 0.004))
    M.assign(base, ['M_Metal'])
    C.set_vertex_colors(base, C.constant_tint((0.3, 0.3, 0.32), alpha=0.0, jitter=0.02, rnd=rnd))

    return _finish([body, cap, base], 'SM_' + variant['name'])


# ---------------------------------------------------------------------------
# 12. Botiquín (caja metálica blanca con cruz roja pintada en la tapa)
# ---------------------------------------------------------------------------
@_register('medkit')
def _build_medkit(variant, rnd):
    box = C.make_box('Box', (0.20, 0.14, 0.075), center=(0.0, 0.0, 0.0375))

    def cross_tint(v):
        # cruz roja en la cara superior (z == +0.0375): coordenadas locales
        # x/y dentro de una banda estrecha en cualquiera de los dos ejes.
        is_top = v.co.z > 0.037
        in_cross = is_top and (abs(v.co.x) < 0.018 or abs(v.co.y) < 0.018)
        if in_cross:
            return (0.62, 0.08, 0.06, 0.0)
        return (0.88, 0.87, 0.83, 0.0)
    C.set_vertex_colors(box, cross_tint)
    M.assign(box, ['M_Metal'])

    latch = C.make_box('Latch', (0.03, 0.012, 0.02), center=(0.0, -0.076, 0.02))
    M.assign(latch, ['M_Metal'])
    C.set_vertex_colors(latch, C.constant_tint((0.55, 0.53, 0.45), alpha=0.0, jitter=0.02, rnd=rnd))

    return _finish([box, latch], 'SM_' + variant['name'])
