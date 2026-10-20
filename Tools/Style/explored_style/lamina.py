"""Lámina de revisión de los pilotos (docs/v3/laminas/f2-pilotos.png).

Importa los FBX ya exportados (revisa lo que se entrega, no la escena de
generación), los coloca sobre arena con una cápsula de 1,75 m de referencia y
un único sol, y los sombrea como `M_Toon`: color de la hoja de paleta
multiplicado por 3 bandas de luz (sombra 0,6 / medio 0,8 / luz 1,0), sin
especular. La vista general y dos primeros planos (cuchillo y barril) se
componen en 1600 x 900 con la tira de la paleta debajo.

Uso: `uv run style-lamina [--out RUTA] [--engine BLENDER_EEVEE] [--percent 100]`.
"""

from __future__ import annotations

import argparse
import math
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any, cast

import bpy

import numpy as np
from mathutils import Vector

from explored_style import meshkit, raster
from explored_style.meshkit import mesh_of
from explored_style.palette import REPO_ROOT, SHEET_PATH, Palette, load_palette, strip_canvas
from explored_style.pilots import EXPORT_DIR, PILOTS

OUT_PATH = REPO_ROOT / 'docs' / 'v3' / 'laminas' / 'f2-pilotos.png'
WIDTH, HEIGHT = 1600, 900
STRIP_H = 110
MAIN_W = 1040
SIDE_W = WIDTH - MAIN_W
PANEL_H = HEIGHT - STRIP_H
BANDS = ((0.0, 0.6), (0.1, 0.8), (0.45, 1.0))  # (umbral de luz, factor) de M_Toon
SUN_STRENGTH = math.pi  # con difuso blanco, Shader to RGB devuelve cos(ángulo)


@dataclass(frozen=True)
class Shot:
    key: str
    location: tuple[float, float, float]
    target: tuple[float, float, float]
    lens: float
    size: tuple[int, int]


# Posición (x, y, giro en grados) de cada pieza en la escena de la lámina.
LAYOUT = {
    'SM_PalmA': (0.9, 1.4, 0.0),
    'SM_BarrilLluvia': (-1.35, 0.1, 0.0),
    'SM_Cuchillo': (-0.25, -0.75, -28.0),
}
CAPSULE_AT = (-2.9, 0.7)
CAPSULE_HEIGHT = 1.75
SHOTS = (
    Shot('general', (-0.4, -11.0, 2.6), (0.35, 0.6, 2.35), 37.0, (MAIN_W, PANEL_H)),
    Shot('cuchillo', (-0.05, -1.25, 0.32), (-0.25, -0.73, 0.0), 50.0, (SIDE_W, PANEL_H // 2)),
    Shot('barril', (-0.45, -2.45, 2.0), (-1.35, 0.1, 0.45), 50.0, (SIDE_W, PANEL_H - PANEL_H // 2)),
)


def _enum_ids(struct: object, prop: str) -> list[str]:
    return [i.identifier for i in struct.bl_rna.properties[prop].enum_items]  # type: ignore[attr-defined]


def _pick(struct: object, prop: str, wanted: str) -> str:
    """Valor de enum leído del RNA: «wanted» si existe; si no, error claro."""
    ids = _enum_ids(struct, prop)
    if wanted not in ids:
        raise RuntimeError(f'{prop}: {wanted} no existe en Blender {bpy.app.version_string} ({ids})')
    return wanted


def toon_preview_material(sheet: bpy.types.Image) -> bpy.types.Material:
    """Réplica de M_Toon para la lámina: hoja de paleta (Nearest) x bandas."""
    mat = bpy.data.materials.new('M_ToonPreview')
    tree = mat.node_tree
    if tree is None:
        raise RuntimeError('el material nuevo no trae árbol de nodos')
    nodes, links = tree.nodes, tree.links
    out = next(n for n in nodes if n.type == 'OUTPUT_MATERIAL')
    for node in [n for n in nodes if n.type != 'OUTPUT_MATERIAL']:
        nodes.remove(node)
    tex = cast(bpy.types.ShaderNodeTexImage, nodes.new('ShaderNodeTexImage'))
    tex.image = sheet
    _pick(tex, 'interpolation', 'Closest')
    tex.interpolation = 'Closest'
    diffuse = nodes.new('ShaderNodeBsdfDiffuse')
    cast(Any, diffuse.inputs[0]).default_value = (1.0, 1.0, 1.0, 1.0)
    to_rgb = nodes.new('ShaderNodeShaderToRGB')
    ramp = cast(bpy.types.ShaderNodeValToRGB, nodes.new('ShaderNodeValToRGB'))
    color_ramp = cast(bpy.types.ColorRamp, ramp.color_ramp)
    _pick(color_ramp, 'interpolation', 'CONSTANT')
    color_ramp.interpolation = 'CONSTANT'
    elements = color_ramp.elements
    while len(elements) < len(BANDS):
        elements.new(0.5)
    for element, (pos, factor) in zip(elements, BANDS, strict=True):
        element.position = pos
        cast(Any, element).color = (factor, factor, factor, 1.0)
    mix = cast(bpy.types.ShaderNodeMix, nodes.new('ShaderNodeMix'))
    _pick(mix, 'data_type', 'RGBA')
    _pick(mix, 'blend_type', 'MULTIPLY')
    mix.data_type = 'RGBA'
    mix.blend_type = 'MULTIPLY'
    emission = nodes.new('ShaderNodeEmission')
    # Los stubs tipan las colecciones de sockets como Optional y con claves
    # literales incompletas; los sockets se buscan por identificador.
    mix_in, mix_out = cast(Any, mix.inputs), cast(Any, mix.outputs)
    mix_in['Factor'].default_value = 1.0
    d, t, r, x, e, o = (cast(Any, n) for n in (diffuse, to_rgb, ramp, tex, emission, out))
    links.new(d.outputs[0], t.inputs[0])
    links.new(t.outputs['Color'], r.inputs['Fac'])
    links.new(x.outputs['Color'], next(s for s in mix_in if s.identifier == 'A_Color'))
    links.new(r.outputs['Color'], next(s for s in mix_in if s.identifier == 'B_Color'))
    links.new(next(s for s in mix_out if s.identifier == 'Result_Color'), e.inputs['Color'])
    links.new(e.outputs[0], o.inputs['Surface'])
    return mat


def _import_pilot(path: Path) -> bpy.types.Object:
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(path))
    new = [o for o in bpy.data.objects if o not in before and o.type == 'MESH']
    if len(new) != 1:
        raise RuntimeError(f'{path.name}: se esperaba 1 malla y hay {len(new)}')
    return new[0]


def _prop(name: str, builder: meshkit.MeshBuilder, x: float, y: float) -> bpy.types.Object:
    obj = meshkit.build_object(name, builder)
    obj.location = (x, y, 0.0)
    return obj


def _ground(pal: Palette) -> meshkit.MeshBuilder:
    b = meshkit.MeshBuilder()
    s = 40.0
    quad = [b.vert((-s, -s, 0.0)), b.vert((s, -s, 0.0)), b.vert((s, s, 0.0)), b.vert((-s, s, 0.0))]
    b.face(quad, pal.index_of('arena_clara'))
    return b


def _capsule(pal: Palette) -> meshkit.MeshBuilder:
    b = meshkit.MeshBuilder()
    r, h = 0.25, CAPSULE_HEIGHT
    profile = [(r * math.sin(math.radians(a)), r - r * math.cos(math.radians(a))) for a in (30, 60, 90)]
    profile += [(r * math.sin(math.radians(a)), h - r + r * math.cos(math.radians(a))) for a in (90, 60, 30)]
    color = pal.index_of('cielo_noche')
    meshkit.lathe(b, profile, 12, [color] * (len(profile) - 1), cap_bottom=color, cap_top=color, tips=(0.0, h))
    return b


def build_scene(pal: Palette, export_dir: Path, sheet_path: Path) -> dict[str, int]:
    """Monta la escena y devuelve los triángulos de cada pieza importada."""
    meshkit.reset_scene()
    sheet = bpy.data.images.load(str(sheet_path))
    material = toon_preview_material(sheet)
    tris = {}
    for spec in PILOTS:
        obj = _import_pilot(export_dir / f'{spec.name}.fbx')
        x, y, turn = LAYOUT[spec.name]
        obj.location = (x, y, 0.0)
        obj.rotation_euler = (0.0, 0.0, math.radians(turn))
        tris[spec.name] = meshkit.triangle_count(obj)
    _prop('Suelo', _ground(pal), 0.0, 0.0)
    _prop('Capsula175', _capsule(pal), *CAPSULE_AT)
    for obj in (o for o in bpy.data.objects if o.type == 'MESH'):
        mesh_of(obj).materials.clear()
        mesh_of(obj).materials.append(material)
    _pick(bpy.types.Light, 'type', 'SUN')
    sun_data = cast(bpy.types.SunLight, bpy.data.lights.new('Sol', 'SUN'))
    sun_data.energy = SUN_STRENGTH
    sun_data.angle = math.radians(0.6)
    sun = bpy.data.objects.new('Sol', sun_data)
    sun.rotation_euler = (math.radians(48.0), 0.0, math.radians(-38.0))
    bpy.context.scene.collection.objects.link(sun)
    world = bpy.data.worlds.new('Negro')
    world.color = (0.0, 0.0, 0.0)
    bpy.context.scene.world = world
    return tris


def _set_engine(scene: bpy.types.Scene, engine: str) -> None:
    # el enum de motores depende de la compilación: se asigna y, si no existe,
    # el TypeError de Blender trae la lista de motores válidos.
    try:
        render: Any = scene.render  # enum de motores tipado como literal en los stubs
        render.engine = engine
    except TypeError as exc:
        raise RuntimeError(f'motor de render no válido: {engine} ({exc})') from exc
    display, eevee = scene.display, scene.eevee
    if engine == 'BLENDER_WORKBENCH' and display is not None:
        shading = cast(Any, display.shading)
        shading.light = _pick(shading, 'light', 'FLAT')
        shading.color_type = _pick(shading, 'color_type', 'TEXTURE')
    elif engine == 'BLENDER_EEVEE' and eevee is not None:
        eevee.taa_render_samples = 32


def render_shot(shot: Shot, out_dir: Path, engine: str, percent: int) -> np.ndarray:
    """Renderiza un plano y lo devuelve como RGB uint8 sobre el cielo."""
    scene = bpy.context.scene
    _set_engine(scene, engine)
    # view_transform es un enum dinámico (OCIO): su RNA estático no lista los
    # valores, así que se asigna y el TypeError trae la lista válida.
    try:
        scene.view_settings.view_transform = 'Standard'
        scene.view_settings.look = 'None'
    except TypeError as exc:
        raise RuntimeError(f'transformación de vista no disponible: {exc}') from exc
    scene.render.film_transparent = True
    scene.render.resolution_x, scene.render.resolution_y = shot.size
    scene.render.resolution_percentage = percent
    scene.render.image_settings.file_format = 'PNG'
    scene.render.image_settings.color_mode = 'RGBA'
    cam_data = bpy.data.cameras.new(f'Cam_{shot.key}')
    cam_data.lens = shot.lens
    cam = bpy.data.objects.new(f'Cam_{shot.key}', cam_data)
    scene.collection.objects.link(cam)
    cam.location = shot.location
    cam.rotation_euler = (Vector(shot.target) - Vector(shot.location)).to_track_quat('-Z', 'Y').to_euler()
    scene.camera = cam
    path = out_dir / f'{shot.key}.png'
    scene.render.filepath = str(path)
    bpy.ops.render.render(write_still=True)
    return load_rgba(path)


def load_rgba(path: Path) -> np.ndarray:
    """PNG -> RGBA float (0-1) con la fila 0 arriba. Las imágenes de 8 bits
    devuelven sus bytes / 255 sin conversión de color."""
    image = bpy.data.images.load(str(path))
    size: Any = image.size
    w, h = int(size[0]), int(size[1])
    buf = np.empty(w * h * 4, dtype=np.float32)
    image.pixels.foreach_get(buf)
    bpy.data.images.remove(image)
    return buf.reshape(h, w, 4)[::-1]


def over_background(rgba: np.ndarray, background: raster.Rgb) -> np.ndarray:
    """Compone RGBA (alfa no premultiplicado) sobre un color liso."""
    alpha = rgba[..., 3:4]
    bg = np.array(background, dtype=np.float32) / 255.0
    rgb = rgba[..., :3] * alpha + bg * (1.0 - alpha)
    return np.clip(np.rint(rgb * 255.0), 0, 255).astype(np.uint8)


def _fit(image: np.ndarray, size: tuple[int, int]) -> np.ndarray:
    """Escala al vecino más próximo (solo para renders a porcentaje reducido)."""
    w, h = size
    if image.shape[1] == w and image.shape[0] == h:
        return image
    ys = (np.arange(h) * image.shape[0] // h).clip(0, image.shape[0] - 1)
    xs = (np.arange(w) * image.shape[1] // w).clip(0, image.shape[1] - 1)
    return image[ys][:, xs]


def _caption(canvas: np.ndarray, text: str, x: int, y: int, pal: Palette) -> np.ndarray:
    scale = 2
    w = raster.text_width(text, scale) + 16
    canvas = raster.fill_rect(canvas, x, y, w, 5 * scale + 12, pal.by_name('papel').rgb)
    return raster.draw_text(canvas, text, x + 8, y + 6, scale, pal.by_name('tinta').rgb)


def compose(panels: dict[str, np.ndarray], tris: dict[str, int], pal: Palette) -> np.ndarray:
    """Lámina final: general a la izquierda, primeros planos a la derecha,
    tira de paleta abajo y pies con nombre y triángulos."""
    canvas = raster.new_canvas(WIDTH, HEIGHT, pal.by_name('tinta').rgb)
    side_top = PANEL_H // 2
    canvas = raster.paste(canvas, _fit(panels['general'], (MAIN_W, PANEL_H)), 0, 0)
    canvas = raster.paste(canvas, _fit(panels['cuchillo'], (SIDE_W - 4, side_top - 2)), MAIN_W + 4, 0)
    canvas = raster.paste(canvas, _fit(panels['barril'], (SIDE_W - 4, PANEL_H - side_top - 2)), MAIN_W + 4,
                          side_top + 2)
    canvas = raster.paste(canvas, strip_canvas(pal, WIDTH, STRIP_H - 4), 0, PANEL_H + 4)
    canvas = _caption(canvas, f'F2 PILOTOS - SM_PALMA {tris["SM_PalmA"]} TRIS - CAPSULA 1,75 M', 12, 12, pal)
    canvas = _caption(canvas, f'SM_CUCHILLO {tris["SM_Cuchillo"]} TRIS', MAIN_W + 16, 12, pal)
    return _caption(canvas, f'SM_BARRILLLUVIA {tris["SM_BarrilLluvia"]} TRIS', MAIN_W + 16, side_top + 14, pal)


def make_sheet(out: Path, export_dir: Path = EXPORT_DIR, sheet_path: Path = SHEET_PATH,
               engine: str = 'BLENDER_EEVEE', percent: int = 100) -> Path:
    pal = load_palette()
    tris = build_scene(pal, export_dir, sheet_path)
    sky = pal.by_name('cielo_dia').rgb
    with tempfile.TemporaryDirectory(prefix='lamina_') as tmp:
        panels = {s.key: over_background(render_shot(s, Path(tmp), engine, percent), sky) for s in SHOTS}
    return raster.write_png(out, compose(panels, tris, pal))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog='style-lamina', description='Renderiza la lámina de los pilotos.')
    parser.add_argument('--out', type=Path, default=OUT_PATH)
    parser.add_argument('--export-dir', type=Path, default=EXPORT_DIR)
    parser.add_argument('--engine', default='BLENDER_EEVEE', help='BLENDER_EEVEE o BLENDER_WORKBENCH')
    parser.add_argument('--percent', type=int, default=100, help='porcentaje de resolución (pruebas)')
    args = parser.parse_args(argv)
    missing = [s.name for s in PILOTS if not (args.export_dir / f'{s.name}.fbx').is_file()]
    if missing:
        print(f'[lamina] faltan FBX en {args.export_dir}: {", ".join(missing)} (ejecuta style-pilotos)')
        return 2
    path = make_sheet(args.out, args.export_dir, SHEET_PATH, args.engine, args.percent)
    print(f'[lamina] escrito {path}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
