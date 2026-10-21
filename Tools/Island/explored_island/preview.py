"""Vista previa del terreno sin Unreal: lámina «antes / después» renderizada con Blender (EEVEE).

Cada columna es una versión del terreno: el FBX de Art/Island y los island_decor/island_props de
una revisión de git (o del árbol de trabajo). Se importa el FBX tal cual se entrega (normales por
esquina y color por vértice «Col»; los terrenos facetados antiguos, sin color por vértice, con la
hoja de paleta por UV), se añade el mar como un plano turquesa, se colocan las mallas K1 de
island_decor.json y los objetos de island_props.json con FBX en Art/Export (apoyados en el terreno
con un rayo) y se renderizan las vistas cala, palmar y faro de las capturas f4b/f6 (mismas
cámaras que ExploredShotSubsystem, FOV horizontal de 90°).

El material del terreno replica M_TerrainToon: color por vértice multiplicado por la máscara de
trazos T_TerrainStrokes (Tools/Textures/strokes.json) en espacio de mundo, que se funde con la
pendiente y la distancia.

Uso: uv run island-preview [--out docs/v3/laminas/relieve-antes-despues.png]
                           [--column ETIQUETA=REVISIÓN ...] [--percent 100]
REVISIÓN «.» = árbol de trabajo.
"""

from __future__ import annotations

import argparse
import json
import math
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any, cast

import bpy

import numpy as np
from explored_style import raster
from explored_style.lamina import load_rgba, over_background
from explored_style.meshkit import reset_scene
from explored_style.palette import SHEET_PATH, load_palette
from mathutils import Vector

from explored_island.layout import REPO_ROOT, Layout, height_at, load_layout
from explored_island.preview_materials import flat_material, palette_material, terrain_material

OUT = REPO_ROOT / 'docs' / 'v3' / 'laminas' / 'relieve-antes-despues.png'
STROKES_JSON = REPO_ROOT / 'Tools' / 'Textures' / 'strokes.json'
STROKES_PNG = REPO_ROOT / 'Art' / 'Export' / 'Textures' / 'T_TerrainStrokes.png'
TERRAIN_FBX = 'Art/Island/SM_IslaSal_Terrain.fbx'
DECOR = 'Content/Data/island_decor.json'
PROPS = 'Content/Data/island_props.json'
EXPORT_DIR = REPO_ROOT / 'Art' / 'Export'
DEFAULT_COLUMNS = ('F4B=c7b220c3', 'F6 SUAVE=8d3fa23b', 'RELIEVE=.')
VIEW_W, VIEW_H = 640, 360
LFS_POINTER = b'version https://git-lfs.github.com/spec/'
PAD, LABEL_H, SCALE = 12, 30, 3


@dataclass(frozen=True)
class View:
    key: str
    eye: tuple[float, float, float]     # metros, +X norte, +Y este (mundo de Unreal / 100)
    target: tuple[float, float, float]


def ground(layout: Layout, x: float, y: float) -> float:
    return max(float(height_at(layout, x, y)), 0.0)


def views(layout: Layout) -> tuple[View, ...]:
    """Las vistas cala, palmar, faro e islote del conjunto f4b de ExploredShotSubsystem y un primer plano."""
    palm_eye, palm_target = (-62.0, -38.0), (-30.0, -8.0)
    faro_eye = (22.0, -6.0)
    suelo_eye, suelo_target = (-8.0, -26.0), (4.0, -14.0)
    return (
        View('cala', (-150.0, -12.0, 7.0), (-98.0, 2.0, 2.0)),
        View('palmar', (*palm_eye, ground(layout, *palm_eye) + 3.0), (*palm_target, ground(layout, *palm_target) + 3.0)),
        View('faro', (*faro_eye, ground(layout, *faro_eye) + 4.0), (42.0, 15.0, ground(layout, 45.0, 15.0) + 6.0)),
        # Primer plano a la altura de los ojos en la ladera del Palmar: textura y relieve de cerca.
        View('suelo', (*suelo_eye, ground(layout, *suelo_eye) + 1.7), (*suelo_target, ground(layout, *suelo_target))),
        # El islote del carguero desde Playa Norte (f4b-islote): la ladera rocosa en primer plano.
        View('islote', (200.0, 166.0, ground(layout, 200.0, 166.0) + 9.0), (229.0, 148.0, 3.0)),
    )


def to_blender(x: float, y: float, z: float) -> tuple[float, float, float]:
    """Mundo del juego -> escena de Blender (el FBX invierte Y, como el importador de Unreal)."""
    return (x, -y, z)


def lfs_content(path: Path, folder: Path) -> Path:
    """«path» si ya tiene el contenido; si es un puntero de Git LFS (checkout sin LFS), su contenido
    descargado con `git lfs smudge` en «folder»."""
    data = path.read_bytes()
    if not data.startswith(LFS_POINTER):
        return path
    target = folder / f'lfs_{path.name}'
    target.write_bytes(subprocess.run(['git', 'lfs', 'smudge'], cwd=REPO_ROOT, input=data, check=True,
                                      capture_output=True).stdout)
    return target


def git_file(revision: str, rel_path: str, folder: Path) -> Path:
    """Fichero del árbol de trabajo («.») o de una revisión de git, con su contenido LFS."""
    if revision == '.':
        return lfs_content(REPO_ROOT / rel_path, folder)
    target = folder / f'{revision}_{Path(rel_path).name}'
    target.write_bytes(subprocess.run(['git', 'show', f'{revision}:{rel_path}'], cwd=REPO_ROOT, check=True,
                                      capture_output=True).stdout)
    return lfs_content(target, folder)


def import_fbx(path: Path) -> list[bpy.types.Object]:
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(path))
    return [o for o in bpy.data.objects if o not in before and o.type == 'MESH']


def kit_fbx(mesh_path: str) -> Path | None:
    """/Game/Art/<Kit>/<Nombre> -> Art/Export/<Kit>/<Nombre>.fbx, si existe."""
    parts = mesh_path.strip('/').split('/')
    if len(parts) != 4 or not mesh_path.startswith('/Game/Art/'):
        return None
    path = EXPORT_DIR / parts[2] / f'{parts[3]}.fbx'
    return path if path.is_file() else None


class Library:
    """Mallas de kit importadas una vez y reutilizadas por objetos que comparten los datos."""

    def __init__(self, material: bpy.types.Material, folder: Path) -> None:
        self.material = material
        self.folder = folder
        self.meshes: dict[str, bpy.types.Mesh | None] = {}
        self.count = 0

    def mesh(self, mesh_path: str) -> bpy.types.Mesh | None:
        if mesh_path not in self.meshes:
            path = kit_fbx(mesh_path)
            data = None
            if path is not None:
                objs = import_fbx(lfs_content(path, self.folder))
                data = cast(bpy.types.Mesh, objs[0].data) if objs else None
                if data is not None:
                    data.materials.clear()
                    data.materials.append(self.material)
                for obj in objs:
                    bpy.data.objects.remove(obj)
            self.meshes[mesh_path] = data
        return self.meshes[mesh_path]

    def place(self, mesh_path: str, x: float, y: float, z: float, yaw: float, scale: float = 1.0) -> bool:
        data = self.mesh(mesh_path)
        if data is None:
            return False
        obj = bpy.data.objects.new(f'I{self.count}', data)
        self.count += 1
        obj.location = to_blender(x, y, z)
        # Unreal invierte Y al importar: en la escena (Y invertida) el giro de yaw es −yaw.
        obj.rotation_euler = (0.0, 0.0, math.radians(-yaw))
        obj.scale = (scale, scale, scale)
        bpy.context.scene.collection.objects.link(obj)
        return True


def ground_ray(x: float, y: float) -> float:
    """Altura del terreno (primer impacto de un rayo vertical) en el punto del juego (x, y)."""
    scene = bpy.context.scene
    depsgraph = bpy.context.evaluated_depsgraph_get()
    hit, location, *_ = scene.ray_cast(depsgraph, Vector(to_blender(x, y, 500.0)), Vector((0.0, 0.0, -1.0)))
    return float(location.z) if hit else 0.0


def place_props(props: dict, library: Library) -> int:
    """Objetos de island_props.json con FBX en Art/Export, apoyados con un rayo (como el poblador)."""
    types = props['types']
    grounds = {p['id']: ground_ray(float(p['at'][0]), float(p['at'][1])) for p in props['props']}
    placed = []
    for prop in props['props']:
        mesh = types.get(prop['type'], {}).get('mesh', '')
        x, y = (float(v) for v in prop['at'])
        floor = grounds.get(prop.get('anchor', prop['id']), grounds[prop['id']])
        placed.append((mesh, x, y, floor + float(prop.get('elevation', 0.0)), float(prop.get('yaw', 0.0))))
    return sum(library.place(*p) for p in placed if p[0])


def place_decor(decor: dict, library: Library) -> int:
    meshes = decor['meshes']
    return sum(library.place(meshes[int(m)], x, y, z, yaw, s) for m, x, y, z, yaw, s in decor['instances'])


def _sun_and_sky() -> None:
    pal = load_palette()
    sun_data = cast(bpy.types.SunLight, bpy.data.lights.new('Sol', 'SUN'))
    sun_data.energy = 4.5
    sun_data.angle = math.radians(1.5)
    sun_data.color = (1.0, 0.95, 0.86)
    sun = bpy.data.objects.new('Sol', sun_data)
    sun.rotation_euler = (math.radians(45.0), 0.0, math.radians(150.0))
    bpy.context.scene.collection.objects.link(sun)
    world = bpy.data.worlds.new('Cielo')
    sky = (np.array(pal.by_name('cielo_dia').rgb) / 255.0) ** 2.2
    world.color = tuple(sky.tolist())
    tree = cast(Any, world.node_tree)
    if tree is not None:
        background = next((n for n in tree.nodes if n.type == 'BACKGROUND'), None)
        if background is not None:
            background.inputs['Color'].default_value = (*sky.tolist(), 1.0)
            background.inputs['Strength'].default_value = 0.55
    bpy.context.scene.world = world


def build_scene(terrain_fbx: Path, decor: dict, props: dict, strokes: dict | None,
                strokes_image: Path | None, folder: Path) -> dict[str, int]:
    """Monta terreno, mar, sol, cielo y objetos; devuelve cifras de la escena. «folder» es la carpeta
    temporal donde se descargan las mallas que estén en LFS."""
    reset_scene()
    terrain = import_fbx(terrain_fbx)
    if len(terrain) != 1:
        raise RuntimeError(f'{terrain_fbx}: se esperaba 1 malla y hay {len(terrain)}')
    data = cast(bpy.types.Mesh, terrain[0].data)
    sheet = bpy.data.images.load(str(SHEET_PATH))
    data.materials.clear()
    if len(cast(Any, data.color_attributes)):
        image = bpy.data.images.load(str(strokes_image)) if strokes_image is not None else None
        if image is not None:
            cast(Any, image).colorspace_settings.name = 'Non-Color'
        attributes = cast(Any, data.color_attributes)
        data.materials.append(terrain_material(attributes[0].name, strokes, image))
    else:
        data.materials.append(palette_material(sheet))
    library = Library(palette_material(sheet), folder)
    props_count = place_props(props, library)
    decor_count = place_decor(decor, library)
    sea_mesh = bpy.data.meshes.new('Mar')
    s = 1500.0
    sea_mesh.from_pydata([(-s, -s, 0.0), (s, -s, 0.0), (s, s, 0.0), (-s, s, 0.0)], [], [(0, 1, 2, 3)])
    sea_mesh.materials.append(flat_material('M_MarPreview', load_palette().by_name('agua_somera').rgb, 0.35))
    bpy.context.scene.collection.objects.link(bpy.data.objects.new('Mar', sea_mesh))
    _sun_and_sky()
    return {'triangles': sum(p.loop_total - 2 for p in data.polygons), 'props': props_count,
            'decor': decor_count}


def render_view(view: View, out_dir: Path, percent: int) -> np.ndarray:
    scene = bpy.context.scene
    try:
        cast(Any, scene.render).engine = 'BLENDER_EEVEE'
        scene.view_settings.view_transform = 'Standard'
    except TypeError as exc:
        raise RuntimeError(f'Blender {bpy.app.version_string} sin EEVEE o sin «Standard»: {exc}') from exc
    scene.render.resolution_x, scene.render.resolution_y = VIEW_W, VIEW_H
    scene.render.resolution_percentage = percent
    scene.render.film_transparent = True
    scene.render.image_settings.file_format = 'PNG'
    scene.render.image_settings.color_mode = 'RGBA'
    eevee = scene.eevee
    if eevee is not None:
        eevee.taa_render_samples = 16
    cam_data = bpy.data.cameras.new(f'Cam_{view.key}')
    cam_data.sensor_fit = 'HORIZONTAL'
    cam_data.angle = math.radians(90.0)
    cam_data.clip_end = 3000.0
    cam = bpy.data.objects.new(f'Cam_{view.key}', cam_data)
    scene.collection.objects.link(cam)
    eye, target = Vector(to_blender(*view.eye)), Vector(to_blender(*view.target))
    cam.location = eye
    cam.rotation_euler = (target - eye).to_track_quat('-Z', 'Y').to_euler()
    scene.camera = cam
    path = out_dir / f'{view.key}.png'
    scene.render.filepath = str(path)
    bpy.ops.render.render(write_still=True)
    return load_rgba(path)


def fit(image: np.ndarray, size: tuple[int, int]) -> np.ndarray:
    w, h = size
    ys = (np.arange(h) * image.shape[0] // h).clip(0, image.shape[0] - 1)
    xs = (np.arange(w) * image.shape[1] // w).clip(0, image.shape[1] - 1)
    return image[ys][:, xs]


def compose(columns: list[tuple[str, dict[str, np.ndarray]]], keys: list[str]) -> np.ndarray:
    pal = load_palette()
    paper, ink = pal.by_name('papel').rgb, pal.by_name('tinta').rgb
    width = len(columns) * (VIEW_W + PAD) + PAD
    height = LABEL_H + PAD + len(keys) * (VIEW_H + LABEL_H + PAD)
    canvas = raster.new_canvas(width, height, paper)
    for c, (label, _) in enumerate(columns):
        canvas = raster.draw_text(canvas, label.upper(), PAD + c * (VIEW_W + PAD), PAD, SCALE, ink)
    y = LABEL_H + PAD
    for key in keys:
        canvas = raster.draw_text(canvas, key.upper(), PAD, y + 6, SCALE, ink)
        y += LABEL_H
        for c, (_, panels) in enumerate(columns):
            canvas = raster.paste(canvas, fit(panels[key], (VIEW_W, VIEW_H)), PAD + c * (VIEW_W + PAD), y)
        y += VIEW_H + PAD
    return canvas


def render_column(revision: str, layout: Layout, tmp: Path, percent: int, keys: list[str]) -> dict[str, np.ndarray]:
    fbx = git_file(revision, TERRAIN_FBX, tmp)
    decor = json.loads(git_file(revision, DECOR, tmp).read_text(encoding='utf-8'))
    props = json.loads(git_file(revision, PROPS, tmp).read_text(encoding='utf-8'))
    strokes = json.loads(STROKES_JSON.read_text(encoding='utf-8'))
    image = STROKES_PNG if revision == '.' and STROKES_PNG.is_file() else None
    info = build_scene(fbx, decor, props, strokes if image is not None else None, image, tmp)
    print(f'[preview] {revision}: {info}')
    sky = load_palette().by_name('cielo_dia').rgb
    shots = tmp / revision.replace('.', 'wt')
    shots.mkdir(exist_ok=True)
    return {v.key: over_background(render_view(v, shots, percent), sky) for v in views(layout) if v.key in keys}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog='island-preview', description='Lámina antes/después del terreno.')
    parser.add_argument('--out', type=Path, default=OUT)
    parser.add_argument('--column', action='append', default=None, help='ETIQUETA=REVISIÓN («.» = árbol)')
    parser.add_argument('--view', action='append', default=None, help='cala, palmar, faro, suelo o islote (por defecto, todas)')
    parser.add_argument('--percent', type=int, default=100)
    args = parser.parse_args(argv)
    layout = load_layout()
    keys = args.view or [v.key for v in views(layout)]
    specs = [c.split('=', 1) for c in (args.column or DEFAULT_COLUMNS)]
    if any(len(s) != 2 for s in specs):
        print('[preview] ERROR: cada --column es ETIQUETA=REVISIÓN')
        return 2
    if not STROKES_PNG.is_file():
        print(f'[preview] AVISO: falta {STROKES_PNG} (cd Tools/Textures; uv run textures-strokes): sin trazos')
    with tempfile.TemporaryDirectory(prefix='island_preview_') as tmp:
        columns = [(label, render_column(rev, layout, Path(tmp), args.percent, keys)) for label, rev in specs]
    raster.write_png(args.out, compose(columns, keys))
    print(f'[preview] escrito {args.out}')
    return 0


if __name__ == '__main__':  # pragma: no cover
    raise SystemExit(main())
