"""
preview_kit.py — láminas de revisión del kit de construcción modular
(kit_construccion.py) a ESCALA REAL, sin normalizar tamaños: lo que se
revisa aquí es justo que las piezas encajen en la rejilla.

    blender -b --factory-startup --python Tools/Blender/props/preview_kit.py -- \
        [--mode=all|catalog|montage] [--materials=Palm,Wood] [--samples=24]

Escribe en docs/art/modelos/:
    kit-construccion-<material>.png  catálogo de las 12 piezas (4 x 3)
    kit-construccion-montaje.png     una cabaña montada por material con las
                                     mismas medidas de rejilla (detecta piezas
                                     flotando, huecos y solapes).

Render con Cycles en CPU (la nube no tiene GPU; EEVEE necesita OpenGL) y
denoise OIDN, a cámara cercana 3/4. Las PNG se guardan en paleta de 8 bits
comprimida para quedar por debajo de 1 MB.
"""

import math
import os
import sys

import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
for _p in (os.path.join(HERE, '..', 'lib'), HERE):
    if _p not in sys.path:
        sys.path.insert(0, _p)

import common as C  # noqa: E402
import kit_construccion as K  # noqa: E402

OUT_DIR = os.path.join(REPO_ROOT, 'docs', 'art', 'modelos')
MAT_SLUG = {'Palm': 'palma', 'Bamboo': 'bambu', 'Wood': 'madera', 'Stone': 'piedra'}


def _args():
    opts = dict(mode='all', materials=','.join(K.MATERIALS), samples='24', res='1400x940')
    if '--' in sys.argv:
        for a in sys.argv[sys.argv.index('--') + 1:]:
            if a.startswith('--') and '=' in a:
                k, v = a[2:].split('=', 1)
                opts[k] = v
    return opts


def _variant(mat, piece):
    for v in K.VARIANTS:
        if v['material'] == mat and v['piece'] == piece:
            return v
    raise KeyError((mat, piece))


def _place(obj, loc, rot_z_deg=0.0):
    obj.location = loc
    obj.rotation_euler = (0.0, 0.0, math.radians(rot_z_deg))


def _bounds(obj):
    cs = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
    return (min(c.x for c in cs), max(c.x for c in cs), min(c.y for c in cs),
            max(c.y for c in cs), min(c.z for c in cs), max(c.z for c in cs))


def _look_at(obj, target):
    d = Vector(target) - obj.location
    obj.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()


def _frame(cam, loc, target, objs, margin=0.04):
    """Acerca la cámara por su dirección de vista (fija, 3/4) hasta la
    distancia MÍNIMA en que todas las cajas de `objs` caben en el encuadre:
    lo más cerca posible sin cortar piezas en los bordes."""
    from bpy_extras.object_utils import world_to_camera_view
    scene = bpy.context.scene
    corners = [o.matrix_world @ Vector(c) for o in objs for c in o.bound_box]
    center = sum(corners, Vector()) / len(corners)
    target = Vector((center.x, center.y, target.z))
    direction = (loc - target).normalized()
    dist = 2.0
    while dist < 400.0:
        cam.location = target + direction * dist
        _look_at(cam, target)
        bpy.context.view_layer.update()
        ok = True
        for c in corners:
            p = world_to_camera_view(scene, cam, c)
            if not (margin <= p.x <= 1 - margin and margin <= p.y <= 1 - margin and p.z > 0):
                ok = False
                break
        if ok:
            return
        dist *= 1.03


def _stage(extent_x, extent_y, center, cam_loc, cam_target, fov_deg, out_path, samples, res):
    scene = bpy.context.scene
    bpy.ops.mesh.primitive_plane_add(size=1.0, location=(center[0], center[1], -0.001))
    ground = bpy.context.object
    ground.scale = (extent_x, extent_y, 1.0)
    mat = bpy.data.materials.new('M_Ground')
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value = (0.60, 0.47, 0.28, 1.0)  # arena (lineal)
    bsdf.inputs['Roughness'].default_value = 0.95
    ground.data.materials.append(mat)

    bpy.ops.object.light_add(type='SUN', location=(0, 0, 10))
    key = bpy.context.object
    key.data.energy = 2.6
    key.data.color = (1.0, 0.90, 0.74)
    key.data.angle = math.radians(4.0)
    key.rotation_euler = (math.radians(50), 0.0, math.radians(-35))

    world = bpy.data.worlds.new('World')
    scene.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes.get('Background')
    bg.inputs['Color'].default_value = (0.55, 0.72, 0.92, 1.0)
    bg.inputs['Strength'].default_value = 0.5

    bpy.ops.object.camera_add(location=cam_loc)
    cam = bpy.context.object
    cam.data.lens_unit = 'FOV'
    cam.data.angle = math.radians(fov_deg)
    scene.camera = cam
    rx, ry = (int(x) for x in res.split('x'))
    scene.render.resolution_x = rx
    scene.render.resolution_y = ry
    _frame(cam, Vector(cam_loc), Vector(cam_target), [o for o in scene.objects
                                                      if o.type == 'MESH' and o.name != ground.name])

    scene.render.engine = 'CYCLES'
    scene.cycles.device = 'CPU'
    scene.cycles.samples = samples
    scene.cycles.use_denoising = True
    scene.cycles.max_bounces = 4
    try:
        scene.view_settings.look = 'AgX - Punchy'
    except Exception:
        pass
    scene.render.image_settings.file_format = 'PNG'
    scene.render.image_settings.color_mode = 'RGB'
    scene.render.image_settings.compression = 100
    scene.render.filepath = out_path
    bpy.ops.render.render(write_still=True)
    _shrink_png(out_path)
    print(f'[preview_kit] escrito {out_path} ({os.path.getsize(out_path) // 1024} KB)')


def _shrink_png(path, limit=1_000_000):
    """Si la PNG pasa de 1 MB (límite de las previews del repo), la reescala
    un 10 % cada vez con el propio bpy hasta que entra (el Python de
    Blender no trae PIL para cuantizar a paleta)."""
    tries = 0
    while os.path.getsize(path) > limit and tries < 6:
        img = bpy.data.images.load(path, check_existing=False)
        w, h = img.size
        img.scale(int(w * 0.9), int(h * 0.9))
        img.filepath_raw = path
        img.file_format = 'PNG'
        img.save()
        bpy.data.images.remove(img)
        tries += 1


def catalog(mat, samples, res):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    cols, cell_x, cell_y = 4, 4.6, 5.6
    objs = []
    for i, (piece, *_rest) in enumerate(K.PIECES):
        v = _variant(mat, piece)
        rnd = C.seeded_rng(v['seed'])
        obj = K._BUILDERS[piece](mat, rnd, 'SM_' + v['name'])
        col, row = i % cols, i // cols
        bpy.context.view_layer.update()
        x0, x1, y0, y1, z0, _ = _bounds(obj)
        cx = (col - (cols - 1) / 2) * cell_x
        cy = (row - 1) * cell_y
        obj.location = (cx - (x0 + x1) / 2, cy - (y0 + y1) / 2, -z0)
        objs.append(obj)
    out = os.path.join(OUT_DIR, f'kit-construccion-{MAT_SLUG[mat]}.png')
    _stage(40, 30, (0, 0), (-7.5, -18.5, 14.0), (0.5, 0.6, 0.4), 44, out, samples, res)


def _house(mat, ox, oy):
    """Cabaña de 2 celdas + porche con las medidas de la rejilla."""
    def piece(name, loc, rot=0.0):
        v = _variant(mat, name)
        rnd = C.seeded_rng(v['seed'])
        o = K._BUILDERS[name](mat, rnd, f'{mat}_{name}_{loc[0]:.1f}_{loc[1]:.1f}_{loc[2]:.1f}')
        _place(o, (ox + loc[0], oy + loc[1], loc[2]), rot)
        return o

    G, H = K.GRID, K.HALF
    zf = K.FOUND_H
    zw = zf + K.FLOOR_T
    zr = zw + K.WALL_H
    for x in (-H, H, H + G):
        for y in (-H, H):
            piece('Foundation', (x, y, 0.0))
    for x in (-H, H):
        piece('Foundation', (x, -H - G, 0.0))
    for c in ((0, 0), (G, 0), (0, -G)):
        piece('Floor', (c[0], c[1], zf))
    piece('WallDoor', (0, -H, zw))
    piece('WallWindow', (G, -H, zw))
    piece('Wall', (0, H, zw))
    piece('WallWindow', (G, H, zw), 180)
    piece('Wall', (-H, 0, zw), 90)
    piece('WallHalf', (G + H, 0, zw), 90)
    piece('Door', (-K.DOOR_LEAF_W / 2, -H, zw), 70)
    piece('Railing', (-H, -G, zw), 90)
    piece('Railing', (H, -G, zw), 90)
    piece('Railing', (0, -G - H, zw))
    piece('RoofGable', (0, 0, zr))
    piece('RoofGable', (G, 0, zr))


def montage(mats, samples, res):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    spacing = 7.5
    for i, m in enumerate(mats):
        _house(m, (i - (len(mats) - 1) / 2) * spacing - 1.0, 0.0)
    out = os.path.join(OUT_DIR, 'kit-construccion-montaje.png')
    w = spacing * len(mats)
    _stage(w + 16, 26, (0, 0), (-w * 0.28, -w * 0.78, w * 0.3), (0.0, 0.0, 1.6), 42, out, samples, res)


def module_sheet(mod_name, slug, samples, res, cols=4, gap=1.2):
    """Lámina a escala real de todas las variantes de un módulo de props,
    en rejilla cuyas columnas/filas se dimensionan por las cajas reales."""
    import importlib
    bpy.ops.wm.read_factory_settings(use_empty=True)
    mod = importlib.import_module(mod_name)
    objs = [mod.build(v) for v in mod.VARIANTS]
    bpy.context.view_layer.update()
    dims = [(_bounds(o)[1] - _bounds(o)[0], _bounds(o)[3] - _bounds(o)[2]) for o in objs]
    rows = math.ceil(len(objs) / cols)
    col_w = [max([dims[i][0] for i in range(c, len(objs), cols)] + [0]) + gap for c in range(cols)]
    row_d = [max(dims[i][1] for i in range(r * cols, min(len(objs), (r + 1) * cols))) + gap
             for r in range(rows)]
    total_w, total_d = sum(col_w), sum(row_d)
    for i, o in enumerate(objs):
        c, r = i % cols, i // cols
        cx = -total_w / 2 + sum(col_w[:c]) + col_w[c] / 2
        cy = -total_d / 2 + sum(row_d[:r]) + row_d[r] / 2
        x0, x1, y0, y1, z0, _ = _bounds(o)
        o.location = (cx - (x0 + x1) / 2, cy - (y0 + y1) / 2, -z0)
    out = os.path.join(OUT_DIR, f'{slug}.png')
    _stage(total_w + 20, total_d + 20, (0, 0), (-0.45 * total_w, -1.3 * total_d, 0.9 * total_d), (0, 0, 0.3), 40,
           out, samples, res)


def main():
    o = _args()
    os.makedirs(OUT_DIR, exist_ok=True)
    mats = [m for m in o['materials'].split(',') if m]
    samples = int(o['samples'])
    if o['mode'] in ('all', 'catalog'):
        for m in mats:
            catalog(m, samples, o['res'])
    if o['mode'] in ('all', 'montage'):
        montage(mats, samples, o['res'])
    if o['mode'] == 'module':
        module_sheet(o['module'], o['out'], samples, o['res'], cols=int(o.get('cols', '4')))


if __name__ == '__main__':
    main()
