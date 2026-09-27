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
    # El material «__fbx_import» apartado arriba (y la Image Texture que el
    # importador FBX resuelve y carga colgada de él, aparte de la que ya
    # cachea C.get_material) queda huérfano justo aquí. Purgar ahora, no al
    # final de la lámina: en una rejilla de varias mallas se iban
    # acumulando fichero a fichero hasta agotar la memoria de texturas de
    # la GPU («Failed to create GPU texture», materiales a magenta).
    C.purge_orphans()
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
# Láminas de contacto del kit de props (Tools/Blender/props/run_props.py),
# UNA POR GRUPO NARRATIVO en vez de una sola rejilla de 11 filas: la primera
# versión metía las 11 filas en una sola cámara lejana y el resultado era
# ilegible (todo diminuto cerca del horizonte). Cada grupo se renderiza
# ahora en su propia lámina, en rejilla de hasta 4 columnas si tiene más de
# 4 props, con cámara en 3/4 MUY cerca del grupo (no de todo el kit) y luz
# de sol cálida + cielo claro — se pidió explícitamente tras revisar la
# primera lámina ("cámara lejana, todo diminuto... oscuro y apagado").
# ---------------------------------------------------------------------------
PROPS_DIR = os.path.join(BLENDER_DIR, 'props')
if PROPS_DIR not in sys.path:
    sys.path.insert(0, PROPS_DIR)
import _materials as PM  # noqa: E402

EXPORT_DIR_PROPS = os.path.join(REPO_ROOT, 'Art', 'Export', 'Props')
MANIFEST_PATH_PROPS = os.path.join(EXPORT_DIR_PROPS, 'manifest.json')

GROUP_ORDER_PROPS = [
    'Albatros', 'Faro', 'Baliza', 'Halden', 'BrujulaEstelar',
    'Petroglifos', 'Marae', 'Pecio', 'Embarcaciones', 'Construccion',
    'ObjetosPequenos', 'KitPalma', 'KitBambu', 'KitMadera', 'KitPiedra',
    'MobiliarioBase', 'RuinasMarae', 'RuinasTallas', 'Tesoros', 'Items',
    'AcantiladoFormaciones', 'AcantiladoBloques',
]
GROUP_TARGET_HEIGHT_PROPS = {
    'Albatros': 2.2, 'Faro': 2.2, 'Baliza': 1.3, 'Halden': 1.6,
    'BrujulaEstelar': 1.4, 'Petroglifos': 0.9, 'Marae': 1.4, 'Pecio': 1.4,
    'Embarcaciones': 1.4, 'Construccion': 1.2, 'ObjetosPequenos': 0.7,
    'KitPalma': 1.4, 'KitBambu': 1.4, 'KitMadera': 1.4, 'KitPiedra': 1.4,
    'MobiliarioBase': 1.4, 'RuinasMarae': 1.4, 'RuinasTallas': 1.4,
    'Tesoros': 0.7, 'Items': 0.7,
    # cada prop se normaliza a esta altura por SU PROPIA dimension mayor
    # (ver _render_props_group): formaciones grandes vs. bloques sueltos
    # solo necesitan alturas de encuadre distintas, no un rango real.
    'AcantiladoFormaciones': 2.2, 'AcantiladoBloques': 1.0,
}
PROPS_GRID_MAX_COLS = 4


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
    C.purge_orphans()  # ver nota en _import_and_fix_materials
    return obj


def _render_props_group(entries, target_height, out_path, max_cols=PROPS_GRID_MAX_COLS):
    """Una lámina cercana para UN grupo narrativo: rejilla de hasta
    `max_cols` columnas (varias filas si el grupo tiene más props que eso).

    Cada prop se normaliza a `target_height` por SU PROPIA dimensión mayor
    (no una escala compartida derivada del miembro más grande del grupo):
    un grupo como Albatros mezcla el avión entero (~11 m) con restos de
    ~1-4 m, y una escala compartida dejaba los restos diminutos en la
    esquina — exactamente el mismo ajuste que ya usa main_animals() para
    fauna, por el mismo motivo. Esta lámina es solo para revisar la FORMA
    y el color de cada prop de cerca, no la escala relativa entre ellos."""
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene

    imported = [_import_and_fix_materials_props(e) for e in entries]
    imported = [o for o in imported if o is not None]
    if not imported:
        return False
    bpy.context.view_layer.update()

    n = len(imported)
    # rejilla lo más cuadrada posible (nunca "N-1 completas + 1 suelta"):
    # con pocas filas y una cámara en 3/4 baja, una fila casi vacía queda
    # detrás de la columna 0 de la fila anterior y las dos piezas se
    # superponen en pantalla por la perspectiva.
    cols = min(max_cols, max(1, math.ceil(math.sqrt(n))))
    rows = math.ceil(n / cols)
    cell_x = target_height * 1.6
    cell_y = target_height * 2.3  # más separación en profundidad que en anchura

    for i, obj in enumerate(imported):
        col, row = i % cols, i // cols
        biggest = max(_bounds_world(obj)[1] - _bounds_world(obj)[0],
                       _bounds_world(obj)[3] - _bounds_world(obj)[2],
                       _bounds_world(obj)[5] - _bounds_world(obj)[4])
        scale_factor = target_height / max(biggest, 1e-4)
        obj.scale = (scale_factor, scale_factor, scale_factor)
        bpy.context.view_layer.update()
        x0, x1, y0, y1, z0, z1 = _bounds_world(obj)
        cx = (col - (cols - 1) / 2.0) * cell_x
        cy = (row - (rows - 1) / 2.0) * cell_y
        obj.location.x += -((x0 + x1) / 2.0) + cx
        obj.location.y += -((y0 + y1) / 2.0) + cy
        obj.location.z += -z0
        bpy.context.view_layer.update()

    grid_w = cols * cell_x
    grid_d = rows * cell_y

    bpy.ops.mesh.primitive_plane_add(size=1.0)
    ground = bpy.context.object
    ground.name = 'Ground'
    ground.scale = (grid_w * 0.75 + 2.5, grid_d * 0.9 + 3.0, 1.0)
    ground.location = (0.0, 0.0, 0.0)
    mat = bpy.data.materials.new('M_Ground')
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get('Principled BSDF')
    # arena/tierra clara neutra en vez del gris apagado de la primera
    # pasada: sube de valor y de saturación para que no absorba luz.
    bsdf.inputs['Base Color'].default_value = (0.82, 0.78, 0.68, 1.0)
    bsdf.inputs['Roughness'].default_value = 0.9
    ground.data.materials.append(mat)

    # Sol cálido de tarde como luz clave (más energía y más cálido que la
    # primera pasada) + relleno frío suave desde el otro lado + un rebote
    # tenue desde abajo para que las caras en sombra no se vayan a negro.
    bpy.ops.object.light_add(type='SUN', location=(-grid_w * 0.5, -grid_d * 1.2, target_height * 3.0))
    key = bpy.context.object
    key.data.energy = 1.8
    key.data.color = (1.0, 0.85, 0.62)
    key.data.angle = math.radians(6.0)
    _point_camera(key, Vector((0.0, 0.0, target_height * 0.35)))

    bpy.ops.object.light_add(type='SUN', location=(grid_w * 0.6, grid_d * 0.6, target_height * 2.0))
    fill = bpy.context.object
    fill.data.energy = 0.5
    fill.data.color = (0.66, 0.78, 1.0)
    _point_camera(fill, Vector((0.0, 0.0, target_height * 0.35)))

    bpy.ops.object.light_add(type='SUN', location=(0.0, grid_d * 0.3, -target_height))
    bounce = bpy.context.object
    bounce.data.energy = 0.2
    bounce.data.color = (0.9, 0.85, 0.75)
    _point_camera(bounce, Vector((0.0, 0.0, target_height * 0.5)))

    # Cielo claro (no el gris oscuro por defecto de un World vacío) pero sin
    # sobreexponer los props de color pálido (piedra clara, lona crema): la
    # primera versión de esta lámina (luces + fondo muy fuertes) los dejaba
    # lavados casi a blanco puro en vez de leerse con su color.
    world = bpy.data.worlds.new('World')
    scene.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes.get('Background')
    bg.inputs['Color'].default_value = (0.68, 0.78, 0.88, 1.0)
    bg.inputs['Strength'].default_value = 0.55

    # Cámara en 3/4 MUY cerca de este grupo (no de todo el kit): el
    # encuadre se ajusta al ancho/profundidad de ESTA rejilla, nunca a la
    # del kit completo.
    fov = math.radians(42.0)
    aspect = 1600.0 / 1200.0
    dist_for_width = (grid_w / 2.0 + 0.4) / math.tan(fov / 2.0) / aspect
    distance = max(dist_for_width * 1.08, target_height * 1.6, grid_d * 0.9)

    az = math.radians(32.0)
    cam_x = -math.sin(az) * distance
    cam_y = -math.cos(az) * distance
    # más elevada que una vista a la altura del ojo: con varias filas en
    # profundidad, una cámara baja las apila unas sobre otras en pantalla
    # por la perspectiva (parecía una sola pieza flotando encima de otra).
    cam_z = target_height * 1.5 + grid_d * 0.25
    bpy.ops.object.camera_add(location=(cam_x, cam_y, cam_z))
    cam = bpy.context.object
    cam.data.lens_unit = 'FOV'
    cam.data.angle = fov
    _point_camera(cam, Vector((0.0, 0.0, target_height * 0.28)))
    scene.camera = cam

    scene.render.engine = 'BLENDER_EEVEE'
    scene.render.resolution_x = 1600
    scene.render.resolution_y = 1200
    scene.render.image_settings.file_format = 'PNG'
    scene.render.filepath = out_path
    try:
        # el trazado de rayos (SSR/SSGI) de EEVEE Next hacía que un objeto
        # metálico grande y brillante (p.ej. el domo de la sala de la
        # lámpara del faro) sobreexpusiera TODA la lámina por luz
        # rebotada — se detectó comparando la lámina de Faro (lavada casi
        # a blanco) con las de Albatros/Halden (bien expuestas) con el
        # mismo código de luces. Desactivarlo la deja consistente.
        scene.eevee.use_raytracing = False
    except Exception:
        pass
    try:
        scene.eevee.taa_render_samples = 64
    except Exception:
        pass

    bpy.ops.render.render(write_still=True)
    print(f'[render_preview] escrito {out_path}')
    return True


# ---------------------------------------------------------------------------
# Hojas de contacto de vegetación (verificación del encargo de rehacer el
# kit desde cero): una lámina cercana POR FAMILIA en vez de la rejilla
# lejana de main() (pensada para ver el conjunto, no el detalle de cada
# hoja/tarjeta) + una escena de conjunto («claro de selva»). Van a
# docs/art/vegetacion/, no a Art/Export/ (que no se versiona): son la
# evidencia visual del informe, deben sobrevivir en el repo.
# ---------------------------------------------------------------------------
DOCS_ART_DIR = os.path.join(REPO_ROOT, 'docs', 'art', 'vegetacion')
VEG_CONTACT_ROWS = ['palm', 'tree', 'shrub', 'grass', 'debris']
VEG_CONTACT_HEIGHT = {'palm': 3.0, 'tree': 3.0, 'shrub': 2.2, 'grass': 1.4, 'debris': 1.6}


def main_contact_sheets():
    os.makedirs(DOCS_ART_DIR, exist_ok=True)
    with open(MANIFEST_PATH, 'r', encoding='utf-8') as f:
        manifest = json.load(f)

    by_category = {}
    for entry in manifest['meshes']:
        by_category.setdefault(entry['category'], []).append(entry)

    for cat in VEG_CONTACT_ROWS:
        entries = by_category.get(cat, [])
        if not entries:
            continue
        out_path = os.path.join(DOCS_ART_DIR, f'hoja_contacto_{cat}.png')
        _render_vegetation_group(entries, VEG_CONTACT_HEIGHT[cat], out_path)


def _render_vegetation_group(entries, target_height, out_path, max_cols=4):
    """Lámina cercana de UNA familia de vegetación: misma lógica de rejilla/
    cámara/luces que _render_props_group (grid cuadrada, 3/4 cerca, sol
    cálido + relleno frío + rebote), pero reimportando con
    _import_and_fix_materials (reconstruye M_Leaf/M_Grass/M_Bark ya
    texturizados con C.get_material en vez de los genéricos del FBX)."""
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene

    imported = [_import_and_fix_materials(e) for e in entries]
    imported = [o for o in imported if o is not None]
    if not imported:
        return False
    bpy.context.view_layer.update()

    n = len(imported)
    cols = min(max_cols, max(1, math.ceil(math.sqrt(n))))
    rows = math.ceil(n / cols)
    cell_x = target_height * 1.6
    cell_y = target_height * 2.3

    for i, obj in enumerate(imported):
        col, row = i % cols, i // cols
        b = _bounds_world(obj)
        biggest = max(b[1] - b[0], b[3] - b[2], b[5] - b[4])
        scale_factor = target_height / max(biggest, 1e-4)
        obj.scale = (scale_factor, scale_factor, scale_factor)
        bpy.context.view_layer.update()
        x0, x1, y0, y1, z0, z1 = _bounds_world(obj)
        cx = (col - (cols - 1) / 2.0) * cell_x
        cy = (row - (rows - 1) / 2.0) * cell_y
        obj.location.x += -((x0 + x1) / 2.0) + cx
        obj.location.y += -((y0 + y1) / 2.0) + cy
        obj.location.z += -z0
        bpy.context.view_layer.update()

    grid_w = cols * cell_x
    grid_d = rows * cell_y

    bpy.ops.mesh.primitive_plane_add(size=1.0)
    ground = bpy.context.object
    ground.name = 'Ground'
    ground.scale = (grid_w * 0.75 + 2.5, grid_d * 0.9 + 3.0, 1.0)
    ground.location = (0.0, 0.0, 0.0)
    mat = bpy.data.materials.new('M_Ground')
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value = (0.36, 0.30, 0.20, 1.0)  # tierra de selva, no arena
    bsdf.inputs['Roughness'].default_value = 0.92
    ground.data.materials.append(mat)

    # Sol tropical claro y DOMINANTE, con poca ambiental gris (encargo
    # 2026-09-27, 4ª pasada: "revísalo con una luz de sol clara... porque
    # esa luz engaña" — el cielo gris plano de antes oscurecía y
    # deslavaba el color real de las masas de copa).
    bpy.ops.object.light_add(type='SUN', location=(-grid_w * 0.5, -grid_d * 1.2, target_height * 3.0))
    key = bpy.context.object
    key.data.energy = 2.8
    key.data.color = (1.0, 0.93, 0.80)
    key.data.angle = math.radians(4.0)
    _point_camera(key, Vector((0.0, 0.0, target_height * 0.35)))

    bpy.ops.object.light_add(type='SUN', location=(grid_w * 0.6, grid_d * 0.6, target_height * 2.0))
    fill = bpy.context.object
    fill.data.energy = 0.35
    fill.data.color = (0.65, 0.80, 1.0)
    _point_camera(fill, Vector((0.0, 0.0, target_height * 0.35)))

    bpy.ops.object.light_add(type='SUN', location=(0.0, grid_d * 0.3, -target_height))
    bounce = bpy.context.object
    bounce.data.energy = 0.15
    bounce.data.color = (0.85, 0.9, 0.75)
    _point_camera(bounce, Vector((0.0, 0.0, target_height * 0.5)))

    world = bpy.data.worlds.new('World')
    scene.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes.get('Background')
    bg.inputs['Color'].default_value = (0.55, 0.75, 0.98, 1.0)
    bg.inputs['Strength'].default_value = 0.22

    fov = math.radians(42.0)
    aspect = 1600.0 / 1200.0
    dist_for_width = (grid_w / 2.0 + 0.4) / math.tan(fov / 2.0) / aspect
    distance = max(dist_for_width * 1.08, target_height * 1.6, grid_d * 0.9)

    az = math.radians(28.0)
    cam_x = -math.sin(az) * distance
    cam_y = -math.cos(az) * distance
    cam_z = target_height * 1.35 + grid_d * 0.22
    bpy.ops.object.camera_add(location=(cam_x, cam_y, cam_z))
    cam = bpy.context.object
    cam.data.lens_unit = 'FOV'
    cam.data.angle = fov
    _point_camera(cam, Vector((0.0, 0.0, target_height * 0.30)))
    scene.camera = cam

    scene.render.engine = 'BLENDER_EEVEE'
    scene.render.resolution_x = 1600
    scene.render.resolution_y = 1200
    scene.render.image_settings.file_format = 'PNG'
    scene.render.filepath = out_path
    try:
        scene.eevee.use_raytracing = False
    except Exception:
        pass
    try:
        scene.eevee.taa_render_samples = 64
    except Exception:
        pass

    bpy.ops.render.render(write_still=True)
    print(f'[render_preview] escrito {out_path}')
    return True


def build_scatter_clearing():
    """Escena de conjunto: un claro de selva con 10-20 plantas mezcladas
    (palmeras, árboles, arbustos, hierba, restos de suelo) sobre un suelo de
    tierra/hierba, luz de sol tropical — la lámina que de verdad responde a
    «¿esto se ve bien junto, como en el juego?» en vez de piezas aisladas."""
    os.makedirs(DOCS_ART_DIR, exist_ok=True)
    with open(MANIFEST_PATH, 'r', encoding='utf-8') as f:
        manifest = json.load(f)
    by_name = {e['name']: e for e in manifest['meshes']}

    # Composición a mano: 1-2 árboles de fondo, una palmera protagonista,
    # sotobosque variado y hierba/restos en primer plano — no un grid, una
    # escena con profundidad de campo compositiva (cerca grande, lejos chico).
    placements = [
        # (nombre_malla, x, y, escala_altura_m)
        ('SM_JungleTreeWide_01', -3.2, 7.0, 9.5),
        ('SM_JungleTreeUnderstory_01', 3.8, 5.5, 3.4),
        ('SM_PalmCoconut_01', 1.2, 2.6, 6.5),
        ('SM_PalmCoconut_02', -4.5, 3.4, 5.4),
        ('SM_ShrubBanana_01', -1.5, 0.9, 2.1),
        ('SM_ShrubMonstera_01', 2.0, -0.2, 0.9),
        ('SM_ShrubFernTree_01', 4.2, 1.6, 2.8),
        ('SM_ShrubFlowering_01', -2.8, -0.6, 1.1),
        ('SM_ShrubPandanus_01', 3.0, -1.2, 1.6),
        ('SM_ShrubBamboo_01', -5.2, 2.6, 4.4),
        ('SM_GrassTallA_01', 0.5, -1.4, 1.3),
        ('SM_GrassTallB_01', -0.8, -0.9, 1.4),
        ('SM_GrassLowWide_01', 1.8, -1.8, 0.3),
        ('SM_FlowerTropical_01', -0.2, -2.0, 0.22),
        ('SM_FlowerTropical_01', 1.2, -1.6, 0.24),
        ('SM_DebrisLogMoss_01', -1.0, -2.3, 0.55),
        ('SM_DebrisStump_01', 2.5, -2.5, 0.6),
        ('SM_DebrisCoconuts_01', 0.2, -1.1, 0.22),
    ]

    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    rng_seed = 7

    import random as _random
    rnd = _random.Random(rng_seed)

    imported = []
    for mesh_name, px, py, target_h in placements:
        entry = by_name.get(mesh_name)
        if entry is None:
            continue
        obj = _import_and_fix_materials(entry)
        if obj is None:
            continue
        bpy.context.view_layer.update()
        x0, x1, y0, y1, z0, z1 = _bounds_world(obj)
        biggest_z = max(z1 - z0, 1e-4)
        scale_factor = target_h / biggest_z
        obj.scale = (scale_factor, scale_factor, scale_factor)
        obj.rotation_euler.z = rnd.uniform(0.0, 6.28318)
        bpy.context.view_layer.update()
        x0, x1, y0, y1, z0, z1 = _bounds_world(obj)
        obj.location.x += -((x0 + x1) / 2.0) + px
        obj.location.y += -((y0 + y1) / 2.0) + py
        obj.location.z += -z0
        bpy.context.view_layer.update()
        imported.append(obj)

    if not imported:
        print('[render_preview] build_scatter_clearing: nada que renderizar')
        return False

    bpy.ops.mesh.primitive_plane_add(size=1.0)
    ground = bpy.context.object
    ground.name = 'Ground'
    ground.scale = (14.0, 14.0, 1.0)
    ground.location = (0.0, 2.0, 0.0)
    mat = bpy.data.materials.new('M_Ground')
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value = (0.20, 0.28, 0.10, 1.0)
    bsdf.inputs['Roughness'].default_value = 0.95
    ground.data.materials.append(mat)

    bpy.ops.object.light_add(type='SUN', location=(-8.0, -10.0, 16.0))
    key = bpy.context.object
    key.data.energy = 4.5
    key.data.color = (1.0, 0.93, 0.78)
    key.data.angle = math.radians(4.0)
    _point_camera(key, Vector((0.0, 2.0, 3.0)))

    bpy.ops.object.light_add(type='SUN', location=(10.0, 6.0, 10.0))
    fill = bpy.context.object
    fill.data.energy = 0.4
    fill.data.color = (0.60, 0.78, 1.0)
    _point_camera(fill, Vector((0.0, 2.0, 3.0)))

    world = bpy.data.worlds.new('World')
    scene.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes.get('Background')
    bg.inputs['Color'].default_value = (0.55, 0.75, 0.95, 1.0)
    bg.inputs['Strength'].default_value = 0.25

    fov = math.radians(62.0)
    bpy.ops.object.camera_add(location=(0.0, -11.0, 2.0))
    cam = bpy.context.object
    cam.data.lens_unit = 'FOV'
    cam.data.angle = fov
    _point_camera(cam, Vector((0.0, 3.5, 3.6)))
    scene.camera = cam

    scene.render.engine = 'BLENDER_EEVEE'
    scene.render.resolution_x = 1920
    scene.render.resolution_y = 1080
    scene.render.image_settings.file_format = 'PNG'
    out_path = os.path.join(DOCS_ART_DIR, 'escena_claro_selva.png')
    scene.render.filepath = out_path
    try:
        scene.eevee.use_raytracing = True
    except Exception:
        pass
    try:
        scene.eevee.taa_render_samples = 96
    except Exception:
        pass

    bpy.ops.render.render(write_still=True)
    print(f'[render_preview] escrito {out_path}')
    return True


def main_props():
    with open(MANIFEST_PATH_PROPS, 'r', encoding='utf-8') as f:
        manifest = json.load(f)

    by_group = {g: [] for g in GROUP_ORDER_PROPS}
    for entry in manifest['meshes']:
        by_group.setdefault(entry['group'], []).append(entry)

    for group in GROUP_ORDER_PROPS:
        entries = by_group.get(group, [])
        if not entries:
            continue
        out_path = os.path.join(EXPORT_DIR_PROPS, f'preview_{group}.png')
        _render_props_group(entries, GROUP_TARGET_HEIGHT_PROPS[group], out_path)



if __name__ == '__main__':
    main()
    main_contact_sheets()
    build_scatter_clearing()
    main_props()
