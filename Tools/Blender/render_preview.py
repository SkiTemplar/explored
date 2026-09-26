"""
render_preview.py — lámina de contacto de todo el kit.

Se ejecuta dentro de Blender 5.2 DESPUÉS de run_all.py:

    "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" ^
        -b --factory-startup --python Tools\\Blender\\render_preview.py

Reimporta cada FBX del manifest y las coloca en una rejilla de 5 filas (una
por familia: palmera, árbol, arbusto, roca, hierba) sobre un suelo neutro
con luz cálida. Cada fila se normaliza a una altura de referencia común
para que la lámina sea legible pese a que las familias abarcan desde 10 cm
(hierba) hasta 20 m (árbol de dosel): es solo un ajuste visual del
contacto, no toca los FBX exportados.

Nota importante verificada en Blender 5.2: el importador de FBX de Blender
NO reconstruye un atributo de color de vértice «Col» de tipo BYTE_COLOR (se
comprobó exportando e reimportando: la lista color_attributes vuelve
vacía), así que common.py ya lo escribe como FLOAT_COLOR, que sí sobrevive
el viaje de ida y vuelta. Aun así, el material reimportado pierde el grafo
de nodos (el multiplicador color-de-vértice × color base): FBX solo exporta
propiedades básicas de material, no grafos de shader. Por eso este script
reconstruye los 4 materiales estables con common.get_material() en cada
malla reimportada, en vez de fiarse del material que trae el FBX.
"""

import json
import math
import os
import sys

import bpy
from mathutils import Vector

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
BLENDER_DIR = os.path.join(REPO_ROOT, 'Tools', 'Blender')
LIB_DIR = os.path.join(BLENDER_DIR, 'lib')
EXPORT_DIR = os.path.join(REPO_ROOT, 'Art', 'Export', 'Meshes')
MANIFEST_PATH = os.path.join(EXPORT_DIR, 'manifest.json')
PREVIEW_PATH = os.path.join(EXPORT_DIR, 'preview.png')

for _p in (LIB_DIR,):
    if _p not in sys.path:
        sys.path.insert(0, _p)
import common as C  # noqa: E402

ROW_ORDER = ['palm', 'tree', 'shrub', 'rock', 'grass', 'debris']

# Altura de referencia (m) a la que se normaliza cada fila. El árbol se deja
# grande a propósito: es la familia con más detalle nuevo (ramificación real,
# contrafuertes, raíces de manglar) y merece protagonismo en la lámina.
ROW_TARGET_HEIGHT = {
    'palm': 2.4,
    'tree': 4.4,
    'shrub': 1.7,
    'rock': 0.9,
    'grass': 0.6,
    'debris': 0.9,
}
# Rocas y restos de suelo son mallas «tumbadas»: normalizarlas por su altura
# Z las dispararía de tamaño (un tronco caído mide 0,5 m de alto pero 4 m de
# largo). Se normalizan por su dimensión mayor en su lugar.
ROW_SIZE_AXIS = {
    'palm': 'z', 'tree': 'z', 'shrub': 'z', 'grass': 'z',
    'rock': 'max', 'debris': 'max',
}
ROW_DEPTH = 3.3           # separación en Y entre filas
ITEM_MARGIN = 0.5         # separación horizontal entre mallas de una misma fila


def _bounds_world(obj):
    mat = obj.matrix_world
    corners = [mat @ Vector(c) for c in obj.bound_box]
    xs = [c.x for c in corners]
    ys = [c.y for c in corners]
    zs = [c.z for c in corners]
    return min(xs), max(xs), min(ys), max(ys), min(zs), max(zs)


def _point_camera(cam_obj, target):
    direction = target - cam_obj.location
    cam_obj.rotation_euler = direction.to_track_quat('-Z', 'Y').to_euler()


def _import_and_fix_materials(entry):
    fpath = os.path.join(EXPORT_DIR, entry['file'])
    before = set(bpy.data.objects.keys())
    bpy.ops.import_scene.fbx(filepath=fpath, colors_type='LINEAR')
    after = set(bpy.data.objects.keys())
    new_objs = [bpy.data.objects[n] for n in (after - before)]
    meshes = [o for o in new_objs if o.type == 'MESH']
    if not meshes:
        return None
    obj = meshes[0]

    # El material que trae el FBX se llama igual que el nuestro (p.ej.
    # «M_Rock») pero es un Principled BSDF genérico sin el nodo Attribute
    # que multiplica por «Col»: el FBX no exporta grafos de nodos, solo
    # propiedades básicas. Si no se aparta, C.get_material() lo encontraría
    # por nombre en bpy.data.materials y reutilizaría ese genérico en vez de
    # construir el nuestro. Se aparta primero y así el nombre queda libre.
    for m in list(obj.data.materials):
        if m is not None and m.name in C.MATERIAL_NAMES:
            m.name = m.name + '__fbx_import'

    obj.data.materials.clear()
    for slot_name in entry['material_slots']:
        obj.data.materials.append(C.get_material(slot_name))
    return obj


def main():
    with open(MANIFEST_PATH, 'r', encoding='utf-8') as f:
        manifest = json.load(f)

    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene

    by_category = {cat: [] for cat in ROW_ORDER}
    for entry in manifest['meshes']:
        by_category.setdefault(entry['category'], []).append(entry)

    max_row_width = 0.0
    row_ys = []
    for row_i, cat in enumerate(ROW_ORDER):
        entries = by_category.get(cat, [])
        if not entries:
            continue
        row_y = row_i * ROW_DEPTH
        row_ys.append(row_y)
        target_height = ROW_TARGET_HEIGHT[cat]
        axis = ROW_SIZE_AXIS[cat]

        # primera pasada: importar y medir para conocer el factor de escala
        # de normalización de la fila (todas las mallas de la fila comparten
        # el mismo factor, así que las proporciones relativas dentro de la
        # familia se conservan). El eje de medida depende de la categoría:
        # 'z' para lo que crece hacia arriba, 'max' para lo que está tumbado
        # (rocas, restos de suelo) y así no se dispara de tamaño.
        imported = []
        biggest = 1e-6
        for entry in entries:
            obj = _import_and_fix_materials(entry)
            if obj is None:
                continue
            bpy.context.view_layer.update()
            x0, x1, y0, y1, z0, z1 = _bounds_world(obj)
            metric = (z1 - z0) if axis == 'z' else max(x1 - x0, y1 - y0, z1 - z0)
            biggest = max(biggest, metric)
            imported.append(obj)

        scale_factor = target_height / biggest

        cursor_x = 0.0
        for obj in imported:
            obj.scale = (scale_factor, scale_factor, scale_factor)
            bpy.context.view_layer.update()
            x0, x1, y0, y1, z0, z1 = _bounds_world(obj)
            width = x1 - x0

            obj.location.x += -((x0 + x1) / 2.0) + cursor_x + width / 2.0
            obj.location.y += -((y0 + y1) / 2.0) + row_y
            obj.location.z += -z0
            bpy.context.view_layer.update()

            cursor_x += width + ITEM_MARGIN

        max_row_width = max(max_row_width, cursor_x - ITEM_MARGIN)

    total_depth = row_ys[-1] if row_ys else 0.0
    center_x = max_row_width / 2.0
    center_y = total_depth / 2.0
    max_target_height = max(ROW_TARGET_HEIGHT.get(c, 1.0) for c in ROW_ORDER if by_category.get(c))

    # --- suelo neutro ---
    bpy.ops.mesh.primitive_plane_add(size=1.0)
    ground = bpy.context.object
    ground.name = 'Ground'
    ground.scale = (max_row_width * 0.85 + 4.0, total_depth * 1.15 + 8.0, 1.0)
    ground.location = (center_x, center_y, 0.0)
    mat = bpy.data.materials.new('M_Ground')
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value = (0.64, 0.60, 0.52, 1.0)
    bsdf.inputs['Roughness'].default_value = 0.95
    ground.data.materials.append(mat)

    # --- luz: sol cálido de relleno cálido + relleno frío suave ---
    bpy.ops.object.light_add(type='SUN', location=(center_x - max_row_width * 0.3, center_y - 5.0, 8.0))
    key = bpy.context.object
    key.data.energy = 1.5
    key.data.color = (1.0, 0.87, 0.64)
    key.data.angle = math.radians(4.0)
    _point_camera(key, Vector((center_x, center_y, 1.0)))

    bpy.ops.object.light_add(type='SUN', location=(center_x + max_row_width * 0.4, center_y + 4.0, 6.0))
    fill = bpy.context.object
    fill.data.energy = 0.35
    fill.data.color = (0.66, 0.79, 1.0)
    _point_camera(fill, Vector((center_x, center_y, 1.0)))

    world = bpy.data.worlds.new('World')
    scene.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes.get('Background')
    bg.inputs['Color'].default_value = (0.80, 0.82, 0.84, 1.0)
    bg.inputs['Strength'].default_value = 0.18

    # --- cámara: vista 3/4 elevada que abarca toda la rejilla de filas ---
    # La distancia se dimensiona por el ANCHO (lo que de verdad ocupa
    # pantalla en una vista en perspectiva); la profundidad total ya la
    # absorbe la perspectiva + el ángulo de elevación, así que solo suma un
    # margen pequeño en vez de escalar 1:1 con total_depth (eso fue lo que
    # dejó la primera versión de esta lámina demasiado alejada y diminuta).
    fov = math.radians(50.0)
    aspect = 1600.0 / 900.0
    dist_for_width = (max_row_width / 2.0 + 0.6) / math.tan(fov / 2.0) / aspect
    distance = max(dist_for_width * 1.5, 18.0) + total_depth * 0.15

    cam_z = max_target_height * 0.75
    look_y = center_y * 0.62  # sesga la mirada hacia las filas delanteras
    bpy.ops.object.camera_add(location=(center_x, -distance * 0.5, cam_z))
    cam = bpy.context.object
    cam.data.lens_unit = 'FOV'
    cam.data.angle = fov
    _point_camera(cam, Vector((center_x, look_y, max_target_height * 0.25)))
    scene.camera = cam

    # --- render ---
    scene.render.engine = 'BLENDER_EEVEE'
    scene.render.resolution_x = 1600
    scene.render.resolution_y = 900
    scene.render.image_settings.file_format = 'PNG'
    scene.render.filepath = PREVIEW_PATH
    try:
        scene.eevee.use_raytracing = True
    except Exception:
        pass
    try:
        scene.eevee.taa_render_samples = 64
    except Exception:
        pass

    bpy.ops.render.render(write_still=True)
    print(f'[render_preview] escrito {PREVIEW_PATH}')


# ---------------------------------------------------------------------------
# Lámina de contacto del kit de props (Tools/Blender/props/run_props.py).
#
# Duplica la lógica de layout de main() en vez de parametrizarla, a propósito:
# main() es del kit de vegetación y otro agente puede estar tocando ficheros
# vecinos en paralelo, así que se deja intacta y esta función vive aparte.
# La única diferencia real es la fuente de materiales estables (7 del kit de
# props, M.MATERIAL_NAMES de _materials.py, en vez de los 4 de C.MATERIAL_NAMES)
# y que todas las filas se normalizan por su dimensión mayor: el kit de props
# mezcla piezas de mano con estructuras enteras y no todas «crecen hacia
# arriba» como la vegetación.
# ---------------------------------------------------------------------------
PROPS_DIR = os.path.join(BLENDER_DIR, 'props')
if PROPS_DIR not in sys.path:
    sys.path.insert(0, PROPS_DIR)
import _materials as PM  # noqa: E402

EXPORT_DIR_PROPS = os.path.join(REPO_ROOT, 'Art', 'Export', 'Props')
MANIFEST_PATH_PROPS = os.path.join(EXPORT_DIR_PROPS, 'manifest.json')
PREVIEW_PATH_PROPS = os.path.join(EXPORT_DIR_PROPS, 'preview.png')

ROW_ORDER_PROPS = [
    'Albatros', 'Faro', 'Baliza', 'Halden', 'BrujulaEstelar',
    'Petroglifos', 'Marae', 'Pecio', 'Embarcaciones', 'Construccion',
    'ObjetosPequenos',
]
ROW_TARGET_HEIGHT_PROPS = {
    'Albatros': 2.5, 'Faro': 2.5, 'Baliza': 1.5, 'Halden': 1.8,
    'BrujulaEstelar': 1.8, 'Petroglifos': 1.0, 'Marae': 1.8, 'Pecio': 1.8,
    'Embarcaciones': 1.8, 'Construccion': 1.5, 'ObjetosPequenos': 0.8,
}


def _import_and_fix_materials_props(entry):
    fpath = os.path.join(EXPORT_DIR_PROPS, entry['file'])
    before = set(bpy.data.objects.keys())
    bpy.ops.import_scene.fbx(filepath=fpath, colors_type='LINEAR')
    after = set(bpy.data.objects.keys())
    new_objs = [bpy.data.objects[n] for n in (after - before)]
    meshes = [o for o in new_objs if o.type == 'MESH']
    if not meshes:
        return None
    obj = meshes[0]

    for m in list(obj.data.materials):
        if m is not None and m.name in PM.MATERIAL_NAMES:
            m.name = m.name + '__fbx_import'

    obj.data.materials.clear()
    for slot_name in entry['material_slots']:
        obj.data.materials.append(PM.get_material(slot_name))
    return obj


def main_props():
    with open(MANIFEST_PATH_PROPS, 'r', encoding='utf-8') as f:
        manifest = json.load(f)

    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene

    by_group = {g: [] for g in ROW_ORDER_PROPS}
    for entry in manifest['meshes']:
        by_group.setdefault(entry['group'], []).append(entry)

    max_row_width = 0.0
    row_ys = []
    for row_i, group in enumerate(ROW_ORDER_PROPS):
        entries = by_group.get(group, [])
        if not entries:
            continue
        row_y = row_i * ROW_DEPTH
        row_ys.append(row_y)
        target_height = ROW_TARGET_HEIGHT_PROPS[group]

        imported = [_import_and_fix_materials_props(e) for e in entries]
        imported = [o for o in imported if o is not None]
        if not imported:
            continue
        bpy.context.view_layer.update()
        biggest = max((max(_bounds_world(o)[1] - _bounds_world(o)[0],
                            _bounds_world(o)[3] - _bounds_world(o)[2],
                            _bounds_world(o)[5] - _bounds_world(o)[4])
                       for o in imported), default=1.0)
        biggest = max(biggest, 1e-4)

        scale_factor = target_height / biggest

        cursor_x = 0.0
        for obj in imported:
            obj.scale = (scale_factor, scale_factor, scale_factor)
            bpy.context.view_layer.update()
            x0, x1, y0, y1, z0, z1 = _bounds_world(obj)
            width = x1 - x0

            obj.location.x += -((x0 + x1) / 2.0) + cursor_x + width / 2.0
            obj.location.y += -((y0 + y1) / 2.0) + row_y
            obj.location.z += -z0
            bpy.context.view_layer.update()

            cursor_x += width + ITEM_MARGIN

        max_row_width = max(max_row_width, cursor_x - ITEM_MARGIN)

    total_depth = row_ys[-1] if row_ys else 0.0
    center_x = max_row_width / 2.0
    center_y = total_depth / 2.0
    max_target_height = max(ROW_TARGET_HEIGHT_PROPS.get(g, 1.0) for g in ROW_ORDER_PROPS if by_group.get(g))

    bpy.ops.mesh.primitive_plane_add(size=1.0)
    ground = bpy.context.object
    ground.name = 'Ground'
    ground.scale = (max_row_width * 0.85 + 4.0, total_depth * 1.15 + 8.0, 1.0)
    ground.location = (center_x, center_y, 0.0)
    mat = bpy.data.materials.new('M_Ground')
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value = (0.64, 0.60, 0.52, 1.0)
    bsdf.inputs['Roughness'].default_value = 0.95
    ground.data.materials.append(mat)

    bpy.ops.object.light_add(type='SUN', location=(center_x - max_row_width * 0.3, center_y - 5.0, 8.0))
    key = bpy.context.object
    key.data.energy = 1.5
    key.data.color = (1.0, 0.87, 0.64)
    key.data.angle = math.radians(4.0)
    _point_camera(key, Vector((center_x, center_y, 1.0)))

    bpy.ops.object.light_add(type='SUN', location=(center_x + max_row_width * 0.4, center_y + 4.0, 6.0))
    fill = bpy.context.object
    fill.data.energy = 0.35
    fill.data.color = (0.66, 0.79, 1.0)
    _point_camera(fill, Vector((center_x, center_y, 1.0)))

    world = bpy.data.worlds.new('World')
    scene.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes.get('Background')
    bg.inputs['Color'].default_value = (0.80, 0.82, 0.84, 1.0)
    bg.inputs['Strength'].default_value = 0.18

    # El kit de props tiene casi el doble de filas que el de vegetación
    # (11 grupos frente a 6) con objetos mucho más dispares en tamaño real
    # (un petroglifo de 20 cm y un fuselaje de 11 m en la misma lámina), así
    # que esta cámara se aleja y se eleva más para abarcar todas las filas
    # en vez de reutilizar la distancia ajustada para 6 filas de main().
    fov = math.radians(60.0)
    aspect = 1600.0 / 1200.0
    dist_for_width = (max_row_width / 2.0 + 0.6) / math.tan(fov / 2.0) / aspect
    distance = max(dist_for_width * 1.3, 16.0) + total_depth * 0.55

    cam_z = max(total_depth * 0.32, max_target_height * 1.4)
    look_y = center_y
    bpy.ops.object.camera_add(location=(center_x, -distance * 0.5, cam_z))
    cam = bpy.context.object
    cam.data.lens_unit = 'FOV'
    cam.data.angle = fov
    _point_camera(cam, Vector((center_x, look_y, max_target_height * 0.2)))
    scene.camera = cam

    scene.render.engine = 'BLENDER_EEVEE'
    scene.render.resolution_x = 1600
    scene.render.resolution_y = 1200
    scene.render.image_settings.file_format = 'PNG'
    scene.render.filepath = PREVIEW_PATH_PROPS
    try:
        scene.eevee.use_raytracing = True
    except Exception:
        pass
    try:
        scene.eevee.taa_render_samples = 64
    except Exception:
        pass

    bpy.ops.render.render(write_still=True)
    print(f'[render_preview] escrito {PREVIEW_PATH_PROPS}')


if __name__ == '__main__':
    main()
    main_props()
