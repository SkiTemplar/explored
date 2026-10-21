"""Lámina de revisión del kit K2 (docs/v3/laminas/k2.png, 1600 × 900).

Importa los FBX entregados y los monta en una sola escena de arena con un
único sol y cápsulas de 1,75 m en gris claro: el faro (torre + linterna) y la
casa, el carguero varado con su puente, las piezas de supervivencia, una
segunda linterna en el suelo para ver las ranuras y las manos delante de una
cámara en primera persona. Cinco planos (general, linterna, piezas, carguero
y manos) rotulados con el nombre y los triángulos de cada malla. Sombreado
`M_Toon` de la lámina de pilotos (hoja de paleta × 3 bandas).
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
from mathutils import Matrix, Vector

from explored_style import lamina, meshkit, raster
from explored_style.kits import k2_faro
from explored_style.kits.k2 import SPECS, spec_by_name
from explored_style.meshkit import mesh_of
from explored_style.palette import REPO_ROOT, SHEET_PATH, Palette, load_palette

OUT_PATH = REPO_ROOT / 'docs' / 'v3' / 'laminas' / 'k2.png'
WIDTH, HEIGHT = 1600, 900
ROW1_H, GAP = 520, 4
ROW2_H = HEIGHT - ROW1_H - GAP
CAPSULE_COLOR = 'roca_clara'

# Colocación en la escena: (x, y, giro en grados)
PLACES: dict[str, tuple[float, float, float]] = {
    'SM_FaroTorre': (14.0, 30.0, 0.0),
    'SM_FaroCasa': (5.0, 27.0, -12.0),
    'SM_CargueroCasco': (-14.0, 62.0, 8.0),
    'SM_RefugioPalma': (-2.8, 7.0, 10.0),
    'SM_Balsa': (-2.7, 3.0, 15.0),
    'SM_CajaSuministros': (1.5, 4.4, -20.0),
    'SM_BarrilLluvia': (-0.25, 6.1, 0.0),
    'SM_Hoguera': (0.5, 2.4, 0.0),
    'SM_FaroLinterna': (6.4, 6.5, 0.0),     # copia en el suelo para ver las ranuras
}
CAPSULES = ((0.6, 5.7), (5.0, 5.6), (9.0, 24.6))
HANDS_AT = (-40.0, -40.0, 1.45)
HULL_SINK = 0.8          # el casco se entierra en la arena en la lámina (varado)


@dataclass(frozen=True)
class Panel:
    key: str
    title: str
    x: int
    y: int
    w: int
    h: int
    camera: tuple[float, float, float]
    target: tuple[float, float, float]
    lens: float


PANELS = (
    Panel('general', 'K2 ESTRUCTURAS - ESCENA', 0, 0, 1040, ROW1_H, (2.5, -8.5, 3.4), (2.0, 20.0, 5.6), 24.0),
    Panel('linterna', 'SM_FAROLINTERNA - RANURAS', 1044, 0, 556, ROW1_H, (6.75, 2.0, 2.55), (6.4, 6.5, 1.8), 30.0),
    Panel('piezas', 'PIEZAS DE SUPERVIVENCIA', 0, ROW1_H + GAP, 532, ROW2_H, (0.0, -3.0, 2.4), (-0.7, 4.9, 0.7), 28.0),
    Panel('carguero', 'CARGUERO VARADO', 536, ROW1_H + GAP, 532, ROW2_H, (-40.0, 36.0, 9.0), (-17.0, 62.0, 5.5), 28.0),
    Panel('manos', 'SM_MANOIRIA - PRIMERA PERSONA', 1072, ROW1_H + GAP, 528, ROW2_H,
          (HANDS_AT[0] - 0.3, HANDS_AT[1], HANDS_AT[2] + 0.28), (HANDS_AT[0] + 0.45, HANDS_AT[1], HANDS_AT[2] + 0.03),
          32.0),
)
# Rótulos por plano: (malla o texto, punto del mundo); None = encima de la malla.
LABELS: dict[str, tuple[tuple[str, tuple[float, float, float] | None], ...]] = {
    'general': (('SM_FaroTorre', None), ('SM_FaroLinterna@torre', None), ('SM_FaroCasa', None),
                ('SM_CargueroCasco', None), ('SM_CargueroPuente', None), ('SM_RefugioPalma', None)),
    'linterna': (('ARO DE LA LENTE', None), ('TARRO', None), ('TARRO ', None), ('PORTAMECHA', None)),
    'piezas': (('SM_Balsa', None), ('SM_CajaSuministros', None), ('SM_BarrilLluvia', None), ('SM_Hoguera', None),
               ('SM_RefugioPalma', None)),
    'carguero': (('SM_CargueroPuente', None), ('SM_CargueroCasco', None)),
    'manos': (('SM_ManoIria', (HANDS_AT[0] + 0.7, HANDS_AT[1], HANDS_AT[2] + 0.32)),),
}


# Desplazamiento del rótulo respecto a la ranura (para que no se pisen).
_SLOT_OFFSETS = {'aro_lente': (0.0, 0.0, 0.5), 'portamecha': (0.0, 0.0, -0.3), 'tarro_izq': (-0.05, 0.0, 0.1),
                 'tarro_der': (0.05, 0.0, 0.1)}


def _slot_world(key: str) -> Vector:
    x, y, _ = PLACES['SM_FaroLinterna']
    sx, sy, sz = k2_faro.LANTERN_SLOTS[key]
    return Vector((x + sx, y + sy, sz)) + Vector(_SLOT_OFFSETS[key])


_SLOT_LABELS = {'ARO DE LA LENTE': 'aro_lente', 'TARRO': 'tarro_izq', 'TARRO ': 'tarro_der', 'PORTAMECHA': 'portamecha'}


def _place(obj: bpy.types.Object, x: float, y: float, turn: float, z: float = 0.0) -> None:
    obj.location = (x, y, z)
    obj.rotation_euler = (0.0, 0.0, math.radians(turn))


def _import_all(export_dir: Path) -> dict[str, bpy.types.Object]:
    objs = {spec.name: lamina._import_pilot(export_dir / spec.fbx) for spec in SPECS}
    objs['SM_FaroLinterna@torre'] = lamina._import_pilot(export_dir / spec_by_name('SM_FaroLinterna').fbx)
    return objs


def _assemble(objs: dict[str, bpy.types.Object], mount: dict[str, list[float]]) -> None:
    for name, (x, y, turn) in PLACES.items():
        _place(objs[name], x, y, turn)
    tx, ty, _ = PLACES['SM_FaroTorre']
    _place(objs['SM_FaroLinterna@torre'], tx, ty, 0.0, k2_faro.DECK_Z)
    hx, hy, hturn = PLACES['SM_CargueroCasco']
    objs['SM_CargueroCasco'].location.z = -HULL_SINK
    hull_world = Matrix.Translation((hx, hy, -HULL_SINK)) @ Matrix.Rotation(math.radians(hturn), 4, 'Z')
    roll, pitch, _ = (math.radians(a) for a in mount['rotacion_grados'])
    tilt = Matrix.Rotation(pitch, 4, 'Y') @ Matrix.Rotation(roll, 4, 'X')
    objs['SM_CargueroPuente'].matrix_world = hull_world @ Matrix.Translation(mount['ubicacion_m']) @ tilt
    objs['SM_ManoIria'].location = HANDS_AT


def build_scene(pal: Palette, export_dir: Path, mount: dict[str, list[float]],
                sheet_path: Path = SHEET_PATH) -> dict[str, bpy.types.Object]:
    meshkit.reset_scene()
    material = lamina.toon_preview_material(bpy.data.images.load(str(sheet_path)))
    objs = _import_all(export_dir)
    _assemble(objs, mount)
    lamina._prop('Suelo', lamina._ground(pal), 0.0, 0.0).scale = (4.0, 4.0, 1.0)
    for k, (x, y) in enumerate(CAPSULES):
        lamina._prop(f'Capsula175_{k}', lamina._capsule(pal, CAPSULE_COLOR), x, y)
    for obj in (o for o in bpy.data.objects if o.type == 'MESH'):
        mesh_of(obj).materials.clear()
        mesh_of(obj).materials.append(material)
    lamina._pick(bpy.types.Light, 'type', 'SUN')
    sun_data = cast(bpy.types.SunLight, bpy.data.lights.new('Sol', 'SUN'))
    sun_data.energy = lamina.SUN_STRENGTH
    sun = bpy.data.objects.new('Sol', sun_data)
    sun.rotation_euler = (math.radians(50.0), 0.0, math.radians(-32.0))
    bpy.context.scene.collection.objects.link(sun)
    world = bpy.data.worlds.new('Negro')
    world.color = (0.0, 0.0, 0.0)
    bpy.context.scene.world = world
    return objs


def _label_point(name: str, objs: dict[str, bpy.types.Object]) -> Vector:
    if name in _SLOT_LABELS:
        return _slot_world(_SLOT_LABELS[name])
    obj = objs[name]
    corners = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
    center = sum(corners, Vector((0.0, 0.0, 0.0))) / 8
    if name == 'SM_CargueroCasco':  # el casco se rotula en su costado, no sobre los palos
        return center
    return Vector((center.x, center.y, max(c.z for c in corners)))


def _label_text(name: str, objs: dict[str, bpy.types.Object]) -> str:
    if name not in objs:
        return name
    base = name.split('@')[0]
    return f'{base.upper()} {meshkit.triangle_count(objs[name])}'


def project_labels(panel: Panel, objs: dict[str, bpy.types.Object]) -> list[tuple[str, int, int]]:
    """Rótulos del plano en píxeles del panel (la cámara activa es la del plano)."""
    scene, cam = bpy.context.scene, bpy.context.scene.camera
    out = []
    for name, point in LABELS[panel.key]:
        world = Vector(point) if point is not None else _label_point(name, objs)
        u, v, depth = world_to_camera_view(scene, cam, world)
        if depth > 0.0 and 0.0 <= u <= 1.0 and 0.0 <= v <= 1.0:
            out.append((_label_text(name, objs), int(u * panel.w), int((1.0 - v) * panel.h)))
    return out


Rect = tuple[int, int, int, int]


def _overlaps(a: Rect, b: Rect) -> bool:
    return a[0] < b[0] + b[2] and b[0] < a[0] + a[2] and a[1] < b[1] + b[3] and b[1] < a[1] + a[3]


def place_tag(text: str, x: int, y: int, size: tuple[int, int], taken: list[Rect]) -> Rect:
    """Rectángulo del rótulo centrado en x y encima de y; sube hasta no pisar otro."""
    scale = 2
    w, h = raster.text_width(text, scale) + 10, 5 * scale + 8
    left = int(np.clip(x - w // 2, 0, size[0] - w))
    top = int(np.clip(y - h - 6, 34, size[1] - h))
    rect = (left, top, w, h)
    while any(_overlaps(rect, other) for other in taken) and rect[1] > 34:
        rect = (left, rect[1] - h - 2, w, h)
    while any(_overlaps(rect, other) for other in taken) and rect[1] + h < size[1]:
        rect = (left, rect[1] + h + 2, w, h)
    taken.append(rect)
    return rect


def _tag(canvas: np.ndarray, text: str, rect: Rect, pal: Palette) -> np.ndarray:
    left, top, w, h = rect
    canvas = raster.fill_rect(canvas, left, top, w, h, pal.by_name('tinta').rgb)
    return raster.draw_text(canvas, text, left + 5, top + 4, 2, pal.by_name('papel').rgb)


def compose(panels: dict[str, np.ndarray], labels: dict[str, list[tuple[str, int, int]]], pal: Palette,
            tris_total: int) -> np.ndarray:
    canvas = raster.new_canvas(WIDTH, HEIGHT, pal.by_name('tinta').rgb)
    for panel in PANELS:
        image = lamina._fit(panels[panel.key], (panel.w, panel.h))
        taken: list[Rect] = []
        for text, x, y in labels.get(panel.key, []):
            image = _tag(image, text, place_tag(text, x, y, (panel.w, panel.h), taken), pal)
        title = panel.title if panel.key != 'general' else f'{panel.title} - {tris_total} TRIS - CAPSULA 1,75 M'
        image = lamina._caption(image, title, 10, 10, pal)
        canvas = raster.paste(canvas, image, panel.x, panel.y)
    return canvas


def make_sheet(out: Path, export_dir: Path, engine: str = 'BLENDER_EEVEE', percent: int = 100,
               sheet_path: Path = SHEET_PATH) -> Path:
    import json

    pal = load_palette()
    manifest = json.loads((export_dir / 'manifest.json').read_text(encoding='utf-8'))
    mount = manifest['ensamblaje']['SM_CargueroPuente_01']
    objs = build_scene(pal, export_dir, mount, sheet_path)
    sky = pal.by_name('cielo_dia').rgb
    panels, labels = {}, {}
    with tempfile.TemporaryDirectory(prefix='lamina_k2_') as tmp:
        for panel in PANELS:
            shot = lamina.Shot(panel.key, panel.camera, panel.target, panel.lens, (panel.w, panel.h))
            panels[panel.key] = lamina.over_background(lamina.render_shot(shot, Path(tmp), engine, percent), sky)
            labels[panel.key] = project_labels(panel, objs)
    total = sum(m['tris'] for m in manifest['mallas'])
    return raster.write_png(out, compose(panels, labels, pal, total))
