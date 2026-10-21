"""Lámina de revisión del kit K5 Cielo (docs/v3/laminas/k5.png, 1600 x 900).

Importa los FBX exportados (revisa lo entregado) y compone dos planos sobre
un degradado de cielo con colores de la paleta (cielo_dia, cristal y papel):

- Arriba, la vista desde la isla: ojos a 1,7 m sobre el mar, nubes con la
  base a 120 m entre 270 y 520 m de distancia, sol y luna con su diámetro
  angular de juego. El sol de la escena sale de la dirección del disco.
- Abajo, la fila de mallas a la misma escala vista desde abajo y desde el
  lado del sol (ortográfica),
  con el sol y la luna ampliados a Ø 14 m, rotuladas con nombre, tris y ancho.

Las nubes se sombrean con la réplica de `M_Toon` y bandas suaves (instancia
`MI_ToonNube` propuesta); los discos, sin sombreado.
"""

from __future__ import annotations

import math
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any, cast

import bpy

import numpy as np
from bpy_extras.object_utils import world_to_camera_view
from mathutils import Vector

from explored_style import lamina, meshkit, raster
from explored_style.kits.k5 import K5
from explored_style.meshkit import mesh_of
from explored_style.palette import REPO_ROOT, SHEET_PATH, Palette, load_palette

OUT_PATH = REPO_ROOT / 'docs' / 'v3' / 'laminas' / 'k5.png'
WIDTH, HEIGHT = 1600, 900
TOP_H = 560
BOTTOM_H = HEIGHT - TOP_H - 4
EYE = Vector((0.0, 0.0, 1.7))
CLOUD_BASE = 120.0
CLOUD_BANDS = ((0.0, 0.86), (0.1, 0.93), (0.45, 1.0))
FLAT_BANDS = ((0.0, 1.0), (0.1, 1.0), (0.45, 1.0))
SKY_STOPS = ((0.0, 'cielo_dia'), (0.72, 'cristal'), (1.0, 'papel'))
LINEUP_X = 10000.0
LINEUP_GAP = 9.0
DISC_SHOWCASE = 14.0

# Vista desde la isla: nube -> (x lateral, distancia en +Y, giro en grados).
ISLAND: dict[str, tuple[float, float, float]] = {
    'SM_Nube_A': (-300.0, 330.0, 20.0),
    'SM_Nube_B': (150.0, 290.0, -15.0),
    'SM_Nube_C': (-10.0, 500.0, 5.0),
    'SM_Nube_D': (380.0, 520.0, -25.0),
    'SM_Nube_E': (-120.0, 270.0, 8.0),
}
# Discos: (acimut desde +Y hacia +X, elevación, diámetro angular), en grados.
DISCS: dict[str, tuple[float, float, float]] = {
    'SM_DiscoSol': (22.0, 27.0, 5.0),
    'SM_DiscoLuna': (-36.0, 12.0, 3.6),
}
DISC_DISTANCE = 3000.0


@dataclass(frozen=True)
class Shot:
    key: str
    height: int
    ortho: bool


SHOTS = (Shot('isla', TOP_H, False), Shot('fila', BOTTOM_H, True))


def _import(path: Path) -> bpy.types.Object:
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(path))
    new = [o for o in bpy.data.objects if o not in before and o.type == 'MESH']
    if len(new) != 1:
        raise RuntimeError(f'{path.name}: se esperaba 1 malla y hay {len(new)}')
    return new[0]


def _set_bands(mat: bpy.types.Material, bands: tuple[tuple[float, float], ...]) -> bpy.types.Material:
    """Cambia las bandas de la réplica de M_Toon (instancia de material)."""
    tree = mat.node_tree
    if tree is None:
        raise RuntimeError('material sin árbol de nodos')
    ramp = cast(bpy.types.ShaderNodeValToRGB, next(n for n in tree.nodes if n.type == 'VALTORGB'))
    elements = cast(bpy.types.ColorRamp, ramp.color_ramp).elements
    for element, (pos, factor) in zip(elements, bands, strict=True):
        element.position = pos
        cast(Any, element).color = (factor, factor, factor, 1.0)
    return mat


def _sea(pal: Palette) -> meshkit.MeshBuilder:
    b = meshkit.MeshBuilder()
    s = 4000.0
    b.face([b.vert((-s, -200.0, 0.0)), b.vert((s, -200.0, 0.0)), b.vert((s, s, 0.0)), b.vert((-s, s, 0.0))],
           pal.index_of('agua_media'))
    return b


def _direction(azimuth: float, elevation: float) -> Vector:
    az, el = math.radians(azimuth), math.radians(elevation)
    return Vector((math.sin(az) * math.cos(el), math.cos(az) * math.cos(el), math.sin(el)))


def _face(obj: bpy.types.Object, towards: Vector) -> None:
    """Orienta la Z local del disco hacia «towards»."""
    obj.rotation_euler = (towards - obj.location).to_track_quat('Z', 'Y').to_euler()


def _place_island(objs: dict[str, bpy.types.Object]) -> None:
    for name, (x, y, turn) in ISLAND.items():
        objs[name].location = (x, y, CLOUD_BASE)
        objs[name].rotation_euler = (0.0, 0.0, math.radians(turn))
    for name, (az, el, diameter) in DISCS.items():
        obj = objs[name]
        obj.location = EYE + _direction(az, el) * DISC_DISTANCE
        size = 2.0 * DISC_DISTANCE * math.tan(math.radians(diameter) / 2.0)
        obj.scale = (size, size, size)
        _face(obj, EYE)


def _place_lineup(objs: dict[str, bpy.types.Object]) -> None:
    """Fila en el orden del kit hacia -X: la cámara mira desde +Y (lado del
    sol), así que se lee de izquierda a derecha. Discos mirando a +Y."""
    x = LINEUP_X
    for spec in K5:
        obj = objs[spec.name]
        size = DISC_SHOWCASE if not spec.is_cloud else 1.0
        width = obj.dimensions.x * size
        obj.scale = (size, size, size)
        if spec.is_cloud:
            obj.location = (x - width / 2, 0.0, 0.0)
        else:
            obj.location = (x - width / 2, 0.0, 6.0 + width / 2)
            obj.rotation_euler = (math.radians(90.0), 0.0, math.radians(180.0))  # normal +Y, Y local arriba
        x -= width + LINEUP_GAP


def _sun_lamp() -> None:
    lamina._pick(bpy.types.Light, 'type', 'SUN')
    data = cast(bpy.types.SunLight, bpy.data.lights.new('Sol', 'SUN'))
    data.energy = lamina.SUN_STRENGTH
    data.angle = math.radians(2.0)
    sun = bpy.data.objects.new('Sol', data)
    az, el, _ = DISCS['SM_DiscoSol']
    sun.rotation_euler = _direction(az, el).to_track_quat('Z', 'Y').to_euler()
    bpy.context.scene.collection.objects.link(sun)
    world = bpy.data.worlds.new('Negro')
    world.color = (0.0, 0.0, 0.0)
    bpy.context.scene.world = world


def build_scene(pal: Palette, export_dir: Path, sheet_path: Path) -> dict[str, tuple[bpy.types.Object, int]]:
    """Monta las dos escenas (isla y fila, alejadas entre sí). Devuelve
    {nombre: (objeto de la fila, tris)}."""
    meshkit.reset_scene()
    sheet = bpy.data.images.load(str(sheet_path))
    toon = lamina.toon_preview_material(sheet)
    cloud = _set_bands(lamina.toon_preview_material(sheet), CLOUD_BANDS)
    flat = _set_bands(lamina.toon_preview_material(sheet), FLAT_BANDS)
    island: dict[str, bpy.types.Object] = {}
    lineup: dict[str, bpy.types.Object] = {}
    for spec in K5:
        obj = _import(export_dir / f'{spec.asset}.fbx')
        mesh_of(obj).materials.clear()
        mesh_of(obj).materials.append(cloud if spec.is_cloud else flat)
        copy = obj.copy()
        bpy.context.scene.collection.objects.link(copy)
        for o in (obj, copy):
            o.visible_shadow = False
        island[spec.name], lineup[spec.name] = obj, copy
    _place_island(island)
    _place_lineup(lineup)
    sea = meshkit.build_object('Mar', _sea(pal))
    mesh_of(sea).materials.clear()
    mesh_of(sea).materials.append(toon)
    _sun_lamp()
    bpy.context.view_layer.update()
    return {name: (obj, meshkit.triangle_count(obj)) for name, obj in lineup.items()}


def _lineup_bounds(placed: dict[str, tuple[bpy.types.Object, int]]) -> tuple[float, float, float]:
    xs, zs = [], []
    for obj, _ in placed.values():
        for c in obj.bound_box:
            p = obj.matrix_world @ Vector(c)
            xs.append(p.x)
            zs.append(p.z)
    return (min(xs) + max(xs)) / 2, max(xs) - min(xs), (min(zs) + max(zs)) / 2


def _camera(shot: Shot, placed: dict[str, tuple[bpy.types.Object, int]], size: tuple[int, int]) -> bpy.types.Object:
    scene = bpy.context.scene
    scene.render.resolution_x, scene.render.resolution_y = size
    data = bpy.data.cameras.new(f'Cam_{shot.key}')
    cam = bpy.data.objects.new(f'Cam_{shot.key}', data)
    scene.collection.objects.link(cam)
    if shot.ortho:
        cx, span, cz = _lineup_bounds(placed)
        cast(Any, data).type = lamina._pick(data, 'type', 'ORTHO')
        data.ortho_scale = span * 1.08
        data.clip_end = 3000.0
        eye = Vector((cx, 600.0, cz - 60.0))
        target = Vector((cx, 0.0, cz + 6.0))
    else:
        data.lens = 18.0
        data.clip_end = 8000.0
        eye = EYE.copy()
        target = EYE + _direction(0.0, 16.0)
    cam.location = eye
    cam.rotation_euler = (target - eye).to_track_quat('-Z', 'Y').to_euler()
    scene.camera = cam
    bpy.context.view_layer.update()
    return cam


def sky_gradient(pal: Palette, width: int, height: int) -> np.ndarray:
    """Degradado vertical entre colores de la paleta (SKY_STOPS), float 0-1."""
    t = np.linspace(0.0, 1.0, height)
    stops = [s for s, _ in SKY_STOPS]
    cols = np.array([pal.by_name(n).rgb for _, n in SKY_STOPS], dtype=np.float32) / 255.0
    rows = np.stack([np.interp(t, stops, cols[:, k]) for k in range(3)], axis=1)
    return np.broadcast_to(rows[:, None, :], (height, width, 3))


def over_sky(rgba: np.ndarray, sky: np.ndarray) -> np.ndarray:
    alpha = rgba[..., 3:4]
    rgb = rgba[..., :3] * alpha + sky * (1.0 - alpha)
    return np.clip(np.rint(rgb * 255.0), 0, 255).astype(np.uint8)


def label_positions(cam: bpy.types.Object, placed: dict[str, tuple[bpy.types.Object, int]],
                    size: tuple[int, int]) -> list[tuple[str, int, int]]:
    out = []
    w, h = size
    for spec in K5:
        obj, tris = placed[spec.name]
        corners = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
        anchor = Vector((sum(c.x for c in corners) / 8, max(c.y for c in corners), min(c.z for c in corners)))
        p = world_to_camera_view(bpy.context.scene, cam, anchor)
        name = spec.name.removeprefix('SM_').upper()
        text = f'{name} {tris} - {obj.dimensions.x:.0f} M' if spec.is_cloud else f'{name} {tris}'
        out.append((text, int(p.x * w), int((1.0 - p.y) * h)))
    return out


def render_panel(shot: Shot, placed: dict[str, tuple[bpy.types.Object, int]], out_dir: Path, engine: str,
                 percent: int, pal: Palette) -> tuple[np.ndarray, list[tuple[str, int, int]]]:
    scene = bpy.context.scene
    lamina._set_engine(scene, engine)
    try:
        scene.view_settings.view_transform = 'Standard'
        scene.view_settings.look = 'None'
    except TypeError as exc:
        raise RuntimeError(f'transformación de vista no disponible: {exc}') from exc
    size = (WIDTH, shot.height)
    cam = _camera(shot, placed, size)
    labels = label_positions(cam, placed, size) if shot.ortho else []
    scene.render.film_transparent = True
    scene.render.resolution_percentage = percent
    scene.render.image_settings.file_format = 'PNG'
    scene.render.image_settings.color_mode = 'RGBA'
    path = out_dir / f'{shot.key}.png'
    scene.render.filepath = str(path)
    bpy.ops.render.render(write_still=True)
    rgba = lamina._fit(lamina.load_rgba(path), size)
    return over_sky(rgba, sky_gradient(pal, *size)), labels


def _tag(canvas: np.ndarray, text: str, cx: int, y: int, pal: Palette, bounds: tuple[int, int]) -> np.ndarray:
    scale = 2
    w, h = raster.text_width(text, scale) + 12, 5 * scale + 8
    top, bottom = bounds
    x = min(max(cx - w // 2, 4), WIDTH - w - 4)
    y = min(max(y, top + 4), bottom - h - 4)
    canvas = raster.fill_rect(canvas, x, y, w, h, pal.by_name('papel').rgb)
    return raster.draw_text(canvas, text, x + 6, y + 4, scale, pal.by_name('tinta').rgb)


def _caption(canvas: np.ndarray, text: str, x: int, y: int, pal: Palette) -> np.ndarray:
    scale = 2
    w = raster.text_width(text, scale) + 16
    canvas = raster.fill_rect(canvas, x, y, w, 5 * scale + 12, pal.by_name('papel').rgb)
    return raster.draw_text(canvas, text, x + 8, y + 6, scale, pal.by_name('tinta').rgb)


def compose(panels: list[tuple[np.ndarray, list[tuple[str, int, int]]]], pal: Palette) -> np.ndarray:
    canvas = raster.new_canvas(WIDTH, HEIGHT, pal.by_name('tinta').rgb)
    y0 = 0
    for image, labels in panels:
        canvas = raster.paste(canvas, image, 0, y0)
        for text, x, y in labels:
            canvas = _tag(canvas, text, x, y0 + y + 6, pal, (y0, y0 + image.shape[0]))
        y0 += image.shape[0] + 4
    canvas = _caption(canvas, 'K5 CIELO - VISTA DESDE LA ISLA: OJOS A 1,7 M, BASE DE NUBES A 120 M, '
                              'A 270-520 M', 12, 12, pal)
    return _caption(canvas, 'MISMA ESCALA, VISTA DESDE ABAJO - DISCOS DE D 1 M MOSTRADOS A D 14 M', 12, TOP_H + 16, pal)


def make_sheet(out: Path, export_dir: Path, sheet_path: Path = SHEET_PATH, engine: str = 'BLENDER_EEVEE',
               percent: int = 100) -> Path:
    missing = [s.asset for s in K5 if not (export_dir / f'{s.asset}.fbx').is_file()]
    if missing:
        raise FileNotFoundError(f'faltan FBX en {export_dir}: {", ".join(missing)}')
    pal = load_palette()
    placed = build_scene(pal, export_dir, sheet_path)
    with tempfile.TemporaryDirectory(prefix='lamina_k5_') as tmp:
        panels = [render_panel(shot, placed, Path(tmp), engine, percent, pal) for shot in SHOTS]
    return raster.write_png(out, compose(panels, pal))
