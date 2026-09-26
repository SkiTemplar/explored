"""
Módulo común del pipeline de vegetación y rocas de «Explored».

Se importa desde cada script de familia (Tools/Blender/assets/*.py) y desde
run_all.py / validate.py. No depende de nada externo al Python embebido de
Blender 5.2: solo bpy, bmesh y mathutils.

Convenciones del kit:
    - Unidades de escena: metros (1 unidad de Blender = 1 m). El exportador
      FBX aplica apply_unit_scale para que Unreal reciba centímetros
      (1 m = 100 uu), como exige Unreal.
    - Los cuatro materiales del kit tienen nombre estable: M_Bark, M_Leaf,
      M_Rock, M_Grass. Cada malla usa solo los que necesite, en slots
      ordenados de forma determinista (ver ASSIGN order en cada script).
    - El atributo de color de vértice se llama siempre «Col», dominio CORNER,
      tipo FLOAT_COLOR (BYTE_COLOR no sobrevive el roundtrip de FBX en
      Blender 5.2, verificado exportando/reimportando: se pierde por completo).
      El canal RGB lleva el tinte base (variación de tono
      por instancia); el canal A lleva la máscara de viento: 0.0 en la base
      del objeto (tronco, raíz) y 1.0 en las puntas de hojas/hierba, para que
      un material de Unreal pueda usarlo como peso de un offset de viento.
    - La semilla determinista de cada instancia es siempre
      random.Random(seed); nunca se usa random global sin sembrar.
"""

import bpy
import bmesh
import math
import random
from mathutils import Vector, Matrix
from mathutils import noise as mnoise

# ---------------------------------------------------------------------------
# Gestión de escena
# ---------------------------------------------------------------------------

def reset_scene():
    """Deja la escena de Blender completamente vacía (factory startup)."""
    bpy.ops.wm.read_factory_settings(use_empty=True)
    purge_orphans()


def purge_orphans():
    """Elimina bloques de datos sin usuarios (mallas, curvas, materiales...).

    Necesario porque el proceso de Blender se reutiliza entre familias dentro
    de la misma invocación de run_all.py: sin purgar, los nombres de malla
    se van acumulando con sufijos .001, .002...
    """
    for coll in (bpy.data.objects, bpy.data.meshes, bpy.data.curves,
                 bpy.data.images):
        for block in list(coll):
            if block.users == 0:
                try:
                    coll.remove(block)
                except Exception:
                    pass


def seeded_rng(seed):
    """Generador determinista propio de la instancia; nunca random global."""
    return random.Random(seed)


def link_object(obj):
    bpy.context.collection.objects.link(obj)
    return obj


def select_only(obj):
    for o in bpy.context.selected_objects:
        o.select_set(False)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj


# ---------------------------------------------------------------------------
# Materiales (nombres estables M_Bark / M_Leaf / M_Rock / M_Grass)
# ---------------------------------------------------------------------------

_MATERIAL_DEFS = {
    'M_Bark':  dict(base_color=(0.16, 0.10, 0.07, 1.0), roughness=0.92),
    'M_Leaf':  dict(base_color=(0.07, 0.26, 0.10, 1.0), roughness=0.5),
    'M_Rock':  dict(base_color=(0.36, 0.35, 0.33, 1.0), roughness=0.88),
    'M_Grass': dict(base_color=(0.18, 0.42, 0.14, 1.0), roughness=0.55),
}

MATERIAL_NAMES = frozenset(_MATERIAL_DEFS.keys())


def get_material(name):
    """Devuelve (creándolo si falta) uno de los 4 materiales estables.

    El color de vértice «Col» multiplica al color base para dar variación
    de tono por instancia; el canal alfa no se usa en el shading de
    previsualización (solo sirve como máscara de viento para Unreal).
    """
    if name in bpy.data.materials:
        return bpy.data.materials[name]
    if name not in _MATERIAL_DEFS:
        raise ValueError(f"Material desconocido: {name}")
    cfg = _MATERIAL_DEFS[name]

    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = nt.nodes.get('Principled BSDF')

    attr = nt.nodes.new('ShaderNodeAttribute')
    attr.attribute_name = 'Col'
    attr.attribute_type = 'GEOMETRY'

    base = nt.nodes.new('ShaderNodeRGB')
    base.outputs[0].default_value = cfg['base_color']

    mix = nt.nodes.new('ShaderNodeMixRGB')
    mix.blend_type = 'MULTIPLY'
    mix.inputs['Fac'].default_value = 1.0
    nt.links.new(base.outputs[0], mix.inputs['Color1'])
    nt.links.new(attr.outputs['Color'], mix.inputs['Color2'])
    nt.links.new(mix.outputs[0], bsdf.inputs['Base Color'])

    bsdf.inputs['Roughness'].default_value = cfg['roughness']
    if 'Metallic' in bsdf.inputs:
        bsdf.inputs['Metallic'].default_value = 0.0
    return mat


def assign_materials(obj, slot_names):
    """Asigna los slots de material en el orden dado (orden estable)."""
    obj.data.materials.clear()
    for name in slot_names:
        obj.data.materials.append(get_material(name))


def set_face_material_index(obj, poly_indices, slot_index):
    me = obj.data
    for i in poly_indices:
        me.polygons[i].material_index = slot_index


# ---------------------------------------------------------------------------
# Sombreado suave
# ---------------------------------------------------------------------------

def shade_smooth_auto(obj, angle_deg=40.0):
    """Sombreado suave con «auto smooth» por ángulo (shade_smooth_by_angle).

    Reemplaza al viejo Auto Smooth de <4.1: en Blender 5.2 el ángulo se
    hornea como un modificador «Smooth by Angle» (Geometry Nodes) que se
    aplica antes de exportar para que el FBX se quede con las normales
    correctas sin depender de ese modificador en tiempo de exportación.
    """
    select_only(obj)
    bpy.ops.object.shade_smooth_by_angle(angle=math.radians(angle_deg))
    # aplicar el modificador que shade_smooth_by_angle añade, para que el
    # FBX exporte las normales ya resueltas.
    for mod in list(obj.modifiers):
        if mod.type == 'NODES' and 'Smooth by Angle' in mod.name:
            bpy.context.view_layer.objects.active = obj
            bpy.ops.object.modifier_apply(modifier=mod.name)


def shade_flat(obj):
    select_only(obj)
    bpy.ops.object.shade_flat()


# ---------------------------------------------------------------------------
# Color de vértice (tinte base + máscara de viento en alfa)
# ---------------------------------------------------------------------------

def set_vertex_colors(obj, color_fn):
    """Escribe el atributo de color «Col» (CORNER, FLOAT_COLOR).

    color_fn(vertex: bpy.types.MeshVertex) -> (r, g, b, a) en [0, 1].
    Recibe el vértice completo (co, normal, index) para poder cachear
    variación por vértice y evitar costuras entre loops de un mismo vértice.
    """
    me = obj.data
    if 'Col' in me.color_attributes:
        me.color_attributes.remove(me.color_attributes['Col'])
    attr = me.color_attributes.new(name='Col', type='FLOAT_COLOR', domain='CORNER')
    for poly in me.polygons:
        for li in poly.loop_indices:
            loop = me.loops[li]
            v = me.vertices[loop.vertex_index]
            attr.data[li].color = color_fn(v)
    me.color_attributes.active_color_name = 'Col'
    me.color_attributes.render_color_index = list(me.color_attributes).index(attr)


def height_mask(value, v_min, v_max, curve=1.0):
    """Máscara 0→1 según una coordenada local, con exponente opcional."""
    if v_max <= v_min:
        return 0.0
    t = (value - v_min) / (v_max - v_min)
    t = max(0.0, min(1.0, t))
    return t ** curve if curve != 1.0 else t


def tint_along_axis(base_rgb, axis, v_min, v_max, curve=1.0, jitter=0.0, rnd=None):
    """Color_fn de conveniencia para geometría construida a lo largo de un
    eje local (Y para hojas/frondas, Z para troncos): RGB = tinte base con
    jitter por vértice cacheado (sin costuras), A = máscara de viento
    (0 en la base del eje, 1 en la punta)."""
    idx = {'x': 0, 'y': 1, 'z': 2}[axis]
    cache = {}

    def fn(v):
        if v.index not in cache:
            cache[v.index] = rnd.uniform(-jitter, jitter) if (jitter and rnd is not None) else 0.0
        j = cache[v.index]
        a = height_mask(v.co[idx], v_min, v_max, curve)
        r = max(0.0, min(1.0, base_rgb[0] + j))
        g = max(0.0, min(1.0, base_rgb[1] + j))
        b = max(0.0, min(1.0, base_rgb[2] + j))
        return (r, g, b, a)
    return fn


def constant_tint(base_rgb, alpha=0.0, jitter=0.05, rnd=None):
    """Color_fn para geometría sin viento (rocas): alfa fija, jitter por
    vértice cacheado para variación orgánica de tono sin costuras."""
    cache = {}

    def fn(v):
        if v.index not in cache:
            cache[v.index] = rnd.uniform(-jitter, jitter) if (jitter and rnd is not None) else 0.0
        j = cache[v.index]
        r = max(0.0, min(1.0, base_rgb[0] + j))
        g = max(0.0, min(1.0, base_rgb[1] + j))
        b = max(0.0, min(1.0, base_rgb[2] + j))
        return (r, g, b, alpha)
    return fn


def _clamp01(x):
    return max(0.0, min(1.0, x))


def gradient_along_axis(base_color, tip_color, axis, v_min, v_max, curve=1.0,
                         jitter=0.0, rnd=None):
    """Color_fn con degradado real entre dos colores a lo largo de un eje
    local: base_color en v_min, tip_color en v_max. Pensado para «la base
    de las hojas más oscura y las puntas más claras» en vez de un tinte
    plano — el canal alfa (máscara de viento) sigue el mismo degradado
    (0 en la base, 1 en la punta)."""
    idx = {'x': 0, 'y': 1, 'z': 2}[axis]
    cache = {}

    def fn(v):
        if v.index not in cache:
            cache[v.index] = rnd.uniform(-jitter, jitter) if (jitter and rnd is not None) else 0.0
        j = cache[v.index]
        t = height_mask(v.co[idx], v_min, v_max, curve)
        r = _clamp01(base_color[0] + (tip_color[0] - base_color[0]) * t + j)
        g = _clamp01(base_color[1] + (tip_color[1] - base_color[1]) * t + j)
        b = _clamp01(base_color[2] + (tip_color[2] - base_color[2]) * t + j)
        return (r, g, b, t)
    return fn


def bark_streaks_tint(dark_color, light_color, height, seed, streak_count=9,
                       jitter=0.03, alpha=0.0):
    """Vetas verticales de corteza: mezcla dark/light según una onda que
    depende del ángulo alrededor del eje del tronco (co.x/co.y), para que
    la corteza no quede de un marrón plano sino con vetas longitudinales
    de grosor irregular. La máscara de viento (alfa) sigue la altura."""
    rnd = random.Random(seed)
    phase = rnd.uniform(0.0, 2.0 * math.pi)
    freq_jitter = rnd.uniform(0.85, 1.15)
    cache = {}

    def fn(v):
        if v.index not in cache:
            ang = math.atan2(v.co.y, v.co.x)
            streak = 0.5 + 0.5 * math.sin(ang * streak_count * freq_jitter + phase)
            streak = streak ** 1.4
            j = rnd.uniform(-jitter, jitter)
            cache[v.index] = (streak, j)
        streak, j = cache[v.index]
        r = _clamp01(dark_color[0] + (light_color[0] - dark_color[0]) * streak + j)
        g = _clamp01(dark_color[1] + (light_color[1] - dark_color[1]) * streak + j)
        b = _clamp01(dark_color[2] + (light_color[2] - dark_color[2]) * streak + j)
        a = height_mask(v.co.z, 0.0, height, curve=1.0) if height > 0 else alpha
        return (r, g, b, a)
    return fn


# ---------------------------------------------------------------------------
# UVs básicas
# ---------------------------------------------------------------------------

def add_basic_uv(obj, method='SMART'):
    select_only(obj)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    if method == 'SMART':
        bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.02)
    else:
        bpy.ops.uv.cube_project()
    bpy.ops.object.mode_set(mode='OBJECT')


# ---------------------------------------------------------------------------
# Geometría: troncos, ramas y lianas curvadas (curva Bezier -> malla)
# ---------------------------------------------------------------------------

def make_curved_trunk(name, height, base_radius, tip_radius, curvature,
                       n_points=8, bevel_resolution=3, lean_dir=None,
                       wobble=0.0, rnd=None, z_offset=0.0):
    """Crea un tronco/rama/liana curvado biselando una curva Bezier.

    curvature: desplazamiento lateral máximo (m) alcanzado en la punta,
    con una curva de potencia 1.6 para que se doble más cerca del final
    (perfil típico de tronco de palmera).
    z_offset: sube el arranque del tronco esa distancia en Z sin afectar a
    la curvatura (para troncos de manglar que arrancan por encima del
    suelo, levantados por sus raíces zancudas).
    """
    if rnd is None:
        rnd = random.Random(0)
    if lean_dir is None:
        lean_dir = rnd.uniform(0.0, 2.0 * math.pi)

    curve_data = bpy.data.curves.new(name + '_curve', type='CURVE')
    curve_data.dimensions = '3D'
    curve_data.resolution_u = 8
    spline = curve_data.splines.new('BEZIER')
    spline.bezier_points.add(n_points - 1)

    dir_x, dir_y = math.cos(lean_dir), math.sin(lean_dir)
    radius_ratio = tip_radius / base_radius if base_radius > 0 else 1.0

    for i, bp in enumerate(spline.bezier_points):
        t = i / (n_points - 1)
        z = height * t + z_offset
        bend = curvature * (t ** 1.6)
        wob = wobble * math.sin(t * math.pi * 2.3) * (1.0 - t)
        x = dir_x * bend + rnd.uniform(-wob, wob)
        y = dir_y * bend + rnd.uniform(-wob, wob)
        bp.co = Vector((x, y, z))
        bp.handle_left_type = 'AUTO'
        bp.handle_right_type = 'AUTO'
        bp.radius = 1.0 - t * (1.0 - radius_ratio)

    curve_data.bevel_depth = base_radius
    curve_data.bevel_resolution = bevel_resolution
    curve_data.fill_mode = 'FULL'
    curve_data.use_fill_caps = True

    obj = bpy.data.objects.new(name, curve_data)
    link_object(obj)
    select_only(obj)
    bpy.ops.object.convert(target='MESH')
    return obj, lean_dir


def spline_point(height, t, curvature, lean_dir, z_offset=0.0):
    """Punto (Vector) sobre el mismo perfil de curvatura que make_curved_trunk,
    útil para anclar hojas/ramas a lo largo de un tronco sin duplicar curvas."""
    bend = curvature * (t ** 1.6)
    z = height * t + z_offset
    x = math.cos(lean_dir) * bend
    y = math.sin(lean_dir) * bend
    return Vector((x, y, z))


def spline_tangent(height, t, curvature, lean_dir, dt=1e-3, z_offset=0.0):
    p0 = spline_point(height, max(0.0, t - dt), curvature, lean_dir, z_offset)
    p1 = spline_point(height, min(1.0, t + dt), curvature, lean_dir, z_offset)
    d = (p1 - p0)
    if d.length > 1e-8:
        d.normalize()
    else:
        d = Vector((0, 0, 1))
    return d


# ---------------------------------------------------------------------------
# Geometría: mallas planas tipo tarjeta (hojas, hierba)
# ---------------------------------------------------------------------------

def make_leaf_blade(name, length, width_base, width_tip, curve_amount,
                     segments=6, bend_axis='X', double_sided=True):
    """Tarjeta alargada curvada (hoja de palma/plátano, hierba, pétalo).

    Se construye a lo largo de +Y (longitud) con la anchura en X y la
    curvatura hacia -Z o +Z según bend_axis/curve_amount. El origen queda
    en la base (0,0,0) para poder rotarla y anclarla con facilidad.
    """
    bm = bmesh.new()
    verts_top = []
    verts_bot = []
    for i in range(segments + 1):
        t = i / segments
        w = width_base + (width_tip - width_base) * t
        y = length * t
        z = curve_amount * (t ** 1.5)
        vt = bm.verts.new(Vector((-w * 0.5, y, z)))
        vb = bm.verts.new(Vector((w * 0.5, y, z)))
        verts_top.append(vt)
        verts_bot.append(vb)

    for i in range(segments):
        f = bm.faces.new((verts_top[i], verts_bot[i], verts_bot[i + 1], verts_top[i + 1]))
        f.normal_update()

    if double_sided:
        orig_faces = list(bm.faces)
        ret = bmesh.ops.duplicate(bm, geom=orig_faces + list(bm.edges) + list(bm.verts))
        dup_faces = [g for g in ret['geom'] if isinstance(g, bmesh.types.BMFace)]
        bmesh.ops.reverse_faces(bm, faces=dup_faces)

    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new(name, me)
    link_object(obj)
    return obj


def make_blob(name, center, radius, seed, subdivisions=2, noise_scale=1.8,
              noise_strength=0.35, scale=(1.0, 1.0, 1.0), relax_iterations=0):
    """Esfera de icosaedro deformada con ruido: «blob» orgánico de hojas o
    roca. subdivisions controla la densidad (2 -> 42 verts / 80 tris).

    relax_iterations suaviza el resultado con varias pasadas de «smooth
    vertex» tras el ruido: sirve para que los cúmulos de hoja (copas de
    árbol) queden redondeados y con volumen en vez de picudos, sin perder
    la silueta general que da el ruido de baja frecuencia.
    """
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=subdivisions, radius=radius)
    rnd = random.Random(seed)
    offset = Vector((rnd.uniform(-90, 90), rnd.uniform(-90, 90), rnd.uniform(-90, 90)))
    bm.normal_update()
    for v in bm.verts:
        p = v.co * noise_scale + offset
        n1 = mnoise.noise(p)
        n2 = mnoise.noise(p * 2.13 + Vector((7, 3, 1))) * 0.5
        disp = (n1 + n2) / 1.5
        v.co += v.normal * disp * noise_strength * radius
    if relax_iterations > 0:
        bmesh.ops.smooth_vert(bm, verts=bm.verts, factor=0.5,
                               use_axis_x=True, use_axis_y=True, use_axis_z=True)
        for _ in range(relax_iterations - 1):
            bmesh.ops.smooth_vert(bm, verts=bm.verts, factor=0.5,
                                   use_axis_x=True, use_axis_y=True, use_axis_z=True)
    for v in bm.verts:
        v.co.x *= scale[0]
        v.co.y *= scale[1]
        v.co.z *= scale[2]
        v.co += Vector(center)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new(name, me)
    link_object(obj)
    return obj


def displace_mesh_noise(obj, seed, strength=0.15, scale=1.5, octaves=2):
    """Desplaza los vértices de una malla ya construida a lo largo de su
    normal usando ruido Perlin determinista (offset derivado de la semilla).
    Pensado para rocas creadas a partir de un cubo subdividido."""
    me = obj.data
    rnd = random.Random(seed)
    offset = Vector((rnd.uniform(-90, 90), rnd.uniform(-90, 90), rnd.uniform(-90, 90)))
    bm = bmesh.new()
    bm.from_mesh(me)
    bm.normal_update()
    for v in bm.verts:
        total = 0.0
        amp = 1.0
        p = v.co * scale + offset
        acc = 0.0
        for o in range(octaves):
            acc += mnoise.noise(p * (2 ** o)) * amp
            total += amp
            amp *= 0.5
        disp = acc / total
        v.co += v.normal * disp * strength
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me)
    bm.free()
    me.update()


def orient_and_place(obj, origin, forward, up):
    """Orienta un objeto local (construido con el eje de crecimiento en +Y y
    el ancho en +X) para que +Y coincida con «forward» y +Z quede lo más
    alineado posible con «up» (ortogonalizado por Gram-Schmidt), y lo mueve
    a «origin». Aplica la transformación (bake) para dejar coordenadas
    locales limpias antes de unir el objeto al resto de la malla.
    """
    forward = Vector(forward)
    if forward.length < 1e-8:
        forward = Vector((0, 1, 0))
    forward.normalize()
    up_hint = Vector(up)
    up_ortho = up_hint - forward * up_hint.dot(forward)
    if up_ortho.length < 1e-6:
        up_hint = Vector((0, 0, 1)) if abs(forward.z) < 0.9 else Vector((1, 0, 0))
        up_ortho = up_hint - forward * up_hint.dot(forward)
    up_ortho.normalize()
    right = forward.cross(up_ortho).normalized()
    up_final = right.cross(forward).normalized()

    mat = Matrix((
        (right.x, forward.x, up_final.x, origin[0]),
        (right.y, forward.y, up_final.y, origin[1]),
        (right.z, forward.z, up_final.z, origin[2]),
        (0.0, 0.0, 0.0, 1.0),
    ))
    obj.matrix_world = mat
    select_only(obj)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)


def orient_and_place_zaxis(obj, origin, direction, up_hint=None):
    """Como orient_and_place, pero para objetos construidos con el eje de
    crecimiento en +Z (troncos, ramas y raíces de make_curved_trunk): +Z
    pasa a apuntar a «direction» en vez de +Y. Sin esto, colocar una rama
    con orient_and_place (pensado para hojas que crecen en +Y) la deja
    apuntando hacia «up» en vez de hacia fuera del tronco — el bug que
    hacía que los árboles de selva parecieran «piruletas» con las ramas
    pegadas al tronco en vez de abrirse hacia los lados.
    """
    direction = Vector(direction)
    if direction.length < 1e-8:
        direction = Vector((0, 0, 1))
    direction.normalize()
    if up_hint is None:
        up_hint = Vector((0, 1, 0)) if abs(direction.z) > 0.9 else Vector((0, 0, 1))
    else:
        up_hint = Vector(up_hint)
    up_ortho = up_hint - direction * up_hint.dot(direction)
    if up_ortho.length < 1e-6:
        alt = Vector((1, 0, 0))
        up_ortho = alt - direction * alt.dot(direction)
    up_ortho.normalize()
    right = up_ortho.cross(direction).normalized()
    up_final = direction.cross(right).normalized()

    mat = Matrix((
        (right.x, up_final.x, direction.x, origin[0]),
        (right.y, up_final.y, direction.y, origin[1]),
        (right.z, up_final.z, direction.z, origin[2]),
        (0.0, 0.0, 0.0, 1.0),
    ))
    obj.matrix_world = mat
    select_only(obj)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)


def add_ring_bumps(obj, spacing, amplitude, rnd=None, sharpness=6):
    """Añade bultos periódicos a lo largo de Z (anillos de tronco de
    palmera, nudos de bambú) desplazando cada vértice a lo largo de su
    propia normal según una onda coseno elevada a «sharpness» (pulsos
    estrechos en vez de una onda sinusoidal ancha)."""
    me = obj.data
    phase = rnd.uniform(0.0, spacing) if rnd is not None else 0.0
    bm = bmesh.new()
    bm.from_mesh(me)
    bm.normal_update()
    for v in bm.verts:
        wave = 0.5 + 0.5 * math.cos(2.0 * math.pi * (v.co.z + phase) / spacing)
        wave = wave ** sharpness
        v.co += v.normal * wave * amplitude
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me)
    bm.free()
    me.update()


def _add_leaflet(bm, origin, forward, right, up, length, width, curve, rnd):
    """Añade un folíolo (mini-hoja lanceolada) de 4 triángulos a un bmesh:
    un punto de anclaje, dos secciones intermedias (la más ancha) y una
    punta, con caída (curve) hacia -up. Uso interno de make_frond_object."""
    forward = Vector(forward).normalized()
    right = Vector(right)
    up = Vector(up)
    length *= rnd.uniform(0.85, 1.15)

    tip = origin + forward * length + up * (-curve * length)
    mid_a = origin + forward * (length * 0.35) + up * (-curve * length * 0.10)
    mid_b = origin + forward * (length * 0.70) + up * (-curve * length * 0.35)
    w_a, w_b = width * 0.9, width * 0.5

    v_o = bm.verts.new(origin)
    v_al = bm.verts.new(mid_a - right * (w_a * 0.5))
    v_ar = bm.verts.new(mid_a + right * (w_a * 0.5))
    v_bl = bm.verts.new(mid_b - right * (w_b * 0.5))
    v_br = bm.verts.new(mid_b + right * (w_b * 0.5))
    v_t = bm.verts.new(tip)

    bm.faces.new((v_o, v_al, v_ar))
    bm.faces.new((v_al, v_bl, v_br, v_ar))
    bm.faces.new((v_bl, v_t, v_br))


def make_frond_object(name, length, width, leaflet_count, droop, seed,
                       curl=0.15, leaflet_len_ratio=0.55, leaflet_curve=0.35,
                       rachis_width_ratio=0.02):
    """Fronda pinnada genérica (hoja de palmera o helecho): un raquis
    central que se curva hacia abajo (droop) con folíolos alternos a ambos
    lados. Construida en espacio local con el raquis creciendo en +Y desde
    el origen; colócala con orient_and_place().
    """
    rnd = seeded_rng(seed)
    bm = bmesh.new()

    rachis_w = max(0.003, width * rachis_width_ratio)
    n_seg = 10
    left_v, right_v = [], []
    for i in range(n_seg + 1):
        t = i / n_seg
        y = length * t
        z = -droop * (t ** 1.6) + curl * math.sin(t * math.pi) * 0.12
        w = rachis_w * (1.0 - 0.5 * t)
        left_v.append(bm.verts.new(Vector((-w * 0.5, y, z))))
        right_v.append(bm.verts.new(Vector((w * 0.5, y, z))))
    for i in range(n_seg):
        bm.faces.new((left_v[i], right_v[i], right_v[i + 1], left_v[i + 1]))

    leaflet_len = width * leaflet_len_ratio
    n_pairs = max(1, leaflet_count // 2)
    for i in range(n_pairs):
        t = (i + 0.6) / (n_pairs + 0.6)
        y = length * t
        z = -droop * (t ** 1.6)
        origin = Vector((0.0, y, z))
        for side in (-1, 1):
            right = Vector((float(side), 0.0, 0.0))
            forward = Vector((side * 0.35, 0.55, -0.25))
            up = Vector((0.0, 0.0, 1.0))
            _add_leaflet(bm, origin, forward, right, up, leaflet_len, width * 0.24,
                         leaflet_curve, rnd)

    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new(name, me)
    link_object(obj)
    return obj


def make_canopy_blobs(name, center, radius_xy, radius_z, count, seed,
                       blob_scale_range=(0.45, 0.7), noise_strength=0.16,
                       subdivisions=2, relax_iterations=2):
    """Copa frondosa hecha de varios «blobs» de hoja (esferas deformadas)
    repartidos dentro de una elipse, para dar volumen real en vez de una
    sola esfera lisa (pide la sección 8 del GDD: siluetas orgánicas).

    El ruido se mantiene suave (noise_strength bajo + relax_iterations) a
    propósito: nada de picos puntiagudos, la copa debe leerse como
    cúmulos redondeados («low-poly pulido»), no como una piedra con hojas.
    """
    rnd = seeded_rng(seed)
    blobs = []
    for i in range(count):
        ang = rnd.uniform(0.0, 2.0 * math.pi)
        r = radius_xy * math.sqrt(rnd.uniform(0.0, 1.0)) * rnd.uniform(0.55, 1.0)
        cx = center[0] + math.cos(ang) * r
        cy = center[1] + math.sin(ang) * r
        cz = center[2] + rnd.uniform(-radius_z * 0.35, radius_z * 0.55)
        rad = radius_xy * rnd.uniform(*blob_scale_range)
        b = make_blob(f'{name}_blob{i}', (cx, cy, cz), rad, seed * 1000 + i,
                       subdivisions=subdivisions, noise_strength=noise_strength,
                       scale=(1.0, 1.0, rnd.uniform(0.7, 1.0)),
                       relax_iterations=relax_iterations)
        blobs.append(b)
    canopy = join_objects(blobs, name)
    merge_by_distance(canopy, dist=0.01)
    return canopy


def join_objects(objects, name):
    """Une una lista de objetos en uno solo (el primero absorbe al resto)."""
    if not objects:
        raise ValueError("join_objects: lista vacía")
    for o in bpy.context.selected_objects:
        o.select_set(False)
    for o in objects:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.object.join()
    objects[0].name = name
    return objects[0]


def merge_by_distance(obj, dist=0.001):
    select_only(obj)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.remove_doubles(threshold=dist)
    bpy.ops.object.mode_set(mode='OBJECT')


# ---------------------------------------------------------------------------
# Medición y exportación
# ---------------------------------------------------------------------------

def triangle_count(obj):
    me = obj.data
    total = 0
    for poly in me.polygons:
        total += max(0, len(poly.vertices) - 2)
    return total


def dimensions_cm(obj):
    """Dimensiones del bounding box en centímetros (1 m Blender = 100 cm).

    Se calculan a mano a partir de las coordenadas de vértice en espacio
    mundo en vez de leer obj.dimensions, para no depender de que el
    depsgrafo haya refrescado ese valor cacheado tras las últimas
    operaciones de bmesh/curva.
    """
    me = obj.data
    if not me.vertices:
        return (0.0, 0.0, 0.0)
    mat = obj.matrix_world
    xs, ys, zs = [], [], []
    for v in me.vertices:
        w = mat @ v.co
        xs.append(w.x)
        ys.append(w.y)
        zs.append(w.z)
    return ((max(xs) - min(xs)) * 100.0,
            (max(ys) - min(ys)) * 100.0,
            (max(zs) - min(zs)) * 100.0)


# ---------------------------------------------------------------------------
# Geometría: cápsulas ahusadas de eje arbitrario (kit de fauna por piezas)
# ---------------------------------------------------------------------------
#
# A diferencia de make_curved_trunk (pensado para troncos/ramas curvados vía
# curva Bezier biselada), el kit de fauna necesita piezas RÍGIDAS —patas,
# cuello, cola, morro, alas, aletas, garras— con el origen del objeto en el
# pivote de la articulación y la malla ya orientada en su pose de reposo
# final, sin aplicar ninguna rotación al objeto (rotation = identidad):
# make_tapered_capsule construye los vértices directamente a lo largo de
# «direction» partiendo de (0, 0, 0), así el FBX exportado deja el pivote
# exacto donde el C++ lo necesita para reconstruir la jerarquía con
# animals.json (offset de traslación pura respecto al padre).

def _orthonormal_frame(direction, up_hint=None):
    """Base ortonormal (fwd, right, up) a partir de un eje arbitrario,
    usada por make_tapered_capsule para generar anillos de sección sin
    tocar la transformación del objeto (ver nota arriba)."""
    fwd = Vector(direction)
    if fwd.length < 1e-8:
        fwd = Vector((0.0, 0.0, 1.0))
    fwd.normalize()
    if up_hint is None:
        up_hint = Vector((0.0, 0.0, 1.0)) if abs(fwd.z) < 0.9 else Vector((0.0, 1.0, 0.0))
    else:
        up_hint = Vector(up_hint)
    up_ortho = up_hint - fwd * up_hint.dot(fwd)
    if up_ortho.length < 1e-6:
        alt = Vector((1.0, 0.0, 0.0))
        up_ortho = alt - fwd * alt.dot(fwd)
    up_ortho.normalize()
    right = fwd.cross(up_ortho).normalized()
    up_final = right.cross(fwd).normalized()
    return fwd, right, up_final


def make_tapered_capsule(name, direction, length, radii, segments=8,
                          cap_start=True, cap_end=True, dome=0.55, up_hint=None):
    """Cápsula ahusada a lo largo de un eje arbitrario, con el origen del
    objeto en (0, 0, 0) —el pivote de la pieza— y la malla ya orientada en
    su pose de reposo: no hace falta reorientar el objeto después (sin
    transform_apply), así que el FBX exportado deja el pivote exacto en la
    articulación, tal y como pide el kit de fauna por piezas.

    «radii» es una lista de 2 o más radios muestreados a intervalos
    regulares entre el origen y la punta (longitud «length» en esa misma
    unidad de escena, metros); cada radio puede ser un float (sección
    circular) o un par (rx, ry) para sección elíptica —aplanada—, útil en
    alas, aletas y colas de pez. Los remates son un único vértice polar
    desplazado dome*radio a lo largo del eje: aproxima una semiesfera
    barata sin anillos adicionales, suficiente para el low-poly pulido del
    encargo.
    """
    if len(radii) < 2:
        raise ValueError('make_tapered_capsule: se necesitan al menos 2 radios')
    fwd, right, up = _orthonormal_frame(direction, up_hint)
    n = len(radii)
    bm = bmesh.new()
    rings = []
    for i in range(n):
        t = i / (n - 1)
        center = fwd * (length * t)
        r = radii[i]
        rx, ry = r if isinstance(r, (tuple, list)) else (r, r)
        ring = []
        for j in range(segments):
            ang = 2.0 * math.pi * j / segments
            p = center + right * (rx * math.cos(ang)) + up * (ry * math.sin(ang))
            ring.append(bm.verts.new(p))
        rings.append(ring)

    for i in range(n - 1):
        for j in range(segments):
            j2 = (j + 1) % segments
            bm.faces.new((rings[i][j], rings[i][j2], rings[i + 1][j2], rings[i + 1][j]))

    if cap_start:
        rx0, ry0 = (radii[0] if isinstance(radii[0], (tuple, list)) else (radii[0], radii[0]))
        apex = bm.verts.new(-fwd * (dome * max(rx0, ry0)))
        for j in range(segments):
            j2 = (j + 1) % segments
            bm.faces.new((apex, rings[0][j2], rings[0][j]))
    if cap_end:
        rxN, ryN = (radii[-1] if isinstance(radii[-1], (tuple, list)) else (radii[-1], radii[-1]))
        apex = bm.verts.new(fwd * length + fwd * (dome * max(rxN, ryN)))
        for j in range(segments):
            j2 = (j + 1) % segments
            bm.faces.new((apex, rings[-1][j], rings[-1][j2]))

    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new(name, me)
    link_object(obj)
    return obj


# ---------------------------------------------------------------------------
# Geometría: mallas orgánicas vía modificador Skin (kit de fauna, 2ª pasada)
# ---------------------------------------------------------------------------
#
# make_tapered_capsule (arriba) construye cápsulas a mano anillo a anillo:
# rápido y predecible, pero cada pieza queda como un tubo de caras planas
# con una costura visible donde encaja con la siguiente -el aspecto de
# «palo segmentado» que el encargo pide sustituir-. El modificador Skin de
# Blender resuelve exactamente este problema: a partir de un esqueleto de
# vértices (un «palillo») con un radio por nodo, genera él solo una
# superficie tubular YA suave y redondeada, con remates abombados
# naturales y (más importante) RAMAS: si tres o más aristas comparten un
# vértice (un pecho con dos patas y un cuello saliendo del mismo punto de
# la columna), el modificador suelda esa unión en una sola superficie
# continua sin costura, en vez de tres tubos que se tocan. Es la técnica
# estándar para animales estilizados «tipo amigurumi» (low-poly pero con
# siluetas redondeadas y adorables) que pide el encargo.
#
# Aquí se usa a nivel de PIEZA (cada hueso sigue siendo su propio objeto
# con su propio pivote, para no romper el esqueleto de animals.json), no a
# nivel de especie entera: build_blob_piece/build_capsule_piece/build_chain
# (animals/rig.py) construyen cada esqueleto de Skin ligeramente más largo
# de lo estrictamente necesario para que se HUNDA dentro de la pieza padre
# en la articulación (solape), de forma que el remate redondeado que el
# modificador pone en cada extremo quede oculto dentro del volumen de la
# pieza vecina en vez de leerse como una costura.


def make_skin_mesh(name, verts_local, edges, radii, subsurf_levels=1,
                    decimate_ratio=None, root_indices=None):
    """Malla orgánica a partir de un esqueleto de vértices: «verts_local»
    (lista de (x, y, z) en metros, espacio local del objeto) y «edges»
    (pares de índices) definen el «palillo»; «radii» es una lista paralela
    a verts_local de radios elípticos (rx, ry) en metros para el
    modificador Skin. subsurf_levels añade una Subdivision Surface antes
    de aplicar ambos modificadores (mesh real); decimate_ratio (opcional)
    recorta el resultado al presupuesto de triángulos después de subdividir
    -el modificador Skin ya genera pocos triángulos por nodo, pero
    subsurf los multiplica x4 por nivel-. root_indices marca nodos como
    «raíz» (un remate más plano en vez de redondeado del todo; útil en el
    extremo que se solapa dentro del padre, para que no añada un bulto
    extra de más)."""
    me = bpy.data.meshes.new(name)
    me.from_pydata([tuple(v) for v in verts_local], [tuple(e) for e in edges], [])
    me.update()
    obj = bpy.data.objects.new(name, me)
    link_object(obj)

    # el layer «skin_vertices» no existe en la malla hasta que el objeto
    # tiene un modificador Skin (se crea perezosamente); por eso el
    # modificador se añade ANTES de poder tocar los radios por nodo.
    select_only(obj)
    skin_mod = obj.modifiers.new('Skin', 'SKIN')

    roots = root_indices or set()
    for i, r in enumerate(radii):
        rx, ry = r if isinstance(r, (tuple, list)) else (r, r)
        sv = me.skin_vertices[0].data[i]
        sv.radius = (rx, ry)
        if i in roots:
            sv.use_root = True

    sub_mod = None
    if subsurf_levels > 0:
        sub_mod = obj.modifiers.new('Subsurf', 'SUBSURF')
        sub_mod.levels = subsurf_levels
        sub_mod.render_levels = subsurf_levels
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=skin_mod.name)
    if sub_mod is not None:
        bpy.ops.object.modifier_apply(modifier=sub_mod.name)

    if decimate_ratio is not None and decimate_ratio < 1.0:
        dec = obj.modifiers.new('Decimate', 'DECIMATE')
        dec.ratio = decimate_ratio
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=dec.name)

    return obj


def make_skin_capsule(name, direction, length, radii, subsurf_levels=1,
                       decimate_ratio=None, root_start=False, root_end=False,
                       up_hint=None):
    """Sustituye a make_tapered_capsule pieza a pieza: «radii» es una lista
    de 2 o más radios (float o (rx, ry)) muestreados a intervalos
    regulares entre el origen (pivote) y la punta a lo largo de
    «direction», igual que antes -mismo contrato-, pero cada muestra es
    ahora un nodo del esqueleto de Skin en vez de un anillo de vértices
    hecho a mano, así que las transiciones de grosor quedan suaves y los
    remates redondeados por construcción. «up_hint» no cambia el resultado
    -el modificador Skin no expone control de «roll» por Python-; se
    conserva solo para no romper firmas de llamada existentes."""
    fwd = Vector(direction)
    if fwd.length < 1e-8:
        fwd = Vector((0.0, 0.0, 1.0))
    fwd.normalize()
    n = len(radii)
    verts = [tuple(fwd * (length * i / (n - 1))) for i in range(n)]
    edges = [(i, i + 1) for i in range(n - 1)]
    roots = set()
    if root_start:
        roots.add(0)
    if root_end:
        roots.add(n - 1)
    return make_skin_mesh(name, verts, edges, radii, subsurf_levels=subsurf_levels,
                           decimate_ratio=decimate_ratio, root_indices=roots)


def make_skin_blob(name, center, radii, subsurf_levels=2, decimate_ratio=None,
                    extra_nodes=None, axis='x', end_taper=0.6):
    """Bulto orgánico (cuerpo, cabeza, caparazón...): una cadena de 3 nodos
    de Skin a lo largo de «axis» (por defecto X, «adelante» en todo el kit)
    centrada en «center» (metros), con el nodo central al radio elíptico
    pleno «radii»=(rx, ry, rz) -rx controla la separación entre nodos
    (medio «largo» del bulto), (ry, rz) son ancho y alto de la sección- y
    los dos nodos de los extremos a «end_taper» de ese radio, para un
    bulto alargado y redondeado sin las esquinas picudas de un único nodo
    aislado (el modificador Skin no da un control de «radio en el eje del
    hueso» independiente para un nodo sin aristas, así que un solo nodo
    con radios muy distintos en cada eje degenera en un cono en vez de un
    esferoide). «extra_nodes» (opcional) es una lista de (offset_xyz,
    (rx, ry, rz)) para añadir bultos secundarios soldados al nodo CENTRAL
    -por ejemplo el morro de un perro saliendo del cráneo- en una única
    superficie continua, en vez de la unión visible entre dos blobs
    separados de make_blob."""
    rx, ry, rz = radii
    ax = {'x': Vector((1, 0, 0)), 'y': Vector((0, 1, 0)), 'z': Vector((0, 0, 1))}[axis]
    c = Vector(center)
    verts = [tuple(c - ax * rx), tuple(c), tuple(c + ax * rx)]
    radii_list = [(ry * end_taper, rz * end_taper), (ry, rz), (ry * end_taper, rz * end_taper)]
    edges = [(0, 1), (1, 2)]
    if extra_nodes:
        for offset, r2 in extra_nodes:
            idx = len(verts)
            verts.append(tuple(c + Vector(offset)))
            r2x, r2y, r2z = r2
            radii_list.append((r2y, r2z))
            edges.append((1, idx))
    return make_skin_mesh(name, verts, edges, radii_list, subsurf_levels=subsurf_levels,
                           decimate_ratio=decimate_ratio)


# ---------------------------------------------------------------------------
# Ojos pintados (kit de fauna): bulto de geometría + color de vértice negro
# ---------------------------------------------------------------------------

def add_eyes(obj, eye_centers, eye_radius, subdivisions=1, seed=0):
    """Añade un bulto de ojo (esfera pequeña, make_blob sin ruido) por cada
    centro local en «eye_centers» y los une a «obj». Devuelve el objeto ya
    unido; los centros siguen siendo válidos en el espacio local del
    objeto resultante porque bpy.ops.object.join conserva el espacio local
    del objeto activo (el primero de la lista). Pinta los ojos DESPUÉS de
    llamar a esto, con with_eye_dots() envolviendo el color_fn de la pieza,
    antes de set_vertex_colors."""
    eyes = [make_blob(f'{obj.name}_Eye{i}', c, eye_radius, seed=seed + i,
                       subdivisions=subdivisions, noise_strength=0.0)
            for i, c in enumerate(eye_centers)]
    merged = join_objects([obj] + eyes, obj.name)
    merge_by_distance(merged, dist=0.0004)
    return merged


def with_eye_dots(base_fn, eye_centers, eye_radius, eye_color=(0.02, 0.02, 0.03),
                   highlight_color=(0.92, 0.92, 0.94), highlight_ratio=0.32):
    """Envuelve un color_fn: pinta casi negros los vértices a distancia
    <= eye_radius de cualquier centro en «eye_centers» (los bultos que
    añade add_eyes), delegando en base_fn para el resto de la pieza. Añade
    además un pequeño «brillo» (catchlight) casi blanco desplazado hacia
    arriba y adelante en cada ojo -el toque de vida que pide un estilo
    «animado, adorable» en vez de un punto negro plano-. El canal alfa de
    los ojos se deja a 0.0 (sin uso en piezas de fauna salvo en los
    nadadores de una sola malla, donde alpha ya codifica la posición en
    la columna y las mallas de nadador no llevan add_eyes)."""
    centers = [Vector(c) for c in eye_centers]
    hl_offset = Vector((eye_radius * 0.30, 0.0, eye_radius * 0.40))
    hl_radius = eye_radius * highlight_ratio
    hl_centers = [c + hl_offset for c in centers]

    def fn(v):
        for hc in hl_centers:
            if (v.co - hc).length <= hl_radius:
                return (*highlight_color, 0.0)
        for c in centers:
            if (v.co - c).length <= eye_radius:
                return (*eye_color, 0.0)
        return base_fn(v)
    return fn


def export_fbx(filepath, objects):
    """Exporta objetos a un FBX listo para Unreal:
        - apply_unit_scale=True   -> 1 m Blender = 100 uu Unreal.
        - axis_forward='-Z', axis_up='Y' (convención estándar del exportador
          FBX de Blender para motores de videojuegos: Unreal reinterpreta
          estos ejes FBX como su propio Z-arriba/X-adelante al importar).
        - mesh_smooth_type='FACE' conserva las normales suaves horneadas
          por shade_smooth_auto en vez de recalcularlas por smoothing group.
        - colors_type='LINEAR' porque el atributo «Col» se escribe ya en
          espacio lineal (no es una textura sRGB que haya que reconvertir).
    """
    for o in bpy.context.selected_objects:
        o.select_set(False)
    for o in objects:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]

    bpy.ops.export_scene.fbx(
        filepath=filepath,
        check_existing=False,
        use_selection=True,
        global_scale=1.0,
        apply_unit_scale=True,
        apply_scale_options='FBX_SCALE_ALL',
        axis_forward='-Z',
        axis_up='Y',
        object_types={'MESH'},
        use_mesh_modifiers=True,
        mesh_smooth_type='FACE',
        colors_type='LINEAR',
        prioritize_active_color=True,
        use_triangles=True,
        use_tspace=True,
        bake_anim=False,
        path_mode='COPY',
        embed_textures=False,
    )


# ---------------------------------------------------------------------------
# Materiales genéricos (para kits hermanos: props, animales...) — no toca
# _MATERIAL_DEFS/MATERIAL_NAMES del kit de vegetación, solo añade una
# fábrica de materiales con el mismo grafo de nodos para nombres/colores
# arbitrarios que decide el llamador.
# ---------------------------------------------------------------------------

def get_material_ext(name, base_color, roughness, metallic=0.0, alpha_blend=False):
    """Como get_material(), pero para materiales fuera del kit estable de
    vegetación: nombre y color los define el llamador (p.ej. el kit de
    props con M_Wood/M_Metal/M_Fabric/M_Stone/M_Glass/M_Paper/M_Leaf). Mismo
    grafo de nodos: Attribute «Col» multiplicado por un color base
    constante -> Base Color, para que el tinte de vértice siga funcionando
    igual que en el resto del proyecto."""
    if name in bpy.data.materials:
        return bpy.data.materials[name]
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = nt.nodes.get('Principled BSDF')

    attr = nt.nodes.new('ShaderNodeAttribute')
    attr.attribute_name = 'Col'
    attr.attribute_type = 'GEOMETRY'

    base = nt.nodes.new('ShaderNodeRGB')
    base.outputs[0].default_value = (base_color[0], base_color[1], base_color[2], 1.0)

    mix = nt.nodes.new('ShaderNodeMixRGB')
    mix.blend_type = 'MULTIPLY'
    mix.inputs['Fac'].default_value = 1.0
    nt.links.new(base.outputs[0], mix.inputs['Color1'])
    nt.links.new(attr.outputs['Color'], mix.inputs['Color2'])
    nt.links.new(mix.outputs[0], bsdf.inputs['Base Color'])

    bsdf.inputs['Roughness'].default_value = roughness
    if 'Metallic' in bsdf.inputs:
        bsdf.inputs['Metallic'].default_value = metallic

    if alpha_blend:
        mat.blend_method = 'BLEND'
        mat.show_transparent_back = False
        if 'Alpha' in bsdf.inputs:
            bsdf.inputs['Alpha'].default_value = 0.35
            nt.links.new(attr.outputs['Alpha'], bsdf.inputs['Alpha'])
    return mat


# ---------------------------------------------------------------------------
# Geometría: primitivas de props (cajas, cilindros/conos, loft de anillos,
# tubos huecos) — el kit de vegetación no las necesitaba (todo era troncos,
# frondas y blobs), pero el kit de props sí: fuselajes, cascos, torres,
# cajas, barriles...
# ---------------------------------------------------------------------------

def make_box(name, size, center=(0.0, 0.0, 0.0)):
    """Caja simple (8 vértices). size=(sx, sy, sz) en metros; centrada en
    «center». Base de cajas, mesas, cabañas, piezas de construcción..."""
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    cx, cy, cz = center
    for v in bm.verts:
        v.co.x *= size[0]
        v.co.y *= size[1]
        v.co.z *= size[2]
        v.co += Vector((cx, cy, cz))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new(name, me)
    link_object(obj)
    return obj


def make_cylinder(name, radius, depth, segments=16, center=(0.0, 0.0, 0.0),
                   cap_ends=True, radius2=None):
    """Cilindro (o cono/tronco de cono si radius2 se indica) a lo largo de
    Z, centrado en «center»."""
    bm = bmesh.new()
    bmesh.ops.create_cone(
        bm, cap_ends=cap_ends, cap_tris=False, segments=segments,
        radius1=radius, radius2=(radius if radius2 is None else radius2),
        depth=depth)
    cx, cy, cz = center
    for v in bm.verts:
        v.co += Vector((cx, cy, cz))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new(name, me)
    link_object(obj)
    return obj


def point_ring(center, radius, segments, normal='z', start_angle=0.0):
    """Anillo de puntos (list[Vector]) en el plano perpendicular a
    «normal» ('x'/'y'/'z'), para construir loft a mano con ring_loft() o
    make_tube()."""
    pts = []
    for i in range(segments):
        a = start_angle + 2.0 * math.pi * i / segments
        u, w = math.cos(a) * radius, math.sin(a) * radius
        if normal == 'z':
            pts.append(Vector(center) + Vector((u, w, 0.0)))
        elif normal == 'y':
            pts.append(Vector(center) + Vector((u, 0.0, w)))
        else:
            pts.append(Vector(center) + Vector((0.0, u, w)))
    return pts


def ring_loft(name, rings_pts, cap_start=False, cap_end=False, skip_fn=None):
    """Construye una malla uniendo anillos de puntos consecutivos con caras
    (loft): rings_pts es una lista de anillos (cada uno list[Vector], todos
    con el mismo número de puntos). Pensado para fuselajes, cascos, torres y
    cualquier perfil que varíe de sección en sección.

    skip_fn(ring_index, point_index) -> bool: si se indica, omite la cara
    entre ese anillo y el siguiente en esa posición angular — sirve para
    abrir huecos (cabina del Albatros, puertas, ventanas) sin recurrir a
    una operación booleana.
    """
    bm = bmesh.new()
    vert_rings = [[bm.verts.new(p) for p in ring_pts] for ring_pts in rings_pts]
    n = len(vert_rings[0])
    for r in range(len(vert_rings) - 1):
        a, b = vert_rings[r], vert_rings[r + 1]
        for i in range(n):
            if skip_fn is not None and skip_fn(r, i):
                continue
            i2 = (i + 1) % n
            bm.faces.new((a[i], a[i2], b[i2], b[i]))
    if cap_start:
        bm.faces.new(list(reversed(vert_rings[0])))
    if cap_end:
        bm.faces.new(vert_rings[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new(name, me)
    link_object(obj)
    return obj


def make_tube(name, height, segments, outer_radii, inner_radii,
              cap_bottom=True, cap_top=False, z0=0.0):
    """Tubo hueco (pared con espesor) a lo largo de Z: pared exterior,
    pared interior (normales hacia adentro) y tapas de anillo opcionales.
    outer_radii/inner_radii son listas de radios (un valor por nivel; la
    altura se reparte uniformemente entre z0 y z0+height). Pensado para
    volúmenes transitables por dentro: la torre del faro, pozos, chimeneas.
    """
    n_levels = len(outer_radii)
    if n_levels != len(inner_radii) or n_levels < 2:
        raise ValueError('make_tube: outer_radii/inner_radii deben tener la misma '
                          'longitud (>=2 niveles)')
    bm = bmesh.new()
    outer_rings, inner_rings = [], []
    for li in range(n_levels):
        z = z0 + height * li / (n_levels - 1)
        outer_rings.append([bm.verts.new(Vector((
            math.cos(2.0 * math.pi * i / segments) * outer_radii[li],
            math.sin(2.0 * math.pi * i / segments) * outer_radii[li], z)))
            for i in range(segments)])
        inner_rings.append([bm.verts.new(Vector((
            math.cos(2.0 * math.pi * i / segments) * inner_radii[li],
            math.sin(2.0 * math.pi * i / segments) * inner_radii[li], z)))
            for i in range(segments)])

    for li in range(n_levels - 1):
        a, b = outer_rings[li], outer_rings[li + 1]
        for i in range(segments):
            i2 = (i + 1) % segments
            bm.faces.new((a[i], a[i2], b[i2], b[i]))
        a, b = inner_rings[li], inner_rings[li + 1]
        for i in range(segments):
            i2 = (i + 1) % segments
            bm.faces.new((b[i], b[i2], a[i2], a[i]))  # normal hacia el eje

    if cap_bottom:
        a, b = outer_rings[0], inner_rings[0]
        for i in range(segments):
            i2 = (i + 1) % segments
            bm.faces.new((a[i], a[i2], b[i2], b[i]))
    if cap_top:
        a, b = outer_rings[-1], inner_rings[-1]
        for i in range(segments):
            i2 = (i + 1) % segments
            bm.faces.new((b[i], b[i2], a[i2], a[i]))

    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new(name, me)
    link_object(obj)
    return obj


def boolean_cut(obj, cutter, delete_cutter=True, solver='EXACT'):
    """Recorte booleano (DIFFERENCE) aplicado como modificador. Se reserva
    para huecos irregulares donde construir el hueco a mano (skip_fn de
    ring_loft, o cajas encajadas) no compensa: p.ej. los 32 agujeros de la
    brújula estelar sobre un disco ya deformado por ruido."""
    select_only(obj)
    mod = obj.modifiers.new('Cut', 'BOOLEAN')
    mod.operation = 'DIFFERENCE'
    mod.object = cutter
    mod.solver = solver
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=mod.name)
    if delete_cutter:
        bpy.data.objects.remove(cutter, do_unlink=True)


# ---------------------------------------------------------------------------
# Color de vértice: grabado en relieve (petroglifos)
# ---------------------------------------------------------------------------

def _dist_point_segment_xy(p, a, b):
    """Distancia de un punto 3D a un segmento, proyectados ambos sobre XY
    (para grabar trazos en una losa que crece en X/Y con Z ~ profundidad)."""
    a2 = Vector((a[0], a[1], 0.0))
    b2 = Vector((b[0], b[1], 0.0))
    p2 = Vector((p.x, p.y, 0.0))
    ab = b2 - a2
    if ab.length_squared < 1e-12:
        return (p2 - a2).length
    t = max(0.0, min(1.0, (p2 - a2).dot(ab) / ab.length_squared))
    proj = a2 + ab * t
    return (p2 - proj).length


def carve_strokes(obj, strokes, width, depth):
    """Hunde la malla a lo largo de -normal cerca de una lista de trazos
    (cada trazo es una polilínea de puntos locales (x, y) sobre el plano de
    la losa) para grabar relieve hundido tipo petroglifo. Devuelve un dict
    {vertex_index: profundidad 0..1} para poder pintar el fondo del grabado
    más claro con groove_tint()."""
    me = obj.data
    bm = bmesh.new()
    bm.from_mesh(me)
    bm.normal_update()
    depths = {}
    for v in bm.verts:
        best = 1e9
        for stroke in strokes:
            for i in range(len(stroke) - 1):
                d = _dist_point_segment_xy(v.co, stroke[i], stroke[i + 1])
                if d < best:
                    best = d
        t = max(0.0, 1.0 - best / width) if width > 0 else 0.0
        if t > 0.0:
            v.co -= v.normal * depth * t
        depths[v.index] = t
    bm.to_mesh(me)
    bm.free()
    me.update()
    return depths


def groove_tint(base_rgb, light_rgb, depths, alpha=0.0, jitter=0.03, rnd=None):
    """Color_fn para losas grabadas: interpola de base_rgb (piedra sin
    tocar) a light_rgb (fondo del grabado, más claro/erosionado) según la
    profundidad devuelta por carve_strokes()."""
    cache = {}

    def fn(v):
        if v.index not in cache:
            cache[v.index] = rnd.uniform(-jitter, jitter) if (jitter and rnd is not None) else 0.0
        j = cache[v.index]
        t = depths.get(v.index, 0.0)
        r = _clamp01(base_rgb[0] + (light_rgb[0] - base_rgb[0]) * t + j)
        g = _clamp01(base_rgb[1] + (light_rgb[1] - base_rgb[1]) * t + j)
        b = _clamp01(base_rgb[2] + (light_rgb[2] - base_rgb[2]) * t + j)
        return (r, g, b, alpha)
    return fn
