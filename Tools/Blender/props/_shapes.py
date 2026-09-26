"""
_shapes.py — utilidades compartidas de "look" para el kit de props:
bisel suave (para el aspecto "low-poly pero smooth, como animado" pedido
tras la primera pasada de arte) y funciones de color de vértice para
franjas de pintura y desgaste/óxido en parches, en vez de un tinte plano.

Vive en Tools/Blender/props/ (no toca common.py: es aditivo, solo para
este kit, igual que _materials.py).
"""
import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import common as C  # noqa: E402

import bpy  # noqa: E402


def bevel_obj(obj, width=0.012, segments=2, limit_angle_deg=35.0):
    """Bisela todas las aristas duras del objeto y aplica el modificador (el
    FBX exporta la geometría ya biselada). Redondea siluetas de caja/cilindro
    sin disparar tanto el recuento de triángulos como subdividir toda la
    malla — es la diferencia entre "low-poly cuadrado" y "low-poly pulido".

    `use_clamp_overlap` evita que el bisel se coma piezas finas (antenas,
    placas, estacas) cuando `width` es mayor que la mitad del grosor de la
    pieza, pero en geometría muy fina (varias cajas casi coplanares
    unidas) puede seguir dejando caras de area ~0 en las costuras del
    join; `dissolve_degenerate` limpia esas caras/aristas degeneradas
    justo despues de aplicar el modificador — sin este paso, validate.py
    detectaba cientos de caras de area cero en piezas biseladas."""
    C.select_only(obj)
    mod = obj.modifiers.new('Bevel', 'BEVEL')
    mod.width = width
    mod.segments = segments
    mod.limit_method = 'ANGLE'
    mod.angle_limit = math.radians(limit_angle_deg)
    mod.miter_outer = 'MITER_ARC'
    mod.use_clamp_overlap = True
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=mod.name)

    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.dissolve_degenerate(threshold=1e-4)
    bpy.ops.object.mode_set(mode='OBJECT')
    return obj


def bevel_all(objs, width=0.012, segments=2, limit_angle_deg=35.0):
    for o in objs:
        bevel_obj(o, width=width, segments=segments, limit_angle_deg=limit_angle_deg)
    return objs


def stripe_tint(base_rgb, stripe_rgb, axis, stripe_min, stripe_max,
                 accent_rgb=None, accent_at=None, accent_width=0.04,
                 jitter=0.02, rnd=None):
    """Color_fn con una franja de color solida entre stripe_min/stripe_max a
    lo largo de un eje local (librea de avion: fuselaje claro + banda de
    color), con un acento opcional (filete fino) centrado en accent_at."""
    idx = {'x': 0, 'y': 1, 'z': 2}[axis]
    cache = {}

    def fn(v):
        if v.index not in cache:
            cache[v.index] = rnd.uniform(-jitter, jitter) if (jitter and rnd is not None) else 0.0
        j = cache[v.index]
        t = v.co[idx]
        color = stripe_rgb if stripe_min <= t <= stripe_max else base_rgb
        if accent_at is not None and abs(t - accent_at) <= accent_width and accent_rgb is not None:
            color = accent_rgb
        r = max(0.0, min(1.0, color[0] + j))
        g = max(0.0, min(1.0, color[1] + j))
        b = max(0.0, min(1.0, color[2] + j))
        return (r, g, b, 0.0)
    return fn


def weathered_tint(base_rgb, wear_rgb, seed, patchiness=3.0, wear_amount=0.35,
                    jitter=0.03, rnd=None):
    """Color_fn de desgaste/oxido en PARCHES organicos (ruido de senos
    cruzados en las 3 coordenadas locales, barato y determinista) en vez de
    un degradado uniforme: lee como suciedad/oxido real, no como un
    "gradiente de PowerPoint"."""
    cache = {}

    def fn(v):
        if v.index not in cache:
            n = (math.sin(v.co.x * patchiness * 7.1 + seed)
                 * math.sin(v.co.y * patchiness * 5.3 + seed * 2.0)
                 * math.sin(v.co.z * patchiness * 3.7 + seed * 3.0))
            j = rnd.uniform(-jitter, jitter) if rnd is not None else 0.0
            cache[v.index] = (max(0.0, n), j)
        n, j = cache[v.index]
        w = min(1.0, n / max(1e-4, (1.0 - wear_amount)))
        r = max(0.0, min(1.0, base_rgb[0] * (1.0 - w) + wear_rgb[0] * w + j))
        g = max(0.0, min(1.0, base_rgb[1] * (1.0 - w) + wear_rgb[1] * w + j))
        b = max(0.0, min(1.0, base_rgb[2] * (1.0 - w) + wear_rgb[2] * w + j))
        return (r, g, b, 0.0)
    return fn


def banded_tint(colors, axis, v_min, v_max, jitter=0.02, rnd=None):
    """Color_fn de bandas discretas (N colores en franjas iguales a lo largo
    de un eje): tablones de madera con vetas de tono distinto, capas de hoja
    de palma de tono alterno, etc. `colors` es una lista de RGB."""
    idx = {'x': 0, 'y': 1, 'z': 2}[axis]
    n = max(1, len(colors))
    span = max(1e-6, v_max - v_min)
    cache = {}

    def fn(v):
        if v.index not in cache:
            cache[v.index] = rnd.uniform(-jitter, jitter) if (jitter and rnd is not None) else 0.0
        j = cache[v.index]
        t = max(0.0, min(0.999, (v.co[idx] - v_min) / span))
        color = colors[int(t * n)]
        r = max(0.0, min(1.0, color[0] + j))
        g = max(0.0, min(1.0, color[1] + j))
        b = max(0.0, min(1.0, color[2] + j))
        return (r, g, b, 0.0)
    return fn
