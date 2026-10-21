"""Kit K2 · Estructuras (GDD v3 §11.1) más SM_ManoIria.

    uv run style-kit-k2 [--out Art/Export/K2_Estructuras] [--sin-lamina]
                        [--lamina docs/v3/laminas/k2.png] [--engine BLENDER_EEVEE] [--percent 100]

Genera las 11 mallas por script (deterministas, sin fuentes externas), las
exporta a FBX `SM_<Nombre>_01.fbx`, escribe `manifest.json` (tris, medidas,
colores, presupuesto, pivote y ensamblajes) y renderiza la lámina de revisión
importando los FBX entregados. Las formas viven en `k2_faro`, `k2_carguero`,
`k2_props` y `k2_manos`; la lámina, en `k2_lamina`.
"""

from __future__ import annotations

import argparse
import json
from collections.abc import Callable
from dataclasses import dataclass
from pathlib import Path

import bpy

from explored_style import check_style, meshkit
from explored_style.kits import k2_carguero, k2_faro, k2_manos, k2_props
from explored_style.palette import REPO_ROOT, load_palette

EXPORT_DIR = REPO_ROOT / 'Art' / 'Export' / 'K2_Estructuras'
SOURCE = 'generado por Tools/Style/explored_style/kits/k2*.py (sin fuentes externas)'


@dataclass(frozen=True)
class KitSpec:
    name: str
    budget: str
    build: Callable[[], meshkit.MeshBuilder]
    note: str

    @property
    def object_name(self) -> str:
        return f'{self.name}_01'

    @property
    def fbx(self) -> str:
        return f'{self.object_name}.fbx'

    @property
    def pivot(self) -> str:
        return check_style.PIVOT_RULES.get(self.budget, 'base')


SPECS: tuple[KitSpec, ...] = (
    KitSpec('SM_FaroTorre', 'faro', k2_faro.build_tower, 'torre de 12 m; la linterna se apoya en z = 11 m'),
    KitSpec('SM_FaroLinterna', 'faro', k2_faro.build_lantern, 'puerta hacia -Y; 3 ranuras en «ranuras_m»'),
    KitSpec('SM_FaroCasa', 'faro', k2_faro.build_house, 'puerta hacia -Y'),
    KitSpec('SM_CajaSuministros', 'estructura', k2_props.build_crate, 'caja de mapas = misma malla, otra MI'),
    KitSpec('SM_CargueroCasco', 'carguero', k2_carguero.build_hull, 'proa hacia +X; escora 8° y proa 2° arriba'),
    KitSpec('SM_CargueroPuente', 'carguero', k2_carguero.build_bridge, 'recto; colocación en «ensamblaje»'),
    KitSpec('SM_RefugioPalma', 'estructura', k2_props.build_shelter, 'abierto hacia -Y'),
    KitSpec('SM_BarrilLluvia', 'estructura', k2_props.build_barrel, 'piloto F2 revisado; grifo hacia -Y'),
    KitSpec('SM_Hoguera', 'estructura', k2_props.build_campfire, 'sin fuego: lo pone el motor'),
    KitSpec('SM_Balsa', 'estructura', k2_props.build_raft, 'eslora en X'),
    KitSpec('SM_ManoIria', 'manos', k2_manos.build_hands, 'pivote en el punto medio de los codos; +X adelante'),
)


def spec_by_name(name: str) -> KitSpec:
    for spec in SPECS:
        if spec.name == name:
            return spec
    raise KeyError(f'malla desconocida en K2: {name}')


def generate(spec: KitSpec) -> bpy.types.Object:
    return meshkit.build_object(spec.object_name, spec.build(), base_pivot=spec.pivot == 'base')


def assemblies() -> dict[str, dict]:
    """Colocación de las piezas que van unas sobre otras (espacio local del padre)."""
    return {
        'SM_FaroLinterna_01': {'padre': 'SM_FaroTorre_01', 'ubicacion_m': [0.0, 0.0, k2_faro.DECK_Z],
                               'rotacion_grados': [0.0, 0.0, 0.0],
                               'ranuras_m': {k: list(v) for k, v in k2_faro.LANTERN_SLOTS.items()}},
        'SM_CargueroPuente_01': {'padre': 'SM_CargueroCasco_01', **k2_carguero.bridge_mount()},
    }


def _entry(spec: KitSpec, obj: bpy.types.Object, names: list[str]) -> dict:
    entry = {
        'malla': spec.object_name, 'fbx': spec.fbx, 'presupuesto': spec.budget,
        'tope_tris': check_style.BUDGETS[spec.budget], 'tris': meshkit.triangle_count(obj),
        'dimensiones_m': [round(d, 3) for d in obj.dimensions],
        'colores': sorted({names[i] for i in meshkit.face_colors(obj)}),
        'pivote': spec.pivot, 'nota': spec.note, 'origen': SOURCE, 'licencia': 'propia del proyecto Explored',
    }
    if spec.name == 'SM_ManoIria':
        right, left = k2_manos.per_hand_triangles(spec.build())
        entry['tris_por_mano'] = {'derecha': right, 'izquierda': left, 'tope': k2_manos.HAND_BUDGET}
    return entry


def generate_all(out_dir: Path) -> list[dict]:
    """Genera y exporta las 11 mallas; escribe y devuelve el manifiesto."""
    out_dir.mkdir(parents=True, exist_ok=True)
    names = [c.name for c in load_palette().colors]
    entries = []
    for spec in SPECS:
        meshkit.reset_scene()
        obj = generate(spec)
        meshkit.export_fbx(obj, str(out_dir / spec.fbx))
        entries.append(_entry(spec, obj, names))
        print(f'[k2] {spec.object_name}: {entries[-1]["tris"]}/{entries[-1]["tope_tris"]} tris -> {spec.fbx}')
    manifest = {'kit': 'K2 Estructuras', 'generador': 'uv run style-kit-k2', 'unidades': 'metros, Z arriba',
                'mallas': entries, 'ensamblaje': assemblies()}
    (out_dir / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n',
                                           encoding='utf-8')
    return entries


def main(argv: list[str] | None = None) -> int:
    from explored_style.kits import k2_lamina

    parser = argparse.ArgumentParser(prog='style-kit-k2', description='Genera el kit K2 y su lámina.')
    parser.add_argument('--out', type=Path, default=EXPORT_DIR)
    parser.add_argument('--sin-lamina', action='store_true', help='solo FBX y manifiesto')
    parser.add_argument('--lamina', type=Path, default=k2_lamina.OUT_PATH)
    parser.add_argument('--engine', default='BLENDER_EEVEE', help='BLENDER_EEVEE o BLENDER_WORKBENCH')
    parser.add_argument('--percent', type=int, default=100, help='porcentaje de resolución (pruebas)')
    args = parser.parse_args(argv)
    generate_all(args.out)
    if not args.sin_lamina:
        path = k2_lamina.make_sheet(args.lamina, args.out, engine=args.engine, percent=args.percent)
        print(f'[k2] lámina {path}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
