"""Lámina de revisión del kit K3 (docs/v3/laminas/k3.png, 1600 x 900).

Importa los FBX exportados (revisa lo entregado), los reparte sobre arena de
la paleta con una cápsula de 1,75 m en gris claro neutro y un único sol, y
los sombrea como `M_Toon` (misma réplica que la lámina de los pilotos).
Arriba, vista general con las piezas grandes; abajo, primer plano de los
objetos de mano. Cada pieza lleva su nombre y sus triángulos.
"""

from __future__ import annotations

import math
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import cast

import bpy

import numpy as np
from bpy_extras.object_utils import world_to_camera_view
from mathutils import Vector

from explored_style import lamina, meshkit, raster
from explored_style.kits.k3 import K3
from explored_style.meshkit import mesh_of
from explored_style.palette import REPO_ROOT, SHEET_PATH, Palette, load_palette

OUT_PATH = REPO_ROOT / 'docs' / 'v3' / 'laminas' / 'k3.png'
WIDTH, HEIGHT = 1600, 900
TOP_H = 500
BOTTOM_H = HEIGHT - TOP_H - 4
CAPSULE_AT = (-1.9, 1.55)
CAPSULE_HEIGHT = 1.75

# Posición (x, y, giro en grados, altura) de cada pieza en la escena.
LAYOUT: dict[str, tuple[float, float, float, float]] = {
    'SM_Tronco': (0.45, 1.5, 0.0, 0.0),
    'SM_Remo': (-0.6, 0.95, 0.0, 0.0),
    'SM_CanaPescar': (0.3, 0.5, 0.0, 0.0),
    'SM_CanaBambu': (-0.35, 0.05, 0.0, 0.0),
    'SM_Lente': (1.65, 0.3, -12.0, 0.0),
    'SM_FaunaGaviota': (2.45, 1.25, 30.0, 0.85),
    'SM_Cuchillo': (-1.05, -0.85, 10.0, 0.0),
    'SM_Cantimplora': (-0.7, -0.85, 0.0, 0.0),
    'SM_Barometro': (-0.35, -0.85, 0.0, 0.0),
    'SM_Coco': (0.0, -0.85, 55.0, 0.0),
    'SM_CocoVacio': (0.35, -0.85, 0.0, 0.0),
    'SM_TarroAceite': (0.7, -0.85, 0.0, 0.0),
    'SM_FaunaPez': (1.05, -0.85, 0.0, 0.0),
    'SM_Lena': (-1.05, -1.4, 15.0, 0.0),
    'SM_Fibra': (-0.62, -1.4, -25.0, 0.0),
    'SM_Cuerda': (-0.2, -1.4, 0.0, 0.0),
    'SM_Mecha': (0.2, -1.4, -20.0, 0.0),
    'SM_Pez': (0.6, -1.4, 15.0, 0.0),
    'SM_PezAsado': (1.05, -1.4, -20.0, 0.0),
}
BIG = ('SM_Tronco', 'SM_Remo', 'SM_CanaPescar', 'SM_CanaBambu', 'SM_Lente', 'SM_FaunaGaviota')


@dataclass(frozen=True)
class Shot:
    key: str
    location: tuple[float, float, float]
    target: tuple[float, float, float]
    lens: float
    labels: tuple[str, ...]
    label_below: bool
    height: int


SHOTS = (
    Shot('general', (0.1, -5.2, 2.7), (0.1, 0.85, 0.85), 28.0, (*BIG, 'Capsula'), False, TOP_H),
    Shot('objetos', (0.0, -3.85, 1.8), (0.0, -1.17, 0.07), 36.0,
         tuple(n for n in LAYOUT if n not in BIG), True, BOTTOM_H),
)


def _import(path: Path) -> bpy.types.Object:
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(path))
    new = [o for o in bpy.data.objects if o not in before and o.type == 'MESH']
    if len(new) != 1:
        raise RuntimeError(f'{path.name}: se esperaba 1 malla y hay {len(new)}')
    return new[0]


def _ground(pal: Palette) -> meshkit.MeshBuilder:
    b = meshkit.MeshBuilder()
    s = 40.0
    b.face([b.vert((-s, -s, 0.0)), b.vert((s, -s, 0.0)), b.vert((s, s, 0.0)), b.vert((-s, s, 0.0))],
           pal.index_of('arena_clara'))
    return b


def _capsule(pal: Palette) -> meshkit.MeshBuilder:
    """Cápsula de referencia de 1,75 m en gris claro neutro (roca_clara)."""
    b = meshkit.MeshBuilder()
    r, h = 0.25, CAPSULE_HEIGHT
    profile = [(r * math.sin(math.radians(a)), r - r * math.cos(math.radians(a))) for a in (30, 60, 90)]
    profile += [(r * math.sin(math.radians(a)), h - r + r * math.cos(math.radians(a))) for a in (90, 60, 30)]
    color = pal.index_of('roca_clara')
    meshkit.lathe(b, profile, 12, [color] * (len(profile) - 1), cap_bottom=color, cap_top=color, tips=(0.0, h))
    return b


def _add(name: str, builder: meshkit.MeshBuilder, x: float, y: float) -> bpy.types.Object:
    obj = meshkit.build_object(name, builder)
    obj.location = (x, y, 0.0)
    return obj


def _sun() -> None:
    lamina._pick(bpy.types.Light, 'type', 'SUN')
    data = cast(bpy.types.SunLight, bpy.data.lights.new('Sol', 'SUN'))
    data.energy = lamina.SUN_STRENGTH
    data.angle = math.radians(0.6)
    sun = bpy.data.objects.new('Sol', data)
    sun.rotation_euler = (math.radians(48.0), 0.0, math.radians(-38.0))
    bpy.context.scene.collection.objects.link(sun)
    world = bpy.data.worlds.new('Negro')
    world.color = (0.0, 0.0, 0.0)
    bpy.context.scene.world = world


def build_scene(pal: Palette, export_dir: Path, sheet_path: Path) -> dict[str, tuple[bpy.types.Object, int]]:
    """Monta la escena; devuelve {nombre: (objeto, tris)} de las piezas y la cápsula."""
    meshkit.reset_scene()
    material = lamina.toon_preview_material(bpy.data.images.load(str(sheet_path)))
    placed: dict[str, tuple[bpy.types.Object, int]] = {}
    for spec in K3:
        obj = _import(export_dir / f'{spec.asset}.fbx')
        x, y, turn, z = LAYOUT[spec.name]
        obj.location = (x, y, z)
        obj.rotation_euler = (0.0, 0.0, math.radians(turn))
        placed[spec.name] = (obj, meshkit.triangle_count(obj))
    _add('Suelo', _ground(pal), 0.0, 0.0)
    capsule = _add('Capsula', _capsule(pal), *CAPSULE_AT)
    placed['Capsula'] = (capsule, meshkit.triangle_count(capsule))
    for obj in (o for o in bpy.data.objects if o.type == 'MESH'):
        mesh_of(obj).materials.clear()
        mesh_of(obj).materials.append(material)
    _sun()
    bpy.context.view_layer.update()
    return placed


def _label_text(name: str, tris: int) -> str:
    if name == 'Capsula':
        return 'CAPSULA 1,75 M'
    return f'{name.removeprefix("SM_").upper()} {tris}'


def _anchor(obj: bpy.types.Object, below: bool) -> Vector:
    """Punto del mundo donde se cuelga el rótulo: centro de la cara superior
    de la caja (encima) o borde delantero a ras de suelo (debajo)."""
    corners = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
    xs, ys, zs = ([c[i] for c in corners] for i in range(3))
    cx, cy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
    if below:
        return Vector((cx, min(ys), min(zs)))
    if max(xs) - min(xs) > 1.0:  # piezas largas: rótulo en el extremo izquierdo
        return Vector((min(xs) + 0.25, cy, max(zs)))
    return Vector((cx, cy, max(zs)))


def _camera(shot: Shot, size: tuple[int, int]) -> bpy.types.Object:
    scene = bpy.context.scene
    scene.render.resolution_x, scene.render.resolution_y = size
    data = bpy.data.cameras.new(f'Cam_{shot.key}')
    data.lens = shot.lens
    cam = bpy.data.objects.new(f'Cam_{shot.key}', data)
    scene.collection.objects.link(cam)
    cam.location = shot.location
    cam.rotation_euler = (Vector(shot.target) - Vector(shot.location)).to_track_quat('-Z', 'Y').to_euler()
    scene.camera = cam
    bpy.context.view_layer.update()
    return cam


def label_positions(shot: Shot, cam: bpy.types.Object, placed: dict[str, tuple[bpy.types.Object, int]],
                    size: tuple[int, int]) -> list[tuple[str, int, int]]:
    """(texto, x, y) en píxeles del panel para cada pieza rotulada del plano."""
    out = []
    w, h = size
    for name in shot.labels:
        obj, tris = placed[name]
        p = world_to_camera_view(bpy.context.scene, cam, _anchor(obj, shot.label_below))
        out.append((_label_text(name, tris), int(p.x * w), int((1.0 - p.y) * h)))
    return out


def render_panel(shot: Shot, placed: dict[str, tuple[bpy.types.Object, int]], out_dir: Path, engine: str,
                 percent: int, sky: raster.Rgb) -> tuple[np.ndarray, list[tuple[str, int, int]]]:
    scene = bpy.context.scene
    lamina._set_engine(scene, engine)
    try:
        scene.view_settings.view_transform = 'Standard'
        scene.view_settings.look = 'None'
    except TypeError as exc:
        raise RuntimeError(f'transformación de vista no disponible: {exc}') from exc
    size = (WIDTH, shot.height)
    cam = _camera(shot, size)
    labels = label_positions(shot, cam, placed, size)
    scene.render.film_transparent = True
    scene.render.resolution_percentage = percent
    scene.render.image_settings.file_format = 'PNG'
    scene.render.image_settings.color_mode = 'RGBA'
    path = out_dir / f'{shot.key}.png'
    scene.render.filepath = str(path)
    bpy.ops.render.render(write_still=True)
    image = lamina.over_background(lamina.load_rgba(path), sky)
    return lamina._fit(image, size), labels


def _tag(canvas: np.ndarray, text: str, cx: int, y: int, pal: Palette, bounds: tuple[int, int]) -> np.ndarray:
    scale = 2
    w, h = raster.text_width(text, scale) + 12, 5 * scale + 8
    top, bottom = bounds
    x = min(max(cx - w // 2, 4), WIDTH - w - 4)
    y = min(max(y, top + 4), bottom - h - 4)
    canvas = raster.fill_rect(canvas, x, y, w, h, pal.by_name('papel').rgb)
    return raster.draw_text(canvas, text, x + 6, y + 4, scale, pal.by_name('tinta').rgb)


def compose(panels: list[tuple[np.ndarray, list[tuple[str, int, int]], bool]], title: str,
            pal: Palette) -> np.ndarray:
    """Paneles apilados con sus rótulos y el título arriba a la derecha."""
    canvas = raster.new_canvas(WIDTH, HEIGHT, pal.by_name('tinta').rgb)
    y0 = 0
    for image, labels, below in panels:
        canvas = raster.paste(canvas, image, 0, y0)
        for text, x, y in labels:
            ty = y0 + y + 8 if below else y0 + y - 26
            canvas = _tag(canvas, text, x, ty, pal, (y0, y0 + image.shape[0]))
        y0 += image.shape[0] + 4
    scale = 2
    w = raster.text_width(title, scale) + 16
    canvas = raster.fill_rect(canvas, WIDTH - w - 12, 12, w, 5 * scale + 12, pal.by_name('papel').rgb)
    return raster.draw_text(canvas, title, WIDTH - w - 4, 18, scale, pal.by_name('tinta').rgb)


def make_sheet(out: Path, export_dir: Path, sheet_path: Path = SHEET_PATH, engine: str = 'BLENDER_EEVEE',
               percent: int = 100) -> Path:
    missing = [s.asset for s in K3 if not (export_dir / f'{s.asset}.fbx').is_file()]
    if missing:
        raise FileNotFoundError(f'faltan FBX en {export_dir}: {", ".join(missing)}')
    pal = load_palette()
    placed = build_scene(pal, export_dir, sheet_path)
    sky = pal.by_name('cielo_dia').rgb
    panels = []
    with tempfile.TemporaryDirectory(prefix='lamina_k3_') as tmp:
        for shot in SHOTS:
            image, labels = render_panel(shot, placed, Path(tmp), engine, percent, sky)
            panels.append((image, labels, shot.label_below))
    title = 'K3 OBJETOS - 15 OBJETOS, 2 VARIANTES Y 2 FAUNA - SOL UNICO - CAPSULA 1,75 M'
    return raster.write_png(out, compose(panels, title, pal))
