"""
_cliffkit.py — utilidades compartidas del kit de rocas y acantilados
(Tools/Blender/props/rocks_cliffs.py): anillos de radio irregular para
columnas rocosas, paredes de acantilado por estratos con grietas
verticales, puentes de arco curvados y el tinte de vertice comun del kit
(bandas de estrato, grietas oscurecidas, base humeda y una falsa oclusion
ambiental por normal que marca las aristas).

Vive en Tools/Blender/props/ (no toca common.py, _shapes.py ni ningun
fichero de vegetacion: es aditivo, solo para este kit, igual que
_materials.py).
"""

import math
import os
import random
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import common as C  # noqa: E402
import _shapes as S  # noqa: E402

import bmesh  # noqa: E402
import bpy  # noqa: E402
from mathutils import Matrix, Vector, noise as mnoise  # noqa: E402


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


def cap_peak(bm, ring, z_tip, rnd, peak_jitter=0.3):
    """Tapa un anillo con un pico quebrado (vertice central desplazado, no
    un cono perfecto): remate de farallon o espolon."""
    cx = sum(v.co.x for v in ring) / len(ring)
    cy = sum(v.co.y for v in ring) / len(ring)
    tip = bm.verts.new(Vector((cx + rnd.uniform(-peak_jitter, peak_jitter),
                                cy + rnd.uniform(-peak_jitter, peak_jitter), z_tip)))
    n = len(ring)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((ring[i], ring[j], tip))
    return tip


def cap_flat(bm, ring):
    bm.faces.new(ring)


def build_irregular_column(name, seed, height, base_radius, tip_radius, sides=9,
                            segments=8, radius_jitter=0.22, waist=0.0, twist=0.0,
                            taper_curve=1.0, peak=True, z_offset=0.0):
    """Columna rocosa irregular (farallon, espolon o pilar de arco): pila de
    anillos de radio variable entre base_radius y tip_radius, con un pico
    quebrado arriba (o una tapa plana si peak=False, para un pilar que
    sigue hacia el puente de un arco)."""
    rnd = random.Random(seed)
    bm = bmesh.new()
    rings = []
    for s in range(segments + 1):
        t = s / segments
        z = height * t + z_offset
        radius = base_radius + (tip_radius - base_radius) * (t ** taper_curve)
        radius *= 1.0 - waist * math.sin(t * math.pi)
        ring = ring_verts(bm, z, radius, sides, rnd, radius_jitter=radius_jitter,
                           angle_offset=twist * t)
        rings.append(ring)
    for a, b in zip(rings[:-1], rings[1:]):
        bridge_rings(bm, a, b)
    if peak:
        cap_peak(bm, rings[-1], height + z_offset + tip_radius * rnd.uniform(0.4, 0.9), rnd)
    else:
        cap_flat(bm, rings[-1])
    cap_flat(bm, list(reversed(rings[0])))

    bm.normal_update()
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new(name, me)
    C.link_object(obj)
    return obj


def column_top_center(height, tip_radius, z_offset=0.0):
    """Punto aproximado del remate superior de una columna sin pico (peak=False),
    para anclar ahi un puente de arco."""
    return Vector((0.0, 0.0, height + z_offset))


# ---------------------------------------------------------------------------
# Paredes de acantilado por estratos (bmesh en rejilla: cara frontal con
# saliente/retranqueo por banda + grietas verticales, trasera plana o
# concava, lados y remates cerrando el volumen).
# ---------------------------------------------------------------------------

def build_strata_wall(name, seed, width, height, depth, n_bands=7,
                       band_offset=None, x_segments=16, z_segments=18,
                       crack_count=3, crack_depth=0.42, crack_width=0.35,
                       top_jag=1.2, base_jag=0.9, side_jag=0.9,
                       noise_scale=0.55, noise_strength=0.045,
                       concave_back=0.0, taper=0.0, lean=0.0):
    """Pared/espolon de acantilado: rejilla XZ desplazada en Y por bandas
    horizontales (estratos que sobresalen o se retiran a saltos, no un
    degradado) mas grietas verticales que hunden columnas concretas.
    `taper` estrecha la anchura con la altura (0 = pared recta, >0 =
    espolon que se afila); `lean` desplaza el eje lateralmente con la
    altura. La cara trasera (-Y) se deja simple (plana o concava con
    `concave_back`) porque va hundida en el terreno."""
    rnd = random.Random(seed)
    offset3 = Vector((rnd.uniform(-90, 90), rnd.uniform(-90, 90), rnd.uniform(-90, 90)))

    # saliente/retranqueo por banda relativo al grosor de la pared: un valor
    # absoluto fijo se notaba demasiado poco en paredes gruesas y demasiado
    # en las finas (estratos "marcados" pide ~20-30% del grosor por salto).
    if band_offset is None:
        band_offset = depth * 0.4

    band_shift = [0.0]
    for _ in range(1, n_bands):
        band_shift.append(band_shift[-1] + rnd.uniform(-band_offset, band_offset) * 0.6)
    mean_shift = sum(band_shift) / len(band_shift)
    band_shift = [s - mean_shift for s in band_shift]

    n_cracks = max(1, crack_count)
    crack_xs = sorted(rnd.uniform(-width * 0.4, width * 0.4) for _ in range(n_cracks))

    def crack_amount(x, t):
        amt = 0.0
        for cx in crack_xs:
            d = abs(x - cx)
            amt = max(amt, math.exp(-(d / crack_width) ** 2))
        # se cierra cerca de la cresta y de la base para no partir la silueta
        fade = min(1.0, t / 0.12) * min(1.0, (1.0 - t) / 0.12)
        return amt * max(0.0, fade)

    bm = bmesh.new()
    grid = [[None] * (z_segments + 1) for _ in range(x_segments + 1)]
    for i in range(x_segments + 1):
        u = i / x_segments
        for j in range(z_segments + 1):
            t = j / z_segments
            z = height * t
            half_w = (width * 0.5) * (1.0 - taper * t)
            cx = lean * t
            x = cx + (-half_w + width * (1.0 - taper * t) * u)

            band = min(n_bands - 1, int(t * n_bands))
            y = depth * 0.5 + band_shift[band]
            y += mnoise.noise(Vector((x, 0.0, z)) * noise_scale + offset3) * noise_strength * depth
            y -= crack_amount(x, t) * crack_depth * depth

            dx = dz = 0.0
            if j == z_segments:
                dz = rnd.uniform(-top_jag, top_jag) * (height / z_segments)
                dx = rnd.uniform(-0.5, 0.5)
            elif j == 0:
                dz = -abs(rnd.uniform(0.0, base_jag)) * (height / z_segments)
            if i in (0, x_segments):
                dx += rnd.uniform(-side_jag, side_jag)

            grid[i][j] = bm.verts.new(Vector((x + dx, y, z + dz)))

    for i in range(x_segments):
        for j in range(z_segments):
            a, b, c, d = grid[i][j], grid[i + 1][j], grid[i + 1][j + 1], grid[i][j + 1]
            bm.faces.new((a, b, c, d))

    back = [[None] * (z_segments + 1) for _ in range(x_segments + 1)]
    for i in range(x_segments + 1):
        u = i / x_segments
        for j in range(z_segments + 1):
            t = j / z_segments
            fx, fz = grid[i][j].co.x, grid[i][j].co.z
            bulge = concave_back * depth * (1.0 - (2.0 * u - 1.0) ** 2) if concave_back else 0.0
            back[i][j] = bm.verts.new(Vector((fx, -depth * 0.5 + bulge, fz)))

    for i in range(x_segments):
        for j in range(z_segments):
            a, b, c, d = back[i][j], back[i + 1][j], back[i + 1][j + 1], back[i][j + 1]
            bm.faces.new((d, c, b, a))  # orden invertido: normal hacia -Y

    for j in range(z_segments):
        bm.faces.new((grid[0][j], back[0][j], back[0][j + 1], grid[0][j + 1]))
        bm.faces.new((back[x_segments][j], grid[x_segments][j],
                       grid[x_segments][j + 1], back[x_segments][j + 1]))

    for i in range(x_segments):
        bm.faces.new((grid[i][z_segments], grid[i + 1][z_segments],
                       back[i + 1][z_segments], back[i][z_segments]))
        bm.faces.new((back[i][0], back[i + 1][0], grid[i + 1][0], grid[i][0]))

    bm.normal_update()
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new(name, me)
    C.link_object(obj)

    # la cara detallada se construyo mirando a +Y; el resto del kit de props
    # (p.ej. petroglyphs.py) presenta su cara frontal hacia -Y para la
    # camara fija de render_preview.py / preview_kit.py, asi que se espeja
    # en Y (con flip_normals para no invertir el sombreado) en vez de
    # duplicar toda la logica de bandas/grietas con los signos cambiados.
    obj.data.transform(Matrix.Scale(-1.0, 4, Vector((0.0, 1.0, 0.0))))
    obj.data.flip_normals()
    obj.data.update()
    return obj, crack_xs


# ---------------------------------------------------------------------------
# Arco marino: curva Bezier biselada con perfil en arco (no recto), misma
# tecnica que common.make_curved_trunk pero en el plano vertical.
# ---------------------------------------------------------------------------

def build_arch_bridge(name, seed, p0, p1, rise, base_radius, tip_radius=None,
                       n_points=9, bevel_resolution=4, wobble=0.0):
    rnd = random.Random(seed)
    tip_radius = base_radius if tip_radius is None else tip_radius
    p0, p1 = Vector(p0), Vector(p1)

    curve_data = bpy.data.curves.new(name + '_curve', type='CURVE')
    curve_data.dimensions = '3D'
    curve_data.resolution_u = 10
    spline = curve_data.splines.new('BEZIER')
    spline.bezier_points.add(n_points - 1)

    ratio = tip_radius / base_radius if base_radius > 0 else 1.0
    for i, bp in enumerate(spline.bezier_points):
        t = i / (n_points - 1)
        pos = p0.lerp(p1, t)
        pos.z += rise * math.sin(t * math.pi)
        wob = wobble * math.sin(t * math.pi * 3.1)
        pos.x += rnd.uniform(-wob, wob)
        pos.y += rnd.uniform(-wob, wob)
        bp.co = pos
        bp.handle_left_type = 'AUTO'
        bp.handle_right_type = 'AUTO'
        # mas grueso en la clave (centro) que en los arranques: perfil de
        # arco natural, no un tubo de grosor constante.
        arch_t = math.sin(t * math.pi)
        bp.radius = (1.0 - t * (1.0 - ratio)) * (0.85 + 0.25 * arch_t)

    curve_data.bevel_depth = base_radius
    curve_data.bevel_resolution = bevel_resolution
    curve_data.fill_mode = 'FULL'
    curve_data.use_fill_caps = True

    obj = bpy.data.objects.new(name, curve_data)
    C.link_object(obj)
    C.select_only(obj)
    bpy.ops.object.convert(target='MESH')
    return obj


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
# Cornisas: repisas finas adosadas a una pared, ancladas a su superficie
# frontal en una banda de altura concreta.
# ---------------------------------------------------------------------------

def build_ledge(name, seed, width, out_depth, thickness, front_y):
    """front_y es la coordenada de la cara frontal de la pared (negativa:
    ver la nota de espejo en build_strata_wall); la repisa sobresale hacia
    -Y, alejandose del bloque de la pared."""
    rnd = random.Random(seed)
    obj = C.make_box(name, (width, out_depth, thickness),
                      center=(0.0, front_y - out_depth * 0.5, 0.0))
    C.select_only(obj)
    bpy.ops.transform.rotate(value=rnd.uniform(-0.05, 0.05), orient_axis='Z')
    bpy.ops.transform.rotate(value=rnd.uniform(-0.06, 0.02), orient_axis='X')
    bpy.ops.object.transform_apply(rotation=True)
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
    C.shade_smooth_auto(obj, angle_deg=smooth_angle_deg)
    C.add_basic_uv(obj, method=uv_method)
    obj.name = name
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
