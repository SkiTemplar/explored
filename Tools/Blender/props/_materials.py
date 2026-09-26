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
    # base_color se MULTIPLICA por el color de vertice en get_material_ext
    # (Base = RGB constante x atributo «Col»): si aqui tambien va oscuro, el
    # resultado final se oscurece dos veces y sale "apagado" aunque el color
    # de vertice de cada pieza sea vivo (bug real detectado en la revision
    # visual 2026-09-26: todo el kit salia oscuro y apagado). Por eso el
    # color de CADA material vive casi blanco/neutro aqui, y es el color de
    # vertice de cada builder el que aporta el tono real (madera calida,
    # lona con color, piedra clara...); solo M_Glass e M_Metal llevan un
    # ligero tinte propio porque no todas las piezas de vidrio/metal fijan
    # tinte de vertice explicito.
    'M_Wood':   dict(base_color=(1.00, 0.97, 0.90), roughness=0.75, metallic=0.0),
    'M_Metal':  dict(base_color=(0.92, 0.93, 0.95), roughness=0.35, metallic=0.85),
    'M_Fabric': dict(base_color=(1.00, 0.98, 0.94), roughness=0.85, metallic=0.0),
    'M_Stone':  dict(base_color=(0.97, 0.95, 0.90), roughness=0.85, metallic=0.0),
    'M_Glass':  dict(base_color=(0.80, 0.92, 0.85), roughness=0.05, metallic=0.0, alpha_blend=True),
    'M_Paper':  dict(base_color=(1.00, 0.98, 0.92), roughness=0.7, metallic=0.0),
    'M_Leaf':   dict(base_color=(0.95, 1.00, 0.85), roughness=0.5, metallic=0.0),
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
