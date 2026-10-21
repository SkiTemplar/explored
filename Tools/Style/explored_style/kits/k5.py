"""Kit K5 Cielo de Baliza Siete: 5 nubes de cúmulo y los discos del sol y la luna.

Genera `SM_Nube_A_01` … `SM_Nube_E_01` (3 a 7 lóbulos, 20 a 60 m de ancho,
≤ 400 tris, pivote en el centro de la base) y `SM_DiscoSol_01` y
`SM_DiscoLuna_01` (discos planos de Ø 1 m en el plano XY, ≤ 64 tris, pivote
en el centro), exporta `Art/Export/K5_Cielo/*.fbx` con `manifest.json` y
renderiza la lámina `docs/v3/laminas/k5.png`.

Todo es determinista: volver a ejecutar regenera la misma geometría.
Uso: `uv run style-kit-k5 [--out DIR] [--lamina RUTA] [--sin-lamina] [--engine ...] [--percent N]`.
"""

from __future__ import annotations

import argparse
import json
from collections.abc import Callable
from dataclasses import dataclass
from functools import partial
from pathlib import Path

import bpy

from explored_style import meshkit
from explored_style.check_style import parse_budget
from explored_style.kits import k5_formas as formas
from explored_style.meshkit import MeshBuilder
from explored_style.palette import REPO_ROOT, load_palette

EXPORT_DIR = REPO_ROOT / 'Art' / 'Export' / 'K5_Cielo'
SUFFIX = '_01'
NUBE_BUDGET = '400'
DISCO_BUDGET = '64'


@dataclass(frozen=True)
class KitSpec:
    name: str        # SM_<Nombre>, sin sufijo
    budget: str      # número de triángulos (check-style --budget)
    build: Callable[[], MeshBuilder]
    role: str

    @property
    def asset(self) -> str:
        return f'{self.name}{SUFFIX}'

    @property
    def max_tris(self) -> int:
        return parse_budget(self.budget)

    @property
    def is_cloud(self) -> bool:
        return self.name in formas.NUBES


K5: tuple[KitSpec, ...] = (
    *(KitSpec(name, NUBE_BUDGET, partial(formas.cloud_builder, name),
              f'nube de {len(shape.lobes)} lóbulos, {shape.width:.0f} m de ancho')
      for name, shape in formas.NUBES.items()),
    KitSpec('SM_DiscoSol', DISCO_BUDGET, formas.build_sol, 'disco del sol, Ø 1 m, normal +Z'),
    KitSpec('SM_DiscoLuna', DISCO_BUDGET, formas.build_luna, 'disco de la luna, Ø 1 m, normal +Z'),
)

NOTAS = {
    'pivote': 'nubes: centro de la base (z = 0 es la base plana); discos: centro del disco (z = 0)',
    'discos': 'en el plano XY con normal +Z: el motor orienta la Z local hacia la cámara y escala '
              'Ø = 2 · distancia · tan(diámetro angular / 2); sin sombra proyectada y material sin sombreado. '
              'La Y local es «arriba» en pantalla: la cara iluminada de la luna queda arriba a la izquierda',
    'nubes': 'MI_ToonNube recomendado: bandas suaves (0,86 / 0,93 / 1,0) para que el gris de la base no '
             'parezca roca; sin sombra proyectada o con sombra suave',
    'propuesta_color': formas.NUBE_PROPUESTA,
}


def spec_named(name: str) -> KitSpec:
    for spec in K5:
        if spec.name == name:
            return spec
    raise KeyError(name)


def generate(spec: KitSpec) -> bpy.types.Object:
    return meshkit.build_object(spec.asset, spec.build())


def manifest_entry(spec: KitSpec, obj: bpy.types.Object, fbx_name: str) -> dict:
    pal = load_palette()
    entry = {
        'malla': spec.asset, 'fbx': fbx_name, 'uso': spec.role,
        'presupuesto': spec.budget, 'tope_tris': spec.max_tris,
        'tris': meshkit.triangle_count(obj),
        'dimensiones_m': [round(d, 3) for d in obj.dimensions],
        'colores': sorted({pal.colors[i].name for i in meshkit.face_colors(obj)}),
        'origen': 'generado por Tools/Style/explored_style/kits/k5.py (sin fuentes externas)',
        'licencia': 'propia del proyecto Explored',
    }
    if spec.is_cloud:
        entry['lobulos'] = len(formas.NUBES[spec.name].lobes)
    return entry


def generate_all(out_dir: Path) -> list[dict]:
    """Genera y exporta todo el kit; devuelve las entradas del manifiesto."""
    out_dir.mkdir(parents=True, exist_ok=True)
    entries = []
    for spec in K5:
        meshkit.reset_scene()
        obj = generate(spec)
        path = out_dir / f'{spec.asset}.fbx'
        meshkit.export_fbx(obj, str(path))
        entries.append(manifest_entry(spec, obj, path.name))
        print(f'[k5] {spec.asset}: {entries[-1]["tris"]}/{spec.max_tris} tris -> {path}')
    manifest = {'kit': 'K5 Cielo', 'generador': 'uv run style-kit-k5', 'notas': NOTAS, 'mallas': entries}
    (out_dir / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n',
                                           encoding='utf-8')
    return entries


def main(argv: list[str] | None = None) -> int:
    from explored_style.kits import k5_lamina  # k5_lamina importa este módulo: evita el ciclo

    parser = argparse.ArgumentParser(prog='style-kit-k5', description='Genera el kit K5 Cielo y su lámina.')
    parser.add_argument('--out', type=Path, default=EXPORT_DIR)
    parser.add_argument('--lamina', type=Path, default=k5_lamina.OUT_PATH)
    parser.add_argument('--sin-lamina', action='store_true', help='solo exporta los FBX y el manifiesto')
    parser.add_argument('--engine', default='BLENDER_EEVEE', help='BLENDER_EEVEE o BLENDER_WORKBENCH')
    parser.add_argument('--percent', type=int, default=100, help='porcentaje de resolución (pruebas)')
    args = parser.parse_args(argv)
    generate_all(args.out)
    if not args.sin_lamina:
        path = k5_lamina.make_sheet(args.lamina, args.out, engine=args.engine, percent=args.percent)
        print(f'[k5] lámina: {path}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
