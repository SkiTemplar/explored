"""Lámina de revisión del kit K1 (docs/v3/laminas/k1.png, 1600 x 900).

Importa los FBX exportados (revisa lo entregado), los coloca en una escena de
arena con dos acantilados encadenados (comprueba la junta modular), un único
sol y una cápsula de 1,75 m en color neutro claro, y los sombrea como
`M_Toon` (reutiliza el material y el render de `explored_style.lamina`). Cada
pieza lleva su nombre y triángulos rotulados bajo la base.
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
from explored_style.kits.k1 import EXPORT_DIR, KIT
from explored_style.meshkit import mesh_of
from explored_style.palette import REPO_ROOT, SHEET_PATH, Palette, load_palette

OUT_PATH = REPO_ROOT / 'docs' / 'v3' / 'laminas' / 'k1.png'
WIDTH, HEIGHT = 1600, 900
CAPSULE_HEIGHT = 1.75
CAPSULE_COLOR = 'acero'


@dataclass(frozen=True)
class Placement:
    x: float
    y: float
    turn_deg: float
    label_dy: int = 14     # desplazamiento del rótulo bajo el ancla (px)
    anchor_z: float = 0.0  # altura del ancla del rótulo (m)


# Posición de cada pieza (m) y giro; el acantilado se repite en COPIES.
LAYOUT = {
    'SM_CliffRock_01': Placement(-4.0, 9.0, 0.0, label_dy=-40, anchor_z=6.0),
    'SM_PalmA_01': Placement(-5.6, 3.2, 20.0),
    'SM_PalmB_01': Placement(4.3, 2.4, 160.0),
    'SM_Bamboo_01': Placement(-1.9, 4.6, 0.0),
    'SM_RockBig_01': Placement(1.4, 4.2, -20.0),
    'SM_Shrub_01': Placement(-2.6, -1.6, 0.0),
    'SM_GrassClump_01': Placement(-0.9, -3.3, 0.0),
    'SM_RockSmall_01': Placement(1.1, -3.1, 30.0),
}
COPIES = {'SM_CliffRock_01': (4.0, 9.0)}
CAPSULE_AT = (-4.4, -2.0)
SHOT = lamina.Shot('k1', (0.0, -15.5, 4.2), (0.0, 3.5, 2.0), 30.0, (WIDTH, HEIGHT))


def _capsule(pal: Palette) -> meshkit.MeshBuilder:
    b = meshkit.MeshBuilder()
    r, h = 0.25, CAPSULE_HEIGHT
    profile = [(r * math.sin(math.radians(a)), r - r * math.cos(math.radians(a))) for a in (30, 60, 90)]
    profile += [(r * math.sin(math.radians(a)), h - r + r * math.cos(math.radians(a))) for a in (90, 60, 30)]
    color = pal.index_of(CAPSULE_COLOR)
    meshkit.lathe(b, profile, 12, [color] * (len(profile) - 1), cap_bottom=color, cap_top=color, tips=(0.0, h))
    return b


def _place(obj: bpy.types.Object, x: float, y: float, turn_deg: float) -> None:
    obj.location = (x, y, 0.0)
    obj.rotation_euler = (0.0, 0.0, math.radians(turn_deg))


def build_scene(pal: Palette, export_dir: Path, sheet_path: Path) -> dict[str, int]:
    """Monta la escena y devuelve los triángulos de cada pieza importada."""
    meshkit.reset_scene()
    material = lamina.toon_preview_material(bpy.data.images.load(str(sheet_path)))
    tris = {}
    for spec in KIT:
        obj = lamina._import_pilot(export_dir / f'{spec.name}.fbx')
        where = LAYOUT[spec.name]
        _place(obj, where.x, where.y, where.turn_deg)
        tris[spec.name] = meshkit.triangle_count(obj)
        if spec.name in COPIES:
            twin = obj.copy()
            bpy.context.scene.collection.objects.link(twin)
            _place(twin, *COPIES[spec.name], where.turn_deg)
    lamina._prop('Suelo', lamina._ground(pal), 0.0, 0.0)
    lamina._prop('Capsula175', _capsule(pal), *CAPSULE_AT)
    for obj in (o for o in bpy.data.objects if o.type == 'MESH'):
        mesh_of(obj).materials.clear()
        mesh_of(obj).materials.append(material)
    lamina._pick(bpy.types.Light, 'type', 'SUN')
    sun_data = cast(bpy.types.SunLight, bpy.data.lights.new('Sol', 'SUN'))
    sun_data.energy = lamina.SUN_STRENGTH
    sun_data.angle = math.radians(0.6)
    sun = bpy.data.objects.new('Sol', sun_data)
    sun.rotation_euler = (math.radians(52.0), 0.0, math.radians(-62.0))
    bpy.context.scene.collection.objects.link(sun)
    world = bpy.data.worlds.new('Negro')
    world.color = (0.0, 0.0, 0.0)
    bpy.context.scene.world = world
    return tris


def label_anchors() -> dict[str, tuple[int, int]]:
    """Píxel (x, y desde arriba) de la base de cada pieza en el plano de la cámara."""
    scene = bpy.context.scene
    cam = scene.camera
    if cam is None:
        raise RuntimeError('la escena no tiene cámara')
    out = {}
    for name, where in LAYOUT.items():
        x, y = (where.x + 4.0, where.y - 2.0) if name in COPIES else (where.x, where.y)
        co = cast(Any, world_to_camera_view(scene, cam, Vector((x, y, where.anchor_z))))
        out[name] = (int(co.x * WIDTH), int((1.0 - co.y) * HEIGHT) + where.label_dy)
    return out


def _label(canvas: np.ndarray, text: str, cx: int, y: int, pal: Palette) -> np.ndarray:
    scale = 2
    w = raster.text_width(text, scale) + 12
    x = min(max(cx - w // 2, 4), WIDTH - w - 4)
    y = min(max(y, 4), HEIGHT - 5 * scale - 14)
    canvas = raster.fill_rect(canvas, x, y, w, 5 * scale + 10, pal.by_name('papel').rgb)
    return raster.draw_text(canvas, text, x + 6, y + 5, scale, pal.by_name('tinta').rgb)


def compose(panel: np.ndarray, tris: dict[str, int], anchors: dict[str, tuple[int, int]],
            pal: Palette) -> np.ndarray:
    canvas = lamina._fit(panel, (WIDTH, HEIGHT))
    for name, (cx, y) in anchors.items():
        canvas = _label(canvas, f'{name.upper()} {tris[name]}', cx, y, pal)
    title = f'K1 NATURALEZA - {len(tris)} MALLAS - {sum(tris.values())} TRIS - CAPSULA 1,75 M'
    canvas = raster.fill_rect(canvas, 12, 12, raster.text_width(title, 3) + 20, 5 * 3 + 16, pal.by_name('tinta').rgb)
    return raster.draw_text(canvas, title, 22, 20, 3, pal.by_name('papel').rgb)


def make_sheet(out: Path, export_dir: Path = EXPORT_DIR, sheet_path: Path = SHEET_PATH,
               engine: str = 'BLENDER_EEVEE', percent: int = 100) -> Path:
    pal = load_palette()
    missing = [s.name for s in KIT if not (export_dir / f'{s.name}.fbx').is_file()]
    if missing:
        raise FileNotFoundError(f'faltan FBX en {export_dir}: {", ".join(missing)}')
    tris = build_scene(pal, export_dir, sheet_path)
    with tempfile.TemporaryDirectory(prefix='lamina_k1_') as tmp:
        panel = lamina.over_background(lamina.render_shot(SHOT, Path(tmp), engine, percent),
                                       pal.by_name('cielo_dia').rgb)
    return raster.write_png(out, compose(panel, tris, label_anchors(), pal))
