"""
_items.py — helpers compartidos por las mallas de objetos de inventario
(items_herramientas.py, items_materiales.py): palos torcidos, ataduras en
hélice, lascas talladas, conchas de valva y utilidades de pivote.

Convención de los objetos de items.json:

* Nombre de malla ``SM_Item_<IdEnPascalCase>`` (``lasca_obsidiana`` ->
  ``SM_Item_LascaObsidiana``); el asset de Unreal queda en
  ``/Game/Generated/Meshes/Items/SM_Item_<Id>.SM_Item_<Id>`` (grupo
  ``Items`` de run_props.py).
* Escala real en metros.
* Objetos sueltos (materiales): pivote en la base (z = 0), tumbados como
  quedarían en el suelo.
* Herramientas: pivote en el PUNTO DE AGARRE (socket de mano). El mango va
  a lo largo de +Z (la cabeza/punta hacia +Z) y el filo o la cara de golpe
  mira a +X. En Unreal se enganchan tal cual al socket ``hand_r`` con
  transformación relativa identidad (o la rotación fija del socket del
  esqueleto del jugador, igual para todas).
"""

import math
import os
import sys
from typing import cast

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import bpy  # noqa: E402

import _materials as M  # noqa: E402
import common as C  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402
from mathutils import noise as mnoise

GROUP = 'Items'

# Colores lineales (se multiplican por los materiales casi blancos del kit).
PAL = {
    'bark': [(0.16, 0.09, 0.045), (0.19, 0.11, 0.05), (0.14, 0.08, 0.04)],
    'bark_green': [(0.09, 0.16, 0.035), (0.11, 0.19, 0.04)],
    'wood_cut': (0.55, 0.36, 0.16),
    'wood_core': (0.34, 0.18, 0.07),
    'wood_light': [(0.42, 0.26, 0.11), (0.36, 0.22, 0.09)],
    'wood_handle': [(0.26, 0.13, 0.05), (0.22, 0.11, 0.045)],
    'char': (0.03, 0.018, 0.012),
    'bamboo': [(0.28, 0.36, 0.06), (0.34, 0.40, 0.08), (0.40, 0.38, 0.10)],
    'bamboo_node': (0.17, 0.23, 0.04),
    'bamboo_cut': (0.66, 0.58, 0.30),
    'cord': [(0.45, 0.30, 0.12), (0.36, 0.23, 0.09)],
    'cord_red': (0.40, 0.07, 0.02),
    'coir': [(0.28, 0.13, 0.04), (0.36, 0.18, 0.06), (0.22, 0.10, 0.03)],
    'vine': [(0.12, 0.16, 0.04), (0.16, 0.13, 0.05)],
    'leaf': (0.05, 0.22, 0.03),
    'leaf_tip': (0.24, 0.42, 0.06),
    'flint': (0.30, 0.12, 0.035),
    'flint_edge': (0.58, 0.34, 0.12),
    'flint_cortex': (0.55, 0.44, 0.27),
    'obsidian': (0.012, 0.010, 0.018),
    'obsidian_edge': (0.12, 0.06, 0.17),
    # «piedra verde» pulida de azuela polinesia: nunca gris
    'greenstone': (0.05, 0.10, 0.075),
    'greenstone_edge': (0.20, 0.32, 0.24),
    'cobble': [(0.20, 0.13, 0.08), (0.10, 0.07, 0.05), (0.24, 0.15, 0.08)],
    'cobble_light': (0.32, 0.22, 0.13),
    'quartz': (0.80, 0.74, 0.64),
    'shell_out': (0.55, 0.42, 0.26),
    'shell_rib': (0.34, 0.21, 0.10),
    'shell_in': (0.50, 0.42, 0.32),
    'shell_in_rib': (0.38, 0.27, 0.19),
    'straw': (0.58, 0.40, 0.14),
    'resin': (0.22, 0.08, 0.015),
    'pitch': (0.09, 0.035, 0.01),
}


def tint(o, rgb, rnd, j=0.03):
    C.set_vertex_colors(o, C.constant_tint(rgb, jitter=j, rnd=rnd))


def color_fn(o, fn, rnd=None, j=0.0):
    """Pinta con fn(co) -> rgb, con jitter por vértice cacheado."""
    cache = {}

    def f(v):
        if v.index not in cache:
            jj = rnd.uniform(-j, j) if (rnd is not None and j) else 0.0
            c = fn(v.co)
            cache[v.index] = tuple(max(0.0, min(1.0, x + jj)) for x in c) + (0.0,)
        return cache[v.index]
    C.set_vertex_colors(o, f)


def lerp3(a, b, t):
    t = max(0.0, min(1.0, t))
    return tuple(a[i] * (1 - t) + b[i] * t for i in range(3))


def sweep(name, pts, radii, segs=8, cap=True):
    """Tubo a lo largo de una polilínea 3D con marco transportado (no se
    retuerce). radii: float o lista (un radio por punto)."""
    pts = [Vector(p) for p in pts]
    rings = []
    prev_n = None
    for i, p in enumerate(pts):
        t = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
        if prev_n is None:
            ref = Vector((0, 0, 1)) if abs(t.z) < 0.9 else Vector((1, 0, 0))
            n = (ref - t * ref.dot(t)).normalized()
        else:
            n = (prev_n - t * prev_n.dot(t)).normalized()
        prev_n = n
        # los stubs tipan cross() como Vector | float (caso 2D); aquí es 3D
        b = cast(Vector, t.cross(n))
        r = radii[i] if isinstance(radii, (list, tuple)) else radii
        rings.append([p + (n * math.cos(2 * math.pi * k / segs) + b * math.sin(2 * math.pi * k / segs)) * r
                      for k in range(segs)])
    return C.ring_loft(name, rings, cap_start=cap, cap_end=cap)


def crooked(length, rnd, n=8, bend=0.03, wobble=0.012, z0=0.0):
    """Puntos de un palo algo torcido a lo largo de +Z (arco suave + ruido)."""
    ang = rnd.uniform(0, 2 * math.pi)
    dx, dy = math.cos(ang), math.sin(ang)
    pts = []
    for i in range(n + 1):
        t = i / n
        arc = math.sin(t * math.pi) * bend * length
        w = (rnd.uniform(-1, 1) * wobble * length) if 0 < i < n else 0.0
        pts.append((dx * arc + w * -dy, dy * arc + w * dx, z0 + t * length))
    return pts


def stick(name, length, r0, r1, rnd, n=8, bend=0.03, wobble=0.01, segs=8, z0=0.0, knots=0):
    """Palo/rama ahusado y algo torcido a lo largo de +Z; `knots` engordes
    sueltos (nudos) a lo largo."""
    pts = crooked(length, rnd, n=n, bend=bend, wobble=wobble, z0=z0)
    knot_at = {rnd.randint(1, n - 1) for _ in range(knots)}
    radii = [(r0 + (r1 - r0) * i / n) * (1.25 if i in knot_at else 1.0) * rnd.uniform(0.95, 1.05)
             for i in range(n + 1)]
    return sweep(name, pts, radii, segs=segs), pts


def helix_wrap(name, z0, z1, radius, cord_r, rnd, turns=None, segs=5, center=(0.0, 0.0)):
    """Atadura: cordel enrollado en hélice alrededor del eje Z."""
    if turns is None:
        turns = max(2.0, (z1 - z0) / (cord_r * 2.1))
    steps = max(8, int(turns * 10))
    a0 = rnd.uniform(0, 2 * math.pi)
    pts = []
    for i in range(steps + 1):
        t = i / steps
        a = a0 + t * turns * 2 * math.pi
        pts.append((center[0] + math.cos(a) * radius, center[1] + math.sin(a) * radius, z0 + (z1 - z0) * t))
    return sweep(name, pts, cord_r, segs=segs)


def cord_stripes(o, base, dark, segs, period=3):
    """Rayas diagonales de cordel retorcido (sweep de `segs` lados)."""
    def f(v):
        ring, k = divmod(v.index, segs)
        c = dark if ((ring + k) % period) == 0 else base
        return tuple(c) + (0.0,)
    C.set_vertex_colors(o, f)


# aristas de talla por vértice de cada lasca (0..1), para pintarlas más claras
FLAKE_RIDGES = {}


def flake(name, length, width, thick, seed, point=0.7, scars=7, scar_depth=0.4):
    """Lasca tallada: lente biconvexa con bordes finos, ahusada a +Z hasta
    una punta. La cara dorsal (+Y) lleva `scars` cicatrices de talla
    (cuencos poco profundos que se cortan en aristas, como el lascado
    concoide real); la ventral (-Y) es casi plana con el bulbo de percusión
    en la base. Largo en Z, ancho en X, grosor en Y; centrada en el origen."""
    import random
    rnd = random.Random(seed)
    top = [(rnd.uniform(-0.7, 0.7), rnd.uniform(-0.85, 0.85), rnd.uniform(0.4, 0.65)) for _ in range(scars)]
    o = C.make_blob(name, (0, 0, 0), radius=1.0, seed=seed, subdivisions=4, noise_strength=0.0)
    ridge = {}
    for v in o.data.vertices:
        x, y, z = v.co
        x *= 1.0 - point * max(0.0, z) ** 1.4
        rr = min(1.0, x * x + z * z)
        h = (1.0 - rr) ** 0.55 * 0.92 + 0.08
        if y > 0:
            cuts = sorted((scar_depth * (1.0 - ((x - cx) ** 2 + (z - cz) ** 2) / (R * R)) for cx, cz, R in top),
                          reverse=True)
            cut = max(0.0, cuts[0])
            # arista: donde dos cicatrices casi empatan
            ridge[v.index] = max(0.0, 1.0 - abs(cuts[0] - cuts[1]) / 0.12) if cuts[1] > 0 else 0.0
            y = y * h * (1.0 - cut)
        else:
            # ventral casi plana, bulbo de percusión cerca del talón (-Z)
            bulb = 0.35 * math.exp(-((x / 0.35) ** 2 + ((z + 0.75) / 0.3) ** 2))
            y = y * h * (0.35 + bulb)
        v.co = Vector((x * width / 2, y * thick / 2, z * length / 2))
    o.data.update()
    FLAKE_RIDGES[o.name] = ridge
    return o


def clam_shell(name, width, height, depth, ribs=5, thick=0.006, segs_u=22, segs_v=8, scallop=0.06, rib_depth=0.22):
    """Valva de almeja gigante (Tridacna): abanico con costillas onduladas y
    borde festoneado. Charnela en el origen, crece hacia +Z, convexa hacia
    -Y (cara exterior), con grosor real (solidify)."""
    import bmesh
    bm = bmesh.new()
    grid = []
    spread = math.radians(62)
    for j in range(segs_v + 1):
        v = 0.06 + 0.94 * j / segs_v
        row = []
        for i in range(segs_u + 1):
            u = i / segs_u * 2 - 1
            th = u * spread
            wave = math.cos(u * (ribs + 0.5) * math.pi)
            r = v * (1.0 + scallop * wave * v ** 3)
            x = math.sin(th) * r * width / 2 / math.sin(spread)
            z = math.cos(th) * r * height
            bulge = depth * (1 - u * u) ** 0.7 * math.sin(min(1.0, v * 1.15) * math.pi * 0.62)
            rib = depth * rib_depth * wave * v
            row.append(bm.verts.new((x, -(bulge + rib), z)))
        grid.append(row)
    for j in range(segs_v):
        for i in range(segs_u):
            bm.faces.new((grid[j][i], grid[j][i + 1], grid[j + 1][i + 1], grid[j + 1][i]))
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-6)
    bmesh.ops.dissolve_degenerate(bm, edges=bm.edges, dist=1e-6)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new(name, me)
    C.link_object(o)
    solidify(o, thick)
    return o


def solidify(o, thick):
    C.select_only(o)
    bpy.context.view_layer.objects.active = o
    mod = o.modifiers.new('Solid', 'SOLIDIFY')
    mod.thickness = thick
    mod.offset = 0.0
    mod.use_even_offset = True
    bpy.ops.object.modifier_apply(modifier=mod.name)
    return o


def leaf(name, length, width, curve, rnd, segments=6, fold=0.25, thick=0.0015, rgb0=None, rgb1=None):
    """Hoja lanceolada con nervio central plegado en V suave y un poco de
    grosor (se ve por las dos caras sin material de doble cara). Crece a lo
    largo de +Y desde el origen (peciolo), ancho en X, se curva hacia -Z."""
    rgb0 = rgb0 or PAL['leaf']
    rgb1 = rgb1 or PAL['leaf_tip']
    rows = []
    for i in range(segments + 1):
        t = i / segments
        w = width * 0.5 * math.sin(math.pi * min(1.0, t * 0.95 + 0.03)) ** 0.8
        y = length * t
        zc = -curve * length * t * t
        rows.append([Vector((-w, y, zc + w * fold)), Vector((0.0, y, zc)), Vector((w, y, zc + w * fold))])
    import bmesh
    bm = bmesh.new()
    vs = [[bm.verts.new(p) for p in r] for r in rows]
    for i in range(segments):
        for k in range(2):
            bm.faces.new((vs[i][k], vs[i][k + 1], vs[i + 1][k + 1], vs[i + 1][k]))
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-6)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new(name, me)
    C.link_object(o)
    solidify(o, thick)
    M.assign(o, ['M_Leaf'])
    C.set_vertex_colors(o, lambda v: lerp3(rgb0, rgb1, v.co.y / length + 0.15 * abs(v.co.x) / max(width, 1e-4))
                        + (0.0,))
    return o


def lay_along_x(parts):
    """Tumba una pieza construida a lo largo de +Z para que quede en el
    suelo a lo largo de +X."""
    parts.transform(Matrix.Rotation(math.pi / 2, 4, 'Y'))


def pivot_at(obj, point):
    """Pone el pivote en `point` (coordenadas locales de la malla)."""
    obj.data.transform(Matrix.Translation(-Vector(point)))
    obj.data.update()
    return obj


def ground_centered(obj):
    """Pivote en la base, centrado en XY (objetos sueltos en el suelo)."""
    xs = [v.co.x for v in obj.data.vertices]
    ys = [v.co.y for v in obj.data.vertices]
    zs = [v.co.z for v in obj.data.vertices]
    return pivot_at(obj, ((min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2, min(zs)))


# ---------------------------------------------------------------------------
# helpers de contenedores (items_contenedores.py, items_naturales.py)
# ---------------------------------------------------------------------------
def lathe(name, prof, segs=24, sx=1.0, sy=1.0, arc=None):
    """Sólido de revolución alrededor de Z a partir de un perfil (r, z).
    Si el primer/último punto tiene r = 0 se cierra con un polo (abanico de
    triángulos). Un perfil que sube por fuera, pasa el labio y baja por
    dentro hasta el eje da un recipiente cerrado con pared de grosor real.
    sx/sy aplastan la sección (ovalada). El índice de vértice sigue
    anillo * segs + k (el polo inicial, si lo hay, va el primero).
    `arc` (radianes, < 2π) gira solo ese ángulo desde +X y cierra el corte
    con una cara plana triangulada (media luna de un hongo yesquero); en
    ese caso cada anillo tiene segs + 1 vértices."""
    import bmesh
    bm = bmesh.new()
    closed = arc is None
    npts = segs if closed else segs + 1
    step = (2 * math.pi / segs) if closed else (arc / segs)
    rings = []
    poles = {}
    for j, (r, z) in enumerate(prof):
        if r <= 1e-6 and j in (0, len(prof) - 1):
            poles[j] = bm.verts.new((0.0, 0.0, z))
            rings.append(None)
            continue
        rings.append([bm.verts.new((math.cos(step * k) * r * sx, math.sin(step * k) * r * sy, z))
                      for k in range(npts)])
    for j in range(len(prof) - 1):
        a, b = rings[j], rings[j + 1]
        for k in range(segs):
            k2 = (k + 1) % npts
            if a is None:
                bm.faces.new((poles[j], b[k], b[k2]))
            elif b is None:
                bm.faces.new((a[k], poles[j + 1], a[k2]))
            else:
                bm.faces.new((a[k], b[k], b[k2], a[k2]))
    if not closed:
        boundary = [e for e in bm.edges if e.is_boundary]
        res = bmesh.ops.holes_fill(bm, edges=boundary, sides=0)
        bmesh.ops.triangulate(bm, faces=res['faces'])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new(name, me)
    C.link_object(o)
    return o


def soft_box(name, size, center=(0.0, 0.0, 0.0), roundness=0.35, cuts=4):
    """Caja de esquinas redondeadas (almohadilla de lona, cuero, bloque):
    cubo subdividido proyectado sobre una caja con esquinas de radio
    `roundness` (0..1, fracción de la media arista). Escala real `size`."""
    import bmesh
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=2.0)
    bmesh.ops.subdivide_edges(bm, edges=bm.edges[:], cuts=cuts, use_grid_fill=True)
    inner = 1.0 - roundness
    for v in bm.verts:
        c = Vector(v.co)
        q = Vector((max(-inner, min(inner, c.x)), max(-inner, min(inner, c.y)), max(-inner, min(inner, c.z))))
        d = c - q
        if d.length > 1e-6:
            c = q + d.normalized() * roundness
        v.co = Vector((c.x * size[0] / 2 + center[0], c.y * size[1] / 2 + center[1], c.z * size[2] / 2 + center[2]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new(name, me)
    C.link_object(o)
    return o


def weave_col(a, b, n_around, pitch, z0=0.0, cx=0.0, cy=0.0, blend=0.15):
    """Color de cestería: damero de tiras (n_around tiras en la vuelta, alto
    `pitch`) alrededor del eje Z que pasa por (cx, cy)."""
    def f(co):
        ang = (math.atan2(co.y - cy, co.x - cx) / (2 * math.pi) + 0.5) * n_around
        row = (co.z - z0) / pitch
        chk = (int(math.floor(ang)) + int(math.floor(row))) % 2
        # dentro de cada tira, un poco más oscuro en los bordes (volumen)
        edge = abs((ang % 1.0) - 0.5) * 2 if chk else abs((row % 1.0) - 0.5) * 2
        c = b if chk else a
        return lerp3(c, lerp3(c, (0.05, 0.03, 0.01), 0.6), max(0.0, edge - 0.6) / 0.4 * blend * 3)
    return f


def weave_bump(o, n_around, pitch, amp, z0=0.0, zmin=-1e9, zmax=1e9, cx=0.0, cy=0.0):
    """Relieve de trenzado: empuja hacia fuera (radial en XY) las casillas
    pares del damero de weave_col."""
    for v in o.data.vertices:
        co = v.co
        if not (zmin <= co.z <= zmax):
            continue
        d = Vector((co.x - cx, co.y - cy, 0.0))
        if d.length < 1e-5:
            continue
        ang = (math.atan2(d.y, d.x) / (2 * math.pi) + 0.5) * n_around
        row = (co.z - z0) / pitch
        s = math.sin(ang * math.pi) * math.sin(row * math.pi)
        v.co += d.normalized() * amp * s
    o.data.update()


def smooth_profile(pts, per_seg=3):
    """Remuestrea un perfil (r, z) con Catmull-Rom (per_seg puntos por
    tramo) para que el torno tenga anillos suficientes donde se pintan
    bandas finas, sin perder los puntos de control."""
    out = []
    n = len(pts)
    for i in range(n - 1):
        p0 = pts[max(i - 1, 0)]
        p1, p2 = pts[i], pts[i + 1]
        p3 = pts[min(i + 2, n - 1)]
        for s in range(per_seg):
            t = s / per_seg
            t2, t3 = t * t, t * t * t
            out.append(tuple(0.5 * ((2 * p1[k]) + (-p0[k] + p2[k]) * t + (2 * p0[k] - 5 * p1[k] + 4 * p2[k] - p3[k]) * t2
                                    + (-p0[k] + 3 * p1[k] - 3 * p2[k] + p3[k]) * t3) for k in range(2)))
    out.append(pts[-1])
    return [(max(0.0, r), z) for r, z in out]


def lathe_ring(v_index, segs, has_pole=True):
    """Índice de punto del perfil al que pertenece un vértice de lathe()."""
    if has_pole:
        return 0 if v_index == 0 else (v_index - 1) // segs + 1
    return v_index // segs


def smooth_all(obj):
    """Sombreado suave en toda la malla, borrando las aristas marcadas como
    duras por finish() (conchas y piezas orgánicas de costillas suaves)."""
    C.select_only(obj)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.shade_smooth(keep_sharp_edges=False)
    return obj


# ---------------------------------------------------------------------------
# helpers de barras y perfiles (items_rescatados.py, items_recursos.py)
# ---------------------------------------------------------------------------
def rrect(w, t, r, corner_segs=3, side_pts=2):
    """Perfil de rectángulo redondeado (ancho `w` en el primer eje, alto
    `t` en el segundo, radio de esquina `r` ABSOLUTO) como lista de
    (u, v) en sentido antihorario. `side_pts` puntos intermedios por lado
    recto, para poder pintar bandas finas en las caras."""
    r = min(r, w / 2 * 0.99, t / 2 * 0.99)
    hu, hv = w / 2 - r, t / 2 - r
    corners = [(hu, hv, 0.0), (-hu, hv, math.pi / 2), (-hu, -hv, math.pi), (hu, -hv, 1.5 * math.pi)]
    pts = []
    for ci, (cu, cv, a0) in enumerate(corners):
        for k in range(corner_segs + 1):
            a = a0 + (math.pi / 2) * k / corner_segs
            pts.append((cu + math.cos(a) * r, cv + math.sin(a) * r))
        # lado recto hasta la siguiente esquina
        nu, nv, na = corners[(ci + 1) % 4]
        end = (pts[-1][0], pts[-1][1])
        nxt = (nu + math.cos(na) * r, nv + math.sin(na) * r)
        for s in range(1, side_pts + 1):
            f = s / (side_pts + 1)
            pts.append((end[0] + (nxt[0] - end[0]) * f, end[1] + (nxt[1] - end[1]) * f))
    return pts


def loft_x(name, xs, section, cap=True):
    """Barra a lo largo de +X: `section(x)` devuelve la lista de puntos
    (y, z) de la sección en esa x (todas con el mismo número de puntos).
    El índice de vértice sigue anillo * n + k, útil para pintar."""
    rings = [[Vector((x, y, z)) for (y, z) in section(x)] for x in xs]
    return C.ring_loft(name, rings, cap_start=cap, cap_end=cap)


def noise3(co, scale, seed=0.0):
    """Ruido Perlin -1..1 en una posición (para manchas y vetas)."""
    return mnoise.noise(Vector((co.x * scale + seed, co.y * scale - seed * 0.7, co.z * scale + seed * 1.3)))
