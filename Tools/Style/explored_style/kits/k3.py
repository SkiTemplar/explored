"""Kit K3 Objetos de Baliza Siete (GDD v3 §7, §8 y §11.1).

Genera las 15 mallas de objeto (más las variantes coco vacío y pez asado) y
la fauna estática (pez y gaviota, animadas por código en Unreal), exporta
`Art/Export/K3_Objetos/SM_<Nombre>_01.fbx` con `manifest.json` y renderiza
la lámina de revisión `docs/v3/laminas/k3.png`.

Todo es determinista: volver a ejecutar regenera la misma geometría.
Uso: `uv run style-kit-k3 [--out DIR] [--lamina RUTA] [--sin-lamina] [--engine ...] [--percent N]`.
"""

from __future__ import annotations

import argparse
import json
from collections.abc import Callable
from dataclasses import dataclass
from pathlib import Path

import bpy

from explored_style import meshkit
from explored_style.check_style import parse_budget
from explored_style.kits import k3_fauna as fauna
from explored_style.kits import k3_largos as largos
from explored_style.kits import k3_objetos as objetos
from explored_style.meshkit import MeshBuilder
from explored_style.palette import REPO_ROOT, load_palette

EXPORT_DIR = REPO_ROOT / 'Art' / 'Export' / 'K3_Objetos'
SUFFIX = '_01'


@dataclass(frozen=True)
class KitSpec:
    name: str        # SM_<Nombre>, sin sufijo
    budget: str      # tipo de check-style o número de triángulos
    build: Callable[[], MeshBuilder]
    role: str        # objeto del GDD §7 o fauna del §8

    @property
    def asset(self) -> str:
        return f'{self.name}{SUFFIX}'

    @property
    def max_tris(self) -> int:
        return parse_budget(self.budget)


K3: tuple[KitSpec, ...] = (
    KitSpec('SM_Cuchillo', 'objeto_mano', objetos.build_cuchillo, 'cuchillo'),
    KitSpec('SM_Cantimplora', 'objeto_mano', objetos.build_cantimplora, 'cantimplora'),
    KitSpec('SM_Barometro', 'objeto_mano', objetos.build_barometro, 'barometro'),
    KitSpec('SM_Coco', 'objeto_mano', objetos.build_coco, 'coco'),
    KitSpec('SM_CocoVacio', 'objeto_mano', objetos.build_coco_vacio, 'coco_vacio'),
    KitSpec('SM_Lena', 'objeto_mano', objetos.build_lena, 'lena'),
    KitSpec('SM_Fibra', 'objeto_mano', objetos.build_fibra, 'fibra'),
    KitSpec('SM_Cuerda', 'objeto_mano', objetos.build_cuerda, 'cuerda'),
    KitSpec('SM_CanaBambu', 'objeto_mano', largos.build_cana_bambu, 'cana_bambu'),
    KitSpec('SM_CanaPescar', 'objeto_mano', largos.build_cana_pescar, 'cana_pescar'),
    KitSpec('SM_Tronco', 'estructura', largos.build_tronco, 'tronco'),
    KitSpec('SM_Remo', 'estructura', largos.build_remo, 'remo'),
    KitSpec('SM_Pez', 'objeto_mano', fauna.build_pez, 'pez_crudo'),
    KitSpec('SM_PezAsado', 'objeto_mano', fauna.build_pez_asado, 'pez_asado'),
    KitSpec('SM_TarroAceite', 'objeto_mano', objetos.build_tarro_aceite, 'tarro_aceite'),
    KitSpec('SM_Mecha', 'objeto_mano', objetos.build_mecha, 'mecha'),
    KitSpec('SM_Lente', 'estructura', largos.build_lente, 'lente'),
    KitSpec('SM_FaunaPez', '200', fauna.build_fauna_pez, 'fauna: pez'),
    KitSpec('SM_FaunaGaviota', '400', fauna.build_fauna_gaviota, 'fauna: gaviota'),
)


def spec_named(name: str) -> KitSpec:
    for spec in K3:
        if spec.name == name:
            return spec
    raise KeyError(name)


def generate(spec: KitSpec) -> bpy.types.Object:
    return meshkit.build_object(spec.asset, spec.build())


def manifest_entry(spec: KitSpec, obj: bpy.types.Object, fbx_name: str) -> dict:
    pal = load_palette()
    return {
        'malla': spec.asset, 'fbx': fbx_name, 'uso': spec.role,
        'presupuesto': spec.budget, 'tope_tris': spec.max_tris,
        'tris': meshkit.triangle_count(obj),
        'dimensiones_m': [round(d, 3) for d in obj.dimensions],
        'colores': sorted({pal.colors[i].name for i in meshkit.face_colors(obj)}),
        'origen': 'generado por Tools/Style/explored_style/kits/k3.py (sin fuentes externas)',
        'licencia': 'propia del proyecto Explored',
    }


def generate_all(out_dir: Path) -> list[dict]:
    """Genera y exporta todo el kit; devuelve las entradas del manifiesto."""
    out_dir.mkdir(parents=True, exist_ok=True)
    entries = []
    for spec in K3:
        meshkit.reset_scene()
        obj = generate(spec)
        path = out_dir / f'{spec.asset}.fbx'
        meshkit.export_fbx(obj, str(path))
        entries.append(manifest_entry(spec, obj, path.name))
        print(f'[k3] {spec.asset}: {entries[-1]["tris"]}/{spec.max_tris} tris -> {path}')
    manifest = {'kit': 'K3 Objetos', 'generador': 'uv run style-kit-k3', 'mallas': entries}
    (out_dir / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n',
                                           encoding='utf-8')
    return entries


def main(argv: list[str] | None = None) -> int:
    from explored_style.kits import k3_lamina  # k3_lamina importa este módulo: evita el ciclo

    parser = argparse.ArgumentParser(prog='style-kit-k3', description='Genera el kit K3 y su lámina.')
    parser.add_argument('--out', type=Path, default=EXPORT_DIR)
    parser.add_argument('--lamina', type=Path, default=k3_lamina.OUT_PATH)
    parser.add_argument('--sin-lamina', action='store_true', help='solo exporta los FBX y el manifiesto')
    parser.add_argument('--engine', default='BLENDER_EEVEE', help='BLENDER_EEVEE o BLENDER_WORKBENCH')
    parser.add_argument('--percent', type=int, default=100, help='porcentaje de resolución (pruebas)')
    args = parser.parse_args(argv)
    generate_all(args.out)
    if not args.sin_lamina:
        path = k3_lamina.make_sheet(args.lamina, args.out, engine=args.engine, percent=args.percent)
        print(f'[k3] lámina: {path}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
