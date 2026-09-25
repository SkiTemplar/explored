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
