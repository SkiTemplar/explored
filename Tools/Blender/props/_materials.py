"""
_materials.py — los 7 materiales estables del kit de props narrativos.

Nombres fijos que usan todos los scripts de Tools/Blender/props/: M_Wood,
M_Metal, M_Fabric, M_Stone, M_Glass, M_Paper, M_Leaf. No son los 4 del kit
de vegetación (M_Bark/M_Leaf/M_Rock/M_Grass de common.py): se construyen
con common.get_material_ext(), que es una función nueva y aditiva (no toca
_MATERIAL_DEFS del kit de vegetación).
"""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'lib'))
import common as C  # noqa: E402

MATERIAL_DEFS = {
    'M_Wood':   dict(base_color=(0.30, 0.19, 0.11), roughness=0.85, metallic=0.0),
    'M_Metal':  dict(base_color=(0.32, 0.32, 0.34), roughness=0.40, metallic=0.85),
    'M_Fabric': dict(base_color=(0.56, 0.53, 0.41), roughness=0.90, metallic=0.0),
    'M_Stone':  dict(base_color=(0.35, 0.34, 0.32), roughness=0.88, metallic=0.0),
    'M_Glass':  dict(base_color=(0.55, 0.78, 0.62), roughness=0.05, metallic=0.0, alpha_blend=True),
    'M_Paper':  dict(base_color=(0.80, 0.74, 0.56), roughness=0.75, metallic=0.0),
    'M_Leaf':   dict(base_color=(0.17, 0.35, 0.15), roughness=0.55, metallic=0.0),
}

MATERIAL_NAMES = frozenset(MATERIAL_DEFS)


def get_material(name):
    """Crea (si falta) o devuelve uno de los 7 materiales estables del kit
    de props, usando el grafo genérico de common.get_material_ext()."""
    if name not in MATERIAL_DEFS:
        raise ValueError(f'Material fuera del kit de props: {name}')
    cfg = MATERIAL_DEFS[name]
    return C.get_material_ext(
        name, cfg['base_color'], cfg['roughness'],
        metallic=cfg.get('metallic', 0.0), alpha_blend=cfg.get('alpha_blend', False))


def assign(obj, slot_names):
    """Asigna los slots de material en el orden dado (orden estable)."""
    obj.data.materials.clear()
    for name in slot_names:
        obj.data.materials.append(get_material(name))
