"""Kit K6 Césped de Baliza Siete: mallas para el sistema de hierba instanciada.

Genera `SM_GrassTuft_A/B/C_01` (matas de 5-9 hojas, 0,25-0,45 m, 16-40 tris),
`SM_GrassTall_01` (hierba alta con espigas, 0,6-0,8 m, ≤ 60 tris),
`SM_FlowerA_01` y `SM_FlowerB_01` (flores blanca y lila, ≤ 50 tris),
`SM_Fern_01` (helecho, ≤ 120 tris) y `SM_Clover_01` (tréboles a ras de suelo,
≤ 40 tris); exporta `Art/Export/K6_Cesped/*.fbx` con `manifest.json` y
renderiza la lámina `docs/v3/laminas/k6-cesped.png` (Workbench).

Mismo método que K1-K5 (Tools/Style): un material `M_Toon`, cada cara con su
UV en una celda de `T_Paleta`, normales planas y pivote en la base (z = 0),
metros. Además, color por vértice `Col` (esquina): RGB = color de paleta de la
cara (sRGB exacto) y alfa = peso de viento (0 en la base, 1 en la punta).
M_Toon no tiene canal de viento por vértice (su WPO usa la altura sobre el
pivote dividida entre 7,5 m), así que el peso va en el alfa: ver NOTAS.

Determinista: volver a ejecutar regenera los mismos FBX.
Uso (desde Tools/Blender): `uv run python -m kits.k6_cesped [--out DIR] [--lamina RUTA] [--sin-lamina]`.
"""

from __future__ import annotations

import argparse
import json
from collections.abc import Callable
from dataclasses import dataclass
from functools import partial
from pathlib import Path
from typing import Any, cast

import bpy

from explored_style import meshkit
from explored_style.palette import REPO_ROOT, load_palette

from kits import k6_formas as formas

EXPORT_DIR = REPO_ROOT / 'Art' / 'Export' / 'K6_Cesped'
SUFFIX = '_01'
COLOR_ATTR = 'Col'
GREENS = frozenset(c.name for c in load_palette().colors if c.family == 'verde')


@dataclass(frozen=True)
class KitSpec:
    """Malla del kit con sus criterios de aceptación (los comprueba tests/test_k6.py)."""

    name: str                           # SM_<Nombre>, sin sufijo
    build: Callable[[], formas.WindBuilder]
    role: str
    tris: tuple[int, int]               # mínimo y máximo de triángulos
    height: tuple[float, float]         # m (dimensión Z)
    width_max: float                    # m (dimensión XY mayor)
    pieces: tuple[int, int] | None = None   # hojas sueltas (islas), si aplica
    tip_weight: float = 1.0

    @property
    def asset(self) -> str:
        return f'{self.name}{SUFFIX}'


def _tuft(name: str, role: str) -> KitSpec:
    spec = formas.TUFTS[name]
    return KitSpec(name, partial(formas.build_tuft, spec), role, (16, 40), (0.25, 0.45), 0.6, (5, 9))


K6: tuple[KitSpec, ...] = (
    _tuft('SM_GrassTuft_A', 'mata media de 7 hojas, tres verdes'),
    _tuft('SM_GrassTuft_B', 'mata baja y abierta de 9 hojas, al sol'),
    _tuft('SM_GrassTuft_C', 'mata alta de 5 hojas con la punta doblada'),
    KitSpec('SM_GrassTall', formas.build_tall, 'hierba alta de 7 hojas y 3 espigas', (20, 60), (0.6, 0.8), 0.8),
    KitSpec('SM_FlowerA', partial(formas.build_flower, formas.FLOWERS['SM_FlowerA']),
            'dos margaritas blancas de botón amarillo', (12, 50), (0.2, 0.42), 0.45),
    KitSpec('SM_FlowerB', partial(formas.build_flower, formas.FLOWERS['SM_FlowerB']),
            'tres florecillas lilas de 4 pétalos', (12, 50), (0.2, 0.42), 0.45),
    KitSpec('SM_Fern', formas.build_fern, 'helecho de 9 frondas en arco', (40, 120), (0.25, 0.55), 1.0),
    KitSpec('SM_Clover', formas.build_clover, '4 tréboles y una cabezuela blanca a ras de suelo', (16, 40),
            (0.03, 0.12), 0.4, tip_weight=formas.CLOVER_TIP_WEIGHT),
)

NOTAS = {
    'pivote': 'centro de la base (z = 0); +Z arriba; metros (1 m = 100 uu tras import_uniform_scale = 100)',
    'color': 'UV 0 en una celda de T_Paleta por cara (como K1-K5) y el mismo color en el RGB del color por '
             'vértice «Col» (sRGB exacto de paleta.json)',
    'viento': 'alfa del color por vértice «Col»: 0 en la base y 1 en las puntas (tréboles: 0,35 como máximo, '
              'van pegados al suelo). M_Toon no tiene canal de viento por vértice: su WPO usa la altura sobre '
              'el pivote / 750 uu, que en una mata de 30 cm da 0,002. El material del césped debe multiplicar '
              'su WPO por VertexColor.A',
    'material': 'tarjetas de una cara: hace falta una variante de M_Toon de dos caras (two_sided) con el WPO '
                'por VertexColor.A, p. ej. M_ToonCesped; sin dos caras, la mitad de las hojas desaparece',
    'importacion': 'FBX con vertex_color_import_option = REPLACE, normales importadas, sin colisión, sin '
                   'lightmap UV, sin Nanite y sin LOD (≤ 120 tris); 8 mallas y 1 material: 8 draw calls por '
                   'grupo de instancias (HISM/foliage)',
}


def spec_named(name: str) -> KitSpec:
    for spec in K6:
        if spec.name == name:
            return spec
    raise KeyError(name)


def _srgb(hex_value: str) -> tuple[float, float, float]:
    h = hex_value.lstrip('#')
    return int(h[0:2], 16) / 255.0, int(h[2:4], 16) / 255.0, int(h[4:6], 16) / 255.0


def paint_vertex_colors(obj: bpy.types.Object, builder: formas.WindBuilder) -> None:
    """Color por vértice «Col» en las esquinas: RGB de la celda de la cara y
    alfa = peso de viento del vértice."""
    mesh = meshkit.mesh_of(obj)
    pal = load_palette()
    attr = mesh.color_attributes.new(name=COLOR_ATTR, type='BYTE_COLOR', domain='CORNER')
    data = cast(Any, attr.data)
    for poly, color in zip(mesh.polygons, builder.colors, strict=True):
        r, g, b = _srgb(pal.colors[color].hex)
        for loop_index in poly.loop_indices:
            weight = builder.weights[mesh.loops[loop_index].vertex_index]
            data[loop_index].color_srgb = (r, g, b, weight)
    mesh.color_attributes.active_color_name = COLOR_ATTR
    mesh.color_attributes.render_color_index = list(mesh.color_attributes).index(attr)


def generate(spec: KitSpec) -> bpy.types.Object:
    builder = spec.build()
    if len(builder.weights) != len(builder.verts):
        raise ValueError(f'{spec.asset}: falta el peso de viento de algún vértice')
    obj = meshkit.build_object(spec.asset, builder)
    paint_vertex_colors(obj, builder)
    return obj


def _enum_ids(operator: Any, prop: str) -> list[str]:
    return [i.identifier for i in operator.get_rna_type().properties[prop].enum_items]


def export_fbx(obj: bpy.types.Object, filepath: Path) -> Path:
    """meshkit.export_fbx (metros, -Z adelante / Y arriba, caras planas,
    triangulado) más el color por vértice en sRGB."""
    exporter = cast(Any, bpy.ops.export_scene.fbx)
    if 'SRGB' not in _enum_ids(exporter, 'colors_type'):
        raise RuntimeError(f'el exportador FBX de Blender {bpy.app.version_string} no exporta color sRGB')
    for other in bpy.context.selected_objects or []:
        other.select_set(False)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    exporter(
        filepath=str(filepath), check_existing=False, use_selection=True, global_scale=1.0,
        apply_unit_scale=True, apply_scale_options='FBX_SCALE_ALL', axis_forward='-Z', axis_up='Y',
        object_types={'MESH'}, use_mesh_modifiers=True, mesh_smooth_type='FACE', use_triangles=True,
        use_tspace=False, bake_anim=False, path_mode='STRIP', embed_textures=False,
        colors_type='SRGB', prioritize_active_color=True,
    )
    return filepath


def manifest_entry(spec: KitSpec, obj: bpy.types.Object, fbx_name: str) -> dict:
    pal = load_palette()
    entry = {
        'malla': spec.asset, 'fbx': fbx_name, 'uso': spec.role,
        'presupuesto': 'vegetacion', 'tope_tris': spec.tris[1],
        'tris': meshkit.triangle_count(obj),
        'dimensiones_m': [round(d, 3) for d in obj.dimensions],
        'colores': sorted({pal.colors[i].name for i in meshkit.face_colors(obj)}),
        'viento': {'canal': f'{COLOR_ATTR}.A', 'base': 0.0, 'punta': spec.tip_weight},
        'colision': 'ninguna',
        'material': 'M_ToonCesped (M_Toon de dos caras con WPO por VertexColor.A)',
        'origen': 'generado por Tools/Blender/kits/k6_cesped.py (sin fuentes externas)',
        'licencia': 'propia del proyecto Explored',
    }
    if spec.pieces is not None:
        entry['hojas'] = meshkit.island_count(obj)
    return entry


def generate_all(out_dir: Path) -> list[dict]:
    """Genera y exporta todo el kit; devuelve las entradas del manifiesto."""
    out_dir.mkdir(parents=True, exist_ok=True)
    entries = []
    for spec in K6:
        meshkit.reset_scene()
        obj = generate(spec)
        path = export_fbx(obj, out_dir / f'{spec.asset}.fbx')
        entries.append(manifest_entry(spec, obj, path.name))
        print(f'[k6] {spec.asset}: {entries[-1]["tris"]}/{spec.tris[1]} tris -> {path}')
    manifest = {'kit': 'K6 Césped', 'generador': 'uv run python -m kits.k6_cesped (Tools/Blender)',
                'notas': NOTAS, 'mallas': entries}
    (out_dir / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n',
                                           encoding='utf-8')
    return entries


def main(argv: list[str] | None = None) -> int:
    from kits import k6_lamina  # k6_lamina importa este módulo: evita el ciclo

    parser = argparse.ArgumentParser(prog='k6_cesped', description='Genera el kit K6 Césped y su lámina.')
    parser.add_argument('--out', type=Path, default=EXPORT_DIR)
    parser.add_argument('--lamina', type=Path, default=k6_lamina.OUT_PATH)
    parser.add_argument('--sin-lamina', action='store_true', help='solo exporta los FBX y el manifiesto')
    parser.add_argument('--percent', type=int, default=100, help='porcentaje de resolución (pruebas)')
    args = parser.parse_args(argv)
    generate_all(args.out)
    if not args.sin_lamina:
        path = k6_lamina.make_sheet(args.lamina, args.out, percent=args.percent)
        print(f'[k6] lámina: {path}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
