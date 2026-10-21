"""Validación de Content/Data/island_props.json contra el GDD v3 (§5.3, §6, §11.1).

`check_props` devuelve la lista de errores (vacía = válido). Comprueba ids
únicos, tipos y anclas definidos, rutas de malla definitivas del GDD §11.1,
índices de paleta, las cantidades de la lista cerrada de recursos y que
cada objeto de juego sin ancla quede en tierra firme.
"""

from __future__ import annotations

import json
import re
import sys
from collections import Counter
from pathlib import Path

from explored_island.layout import REPO_ROOT, Layout, height_at, load_layout, zone_at

PROPS_JSON = REPO_ROOT / 'Content' / 'Data' / 'island_props.json'

# GDD §11.1: kits y mallas (sin el sufijo _01).
GDD_MESHES = {
    'K1_Naturaleza': {'SM_PalmA', 'SM_PalmB', 'SM_Bamboo', 'SM_Shrub', 'SM_GrassClump', 'SM_RockBig',
                      'SM_RockSmall', 'SM_CliffRock'},
    'K2_Estructuras': {'SM_FaroTorre', 'SM_FaroLinterna', 'SM_FaroCasa', 'SM_CajaSuministros',
                       'SM_CargueroCasco', 'SM_CargueroPuente', 'SM_RefugioPalma', 'SM_BarrilLluvia',
                       'SM_Hoguera', 'SM_Balsa'},
    'K3_Objetos': {'SM_Cuchillo', 'SM_Cantimplora', 'SM_Barometro', 'SM_Coco', 'SM_Lena', 'SM_Fibra',
                   'SM_Cuerda', 'SM_CanaBambu', 'SM_CanaPescar', 'SM_Tronco', 'SM_Remo', 'SM_Pez',
                   'SM_TarroAceite', 'SM_Mecha', 'SM_Lente'},
}
MESH_RE = re.compile(r'^/Game/Art/(K[123]_\w+)/(SM_\w+)_01$')
SHAPES = {'cube', 'cylinder', 'sphere', 'cone'}
COLLISIONS = {'solid', 'query', 'none'}

# GDD §5.3 y §6: número exacto de colocaciones por tipo.
EXPECTED_COUNTS = {
    'palma': 8, 'lena': 10, 'fibra': 15, 'cana_bambu': 3, 'tronco': 7, 'pagina': 8, 'lente': 1,
    'punto_pesca': 3, 'shelter_volume': 2, 'refugio_palma': 1, 'refugio_faro': 1, 'barril_lluvia': 1,
    'caja_suministros': 1, 'hoguera_fija': 1, 'faro_linterna': 1, 'balsa_sitio': 1, 'faro_torre': 1,
    'faro_casa': 1, 'carguero_casco': 1, 'carguero_puente': 1, 'caja_mapas': 1,
}
# Totales de recurso (campo «cocos» o «amount»).
EXPECTED_TOTALS = {('palma', 'cocos'): 16, ('lena', 'amount'): 20, ('fibra', 'amount'): 15}
# Zona de cada página del cuaderno (GDD §2.1); None = anclada a una estructura.
PAGE_ZONES = {1: 'colina_faro', 2: 'rocas_pescador', 3: 'palmar', 4: 'bosque_bambu', 5: 'playa_norte',
              6: None, 7: 'colina_faro', 8: None}
DRY_MIN = 0.15


def load_props(path: Path = PROPS_JSON) -> dict:
    return json.loads(path.read_text(encoding='utf-8'))


def _check_types(types: dict) -> list[str]:
    errors = []
    for name, t in types.items():
        mesh = t.get('mesh', '')
        if mesh:
            m = MESH_RE.match(mesh)
            if not m or m.group(2) not in GDD_MESHES.get(m.group(1), set()):
                errors.append(f'tipo {name}: malla fuera del GDD §11.1: {mesh}')
        if t.get('shape') not in SHAPES:
            errors.append(f'tipo {name}: primitiva desconocida {t.get("shape")}')
        size = t.get('size', [])
        if len(size) != 3 or min(size) <= 0:
            errors.append(f'tipo {name}: tamaño inválido {size}')
        if not 0 <= int(t.get('color', -1)) < 32:
            errors.append(f'tipo {name}: color fuera de la paleta')
        if t.get('collision') not in COLLISIONS:
            errors.append(f'tipo {name}: colisión desconocida {t.get("collision")}')
    return errors


def _check_placement(layout: Layout, types: dict, props: list[dict]) -> list[str]:
    errors = []
    seen: set[str] = set()
    for p in props:
        pid = p.get('id', '')
        if not pid or pid in seen:
            errors.append(f'id vacío o repetido: {pid!r}')
        seen.add(pid)
        if p.get('type') not in types:
            errors.append(f'{pid}: tipo sin definir {p.get("type")}')
            continue
        anchor = p.get('anchor')
        if anchor and anchor not in seen:
            errors.append(f'{pid}: ancla {anchor} sin definir antes')
        if p.get('mesh') and not MESH_RE.match(p['mesh']):
            errors.append(f'{pid}: malla de sustitución inválida {p["mesh"]}')
        if types[p['type']].get('gameplay') and not anchor:
            ground = float(height_at(layout, *p['at']))
            if ground < DRY_MIN:
                errors.append(f'{pid}: en el agua ({ground:.2f} m)')
    return errors


def _check_counts(props: list[dict]) -> list[str]:
    errors = []
    counts = Counter(p.get('type') for p in props)
    for name, expected in EXPECTED_COUNTS.items():
        if counts[name] != expected:
            errors.append(f'{name}: {counts[name]} colocaciones, el GDD pide {expected}')
    for (name, field), expected in EXPECTED_TOTALS.items():
        total = sum(int(p.get(field, 0)) for p in props if p.get('type') == name)
        if total != expected:
            errors.append(f'{name}: {field} suma {total}, el GDD pide {expected}')
    shelters = [p for p in props if p.get('type') == 'shelter_volume']
    if not all(p.get('respawn') and len(p.get('extent', [])) == 3 for p in shelters):
        errors.append('cada shelter_volume necesita extent [x, y, z] y respawn')
    return errors


def _check_pages(layout: Layout, props: list[dict]) -> list[str]:
    errors = []
    pages = {int(p.get('page', 0)): p for p in props if p.get('type') == 'pagina'}
    if sorted(pages) != list(range(1, 9)):
        errors.append(f'páginas: se esperaban 1-8 y hay {sorted(pages)}')
        return errors
    for n, zone in PAGE_ZONES.items():
        p = pages[n]
        if zone is None:
            if not p.get('anchor'):
                errors.append(f'página {n}: debería ir anclada a su estructura')
        elif zone_at(layout, *p['at']) != zone:
            errors.append(f'página {n}: fuera de {zone}')
    return errors


def check_props(data: dict, layout: Layout) -> list[str]:
    types = data.get('types', {})
    props = data.get('props', [])
    return (_check_types(types) + _check_placement(layout, types, props) + _check_counts(props)
            + _check_pages(layout, props))


def main(argv: list[str] | None = None) -> int:
    del argv
    errors = check_props(load_props(), load_layout())
    for e in errors:
        print(e, file=sys.stderr)
    print(f'island_props.json: {len(errors)} errores')
    return 1 if errors else 0
