"""
kit_construccion.py — kit de construcción MODULAR de la base del jugador en
los 4 materiales de progresión (hoja de palma → bambú → madera → piedra).

12 piezas por material (48 mallas): cimiento/pilote, suelo, pared, pared con
puerta, pared con ventana, media pared, tejado a un agua, tejado a dos
aguas, esquina de tejado, escalera, puerta y barandilla. Todas comparten la
misma rejilla (módulo de 2 m, ver docs/art/kit-construccion.md) para que en
Unreal encajen entre sí y entre materiales: una pared de piedra se apoya en
un suelo de bambú, un tejado de palma sobre paredes de madera, etc.

Convenciones de pivote (todas en la BASE de la pieza, escala real en m):
    - Cimiento/pilote: centro de su huella en z=0; se coloca en las
      ESQUINAS de la celda. Su cara superior queda a FOUND_H.
    - Suelo: centro de la celda en z=0; cara superior a FLOOR_T.
    - Paredes/barandilla: centro del lado de la celda (la pared va a lo
      largo de X, centrada en su grosor sobre y=0), z=0 = cara superior del
      suelo.
    - Tejados: celda centrada en x/y=0, z=0 = coronación de la pared; el
      alero sobresale ROOF_OVERHANG por el lado bajo (-Y) y baja por debajo
      de z=0 (ver doc).
    - Escalera: centro del primer peldaño en z=0, sube hacia +Y.
    - Puerta: bisagra en x=0 (la hoja se abre desde ahí hacia +X), z=0.

El estilo es el de la revisión de arte del resto del kit: low-poly suave,
biseles generosos (piedra "almohadillada", madera con canto redondo),
color por vértice vivo por ELEMENTO (cada tabla, caña, piedra o fila de
hoja con su propio tono dentro de la paleta del material) en vez de un
tinte plano por pieza.
"""

import math
import os
import sys
from itertools import pairwise

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import bpy  # noqa: E402

import _materials as M  # noqa: E402
import _shapes as S  # noqa: E402
import bmesh  # noqa: E402
import common as C  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

CATEGORY = 'building_kit_modular'

# ---------------------------------------------------------------------------
# Rejilla (documentada en docs/art/kit-construccion.md: si cambia aquí, cambia
# allí también).
# ---------------------------------------------------------------------------
GRID = 2.0              # lado de la celda (m)
HALF = GRID / 2.0
FOUND_H = 0.6           # altura del pilote/cimiento: el suelo se apoya aquí
FLOOR_T = 0.15          # grosor del suelo (cara superior = z de las paredes)
WALL_H = 2.5            # altura de pared completa
HALF_WALL_H = 1.25      # media pared
STOREY = WALL_H + FLOOR_T  # 2.65: de cara de suelo a cara de suelo
DOOR_W, DOOR_H = 1.0, 2.05          # hueco de puerta (centrado en X)
WIN_W, WIN_Z0, WIN_Z1 = 0.9, 0.95, 1.75  # hueco de ventana
ROOF_PITCH = math.radians(35.0)
ROOF_OVERHANG = 0.35    # alero en el lado bajo (medido a lo largo de la pendiente)
STAIR_W, STAIR_RUN = 1.0, 2.0 * GRID  # escalera de un piso: 1 m x 4 m
STAIR_STEPS = 13
RAIL_H = 1.0
DOOR_LEAF_W, DOOR_LEAF_H = DOOR_W - 0.04, DOOR_H - 0.03

WALL_T = {'Palm': 0.12, 'Bamboo': 0.12, 'Wood': 0.14, 'Stone': 0.30}

MATERIALS = ['Palm', 'Bamboo', 'Wood', 'Stone']
MATERIAL_GROUP = {'Palm': 'KitPalma', 'Bamboo': 'KitBambu',
                  'Wood': 'KitMadera', 'Stone': 'KitPiedra'}

# (clave de pieza, presupuesto de tris (min, max), colisión compleja, interactuable)
PIECES = [
    ('Foundation', (40, 1200), False, False),
    ('Floor', (100, 4000), False, False),
    ('Wall', (150, 6000), False, False),
    ('WallDoor', (150, 6000), True, False),
    ('WallWindow', (150, 6000), True, False),
    ('WallHalf', (80, 4000), False, False),
    ('RoofShed', (200, 7000), True, False),
    ('RoofGable', (200, 7000), True, False),
    ('RoofCorner', (200, 7000), True, False),
    ('Stairs', (100, 4000), True, False),
    ('Door', (60, 3000), False, True),
    ('Railing', (60, 3000), False, False),
    # hastiales: al final para no mover las semillas de las piezas previas
    ('Gable', (60, 4000), False, False),
    ('GableShed', (60, 5000), False, False),
]

VARIANTS = []
for _mi, _mat in enumerate(MATERIALS):
    for _pi, (_key, _budget, _cplx, _inter) in enumerate(PIECES):
        VARIANTS.append(dict(
            name=f'Kit_{_mat}_{_key}', seed=2600 + _mi * 50 + _pi,
            material=_mat, piece=_key, group=MATERIAL_GROUP[_mat],
            tri_budget=_budget, needs_collision=True,
            collision_complex=_cplx, interactable=_inter))


# ---------------------------------------------------------------------------
# Paletas (color de vértice lineal; el material casi blanco lo multiplica)
# ---------------------------------------------------------------------------
PAL = {
    # OJO: valores LINEALES (el render los pasa a sRGB y los aclara mucho):
    # un 0.8 lineal ya se ve casi blanco. La primera pasada usó valores
    # "de sRGB" y el catálogo salió lavado, todo color paja pálido.
    # hoja de palma seca: paja dorada con algún manojo aún verdoso
    'thatch': [(0.66, 0.40, 0.10), (0.56, 0.33, 0.08), (0.74, 0.50, 0.16),
               (0.46, 0.42, 0.12), (0.62, 0.36, 0.09)],
    # rama sin descortezar para la estructura de palma
    'branch': [(0.30, 0.17, 0.07), (0.36, 0.21, 0.09), (0.26, 0.15, 0.06)],
    'bamboo': [(0.55, 0.45, 0.12), (0.44, 0.46, 0.10), (0.62, 0.50, 0.16),
               (0.40, 0.44, 0.10)],
    'bamboo_node': (0.30, 0.22, 0.06),
    'lashing': (0.45, 0.28, 0.10),
    'wood': [(0.45, 0.24, 0.08), (0.38, 0.20, 0.07), (0.50, 0.29, 0.10),
             (0.42, 0.23, 0.08), (0.34, 0.18, 0.07)],
    'wood_dark': [(0.22, 0.11, 0.05), (0.26, 0.13, 0.05), (0.20, 0.10, 0.04)],
    # piedra coralina/basáltica cálida con variación (nunca gris plano)
    'stone': [(0.40, 0.34, 0.26), (0.33, 0.30, 0.25), (0.45, 0.38, 0.28),
              (0.30, 0.29, 0.26), (0.38, 0.31, 0.23), (0.35, 0.33, 0.29)],
    'moss': (0.20, 0.28, 0.08),
    'slate': [(0.22, 0.24, 0.27), (0.28, 0.26, 0.24), (0.20, 0.21, 0.24),
              (0.32, 0.28, 0.22)],
}

# Bisel por tipo de elemento: (ancho, segmentos, ángulo límite)
BEVEL = {
    'wood': (0.012, 2, 35.0),
    # cilindros de 8-12 lados: el ángulo límite va por ENCIMA de 45° para
    # biselar solo el canto de las tapas y no cada arista longitudinal
    # (con 40° un bambú de 8 lados multiplicaba sus triángulos x3).
    'pole': (0.008, 1, 50.0),
    'thatch': (0.012, 1, 60.0),
    'stone': (0.035, 2, 35.0),
    'slab': (0.018, 2, 35.0),
    'shingle': (0.01, 1, 35.0),
    'soft': (0.02, 2, 35.0),
    # sillares grandes de ruina: bisel ancho de un solo segmento (el
    # redondeo lo pone el sombreado suave; 2 segmentos disparaban el marae)
    'block': (0.03, 1, 35.0),
    # piezas orgánicas o diminutas (blobs, brotes): sin bisel
    'none': None,
}

_BUILDERS = {}


def _register(key):
    def deco(fn):
        _BUILDERS[key] = fn
        return fn
    return deco


def build(variant):
    rnd = C.seeded_rng(variant['seed'])
    return _BUILDERS[variant['piece']](variant['material'], rnd, 'SM_' + variant['name'])


# ---------------------------------------------------------------------------
# Contenedor de piezas: agrupa por tipo de bisel, bisela cada grupo por
# separado (una piedra se redondea mucho más que una caña) y une al final.
# ---------------------------------------------------------------------------
class Parts:
    def __init__(self):
        self.kinds = {}

    def add(self, obj, kind):
        self.kinds.setdefault(kind, []).append(obj)
        return obj

    def extend(self, other):
        for k, objs in other.kinds.items():
            self.kinds.setdefault(k, []).extend(objs)

    def join_all(self, name):
        objs = [o for lst in self.kinds.values() for o in lst]
        return C.join_objects(objs, name) if len(objs) > 1 else objs[0]

    def transform(self, mat):
        """Aplica una matriz a todas las piezas (con volteo de normales si
        la matriz es un espejo)."""
        flip = mat.to_3x3().determinant() < 0
        for lst in self.kinds.values():
            for o in lst:
                o.data.transform(mat)
                if flip:
                    o.data.flip_normals()
                o.data.update()

    def finish(self, name):
        joined = []
        for kind, objs in self.kinds.items():
            if not objs:
                continue
            obj = C.join_objects(objs, f'{name}_{kind}') if len(objs) > 1 else objs[0]
            if BEVEL.get(kind) is not None:
                w, seg, ang = BEVEL[kind]
                S.bevel_obj(obj, width=w, segments=seg, limit_angle_deg=ang)
            joined.append(obj)
        obj = C.join_objects(joined, name) if len(joined) > 1 else joined[0]
        obj.name = name
        C.shade_smooth_auto(obj, angle_deg=35.0)
        C.add_basic_uv(obj)
        return obj


def ground(obj):
    """Baja/sube la malla para que su punto más bajo quede en z=0 (pivote
    en la base aunque blobs con ruido o piedras asomen unos mm por debajo)."""
    mz = min(v.co.z for v in obj.data.vertices)
    if abs(mz) > 1e-5:
        obj.data.transform(Matrix.Translation((0.0, 0.0, -mz)))
        obj.data.update()
    return obj


# ---------------------------------------------------------------------------
# Primitivas con color y material
# ---------------------------------------------------------------------------
def _pick(rnd, key):
    return rnd.choice(PAL[key])


def _tint(obj, rgb, rnd, jitter=0.025):
    C.set_vertex_colors(obj, C.constant_tint(rgb, alpha=0.0, jitter=jitter, rnd=rnd))


def _box(name, size, center, mat, rgb, rnd, rot_z=0.0, jitter=0.025):
    o = C.make_box(name, size, center=(0.0, 0.0, 0.0))
    if rot_z:
        o.data.transform(Matrix.Rotation(rot_z, 4, 'Z'))
    o.data.transform(Matrix.Translation(Vector(center)))
    M.assign(o, [mat])
    _tint(o, rgb, rnd, jitter)
    return o


def _axis_matrix(p0, p1):
    """Matriz que lleva el eje Z local (0..len) al segmento p0→p1."""
    p0, p1 = Vector(p0), Vector(p1)
    d = p1 - p0
    length = d.length
    q = Vector((0.0, 0.0, 1.0)).rotation_difference(d.normalized())
    return Matrix.Translation(p0) @ q.to_matrix().to_4x4(), length


def _rod(name, p0, p1, r, mat, rgb, rnd, segs=12, taper=1.0, jitter=0.03):
    """Cilindro entre dos puntos (poste, rama, travesaño)."""
    m, length = _axis_matrix(p0, p1)
    o = C.make_cylinder(name, radius=r, depth=length, segments=segs,
                        center=(0.0, 0.0, length / 2.0), radius2=r * taper)
    o.data.transform(m)
    M.assign(o, [mat])
    _tint(o, rgb, rnd, jitter)
    return o


def _bamboo(name, p0, p1, r, rnd, segs=10, node_step=0.42, rgb=None):
    """Caña de bambú: tubo con anillos de nudo abultados y más oscuros."""
    m, length = _axis_matrix(p0, p1)
    rgb = rgb or _pick(rnd, 'bamboo')
    zs = [(0.0, r, False)]
    z = rnd.uniform(0.12, node_step)
    while z < length - 0.08:
        # un solo anillo abultado por nudo (el sombreado suave lo redondea):
        # con 3 anillos por nudo los paneles de caña triplicaban sus tris
        zs.append((z, r * 1.12, True))
        z += node_step * rnd.uniform(0.9, 1.1)
    zs.append((length, r, False))
    rings = [C.point_ring((0.0, 0.0, zz), rr, segs) for zz, rr, _ in zs]
    o = C.ring_loft(name, rings, cap_start=True, cap_end=True)
    node_rings = {i for i, (_, _, n) in enumerate(zs) if n}
    jit = {}

    def fn(v):
        ring = v.index // segs
        if v.index not in jit:
            jit[v.index] = rnd.uniform(-0.025, 0.025)
        base = PAL['bamboo_node'] if ring in node_rings else rgb
        j = jit[v.index]
        return (min(1, max(0, base[0] + j)), min(1, max(0, base[1] + j)),
                min(1, max(0, base[2] + j)), 0.0)
    C.set_vertex_colors(o, fn)
    o.data.transform(m)
    M.assign(o, ['M_Wood'])
    return o


def _fringe_slab(name, u0, u1, v0, v1, t, rnd, rgb, fringe=0.07, cells=None):
    """Tira de hoja de palma en el marco local (u=X a lo largo de la fila,
    v=Y en el sentido del agua, w=Z grosor): caja subdividida a lo largo de
    u cuyo borde bajo (v0) está deshilachado en dientes irregulares — lee
    como un manojo de hojas atado, no como una tabla."""
    n = cells or max(2, int((u1 - u0) / 0.12))
    bm = bmesh.new()
    bot = []
    for i in range(n + 1):
        u = u0 + (u1 - u0) * i / n
        dv = 0.0 if i in (0, n) else rnd.uniform(0.0, fringe) * (1.0 if i % 2 else 0.35)
        dt = rnd.uniform(-0.2, 0.2) * t
        # sección: [v0-dv (abajo, fino) .. v1 (arriba, grueso)]
        a = bm.verts.new((u, v0 - dv, 0.0))
        b = bm.verts.new((u, v0 - dv, t * 0.45 + dt * 0.3))
        c = bm.verts.new((u, v1, t + dt))
        d = bm.verts.new((u, v1, 0.0))
        bot.append((a, b, c, d))
    for i in range(n):
        r0, r1 = bot[i], bot[i + 1]
        for k in range(4):
            k2 = (k + 1) % 4
            bm.faces.new((r0[k], r1[k], r1[k2], r0[k2]))
    bm.faces.new(list(reversed(bot[0])))
    bm.faces.new(bot[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new(name, me)
    C.link_object(o)
    M.assign(o, ['M_Leaf'])
    # variación suave a lo largo de la tira: puntas algo más claras
    jit = {}

    def fn(v):
        if v.index not in jit:
            jit[v.index] = rnd.uniform(-0.035, 0.035)
        j = jit[v.index]
        # la parte alta de cada manojo queda bajo la fila siguiente: más
        # oscura; las puntas del fleco, más claras (lee como capas de hoja)
        k = max(0.0, min(1.0, (v.co.y - v0) / max(1e-4, v1 - v0)))
        f = 1.12 - 0.45 * k
        return (min(1, max(0, rgb[0] * f + j)), min(1, max(0, rgb[1] * f + j)),
                min(1, max(0, rgb[2] * f + j * 0.5)), 0.0)
    C.set_vertex_colors(o, fn)
    return o


# ---------------------------------------------------------------------------
# Intervalos (recorte de filas/columnas alrededor de huecos)
# ---------------------------------------------------------------------------
def _subtract(a, b, cuts):
    """[a,b] menos la unión de los intervalos `cuts`; devuelve la lista de
    tramos restantes (se descartan los de menos de 4 cm)."""
    segs = [(a, b)]
    for c0, c1 in cuts:
        nxt = []
        for s0, s1 in segs:
            if c1 <= s0 or c0 >= s1:
                nxt.append((s0, s1))
                continue
            if c0 > s0:
                nxt.append((s0, c0))
            if c1 < s1:
                nxt.append((c1, s1))
        segs = nxt
    return [(s0, s1) for s0, s1 in segs if s1 - s0 > 0.04]


def _row_cuts(openings, z0, z1):
    """Huecos (x0,x1,z0,z1) que cortan una fila horizontal [z0,z1]."""
    return [(o[0], o[1]) for o in openings if o[2] < z1 - 1e-4 and o[3] > z0 + 1e-4]


def _col_cuts(openings, x0, x1):
    return [(o[2], o[3]) for o in openings if o[0] < x1 - 1e-4 and o[1] > x0 + 1e-4]


# ---------------------------------------------------------------------------
# PAREDES (genérico: altura + huecos) — la pared corre a lo largo de X,
# centrada en y=0, de x=-1 a x=+1.
# ---------------------------------------------------------------------------
def _wall_parts(mat, rnd, height, openings):
    p = Parts()
    t = WALL_T[mat]
    x0, x1 = -HALF, HALF
    if mat == 'Palm':
        r = 0.055
        # estructura: postes de rama en los extremos, soleras y marcos de hueco
        for i, x in enumerate((x0 + r, x1 - r)):
            p.add(_rod(f'Post{i}', (x, 0, 0), (x, 0, height), r, 'M_Wood',
                       _pick(rnd, 'branch'), rnd, taper=0.9), 'pole')
        for i, z in enumerate((r * 0.8, height - r * 0.8)):
            for k, (s0, s1) in enumerate(_subtract(x0 + r, x1 - r, _row_cuts(openings, z - r, z + r))):
                p.add(_rod(f'Rail{i}_{k}', (s0, 0, z), (s1, 0, z), r * 0.8, 'M_Wood',
                           _pick(rnd, 'branch'), rnd), 'pole')
        _opening_frames_round(p, rnd, openings, r * 0.85, 'branch', height)
        # relleno: filas de hoja superpuestas (la de arriba solapa por fuera)
        row_h, step = 0.34, 0.28
        z = height - r
        k = 0
        while z > r * 0.5:
            za = max(r * 0.5, z - row_h)
            for j, (s0, s1) in enumerate(_subtract(x0 + r * 1.5, x1 - r * 1.5,
                                                   _row_cuts(openings, za, z))):
                for side in (-1, 1):
                    s = _fringe_slab(f'Th{k}_{j}_{side}', s0, s1, za, z, t * 0.32, rnd,
                                     _pick(rnd, 'thatch'), fringe=max(0.0, min(0.09, za - 0.005)))
                    # marco local (u, v, w) -> pared: v sube en Z (flecos
                    # abajo), w sale hacia -Y; la cara +Y es su espejo.
                    m = Matrix.Translation((0.0, -t * 0.02, 0.0)) @ Matrix.Rotation(math.pi / 2, 4, 'X')
                    if side > 0:
                        m = Matrix.Scale(-1, 4, (0, 1, 0)) @ m
                    s.data.transform(m)
                    if side > 0:
                        s.data.flip_normals()
                    s.data.update()
                    p.add(s, 'thatch')
            z -= step
            k += 1
        # listones de atado por fuera y por dentro
        for i, zb in enumerate((height * 0.33, height * 0.7)):
            for j, (s0, s1) in enumerate(_subtract(x0 + r, x1 - r, _row_cuts(openings, zb - 0.03, zb + 0.03))):
                for side in (-1, 1):
                    p.add(_rod(f'Bind{i}_{j}_{side}', (s0, side * t * 0.42, zb),
                               (s1, side * t * 0.42, zb), 0.018, 'M_Wood',
                               _pick(rnd, 'branch'), rnd, segs=8), 'pole')
    elif mat == 'Bamboo':
        r_post, r = 0.06, 0.034
        for i, x in enumerate((x0 + r_post, x1 - r_post)):
            p.add(_bamboo(f'Post{i}', (x, 0, 0), (x, 0, height), r_post, rnd, segs=12), 'pole')
        # cañas verticales de relleno, cortadas por los huecos
        xs = []
        x = x0 + r_post * 2 + r
        while x < x1 - r_post * 2 - r * 0.5:
            xs.append(x)
            x += r * 2.05
        for i, x in enumerate(xs):
            for j, (s0, s1) in enumerate(_subtract(0.0, height, _col_cuts(openings, x - r, x + r))):
                p.add(_bamboo(f'Cane{i}_{j}', (x, 0, s0), (x, 0, s1), r * rnd.uniform(0.92, 1.05),
                              rnd, segs=8, node_step=0.5), 'pole')
        # travesaños horizontales a ambos lados + marcos
        for i, z in enumerate((0.18, height * 0.55, height - 0.12)):
            for j, (s0, s1) in enumerate(_subtract(x0 + r_post, x1 - r_post,
                                                   _row_cuts(openings, z - 0.05, z + 0.05))):
                for side in (-1, 1):
                    p.add(_bamboo(f'Rail{i}_{j}_{side}', (s0, side * (r + 0.024), z),
                                  (s1, side * (r + 0.024), z), 0.026, rnd, segs=8), 'pole')
        for oi, (ox0, ox1, oz0, oz1) in enumerate(openings):
            for zz in ([oz0, oz1] if oz0 > 0.01 else [oz1]):
                if zz >= height - 0.05:
                    continue
                p.add(_bamboo(f'OpH{oi}_{zz:.2f}', (ox0 - 0.05, 0, zz + (0.03 if zz == oz1 else -0.03)),
                              (ox1 + 0.05, 0, zz + (0.03 if zz == oz1 else -0.03)), 0.04, rnd,
                              segs=10), 'pole')
            for xx in (ox0 - 0.04, ox1 + 0.04):
                p.add(_bamboo(f'OpV{oi}_{xx:.2f}', (xx, 0, oz0), (xx, 0, min(oz1 + 0.07, height)),
                              0.042, rnd, segs=10), 'pole')
    elif mat == 'Wood':
        post = 0.14
        for i, x in enumerate((x0 + post / 2, x1 - post / 2)):
            p.add(_box(f'Post{i}', (post, t + 0.02, height), (x, 0, height / 2), 'M_Wood',
                       _pick(rnd, 'wood_dark'), rnd), 'wood')
        # tablas verticales cortadas por los huecos
        n = 7
        bw = (GRID - 2 * post) / n
        board_top = height - 0.1  # bajo la viga de coronación (sin caras coplanarias)
        for i in range(n):
            x = x0 + post + bw * (i + 0.5)
            for j, (s0, s1) in enumerate(_subtract(0.0, board_top, _col_cuts(openings, x - bw / 2, x + bw / 2))):
                p.add(_box(f'Board{i}_{j}', (bw * 0.94, t * 0.7, s1 - s0), (x, 0, (s0 + s1) / 2),
                           'M_Wood', _pick(rnd, 'wood'), rnd), 'wood')
        # travesaños de canto por el interior (cara +Y) y viga de coronación
        for i, z in enumerate((0.35, height * 0.62)):
            for j, (s0, s1) in enumerate(_subtract(x0 + post, x1 - post, _row_cuts(openings, z - 0.06, z + 0.06))):
                p.add(_box(f'Batten{i}_{j}', (s1 - s0, 0.05, 0.12), ((s0 + s1) / 2, t * 0.35 + 0.025, z),
                           'M_Wood', _pick(rnd, 'wood_dark'), rnd), 'wood')
        if not any(o[3] >= height - 0.01 for o in openings):
            p.add(_box('TopBeam', (GRID, t + 0.04, 0.12), (0, 0, height - 0.06), 'M_Wood',
                       _pick(rnd, 'wood_dark'), rnd), 'wood')
        for oi, (ox0, ox1, oz0, oz1) in enumerate(openings):
            fw = 0.09
            p.add(_box(f'Lintel{oi}', (ox1 - ox0 + 2 * fw, t + 0.04, fw),
                       ((ox0 + ox1) / 2, 0, oz1 + fw / 2), 'M_Wood', _pick(rnd, 'wood_dark'), rnd), 'wood')
            if oz0 > 0.01:
                p.add(_box(f'Sill{oi}', (ox1 - ox0 + 2 * fw + 0.08, t + 0.08, fw * 0.8),
                           ((ox0 + ox1) / 2, 0, oz0 - fw * 0.4), 'M_Wood', _pick(rnd, 'wood_dark'), rnd), 'wood')
            for xx in (ox0 - fw / 2, ox1 + fw / 2):
                p.add(_box(f'Jamb{oi}_{xx:.2f}', (fw, t + 0.04, oz1 - oz0), (xx, 0, (oz0 + oz1) / 2),
                           'M_Wood', _pick(rnd, 'wood_dark'), rnd), 'wood')
    else:  # Stone
        # dinteles y alféizares como piedras largas; las hiladas los rodean
        lintels = []
        for oi, (ox0, ox1, oz0, oz1) in enumerate(openings):
            lz = oz1 + 0.13
            if lz + 0.13 <= height + 1e-4:
                lintels.append((ox0 - 0.22, ox1 + 0.22, oz1, oz1 + 0.26))
                p.add(_box(f'Lintel{oi}', (ox1 - ox0 + 0.44, t + 0.03, 0.26), ((ox0 + ox1) / 2, 0, lz),
                           'M_Stone', _pick(rnd, 'stone'), rnd), 'stone')
            if oz0 > 0.01:
                lintels.append((ox0 - 0.12, ox1 + 0.12, oz0 - 0.14, oz0))
                p.add(_box(f'Sill{oi}', (ox1 - ox0 + 0.24, t + 0.08, 0.14), ((ox0 + ox1) / 2, 0, oz0 - 0.07),
                           'M_Stone', _pick(rnd, 'stone'), rnd), 'stone')
        cuts_all = openings + lintels
        breaks = sorted({c[k] for c in cuts_all for k in (2, 3) if 0.0 < c[k] < height})
        _stone_courses(p, rnd, x0, x1, 0.0, height, t, cuts_all, 'Blk', breaks=breaks)
    return p


def _opening_frames_round(p, rnd, openings, r, pal, height):
    for oi, (ox0, ox1, oz0, oz1) in enumerate(openings):
        top = min(oz1 + r, height - r)
        for xx in (ox0 - r, ox1 + r):
            p.add(_rod(f'OpV{oi}_{xx:.2f}', (xx, 0, max(0.0, oz0 - r)), (xx, 0, top), r, 'M_Wood',
                       _pick(rnd, pal), rnd), 'pole')
        if oz1 + r < height - r:
            p.add(_rod(f'OpT{oi}', (ox0 - 2 * r, 0, oz1 + r), (ox1 + 2 * r, 0, oz1 + r), r, 'M_Wood',
                       _pick(rnd, pal), rnd), 'pole')
        if oz0 > 0.01:
            p.add(_rod(f'OpB{oi}', (ox0 - 2 * r, 0, oz0 - r), (ox1 + 2 * r, 0, oz0 - r), r, 'M_Wood',
                       _pick(rnd, pal), rnd), 'pole')


def _stone_courses(p, rnd, x0, x1, z0, z1, t, cuts, prefix, moss=True, breaks=()):
    """Hiladas de sillarejo: filas de altura algo irregular, piedras de
    largo aleatorio a matajuntas, recortadas alrededor de `cuts`.

    `breaks` son cotas en las que una hilada DEBE terminar (bordes de
    huecos, dinteles y alféizares): sin ellas, la hilada que cruza un
    alféizar de 14 cm se recortaba entera (28-32 cm) y dejaba un agujero
    bajo él (visto en la primera lámina de la pared de piedra con ventana)."""
    z = z0
    row = 0
    gap = 0.018
    while z < z1 - 1e-4:
        ch = rnd.uniform(0.24, 0.32)
        nxt = [b for b in breaks if b > z + 1e-4]
        if nxt and nxt[0] - z < ch + 0.12:
            ch = nxt[0] - z
        elif z1 - (z + ch) < 0.16:
            ch = z1 - z
        segs = _subtract(x0, x1, _row_cuts(cuts, z, z + ch))
        for si, (s0, s1) in enumerate(segs):
            x = s0
            first = rnd.uniform(0.18, 0.45) if row % 2 else rnd.uniform(0.35, 0.6)
            ln = first
            k = 0
            while x < s1 - 1e-4:
                if s1 - (x + ln) < 0.18:
                    ln = s1 - x
                rgb = _pick(rnd, 'stone')
                if moss and z < 0.35 and rnd.random() < 0.35:
                    rgb = tuple(a * 0.5 + b * 0.5 for a, b in zip(rgb, PAL['moss'], strict=True))
                d = t * rnd.uniform(0.94, 1.0)
                p.add(_box(f'{prefix}{row}_{si}_{k}', (ln - gap, d, ch - gap),
                           (x + ln / 2, rnd.uniform(-0.012, 0.012), z + ch / 2), 'M_Stone', rgb, rnd,
                           rot_z=rnd.uniform(-0.02, 0.02), jitter=0.03), 'stone')
                x += ln
                ln = rnd.uniform(0.32, 0.62)
                k += 1
        z += ch
        row += 1


def _opening_door():
    return [(-DOOR_W / 2, DOOR_W / 2, 0.0, DOOR_H)]


def _opening_window():
    return [(-WIN_W / 2, WIN_W / 2, WIN_Z0, WIN_Z1)]


@_register('Wall')
def _b_wall(mat, rnd, name):
    return _wall_parts(mat, rnd, WALL_H, []).finish(name)


@_register('WallDoor')
def _b_wall_door(mat, rnd, name):
    return _wall_parts(mat, rnd, WALL_H, _opening_door()).finish(name)


@_register('WallWindow')
def _b_wall_window(mat, rnd, name):
    return _wall_parts(mat, rnd, WALL_H, _opening_window()).finish(name)


@_register('WallHalf')
def _b_wall_half(mat, rnd, name):
    p = _wall_parts(mat, rnd, HALF_WALL_H, [])
    t = WALL_T[mat]
    # remate superior propio de la media pared (se ve desde arriba)
    if mat == 'Palm':
        p.add(_rod('Cap', (-HALF, 0, HALF_WALL_H + 0.03), (HALF, 0, HALF_WALL_H + 0.03), 0.06,
                   'M_Wood', _pick(rnd, 'branch'), rnd), 'pole')
    elif mat == 'Bamboo':
        p.add(_bamboo('Cap', (-HALF, 0, HALF_WALL_H + 0.04), (HALF, 0, HALF_WALL_H + 0.04), 0.05,
                      rnd, segs=12), 'pole')
    elif mat == 'Wood':
        p.add(_box('Cap', (GRID, t + 0.08, 0.06), (0, 0, HALF_WALL_H + 0.03), 'M_Wood',
                   _pick(rnd, 'wood'), rnd), 'wood')
    else:
        x = -HALF
        while x < HALF - 1e-4:
            ln = min(rnd.uniform(0.45, 0.7), HALF - x)
            if HALF - (x + ln) < 0.2:
                ln = HALF - x
            p.add(_box(f'Cap{x:.2f}', (ln - 0.02, t + 0.08, 0.1), (x + ln / 2, 0, HALF_WALL_H + 0.05),
                       'M_Stone', _pick(rnd, 'stone'), rnd), 'stone')
            x += ln
    return p.finish(name)


# ---------------------------------------------------------------------------
# CIMIENTO / PILOTE (se coloca en las esquinas de la celda)
# ---------------------------------------------------------------------------
@_register('Foundation')
def _b_foundation(mat, rnd, name):
    p = Parts()
    h = FOUND_H
    if mat in ('Palm', 'Bamboo', 'Wood'):
        # zapata de piedra plana bajo el poste (no se hunde en arena blanda)
        p.add(_box('Pad', (0.42, 0.42, 0.12), (0, 0, 0.06), 'M_Stone', _pick(rnd, 'stone'), rnd,
                   rot_z=rnd.uniform(-0.3, 0.3)), 'stone')
    if mat == 'Palm':
        p.add(_rod('Log', (0, 0, 0.08), (0, 0, h), 0.11, 'M_Wood', _pick(rnd, 'branch'), rnd,
                   segs=12, taper=0.9), 'pole')
        for i, zb in enumerate((h - 0.14, 0.3)):
            p.add(_rod(f'Lash{i}', (0, 0, zb), (0, 0, zb + 0.05), 0.118, 'M_Wood', PAL['lashing'], rnd,
                       segs=12), 'pole')
    elif mat == 'Bamboo':
        for i in range(3):
            a = 2 * math.pi * i / 3 + 0.4
            cx, cy = math.cos(a) * 0.058, math.sin(a) * 0.058
            p.add(_bamboo(f'Culm{i}', (cx, cy, 0.1), (cx, cy, h), 0.055, rnd, segs=10, node_step=0.3), 'pole')
        for i, zb in enumerate((h - 0.12, 0.3)):
            p.add(_rod(f'Lash{i}', (0, 0, zb), (0, 0, zb + 0.05), 0.125, 'M_Wood', PAL['lashing'], rnd,
                       segs=12), 'pole')
    elif mat == 'Wood':
        p.add(_box('Post', (0.2, 0.2, h - 0.12 - 0.06), (0, 0, 0.12 + (h - 0.18) / 2), 'M_Wood',
                   _pick(rnd, 'wood_dark'), rnd), 'wood')
        p.add(_box('Cap', (0.36, 0.36, 0.06), (0, 0, h - 0.03), 'M_Wood', _pick(rnd, 'wood'), rnd), 'wood')
    else:
        _stone_pier(p, rnd, 0.52, h)
    return p.finish(name)


def _stone_pier(p, rnd, w, h):
    z = 0.0
    k = 0
    while z < h - 1e-4:
        ch = rnd.uniform(0.17, 0.22)
        if h - (z + ch) < 0.12:
            ch = h - z
        if k % 2 == 0:
            p.add(_box(f'P{k}a', (w, w * 0.5 - 0.01, ch - 0.015), (0, -w * 0.25, z + ch / 2), 'M_Stone',
                       _pick(rnd, 'stone'), rnd), 'stone')
            p.add(_box(f'P{k}b', (w, w * 0.5 - 0.01, ch - 0.015), (0, w * 0.25, z + ch / 2), 'M_Stone',
                       _pick(rnd, 'stone'), rnd), 'stone')
        else:
            p.add(_box(f'P{k}a', (w * 0.5 - 0.01, w, ch - 0.015), (-w * 0.25, 0, z + ch / 2), 'M_Stone',
                       _pick(rnd, 'stone'), rnd), 'stone')
            p.add(_box(f'P{k}b', (w * 0.5 - 0.01, w, ch - 0.015), (w * 0.25, 0, z + ch / 2), 'M_Stone',
                       _pick(rnd, 'stone'), rnd), 'stone')
        z += ch
        k += 1


# ---------------------------------------------------------------------------
# SUELO (celda de 2 x 2 m, cara superior a FLOOR_T)
# ---------------------------------------------------------------------------
@_register('Floor')
def _b_floor(mat, rnd, name):
    p = Parts()
    t = FLOOR_T
    if mat in ('Palm', 'Bamboo'):
        # durmientes bajo el entarimado de rollizos/cañas
        rb = 0.045
        for i, x in enumerate((-HALF + 0.12, HALF - 0.12)):
            if mat == 'Palm':
                p.add(_rod(f'Bearer{i}', (-HALF, x, rb), (HALF, x, rb), rb, 'M_Wood', _pick(rnd, 'branch'), rnd),
                      'pole')
            else:
                p.add(_bamboo(f'Bearer{i}', (-HALF, x, rb), (HALF, x, rb), rb, rnd, segs=10), 'pole')
        r = (t - 2 * rb) / 2.0 + 0.004
        n = int(GRID / (2 * r * 1.02))
        step = GRID / n
        for i in range(n):
            x = -HALF + step * (i + 0.5)
            rr = r * rnd.uniform(0.93, 1.0)
            z = 2 * rb + rr - 0.004
            if mat == 'Palm':
                p.add(_rod(f'Pole{i}', (x, -HALF, z), (x, HALF, z), rr, 'M_Wood',
                           _pick(rnd, 'branch') if i % 3 else _pick(rnd, 'thatch'), rnd, segs=10), 'pole')
            else:
                p.add(_bamboo(f'Cane{i}', (x, -HALF, z), (x, HALF, z), rr, rnd, segs=8, node_step=0.55), 'pole')
        if mat == 'Palm':
            # esterilla de hoja trenzada en el centro: cálida y "habitada"
            for j in range(5):
                y0 = -0.6 + j * 0.24
                s = _fringe_slab(f'Mat{j}', -0.7, 0.7, y0, y0 + 0.26, 0.012, rnd,
                                 PAL['thatch'][j % len(PAL['thatch'])], fringe=0.0, cells=4)
                s.data.transform(Matrix.Translation((0, 0, t - 0.004)))
                p.add(s, 'thatch')
    elif mat == 'Wood':
        for i, x in enumerate((-HALF + 0.15, 0.0, HALF - 0.15)):
            p.add(_box(f'Joist{i}', (0.1, GRID, t - 0.05), (x, 0, (t - 0.05) / 2), 'M_Wood',
                       _pick(rnd, 'wood_dark'), rnd), 'wood')
        n = 7
        pw = GRID / n
        for i in range(n):
            y = -HALF + pw * (i + 0.5)
            p.add(_box(f'Plank{i}', (GRID, pw * 0.95, 0.05), (0, y, t - 0.025), 'M_Wood',
                       _pick(rnd, 'wood'), rnd), 'wood')
    else:
        p.add(_box('Bed', (GRID - 0.02, GRID - 0.02, t - 0.06), (0, 0, (t - 0.06) / 2), 'M_Stone',
                   PAL['stone'][3], rnd), 'slab')
        # losas irregulares a matajuntas
        rows = [0.52, 0.46, 0.5, 0.52]
        y = -HALF
        for ri, rh in enumerate(rows):
            rh = rh * GRID / sum(rows)
            x = -HALF
            k = 0
            while x < HALF - 1e-4:
                ln = rnd.uniform(0.45, 0.8) if not (ri % 2 and k == 0) else rnd.uniform(0.25, 0.4)
                if HALF - (x + ln) < 0.22:
                    ln = HALF - x
                p.add(_box(f'Flag{ri}_{k}', (ln - 0.025, rh - 0.025, 0.07), (x + ln / 2, y + rh / 2, t - 0.035),
                           'M_Stone', _pick(rnd, 'stone'), rnd, jitter=0.035), 'slab')
                x += ln
                k += 1
            y += rh
    return p.finish(name)


# ---------------------------------------------------------------------------
# TEJADOS — cobertura construida en el marco local de UNA vertiente
# (u = X a lo largo del alero, v = distancia sobre la pendiente desde el
# alero, w = normal) y llevada a su sitio con una matriz.
# ---------------------------------------------------------------------------
def _slope_matrix(y_eave=-HALF):
    """Vertiente con alero (v=0) en y=y_eave, z=0, subiendo hacia +Y."""
    return Matrix.Translation((0.0, y_eave, 0.0)) @ Matrix.Rotation(ROOF_PITCH, 4, 'X')


def _covering(mat, rnd, v_len, u_range_fn, prefix):
    """Cobertura de una vertiente de longitud v_len (sin alero); el alero
    añade ROOF_OVERHANG por debajo de v=0. u_range_fn(v) -> (u0, u1)."""
    p = Parts()
    v_start = -ROOF_OVERHANG
    if mat == 'Palm':
        step, rh = 0.2, 0.36
        v = v_start
        k = 0
        while v < v_len - 0.05:
            v1 = min(v + rh, v_len + 0.04)
            u0, u1 = u_range_fn((v + v1) / 2)
            if u1 - u0 > 0.08:
                s = _fringe_slab(f'{prefix}Th{k}', u0, u1, v, v1, 0.07, rnd, _pick(rnd, 'thatch'),
                                 fringe=0.09)
                s.data.transform(Matrix.Translation((0, 0, 0.03 + (k % 2) * 0.012)))
                p.add(s, 'thatch')
            v += step
            k += 1
    elif mat == 'Bamboo':
        # medias cañas a lo largo de la pendiente, alternas cóncava/convexa
        r = 0.055
        n = int(GRID / (r * 2.0))
        for i in range(n):
            u = -HALF + (i + 0.5) * GRID / n
            # tramo de v donde esta columna u cae dentro de la vertiente
            vs = [vv for vv in _frange(v_start, v_len, 0.05) if u_range_fn(vv)[0] <= u - r * 0.5 <= u_range_fn(vv)[1]
                  and u + r * 0.5 <= u_range_fn(vv)[1]]
            if len(vs) < 2:
                continue
            lift = r * (1.0 if i % 2 else 0.35)
            p.add(_bamboo(f'{prefix}Cn{i}', (u, vs[0], lift), (u, vs[-1] + 0.05, lift), r * rnd.uniform(0.94, 1.04),
                          rnd, segs=10, node_step=0.5), 'pole')
    elif mat in ('Wood', 'Stone'):
        # tejuelas de madera / lajas de piedra a matajuntas, fila sobre fila
        if mat == 'Wood':
            step, rl, th, kind = 0.26, 0.42, 0.03, 'shingle'
        else:
            step, rl, th, kind = 0.28, 0.44, 0.05, 'shingle'
        v = v_start
        row = 0
        while v < v_len - 0.05:
            v1 = min(v + rl, v_len + 0.05)
            u0, u1 = u_range_fn((v + v1) / 2)
            u = u0 - (0.1 if row % 2 else 0.0)
            k = 0
            while u < u1 - 0.05:
                w = rnd.uniform(0.2, 0.3) if mat == 'Wood' else rnd.uniform(0.26, 0.4)
                a, b = max(u, u0), min(u + w, u1)
                if b - a > 0.05:
                    rgb = _pick(rnd, 'wood') if mat == 'Wood' else _pick(rnd, 'slate' if rnd.random() < 0.6 else 'stone')
                    o = _box(f'{prefix}Sh{row}_{k}', (b - a - 0.012, v1 - v, th),
                             ((a + b) / 2, (v + v1) / 2, 0.02 + th / 2 + row % 2 * 0.003 + th * 0.4),
                             'M_Wood' if mat == 'Wood' else 'M_Stone', rgb, rnd,
                             rot_z=rnd.uniform(-0.03, 0.03), jitter=0.03)
                    # la cola de cada pieza se levanta sobre la fila de abajo
                    for vert in o.data.vertices:
                        if vert.co.y < (v + v1) / 2:
                            vert.co.z += th * 0.6
                    p.add(o, kind)
                u += w
                k += 1
            v += step
            row += 1
    return p


def _frange(a, b, s):
    out = []
    x = a
    while x <= b + 1e-9:
        out.append(x)
        x += s
    return out


def _rafters(mat, rnd, v_len, us, prefix, u_range_fn):
    """Cabios bajo la cobertura (se ven desde dentro de la casa), recortados
    a la zona de la vertiente (en la esquina no deben asomar por la otra)."""
    p = Parts()
    for i, u in enumerate(us):
        vs = [vv for vv in _frange(0.0, v_len, 0.02) if u_range_fn(vv)[0] <= u - 0.05]
        v_top = vs[-1] if vs else 0.0
        a, b = (u, -ROOF_OVERHANG * 0.8, -0.04), (u, v_top, -0.04)
        if mat == 'Palm':
            p.add(_rod(f'{prefix}Raf{i}', a, b, 0.045, 'M_Wood', _pick(rnd, 'branch'), rnd), 'pole')
        elif mat == 'Bamboo':
            p.add(_bamboo(f'{prefix}Raf{i}', a, b, 0.045, rnd, segs=10), 'pole')
        else:
            p.add(_box(f'{prefix}Raf{i}', (0.08, v_top + ROOF_OVERHANG * 0.8, 0.12),
                       (u, (v_top - ROOF_OVERHANG * 0.8) / 2, -0.06), 'M_Wood', _pick(rnd, 'wood_dark'), rnd),
                  'wood')
    # correa/listón transversal a media pendiente para atar la cobertura
    for j, vv in enumerate((v_len * 0.35, v_len * 0.8)):
        u0, u1 = u_range_fn(vv)
        u0 = u0 + (0.06 if u0 > -HALF else 0.0)
        if u1 - u0 < 0.1:
            continue
        if mat == 'Palm':
            p.add(_rod(f'{prefix}Pur{j}', (u0, vv, 0.0), (u1, vv, 0.0), 0.03, 'M_Wood',
                       _pick(rnd, 'branch'), rnd, segs=8), 'pole')
        elif mat == 'Bamboo':
            p.add(_bamboo(f'{prefix}Pur{j}', (u0, vv, -0.005), (u1, vv, -0.005), 0.03, rnd, segs=8), 'pole')
        else:
            p.add(_box(f'{prefix}Pur{j}', (u1 - u0, 0.08, 0.05), ((u0 + u1) / 2, vv, 0.0), 'M_Wood',
                       _pick(rnd, 'wood'), rnd), 'wood')
    return p


def _ridge(mat, rnd, p0, p1, prefix):
    """Cumbrera a lo largo del segmento p0→p1 (espacio de pieza)."""
    p = Parts()
    if mat == 'Palm':
        p.add(_rod(f'{prefix}Roll', p0, p1, 0.12, 'M_Leaf', _pick(rnd, 'thatch'), rnd, segs=12), 'thatch')
        d = Vector(p1) - Vector(p0)
        n = max(2, int(d.length / 0.45))
        for i in range(n + 1):
            c = Vector(p0) + d * (i / n)
            p.add(_rod(f'{prefix}Tie{i}', c - d.normalized() * 0.025, c + d.normalized() * 0.025, 0.128,
                       'M_Wood', PAL['lashing'], rnd, segs=12), 'pole')
    elif mat == 'Bamboo':
        p.add(_bamboo(f'{prefix}Ridge', p0, p1, 0.085, rnd, segs=12, node_step=0.5), 'pole')
    elif mat == 'Wood':
        m, length = _axis_matrix(p0, p1)
        o = C.make_box(f'{prefix}Ridge', (0.2, 0.2, length), center=(0, 0, length / 2))
        o.data.transform(Matrix.Rotation(math.radians(45), 4, 'Z'))
        o.data.transform(m)
        M.assign(o, ['M_Wood'])
        _tint(o, _pick(rnd, 'wood_dark'), rnd)
        p.add(o, 'wood')
    else:
        d = Vector(p1) - Vector(p0)
        n = max(2, int(d.length / 0.4))
        for i in range(n):
            a = Vector(p0) + d * (i / n)
            b = Vector(p0) + d * ((i + 1) / n)
            m, length = _axis_matrix(a, b)
            o = C.make_box(f'{prefix}RS{i}', (0.26, 0.2, length - 0.02), center=(0, 0, length / 2))
            o.data.transform(Matrix.Rotation(math.radians(45), 4, 'Z'))
            o.data.transform(m)
            M.assign(o, ['M_Stone'])
            _tint(o, _pick(rnd, 'stone'), rnd)
            p.add(o, 'stone')
    return p


def _plate(mat, rnd, y, prefix):
    """Carrera/durmiente de apoyo sobre la coronación de la pared (z=0)."""
    p = Parts()
    if mat == 'Palm':
        p.add(_rod(f'{prefix}Plate', (-HALF, y, 0.05), (HALF, y, 0.05), 0.05, 'M_Wood', _pick(rnd, 'branch'), rnd),
              'pole')
    elif mat == 'Bamboo':
        p.add(_bamboo(f'{prefix}Plate', (-HALF, y, 0.05), (HALF, y, 0.05), 0.05, rnd, segs=10), 'pole')
    else:
        p.add(_box(f'{prefix}Plate', (GRID, 0.14, 0.1), (0, y, 0.05), 'M_Wood', _pick(rnd, 'wood_dark'), rnd), 'wood')
    return p


def _slope(mat, rnd, v_len, u_range_fn, prefix, rafter_us=(-0.5, 0.5)):
    p = _covering(mat, rnd, v_len, u_range_fn, prefix)
    p.extend(_rafters(mat, rnd, v_len, rafter_us, prefix, u_range_fn))
    return p


@_register('RoofShed')
def _b_roof_shed(mat, rnd, name):
    v_len = GRID / math.cos(ROOF_PITCH)
    p = _slope(mat, rnd, v_len, lambda v: (-HALF, HALF), 'A')
    p.transform(_slope_matrix())
    top = (GRID * math.tan(ROOF_PITCH))
    # remate superior: cumbrera/tapajuntas en el lado alto
    p.extend(_ridge(mat, rnd, (-HALF, HALF - 0.02, top + 0.02), (HALF, HALF - 0.02, top + 0.02), 'R'))
    p.extend(_plate(mat, rnd, -HALF + 0.08, 'P'))
    return p.finish(name)


@_register('RoofGable')
def _b_roof_gable(mat, rnd, name):
    v_len = HALF / math.cos(ROOF_PITCH)
    a = _slope(mat, rnd, v_len + 0.02, lambda v: (-HALF, HALF), 'A')
    a.transform(_slope_matrix())
    b = _slope(mat, rnd, v_len + 0.02, lambda v: (-HALF, HALF), 'B')
    b.transform(Matrix.Scale(-1, 4, (0, 1, 0)) @ _slope_matrix())
    a.extend(b)
    top = HALF * math.tan(ROOF_PITCH)
    a.extend(_ridge(mat, rnd, (-HALF, 0, top + 0.06), (HALF, 0, top + 0.06), 'R'))
    a.extend(_plate(mat, rnd, -HALF + 0.08, 'P0'))
    a.extend(_plate(mat, rnd, HALF - 0.08, 'P1'))
    return a.finish(name)


@_register('RoofCorner')
def _b_roof_corner(mat, rnd, name):
    """Esquina de tejado (limatesa) para cerrar dos tejados a un agua que
    doblan una esquina: aleros en -Y y -X, punto alto en (+1, +1). En su
    borde +X empalma con un RoofShed normal; en su borde +Y, con un RoofShed
    girado 90°. La altura en cada punto es tan(pendiente)·min(x+1, y+1)."""
    v_len = GRID / math.cos(ROOF_PITCH)
    cosp = math.cos(ROOF_PITCH)

    def u_range(v):
        d = v * cosp  # distancia horizontal al alero
        return (min(HALF, d - HALF), HALF)
    a = _slope(mat, rnd, v_len, u_range, 'A', rafter_us=(0.5,))
    a.transform(_slope_matrix())
    b = _slope(mat, rnd, v_len, u_range, 'B', rafter_us=(0.5,))
    swap = Matrix(((0, 1, 0, 0), (1, 0, 0, 0), (0, 0, 1, 0), (0, 0, 0, 1)))
    b.transform(swap @ _slope_matrix())
    a.extend(b)
    top = GRID * math.tan(ROOF_PITCH)
    ov = ROOF_OVERHANG * cosp
    low = (-HALF - ov, -HALF - ov, -ROOF_OVERHANG * math.sin(ROOF_PITCH) + 0.06)
    a.extend(_ridge(mat, rnd, low, (HALF, HALF, top + 0.06), 'H'))
    a.extend(_plate(mat, rnd, -HALF + 0.08, 'P0'))
    pl = _plate(mat, rnd, -HALF + 0.08, 'P1')
    pl.transform(swap)
    a.extend(pl)
    return a.finish(name)


# ---------------------------------------------------------------------------
# HASTIALES — cierran el triángulo que dejan los tejados en sus testeros.
# Misma pared que las demás (corre a lo largo de X, pivote en el centro del
# lado a z=0 = coronación de pared) recortada por el perfil del tejado.
# ---------------------------------------------------------------------------
GABLE_CLEAR = 0.03  # holgura vertical bajo la cobertura (sin z-fighting)


def _clip_parts(p, planes):
    """Recorta todas las piezas por los planos (co, normal) quedándose con
    el semiespacio de detrás de la normal, y tapa los cortes. Cada pieza es
    un volumen cerrado casi convexo, así que el borde de cada corte es un
    polígono sencillo que holes_fill cierra bien. Las tapas copian el color
    de vértice de la cara vecina (si no, salían negras)."""
    for kind, objs in p.kinds.items():
        keep = []
        for o in objs:
            bm = bmesh.new()
            bm.from_mesh(o.data)
            for co, no in planes:
                geom = list(bm.verts) + list(bm.edges) + list(bm.faces)
                bmesh.ops.bisect_plane(bm, geom=geom, plane_co=co, plane_no=no,
                                       clear_outer=True, dist=1e-5)
                if not bm.faces:
                    break
                old = set(bm.faces)
                bmesh.ops.holes_fill(bm, edges=[e for e in bm.edges if e.is_boundary], sides=0)
                col = bm.loops.layers.float_color.get('Col')
                for f in bm.faces:
                    if f in old:
                        continue
                    for lp in f.loops:
                        src = next((l2 for l2 in lp.vert.link_loops if l2.face in old), None)
                        if src is not None and col is not None:
                            lp[col] = src[col]
                        # la tapa hereda también el material de la vecina
                    nb = next((l2.face for lp in f.loops for l2 in lp.vert.link_loops
                               if l2.face in old), None)
                    if nb is not None:
                        f.material_index = nb.material_index
                # los cortes por tiras subdivididas dejan varios vértices
                # alineados en la tapa: triangularla y girar aristas evita
                # triángulos de área nula (fallo de validate.py)
                caps = [f for f in bm.faces if f not in old and len(f.verts) > 3]
                if caps:
                    tri = bmesh.ops.triangulate(bm, faces=caps, quad_method='BEAUTY',
                                                ngon_method='BEAUTY')['faces']
                    bmesh.ops.beautify_fill(bm, faces=tri, edges=list({e for f in tri for e in f.edges
                                                                        if not e.is_boundary}))
            bmesh.ops.dissolve_degenerate(bm, edges=bm.edges, dist=1e-4)
            # descarta astillas: lo que queda por debajo de 3 cm de alto o
            # de 2 cm de ancho en la punta del triángulo
            zs = [v.co.z for v in bm.verts]
            xs = [v.co.x for v in bm.verts]
            if not bm.faces or max(zs) - min(zs) < 0.03 or max(xs) - min(xs) < 0.02:
                bm.free()
                bpy.data.objects.remove(o, do_unlink=True)
                continue
            bm.to_mesh(o.data)
            bm.free()
            o.data.update()
            keep.append(o)
        p.kinds[kind] = keep


def _rake_trim(mat, rnd, p0, p1, prefix):
    """Remate que sigue la pendiente sobre el hastial (tapa los cortes)."""
    p = Parts()
    if mat == 'Palm':
        p.add(_rod(f'{prefix}Rake', p0, p1, 0.045, 'M_Wood', _pick(rnd, 'branch'), rnd), 'pole')
    elif mat == 'Bamboo':
        p.add(_bamboo(f'{prefix}Rake', p0, p1, 0.045, rnd, segs=10), 'pole')
    else:
        # madera: tabla de canto; piedra: dintel inclinado de madera oscura,
        # igual que los cabios del tejado de lajas
        m, length = _axis_matrix(p0, p1)
        t = WALL_T[mat] + 0.04
        o = C.make_box(f'{prefix}Rake', (0.1, t, length), center=(0, 0, length / 2))
        o.data.transform(m)
        M.assign(o, ['M_Wood'])
        _tint(o, _pick(rnd, 'wood_dark'), rnd)
        p.add(o, 'wood')
    return p


def _gable_parts(mat, rnd, profile):
    """profile: lista de (x, z) de la línea de tejado a lo largo de la pared
    (x de -1 a +1). Se construye una pared de la altura máxima y se recorta
    por cada tramo recto del perfil."""
    top = max(z for _, z in profile)
    p = _wall_parts(mat, rnd, top + 0.25, [])
    planes = []
    for (xa, za), (xb, zb) in pairwise(profile):
        d = Vector((xb - xa, 0.0, zb - za)).normalized()
        n = Vector((-d.z, 0.0, d.x))  # normal hacia arriba del tramo
        if n.z < 0:
            n = -n
        planes.append((Vector((xa, 0.0, za)), n))
    _clip_parts(p, planes)
    for i, ((xa, za), (xb, zb)) in enumerate(pairwise(profile)):
        lift = 0.05
        k = _rake_trim(mat, rnd, (xa, 0, za - lift), (xb, 0, zb - lift), f'K{i}')
        # el remate no baja de la coronación: pivote en la base (z = 0)
        _clip_parts(k, [(Vector((0.0, 0.0, 0.0)), Vector((0.0, 0.0, -1.0)))])
        p.extend(k)
    return p


@_register('Gable')
def _b_gable(mat, rnd, name):
    """Hastial para RoofGable: triángulo de 2 m de base y 0,70 m de alto.
    Va bajo cada testero del tejado a dos aguas (girado 90°: la pared corre
    a lo largo de Y del tejado, en x = ±1)."""
    rise = HALF * math.tan(ROOF_PITCH) - GABLE_CLEAR
    return _gable_parts(mat, rnd, [(-HALF, -GABLE_CLEAR), (0.0, rise), (HALF, -GABLE_CLEAR)]).finish(name)


@_register('GableShed')
def _b_gable_shed(mat, rnd, name):
    """Hastial para RoofShed: triángulo rectángulo, bajo en -X y alto en +X
    (1,40 m). Girado +90° queda con su +X hacia el +Y (lado alto) del
    tejado a un agua; en ambos testeros (x = ±1) va con ese mismo giro."""
    rise = GRID * math.tan(ROOF_PITCH) - GABLE_CLEAR
    return _gable_parts(mat, rnd, [(-HALF, -GABLE_CLEAR), (HALF, rise)]).finish(name)


# ---------------------------------------------------------------------------
# ESCALERA (un piso: sube STOREY en STAIR_RUN, 1 m de ancho)
# ---------------------------------------------------------------------------
@_register('Stairs')
def _b_stairs(mat, rnd, name):
    p = Parts()
    n = STAIR_STEPS
    rise = STOREY / n
    run = STAIR_RUN / n
    hw = STAIR_W / 2
    angle_len = math.hypot(STAIR_RUN, STOREY)
    if mat == 'Stone':
        for i in range(n):
            z1 = rise * (i + 1)
            # bloque macizo por peldaño, apoyado en el suelo
            p.add(_box(f'Step{i}', (STAIR_W, run + 0.03, z1), (0, run * i + run / 2, z1 / 2), 'M_Stone',
                       _pick(rnd, 'stone'), rnd, jitter=0.03), 'stone')
        return p.finish(name)
    # zancas
    for i, x in enumerate((-hw + 0.05, hw - 0.05)):
        a, b = (x, -0.05, 0.0), (x, STAIR_RUN, STOREY)
        if mat == 'Palm':
            p.add(_rod(f'Str{i}', a, b, 0.07, 'M_Wood', _pick(rnd, 'branch'), rnd), 'pole')
        elif mat == 'Bamboo':
            p.add(_bamboo(f'Str{i}', a, b, 0.07, rnd, segs=12, node_step=0.45), 'pole')
        else:
            m, length = _axis_matrix(a, b)
            o = C.make_box(f'Str{i}', (0.07, 0.24, angle_len), center=(0, 0.06, angle_len / 2))
            o.data.transform(m)
            M.assign(o, ['M_Wood'])
            _tint(o, _pick(rnd, 'wood_dark'), rnd)
            p.add(o, 'wood')
    for i in range(n):
        y = run * (i + 0.5)
        z = rise * (i + 1)
        if mat == 'Palm':
            p.add(_rod(f'Tr{i}', (-hw - 0.04, y, z - 0.04), (hw + 0.04, y, z - 0.04), 0.045, 'M_Wood',
                       _pick(rnd, 'branch'), rnd), 'pole')
            p.add(_rod(f'Tl{i}', (-hw + 0.05, y, z - 0.07), (-hw + 0.05, y, z - 0.02), 0.08, 'M_Wood',
                       PAL['lashing'], rnd), 'pole')
        elif mat == 'Bamboo':
            for k, dy in enumerate((-0.05, 0.05)):
                p.add(_bamboo(f'Tr{i}_{k}', (-hw - 0.03, y + dy, z - 0.04), (hw + 0.03, y + dy, z - 0.04), 0.04, rnd,
                              segs=10, node_step=0.6), 'pole')
        else:
            p.add(_box(f'Tr{i}', (STAIR_W + 0.04, run * 0.98 + 0.04, 0.05), (0, y, z - 0.025), 'M_Wood',
                       _pick(rnd, 'wood'), rnd), 'wood')
    return p.finish(name)


# ---------------------------------------------------------------------------
# PUERTA (bisagra en x=0, hoja hacia +X)
# ---------------------------------------------------------------------------
@_register('Door')
def _b_door(mat, rnd, name):
    p = Parts()
    w, h = DOOR_LEAF_W, DOOR_LEAF_H
    if mat == 'Palm':
        r = 0.03
        for i, x in enumerate((r, w - r)):
            p.add(_rod(f'Stile{i}', (x, 0, 0.0), (x, 0, h), r, 'M_Wood', _pick(rnd, 'branch'), rnd), 'pole')
        for i, z in enumerate((r, h * 0.5, h - r)):
            p.add(_rod(f'Rail{i}', (0, 0, z), (w, 0, z), r * 0.9, 'M_Wood', _pick(rnd, 'branch'), rnd), 'pole')
        z = h - r
        k = 0
        while z > r:
            za = max(r * 0.3, z - 0.26)
            s = _fringe_slab(f'Th{k}', r * 1.5, w - r * 1.5, za, z, 0.03, rnd, _pick(rnd, 'thatch'),
                             fringe=max(0.0, min(0.05, za - 0.005)))
            s.data.transform(Matrix.Translation((0, 0.012, 0)) @ Matrix.Rotation(math.pi / 2, 4, 'X'))
            p.add(s, 'thatch')
            z -= 0.2
            k += 1
    elif mat == 'Bamboo':
        r = 0.032
        n = int(w / (2 * r * 1.02))
        for i in range(n):
            x = (i + 0.5) * w / n
            p.add(_bamboo(f'Cane{i}', (x, 0, 0), (x, 0, h), r, rnd, segs=8, node_step=0.4), 'pole')
        for i, z in enumerate((0.3, h - 0.3)):
            p.add(_bamboo(f'Bar{i}', (0, -0.055, z), (w, -0.055, z), 0.03, rnd, segs=8), 'pole')
        p.add(_bamboo('Brace', (0.08, -0.055, 0.34), (w - 0.08, -0.055, h - 0.34), 0.026, rnd, segs=8), 'pole')
    else:
        n = 5
        pw = w / n
        dark = mat == 'Stone'
        th = 0.08 if dark else 0.05
        for i in range(n):
            p.add(_box(f'Board{i}', (pw * 0.96, th, h), (pw * (i + 0.5), 0, h / 2), 'M_Wood',
                       _pick(rnd, 'wood_dark' if dark else 'wood'), rnd), 'wood')
        for i, z in enumerate((0.3, h - 0.3)):
            p.add(_box(f'Batten{i}', (w - 0.06, 0.04, 0.14), (w / 2, -th / 2 - 0.02, z), 'M_Wood',
                       _pick(rnd, 'wood'), rnd), 'wood')
        brace_len = math.hypot(w - 0.2, h - 0.74)
        ang = math.atan2(h - 0.74, w - 0.2)
        o = C.make_box('Brace', (brace_len, 0.04, 0.12), center=(0, 0, 0))
        o.data.transform(Matrix.Translation((w / 2, -th / 2 - 0.02, h / 2)) @ Matrix.Rotation(-ang, 4, 'Y'))
        M.assign(o, ['M_Wood'])
        _tint(o, _pick(rnd, 'wood'), rnd)
        p.add(o, 'wood')
        if dark:
            # aldaba de piedra tallada: el nivel "piedra" es la puerta maciza
            p.add(_box('KnobStone', (0.14, 0.06, 0.2), (w - 0.14, -th / 2 - 0.03, h * 0.48), 'M_Stone',
                       _pick(rnd, 'stone'), rnd), 'stone')
    if mat != 'Stone':
        p.add(_rod('Pull', (w - 0.12, -0.05, h * 0.45), (w - 0.12, -0.05, h * 0.55), 0.02, 'M_Wood',
                   PAL['lashing'], rnd, segs=8), 'pole')
    return p.finish(name)


# ---------------------------------------------------------------------------
# BARANDILLA (2 m a lo largo de X, sobre la cara del suelo)
# ---------------------------------------------------------------------------
@_register('Railing')
def _b_railing(mat, rnd, name):
    p = Parts()
    h = RAIL_H
    x0, x1 = -HALF + 0.06, HALF - 0.06
    if mat == 'Palm':
        for i, x in enumerate((x0, 0.0, x1)):
            p.add(_rod(f'Post{i}', (x, 0, 0), (x, 0, h), 0.045, 'M_Wood', _pick(rnd, 'branch'), rnd, taper=0.85),
                  'pole')
        for i, z in enumerate((h - 0.05, h * 0.5)):
            p.add(_rod(f'Rail{i}', (-HALF, 0.0, z), (HALF, 0.0, z), 0.035 if i else 0.042, 'M_Wood',
                       _pick(rnd, 'branch'), rnd), 'pole')
        for i, x in enumerate((x0, 0.0, x1)):
            for k, z in enumerate((h - 0.05, h * 0.5)):
                p.add(_rod(f'L{i}_{k}', (x, 0, z - 0.03), (x, 0, z + 0.03), 0.06, 'M_Wood', PAL['lashing'], rnd),
                      'pole')
    elif mat == 'Bamboo':
        for i, x in enumerate((x0, 0.0, x1)):
            p.add(_bamboo(f'Post{i}', (x, 0, 0), (x, 0, h), 0.05, rnd, segs=10, node_step=0.35), 'pole')
        for i, z in enumerate((h - 0.03, h * 0.55, 0.12)):
            p.add(_bamboo(f'Rail{i}', (-HALF, -0.07, z), (HALF, -0.07, z), 0.035, rnd, segs=10), 'pole')
        for i in range(8):
            x = -HALF + (i + 0.5) * GRID / 8
            if abs(x) < 0.08 or abs(abs(x) - (HALF - 0.06)) < 0.08:
                continue
            p.add(_bamboo(f'Bal{i}', (x, -0.07, 0.12), (x, -0.07, h * 0.55), 0.022, rnd, segs=8), 'pole')
    elif mat == 'Wood':
        for i, x in enumerate((x0, x1)):
            p.add(_box(f'Post{i}', (0.1, 0.1, h), (x, 0, h / 2), 'M_Wood', _pick(rnd, 'wood_dark'), rnd), 'wood')
        p.add(_box('Hand', (GRID, 0.14, 0.06), (0, 0, h + 0.03), 'M_Wood', _pick(rnd, 'wood'), rnd), 'wood')
        p.add(_box('Bottom', (GRID - 0.2, 0.08, 0.06), (0, 0, 0.12), 'M_Wood', _pick(rnd, 'wood'), rnd), 'wood')
        for i in range(9):
            x = x0 + 0.05 + (i + 1) * (x1 - x0 - 0.1) / 10
            p.add(_box(f'Bal{i}', (0.05, 0.05, h - 0.15), (x, 0, 0.15 + (h - 0.15) / 2), 'M_Wood',
                       _pick(rnd, 'wood'), rnd), 'wood')
    else:
        # balaustrada baja de piedra: pilares, balaustres rechonchos, albardilla
        for i, x in enumerate((x0 + 0.07, x1 - 0.07)):
            p.add(_box(f'Pier{i}', (0.26, 0.26, h - 0.1), (x, 0, (h - 0.1) / 2), 'M_Stone', _pick(rnd, 'stone'), rnd),
                  'stone')
        p.add(_box('Base', (GRID - 0.02, 0.24, 0.16), (0, 0, 0.08), 'M_Stone', _pick(rnd, 'stone'), rnd), 'stone')
        x = -HALF
        k = 0
        while x < HALF - 1e-4:
            ln = min(HALF - x, rnd.uniform(0.6, 0.8))
            if HALF - (x + ln) < 0.3:
                ln = HALF - x
            p.add(_box(f'Coping{k}', (ln - 0.02, 0.3, 0.12), (x + ln / 2, 0, h - 0.06), 'M_Stone',
                       _pick(rnd, 'stone'), rnd), 'stone')
            x += ln
            k += 1
        n = 5
        for i in range(n):
            x = (x0 + 0.26) + (i + 0.5) * ((x1 - 0.26) - (x0 + 0.26)) / n
            p.add(_rod(f'Bal{i}', (x, 0, 0.16), (x, 0, h - 0.12), 0.07, 'M_Stone', _pick(rnd, 'stone'), rnd,
                       segs=10, taper=0.7), 'slab')
    return p.finish(name)
