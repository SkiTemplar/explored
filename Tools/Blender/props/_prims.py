"""
_prims.py — primitivas de malla con caras etiquetadas y pintor por cara,
compartidas por los kits de props hechos a medida (limon.py, albatros.py).

- loft / sweep / slab / cloth devuelven (obj, tags): tags[i] describe la cara
  i de la malla (anillo, segmento, tapa...) para colorearla después.
- paint escribe el color de vértice «Col» POR CARA (todas las esquinas de una
  cara con el mismo color): franjas, ventanillas y tablones quedan nítidos
  sin subdividir la malla.
"""

import math
import os
import sys

import bpy

import bmesh
from mathutils import Vector

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import _materials as M  # noqa: E402
import common as C  # noqa: E402


def _clamp01(x):
    return max(0.0, min(1.0, x))

# ---------------------------------------------------------------------------
# 2. Primitivas con caras etiquetadas
# ---------------------------------------------------------------------------

def _obj_from_bm(name, bm):
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new(name, me)
    C.link_object(obj)
    return obj


def loft(name, rings, wrap_around=True, wrap_rings=False, caps=True):
    """Une anillos de puntos consecutivos con quads. Devuelve (obj, tags) con
    tags[i] = ('side', anillo, segmento) | ('cap0',) | ('cap1',) en el orden
    de las caras de la malla (to_mesh conserva el orden de creación)."""
    bm = bmesh.new()
    vr = [[bm.verts.new(Vector(p)) for p in ring] for ring in rings]
    n, n_r = len(vr[0]), len(vr)
    tags = []
    spans = n_r if wrap_rings else n_r - 1
    segs = n if wrap_around else n - 1
    for r in range(spans):
        a, b = vr[r], vr[(r + 1) % n_r]
        for i in range(segs):
            i2 = (i + 1) % n
            bm.faces.new((a[i], a[i2], b[i2], b[i]))
            tags.append(('side', r, i))
    if caps and not wrap_rings:
        bm.faces.new(list(reversed(vr[0])))
        tags.append(('cap0',))
        bm.faces.new(vr[-1])
        tags.append(('cap1',))
    return _obj_from_bm(name, bm), tags


def sweep(name, path, radius, sides=6, closed=False, roll=0.0, up=(0.0, 0.0, 1.0)):
    """Barre un polígono regular de `sides` lados a lo largo de `path`
    (marco por transporte paralelo). `radius` escalar o uno por punto.
    roll = pi/4 con sides = 4 da una sección cuadrada alineada con `up`."""
    pts = [Vector(p) for p in path]
    n = len(pts)
    radii = list(radius) if isinstance(radius, (list, tuple)) else [radius] * n
    tans = []
    for i in range(n):
        if closed:
            d = pts[(i + 1) % n] - pts[(i - 1) % n]
        else:
            d = pts[min(i + 1, n - 1)] - pts[max(i - 1, 0)]
        tans.append(d.normalized())
    upv = Vector(up)
    if abs(tans[0].dot(upv)) > 0.9:
        upv = Vector((1.0, 0.0, 0.0)) if abs(tans[0].x) < 0.9 else Vector((0.0, 1.0, 0.0))
    nrm = (upv - tans[0] * upv.dot(tans[0])).normalized()
    rings = []
    for i in range(n):
        if i:
            nrm = tans[i - 1].rotation_difference(tans[i]) @ nrm
            nrm = (nrm - tans[i] * nrm.dot(tans[i])).normalized()
        bnm = tans[i].cross(nrm)
        ring = []
        for k in range(sides):
            a = roll + 2.0 * math.pi * k / sides
            ring.append(pts[i] + (nrm * math.cos(a) + bnm * math.sin(a)) * radii[i])
        rings.append(ring)
    return loft(name, rings, wrap_around=True, wrap_rings=closed, caps=not closed)


def slab(name, pts2d, thickness, to3d):
    """Placa: polígono 2D extruido `thickness` (centrado). to3d(a, b, d)
    devuelve el Vector 3D del punto (a, b) a la profundidad d."""
    h = 0.5 * thickness
    r0 = [to3d(a, b, -h) for a, b in pts2d]
    r1 = [to3d(a, b, h) for a, b in pts2d]
    return loft(name, [r0, r1], wrap_around=True, caps=True)


def xz(y0=0.0):
    """Mapeo de slab() al plano XZ (grosor en Y alrededor de y0)."""
    return lambda a, b, d: Vector((a, y0 + d, b))


def cloth(name, pfn, nu, nv, thickness, back_dir):
    """Lona con grosor: rejilla nu x nv de pfn(u, v) -> Vector, cara trasera
    desplazada `thickness` según back_dir y canto cerrado. Tags:
    ('front', i, j) | ('back', i, j) | ('edge',)."""
    bm = bmesh.new()
    off = Vector(back_dir).normalized() * thickness
    fr = [[bm.verts.new(pfn(i / nu, j / nv)) for j in range(nv + 1)] for i in range(nu + 1)]
    bk = [[bm.verts.new(fr[i][j].co + off) for j in range(nv + 1)] for i in range(nu + 1)]
    tags = []
    for i in range(nu):
        for j in range(nv):
            bm.faces.new((fr[i][j], fr[i + 1][j], fr[i + 1][j + 1], fr[i][j + 1]))
            tags.append(('front', i, j))
            bm.faces.new((bk[i][j + 1], bk[i + 1][j + 1], bk[i + 1][j], bk[i][j]))
            tags.append(('back', i, j))
    loop = ([(i, 0) for i in range(nu + 1)] + [(nu, j) for j in range(1, nv + 1)]
            + [(i, nv) for i in range(nu - 1, -1, -1)] + [(0, j) for j in range(nv - 1, 0, -1)])
    for a, b in zip(loop, loop[1:] + loop[:1], strict=True):
        bm.faces.new((fr[a[0]][a[1]], fr[b[0]][b[1]], bk[b[0]][b[1]], bk[a[0]][a[1]]))
        tags.append(('edge',))
    return _obj_from_bm(name, bm), tags


# ---------------------------------------------------------------------------
# Pintor por cara y acabado
# ---------------------------------------------------------------------------

def paint(obj, slots, face_fn, rnd=None, jitter=0.0):
    """Asigna los slots (mismo orden en todas las piezas, para que el join
    no los reordene) y escribe «Col» por cara: face_fn(poly) -> (slot, rgb).
    Alfa 0, como el resto del kit de props."""
    M.assign(obj, slots)
    me = obj.data
    if 'Col' in me.color_attributes:
        me.color_attributes.remove(me.color_attributes['Col'])
    attr = me.color_attributes.new(name='Col', type='FLOAT_COLOR', domain='CORNER')
    for poly in me.polygons:
        slot, rgb = face_fn(poly)
        poly.material_index = slots.index(slot)
        j = rnd.uniform(-jitter, jitter) if (rnd is not None and jitter) else 0.0
        col = (_clamp01(rgb[0] + j), _clamp01(rgb[1] + j), _clamp01(rgb[2] + j), 0.0)
        for li in poly.loop_indices:
            attr.data[li].color = col
    me.color_attributes.active_color_name = 'Col'
    me.color_attributes.render_color_index = list(me.color_attributes).index(attr)
    return obj


def solid(obj, slots, slot, rgb, rnd=None, jitter=0.02):
    return paint(obj, slots, lambda _p: (slot, rgb), rnd=rnd, jitter=jitter)


def smooth(obj, angle_deg):
    """Sombreado suave por ángulo, horneado en la pieza ANTES del join: cada
    pieza lleva su ángulo (redondas a 65 grados, cajas y casco a 35)."""
    C.shade_smooth_auto(obj, angle_deg=angle_deg)
    return obj


def translate_mesh(obj, offset):
    """Desplaza los vértices (no el objeto): el origen se queda en 0."""
    from mathutils import Matrix
    obj.data.transform(Matrix.Translation(Vector(offset)))
    obj.data.update()
    return obj
