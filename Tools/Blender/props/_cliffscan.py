"""
_cliffscan.py — pipeline de estilizado a partir de escaneos CC0 (Poly Haven)
para las formaciones grandes del kit de acantilados
(Tools/Blender/props/rocks_cliffs.py): paredes, espolones, farallones y el
arco marino. Los bloques/cantos/losas de AcantiladoBloques NO pasan por
aqui (se aprobaron tal cual, ver _cliffkit.py).

Los escaneos originales (glTF 1k, geometria unicamente — las texturas
fotograficas no se usan, el look final es color de vertice + material
triplanar M_Stone) viven en Tools/Blender/props/.cache/rocks_cc0/<id>/,
fuera de git (regla `.cache/` de .gitignore). Atribucion y licencia en
docs/art/rocas/README.md.

Tecnica (encargo de arte 2026-09-27, segunda pasada tras rechazar la
version 100% procedural): voxel remesh -> suavizado ligero -> decimate
planar -> bisel, para pasar de detalle fotogrametrico de alta frecuencia a
facetas grandes y limpias tipo Sea of Thieves. Las paredes anchas se
montan combinando 2-3 escaneos que se solapan y se funden en un solo
remesh; los farallones estiran un escaneo en vertical; el arco recorta un
bloque de un escaneo grande y le resta un tunel curvado (booleano).
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
from mathutils import Matrix, Vector  # noqa: E402

CACHE_DIR = os.path.join(os.path.dirname(__file__), '.cache', 'rocks_cc0')

# (id de Poly Haven, url de la pagina) — para el README de atribucion.
SOURCE_ASSETS = {
    'namaqualand_cliff_01': 'https://polyhaven.com/a/namaqualand_cliff_01',
    'namaqualand_cliff_02': 'https://polyhaven.com/a/namaqualand_cliff_02',
    'coastal_cliff_04': 'https://polyhaven.com/a/coastal_cliff_04',
    'namaqualand_boulder_02': 'https://polyhaven.com/a/namaqualand_boulder_02',
    'moon_rock_01': 'https://polyhaven.com/a/moon_rock_01',
}


def import_scan(asset_id, lod_substring=None):
    """Importa el glTF CC0 cacheado de un escaneo y devuelve UN objeto de
    malla (varios trozos del gltf se unen; si `lod_substring` filtra por
    nombre —los rocks con varios LOD, p.ej. moon_rock_01— se queda solo con
    los que casan). Sin materiales (las texturas fotograficas no se
    cachean: el look final es color de vertice + M_Stone triplanar)."""
    path = os.path.join(CACHE_DIR, asset_id, asset_id + '.gltf')
    if not os.path.isfile(path):
        raise FileNotFoundError(
            f"Falta el escaneo CC0 en cache: {path}\n"
            f"Descargarlo de {SOURCE_ASSETS.get(asset_id, '(Poly Haven, CC0)')} "
            f"(glTF 1k) y colocar el .gltf + .bin ahi antes de generar este kit."
        )
    before = set(bpy.data.objects.keys())
    bpy.ops.import_scene.gltf(filepath=path)
    after = set(bpy.data.objects.keys())
    new_objs = [bpy.data.objects[n] for n in (after - before)]
    meshes = [o for o in new_objs if o.type == 'MESH']
    if lod_substring:
        picked = [o for o in meshes if lod_substring in o.name]
        if picked:
            for o in meshes:
                if o not in picked:
                    bpy.data.objects.remove(o, do_unlink=True)
            meshes = picked
    obj = C.join_objects(meshes, 'Scan') if len(meshes) > 1 else meshes[0]
    obj.data.materials.clear()
    C.select_only(obj)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    keep_largest_island(obj)
    return obj


def keep_largest_island(obj):
    """Los escaneos de Poly Haven traen decenas de islas sueltas (ruido de
    la reconstruccion fotogrametrica: polvo, vegetacion, fragmentos
    desconectados de la roca principal — un escaneo real de
    namaqualand_cliff_02 trae 170 islas, la mayor solo el 6% de los
    vertices). Sin filtrarlas, el voxel remesh las funde/deja como
    confeti disperso en vez de una unica formacion solida. Se queda solo
    con la isla de mas vertices (la roca) y borra el resto ANTES de
    cualquier otro procesado."""
    me = obj.data
    bm = bmesh.new()
    bm.from_mesh(me)
    # glTF duplica vertices en cada costura de UV/normal dura: sin soldar
    # primero, una superficie continua se ve como cientos de "islas"
    # (cada region entre costuras) aunque este perfectamente conectada en
    # el espacio 3D — eso, no ruido real de fotogrametria, era la causa de
    # que el filtro de islas se comiera casi toda la roca.
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    bm.verts.ensure_lookup_table()

    visited = set()
    best_verts = []
    for seed in bm.verts:
        if seed.index in visited:
            continue
        stack = [seed]
        island = []
        while stack:
            v = stack.pop()
            if v.index in visited:
                continue
            visited.add(v.index)
            island.append(v)
            for e in v.link_edges:
                other = e.other_vert(v)
                if other.index not in visited:
                    stack.append(other)
        if len(island) > len(best_verts):
            best_verts = island

    keep = set(v.index for v in best_verts)
    to_delete = [v for v in bm.verts if v.index not in keep]
    bmesh.ops.delete(bm, geom=to_delete, context='VERTS')
    bm.normal_update()
    bm.to_mesh(me)
    bm.free()
    me.update()
    return obj


def center_and_ground(obj):
    """Centra la huella XY en el origen y apoya el punto mas bajo en z=0,
    para que combinar/recortar/escalar varios escaneos sea predecible."""
    me = obj.data
    xs = [v.co.x for v in me.vertices]
    ys = [v.co.y for v in me.vertices]
    zs = [v.co.z for v in me.vertices]
    cx = (max(xs) + min(xs)) / 2.0
    cy = (max(ys) + min(ys)) / 2.0
    me.transform(Matrix.Translation((-cx, -cy, -min(zs))))
    me.update()
    return obj


def fit_dimensions(obj, target_x=None, target_y=None, target_z=None, uniform=False):
    """Escala el objeto (centrado/apoyado, ver center_and_ground) para que
    su caja envolvente alcance las dimensiones pedidas. `uniform=True`
    escala igual en los 3 ejes (a partir del eje mas restrictivo de los
    dados); si no, cada eje se estira por separado (estiramiento anisotropo
    deliberado para los farallones, pedido en el encargo)."""
    me = obj.data
    xs = [v.co.x for v in me.vertices]
    ys = [v.co.y for v in me.vertices]
    zs = [v.co.z for v in me.vertices]
    dx = max(1e-4, max(xs) - min(xs))
    dy = max(1e-4, max(ys) - min(ys))
    dz = max(1e-4, max(zs) - min(zs))
    sx = target_x / dx if target_x else 1.0
    sy = target_y / dy if target_y else 1.0
    sz = target_z / dz if target_z else 1.0
    if uniform:
        s = min([v for v in (sx, sy, sz) if v != 1.0], default=1.0)
        sx = sy = sz = s
    me.transform(Matrix.Diagonal((sx, sy, sz, 1.0)))
    me.update()
    return obj


def predecimate_if_heavy(obj, max_polys=250000):
    """Decimate barato (COLLAPSE por ratio) antes de un voxel remesh caro,
    para los escaneos mas pesados (coastal_cliff_04 llega a ~1.5M caras).
    Sin esto el remesh sigue siendo correcto pero mucho mas lento."""
    n = len(obj.data.polygons)
    if n <= max_polys:
        return obj
    C.select_only(obj)
    dec = obj.modifiers.new('PreDecimate', 'DECIMATE')
    dec.decimate_type = 'COLLAPSE'
    dec.ratio = max(0.02, max_polys / n)
    bpy.ops.object.modifier_apply(modifier=dec.name)
    return obj


def crop_to_box(obj, size, center=(0.0, 0.0, 0.0)):
    """Recorta `obj` a la interseccion booleana con una caja: saca un
    "sillar" manejable de un escaneo enorme (p.ej. coastal_cliff_04, 87 m de
    ancho) en vez de cargar la pieza entera cada vez."""
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=center)
    cutter = bpy.context.object
    cutter.scale = size
    C.select_only(cutter)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)

    C.select_only(obj)
    mod = obj.modifiers.new('Crop', 'BOOLEAN')
    mod.operation = 'INTERSECT'
    mod.object = cutter
    mod.solver = 'EXACT'
    bpy.ops.object.modifier_apply(modifier=mod.name)

    bpy.data.objects.remove(cutter, do_unlink=True)
    return obj


def auto_voxel_size(objs, divisions=48.0, min_size=0.05):
    """Tamano de voxel proporcional a la dimension mayor ACTUAL de los
    objetos (antes de deformarlos al tamano final): un valor fijo en
    metros no tenia sentido para escaneos de escalas nativas muy distintas
    (una roca de 2 m y un acantilado de 20 m)."""
    biggest = 0.0
    for obj in objs:
        me = obj.data
        xs = [v.co.x for v in me.vertices]
        ys = [v.co.y for v in me.vertices]
        zs = [v.co.z for v in me.vertices]
        if not xs:
            continue
        biggest = max(biggest, max(xs) - min(xs), max(ys) - min(ys), max(zs) - min(zs))
    return max(min_size, biggest / divisions)


def fuse_and_stylize(objs, voxel_size, planar_angle_deg=14.0, smooth_factor=0.4,
                      smooth_iterations=2, name='Scan'):
    """Une (con solape, si son varios) y estiliza un grupo de trozos de
    escaneo: voxel remesh (limpia la densidad fotogrametrica Y funde en una
    sola superficie los trozos solapados — asi se "combinan 2-3 escaneos"
    en una pared ancha sin costura visible) -> suavizado ligero -> decimate
    planar (facetas grandes, no ruido de alta frecuencia). El bisel se deja
    para DESPUES de fijar las dimensiones finales exactas (finish_rock),
    para que su anchura no salga deformada por un reescalado no uniforme
    posterior."""
    obj = C.join_objects(objs, name) if len(objs) > 1 else objs[0]
    obj.name = name

    C.select_only(obj)
    # Un escaneo es una lamina de UN solo lado (sin trasera: ~16% de sus
    # aristas son de borde, no manifold) y el voxel remesh de Blender
    # necesita un solido con dentro/fuera bien definido — sobre una lamina
    # abierta se rompia en decenas de islas sueltas aunque la entrada fuera
    # una unica superficie conectada. Darle grosor con Solidify antes del
    # remesh la cierra en un solido delgado y arregla eso.
    solidify = obj.modifiers.new('PreSolidify', 'SOLIDIFY')
    solidify.thickness = voxel_size * 2.5
    solidify.offset = -1.0
    bpy.ops.object.modifier_apply(modifier=solidify.name)

    remesh = obj.modifiers.new('Voxel', 'REMESH')
    remesh.mode = 'VOXEL'
    remesh.voxel_size = voxel_size
    remesh.adaptivity = 0.0
    bpy.ops.object.modifier_apply(modifier=remesh.name)

    if smooth_iterations > 0:
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.mesh.vertices_smooth(factor=smooth_factor, repeat=smooth_iterations)
        bpy.ops.object.mode_set(mode='OBJECT')

    dec = obj.modifiers.new('PlanarDecimate', 'DECIMATE')
    dec.decimate_type = 'DISSOLVE'
    dec.angle_limit = math.radians(planar_angle_deg)
    bpy.ops.object.modifier_apply(modifier=dec.name)
    return obj


def boolean_arch_negative_cut(obj, span, pier_z, rise, leg_inset, extrude_depth,
                               n_points=14, wobble=0.0, seed=0):
    """Resta el hueco de un arco marino: perfil 2D en el plano XZ (sube por
    dentro de la pata izquierda, sigue el intrados del arco, baja por la
    pata derecha, cerrado por abajo en z=0) extruido en Y mas alla del
    grosor del bloque de escaneo, para que el hueco atraviese de cara a
    cara de verdad — un tubo-capsula centrado en el interior (primer
    intento) dejaba un hueco cerrado que se comia casi todo el bloque al
    restarlo."""
    rnd = random.Random(seed)
    half = span * 0.5 - leg_inset
    profile = [(-half, 0.0)]
    for i in range(n_points + 1):
        t = i / n_points
        x = -half + 2.0 * half * t
        z = pier_z + rise * math.sin(t * math.pi) + rnd.uniform(-wobble, wobble)
        profile.append((x, z))
    profile.append((half, 0.0))

    bm = bmesh.new()
    y0 = -extrude_depth / 2.0
    verts0 = [bm.verts.new(Vector((x, y0, z))) for x, z in profile]
    face = bm.faces.new(verts0)
    ret = bmesh.ops.extrude_face_region(bm, geom=[face])
    new_verts = [g for g in ret['geom'] if isinstance(g, bmesh.types.BMVert)]
    bmesh.ops.translate(bm, verts=new_verts, vec=(0.0, extrude_depth, 0.0))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)

    me = bpy.data.meshes.new('ArchCutter')
    bm.to_mesh(me)
    bm.free()
    cutter = bpy.data.objects.new('ArchCutter', me)
    C.link_object(cutter)

    C.select_only(obj)
    mod = obj.modifiers.new('ArchCut', 'BOOLEAN')
    mod.operation = 'DIFFERENCE'
    mod.object = cutter
    mod.solver = 'EXACT'
    bpy.ops.object.modifier_apply(modifier=mod.name)

    bpy.data.objects.remove(cutter, do_unlink=True)
    return obj


def carve_tide_notch(obj, z0, z1, pinch):
    """Hunde hacia el eje central los vertices en la banda [z0, z1]: notch
    de marea en la base de un farallon (referencia caliza karstica El Nido
    / Ha Long — la disolucion por el oleaje socava la base). No usa un
    booleano: empuja los vertices de esa banda hacia el eje XY del objeto,
    con una caida suave en los bordes de la banda."""
    me = obj.data
    xs = [v.co.x for v in me.vertices]
    ys = [v.co.y for v in me.vertices]
    cx = (max(xs) + min(xs)) / 2.0
    cy = (max(ys) + min(ys)) / 2.0
    mid = (z0 + z1) / 2.0
    half = max(1e-6, (z1 - z0) / 2.0)
    for v in me.vertices:
        if z0 <= v.co.z <= z1:
            t = max(0.0, 1.0 - abs((v.co.z - mid) / half)) ** 0.6
            v.co.x = cx + (v.co.x - cx) * (1.0 - pinch * t)
            v.co.y = cy + (v.co.y - cy) * (1.0 - pinch * t)
    me.update()
    return obj


def cleanup_mesh(obj):
    C.select_only(obj)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.dissolve_degenerate(threshold=1e-3)
    bpy.ops.mesh.delete_loose()
    bpy.ops.object.mode_set(mode='OBJECT')
    return obj


# ---------------------------------------------------------------------------
# AO real horneada con el operador nativo (Dirty Vertex Colors), leida como
# diccionario vertice->0..1 (0 = hueco/grieta, 1 = abierto) para combinarla
# con el tinte base en vez de la aproximacion solo-por-normal del resto del
# kit (aqui hay huecos/tuneles reales que un color_fn ciego a la topologia
# no puede ver).
# ---------------------------------------------------------------------------

def bake_cavity_ao(obj, blur_iterations=4):
    me = obj.data
    if 'Col' not in me.color_attributes:
        me.color_attributes.new('Col', 'BYTE_COLOR', 'CORNER')
    me.color_attributes.active_color_name = 'Col'
    C.select_only(obj)
    bpy.ops.object.mode_set(mode='VERTEX_PAINT')
    bpy.ops.paint.vertex_color_dirt(blur_strength=1.0, blur_iterations=blur_iterations,
                                     clean_angle=math.radians(180.0), dirt_angle=math.radians(0.0),
                                     dirt_only=False, normalize=True)
    bpy.ops.object.mode_set(mode='OBJECT')
    attr = me.color_attributes['Col']
    sums, counts = {}, {}
    for poly in me.polygons:
        for li in poly.loop_indices:
            vi = me.loops[li].vertex_index
            sums[vi] = sums.get(vi, 0.0) + attr.data[li].color[0]
            counts[vi] = counts.get(vi, 0) + 1
    return {vi: sums[vi] / counts[vi] for vi in sums}


def scan_rock_tint(base_rgb, ao_dict, seed, rnd, height=None, wet_height=0.0,
                    wet_dark=0.45, dark_rgb=(0.05, 0.05, 0.06), ao_strength=0.85,
                    rim_light=0.10, rim_shadow=0.16, jitter=0.03):
    """Tinte de vertice para piezas basadas en escaneo: AO real (bake_cavity_ao)
    en huecos/grietas/el hueco del arco, base humeda oscura junto al agua y el
    mismo realce de arista por normal que constant_tint/rock_tint usan en el
    resto del kit, para que las piezas escaneadas y las procedurales lean
    igual de familia."""
    cache = {}

    def fn(v):
        if v.index not in cache:
            cache[v.index] = rnd.uniform(-jitter, jitter)
        j = cache[v.index]
        r, g, b = base_rgb

        ao = ao_dict.get(v.index, 1.0)
        dark = (1.0 - ao) * ao_strength
        r = r * (1.0 - dark) + dark_rgb[0] * dark
        g = g * (1.0 - dark) + dark_rgb[1] * dark
        b = b * (1.0 - dark) + dark_rgb[2] * dark

        if wet_height > 0.0 and height:
            wet = max(0.0, 1.0 - v.co.z / wet_height)
            r = r * (1.0 - wet * wet_dark) + dark_rgb[0] * wet * wet_dark
            g = g * (1.0 - wet * wet_dark) + dark_rgb[1] * wet * wet_dark
            b = b * (1.0 - wet * wet_dark) + dark_rgb[2] * wet * wet_dark

        nz = v.normal.z
        boost = nz * rim_light if nz > 0.0 else nz * rim_shadow
        r, g, b = r + boost + j, g + boost + j, b + boost + j
        return (max(0.0, min(1.0, r)), max(0.0, min(1.0, g)), max(0.0, min(1.0, b)), 0.0)
    return fn
