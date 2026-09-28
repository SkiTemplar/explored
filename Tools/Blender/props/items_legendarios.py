"""
items_legendarios.py — recompensas de las capturas legendarias (biblia
§4.6, items.json): trofeo de El Viejo, dientes de Sombra, aguijón de La
Manta Negra, piel curtida de El Errante, y el sedal y el anzuelo del Rey de
Plata.

Son objetos únicos: más detalle y color más rico que el material corriente
del que son «versión legendaria» (nácar tornasolado, marfil con sierra,
negro violáceo con dientes claros, cuero con cicatrices).

El trofeo reutiliza el mero de items_pescados.py escalado a 1,35 (El Viejo
es un mero gigante) montado en un tablero tallado que se tiene en pie.

Objetos sueltos: pivote en la base (z = 0), centrados en XY, en su postura
de reposo (los alargados tumbados a lo largo de +X). Escala real en metros.
"""

import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
sys.path.insert(0, os.path.dirname(__file__))
import bpy  # noqa: E402

import _items as I  # noqa: E402
import _materials as M  # noqa: E402
import bmesh  # noqa: E402
import common as C  # noqa: E402
import items_despojos as D  # noqa: E402
import items_pescados as F  # noqa: E402
import kit_construccion as K  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

GROUP = I.GROUP
PAL = I.PAL

LPAL = {
    'ivory': (0.93, 0.88, 0.74),
    'ivory_edge': (0.98, 0.96, 0.88),
    'tooth_root': (0.52, 0.32, 0.14),
    'barb': (0.05, 0.025, 0.08),
    'barb_mid': (0.20, 0.10, 0.28),
    'barb_edge': (0.86, 0.78, 0.60),
    'pearl': [(0.95, 0.80, 0.84), (0.55, 0.88, 0.86), (0.96, 0.92, 0.78), (0.74, 0.70, 0.95)],
    'pearl_back': (0.30, 0.20, 0.12),
    'bone': (0.88, 0.80, 0.60),
    'bone_dark': (0.66, 0.52, 0.30),
    'line': (0.78, 0.92, 0.96),
    'line_dark': (0.46, 0.66, 0.78),
    'line_gold': (0.95, 0.72, 0.20),
    'hide_out': (0.03, 0.15, 0.18),
    'hide_out_hi': (0.10, 0.28, 0.28),
    'hide_belly': (0.80, 0.70, 0.52),
    'hide_in': (0.62, 0.38, 0.15),
    'hide_in_hi': (0.78, 0.54, 0.24),
    'scar': (0.92, 0.72, 0.62),
    'board': (0.40, 0.19, 0.07),
    'board_hi': (0.58, 0.32, 0.12),
    'frame': (0.18, 0.08, 0.03),
    'feather_red': (0.80, 0.08, 0.03),
    'feather_white': (0.95, 0.92, 0.84),
}

VARIANTS = [
    dict(name='Item_TrofeoElViejo', item_id='trofeo_el_viejo', seed=5201, builder='trofeo', tri_budget=(1500, 16000)),
    dict(name='Item_DientesSombra', item_id='dientes_sombra', seed=5202, builder='dientes', tri_budget=(500, 8000)),
    dict(name='Item_AguijonMantaNegra', item_id='aguijon_manta_negra', seed=5203, builder='aguijon',
         tri_budget=(300, 6000)),
    dict(name='Item_PielElErrante', item_id='piel_el_errante', seed=5204, builder='piel_errante',
         tri_budget=(500, 8000), preview_rot_z=0.4),
    dict(name='Item_SedalLegendario', item_id='sedal_legendario', seed=5205, builder='sedal', tri_budget=(500, 8000)),
    dict(name='Item_AnzueloLegendario', item_id='anzuelo_legendario', seed=5206, builder='anzuelo',
         tri_budget=(400, 8000)),
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
    return I.ground_centered(obj)


# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------
def _densify(outline, per=4, serrate=None):
    """Remuestrea un contorno cerrado [(x, y)] con `per` puntos por lado.
    serrate(i_lado) -> amplitud: dientes de sierra perpendiculares al lado."""
    out = []
    n = len(outline)
    for i in range(n):
        a, b = Vector(outline[i]), Vector(outline[(i + 1) % n])
        amp = serrate(i) if serrate else 0.0
        d = b - a
        nrm = Vector((d.y, -d.x)).normalized() if d.length > 1e-9 else Vector((0, 0))
        for s in range(per):
            p = a.lerp(b, s / per)
            if amp and s % 2 == 1:
                p = p + nrm * amp
            out.append((p.x, p.y))
    return out


def _flat_piece(name, outline, thick, rings=4, power=0.55, center=None):
    """Pieza plana biconvexa (diente, aguijón, lámina de nácar) a partir de
    un contorno en XY con forma de estrella respecto a `center`: anillos
    concéntricos que suben hasta el grosor máximo en el centro, filo en el
    borde. Grosor en Z, apoyada con su cara de abajo en z = -thick/2.
    Índices: anillo 0 (borde) compartido; luego capa de arriba y de abajo."""
    pts = [Vector((x, y, 0.0)) for x, y in outline]
    c = Vector(center + (0.0,)) if center else sum(pts, Vector()) / len(pts)
    bm = bmesh.new()
    edge = [bm.verts.new(p) for p in pts]
    n = len(pts)
    layers = {}
    for sgn in (1, -1):
        prev = edge
        for k in range(1, rings + 1):
            t = k / (rings + 1)
            z = sgn * thick / 2 * (t ** power)
            ring = [bm.verts.new(c + (p - c) * (1 - t) + Vector((0, 0, z))) for p in pts]
            for i in range(n):
                i2 = (i + 1) % n
                f = (prev[i], prev[i2], ring[i2], ring[i])
                bm.faces.new(f if sgn > 0 else f[::-1])
            prev = ring
        pole = bm.verts.new(c + Vector((0, 0, sgn * thick / 2)))
        for i in range(n):
            f = (prev[i], prev[(i + 1) % n], pole)
            bm.faces.new(f if sgn > 0 else f[::-1])
        layers[sgn] = prev
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new(name, me)
    C.link_object(o)
    return o


def _cord(p, rnd, name, pts, r, rgb, segs=5):
    o = I.sweep(name, pts, r, segs=segs)
    M.assign(o, ['M_Fabric'])
    I.cord_stripes(o, rgb, I.lerp3(rgb, (0.05, 0.02, 0.01), 0.45), segs, period=2)
    p.add(o, 'none')
    return o


def _wrap_x(p, rnd, name, x0, x1, radius, cord_r, rgb, center=(0.0, 0.0), turns=None):
    """Ligadura en hélice a lo largo de X (centro de la sección en (y, z))."""
    w = I.helix_wrap(name, x0, x1, radius, cord_r, rnd, turns=turns, segs=5)
    w.data.transform(Matrix.Rotation(math.pi / 2, 4, 'Y'))
    # tras el giro, z -> -x: vuelve a poner el tramo en [x0, x1]
    w.data.transform(Matrix.Scale(-1, 4, (1, 0, 0)))
    w.data.flip_normals()
    w.data.transform(Matrix.Translation((0.0, center[0], center[1])))
    M.assign(w, ['M_Fabric'])
    I.cord_stripes(w, rgb, I.lerp3(rgb, (0.05, 0.02, 0.01), 0.45), 5, period=2)
    p.add(w, 'none')
    return w


# ---------------------------------------------------------------------------
# TROFEO
# ---------------------------------------------------------------------------
@_register('trofeo')
def _b_trofeo(v, rnd, name):
    """Trofeo de El Viejo (1,2 m): el mero gigante (95 cm) montado sobre
    un tablero de madera oscura con marco y cordel de colgar, de pie sobre
    dos patas. El pez mira a -Y (la cara del tablero)."""
    p = K.Parts()
    S = 1.35
    fish = F._BUILDERS['mero'](dict(v, seed=4706), C.seeded_rng(4706), 'Fish')
    # el constructor lo tumba de costado: de vuelta a nado (lomo +Z)
    fish.data.transform(Matrix.Rotation(-math.pi / 2, 4, 'X'))
    fish.data.transform(Matrix.Scale(S, 4))
    xs = [q.co.x for q in fish.data.vertices]
    ys = [q.co.y for q in fish.data.vertices]
    zs = [q.co.z for q in fish.data.vertices]
    fx, fz = (min(xs) + max(xs)) / 2, (min(zs) + max(zs)) / 2
    board_w, board_h, board_t = max(xs) - min(xs) + 0.16, max(zs) - min(zs) + 0.16, 0.035
    z_board0 = 0.03
    cz = z_board0 + board_h / 2
    fish.data.transform(Matrix.Translation((-fx, 0.0, cz - fz)))
    fish.data.update()
    p.add(fish, 'none')
    y_back = max(ys) + 0.015
    # tablero con marco oscuro
    panel = I.soft_box('Panel', (board_w, board_t, board_h), center=(0.0, y_back + board_t / 2, cz), roundness=0.2,
                       cuts=10)
    M.assign(panel, ['M_Wood'])
    I.color_fn(panel, lambda co: I.lerp3(LPAL['board'], LPAL['board_hi'],
                                         0.5 + 0.5 * math.sin(co.z * 60 + 4 * I.noise3(co, 6))), rnd, 0.01)
    p.add(panel, 'none')
    fw = 0.045
    for k, (sx, sz, cx, czz) in enumerate(((board_w + fw, fw, 0.0, cz + board_h / 2),
                                           (board_w + fw, fw, 0.0, cz - board_h / 2),
                                           (fw, board_h, board_w / 2, cz), (fw, board_h, -board_w / 2, cz))):
        fr = I.soft_box(f'Frame{k}', (sx, board_t + 0.012, sz), center=(cx, y_back + board_t / 2, czz),
                        roundness=0.35, cuts=4)
        M.assign(fr, ['M_Wood'])

        def frc(co):
            # dientes de tiburón tallados (niho mano): triángulos claros
            u = (co.x + co.z) * 30
            return LPAL['frame'] if (u % 1.0) < 0.5 else I.lerp3(LPAL['frame'], LPAL['board_hi'], 0.6)
        I.color_fn(fr, frc, rnd, 0.01)
        p.add(fr, 'none')
    # patas: dos zapatas que calzan el tablero por detrás y por delante
    for sx in (-1, 1):
        foot = I.soft_box(f'Foot{sx}', (0.07, 0.34, 0.05), center=(sx * board_w * 0.33, y_back + board_t / 2, 0.025),
                          roundness=0.35, cuts=4)
        M.assign(foot, ['M_Wood'])
        I.tint(foot, LPAL['frame'], rnd, 0.02)
        p.add(foot, 'none')
    # espigas que sujetan el pez al tablero
    for px in (-0.18, 0.12):
        peg = I.lathe(f'Peg{px}', [(0.0, 0.0), (0.012, 0.0), (0.012, 0.1), (0.0, 0.1)], segs=10)
        peg.data.transform(Matrix.Rotation(-math.pi / 2, 4, 'X'))
        peg.data.transform(Matrix.Translation((px, y_back - 0.1 + 0.005, cz)))
        M.assign(peg, ['M_Wood'])
        I.tint(peg, LPAL['frame'], rnd, 0.02)
        p.add(peg, 'none')
    # cordel de colgar en lo alto
    top = cz + board_h / 2 + fw / 2
    pts = [(-0.22 + 0.44 * i / 12, y_back + board_t / 2, top + 0.09 * math.sin(math.pi * i / 12)) for i in range(13)]
    _cord(p, rnd, 'Hang', pts, 0.006, PAL['cord_red'], segs=6)
    obj = p.finish(name)
    return obj


# ---------------------------------------------------------------------------
# DIENTES DE SOMBRA
# ---------------------------------------------------------------------------
def _tooth_outline(w, h):
    """Diente de tiburón tigre en «cresta de gallo»: corona asimétrica con
    muesca en el borde distal, sierra en los dos filos; raíz bilobulada.
    Punta hacia +Y, raíz hacia -Y."""
    crown = [(-w / 2, 0.0), (-w * 0.3, h * 0.35), (-w * 0.05, h * 0.75), (w * 0.12, h),
             (w * 0.2, h * 0.55), (w * 0.28, h * 0.4), (w * 0.5, h * 0.18), (w * 0.5, 0.0)]
    root = [(w * 0.46, -h * 0.22), (w * 0.2, -h * 0.3), (0.0, -h * 0.18), (-w * 0.2, -h * 0.3),
            (-w * 0.46, -h * 0.22)]
    outline = crown + root
    # sierra: en los lados de la corona (0..6)
    return _densify(outline, per=4, serrate=lambda i: (w * 0.014 if i <= 6 else 0.0))


@_register('dientes')
def _b_dientes(v, rnd, name):
    """Dientes de Sombra (22 cm): cinco dientes de tiburón tigre de 5 cm,
    marfil con sierra y raíz parda, ensartados en un cordel rojo con nudos,
    tendidos en arco sobre el suelo."""
    p = K.Parts()
    thick = 0.0095
    R = 0.11
    angs = [-0.62, -0.31, 0.0, 0.31, 0.62]
    for k, a in enumerate(angs):
        s = 1.0 - 0.12 * abs(k - 2)
        w, h = 0.036 * s, 0.05 * s
        o = _flat_piece(f'Tooth{k}', _tooth_outline(w, h), thick * s, rings=3, power=0.8, center=(0.0, h * 0.25))

        def tc(co, h=h):
            if co.y < 0.0:
                return I.lerp3(LPAL['tooth_root'], LPAL['bone_dark'], 0.5 + 0.5 * I.noise3(co, 200))
            t = co.y / h
            c = I.lerp3(LPAL['ivory'], LPAL['ivory_edge'], t)
            # esmalte más claro hacia los filos (la capa fina)
            return I.lerp3(c, (1.0, 1.0, 0.95), max(0.0, 1 - abs(co.z) / (thick * 0.25)) * 0.4)
        I.color_fn(o, tc, rnd, 0.01)
        M.assign(o, ['M_Stone'])
        # la punta hacia fuera del arco, la raíz sobre el cordel
        o.data.transform(Matrix.Translation((0.0, 0.0, thick * s / 2)))
        o.data.transform(Matrix.Rotation(-a, 4, 'Z'))
        o.data.transform(Matrix.Translation((math.sin(a) * R, math.cos(a) * R - R, 0.0)))
        p.add(o, 'none')
        # nudo que sujeta cada diente (pasa por un agujero de la raíz)
        kp = Vector((math.sin(a) * (R - 0.009 * s), math.cos(a) * (R - 0.009 * s) - R, thick * s + 0.0015))
        knot = C.make_blob(f'Knot{k}', kp, 0.005, v['seed'] + k, subdivisions=2, noise_strength=0.25,
                           scale=(1.0, 1.0, 0.6))
        M.assign(knot, ['M_Fabric'])
        I.tint(knot, PAL['cord_red'], rnd, 0.02)
        p.add(knot, 'none')
    # cordel: arco por las raíces que baja al suelo entre dientes y remata
    # en dos cabos sueltos
    pts = []
    rc = R - 0.009
    for i in range(41):
        a = -0.95 + 1.9 * i / 40
        near = min(abs(a - b) for b in angs)
        z = 0.002 + max(0.0, 1 - near / 0.12) * 0.0075 if abs(a) < 0.75 else 0.002
        rr = rc if abs(a) < 0.75 else rc - (abs(a) - 0.75) * 0.1
        pts.append((math.sin(a) * rr, math.cos(a) * rr - R, z))
    _cord(p, rnd, 'Cord', pts, 0.002, PAL['cord_red'], segs=6)
    for k, q in enumerate((pts[0], pts[-1])):
        end = C.make_blob(f'End{k}', q, 0.0042, v['seed'] + 10 + k, subdivisions=2, noise_strength=0.25,
                          scale=(1.0, 1.0, 0.7))
        M.assign(end, ['M_Fabric'])
        I.tint(end, PAL['cord_red'], rnd, 0.02)
        p.add(end, 'none')
    return p.finish(name)


# ---------------------------------------------------------------------------
# AGUIJÓN DE LA MANTA NEGRA
# ---------------------------------------------------------------------------
@_register('aguijon')
def _b_aguijon(v, rnd, name):
    """Aguijón de La Manta Negra (26 cm): púa larga y plana negro violácea
    con una hilera de dientes de sierra claros vueltos hacia atrás en cada
    filo, base envuelta en cordel rojo con un trozo de piel oscura."""
    p = K.Parts()
    L, W = 0.22, 0.02
    n = 26
    right, left = [], []
    for i in range(n + 1):
        t = i / n
        x = t * L
        hw = W / 2 * (1 - t) ** 0.75 + 0.0004
        # dientes vueltos hacia la base: la punta de cada diente queda atrás
        saw = 0.003 * (1 - t) ** 0.3 if (i % 2 == 1 and 0.12 < t < 0.95) else 0.0
        right.append((x - (0.004 if saw else 0.0), -(hw + saw)))
        left.append((x - (0.004 if saw else 0.0), hw + saw))
    outline = right + [(L + 0.004, 0.0)] + left[::-1] + [(-0.004, 0.0)]
    thick = 0.006
    o = _flat_piece('Barb', outline, thick, rings=3, power=0.5, center=(L * 0.35, 0.0))

    def bc(co):
        t = max(0.0, min(1.0, co.x / L))
        hw = W / 2 * (1 - t) ** 0.75 + 0.0004
        s = abs(co.y) / hw
        c = I.lerp3(LPAL['barb'], LPAL['barb_mid'], min(1.0, s))
        if s > 1.0:
            c = LPAL['barb_edge']
        if abs(co.y) < 0.0012:
            c = I.lerp3(c, LPAL['barb_mid'], 0.6)  # cresta central
        return c
    I.color_fn(o, bc, rnd, 0.01)
    M.assign(o, ['M_Stone'])
    o.data.transform(Matrix.Translation((0.0, 0.0, thick / 2)))
    p.add(o, 'none')
    # base: muñón de piel oscura + ligadura roja
    stub = C.make_blob('Stub', (-0.012, 0.0, 0.007), 1.0, v['seed'], subdivisions=3, noise_strength=0.15,
                       scale=(0.022, 0.013, 0.0075), relax_iterations=1)
    M.assign(stub, ['M_Fabric'])
    I.color_fn(stub, lambda co: I.lerp3((0.10, 0.05, 0.09), (0.26, 0.12, 0.16), 0.5 + 0.5 * I.noise3(co, 90)), rnd,
               0.01)
    p.add(stub, 'none')
    _wrap_x(p, rnd, 'Wrap', -0.002, 0.03, 0.0095, 0.0018, PAL['cord_red'], center=(0.0, 0.0065), turns=7)
    return p.finish(name)


# ---------------------------------------------------------------------------
# PIEL CURTIDA DE EL ERRANTE
# ---------------------------------------------------------------------------
def _fold(o, xf, r, rest):
    """Dobla la lámina sobre sí misma por x = xf (lo que queda en +X pasa
    por encima): se enrolla en un cilindro de radio r y el tramo ya doblado
    baja hasta quedar a `rest` de altura sobre la mitad de abajo."""
    for q in o.data.vertices:
        x, y, h = q.co
        d = x - xf
        if d <= 0:
            continue
        th = d / r
        rho = r - h
        if th <= math.pi:
            q.co = Vector((xf + rho * math.sin(th), y, r - rho * math.cos(th)))
        else:
            run = d - math.pi * r
            sag = min(2 * r - rest, run * 0.25)
            q.co = Vector((xf - run, y, r + rho - sag))
    o.data.update()


@_register('piel_errante')
def _b_piel_errante(v, rnd, name):
    """Piel curtida de El Errante (60 cm abierta, doblada por la mitad):
    lomo verde azulado oscuro cruzado por tres cicatrices largas claras,
    vientre crema en los bordes; la solapa doblada enseña la cara curtida
    color miel. Una tira de cuero ata el fardo."""
    p = K.Parts()
    L = 0.60
    scars = [((-0.27, -0.02), (-0.12, 0.05)), ((-0.25, -0.05), (-0.13, 0.0)), ((-0.2, 0.05), (-0.1, -0.03))]

    def lift(u, s):
        return 0.003 * math.sin(u * 11) * math.cos(s * 3) + 0.004 * abs(s) ** 3

    o, n = D._sheet('Hide', L, lambda u: D._shark_hide_shape(u) * 1.1, lift, nu=64, nv=20, thick=0.004)
    M.assign(o, ['M_Fabric'])
    top = D._top_layer(o, n)

    def seg_dist(pt, a, b):
        a, b, pt = Vector(a), Vector(b), Vector(pt)
        ab = b - a
        t = max(0.0, min(1.0, (pt - a).dot(ab) / ab.length_squared))
        return (a + ab * t - pt).length

    def col(vv):
        co = vv.co
        if vv.index not in top:
            return I.lerp3(LPAL['hide_in'], LPAL['hide_in_hi'], 0.5 + 0.5 * I.noise3(co, 25)) + (0.0,)
        w = max(1e-4, D._shark_hide_shape(co.x / (L / 2)) * 1.1)
        s = min(1.0, abs(co.y) / w)
        c = I.lerp3(LPAL['hide_out'], LPAL['hide_out_hi'], min(1.0, s / 0.6) * 0.8 + 0.2 * I.noise3(co, 40))
        c = I.lerp3(c, LPAL['hide_belly'], max(0.0, s - 0.7) / 0.3)
        for a, b in scars:
            d = seg_dist((co.x, co.y), a, b)
            if d < 0.006:
                c = I.lerp3(c, LPAL['scar'], 1.0 - d / 0.006 * 0.4)
        return c + (0.0,)
    C.set_vertex_colors(o, col)
    _fold(o, 0.1, 0.009, 0.0045)
    p.add(o, 'none')
    # tira de cuero alrededor del fardo
    xw = 0.02
    w = max(D._shark_hide_shape(xw / (L / 2)), D._shark_hide_shape(-xw / (L / 2))) * 1.1 + 0.004
    hz = 0.0135
    pts = []
    for i in range(33):
        a = 2 * math.pi * i / 32
        yy = math.cos(a) * w
        zz = hz + math.sin(a) * (hz + 0.0012)
        pts.append((xw + 0.004 * math.sin(a * 2), yy, max(0.0012, zz)))
    band = I.sweep('Band', pts, 0.004, segs=6)
    M.assign(band, ['M_Fabric'])
    I.tint(band, (0.30, 0.13, 0.04), rnd, 0.02)
    p.add(band, 'none')
    knot = C.make_blob('Knot', (xw, 0.0, 2 * hz + 0.003), 0.007, v['seed'], subdivisions=2, noise_strength=0.2,
                       scale=(1.0, 1.3, 0.6))
    M.assign(knot, ['M_Fabric'])
    I.tint(knot, (0.30, 0.13, 0.04), rnd, 0.02)
    p.add(knot, 'none')
    return p.finish(name)


# ---------------------------------------------------------------------------
# SEDAL Y ANZUELO DEL REY DE PLATA
# ---------------------------------------------------------------------------
@_register('sedal')
def _b_sedal(v, rnd, name):
    """Sedal legendario: carrete de madera tallada (Ø 8 cm) con incrustaciones
    de nácar, bien cargado de un hilo plateado azulado con una hebra
    dorada; el cabo suelto baja al suelo y acaba en un palito de hueso."""
    p = K.Parts()
    Rf, Rc, H = 0.04, 0.014, 0.06
    ft = 0.007
    prof = [(0.0, 0.0), (Rf * 0.96, 0.0), (Rf, 0.003), (Rf * 0.96, ft), (Rc, ft), (Rc, H - ft), (Rf * 0.96, H - ft),
            (Rf, H - 0.003), (Rf * 0.96, H), (0.0, H)]
    spool = I.lathe('Spool', prof, segs=28)
    M.assign(spool, ['M_Wood'])
    inlays = [(k / 6 * 2 * math.pi + 0.3) for k in range(6)]

    def sc(co):
        a = math.atan2(co.y, co.x)
        rr = math.hypot(co.x, co.y)
        c = I.lerp3(LPAL['board'], LPAL['board_hi'], 0.5 + 0.5 * math.sin(a * 3 + rr * 200))
        if co.z > H - 0.001 and 0.02 < rr < 0.034:
            for ia in inlays:
                if abs(math.atan2(math.sin(a - ia), math.cos(a - ia))) < 0.22:
                    return LPAL['pearl'][inlays.index(ia) % 4]
        if rr > Rf * 0.97:
            return LPAL['frame']
        return c
    I.color_fn(spool, sc, rnd, 0.01)
    p.add(spool, 'none')
    # hilo enrollado: tambor con bultito en el centro y rayas en hélice
    Rw = 0.031
    wprof = [(0.0, ft), (Rw * 0.96, ft), (Rw, ft + 0.004)] + \
        [(Rw + 0.0015 * math.sin(math.pi * k / 6), ft + 0.004 + (H - 2 * ft - 0.008) * k / 6) for k in range(1, 6)] + \
        [(Rw, H - ft - 0.004), (Rw * 0.96, H - ft), (0.0, H - ft)]
    wind = I.lathe('Line', wprof, segs=36)
    M.assign(wind, ['M_Fabric'])

    def lc(co):
        a = math.atan2(co.y, co.x)
        u = (a / (2 * math.pi) * 18 + co.z * 900) % 1.0
        c = LPAL['line'] if u < 0.6 else LPAL['line_dark']
        if ((a / (2 * math.pi) * 3 + co.z * 120) % 1.0) < 0.07:
            c = LPAL['line_gold']
        return c
    I.color_fn(wind, lc, rnd, 0.01)
    p.add(wind, 'none')
    # cabo suelto: sale del tambor, cae por el borde de la pestaña y serpentea por el suelo
    a0 = -0.6
    start = Vector((math.cos(a0) * Rw, math.sin(a0) * Rw, H * 0.55))
    lip = Vector((math.cos(a0) * (Rf + 0.002), math.sin(a0) * (Rf + 0.002), 0.004))
    pts = [start, start.lerp(lip, 0.5) + Vector((math.cos(a0) * 0.004, math.sin(a0) * 0.004, 0.0)), lip]
    last = lip
    for i in range(1, 9):
        t = i / 8
        q = Vector((lip.x + t * 0.11, lip.y - 0.03 * math.sin(t * 5), 0.0012))
        pts.append(q)
        last = q
    line = I.sweep('Loose', pts, 0.0011, segs=5)
    M.assign(line, ['M_Fabric'])
    I.tint(line, LPAL['line'], rnd, 0.01)
    p.add(line, 'none')
    tog = I.sweep('Toggle', [last + Vector((0.0, -0.018, 0.0022)), last + Vector((0.0, 0.018, 0.0022))],
                  [0.0022, 0.0022], segs=6)
    M.assign(tog, ['M_Stone'])
    I.tint(tog, LPAL['bone'], rnd, 0.02)
    p.add(tog, 'none')
    return p.finish(name)


@_register('anzuelo')
def _b_anzuelo(v, rnd, name):
    """Anzuelo legendario (pā, 11 cm): caña de nácar tornasolado con forma
    de pececillo, punta curva de hueso atada con cordel rojo, penacho de
    fibras blancas y rojas y un ojal de cordel para el sedal."""
    p = K.Parts()
    L, W = 0.085, 0.02
    n = 14
    top, bot = [], []
    for i in range(n + 1):
        t = i / n
        x = t * L
        hw = W / 2 * math.sin(math.pi * (0.08 + 0.84 * t)) ** 0.7
        bend = 0.006 * math.sin(math.pi * t)  # lomo algo curvado
        top.append((x, bend + hw))
        bot.append((x, bend - hw * 0.8))
    outline = bot + top[::-1]
    thick = 0.007
    shank = _flat_piece('Shank', outline, thick, rings=3, power=0.5, center=(L / 2, 0.004))

    def pc(co):
        bands = LPAL['pearl']
        u = (co.x * 60 + I.noise3(co, 50, 2.0) * 1.5 + co.z * 120) % len(bands)
        i0 = int(u)
        c = I.lerp3(bands[i0], bands[(i0 + 1) % len(bands)], u - i0)
        if co.z < -thick * 0.3:
            c = I.lerp3(c, LPAL['pearl_back'], 0.5)
        return c
    I.color_fn(shank, pc, rnd, 0.0)
    M.assign(shank, ['M_Stone'])
    shank.data.transform(Matrix.Translation((0.0, 0.0, thick / 2)))
    p.add(shank, 'none')
    # punta de hueso: sale de la cola de la caña (x = 0) y se curva en U
    pts, radii = [], []
    m = 14
    for i in range(m + 1):
        t = i / m
        a = math.pi * 0.95 * t
        rr = 0.016
        x = 0.008 - math.sin(a) * rr * 1.1
        y = -0.004 - rr + math.cos(a) * rr
        pts.append((x + (0.028 * t * t if t > 0.6 else 0.0), y + (0.03 * (t - 0.6) ** 2 if t > 0.6 else 0.0),
                    0.0035))
        radii.append(0.0032 * (1 - t) ** 0.6 + 0.0005)
    pts = [(0.02, 0.0, 0.0035), (0.012, -0.001, 0.0035)] + pts[1:]
    radii = [0.0032, 0.0032] + radii[1:]
    point = I.sweep('Point', pts, radii, segs=6)
    M.assign(point, ['M_Stone'])
    I.color_fn(point, lambda co: I.lerp3(LPAL['bone'], LPAL['bone_dark'], 0.3 + 0.3 * I.noise3(co, 150)), rnd, 0.01)
    p.add(point, 'none')
    # ligadura roja de la punta a la caña
    _wrap_x(p, rnd, 'Lash', 0.004, 0.02, 0.0062, 0.0014, PAL['cord_red'], center=(0.0, 0.0035), turns=6)
    # penacho de fibras en la cola
    for k in range(7):
        a = math.pi + (k - 3) * 0.14
        ln = 0.03 + rnd.uniform(-0.004, 0.006)
        f0 = Vector((0.004, 0.002, 0.004))
        f1 = f0 + Vector((math.cos(a) * ln, math.sin(a) * ln, 0.0))
        fpts = [f0, f0.lerp(f1, 0.5) + Vector((0, 0, 0.001)), f1 + Vector((0, 0, -0.0025))]
        fib = I.sweep(f'Fiber{k}', fpts, [0.0012, 0.001, 0.0005], segs=4)
        M.assign(fib, ['M_Fabric'])
        I.tint(fib, LPAL['feather_red'] if k % 2 else LPAL['feather_white'], rnd, 0.02)
        p.add(fib, 'none')
    # ojal de cordel en la cabeza y cabo de sedal
    loop = [(L - 0.004 + 0.008 * math.sin(math.pi * i / 10), 0.004 + 0.007 * math.cos(math.pi * i / 10) * 0.8,
             0.004) for i in range(11)]
    _cord(p, rnd, 'Eye', loop, 0.0012, PAL['cord'][0], segs=5)
    lead = [(L + 0.004, 0.004, 0.004), (L + 0.02, 0.006, 0.002), (L + 0.035, 0.0, 0.0011)]
    ld = I.sweep('Lead', lead, 0.0009, segs=5)
    M.assign(ld, ['M_Fabric'])
    I.tint(ld, LPAL['line'], rnd, 0.01)
    p.add(ld, 'none')
    return p.finish(name)
