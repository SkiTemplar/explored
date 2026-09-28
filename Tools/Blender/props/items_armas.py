"""
items_armas.py — arco, flecha y señuelo tallado (items.json; biblia §3.4 y
§4: arco = vara flexible + cuerda, flecha = astil + punta + emplumado).

Pivote = SOCKET DE MANO (ver _items.py): mango/astil por +Z, cara de
golpe o espalda del arco hacia +X. El señuelo es un objeto suelto: pivote
en la base, tumbado.

| Malla | Agarre (origen) |
|---|---|
| SM_Item_Arco | centro de la empuñadura; palas por ±Z, espalda a +X, cuerda a -X (hacia el arquero, 15 cm de fiador) |
| SM_Item_Flecha | culatín (el punto que va en la cuerda); punta de obsidiana en +Z, pluma guía (roja) hacia -X |
| SM_Item_SenueloTallado | objeto suelto: pivote en la base, tumbado de costado |
"""

import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import bpy  # noqa: E402

import _items as I  # noqa: E402
import _materials as M  # noqa: E402
import bmesh  # noqa: E402
import common as C  # noqa: E402
import items_herramientas as H  # noqa: E402
import kit_construccion as K  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

GROUP = I.GROUP
PAL = I.PAL

VARIANTS = [
    dict(name='Item_Arco', item_id='arco', seed=4601, builder='arco', tri_budget=(300, 5000),
         preview_rot_z=-0.9),
    dict(name='Item_Flecha', item_id='flecha', seed=4602, builder='flecha', tri_budget=(200, 2500),
         preview_rot_y=math.pi / 2),
    dict(name='Item_SenueloTallado', item_id='senuelo_tallado', seed=4603, builder='senuelo_tallado',
         tri_budget=(200, 3000)),
]
for _v in VARIANTS:
    _v['group'] = GROUP
    _v['needs_collision'] = True
    _v['interactable'] = True

_BUILDERS = {}


def _register(key):
    def deco(fn):
        _BUILDERS[key] = fn
        return fn
    return deco


def build(variant):
    rnd = C.seeded_rng(variant['seed'])
    obj = _BUILDERS[variant['builder']](variant, rnd, 'SM_' + variant['name'])
    if variant['builder'] == 'senuelo_tallado':
        return I.ground_centered(obj)
    return obj


# ---------------------------------------------------------------------------
# ARCO
# ---------------------------------------------------------------------------
@_register('arco')
def _b_arco(v, rnd, name):
    """Arco de bambú rajado de 1,3 m, montado: palas curvadas hacia el
    arquero, empuñadura forrada de cordel rojo, cuerda de tendón y nudos
    en las puntas."""
    p = K.Parts()
    half = 0.65
    n = 18
    pts, radii = [], []
    for i in range(-n, n + 1):
        t = i / n
        z = half * t
        # palas: curva parabólica hacia -X; la empuñadura (|t| < 0.1) recta
        u = max(0.0, abs(t) - 0.08) / 0.92
        x = -0.11 * u ** 1.7 - 0.02 * u ** 4
        pts.append((x, 0.0, z * (1.0 - 0.05 * u ** 2)))
        radii.append(0.0135 * (1.0 - 0.55 * u) if abs(t) > 0.08 else 0.016)
    limb = I.sweep('Limb', pts, radii, segs=8)
    # sección aplanada: más ancha (Y) que gruesa (X)
    for vv in limb.data.vertices:
        vv.co.y *= 1.5
    limb.data.update()
    M.assign(limb, ['M_Wood'])
    nodes = [-0.48, -0.22, 0.22, 0.48]
    green, green_l = PAL['bamboo'][1], PAL['bamboo'][2]

    def lcol(co):
        near = min(abs(co.z - nd) for nd in nodes)
        c = I.lerp3(green, green_l, 0.5 + 0.5 * math.sin(co.z * 11))
        # cara del vientre (-X) rajada: crema
        if co.x < min(0.0, -0.11 * (max(0.0, abs(co.z) / half - 0.08) / 0.92) ** 1.7) - 0.004:
            c = I.lerp3(PAL['bamboo_cut'], (0.52, 0.42, 0.20), 0.3)
        if near < 0.012:
            c = PAL['bamboo_node']
        return c
    I.color_fn(limb, lcol, rnd, 0.015)
    p.add(limb, 'none')
    # empuñadura forrada
    H._wrap(p, rnd, -0.07, 0.07, 0.019, rgb=PAL['cord_red'], cord_r=0.0032, name='Grip')
    # puntas con nudo y cuerda
    tips = [Vector(pts[0]), Vector(pts[-1])]
    for k, tp in enumerate(tips):
        sgn = -1 if k == 0 else 1
        wrap = I.helix_wrap(f'Nock{k}', tp.z - sgn * 0.045 if sgn > 0 else tp.z, tp.z if sgn > 0 else tp.z + 0.045,
                            0.0075, 0.0022, rnd, segs=5, center=(tp.x + 0.001, 0.0))
        M.assign(wrap, ['M_Fabric'])
        I.cord_stripes(wrap, (0.62, 0.52, 0.34), (0.44, 0.34, 0.18), 5)
        p.add(wrap, 'none')
    s0 = tips[0] + Vector((-0.004, 0, 0.03))
    s1 = tips[1] + Vector((-0.004, 0, -0.03))
    string = I.sweep('String', [s0, s0.lerp(s1, 0.5), s1], 0.0018, segs=5)
    M.assign(string, ['M_Fabric'])
    I.tint(string, (0.66, 0.56, 0.36), rnd, 0.01)
    p.add(string, 'none')
    return p.finish(name)


# ---------------------------------------------------------------------------
# FLECHA
# ---------------------------------------------------------------------------
def _vane(name, z0, z1, h, angle, rnd, rgb, rgb_tip):
    """Pluma recortada: lámina en el plano (radial, Z) con perfil
    parabólico, fijada al astil y girada `angle` alrededor de Z."""
    nu = 8
    bm = bmesh.new()
    rows = []
    for i in range(nu + 1):
        t = i / nu
        z = z0 + (z1 - z0) * t
        # perfil: sube rápido desde atrás y cae en rampa hacia delante
        hh = h * (math.sin(min(1.0, t * 1.4) * math.pi / 2) * (1.0 - max(0.0, t - 0.55) / 0.45 * 0.9))
        rows.append((bm.verts.new((0.003, 0.0, z)), bm.verts.new((0.003 + max(hh, 0.0008), 0.0, z - 0.004 * t))))
    for i in range(nu):
        a0, b0 = rows[i]
        a1, b1 = rows[i + 1]
        bm.faces.new((a0, b0, b1, a1))
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new(name, me)
    C.link_object(o)
    I.solidify(o, 0.0008)
    o.data.transform(Matrix.Rotation(angle, 4, 'Z'))
    M.assign(o, ['M_Fabric'])
    I.color_fn(o, lambda co: I.lerp3(rgb, rgb_tip, 1.0 if int((co.z - z0) / 0.018) % 2 else 0.0)
               if math.hypot(co.x, co.y) > 0.006 else rgb, rnd, 0.02)
    return o


def _band(p, rnd, z0, z1, r, rgb, name):
    """Atadura fina de tendón como banda lisa (las hélices de cordel de
    1 mm multiplican los triángulos sin leerse a 1-2 m)."""
    o = C.make_cylinder(name, r, z1 - z0, segments=8, center=(0, 0, (z0 + z1) / 2))
    M.assign(o, ['M_Fabric'])
    I.color_fn(o, lambda co: I.lerp3(rgb, I.lerp3(rgb, (0.2, 0.12, 0.05), 0.4),
                                     0.5 + 0.5 * math.sin((co.z - z0) * 1800)), rnd, 0.01)
    p.add(o, 'none')
    return o


@_register('flecha')
def _b_flecha(v, rnd, name):
    """Flecha de caña de 72 cm: punta de obsidiana atada con tendón y
    resina, tres plumas (la guía roja) y culatín reforzado."""
    p = K.Parts()
    L = 0.72
    shaft = C.make_cylinder('Shaft', 0.0042, L, segments=8, center=(0, 0, L / 2), radius2=0.0038)
    M.assign(shaft, ['M_Wood'])
    cane = (0.62, 0.46, 0.18)

    def scol(co):
        c = I.lerp3(cane, (0.50, 0.34, 0.12), 0.5 + 0.5 * math.sin(co.z * 60))
        return c
    I.color_fn(shaft, scol, rnd, 0.015)
    p.add(shaft, 'none')
    # nudos de la caña
    for z in (0.21, 0.47):
        nd = C.make_cylinder('Node', 0.0048, 0.006, segments=8, center=(0, 0, z))
        M.assign(nd, ['M_Wood'])
        I.tint(nd, (0.40, 0.26, 0.08), rnd, 0.01)
        p.add(nd, 'none')
    # culatín con muesca (dos orejetas) y atadura
    for sy in (-1, 1):
        ear = C.make_box('Ear', (0.006, 0.0022, 0.012), center=(0, sy * 0.0022, -0.004))
        M.assign(ear, ['M_Wood'])
        I.tint(ear, (0.34, 0.18, 0.06), rnd, 0.01)
        p.add(ear, 'none')
    _band(p, rnd, 0.004, 0.018, 0.0052, (0.62, 0.52, 0.34), 'NockWrap')
    # tres plumas a 120º; la guía (roja) mira a -X
    feathers = [(math.pi, (0.62, 0.08, 0.03), (0.80, 0.30, 0.08)),
                (math.pi / 3, (0.80, 0.74, 0.62), (0.26, 0.18, 0.12)),
                (-math.pi / 3, (0.80, 0.74, 0.62), (0.26, 0.18, 0.12))]
    for k, (ang, rgb, tip) in enumerate(feathers):
        p.add(_vane(f'Vane{k}', 0.035, 0.15, 0.016, ang, rnd, rgb, tip), 'none')
    _band(p, rnd, 0.15, 0.162, 0.0048, (0.50, 0.38, 0.22), 'FWrap')
    _band(p, rnd, 0.026, 0.036, 0.0048, (0.50, 0.38, 0.22), 'FWrap2')
    # punta de obsidiana
    blade, col = H._flake_obj('Point', v['seed'], 0.055, 0.020, 0.0065, PAL['obsidian'], PAL['obsidian_edge'],
                              None, point=0.9)
    C.set_vertex_colors(blade, col)
    # la punta de una lasca de 5 cm junta vértices a < 0,3 mm: se funden
    # para no dejar triángulos degenerados
    C.merge_by_distance(blade, 0.0003)
    blade.data.transform(Matrix.Translation((0, 0, L + 0.018)))
    p.add(blade, 'none')
    res = C.make_blob('Resin', (0, 0, L - 0.004), 1.0, v['seed'] + 5, subdivisions=2, noise_strength=0.1,
                      scale=(0.0062, 0.0058, 0.012))
    M.assign(res, ['M_Stone'])
    I.tint(res, PAL['resin'], rnd)
    p.add(res, 'none')
    _band(p, rnd, L - 0.028, L - 0.006, 0.0056, (0.66, 0.56, 0.38), 'HeadWrap')
    return p.finish(name)


# ---------------------------------------------------------------------------
# SEÑUELO TALLADO
# ---------------------------------------------------------------------------
@_register('senuelo_tallado')
def _b_senuelo_tallado(v, rnd, name):
    """Señuelo de pesca tallado en madera: pececillo de 9 cm pintado (lomo
    azul verdoso, vientre crema, agallas rojas, ojo grande), cola plana,
    anzuelo de hueso colgando de la panza y ojal de cordel en el morro."""
    p = K.Parts()
    L = 0.09
    prof = [(0.0, 0.0), (0.006, 0.004), (0.010, 0.012), (0.0125, 0.024), (0.013, 0.036), (0.012, 0.050),
            (0.009, 0.064), (0.005, 0.076), (0.0025, 0.082), (0.0, 0.084)]
    prof = I.smooth_profile(prof, per_seg=2)
    segs = 14
    body = I.lathe('Body', prof, segs=segs, sx=1.0, sy=0.62)
    # el eje del cuerpo pasa a +X (morro en -X), lomo en +Z
    body.data.transform(Matrix.Rotation(math.pi / 2, 4, 'Y'))
    body.data.transform(Matrix.Translation((-L / 2, 0, 0)))
    M.assign(body, ['M_Wood'])
    back = (0.03, 0.22, 0.26)
    back_l = (0.06, 0.36, 0.34)
    belly = (0.80, 0.70, 0.46)
    red = (0.66, 0.06, 0.02)
    eye_c = Vector((-L / 2 + 0.012, 0.0, 0.004))

    def col(co):
        x = co.x + L / 2
        # costados del color del lomo: tumbado, es el costado lo que se ve
        c = I.lerp3(belly, back, max(0.0, min(1.0, (co.z + 0.008) / 0.006)))
        if co.z > -0.002:
            # rayas verticales del lomo
            c = I.lerp3(c, back_l, 0.5 + 0.5 * math.sin(x * 260))
        if 0.018 < x < 0.024:
            c = red
        # ojo: punto negro con aro amarillo, por las dos caras
        d = math.hypot(co.x - eye_c.x, co.z - eye_c.z)
        if abs(co.y) > 0.003:
            if d < 0.0032:
                c = (0.01, 0.01, 0.01)
            elif d < 0.0052:
                c = (0.86, 0.62, 0.06)
        return c
    I.color_fn(body, col, rnd, 0.01)
    p.add(body, 'none')
    # cola en V plana
    bm = bmesh.new()
    tx = L / 2 - 0.016
    a = bm.verts.new((tx, 0, 0))
    b = bm.verts.new((tx + 0.026, 0, 0.014))
    c_ = bm.verts.new((tx + 0.018, 0, 0.0))
    d = bm.verts.new((tx + 0.026, 0, -0.012))
    bm.faces.new((a, b, c_, d))
    me = bpy.data.meshes.new('Tail')
    bm.to_mesh(me)
    bm.free()
    tail = bpy.data.objects.new('Tail', me)
    C.link_object(tail)
    I.solidify(tail, 0.003)
    M.assign(tail, ['M_Wood'])
    I.color_fn(tail, lambda co: I.lerp3(back, red, min(1.0, (co.x - tx) / 0.024)), rnd, 0.01)
    p.add(tail, 'none')
    # ojal de cordel en el morro y anzuelo de hueso bajo la panza
    ring = [(-L / 2 - 0.004 - math.cos(t) * 0.004 + 0.004, 0.0, math.sin(t) * 0.004)
            for t in [2 * math.pi * k / 12 for k in range(13)]]
    eyelet = I.sweep('Eyelet', ring, 0.0011, segs=5, cap=False)
    M.assign(eyelet, ['M_Fabric'])
    I.tint(eyelet, PAL['cord'][0], rnd, 0.01)
    p.add(eyelet, 'none')
    hook_pts = [(0.006, 0.0, -0.009), (0.006, 0.0, -0.02), (0.004, 0.0, -0.028), (-0.002, 0.0, -0.031),
                (-0.008, 0.0, -0.027), (-0.009, 0.0, -0.020)]
    hook = I.sweep('Hook', hook_pts, [0.0018, 0.0017, 0.0016, 0.0015, 0.0013, 0.0006], segs=6)
    M.assign(hook, ['M_Stone'])
    I.tint(hook, (0.80, 0.72, 0.56), rnd, 0.02)
    p.add(hook, 'none')
    H._wrap(p, rnd, -0.012, -0.007, 0.0028, rgb=PAL['cord'][0], cord_r=0.0009, name='HookWrap',
            center=(0.006, 0.0))
    obj = p.finish(name)
    # tumbado de costado, con el ojo a la vista y el anzuelo hacia delante (-Y)
    obj.data.transform(Matrix.Rotation(math.radians(-80), 4, 'X'))
    return obj
