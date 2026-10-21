"""Lámina de revisión del kit K6 (docs/v3/laminas/k6-cesped.png, 1600 x 900, Workbench).

Importa los FBX exportados (revisa lo entregado) y compone dos planos:

- arriba, las 8 mallas en fila (cámara ortográfica, nombre y triángulos bajo cada una);
- abajo, una parcela de 4 x 4 m de hierba salvia sembrada al azar con todas ellas
  (densidad de prado, semilla fija) junto a `SM_GrassClump_01` y `SM_Shrub_01` de K1
  como referencia de escala y estilo.

Workbench con luz plana y color de textura (la hoja de paleta en Nearest, como M_Toon): los
colores son los exactos de la paleta, sin las bandas de luz del toon (la luz de estudio de
Workbench los oscurecía a la mitad). Sin culling de caras traseras, como el material de dos caras.
"""

from __future__ import annotations

import math
import random
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any, cast

import bpy

import numpy as np
from bpy_extras.object_utils import world_to_camera_view
from explored_style import lamina, meshkit, raster
from explored_style.meshkit import MeshBuilder, mesh_of
from explored_style.palette import REPO_ROOT, SHEET_PATH, Palette, load_palette
from mathutils import Vector

from kits.k6_cesped import EXPORT_DIR, K6

OUT_PATH = REPO_ROOT / 'docs' / 'v3' / 'laminas' / 'k6-cesped.png'
K1_DIR = REPO_ROOT / 'Art' / 'Export' / 'K1_Naturaleza'
REFERENCES = ('SM_GrassClump_01', 'SM_Shrub_01')
WIDTH, HEIGHT = 1600, 900
# Esta lámina revisa las matas sueltas y la vegetación de detalle; los parches
# SM_GrassPatch_* tienen la suya (k6_continuo).
TUFT_KIT = tuple(s for s in K6 if not s.is_patch)
ROW_H = 400
PARCEL_H = HEIGHT - ROW_H
ROW_Y = -25.0              # la fila va lejos de la parcela: cada plano ve solo lo suyo
ROW_GAP = 0.16
PARCEL = 4.0
PARCEL_CELL = 0.2          # rejilla con jitter: 400 huecos en 16 m²
PARCEL_SKIP = 0.12
PARCEL_SEED = 606
# Frecuencia relativa de cada malla en el prado.
MIX = {
    'SM_GrassTuft_A_01': 26, 'SM_GrassTuft_B_01': 26, 'SM_GrassTuft_C_01': 18, 'SM_GrassTall_01': 7,
    'SM_Clover_01': 9, 'SM_FlowerA_01': 7, 'SM_FlowerB_01': 7, 'SM_Fern_01': 3,
}
REFERENCE_AT = {'SM_GrassClump_01': (-2.75, 0.2, 15.0), 'SM_Shrub_01': (3.05, 0.9, 0.0)}


@dataclass(frozen=True)
class Camera:
    key: str
    location: tuple[float, float, float]
    target: tuple[float, float, float]
    size: tuple[int, int]
    ortho_scale: float = 0.0   # > 0: ortográfica
    lens: float = 35.0


def row_positions(widths: dict[str, float]) -> dict[str, float]:
    """x del centro de cada malla en la fila, con ROW_GAP entre cajas."""
    total = sum(widths.values()) + ROW_GAP * (len(widths) - 1)
    x = -total / 2.0
    out = {}
    for name, w in widths.items():
        out[name] = x + w / 2.0
        x += w + ROW_GAP
    return out


def sow(names: list[str], seed: int = PARCEL_SEED) -> list[tuple[str, float, float, float, float]]:
    """(malla, x, y, giro en grados, escala) de cada instancia de la parcela."""
    rng = random.Random(seed)
    weights = [MIX[n] for n in names]
    cells = round(PARCEL / PARCEL_CELL)
    out = []
    for i in range(cells):
        for j in range(cells):
            if rng.random() < PARCEL_SKIP:
                continue
            x = -PARCEL / 2 + (i + rng.uniform(0.1, 0.9)) * PARCEL_CELL
            y = -PARCEL / 2 + (j + rng.uniform(0.1, 0.9)) * PARCEL_CELL
            name = rng.choices(names, weights)[0]
            out.append((name, x, y, rng.uniform(0.0, 360.0), rng.uniform(0.85, 1.2)))
    return out


def _quad(color: int, half_x: float, half_y: float) -> MeshBuilder:
    b = MeshBuilder()
    ids = [b.vert((-half_x, -half_y, 0.0)), b.vert((half_x, -half_y, 0.0)),
           b.vert((half_x, half_y, 0.0)), b.vert((-half_x, half_y, 0.0))]
    b.face(ids, color)
    return b


def _place(obj: bpy.types.Object, x: float, y: float, turn: float = 0.0, scale: float = 1.0) -> None:
    obj.location = (x, y, 0.0)
    obj.rotation_euler = (0.0, 0.0, math.radians(turn))
    obj.scale = (scale, scale, scale)


def build_scene(pal: Palette, export_dir: Path, k1_dir: Path, sheet_path: Path) -> dict[str, Any]:
    """Monta la escena; devuelve triángulos por malla, posiciones de la fila y datos de la parcela."""
    meshkit.reset_scene()
    material = lamina.toon_preview_material(bpy.data.images.load(str(sheet_path)))
    sources: dict[str, bpy.types.Object] = {}
    for spec in TUFT_KIT:
        sources[spec.asset] = lamina._import_pilot(export_dir / f'{spec.asset}.fbx')
    tris = {name: meshkit.triangle_count(obj) for name, obj in sources.items()}
    xs = row_positions({name: max(obj.dimensions.x, obj.dimensions.y) for name, obj in sources.items()})
    for name, obj in sources.items():
        _place(obj, xs[name], ROW_Y)
    scene = bpy.context.scene
    planted = sow(list(sources))
    for name, x, y, turn, scale in planted:
        instance = sources[name].copy()        # instancia: comparte la malla
        scene.collection.objects.link(instance)
        _place(instance, x, y, turn, scale)
    for name in REFERENCES:
        ref = lamina._import_pilot(k1_dir / f'{name}.fbx')
        _place(ref, *REFERENCE_AT[name][:2], REFERENCE_AT[name][2])
        tris[name] = meshkit.triangle_count(ref)
    meshkit.build_object('Suelo', lamina._ground(pal))
    meshkit.build_object('Parcela', _quad(pal.index_of('hierba_salvia'), PARCEL / 2, PARCEL / 2), base_pivot=False)
    bpy.data.objects['Parcela'].location.z = 0.002
    meshkit.build_object('FilaSuelo', _quad(pal.index_of('hierba_salvia'), 2.8, 0.45),
                         base_pivot=False).location = (0.0, ROW_Y, 0.002)
    for obj in (o for o in bpy.data.objects if o.type == 'MESH'):
        mesh_of(obj).materials.clear()
        mesh_of(obj).materials.append(material)
    parcel_tris = sum(tris[name] for name, *_ in planted)
    return {'tris': tris, 'row_x': xs, 'instances': len(planted), 'parcel_tris': parcel_tris}


def _workbench(scene: bpy.types.Scene) -> None:
    try:
        render: Any = scene.render
        render.engine = 'BLENDER_WORKBENCH'
    except TypeError as exc:
        raise RuntimeError(f'Workbench no está disponible: {exc}') from exc
    display = cast(Any, scene.display)
    shading = display.shading
    shading.light = lamina._pick(shading, 'light', 'FLAT')
    shading.color_type = lamina._pick(shading, 'color_type', 'TEXTURE')
    shading.show_backface_culling = False
    try:
        scene.view_settings.view_transform = 'Standard'
    except TypeError as exc:
        raise RuntimeError(f'transformación de vista no disponible: {exc}') from exc
    scene.render.film_transparent = True
    scene.render.image_settings.file_format = 'PNG'
    scene.render.image_settings.color_mode = 'RGBA'


def render(cam: Camera, out_dir: Path, percent: int) -> np.ndarray:
    scene = bpy.context.scene
    _workbench(scene)
    scene.render.resolution_x, scene.render.resolution_y = cam.size
    scene.render.resolution_percentage = percent
    data = bpy.data.cameras.new(f'Cam_{cam.key}')
    if cam.ortho_scale > 0.0:
        data.type = lamina._pick(data, 'type', 'ORTHO')
        data.ortho_scale = cam.ortho_scale
    else:
        data.lens = cam.lens
    obj = bpy.data.objects.new(f'Cam_{cam.key}', data)
    scene.collection.objects.link(obj)
    obj.location = cam.location
    obj.rotation_euler = (Vector(cam.target) - Vector(cam.location)).to_track_quat('-Z', 'Y').to_euler()
    scene.camera = obj
    path = out_dir / f'{cam.key}.png'
    scene.render.filepath = str(path)
    bpy.ops.render.render(write_still=True)
    return lamina.load_rgba(path)


def row_camera(row_x: dict[str, float]) -> Camera:
    span = max(row_x.values()) - min(row_x.values()) + 1.2
    return Camera('fila', (0.0, ROW_Y - 6.0, 2.2), (0.0, ROW_Y, 0.28), (WIDTH, ROW_H), ortho_scale=span)


PARCEL_CAMERA = Camera('parcela', (0.3, -7.6, 3.4), (0.0, 0.3, 0.1), (WIDTH, PARCEL_H), lens=34.0)


def label_anchors(row_x: dict[str, float]) -> dict[str, tuple[int, int]]:
    """Píxel (x, y desde arriba) bajo la base de cada malla en el plano de la fila."""
    scene = bpy.context.scene
    cam = scene.camera
    if cam is None:
        raise RuntimeError('la escena no tiene cámara')
    out = {}
    for k, (name, x) in enumerate(row_x.items()):
        co = cast(Any, world_to_camera_view(scene, cam, Vector((x, ROW_Y - 0.3, 0.0))))
        out[name] = (int(co.x * WIDTH), int((1.0 - co.y) * ROW_H) + (8 if k % 2 == 0 else 34))
    return out


def _label(canvas: np.ndarray, text: str, cx: int, y: int, pal: Palette, scale: int = 2) -> np.ndarray:
    w = raster.text_width(text, scale) + 12
    x = min(max(cx - w // 2, 4), WIDTH - w - 4)
    y = min(max(y, 4), HEIGHT - 5 * scale - 14)
    canvas = raster.fill_rect(canvas, x, y, w, 5 * scale + 10, pal.by_name('papel').rgb)
    return raster.draw_text(canvas, text, x + 6, y + 5, scale, pal.by_name('tinta').rgb)


def compose(row: np.ndarray, parcel: np.ndarray, info: dict[str, Any], anchors: dict[str, tuple[int, int]],
            pal: Palette) -> np.ndarray:
    canvas = raster.new_canvas(WIDTH, HEIGHT, pal.by_name('tinta').rgb)
    canvas = raster.paste(canvas, lamina._fit(row, (WIDTH, ROW_H - 2)), 0, 0)
    canvas = raster.paste(canvas, lamina._fit(parcel, (WIDTH, PARCEL_H - 2)), 0, ROW_H + 2)
    tris = info['tris']
    for name, (cx, y) in anchors.items():
        canvas = _label(canvas, f'{name} {tris[name]}', cx, y, pal)
    kit_tris = sum(tris[s.asset] for s in TUFT_KIT)
    title = f'K6 CESPED - {len(TUFT_KIT)} MALLAS - {kit_tris} TRIS'
    canvas = _label(canvas, title, raster.text_width(title, 3) // 2 + 18, 12, pal, scale=3)
    sub = (f'PARCELA 4 X 4 M: {info["instances"]} INSTANCIAS, {info["parcel_tris"]} TRIS - '
           f'REFERENCIAS K1: SM_GRASSCLUMP_01 {tris["SM_GrassClump_01"]}, SM_SHRUB_01 {tris["SM_Shrub_01"]}')
    return _label(canvas, sub, raster.text_width(sub, 2) // 2 + 16, ROW_H + 14, pal)


def make_sheet(out: Path, export_dir: Path = EXPORT_DIR, k1_dir: Path = K1_DIR, sheet_path: Path = SHEET_PATH,
               percent: int = 100) -> Path:
    pal = load_palette()
    needed = [export_dir / f'{s.asset}.fbx' for s in TUFT_KIT] + [k1_dir / f'{n}.fbx' for n in REFERENCES]
    missing = [p.name for p in needed if not p.is_file()]
    if missing:
        raise FileNotFoundError(f'faltan FBX: {", ".join(missing)}')
    info = build_scene(pal, export_dir, k1_dir, sheet_path)
    sky = pal.by_name('cielo_dia').rgb
    with tempfile.TemporaryDirectory(prefix='lamina_k6_') as tmp:
        row = lamina.over_background(render(row_camera(info['row_x']), Path(tmp), percent), sky)
        anchors = label_anchors(info['row_x'])
        parcel = lamina.over_background(render(PARCEL_CAMERA, Path(tmp), percent), sky)
    return raster.write_png(out, compose(row, parcel, info, anchors, pal))
