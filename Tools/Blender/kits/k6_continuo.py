"""Lámina y medida del césped continuo (docs/v3/laminas/k6-cesped-continuo.png, 1600 x 900).

Importa los FBX exportados del kit K6 y compone:

- arriba, los tres parches `SM_GrassPatch_A/B/C_01` de cerca (cámara ortográfica a 30°);
- abajo, dos parcelas de 4 x 4 m vistas desde 1,7 m de altura mirando 30° hacia abajo:
  a la izquierda las matas actuales (`SM_GrassTuft_A/B/C_01`, 2,2 matas/m²) y a la derecha
  los parches (5,5 parches/m², mezcla A/B/C al azar, escala 0,8-1,3 y giro al azar), sobre
  el verde salvia del terreno. En la esquina de cada parcela, su vista cenital con el suelo
  en un color testigo (lo que se ve de ese color es suelo descubierto).

Medida: render Workbench cenital ortográfico de cada parcela (luz plana, sin sombras ni
antialiasing, 400 x 400 px = 1 cm/px) con el suelo de la parcela en el color testigo;
suelo visible = píxeles testigo con la hierba / píxeles testigo sin ella. Lo mismo desde la
cámara a 1,7 m y 30°. La resolución de la medida no depende de `--percent`.
"""

from __future__ import annotations

import math
import random
import tempfile
from pathlib import Path
from typing import Any, cast

import bpy

import numpy as np
from explored_style import lamina, meshkit, raster
from explored_style.meshkit import mesh_of
from explored_style.palette import REPO_ROOT, SHEET_PATH, Palette, load_palette

from kits import k6_lamina as base
from kits.k6_cesped import EXPORT_DIR, K6

OUT_PATH = REPO_ROOT / 'docs' / 'v3' / 'laminas' / 'k6-cesped-continuo.png'
WIDTH, HEIGHT = 1600, 900
ROW_H = 360
PANEL_W, PANEL_H = WIDTH // 2, HEIGHT - ROW_H
INSET = 170
TUFTS = tuple(s.asset for s in K6 if s.name.startswith('SM_GrassTuft'))
PATCHES = tuple(s.asset for s in K6 if s.is_patch)
PARCEL = 4.0
TUFT_DENSITY, PATCH_DENSITY = 2.2, 5.5          # instancias por m²
TUFT_SCALE, PATCH_SCALE = (0.85, 1.2), (0.8, 1.3)
OLD_X, NEW_X = -7.0, 7.0                        # centro de cada parcela
ROW_Y = -30.0
ROW_X = (-1.05, 0.0, 1.05)
SEED = 707
GROUND = 'hierba_salvia'                        # verde dominante del terreno (Tools/Island)
MARKER = 'malva_ocaso'                          # testigo: ningún verde del césped se le parece
COVER_PX = 400
EYE_HEIGHT, PITCH = 1.7, 30.0
EYE_BACK = 1.5                                  # m por detrás del borde de la parcela
# Interior de 3 x 3 m (sin 0,5 m de cada borde): sin el efecto de borde de la parcela,
# donde falta la mitad de los vecinos; es lo que se ve en un prado sembrado entero.
INTERIOR_BORDER = 0.125


def sow(names: tuple[str, ...], density: float, scale: tuple[float, float], seed: int = SEED,
        ) -> list[tuple[str, float, float, float, float]]:
    """(malla, x, y, giro, escala) en una rejilla con jitter: round(densidad x 16 m²)
    celdas ocupadas, elegidas al azar entre las de la rejilla cuadrada mínima."""
    rng = random.Random(seed)
    count = round(density * PARCEL * PARCEL)
    side = math.ceil(math.sqrt(count))
    cell = PARCEL / side
    out = []
    for index in sorted(rng.sample(range(side * side), count)):
        i, j = divmod(index, side)
        x = -PARCEL / 2 + (i + rng.uniform(0.05, 0.95)) * cell
        y = -PARCEL / 2 + (j + rng.uniform(0.05, 0.95)) * cell
        out.append((rng.choice(names), x, y, rng.uniform(0.0, 360.0), rng.uniform(*scale)))
    return out


def _ground_pair(pal: Palette, name: str, cx: float) -> None:
    """Suelo verde de la parcela y su gemelo en color testigo (se alterna al medir)."""
    for suffix, color in (('', GROUND), ('_testigo', MARKER)):
        obj = meshkit.build_object(f'{name}{suffix}', base._quad(pal.index_of(color), PARCEL / 2, PARCEL / 2),
                                   base_pivot=False)
        obj.location = (cx, 0.0, 0.002)
        obj.hide_render = bool(suffix)


def build_scene(pal: Palette, export_dir: Path, sheet_path: Path) -> dict[str, Any]:
    meshkit.reset_scene()
    material = lamina.toon_preview_material(bpy.data.images.load(str(sheet_path)))
    sources = {name: lamina._import_pilot(export_dir / f'{name}.fbx') for name in TUFTS + PATCHES}
    tris = {name: meshkit.triangle_count(obj) for name, obj in sources.items()}
    for name, x in zip(PATCHES, ROW_X, strict=True):
        base._place(sources[name], x, ROW_Y)
    for name in TUFTS:
        sources[name].hide_render = True
    scene = bpy.context.scene
    plots = {'matas': (sow(TUFTS, TUFT_DENSITY, TUFT_SCALE), OLD_X),
             'parches': (sow(PATCHES, PATCH_DENSITY, PATCH_SCALE), NEW_X)}
    grass: list[bpy.types.Object] = []
    for planted, cx in plots.values():
        for name, x, y, turn, scale in planted:
            instance = sources[name].copy()        # instancia: comparte la malla
            instance.hide_render = False
            scene.collection.objects.link(instance)
            base._place(instance, cx + x, y, turn, scale)
            grass.append(instance)
    _ground_pair(pal, 'ParcelaMatas', OLD_X)
    _ground_pair(pal, 'ParcelaParches', NEW_X)
    world = lamina._ground(pal)
    world.colors = [pal.index_of(GROUND)] * len(world.colors)
    meshkit.build_object('Suelo', world)
    meshkit.build_object('FilaSuelo', base._quad(pal.index_of(GROUND), 1.9, 0.75),
                         base_pivot=False).location = (0.0, ROW_Y, 0.002)
    for obj in (o for o in bpy.data.objects if o.type == 'MESH'):
        mesh_of(obj).materials.clear()
        mesh_of(obj).materials.append(material)
    area = PARCEL * PARCEL
    return {
        'tris': tris, 'grass': grass,
        'plots': {key: {'instancias': len(p), 'tris_m2': round(sum(tris[n] for n, *_ in p) / area, 1)}
                  for key, (p, _cx) in plots.items()},
    }


def eye_camera(key: str, cx: float, size: tuple[int, int]) -> base.Camera:
    """Cámara a 1,7 m de altura que mira 30° hacia abajo, detrás del borde sur de la parcela."""
    y = -PARCEL / 2 - EYE_BACK
    reach = EYE_HEIGHT / math.tan(math.radians(PITCH))
    return base.Camera(key, (cx, y, EYE_HEIGHT), (cx, y + reach, 0.0), size, lens=22.0)


def top_camera(key: str, cx: float) -> base.Camera:
    # cenital: un pelo de inclinación para que to_track_quat tenga un «arriba» estable
    return base.Camera(key, (cx, 0.0, 10.0), (cx, 1e-4, 0.0), (COVER_PX, COVER_PX), ortho_scale=PARCEL)


def _set_aa(scene: bpy.types.Scene, wanted: str) -> str:
    display = cast(Any, scene.display)
    previous = display.render_aa
    display.render_aa = lamina._pick(display, 'render_aa', wanted)
    return previous


def _marker_count(image: np.ndarray, rgb: tuple[int, int, int], border: float = 0.0) -> int:
    """Píxeles del color testigo, descartando «border» (fracción) de cada borde."""
    h, w = image.shape[:2]
    by, bx = round(h * border), round(w * border)
    image = image[by:h - by, bx:w - bx]
    pixels = np.rint(image[..., :3] * 255.0).astype(np.int16)
    alpha = image[..., 3] > 0.99
    return int((alpha & (np.abs(pixels - np.array(rgb)).max(axis=-1) <= 2)).sum())


def visible_ground(cam: base.Camera, grass: list[bpy.types.Object], pal: Palette, tmp: Path,
                   border: float = 0.0) -> tuple[float, np.ndarray]:
    """% de suelo de la parcela visible desde «cam» (sin «border» de cada lado de la
    imagen) y el render con el testigo."""
    markers = [o for o in bpy.data.objects if o.name.endswith('_testigo')]
    greens = [bpy.data.objects[o.name.removesuffix('_testigo')] for o in markers]
    scene = bpy.context.scene
    previous = _set_aa(scene, 'OFF')
    for obj in markers:
        obj.hide_render = False
    for obj in greens:
        obj.hide_render = True
    try:
        for obj in grass:
            obj.hide_render = True
        empty = _marker_count(base.render(base.Camera(f'{cam.key}_vacio', *_cam_args(cam)), tmp, 100),
                              pal.by_name(MARKER).rgb, border)
        for obj in grass:
            obj.hide_render = False
        full = base.render(cam, tmp, 100)
    finally:
        for obj in markers:
            obj.hide_render = True
        for obj in greens:
            obj.hide_render = False
        scene.display.render_aa = previous
    if empty == 0:
        raise RuntimeError(f'{cam.key}: la parcela no se ve desde la cámara')
    return round(100.0 * _marker_count(full, pal.by_name(MARKER).rgb, border) / empty, 1), full


def _cam_args(cam: base.Camera) -> tuple[Any, ...]:
    return cam.location, cam.target, cam.size, cam.ortho_scale, cam.lens


def measure(info: dict[str, Any], pal: Palette, tmp: Path) -> tuple[dict[str, Any], dict[str, np.ndarray]]:
    insets, cenital, interior, oblique = {}, {}, {}, {}
    for key, cx in (('matas', OLD_X), ('parches', NEW_X)):
        cenital[key], top = visible_ground(top_camera(f'cenital_{key}', cx), info['grass'], pal, tmp)
        interior[key], _img = visible_ground(top_camera(f'interior_{key}', cx), info['grass'], pal, tmp,
                                             border=INTERIOR_BORDER)
        oblique[key], _img = visible_ground(eye_camera(f'ojo_{key}', cx, (PANEL_W, PANEL_H)), info['grass'],
                                            pal, tmp)
        insets[key] = lamina.over_background(top, pal.by_name('tinta').rgb)
    tris = info['tris']
    metrics = {
        'tris_por_parche': {name: tris[name] for name in PATCHES},
        'densidad_por_m2': {'matas': TUFT_DENSITY, 'parches': PATCH_DENSITY},
        'tris_por_m2': {key: plot['tris_m2'] for key, plot in info['plots'].items()},
        'instancias_en_16_m2': {key: plot['instancias'] for key, plot in info['plots'].items()},
        'suelo_visible_cenital_pct': cenital,
        'suelo_visible_cenital_interior_3x3_m_pct': interior,
        'suelo_visible_desde_1_7_m_a_30_grados_pct': oblique,
        'siembra': (f'parcela de 4 x 4 m en rejilla con jitter (semilla {SEED}); matas A/B/C a escala '
                    f'{TUFT_SCALE[0]}-{TUFT_SCALE[1]}, parches A/B/C a escala {PATCH_SCALE[0]}-{PATCH_SCALE[1]}; '
                    'mezcla y giro al azar'),
        'medida': (f'Workbench con luz plana, sin sombras ni antialiasing; cenital ortográfico de {COVER_PX} x '
                   f'{COVER_PX} px (1 cm/px) con el suelo de la parcela en {MARKER}; píxeles de suelo con '
                   'hierba / sin hierba (Tools/Blender/kits/k6_continuo.py)'),
    }
    return metrics, insets


def _label(canvas: np.ndarray, text: str, x: int, y: int, pal: Palette, scale: int = 2) -> np.ndarray:
    w = raster.text_width(text, scale) + 12
    x = min(max(x, 4), WIDTH - w - 4)
    canvas = raster.fill_rect(canvas, x, y, w, 5 * scale + 10, pal.by_name('papel').rgb)
    return raster.draw_text(canvas, text, x + 6, y + 5, scale, pal.by_name('tinta').rgb)


def _pct(value: float) -> str:
    return f'{value:.1f}'.replace('.', ',')


def compose(images: dict[str, np.ndarray], metrics: dict[str, Any], pal: Palette) -> np.ndarray:
    canvas = raster.new_canvas(WIDTH, HEIGHT, pal.by_name('tinta').rgb)
    canvas = raster.paste(canvas, lamina._fit(images['fila'], (WIDTH, ROW_H - 2)), 0, 0)
    tris = metrics['tris_por_parche']
    for k, name in enumerate(PATCHES):
        canvas = _label(canvas, f'{name} {tris[name]} TRIS', 120 + k * 500, ROW_H - 44, pal)
    canvas = _label(canvas, 'K6 CESPED CONTINUO - PARCHES DE 12-18 HOJAS, UNA SOLA LOD', 16, 12, pal, scale=3)
    cen, eye = metrics['suelo_visible_cenital_pct'], metrics['suelo_visible_desde_1_7_m_a_30_grados_pct']
    inner = metrics['suelo_visible_cenital_interior_3x3_m_pct']
    texts = {'matas': f'ACTUAL: MATAS A/B/C A {_pct(TUFT_DENSITY)}/M2',
             'parches': f'NUEVO: PARCHES A/B/C A {_pct(PATCH_DENSITY)}/M2'}
    for k, key in enumerate(('matas', 'parches')):
        x0, y0 = k * (PANEL_W + 2), ROW_H + 2
        canvas = raster.paste(canvas, lamina._fit(images[key], (PANEL_W - 2, PANEL_H - 2)), x0, y0)
        canvas = raster.paste(canvas, lamina._fit(images[f'cenital_{key}'], (INSET, INSET)),
                              x0 + PANEL_W - INSET - 14, y0 + 12)
        plot = metrics['tris_por_m2'][key]
        canvas = _label(canvas, f'{texts[key]} - {plot:.0f} TRIS/M2', x0 + 12, y0 + 12, pal)
        canvas = _label(canvas, f'SUELO VISIBLE: CENITAL {_pct(cen[key])} (INTERIOR 3X3 M {_pct(inner[key])}) - '
                                f'A 1,7 M Y 30 GRADOS {_pct(eye[key])} POR CIENTO', x0 + 12, y0 + PANEL_H - 40, pal)
    return canvas


def make_sheet(out: Path, export_dir: Path = EXPORT_DIR, sheet_path: Path = SHEET_PATH,
               percent: int = 100) -> tuple[Path, dict[str, Any]]:
    """Renderiza la lámina y devuelve su ruta y las cifras medidas (para el manifiesto)."""
    pal = load_palette()
    missing = [n for n in TUFTS + PATCHES if not (export_dir / f'{n}.fbx').is_file()]
    if missing:
        raise FileNotFoundError(f'faltan FBX: {", ".join(missing)}')
    info = build_scene(pal, export_dir, sheet_path)
    sky = pal.by_name('cielo_dia').rgb
    with tempfile.TemporaryDirectory(prefix='lamina_k6c_') as tmp:
        metrics, insets = measure(info, pal, Path(tmp))
        row_cam = base.Camera('fila', (0.0, ROW_Y - 4.0, 2.15), (0.0, ROW_Y, 0.16), (WIDTH, ROW_H), ortho_scale=3.6)
        images = {
            'fila': lamina.over_background(base.render(row_cam, Path(tmp), percent), sky),
            'matas': lamina.over_background(base.render(eye_camera('matas', OLD_X, (PANEL_W, PANEL_H)), Path(tmp),
                                                        percent), sky),
            'parches': lamina.over_background(base.render(eye_camera('parches', NEW_X, (PANEL_W, PANEL_H)),
                                                          Path(tmp), percent), sky),
            'cenital_matas': insets['matas'], 'cenital_parches': insets['parches'],
        }
    return raster.write_png(out, compose(images, metrics, pal)), metrics
