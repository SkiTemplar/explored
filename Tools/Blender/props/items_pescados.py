"""
items_pescados.py — pescados y mariscos de items.json: los doce peces del
arrecife y del azul (atún, bonito, barracuda, dorado, jurel, mero, pargo,
pescado de arrecife, pez ballesta, pez cirujano, pez loro, salmonete), los
preparados (filete, pescado asado en espeto, ahumado y salado) y cuatro
mariscos (cangrejo, langosta de arrecife, lapa y erizo de mar).

Todos los peces salen del MISMO constructor paramétrico (`_fish`): cuerpo
en loft a lo largo de X con secciones elípticas (lomo y vientre de alto
distinto), aletas de membrana con radios plegados en abanico (dorsal, anal,
caudal, pectorales y pélvicas), ojo grande de dibujo animado y un patrón de
color por especie (barras, franjas, motas, escamas). Cada especie solo
declara su perfil, sus aletas y su función de color.

Las variantes cocinadas y secas son el MISMO modelo con otra paleta de
color de vértice: el pescado asado es el pescado de arrecife con paleta
dorada y marcas de brasa (más el espeto); el ahumado y el salado comparten
la malla del pescado abierto en mariposa.

Objetos sueltos: pivote en la base (z = 0), centrados en XY, en su postura
de reposo: los peces tumbados sobre el costado izquierdo (flanco derecho
hacia +Z, cabeza hacia +X). Escala real en metros.
"""

import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import common as C  # noqa: E402
import _materials as M  # noqa: E402
import _items as I  # noqa: E402
import kit_construccion as K  # noqa: E402

import bmesh  # noqa: E402
import bpy  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

GROUP = I.GROUP

# Colores lineales (se multiplican por los materiales casi blancos del kit).
FPAL = {
    'eye_iris': (0.80, 0.62, 0.10),
    'eye_pupil': (0.012, 0.010, 0.014),
    'eye_cooked': (0.86, 0.82, 0.70),
    'mouth': (0.10, 0.03, 0.03),
    'silver': (0.62, 0.68, 0.72),
    'belly': (0.86, 0.86, 0.80),
    'cooked': (0.62, 0.34, 0.08),
    'cooked_hi': (0.82, 0.54, 0.16),
    'cooked_char': (0.10, 0.04, 0.015),
    'cooked_fin': (0.40, 0.16, 0.04),
    'stick': (0.40, 0.25, 0.10),
    'stick_burnt': (0.07, 0.04, 0.02),
}


VARIANTS = [
    dict(name='Item_Atun', item_id='atun', seed=4701, builder='atun', tri_budget=(800, 9000)),
    dict(name='Item_Bonito', item_id='bonito', seed=4702, builder='bonito', tri_budget=(800, 9000)),
    dict(name='Item_Barracuda', item_id='barracuda', seed=4703, builder='barracuda', tri_budget=(800, 9000)),
    dict(name='Item_Dorado', item_id='dorado', seed=4704, builder='dorado', tri_budget=(800, 9000)),
    dict(name='Item_Jurel', item_id='jurel', seed=4705, builder='jurel', tri_budget=(800, 9000)),
    dict(name='Item_Mero', item_id='mero', seed=4706, builder='mero', tri_budget=(800, 9000)),
    dict(name='Item_Pargo', item_id='pargo', seed=4707, builder='pargo', tri_budget=(800, 9000)),
    dict(name='Item_PescadoArrecife', item_id='pescado_arrecife', seed=4708, builder='arrecife',
         tri_budget=(800, 9000)),
    dict(name='Item_PezBallesta', item_id='pez_ballesta', seed=4709, builder='ballesta', tri_budget=(800, 9000)),
    dict(name='Item_PezCirujano', item_id='pez_cirujano', seed=4710, builder='cirujano', tri_budget=(800, 9000)),
    dict(name='Item_PezLoro', item_id='pez_loro', seed=4711, builder='loro', tri_budget=(800, 9000)),
    dict(name='Item_Salmonete', item_id='salmonete', seed=4712, builder='salmonete', tri_budget=(800, 9000)),
    dict(name='Item_FiletePescado', item_id='filete_pescado', seed=4713, builder='filete', tri_budget=(200, 4000)),
    dict(name='Item_PescadoAsado', item_id='pescado_asado', seed=4708, builder='asado', tri_budget=(800, 9000)),
    dict(name='Item_PescadoAhumado', item_id='pescado_ahumado', seed=4715, builder='abierto',
         tri_budget=(300, 6000), cure='ahumado'),
    dict(name='Item_PescadoSalado', item_id='pescado_salado', seed=4715, builder='abierto',
         tri_budget=(300, 6000), cure='salado'),
    # preview_rot_z: solo en la lámina, para ver el cangrejo de frente y no con las patas hacia la cámara
    dict(name='Item_Cangrejo', item_id='cangrejo', seed=4717, builder='cangrejo', tri_budget=(800, 9000),
         preview_rot_z=-math.pi / 2),
    dict(name='Item_Langosta', item_id='langosta', seed=4718, builder='langosta', tri_budget=(800, 12000)),
    dict(name='Item_Lapa', item_id='lapa', seed=4719, builder='lapa', tri_budget=(200, 4000)),
    dict(name='Item_Erizo', item_id='erizo', seed=4720, builder='erizo', tri_budget=(500, 8000)),
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
# utilidades
# ---------------------------------------------------------------------------
def _catmull(ctrl, n_per):
    """Remuestrea una lista de tuplas de control (t, a, b, ...) con
    Catmull-Rom, n_per puntos por tramo; conserva los extremos."""
    out = []
    m = len(ctrl)
    k = len(ctrl[0])
    for i in range(m - 1):
        p0, p1, p2, p3 = ctrl[max(i - 1, 0)], ctrl[i], ctrl[i + 1], ctrl[min(i + 2, m - 1)]
        for s in range(n_per):
            t = s / n_per
            t2, t3 = t * t, t * t * t
            out.append(tuple(0.5 * ((2 * p1[j]) + (-p0[j] + p2[j]) * t
                                    + (2 * p0[j] - 5 * p1[j] + 4 * p2[j] - p3[j]) * t2
                                    + (-p0[j] + 3 * p1[j] - 3 * p2[j] + p3[j]) * t3) for j in range(k)))
    out.append(tuple(ctrl[-1]))
    return out


def _interp(table, u):
    """Interpolación lineal en una tabla [(u, a, b, ...)] ordenada por u."""
    if u <= table[0][0]:
        return table[0][1:]
    for a, b in zip(table, table[1:]):
        if u <= b[0]:
            f = (u - a[0]) / max(b[0] - a[0], 1e-9)
            return tuple(a[j] + (b[j] - a[j]) * f for j in range(1, len(a)))
    return table[-1][1:]


def _mesh_from_bm(bm, name):
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new(name, me)
    C.link_object(o)
    return o


def _smoothstep(e0, e1, x):
    t = max(0.0, min(1.0, (x - e0) / (e1 - e0)))
    return t * t * (3 - 2 * t)


def _hash01(i, j, seed):
    h = (i * 73856093) ^ (j * 19349663) ^ (seed * 83492791)
    h = (h ^ (h >> 13)) * 1274126177
    return ((h ^ (h >> 16)) & 0xFFFF) / 65535.0


def _spots(u, v, freq_u, freq_v, radius, seed, jitter=0.35):
    """Motas redondas repartidas en una rejilla con jitter: 0..1 (1 dentro)."""
    gu, gv = u * freq_u, v * freq_v
    best = 9.0
    iu, iv = int(math.floor(gu)), int(math.floor(gv))
    for du in (-1, 0, 1):
        for dv in (-1, 0, 1):
            cu, cv = iu + du, iv + dv
            ou = 0.5 + (_hash01(cu, cv, seed) - 0.5) * 2 * jitter
            ov = 0.5 + (_hash01(cv, cu, seed + 7) - 0.5) * 2 * jitter
            d = math.hypot(gu - (cu + ou), gv - (cv + ov))
            best = min(best, d)
    return 1.0 - _smoothstep(radius * 0.8, radius, best)


# ---------------------------------------------------------------------------
# cuerpo de pez
# ---------------------------------------------------------------------------
class FishBody:
    """Loft del cuerpo a lo largo de X (hocico en +L/2, pedúnculo caudal
    en -L/2) a partir de un perfil de control [(u, lomo, vientre, medio
    ancho)] en fracciones de L, con u = 0 en el hocico y u = 1 en la cola.
    `snout_z` levanta/baja la punta del hocico (fracción de L)."""

    def __init__(self, L, prof, segs=20, n_per=4, snout_z=0.0, mid_z=None):
        self.L = L
        self.table = _catmull(prof, n_per)
        self.segs = segs
        self.snout_z = snout_z
        # línea media: por defecto el centro va del hocico al eje de la cola
        self.mid_z = mid_z or (lambda u: 0.0)

    def at(self, u):
        top, bot, hw = _interp(self.table, u)
        return top * self.L, bot * self.L, hw * self.L, self.mid_z(u) * self.L

    def x(self, u):
        return (0.5 - u) * self.L

    def surface(self, u, ang):
        """Punto de la superficie en (u, ángulo): ang = 0 flanco +Y,
        π/2 lomo, -π/2 vientre."""
        top, bot, hw, zc = self.at(u)
        s, c = math.sin(ang), math.cos(ang)
        return Vector((self.x(u), hw * c, zc + (top if s > 0 else bot) * s))

    def build(self, name):
        bm = bmesh.new()
        L = self.L
        segs = self.segs
        tip = bm.verts.new((L / 2 + 0.004 * L, 0.0, self.snout_z * L))
        rings = []
        for (u, top, bot, hw) in self.table:
            if u <= 0.0:
                continue
            zc = self.mid_z(u) * L
            ring = []
            for k in range(segs):
                a = 2 * math.pi * k / segs
                s, c = math.sin(a), math.cos(a)
                # sección algo «cuadrada» por arriba en peces anchos
                z = zc + (top if s > 0 else bot) * L * s
                ring.append(bm.verts.new(((0.5 - u) * L, hw * L * c, z)))
            rings.append(ring)
        for k in range(segs):
            bm.faces.new((tip, rings[0][(k + 1) % segs], rings[0][k]))
        for a, b in zip(rings, rings[1:]):
            for k in range(segs):
                k2 = (k + 1) % segs
                bm.faces.new((a[k], a[k2], b[k2], b[k]))
        bm.faces.new(list(reversed(rings[-1])))
        o = _mesh_from_bm(bm, name)
        return o

    def coords(self, co):
        """(u, zn) de un punto: u a lo largo del cuerpo, zn -1 vientre .. 1 lomo."""
        u = max(0.0, min(1.0, 0.5 - co.x / self.L))
        top, bot, hw, zc = self.at(u)
        dz = co.z - zc
        zn = dz / max(top, 1e-5) if dz > 0 else dz / max(bot, 1e-5)
        return u, max(-1.0, min(1.0, zn))


def _fin_mesh(name, base, tips, rows=3, fold=0.0, thick=0.002, arch=0.0):
    """Aleta de membrana: rejilla entre la línea de base (sobre el cuerpo)
    y la línea de puntas, con los radios plegados en abanico (`fold`,
    desplazamiento alterno a lo largo de `fold_dir`). Devuelve el objeto
    y una función (row, col) por índice de vértice ANTES de solidificar."""
    n = len(base)
    bm = bmesh.new()
    grid = []
    for r in range(rows + 1):
        t = r / rows
        row = []
        for c in range(n):
            p = base[c].lerp(tips[c], t)
            # pliegue de radios: columnas alternas adelante/atrás de la membrana
            if fold and 0 < c < n - 1:
                p = p + fold[1] * (fold[0] * t * (1 if c % 2 else -1))
            if arch:
                p = p + arch[1] * (arch[0] * math.sin(t * math.pi))
            row.append(bm.verts.new(p))
        grid.append(row)
    for r in range(rows):
        for c in range(n - 1):
            bm.faces.new((grid[r][c], grid[r][c + 1], grid[r + 1][c + 1], grid[r + 1][c]))
    o = _mesh_from_bm(bm, name)
    return o, n


def _solidify(o, thick):
    """Grosor de membrana SIN «even offset»: en las aletas en media luna
    las caras casi degeneradas del centro disparaban el desplazamiento
    uniforme a metros de distancia."""
    C.select_only(o)
    bpy.context.view_layer.objects.active = o
    mod = o.modifiers.new('Solid', 'SOLIDIFY')
    mod.thickness = thick
    mod.offset = 0.0
    mod.use_even_offset = False
    bpy.ops.object.modifier_apply(modifier=mod.name)


def _paint_fin(o, n, rows, c0, c1, ray_dark=0.25, rim=None):
    """Color de aleta por (fila, columna): de c0 en la base a c1 en el
    borde; los radios pares algo más oscuros (pliegues); `rim` tiñe la
    última fila (borde de la aleta)."""
    def f(v):
        r, c = divmod(v.index, n)
        t = r / rows
        col = I.lerp3(c0, c1, t)
        if c % 2 == 0:
            col = I.lerp3(col, (0.0, 0.0, 0.0), ray_dark * t)
        if rim is not None and r == rows:
            col = rim
        return tuple(col) + (0.0,)
    C.set_vertex_colors(o, f)


def _fin(p, name, base, tips, c0, c1, rows=3, fold=None, thick=0.0025, ray_dark=0.25, rim=None, arch=None):
    base = [Vector(b) for b in base]
    tips = [Vector(t) for t in tips]
    o, n = _fin_mesh(name, base, tips, rows=rows, fold=fold, arch=arch)
    M.assign(o, ['M_Leaf'])
    _paint_fin(o, n, rows, c0, c1, ray_dark, rim)
    _solidify(o, thick)
    p.add(o, 'none')
    return o


def median_fin(p, body, name, u0, u1, heights, c0, c1, sweep=0.5, side='top', inset=0.25, rows=3,
               thick=None, ray_dark=0.25, rim=None):
    """Aleta impar (dorsal/anal) entre u0 y u1 sobre el lomo (side='top') o
    el vientre ('bot'); `heights` (fracción de L) se reparte a lo largo;
    `sweep` echa las puntas hacia atrás (fracción de la altura)."""
    L = body.L
    n = len(heights)
    base, tips = [], []
    for i, h in enumerate(heights):
        u = u0 + (u1 - u0) * i / (n - 1)
        top, bot, hw, zc = body.at(u)
        if side == 'top':
            zb = zc + top * (1 - inset)
            zt = zc + top + h * L
        else:
            zb = zc - bot * (1 - inset)
            zt = zc - bot - h * L
        x = body.x(u)
        base.append((x, 0.0, zb))
        tips.append((x - sweep * h * L, 0.0, zt))
    fold = (0.004 * L, Vector((0, 1, 0)))
    return _fin(p, name, base, tips, c0, c1, rows=rows, fold=fold, thick=thick or 0.004 * L,
                ray_dark=ray_dark, rim=rim)


def caudal_fin(p, body, name, length, span, c0, c1, shape='fork', notch=0.55, n=9, rows=3, thick=None,
               ray_dark=0.3, rim=None, u=None, tilt=0.0):
    """Aleta caudal en el pedúnculo. `length` y `span` (medio alto) en
    fracción de L. shape: 'fork' (horquilla), 'lunate' (media luna),
    'truncate' (recta), 'round' (redondeada)."""
    L = body.L
    if u is None:
        u = body.table[-1][0]  # pedúnculo: último anillo del cuerpo
    top, bot, hw, zc = body.at(u)
    x0 = body.x(u) + 0.02 * L
    base, tips = [], []
    for i in range(n):
        s = -1 + 2 * i / (n - 1)
        base.append((x0, 0.0, zc + s * max(top, bot) * 0.9))
        a = abs(s)
        if shape == 'fork':
            ext = length * (1 - notch * (1 - a ** 1.6))
            z = s * span
        elif shape == 'lunate':
            ext = length * (1 - notch * (1 - a ** 2.2))
            z = s * span * (0.75 + 0.25 * a)
        elif shape == 'truncate':
            ext = length * (0.92 + 0.08 * a - 0.06 * (1 - a) * notch)
            z = s * span
        else:  # round
            ext = length * (0.72 + 0.28 * math.cos(s * math.pi / 2) ** 0.7)
            z = s * span * 0.92
        tips.append((x0 - ext * L, 0.0, zc + z * L + tilt * ext * L))
    fold = (0.002 * L, Vector((0, 1, 0)))
    return _fin(p, name, base, tips, c0, c1, rows=rows, fold=fold, thick=thick or 0.004 * L,
                ray_dark=ray_dark, rim=rim)


def paired_fin(p, body, name, u, zn, length, width, c0, c1, angle=0.5, droop=0.3, splay=0.25, n=6, rows=3,
               ray_dark=0.2, thick=None, rim=None, both=True, round_tip=False):
    """Aleta par (pectoral o pélvica) a cada lado: nace en el flanco a la
    altura zn (−1 vientre .. 1 lomo) y se echa hacia atrás pegada al cuerpo
    (`splay` la separa del flanco). `angle`: abanico en radianes."""
    L = body.L
    out = []
    for side in ((1, -1) if both else (1,)):
        top, bot, hw, zc = body.at(u)
        zz = zc + (top if zn > 0 else bot) * zn
        yy = hw * math.sqrt(max(0.0, 1 - zn * zn)) * 0.92 * side
        root = Vector((body.x(u), yy, zz))
        base, tips = [], []
        for i in range(n):
            s = -1 + 2 * i / (n - 1)
            base.append(root + Vector((s * width * 0.18 * L, 0.0, s * width * 0.5 * L)))
            a = -droop + s * angle / 2
            ext = length * L * ((0.75 + 0.25 * math.cos(s * math.pi / 2)) if round_tip else
                                (1.0 - 0.35 * (s + 1) / 2))
            d = Vector((-math.cos(a), splay * side, math.sin(a))).normalized()
            tips.append(root + d * ext)
        fold = (0.003 * L, Vector((0, side, 0)))
        out.append(_fin(p, f'{name}{side}', base, tips, c0, c1, rows=rows, fold=fold,
                        thick=thick or 0.003 * L, ray_dark=ray_dark, rim=rim))
    return out


def eyes(p, body, u, zn, radius, iris, pupil=None, cooked=False, both=True, pupil_scale=0.6):
    """Ojo de dibujo animado a cada lado: iris algo saltón y pupila negra
    grande con un brillo claro (cocinado: ojo blanco opaco)."""
    L = body.L
    r = radius * L * 1.3
    for side in ((1, -1) if both else (1,)):
        top, bot, hw, zc = body.at(u)
        zz = zc + (top if zn > 0 else bot) * zn
        ysurf = hw * math.sqrt(max(0.0, 1 - zn * zn))
        c = Vector((body.x(u), side * (ysurf - r * 0.25), zz))
        e = C.make_blob(f'Eye{side}', c, 1.0, 11, subdivisions=2, noise_strength=0.0, scale=(r, r * 0.55, r))
        M.assign(e, ['M_Leaf'])
        ic = FPAL['eye_cooked'] if cooked else iris
        I.color_fn(e, lambda co: ic)
        p.add(e, 'none')
        if cooked:
            continue
        pc = c + Vector((0.0, side * r * 0.42, 0.0))
        pr = r * pupil_scale
        pu = C.make_blob(f'Pupil{side}', pc, 1.0, 12, subdivisions=2, noise_strength=0.0,
                         scale=(pr, pr * 0.35, pr))
        M.assign(pu, ['M_Leaf'])
        pcol = pupil or FPAL['eye_pupil']
        hl = pc + Vector((pr * 0.35, side * pr * 0.3, pr * 0.4))

        def pf(co, hl=hl, pr=pr, pcol=pcol):
            return (0.92, 0.92, 0.88) if (co - hl).length < pr * 0.32 else pcol
        I.color_fn(pu, pf)
        p.add(pu, 'none')


def paint_body(o, body, fn, rnd, j=0.01, mouth_zn=0.0, mouth_len=0.06, gill_u=0.2, gill_dark=0.25,
               mouth=None):
    """Pinta el cuerpo con fn(u, zn, co) -> rgb y añade la boca (raja
    oscura en el hocico) y el borde del opérculo (arco algo más oscuro)."""
    mcol = mouth or FPAL['mouth']

    def col(co):
        u, zn = body.coords(co)
        c = fn(u, zn, co)
        # opérculo: arco que se abre hacia atrás arriba y abajo
        ug = gill_u + 0.035 * zn * zn
        if gill_dark and abs(u - ug) < 0.012 and abs(zn) < 0.85:
            c = I.lerp3(c, (0.0, 0.0, 0.0), gill_dark)
        if u < mouth_len and abs(zn - mouth_zn) < 0.12 and abs(co.y) < body.at(u)[2] * 0.98:
            c = I.lerp3(c, mcol, 0.85)
        return c
    I.color_fn(o, col, rnd, j)


def _lay_on_side(p):
    """Tumba el pez sobre su costado izquierdo: el flanco +Y queda arriba."""
    p.transform(Matrix.Rotation(math.pi / 2, 4, 'X'))


def _finish(p, name):
    obj = p.finish(name)
    I.smooth_all(obj)
    return obj


def _grad(zn, belly, flank, back, split=0.15, soft=0.35):
    """Degradado de contrasombreado vientre -> flanco -> lomo."""
    if zn < split:
        return I.lerp3(belly, flank, _smoothstep(-0.7, split, zn))
    return I.lerp3(flank, back, _smoothstep(split, split + soft * 2, zn))


# ---------------------------------------------------------------------------
# PECES
# ---------------------------------------------------------------------------
TUNA_PROF = [(0.0, 0.0, 0.0, 0.0), (0.05, 0.055, 0.05, 0.04), (0.16, 0.115, 0.105, 0.085),
             (0.34, 0.14, 0.13, 0.10), (0.55, 0.12, 0.11, 0.085), (0.74, 0.065, 0.06, 0.05),
             (0.88, 0.028, 0.026, 0.028), (0.96, 0.02, 0.018, 0.024)]


def _finlets(p, body, name, u0, u1, n, h, col, side='top'):
    """Pínulas de túnido: aletitas triangulares sueltas entre la segunda
    dorsal/anal y la cola."""
    for i in range(n):
        u = u0 + (u1 - u0) * i / max(1, n - 1)
        median_fin(p, body, f'{name}{i}', u, u + 0.03, [h, h * 0.6, 0.002], col, col, sweep=1.2, side=side,
                   rows=1, ray_dark=0.1)


@_register('atun')
def _b_atun(v, rnd, name):
    """Atún de aleta amarilla (1 m): huso macizo azul marino por arriba y
    plata por abajo con la franja dorada del flanco, segunda dorsal y anal
    en hoz amarillas, pínulas amarillas y cola en media luna."""
    p = K.Parts()
    L = 1.0
    body = FishBody(L, TUNA_PROF, segs=22)
    o = body.build('Body')
    M.assign(o, ['M_Leaf'])
    navy, blue = (0.015, 0.035, 0.13), (0.10, 0.22, 0.45)
    gold = (0.85, 0.60, 0.05)

    def fn(u, zn, co):
        c = _grad(zn, FPAL['belly'], (0.48, 0.56, 0.66), navy, split=0.05, soft=0.25)
        c = I.lerp3(c, blue, 0.35 * _smoothstep(0.3, 0.5, zn) * (1 - _smoothstep(0.6, 0.9, zn)))
        band = 1.0 - _smoothstep(0.05, 0.14, abs(zn - 0.22))
        c = I.lerp3(c, gold, band * 0.85 * _smoothstep(0.1, 0.25, u) * (1 - _smoothstep(0.7, 0.85, u)))
        # barras verticales claras rotas del vientre
        bars = max(0.0, math.sin(u * 60 + I.noise3(co, 20, 1.0) * 2)) ** 6
        return I.lerp3(c, (0.95, 0.95, 0.92), bars * 0.5 * (1 - _smoothstep(-0.4, 0.0, zn)))
    paint_body(o, body, fn, rnd, mouth_zn=-0.1, gill_u=0.24)
    p.add(o, 'none')
    yellow, yel2 = (0.80, 0.55, 0.03), (0.92, 0.72, 0.08)
    median_fin(p, body, 'Dorsal1', 0.30, 0.45, [0.004, 0.07, 0.06, 0.035, 0.01], navy, (0.15, 0.2, 0.3),
               sweep=0.7)
    median_fin(p, body, 'Dorsal2', 0.52, 0.60, [0.01, 0.13, 0.05, 0.01], yellow, yel2, sweep=1.3)
    median_fin(p, body, 'Anal', 0.54, 0.62, [0.01, 0.12, 0.05, 0.01], yellow, yel2, sweep=1.3, side='bot')
    _finlets(p, body, 'FinletT', 0.66, 0.9, 6, 0.018, yel2)
    _finlets(p, body, 'FinletB', 0.66, 0.9, 6, 0.018, yel2, side='bot')
    caudal_fin(p, body, 'Tail', 0.17, 0.2, navy, (0.2, 0.25, 0.35), shape='lunate', notch=0.6, n=11)
    paired_fin(p, body, 'Pect', 0.25, 0.1, 0.24, 0.05, navy, (0.18, 0.25, 0.4), angle=0.25, droop=0.12,
               splay=0.12)
    paired_fin(p, body, 'Pelv', 0.28, -0.75, 0.07, 0.03, (0.6, 0.64, 0.7), (0.3, 0.33, 0.4), angle=0.3,
               droop=0.4, splay=0.3)
    eyes(p, body, 0.1, 0.25, 0.021, (0.32, 0.30, 0.12))
    _lay_on_side(p)
    return _finish(p, name)


@_register('bonito')
def _b_bonito(v, rnd, name):
    """Bonito listado (60 cm): túnido compacto azul violáceo arriba y
    vientre plateado con cuatro franjas oscuras a lo largo."""
    p = K.Parts()
    L = 0.6
    prof = [(u, t * 1.05, b * 1.05, w * 1.05) for (u, t, b, w) in TUNA_PROF]
    body = FishBody(L, prof, segs=22)
    o = body.build('Body')
    M.assign(o, ['M_Leaf'])
    back, violet = (0.03, 0.05, 0.18), (0.20, 0.18, 0.42)

    def fn(u, zn, co):
        c = _grad(zn, (0.88, 0.88, 0.84), (0.62, 0.66, 0.76), back, split=0.1, soft=0.2)
        c = I.lerp3(c, violet, 0.4 * _smoothstep(0.2, 0.4, zn) * (1 - _smoothstep(0.55, 0.8, zn)))
        if zn < 0.05 and 0.12 < u < 0.85:
            s = 0.5 + 0.5 * math.cos((zn + 0.9) * math.pi * 4.3)
            c = I.lerp3(c, (0.08, 0.10, 0.22), _smoothstep(0.7, 0.9, s) * 0.85)
        return c
    paint_body(o, body, fn, rnd, mouth_zn=-0.1, gill_u=0.24)
    p.add(o, 'none')
    dk, dk2 = back, (0.18, 0.22, 0.38)
    median_fin(p, body, 'Dorsal1', 0.28, 0.46, [0.004, 0.08, 0.07, 0.04, 0.01], dk, dk2, sweep=0.6)
    median_fin(p, body, 'Dorsal2', 0.52, 0.59, [0.01, 0.07, 0.03, 0.01], dk, dk2, sweep=1.2)
    median_fin(p, body, 'Anal', 0.54, 0.61, [0.01, 0.065, 0.03, 0.01], (0.7, 0.72, 0.78), dk2, sweep=1.2,
               side='bot')
    _finlets(p, body, 'FinletT', 0.65, 0.88, 5, 0.02, dk2)
    _finlets(p, body, 'FinletB', 0.65, 0.88, 5, 0.02, dk2, side='bot')
    caudal_fin(p, body, 'Tail', 0.16, 0.19, dk, dk2, shape='lunate', notch=0.55, n=11)
    paired_fin(p, body, 'Pect', 0.25, 0.1, 0.13, 0.05, dk, dk2, angle=0.3, droop=0.12, splay=0.12)
    eyes(p, body, 0.1, 0.25, 0.024, (0.35, 0.32, 0.14))
    _lay_on_side(p)
    return _finish(p, name)


@_register('barracuda')
def _b_barracuda(v, rnd, name):
    """Barracuda (95 cm): torpedo largo y fino de hocico puntiagudo con la
    mandíbula inferior saliente, plata con barras oscuras en diagonal en el
    lomo y motas negras hacia la cola; cola en horquilla oscura."""
    p = K.Parts()
    L = 0.95
    prof = [(0.0, 0.0, 0.0, 0.0), (0.04, 0.014, 0.018, 0.014), (0.12, 0.04, 0.042, 0.034),
            (0.3, 0.068, 0.064, 0.05), (0.55, 0.066, 0.062, 0.048), (0.8, 0.04, 0.038, 0.032),
            (0.94, 0.022, 0.02, 0.02)]
    body = FishBody(L, prof, segs=20, snout_z=-0.006)
    o = body.build('Body')
    M.assign(o, ['M_Leaf'])
    back = (0.05, 0.22, 0.30)

    def fn(u, zn, co):
        c = _grad(zn, (0.92, 0.92, 0.86), (0.62, 0.72, 0.76), back, split=0.25, soft=0.3)
        bar = math.sin((u * 22 - zn * 1.2) * math.pi)
        if zn > 0.05 and 0.2 < u < 0.8:
            c = I.lerp3(c, (0.06, 0.10, 0.14), _smoothstep(0.55, 0.8, bar) * 0.8)
        if u > 0.65 and zn < 0.1:
            c = I.lerp3(c, (0.02, 0.02, 0.03), _spots(u, zn, 20, 3, 0.3, 5))
        return c
    paint_body(o, body, fn, rnd, mouth_zn=-0.05, mouth_len=0.1, gill_u=0.2)
    p.add(o, 'none')
    # colmillos: pequeños conos blancos en la raja de la boca
    for i in range(4):
        u = 0.03 + i * 0.018
        top, bot, hw, zc = body.at(u)
        for side in (1, -1):
            b = Vector((body.x(u), side * hw * 0.85, zc - 0.05 * bot))
            t = C.make_blob(f'Tooth{i}{side}', b, 1.0, i, subdivisions=1, noise_strength=0.0,
                            scale=(0.0025, 0.0025, 0.006))
            M.assign(t, ['M_Leaf'])
            I.color_fn(t, lambda co: (0.92, 0.90, 0.82))
            p.add(t, 'none')
    dk, dk2 = (0.08, 0.12, 0.16), (0.18, 0.24, 0.3)
    median_fin(p, body, 'Dorsal1', 0.36, 0.44, [0.005, 0.07, 0.05, 0.01], dk, dk2, sweep=0.7)
    median_fin(p, body, 'Dorsal2', 0.66, 0.72, [0.005, 0.06, 0.03, 0.006], dk, dk2, sweep=0.8)
    median_fin(p, body, 'Anal', 0.68, 0.74, [0.005, 0.055, 0.03, 0.006], dk, dk2, sweep=0.8, side='bot')
    caudal_fin(p, body, 'Tail', 0.14, 0.12, dk, (0.05, 0.07, 0.1), shape='fork', notch=0.5, n=11)
    paired_fin(p, body, 'Pect', 0.24, -0.1, 0.07, 0.03, (0.55, 0.6, 0.6), dk2, angle=0.4, droop=0.2)
    paired_fin(p, body, 'Pelv', 0.42, -0.8, 0.06, 0.025, (0.6, 0.62, 0.62), dk2, angle=0.3, droop=0.4)
    eyes(p, body, 0.13, 0.3, 0.018, (0.62, 0.55, 0.30))
    _lay_on_side(p)
    return _finish(p, name)


@_register('dorado')
def _b_dorado(v, rnd, name):
    """Dorado (95 cm): frente alta y roma de macho, cuerpo comprimido que
    se afila hacia la cola; verde azulado en el lomo, flancos amarillo oro
    con motas azules y la larga aleta dorsal azul de la frente a la cola."""
    p = K.Parts()
    L = 0.95
    prof = [(0.0, 0.0, 0.0, 0.0), (0.02, 0.07, 0.04, 0.03), (0.07, 0.13, 0.08, 0.05),
            (0.18, 0.14, 0.10, 0.06), (0.4, 0.11, 0.095, 0.052), (0.65, 0.07, 0.065, 0.038),
            (0.88, 0.03, 0.03, 0.022), (0.95, 0.022, 0.022, 0.018)]
    body = FishBody(L, prof, segs=22, snout_z=-0.02)
    o = body.build('Body')
    M.assign(o, ['M_Leaf'])
    blue, teal, gold = (0.03, 0.18, 0.42), (0.05, 0.38, 0.30), (0.88, 0.66, 0.05)

    def fn(u, zn, co):
        c = _grad(zn, (0.92, 0.84, 0.36), gold, teal, split=0.3, soft=0.25)
        c = I.lerp3(c, blue, _smoothstep(0.75, 0.98, zn))
        c = I.lerp3(c, (0.10, 0.45, 0.75), _spots(u, zn, 26, 5, 0.22, 3) * 0.9 * _smoothstep(-0.6, 0.0, zn))
        return I.lerp3(c, (0.4, 0.62, 0.12), 0.25 * max(0.0, I.noise3(co, 12, 2.0)))
    paint_body(o, body, fn, rnd, mouth_zn=-0.45, gill_u=0.17, gill_dark=0.15)
    p.add(o, 'none')
    heights = [0.004, 0.045, 0.06, 0.06, 0.055, 0.05, 0.045, 0.04, 0.035, 0.03, 0.02, 0.01]
    median_fin(p, body, 'Dorsal', 0.04, 0.86, heights, blue, (0.08, 0.28, 0.62), sweep=0.5, rows=2,
               ray_dark=0.35)
    median_fin(p, body, 'Anal', 0.48, 0.86, [0.004, 0.04, 0.035, 0.03, 0.025, 0.015, 0.006],
               gold, (0.55, 0.60, 0.25), sweep=0.5, side='bot', rows=2)
    caudal_fin(p, body, 'Tail', 0.2, 0.17, (0.62, 0.62, 0.12), (0.30, 0.45, 0.35), shape='fork', notch=0.62,
               n=11)
    paired_fin(p, body, 'Pect', 0.2, -0.2, 0.11, 0.04, gold, (0.35, 0.55, 0.45), angle=0.35, droop=0.25)
    paired_fin(p, body, 'Pelv', 0.24, -0.85, 0.09, 0.03, (0.9, 0.8, 0.4), (0.6, 0.62, 0.3), angle=0.3,
               droop=0.45)
    eyes(p, body, 0.07, -0.25, 0.024, (0.55, 0.45, 0.12))
    _lay_on_side(p)
    return _finish(p, name)


@_register('jurel')
def _b_jurel(v, rnd, name):
    """Jurel gigante (55 cm): cuerpo alto y comprimido de frente empinada,
    plata oliva con puntitos negros, escudetes a lo largo de la cola y
    aletas amarillo limón con la pectoral en hoz."""
    p = K.Parts()
    L = 0.55
    prof = [(0.0, 0.0, 0.0, 0.0), (0.03, 0.06, 0.04, 0.025), (0.12, 0.16, 0.11, 0.05),
            (0.32, 0.19, 0.17, 0.06), (0.55, 0.14, 0.13, 0.05), (0.78, 0.06, 0.06, 0.03),
            (0.92, 0.025, 0.025, 0.02)]
    body = FishBody(L, prof, segs=22, snout_z=-0.01)
    o = body.build('Body')
    M.assign(o, ['M_Leaf'])
    olive, silver = (0.14, 0.26, 0.20), (0.62, 0.70, 0.66)

    def fn(u, zn, co):
        c = _grad(zn, (0.92, 0.88, 0.74), silver, olive, split=0.2, soft=0.35)
        c = I.lerp3(c, (0.62, 0.52, 0.22), 0.3 * _smoothstep(-0.2, 0.3, zn) * (1 - _smoothstep(0.5, 0.8, zn)))
        c = I.lerp3(c, (0.03, 0.03, 0.02), _spots(u, zn, 30, 8, 0.12, 9) * _smoothstep(-0.2, 0.3, zn))
        # escudetes: línea lateral de la cola, gruesa y más oscura
        if u > 0.7 and abs(zn - 0.05) < 0.22:
            c = I.lerp3(c, (0.30, 0.30, 0.22), 0.7)
        return c
    paint_body(o, body, fn, rnd, mouth_zn=-0.3, gill_u=0.2)
    p.add(o, 'none')
    ye, ye2 = (0.90, 0.66, 0.04), (0.95, 0.82, 0.16)
    median_fin(p, body, 'Dorsal1', 0.3, 0.4, [0.004, 0.05, 0.035, 0.008], olive, (0.4, 0.42, 0.3), sweep=0.6)
    median_fin(p, body, 'Dorsal2', 0.43, 0.8, [0.01, 0.14, 0.07, 0.04, 0.03, 0.02, 0.01], (0.45, 0.45, 0.25),
               (0.2, 0.22, 0.15), sweep=0.7)
    median_fin(p, body, 'Anal', 0.5, 0.8, [0.01, 0.11, 0.05, 0.03, 0.02, 0.01], ye, ye2, sweep=0.7, side='bot')
    caudal_fin(p, body, 'Tail', 0.2, 0.2, ye, ye2, shape='fork', notch=0.62, n=11)
    paired_fin(p, body, 'Pect', 0.22, -0.1, 0.24, 0.04, ye, ye2, angle=0.2, droop=0.18, splay=0.12)
    paired_fin(p, body, 'Pelv', 0.26, -0.85, 0.06, 0.03, ye, ye2, angle=0.3, droop=0.4)
    eyes(p, body, 0.09, 0.2, 0.034, (0.72, 0.62, 0.30))
    _lay_on_side(p)
    return _finish(p, name)


@_register('mero')
def _b_mero(v, rnd, name):
    """Mero (70 cm): pez macizo de bocaza con la mandíbula inferior
    saliente, pardo oliva con manchas claras irregulares y motas oscuras,
    dorsal espinosa y cola redondeada."""
    p = K.Parts()
    L = 0.7
    prof = [(0.0, 0.0, 0.0, 0.0), (0.03, 0.05, 0.07, 0.05), (0.12, 0.12, 0.13, 0.09),
            (0.35, 0.145, 0.14, 0.1), (0.6, 0.12, 0.11, 0.08), (0.82, 0.065, 0.06, 0.045),
            (0.92, 0.055, 0.05, 0.035)]
    body = FishBody(L, prof, segs=22, snout_z=-0.01)
    o = body.build('Body')
    M.assign(o, ['M_Leaf'])
    brown, olive, pale = (0.20, 0.12, 0.05), (0.30, 0.26, 0.10), (0.70, 0.60, 0.40)

    def fn(u, zn, co):
        c = _grad(zn, (0.72, 0.62, 0.44), olive, brown, split=-0.2, soft=0.4)
        blot = I.noise3(co, 9, 4.0) + 0.5 * I.noise3(co, 20, 1.0)
        c = I.lerp3(c, pale, _smoothstep(0.3, 0.5, blot) * 0.7)
        c = I.lerp3(c, (0.09, 0.04, 0.02), _spots(u, zn, 22, 7, 0.14, 4) * 0.9)
        return c
    paint_body(o, body, fn, rnd, mouth_zn=-0.25, mouth_len=0.1, gill_u=0.26, gill_dark=0.35,
               mouth=(0.25, 0.08, 0.06))
    p.add(o, 'none')
    median_fin(p, body, 'Dorsal', 0.22, 0.78, [0.01, 0.05, 0.045, 0.04, 0.035, 0.05, 0.065, 0.05, 0.01],
               brown, (0.35, 0.25, 0.08), sweep=0.3, rim=(0.55, 0.42, 0.12))
    median_fin(p, body, 'Anal', 0.6, 0.8, [0.01, 0.06, 0.06, 0.035, 0.008], brown, (0.35, 0.25, 0.08),
               sweep=0.4, side='bot')
    caudal_fin(p, body, 'Tail', 0.15, 0.11, brown, (0.35, 0.28, 0.1), shape='round', n=11,
               rim=(0.55, 0.42, 0.12))
    paired_fin(p, body, 'Pect', 0.28, -0.15, 0.12, 0.08, olive, (0.55, 0.42, 0.12), angle=0.9, droop=0.15,
               round_tip=True, n=8)
    paired_fin(p, body, 'Pelv', 0.3, -0.85, 0.1, 0.04, brown, (0.45, 0.3, 0.1), angle=0.4, droop=0.5)
    eyes(p, body, 0.1, 0.45, 0.02, (0.55, 0.40, 0.12))
    _lay_on_side(p)
    return _finish(p, name)


@_register('pargo')
def _b_pargo(v, rnd, name):
    """Pargo rojo (50 cm): hocico en punta, lomo alto, rojo coral que se
    aclara a rosa en el vientre, aletas rojas y cola en horquilla suave."""
    p = K.Parts()
    L = 0.5
    prof = [(0.0, 0.0, 0.0, 0.0), (0.04, 0.045, 0.04, 0.03), (0.14, 0.13, 0.1, 0.06),
            (0.36, 0.17, 0.13, 0.068), (0.6, 0.12, 0.1, 0.055), (0.82, 0.05, 0.048, 0.03),
            (0.92, 0.04, 0.038, 0.025)]
    body = FishBody(L, prof, segs=22, snout_z=-0.01)
    o = body.build('Body')
    M.assign(o, ['M_Leaf'])
    red, coral = (0.62, 0.05, 0.03), (0.85, 0.25, 0.18)

    def fn(u, zn, co):
        c = _grad(zn, (0.95, 0.70, 0.62), coral, red, split=-0.1, soft=0.4)
        # escamas: filas finas algo más claras
        sc = math.sin(u * 140 + zn * 12) * math.sin(u * 140 - zn * 12)
        return I.lerp3(c, (0.95, 0.55, 0.45), 0.15 * _smoothstep(0.6, 0.9, sc))
    paint_body(o, body, fn, rnd, mouth_zn=-0.2, gill_u=0.22)
    p.add(o, 'none')
    fr, fr2 = (0.70, 0.10, 0.05), (0.90, 0.30, 0.18)
    median_fin(p, body, 'Dorsal', 0.26, 0.76, [0.01, 0.07, 0.07, 0.06, 0.05, 0.05, 0.06, 0.04, 0.01],
               fr, fr2, sweep=0.4)
    median_fin(p, body, 'Anal', 0.6, 0.78, [0.01, 0.06, 0.05, 0.03, 0.008], fr, fr2, sweep=0.5, side='bot')
    caudal_fin(p, body, 'Tail', 0.17, 0.14, fr, fr2, shape='fork', notch=0.3, n=11)
    paired_fin(p, body, 'Pect', 0.24, -0.25, 0.16, 0.04, (0.95, 0.5, 0.4), fr2, angle=0.3, droop=0.2)
    paired_fin(p, body, 'Pelv', 0.28, -0.85, 0.09, 0.035, fr, fr2, angle=0.3, droop=0.45)
    eyes(p, body, 0.1, 0.35, 0.026, (0.78, 0.18, 0.06))
    _lay_on_side(p)
    return _finish(p, name)


def _reef_body(L):
    prof = [(0.0, 0.0, 0.0, 0.0), (0.04, 0.07, 0.06, 0.04), (0.15, 0.22, 0.18, 0.075),
            (0.4, 0.26, 0.22, 0.08), (0.65, 0.18, 0.15, 0.06), (0.85, 0.07, 0.07, 0.035),
            (0.92, 0.06, 0.06, 0.03)]
    return FishBody(L, prof, segs=22, snout_z=0.0)


def _reef_fish(p, v, rnd, L, cooked=False):
    """Pez de arrecife (sargento): amarillo arriba con cinco barras negras
    y vientre blanco azulado. Con cooked=True, el mismo pez asado: dorado
    tostado con marcas de parrilla y aletas requemadas."""
    body = _reef_body(L)
    o = body.build('Body')
    M.assign(o, ['M_Leaf'])
    yellow, blue = (0.90, 0.72, 0.06), (0.62, 0.72, 0.86)

    def fn(u, zn, co):
        if cooked:
            c = I.lerp3(FPAL['cooked'], FPAL['cooked_hi'], 0.5 + 0.5 * I.noise3(co, 25, 3.0))
            grill = math.sin((u * 9 + zn * 0.8) * math.pi)
            c = I.lerp3(c, FPAL['cooked_char'], _smoothstep(0.8, 0.95, grill) * 0.9)
            return I.lerp3(c, FPAL['cooked_char'], 0.5 * _smoothstep(0.75, 1.0, abs(zn)))
        c = _grad(zn, (0.92, 0.94, 0.95), blue, yellow, split=-0.1, soft=0.35)
        bar = math.sin((u - 0.2) * 5 / 0.7 * math.pi)
        if 0.2 < u < 0.9:
            c = I.lerp3(c, (0.02, 0.02, 0.05), _smoothstep(0.55, 0.75, bar))
        return c
    paint_body(o, body, fn, rnd, mouth_zn=-0.05, gill_u=0.18, gill_dark=0.0 if cooked else 0.2)
    p.add(o, 'none')
    if cooked:
        f0, f1, rim = FPAL['cooked_fin'], FPAL['cooked_char'], FPAL['cooked_char']
    else:
        f0, f1, rim = yellow, (0.95, 0.85, 0.35), None
    median_fin(p, body, 'Dorsal', 0.22, 0.82, [0.01, 0.1, 0.1, 0.09, 0.09, 0.1, 0.07, 0.01], f0, f1, sweep=0.35,
               rim=rim)
    median_fin(p, body, 'Anal', 0.55, 0.82, [0.01, 0.1, 0.08, 0.05, 0.01],
               f0 if cooked else blue, f1 if cooked else (0.85, 0.9, 0.95), sweep=0.4, side='bot', rim=rim)
    caudal_fin(p, body, 'Tail', 0.2, 0.18, f0, f1, shape='fork', notch=0.35, n=11, rim=rim)
    paired_fin(p, body, 'Pect', 0.26, -0.1, 0.18, 0.05, f0 if cooked else (0.95, 0.9, 0.7), f1, angle=0.4,
               droop=0.15)
    paired_fin(p, body, 'Pelv', 0.3, -0.85, 0.12, 0.04, f0, f1, angle=0.3, droop=0.5)
    eyes(p, body, 0.1, 0.3, 0.05, (0.85, 0.72, 0.2), cooked=cooked)
    return body


@_register('arrecife')
def _b_arrecife(v, rnd, name):
    """Pescado de arrecife (26 cm): sargento de cuerpo alto y oval."""
    p = K.Parts()
    _reef_fish(p, v, rnd, 0.26)
    _lay_on_side(p)
    return _finish(p, name)


@_register('asado')
def _b_asado(v, rnd, name):
    """Pescado asado en espeto: el pescado de arrecife (misma malla y
    semilla) dorado y con marcas de brasa, ensartado de la boca a la cola
    en un espeto de 45 cm con la punta quemada."""
    p = K.Parts()
    L = 0.26
    _reef_fish(p, v, rnd, L, cooked=True)
    # espeto a lo largo del eje del pez (X), saliendo por boca y cola
    pts = [(L * 0.5 + 0.1, 0.0, 0.0), (L * 0.5 + 0.02, 0.0, 0.0), (0.0, 0.0, 0.0),
           (-L * 0.5, 0.0, 0.0), (-L * 0.5 - 0.1, 0.0, 0.002)]
    s = I.sweep('Spit', pts, [0.0015, 0.004, 0.0045, 0.0045, 0.005], segs=7)
    M.assign(s, ['M_Wood'])
    x_tip = L * 0.5 + 0.1

    def sc(co):
        c = I.lerp3(FPAL['stick'], (0.52, 0.35, 0.16), 0.5 + 0.5 * I.noise3(co, 90, 2.0))
        return I.lerp3(c, FPAL['stick_burnt'], _smoothstep(x_tip - 0.06, x_tip, co.x))
    I.color_fn(s, sc, rnd, 0.01)
    p.add(s, 'none')
    _lay_on_side(p)
    return _finish(p, name)


@_register('ballesta')
def _b_ballesta(v, rnd, name):
    """Pez ballesta (35 cm): cuerpo romboidal alto con los ojos muy atrás y
    arriba, espina dorsal erguida, dorsal y anal gemelas; arena clara con
    silla negra, líneas azules desde el ojo y labios amarillos."""
    p = K.Parts()
    L = 0.35
    prof = [(0.0, 0.0, 0.0, 0.0), (0.04, 0.05, 0.04, 0.03), (0.2, 0.18, 0.16, 0.07),
            (0.42, 0.26, 0.24, 0.08), (0.65, 0.16, 0.15, 0.06), (0.85, 0.06, 0.06, 0.035),
            (0.92, 0.055, 0.055, 0.03)]
    body = FishBody(L, prof, segs=22, snout_z=0.01)
    o = body.build('Body')
    M.assign(o, ['M_Leaf'])
    sand, black, blue = (0.80, 0.70, 0.48), (0.03, 0.025, 0.03), (0.08, 0.30, 0.75)

    def fn(u, zn, co):
        c = _grad(zn, (0.95, 0.94, 0.88), sand, (0.55, 0.48, 0.32), split=0.0, soft=0.4)
        # silla negra en diagonal del centro hacia el vientre
        sad = abs((u - 0.5) + zn * 0.18)
        c = I.lerp3(c, black, 1.0 - _smoothstep(0.06, 0.09, sad))
        # líneas azules y amarillas del ojo hacia la pectoral
        if 0.1 < u < 0.4:
            ln = math.sin((zn * 3 + u * 6) * math.pi)
            c = I.lerp3(c, blue, _smoothstep(0.8, 0.95, ln))
            c = I.lerp3(c, (0.92, 0.62, 0.05), _smoothstep(0.8, 0.95, -ln) * 0.8)
        if u > 0.72 and abs(zn) < 0.4:
            c = I.lerp3(c, black, 0.7 * _smoothstep(0.5, 0.6, math.sin(u * 60)))
        return c
    paint_body(o, body, fn, rnd, mouth_zn=0.05, mouth_len=0.05, gill_u=0.3, gill_dark=0.0,
               mouth=(0.92, 0.55, 0.05))
    p.add(o, 'none')
    median_fin(p, body, 'Spine', 0.3, 0.36, [0.005, 0.09, 0.02], black, (0.2, 0.18, 0.18), sweep=0.25)
    median_fin(p, body, 'Dorsal', 0.5, 0.82, [0.01, 0.09, 0.07, 0.05, 0.03, 0.01], sand, (0.5, 0.42, 0.25),
               sweep=0.5)
    median_fin(p, body, 'Anal', 0.52, 0.82, [0.01, 0.08, 0.06, 0.045, 0.025, 0.01], sand, (0.5, 0.42, 0.25),
               sweep=0.5, side='bot')
    caudal_fin(p, body, 'Tail', 0.13, 0.12, sand, (0.35, 0.3, 0.2), shape='truncate', n=9,
               rim=(0.2, 0.15, 0.1))
    paired_fin(p, body, 'Pect', 0.38, 0.1, 0.08, 0.04, (0.9, 0.85, 0.7), sand, angle=0.4, droop=0.0)
    eyes(p, body, 0.3, 0.45, 0.045, (0.25, 0.55, 0.2))
    _lay_on_side(p)
    return _finish(p, name)


@_register('cirujano')
def _b_cirujano(v, rnd, name):
    """Pez cirujano azul (30 cm): oval y muy comprimido, azul real con la
    «paleta» negra del flanco, cola amarilla con bordes negros y la
    espina-bisturí amarilla en el pedúnculo."""
    p = K.Parts()
    L = 0.3
    prof = [(0.0, 0.0, 0.0, 0.0), (0.04, 0.06, 0.05, 0.03), (0.16, 0.2, 0.18, 0.06),
            (0.42, 0.25, 0.23, 0.065), (0.68, 0.17, 0.15, 0.05), (0.86, 0.06, 0.06, 0.03),
            (0.92, 0.05, 0.05, 0.025)]
    body = FishBody(L, prof, segs=22, snout_z=0.005)
    o = body.build('Body')
    M.assign(o, ['M_Leaf'])
    royal, black, yellow = (0.03, 0.18, 0.70), (0.01, 0.015, 0.04), (0.92, 0.75, 0.05)

    def fn(u, zn, co):
        c = I.lerp3(royal, (0.25, 0.45, 0.90), _smoothstep(-0.9, -0.3, -zn) * 0.3)
        # paleta: bucle negro del ojo hacia la cola, con un «hueco» azul
        d_upper = abs(zn - (0.55 - 0.2 * u)) < 0.18 and 0.12 < u < 0.85
        loop = abs(zn - (0.15 + 0.55 * math.sin((u - 0.3) / 0.45 * math.pi))) < 0.12 and 0.3 < u < 0.75
        if d_upper or loop:
            c = black
        if u > 0.84 and abs(zn) < 0.6:
            c = yellow
        return c
    paint_body(o, body, fn, rnd, mouth_zn=0.05, mouth_len=0.04, gill_u=0.18, gill_dark=0.2)
    p.add(o, 'none')
    median_fin(p, body, 'Dorsal', 0.18, 0.84, [0.01, 0.07, 0.08, 0.08, 0.08, 0.07, 0.05, 0.01],
               royal, (0.02, 0.06, 0.25), sweep=0.35, rim=black)
    median_fin(p, body, 'Anal', 0.45, 0.84, [0.01, 0.07, 0.07, 0.06, 0.04, 0.01], royal, (0.02, 0.06, 0.25),
               sweep=0.4, side='bot', rim=black)
    caudal_fin(p, body, 'Tail', 0.16, 0.16, yellow, (0.95, 0.82, 0.2), shape='lunate', notch=0.35, n=11,
               ray_dark=0.1, rim=black)
    paired_fin(p, body, 'Pect', 0.24, 0.0, 0.14, 0.05, yellow, (0.95, 0.85, 0.3), angle=0.4, droop=0.1)
    eyes(p, body, 0.12, 0.3, 0.05, (0.05, 0.08, 0.25))
    _lay_on_side(p)
    return _finish(p, name)


@_register('loro')
def _b_loro(v, rnd, name):
    """Pez loro (42 cm): cuerpo robusto de escamas grandes turquesa con el
    borde rosa, pico de dientes soldados verde agua y cola en media luna
    con los lóbulos alargados."""
    p = K.Parts()
    L = 0.42
    prof = [(0.0, 0.0, 0.0, 0.0), (0.03, 0.07, 0.05, 0.04), (0.14, 0.15, 0.12, 0.07),
            (0.38, 0.17, 0.15, 0.08), (0.64, 0.13, 0.12, 0.065), (0.84, 0.06, 0.06, 0.035),
            (0.92, 0.05, 0.05, 0.03)]
    body = FishBody(L, prof, segs=22, snout_z=-0.005)
    o = body.build('Body')
    M.assign(o, ['M_Leaf'])
    teal, green, pink = (0.05, 0.50, 0.42), (0.10, 0.42, 0.18), (0.85, 0.35, 0.45)

    def fn(u, zn, co):
        c = _grad(zn, (0.25, 0.65, 0.60), teal, green, split=0.0, soft=0.4)
        # escamas grandes: celda hexagonal con el borde rosa
        gu, gv = u * 18, zn * 4.5 + 0.5 * (int(u * 18) % 2)
        du, dv = abs((gu % 1.0) - 0.5), abs((gv % 1.0) - 0.5)
        edge = max(du * 1.1, dv)
        if 0.1 < u < 0.86:
            c = I.lerp3(c, pink, _smoothstep(0.38, 0.47, edge) * 0.85)
        # cara: bandas rosas y verdes alrededor del pico
        if u < 0.12:
            c = I.lerp3(c, pink, 0.6 * _smoothstep(0.4, 0.8, math.sin(zn * 9)))
        return c
    paint_body(o, body, fn, rnd, mouth_zn=0.0, mouth_len=0.03, gill_u=0.17, gill_dark=0.0)
    p.add(o, 'none')
    # pico: dos placas redondeadas en la punta del hocico
    for i, zs in enumerate((0.012, -0.012)):
        c = Vector((L / 2 - 0.004, 0.0, zs * L / 0.42))
        b = C.make_blob(f'Beak{i}', c, 1.0, 20 + i, subdivisions=2, noise_strength=0.0,
                        scale=(0.014, 0.016, 0.009))
        M.assign(b, ['M_Leaf'])
        I.color_fn(b, lambda co: (0.55, 0.85, 0.78))
        p.add(b, 'none')
    median_fin(p, body, 'Dorsal', 0.2, 0.84, [0.01, 0.05, 0.05, 0.05, 0.05, 0.05, 0.05, 0.01],
               pink, (0.10, 0.45, 0.62), sweep=0.35, rim=(0.1, 0.35, 0.75))
    median_fin(p, body, 'Anal', 0.6, 0.84, [0.01, 0.05, 0.05, 0.04, 0.01], pink, (0.10, 0.45, 0.62),
               sweep=0.4, side='bot', rim=(0.1, 0.35, 0.75))
    caudal_fin(p, body, 'Tail', 0.2, 0.18, teal, pink, shape='lunate', notch=0.6, n=11, ray_dark=0.15,
               rim=(0.1, 0.35, 0.75))
    paired_fin(p, body, 'Pect', 0.24, -0.1, 0.13, 0.05, (0.9, 0.55, 0.6), (0.2, 0.6, 0.7), angle=0.4,
               droop=0.1)
    paired_fin(p, body, 'Pelv', 0.3, -0.85, 0.08, 0.03, pink, (0.2, 0.6, 0.7), angle=0.3, droop=0.5)
    eyes(p, body, 0.1, 0.35, 0.03, (0.85, 0.55, 0.15))
    _lay_on_side(p)
    return _finish(p, name)


@_register('salmonete')
def _b_salmonete(v, rnd, name):
    """Salmonete de arrecife (26 cm): alargado, vientre plano y frente
    inclinada, rosa rojizo con la franja amarilla a lo largo y las dos
    barbillas amarillas bajo el mentón; dos dorsales separadas."""
    p = K.Parts()
    L = 0.26
    prof = [(0.0, 0.0, 0.0, 0.0), (0.04, 0.05, 0.03, 0.03), (0.16, 0.12, 0.08, 0.065),
            (0.4, 0.12, 0.09, 0.07), (0.66, 0.085, 0.07, 0.05), (0.86, 0.042, 0.04, 0.03),
            (0.92, 0.036, 0.035, 0.026)]
    body = FishBody(L, prof, segs=20, snout_z=-0.02)
    o = body.build('Body')
    M.assign(o, ['M_Leaf'])
    red, pink, yellow = (0.72, 0.12, 0.10), (0.95, 0.55, 0.50), (0.95, 0.72, 0.08)

    def fn(u, zn, co):
        c = _grad(zn, (0.98, 0.85, 0.78), pink, red, split=-0.1, soft=0.35)
        band = 1.0 - _smoothstep(0.08, 0.16, abs(zn - 0.05))
        return I.lerp3(c, yellow, band * _smoothstep(0.1, 0.2, u) * 0.9)
    paint_body(o, body, fn, rnd, mouth_zn=-0.4, mouth_len=0.05, gill_u=0.2)
    p.add(o, 'none')
    # barbillas bajo el mentón, tendidas hacia atrás
    for side in (1, -1):
        top, bot, hw, zc = body.at(0.05)
        a = Vector((body.x(0.05), side * hw * 0.4, zc - bot * 0.9))
        pts = [a, a + Vector((-0.02, side * 0.004, -0.004)), a + Vector((-0.04, side * 0.008, -0.004))]
        b = I.sweep(f'Barbel{side}', pts, [0.0018, 0.0014, 0.0008], segs=5)
        M.assign(b, ['M_Leaf'])
        I.color_fn(b, lambda co: yellow)
        p.add(b, 'none')
    f0, f1 = (0.85, 0.45, 0.35), (0.95, 0.75, 0.40)
    median_fin(p, body, 'Dorsal1', 0.28, 0.4, [0.01, 0.1, 0.08, 0.04, 0.01], f0, f1, sweep=0.5)
    median_fin(p, body, 'Dorsal2', 0.56, 0.68, [0.01, 0.06, 0.04, 0.01], f0, f1, sweep=0.5)
    median_fin(p, body, 'Anal', 0.58, 0.7, [0.01, 0.06, 0.04, 0.01], f0, f1, sweep=0.5, side='bot')
    caudal_fin(p, body, 'Tail', 0.2, 0.14, red, (0.9, 0.45, 0.35), shape='fork', notch=0.55, n=11)
    paired_fin(p, body, 'Pect', 0.24, -0.2, 0.14, 0.04, f0, f1, angle=0.3, droop=0.2)
    paired_fin(p, body, 'Pelv', 0.28, -0.85, 0.1, 0.03, f0, f1, angle=0.3, droop=0.5)
    eyes(p, body, 0.12, 0.45, 0.045, (0.85, 0.35, 0.12))
    _lay_on_side(p)
    return _finish(p, name)


# ---------------------------------------------------------------------------
# PREPARADOS
# ---------------------------------------------------------------------------
@_register('filete')
def _b_filete(v, rnd, name):
    """Filete de pescado (22 cm): lomo ahusado de carne rosada con las
    bandas en V blancas del músculo y la piel plateada por debajo."""
    p = K.Parts()
    L, W, T = 0.22, 0.075, 0.024
    xs = [-L / 2 + L * i / 24 for i in range(25)]

    def section(x):
        t = (x + L / 2) / L  # 0 cola .. 1 cabeza
        w = W * 0.5 * (0.22 + 0.78 * math.sin(min(1.0, t * 1.05 + 0.05) * math.pi * 0.62) ** 0.8)
        th = T * (0.25 + 0.75 * math.sin(min(1.0, t + 0.05) * math.pi * 0.6))
        pts = []
        n = 16
        for k in range(n):
            a = 2 * math.pi * k / n
            s, c = math.sin(a), math.cos(a)
            # canto superior redondeado, cara inferior casi plana
            z = th * (0.55 * s + 0.45) if s > 0 else th * 0.45 * (1 + s * 0.3)
            pts.append((w * c * (1 - 0.1 * s), z))
        return pts
    o = I.loft_x('Fillet', xs, section)
    M.assign(o, ['M_Leaf'])
    flesh, stripe, skin = (0.90, 0.46, 0.36), (0.97, 0.84, 0.78), (0.62, 0.70, 0.76)

    def col(co):
        if co.z < T * 0.12:
            return I.lerp3(skin, (0.30, 0.38, 0.48), 0.4 * _smoothstep(0.0, 1.0, abs(co.y) / (W / 2)))
        chevron = math.sin((co.x * 55 + abs(co.y) * 60) * math.pi / 2)
        c = I.lerp3(flesh, stripe, _smoothstep(0.75, 0.95, chevron) * 0.9)
        return I.lerp3(c, (0.80, 0.30, 0.25), 0.3 * _smoothstep(0.004, 0.0, abs(co.y)))
    I.color_fn(o, col, rnd, 0.01)
    p.add(o, 'none')
    return _finish(p, name)


@_register('abierto')
def _b_abierto(v, rnd, name):
    """Pescado abierto en mariposa para curar (27 cm): las dos mitades
    tendidas con la carne hacia arriba, la cabeza entera y la cola. La
    misma malla sirve para el ahumado (ámbar tostado, bordes oscuros) y el
    salado (carne crema con costra de sal)."""
    p = K.Parts()
    cure = v.get('cure', 'ahumado')
    L = 0.27
    # silueta en planta: dos lóbulos (mitades) unidos por el lomo central
    xs = [L * 0.2 - L * 0.72 * i / 24 for i in range(25)]  # de la cabeza (+X) a la cola

    def half_w(x):
        t = (L * 0.2 - x) / (L * 0.72)  # 0 junto a la cabeza .. 1 en la cola
        # lóbulos de las dos mitades y cuello ancho que empalma con la cabeza
        lobe = 0.07 * math.sin(min(1.0, t * 1.02 + 0.06) * math.pi) ** 0.7 * (1 - 0.45 * t)
        return 0.012 + lobe + 0.018 * (1 - t) ** 3

    def section(x):
        w = half_w(x)
        n = 18
        pts = []
        for k in range(n):
            a = 2 * math.pi * k / n
            s, c = math.sin(a), math.cos(a)
            # carne arriba algo cóncava a lo largo del espinazo, piel abajo
            if s >= 0:
                z = 0.009 + 0.005 * s - 0.004 * (1 - abs(c))
            else:
                z = 0.006 * (1 + s)
            pts.append((w * c, z))
        return pts
    o = I.loft_x('Split', xs, section)
    M.assign(o, ['M_Wood'])
    if cure == 'salado':
        flesh, edge, skin = (0.92, 0.74, 0.50), (0.72, 0.48, 0.24), (0.50, 0.56, 0.62)
    else:
        flesh, edge, skin = (0.72, 0.36, 0.08), (0.28, 0.10, 0.03), (0.35, 0.22, 0.10)

    def col(co):
        w = half_w(co.x)
        r = abs(co.y) / max(w, 1e-4)
        if co.z < 0.004:
            return skin
        c = I.lerp3(flesh, edge, _smoothstep(0.65, 1.0, r))
        # espinazo y costillas marcadas
        if abs(co.y) < 0.004:
            c = I.lerp3(c, edge, 0.6)
        rib = math.sin(co.x * 180)
        c = I.lerp3(c, edge, 0.25 * _smoothstep(0.7, 0.95, rib) * (1 - r))
        if cure == 'salado':
            salt = _smoothstep(0.45, 0.6, I.noise3(co, 260, 7.0))
            c = I.lerp3(c, (0.98, 0.97, 0.93), salt * 0.75)
        else:
            c = I.lerp3(c, (0.85, 0.55, 0.16), 0.3 * max(0.0, I.noise3(co, 60, 2.0)))
        return c
    I.color_fn(o, col, rnd, 0.01)
    p.add(o, 'none')
    # cabeza: media gota cerrada al frente del lomo
    hx = L * 0.2
    head = C.make_blob('Head', (hx + 0.018, 0.0, 0.009), 1.0, v['seed'], subdivisions=3, noise_strength=0.0,
                       scale=(0.036, 0.03, 0.011))
    for vv in head.data.vertices:
        # cabeza en cuña: se estrecha hacia el hocico (+X)
        f = max(0.0, (vv.co.x - hx) / 0.055)
        vv.co.y *= 1.0 - 0.45 * f
        vv.co.z = 0.009 + (vv.co.z - 0.009) * (1.0 - 0.3 * f)
    head.data.update()
    M.assign(head, ['M_Wood'])
    hc = skin if cure == 'ahumado' else (0.74, 0.64, 0.46)
    I.color_fn(head, lambda co: I.lerp3(hc, flesh if cure == 'ahumado' else (0.62, 0.66, 0.70),
                                        0.3 * max(0.0, I.noise3(co, 80, 1.0))), rnd, 0.01)
    p.add(head, 'none')
    for side in (1, -1):
        e = C.make_blob(f'Eye{side}', (hx + 0.026, side * 0.016, 0.0125), 0.0042, 30, subdivisions=2,
                        noise_strength=0.0, scale=(1.0, 0.6, 1.0))
        M.assign(e, ['M_Wood'])
        I.color_fn(e, lambda co: FPAL['eye_cooked'] if cure == 'ahumado' else (0.9, 0.9, 0.86))
        p.add(e, 'none')
    # cola tendida plana
    tx = xs[-1]
    base = [Vector((tx + 0.004, s * 0.012, 0.006)) for s in (-1, -0.5, 0, 0.5, 1)]
    tips = [Vector((tx - 0.05 * (1 - 0.3 * (1 - abs(s))), s * 0.035, 0.004)) for s in (-1, -0.5, 0, 0.5, 1)]
    t = _fin(p, 'Tail', base, tips, edge, skin if cure == 'ahumado' else (0.72, 0.66, 0.5), rows=2,
             fold=(0.0015, Vector((0, 0, 1))), thick=0.003)
    M.assign(t, ['M_Wood'])
    return _finish(p, name)


# ---------------------------------------------------------------------------
# MARISCOS
# ---------------------------------------------------------------------------
def _limb(p, name, pts, radii, rgb0, rgb1, rnd, segs=6, mat='M_Leaf'):
    o = I.sweep(name, pts, radii, segs=segs)
    M.assign(o, [mat])
    n = len(pts)
    # degradado base -> punta por anillo (índice de vértice = anillo * segs)

    def f(vv):
        ring = min(n - 1, vv.index // segs)
        return tuple(I.lerp3(rgb0, rgb1, ring / (n - 1))) + (0.0,)
    C.set_vertex_colors(o, f)
    p.add(o, 'none')
    return o


@_register('cangrejo')
def _b_cangrejo(v, rnd, name):
    """Cangrejo de roca (13 cm de pata a pata): caparazón ancho y chato
    rojo anaranjado con motas claras, ocho patas articuladas en rodilla y
    dos pinzas gruesas, ojos en pedúnculo; vientre azul pálido."""
    p = K.Parts()
    # crab_pal/char: paleta cocida del cangrejo asado (items_cocina.py), misma malla
    red, orange, belly = v.get('crab_pal', ((0.72, 0.12, 0.04), (0.92, 0.42, 0.08), (0.45, 0.62, 0.78)))
    char = v.get('char', 0.0)
    W, D, H = 0.075, 0.06, 0.022
    cz = 0.022
    shell = C.make_blob('Shell', (0, 0, 0), 1.0, v['seed'], subdivisions=4, noise_strength=0.0)
    for vv in shell.data.vertices:
        x, y, z = vv.co
        # frente recta y ancha (+X), trasera redondeada; tapa abombada
        sx = W / 2 * (1.0 + 0.12 * x)
        vv.co = Vector((x * D / 2, y * sx, (z * H if z > 0 else z * H * 0.55) + cz))
    shell.data.update()
    M.assign(shell, ['M_Leaf'])

    def sc(co):
        if co.z < cz:
            return belly
        c = I.lerp3(red, orange, _smoothstep(-0.2, 0.6, I.noise3(co, 45, 3.0)))
        c = I.lerp3(c, (0.98, 0.78, 0.45), _spots(co.x * 10, co.y * 10, 6, 6, 0.12, 2) * 0.7)
        if char:
            c = I.lerp3(c, FPAL['cooked_char'], _smoothstep(0.25, 0.55, I.noise3(co, 60, 9.0)) * char)
        return I.lerp3(c, (0.35, 0.03, 0.02), _smoothstep(0.85, 1.0, abs(co.y) / (W / 2)))
    I.color_fn(shell, sc, rnd, 0.01)
    p.add(shell, 'none')
    # patas: cuatro por lado, de la base del caparazón hacia fuera y abajo
    for side in (1, -1):
        for i in range(4):
            x = D * (0.22 - i * 0.16)
            root = Vector((x, side * W * 0.45, cz - 0.003))
            # patas delanteras algo hacia delante, traseras hacia atrás
            out = Vector((0.35 - i * 0.28, side * 1.0, 0.0)).normalized()
            knee = root + out * 0.03 + Vector((0, 0, 0.008))
            foot = knee + out * 0.03 + Vector((0, 0, -knee.z + 0.001))
            pts = [root, root.lerp(knee, 0.5) + Vector((0, 0, 0.003)), knee, knee.lerp(foot, 0.5), foot]
            _limb(p, f'Leg{side}{i}', pts, [0.0045, 0.0042, 0.0038, 0.003, 0.0012], red, orange, rnd)
        # pinza: brazo + palma + dos dedos
        root = Vector((D * 0.42, side * W * 0.28, cz))
        elbow = root + Vector((0.02, side * 0.02, 0.004))
        wrist = elbow + Vector((0.022, -side * 0.004, -0.004))
        _limb(p, f'Arm{side}', [root, elbow, wrist], [0.005, 0.0055, 0.006], red, red, rnd)
        palm = C.make_blob(f'Palm{side}', wrist + Vector((0.012, 0, 0)), 1.0, 40, subdivisions=3,
                           noise_strength=0.0, scale=(0.017, 0.009, 0.011))
        M.assign(palm, ['M_Leaf'])
        I.color_fn(palm, lambda co: I.lerp3(red, orange, 0.4), rnd, 0.01)
        p.add(palm, 'none')
        tip0 = wrist + Vector((0.026, 0, 0))
        for k, dz in enumerate((0.004, -0.004)):
            f0 = tip0 + Vector((0.0, 0.0, dz))
            f1 = f0 + Vector((0.016, -side * 0.002, -dz * 0.6))
            _limb(p, f'Finger{side}{k}', [f0, f0.lerp(f1, 0.5), f1], [0.0038, 0.003, 0.0008],
                  orange, (0.98, 0.85, 0.6), rnd)
        # ojo en pedúnculo
        st = Vector((D * 0.48, side * 0.012, cz + H * 0.6))
        stalk_end = st + Vector((0.004, side * 0.002, 0.009))
        _limb(p, f'Stalk{side}', [st, stalk_end], [0.0022, 0.0022], red, red, rnd)
        e = C.make_blob(f'Eye{side}', stalk_end + Vector((0, 0, 0.002)), 0.0035, 50, subdivisions=2,
                        noise_strength=0.0)
        M.assign(e, ['M_Leaf'])
        I.color_fn(e, lambda co: FPAL['eye_pupil'])
        p.add(e, 'none')
    return _finish(p, name)


@_register('langosta')
def _b_langosta(v, rnd, name):
    """Langosta de arrecife pintada (32 cm de cuerpo + antenas): cefalotórax
    cilíndrico con espinas y dos cuernos, abdomen de seis segmentos con
    bandas blancas y negras, abanico de cola, cinco pares de patas finas y
    antenas largas espinosas; verde azulado con antenas rosas."""
    p = K.Parts()
    green, teal, white, black = (0.10, 0.40, 0.30), (0.12, 0.45, 0.58), (0.92, 0.90, 0.82), (0.03, 0.04, 0.05)
    pink = (0.85, 0.40, 0.50)
    Lc, Rc = 0.12, 0.038
    zc = 0.034
    # cefalotórax: loft a lo largo de X (frente en +X)
    xs = [Lc * i / 12 for i in range(13)]

    def sec(x):
        t = x / Lc
        r = Rc * (0.75 + 0.25 * math.sin(min(1.0, t * 1.1) * math.pi)) * (1 - 0.25 * t ** 3)
        return [(r * math.cos(2 * math.pi * k / 16) * 1.05, zc + r * math.sin(2 * math.pi * k / 16) * 0.9)
                for k in range(16)]
    ct = I.loft_x('Carapace', xs, sec)
    M.assign(ct, ['M_Leaf'])

    def cc(co):
        c = I.lerp3(teal, green, _smoothstep(-0.3, 0.5, I.noise3(co, 50, 1.0)))
        c = I.lerp3(c, (0.55, 0.75, 0.70), _spots(co.x * 12, co.y * 12 + co.z * 12, 5, 5, 0.15, 3) * 0.6)
        return I.lerp3(c, (0.95, 0.85, 0.55), 0.6 * _smoothstep(-0.5, -0.9, (co.z - zc) / Rc))
    I.color_fn(ct, cc, rnd, 0.01)
    p.add(ct, 'none')
    # espinas del caparazón
    for i in range(10):
        a = rnd.uniform(0.3, math.pi - 0.3)
        x = rnd.uniform(0.02, Lc - 0.02)
        base = Vector((x, math.cos(a) * Rc * 0.95, zc + math.sin(a) * Rc * 0.85))
        d = (base - Vector((x, 0, zc))).normalized() + Vector((0.4, 0, 0))
        sp = I.sweep(f'Spine{i}', [base - d * 0.002, base + d.normalized() * 0.007], [0.0022, 0.0003], segs=4)
        M.assign(sp, ['M_Leaf'])
        I.color_fn(sp, lambda co: white)
        p.add(sp, 'none')
    # cuernos sobre los ojos
    for side in (1, -1):
        b = Vector((Lc - 0.006, side * 0.012, zc + Rc * 0.55))
        _limb(p, f'Horn{side}', [b, b + Vector((0.012, side * 0.003, 0.012)), b + Vector((0.02, side * 0.004, 0.018))],
              [0.004, 0.0028, 0.0006], green, (0.9, 0.85, 0.7), rnd, segs=5)
        e = C.make_blob(f'Eye{side}', b + Vector((0.004, side * 0.009, -0.006)), 0.0045, 50 + side,
                        subdivisions=2, noise_strength=0.0)
        M.assign(e, ['M_Leaf'])
        I.color_fn(e, lambda co: FPAL['eye_pupil'])
        p.add(e, 'none')
        # antenas: base gruesa rosa y látigo largo que se curva hacia atrás
        a0 = Vector((Lc + 0.004, side * 0.02, zc + 0.006))
        pts = [a0]
        for k in range(1, 9):
            t = k / 8
            pts.append(a0 + Vector((0.15 * math.sin(t * 1.4) / 1.4 + 0.02 * t, side * (0.03 * t + 0.07 * t * t),
                                    0.02 * t - 0.03 * t * t)))
        radii = [0.006 * (1 - t / 9) ** 1.6 + 0.0008 for t in range(9)]
        _limb(p, f'Antenna{side}', pts, radii, pink, (0.55, 0.22, 0.30), rnd, segs=6)
        # antenulas finas bicolor
        b2 = Vector((Lc + 0.004, side * 0.006, zc - 0.004))
        _limb(p, f'Antennule{side}', [b2, b2 + Vector((0.03, side * 0.004, 0.004)),
                                     b2 + Vector((0.055, side * 0.012, 0.0))],
              [0.0016, 0.0012, 0.0005], white, black, rnd, segs=4)
        # patas: cinco por lado, finas y articuladas, apoyadas en el suelo
        for i in range(5):
            x = Lc * (0.85 - i * 0.16)
            root = Vector((x, side * Rc * 0.7, zc - Rc * 0.55))
            knee = root + Vector((0.01 - i * 0.006, side * 0.03, 0.008))
            foot = knee + Vector((0.012 - i * 0.008, side * 0.02, -knee.z + 0.0015))
            _limb(p, f'Leg{side}{i}', [root, knee, foot], [0.0032, 0.0028, 0.001],
                  (0.25, 0.45, 0.55), (0.92, 0.80, 0.55), rnd, segs=5)
    # abdomen: seis segmentos tejados hacia -X, que se afinan
    x = 0.004
    for i in range(6):
        r = Rc * (0.9 - i * 0.07)
        seg_l = 0.026 - i * 0.001
        xs2 = [x - seg_l * k / 5 for k in range(6)]

        def sec2(xx, x0=x, r=r, seg_l=seg_l):
            t = (x0 - xx) / seg_l
            rr = r * (1.0 - 0.1 * t)
            return [(rr * math.cos(2 * math.pi * k / 16) * 1.3,
                     zc - Rc * 0.1 + rr * max(-0.35, math.sin(2 * math.pi * k / 16)) * 0.72) for k in range(16)]
        sg = I.loft_x(f'Seg{i}', xs2, sec2)
        M.assign(sg, ['M_Leaf'])

        def segc(co, x0=x, seg_l=seg_l):
            t = (x0 - co.x) / seg_l
            if t < 0.18:
                return white
            if t < 0.3:
                return black
            c = I.lerp3(green, teal, 0.5 + 0.5 * I.noise3(co, 60, 4.0))
            return I.lerp3(c, (0.85, 0.80, 0.55), _smoothstep(0.0, -0.8, (co.z - zc) / Rc))
        I.color_fn(sg, segc, rnd, 0.01)
        p.add(sg, 'none')
        x -= seg_l * 0.82
    # abanico de cola: cinco palas
    tail_x = x + 0.004
    for k in range(5):
        a = math.radians(-50 + k * 25)
        root = Vector((tail_x, 0.0, zc - Rc * 0.2))
        d = Vector((-math.cos(a), math.sin(a), 0.0))
        side_v = Vector((-d.y, d.x, 0.0))
        base = [root + side_v * s * 0.006 for s in (-1, 0, 1)]
        tips = [root + d * 0.045 + side_v * s * 0.013 + Vector((0, 0, -0.012)) for s in (-1, 0, 1)]
        _fin(p, f'Fan{k}', base, tips, green, (0.95, 0.75, 0.35), rows=2, thick=0.003, ray_dark=0.1)
    return _finish(p, name)


@_register('lapa')
def _b_lapa(v, rnd, name):
    """Lapa (5,5 cm): cono bajo de costillas radiales con el ápice algo
    adelantado, a franjas crema y pardo; por debajo asoma el pie naranja."""
    p = K.Parts()
    R, H = 0.0275, 0.018
    prof = [(0.0, H), (R * 0.15, H * 0.94), (R * 0.45, H * 0.7), (R * 0.75, H * 0.38), (R, 0.003), (R * 0.97, 0.0),
            (R * 0.8, 0.0012), (0.0, 0.004)]
    o = I.lathe('Shell', I.smooth_profile(prof, 2), segs=40)
    for vv in o.data.vertices:
        a = math.atan2(vv.co.y, vv.co.x)
        rr = math.hypot(vv.co.x, vv.co.y)
        k = 1.0 + 0.06 * math.cos(a * 14) * (rr / R)
        vv.co.x *= k
        vv.co.y *= k
        # ápice adelantado a +X
        vv.co.x += 0.006 * (vv.co.z / H)
    o.data.update()
    M.assign(o, ['M_Stone'])
    cream, brown, dark = (0.86, 0.74, 0.52), (0.36, 0.18, 0.07), (0.16, 0.07, 0.03)

    def col(co):
        a = math.atan2(co.y, co.x)
        rib = math.cos(a * 14)
        c = I.lerp3(brown, cream, _smoothstep(0.1, 0.8, rib))
        c = I.lerp3(c, dark, 0.3 * _smoothstep(0.004, 0.0, co.z))
        return I.lerp3(c, (0.95, 0.9, 0.78), _smoothstep(H * 0.8, H, co.z))
    I.color_fn(o, col, rnd, 0.01)
    p.add(o, 'none')
    foot = C.make_blob('Foot', (0.0, 0.0, 0.0022), 1.0, 60, subdivisions=3, noise_strength=0.0,
                       scale=(R * 0.8, R * 0.78, 0.0022))
    M.assign(foot, ['M_Leaf'])
    I.color_fn(foot, lambda co: (0.90, 0.48, 0.10), rnd, 0.02)
    p.add(foot, 'none')
    return _finish(p, name)


@_register('erizo')
def _b_erizo(v, rnd, name):
    """Erizo de mar (8 cm de caparazón, 15 con púas): caparazón chato
    violeta oscuro con las filas de tubérculos marcadas y púas cónicas
    morado negro con la punta violeta clara."""
    p = K.Parts()
    R, Hs = 0.04, 0.026
    test = C.make_blob('Test', (0, 0, 0), 1.0, v['seed'], subdivisions=4, noise_strength=0.0)
    for vv in test.data.vertices:
        x, y, z = vv.co
        a = math.atan2(y, x)
        k = 1.0 + 0.04 * math.cos(5 * a)  # simetría pentarradial
        vv.co = Vector((x * R * k, y * R * k, (z * Hs if z > 0 else z * Hs * 0.55) + Hs * 0.55))
    test.data.update()
    M.assign(test, ['M_Leaf'])
    violet, dark, tip = (0.18, 0.04, 0.22), (0.05, 0.01, 0.07), (0.62, 0.40, 0.78)

    def tc(co):
        a = math.atan2(co.y, co.x)
        c = I.lerp3(dark, violet, _smoothstep(0.3, 0.9, math.cos(5 * a)))
        return I.lerp3(c, (0.55, 0.30, 0.20), _smoothstep(Hs * 1.35, Hs * 1.55, co.z))
    I.color_fn(test, tc, rnd, 0.01)
    p.add(test, 'none')
    # púas repartidas en espiral de Fibonacci por el hemisferio superior
    n = 70
    ga = math.pi * (3 - math.sqrt(5))
    for i in range(n):
        zf = 1 - (i + 0.5) / n * 1.15
        if zf < -0.2:
            continue
        rr = math.sqrt(max(0.0, 1 - zf * zf))
        a = i * ga
        d = Vector((math.cos(a) * rr, math.sin(a) * rr, zf))
        base = Vector((d.x * R * 0.95, d.y * R * 0.95, (d.z * Hs if d.z > 0 else d.z * Hs * 0.55) + Hs * 0.55))
        ln = rnd.uniform(0.028, 0.04) * (0.7 if zf < 0.1 else 1.0)
        dd = (d + Vector((0, 0, 0.25))).normalized()
        sp = I.sweep(f'Spine{i}', [base - dd * 0.002, base + dd * ln * 0.5, base + dd * ln],
                     [0.0028, 0.0016, 0.0003], segs=5)
        M.assign(sp, ['M_Leaf'])

        def spc(vv, base=base, ln=ln):
            t = min(1.0, (vv.co - base).length / ln)
            return tuple(I.lerp3(dark, tip, _smoothstep(0.4, 1.0, t))) + (0.0,)
        C.set_vertex_colors(sp, spc)
        p.add(sp, 'none')
    return _finish(p, name)
