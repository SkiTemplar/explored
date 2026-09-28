"""
_cliffkit.py — utilidades compartidas del kit de rocas y acantilados
(Tools/Blender/props/rocks_cliffs.py) para las piezas APROBADAS de
AcantiladoBloques: bloques/cantos (envolvente convexa de puntos en espiral
de Fibonacci) y losas de caliza (anillo irregular extruido), mas
`finish_rock`/`rock_tint`, compartidos con las piezas de AcantiladoFormaciones
basadas en escaneo (ver _cliffscan.py).

Las paredes/espolones/farallones/arco procedurales por bmesh que vivian
aqui se rechazaron en la revision de arte 2026-09-27 ("se leen como
geometria procedural, no rocas naturales") y se sustituyeron por el
pipeline de _cliffscan.py (escaneos CC0 de Poly Haven estilizados).

Vive en Tools/Blender/props/ (no toca common.py, _shapes.py ni ningun
fichero de vegetacion: es aditivo, solo para este kit, igual que
_materials.py).
"""

import math
import os
import random
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import bpy  # noqa: E402

import _shapes as S  # noqa: E402
import bmesh  # noqa: E402
import common as C  # noqa: E402
from mathutils import Matrix, Vector  # noqa: E402

# ---------------------------------------------------------------------------
# Anillos de radio irregular (columnas: farallones, espolones, pilares de
# arco) y remates.
# ---------------------------------------------------------------------------

def ring_verts(bm, z, radius, sides, rnd, radius_jitter=0.22, offset=(0.0, 0.0),
               angle_offset=0.0, squash=(1.0, 1.0)):
    """Anillo de vertices a una altura z con radio irregular: una columna
    rocosa real nunca tiene seccion circular perfecta."""
    verts = []
    for i in range(sides):
        ang = angle_offset + 2.0 * math.pi * i / sides
        r = max(0.02, radius * (1.0 + rnd.uniform(-radius_jitter, radius_jitter)))
        x = offset[0] + math.cos(ang) * r * squash[0]
        y = offset[1] + math.sin(ang) * r * squash[1]
        verts.append(bm.verts.new(Vector((x, y, z))))
    return verts


def bridge_rings(bm, ring_a, ring_b):
    n = len(ring_a)
    faces = []
    for i in range(n):
        j = (i + 1) % n
        faces.append(bm.faces.new((ring_a[i], ring_a[j], ring_b[j], ring_b[i])))
    return faces


def cap_flat(bm, ring):
    bm.faces.new(ring)


# ---------------------------------------------------------------------------
# Bloques sueltos: cubo subdividido + ruido (mismo principio que
# assets/rock.py, escalado a piezas de acantilado) con bisel cartoon.
# ---------------------------------------------------------------------------

def build_boulder(name, seed, size, squash_range=(0.6, 0.95), n_points=26,
                   radius_jitter=0.4, bevel_width_ratio=0.07):
    """Roca de bloque/canto: envolvente convexa (convex hull) de puntos
    cuasi-uniformes (espiral de Fibonacci) sobre un elipsoide con radio
    perturbado, no un cubo subdividido + ruido. El hull da facetas planas
    GRANDES de verdad (cada cara es un plano real entre sus vecinas, no una
    onda continua de ruido) — es lo que da el look "gema tallada" cartoon
    en vez de una patata suavizada."""
    rnd = random.Random(seed)
    sx = size * rnd.uniform(0.85, 1.15)
    sy = size * rnd.uniform(0.85, 1.15)
    sz = size * rnd.uniform(*squash_range)

    bm = bmesh.new()
    verts = []
    for i in range(n_points):
        idx = i + 0.5
        phi = math.acos(max(-1.0, min(1.0, 1.0 - 2.0 * idx / n_points)))
        theta = math.pi * (1.0 + 5.0 ** 0.5) * idx
        r = 0.5 * (1.0 + rnd.uniform(-radius_jitter, radius_jitter))
        x = math.sin(phi) * math.cos(theta) * r * sx
        y = math.sin(phi) * math.sin(theta) * r * sy
        z = math.cos(phi) * r * sz
        verts.append(bm.verts.new(Vector((x, y, z))))
    bmesh.ops.convex_hull(bm, input=verts)
    bm.normal_update()

    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new(name, me)
    C.link_object(obj)

    S.bevel_obj(obj, width=max(0.01, size * bevel_width_ratio), segments=2,
                limit_angle_deg=25.0)
    return obj


# ---------------------------------------------------------------------------
# Losas planas (caliza): anillo irregular extruido a un grosor fino.
# ---------------------------------------------------------------------------

def build_slab(name, seed, radius, thickness, sides=10, radius_jitter=0.26):
    rnd = random.Random(seed)
    bm = bmesh.new()
    top = ring_verts(bm, thickness, radius, sides, rnd, radius_jitter=radius_jitter)
    bot = ring_verts(bm, 0.0, radius * rnd.uniform(0.92, 1.0), sides, rnd,
                      radius_jitter=radius_jitter, angle_offset=rnd.uniform(0.0, 0.3))
    bridge_rings(bm, bot, top)
    cap_flat(bm, top)
    cap_flat(bm, list(reversed(bot)))
    bm.normal_update()
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new(name, me)
    C.link_object(obj)
    return obj


# ---------------------------------------------------------------------------
# Utilidades generales
# ---------------------------------------------------------------------------

def ground_to_zero(obj):
    """Baja/sube la malla para que su punto mas bajo quede en z=0 (pivote
    en la base)."""
    me = obj.data
    if not me.vertices:
        return obj
    mz = min(v.co.z for v in me.vertices)
    if abs(mz) > 1e-5:
        me.transform(Matrix.Translation((0.0, 0.0, -mz)))
        me.update()
    return obj


def finish_rock(obj, name, bevel_width, bevel_segments=2, bevel_angle_deg=38.0,
                 smooth_angle_deg=36.0, uv_method='CUBE', apply_bevel=True):
    ground_to_zero(obj)
    if apply_bevel and bevel_width > 0:
        S.bevel_obj(obj, width=bevel_width, segments=bevel_segments,
                    limit_angle_deg=bevel_angle_deg)
    # limpieza generica de geometria degenerada: las rejillas bmesh propias
    # de este kit (paredes, columnas) a veces dejan alguna cara de area
    # casi cero en costuras de join/bisel que el dissolve_degenerate interno
    # de bevel_obj (umbral 1e-4) no siempre atrapa. validate.py las rechaza,
    # asi que se repite el paso aqui con un umbral algo mayor.
    C.select_only(obj)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.dissolve_degenerate(threshold=1e-3)
    bpy.ops.object.mode_set(mode='OBJECT')
    remove_sliver_triangles(obj)
    C.shade_smooth_auto(obj, angle_deg=smooth_angle_deg)
    C.add_basic_uv(obj, method=uv_method)
    obj.name = name
    return obj


def _loop_tri_area(lt):
    a, b, c = (loop.vert.co for loop in lt)
    return (b - a).cross(c - a).length * 0.5


def remove_sliver_triangles(obj, min_area=1e-7, max_passes=4):
    """Elimina los triángulos «aguja» que saldrían al triangular la malla.

    El decimate planar + bisel de las formaciones escaneadas deja n-gonos
    grandes (hasta ~180 lados) con tramos de vértices casi alineados a
    ~1 mm: sus caras tienen área de sobra (dissolve_degenerate no las toca),
    pero el FBX se exporta triangulado (use_triangles=True) y esos tramos
    se convierten en triángulos de área ~0 que validate.py rechaza. Aquí se
    triangulan (BEAUTY) SOLO las caras que producirían alguno de esos
    triángulos y se colapsa la arista más corta de cada aguja restante; el
    resto de la malla no se toca."""
    me = obj.data
    for _ in range(max_passes):
        bm = bmesh.new()
        bm.from_mesh(me)
        bad = {lt[0].face for lt in bm.calc_loop_triangles() if _loop_tri_area(lt) < min_area}
        if not bad:
            bm.free()
            break
        tris = bmesh.ops.triangulate(bm, faces=list(bad), quad_method='BEAUTY',
                                     ngon_method='BEAUTY')['faces']
        agujas = [f for f in tris if f.is_valid and f.calc_area() < min_area]
        cortas = {min(f.edges, key=lambda e: e.calc_length()) for f in agujas}
        if cortas:
            bmesh.ops.collapse(bm, edges=list(cortas), uvs=True)
        bm.to_mesh(me)
        bm.free()
        me.update()
    return obj


def rock_tint(base_rgb, seed, rnd, height=None, band_count=1, band_var=0.06,
              crack_positions=None, crack_width=0.4, crack_dark=0.55,
              wet_height=0.0, wet_dark=0.45, dark_rgb=(0.04, 0.04, 0.05),
              rim_light=0.14, rim_shadow=0.18, jitter=0.03):
    """Color_fn generico del kit de acantilados: bandas de estrato sutiles,
    grietas verticales oscurecidas, base humeda mas oscura junto al agua y
    una falsa oclusion ambiental por normal (caras hacia arriba mas claras,
    hendiduras/aleros mas oscuros) que marca las aristas biseladas, tal y
    como pide la direccion de arte (AO pintado + aristas claras)."""
    cache = {}

    def fn(v):
        if v.index not in cache:
            cache[v.index] = rnd.uniform(-jitter, jitter)
        j = cache[v.index]
        r, g, b = base_rgb

        if height and band_count > 1:
            t = max(0.0, min(0.999, v.co.z / height))
            band = int(t * band_count)
            bj = (((band * 92821 + seed * 7 + 13) % 1000) / 1000.0 - 0.5) * 2.0 * band_var
            r, g, b = r + bj, g + bj, b + bj

        if crack_positions:
            amt = 0.0
            for cx in crack_positions:
                d = abs(v.co.x - cx)
                amt = max(amt, math.exp(-(d / crack_width) ** 2))
            r = r * (1.0 - amt * crack_dark) + dark_rgb[0] * amt * crack_dark
            g = g * (1.0 - amt * crack_dark) + dark_rgb[1] * amt * crack_dark
            b = b * (1.0 - amt * crack_dark) + dark_rgb[2] * amt * crack_dark

        if wet_height > 0.0:
            wet = max(0.0, 1.0 - v.co.z / wet_height)
            r = r * (1.0 - wet * wet_dark) + dark_rgb[0] * wet * wet_dark
            g = g * (1.0 - wet * wet_dark) + dark_rgb[1] * wet * wet_dark
            b = b * (1.0 - wet * wet_dark) + dark_rgb[2] * wet * wet_dark

        nz = v.normal.z
        boost = nz * rim_light if nz > 0.0 else nz * rim_shadow
        r, g, b = r + boost + j, g + boost + j, b + boost + j
        return (max(0.0, min(1.0, r)), max(0.0, min(1.0, g)), max(0.0, min(1.0, b)), 0.0)
    return fn
