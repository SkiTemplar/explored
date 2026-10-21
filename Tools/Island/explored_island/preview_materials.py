"""Materiales de la vista previa del terreno (Blender): réplicas de M_TerrainToon y M_Toon.

`terrain_material` multiplica el color por vértice por el factor de trazos de M_TerrainToon:
lerp(1, lerp(dark_factor, light_factor, máscara), fundido), con la máscara T_TerrainStrokes
muestreada en espacio de mundo a dos escalas (`tiles_m`, la segunda girada) y el fundido por la
pendiente (componente Z de la normal) y la distancia a la cámara (Tools/Textures/strokes.json).
Los nodos se crean por tipo y los sockets del nodo Mix se buscan por identificador.
"""

from __future__ import annotations

import math
from typing import Any, cast

import bpy

import numpy as np


def _link(tree: Any, a: Any, b: Any) -> None:
    tree.links.new(a, b)


def _sock(sockets: Any, identifier: str) -> Any:
    """Socket por identificador (el nodo Mix repite nombres «A», «B» y «Result» por tipo de dato)."""
    return next(s for s in sockets if s.identifier == identifier)


def _strip(mat: bpy.types.Material) -> tuple[Any, Any, Any]:
    """Árbol vacío con salida y Principled (rugosidad 1, sin especular); devuelve (árbol, bsdf, salida)."""
    tree = cast(Any, mat.node_tree)
    out = next(n for n in tree.nodes if n.type == 'OUTPUT_MATERIAL')
    for node in [n for n in tree.nodes if n.type != 'OUTPUT_MATERIAL']:
        tree.nodes.remove(node)
    bsdf = tree.nodes.new('ShaderNodeBsdfPrincipled')
    bsdf.inputs['Roughness'].default_value = 1.0
    bsdf.inputs['Specular IOR Level'].default_value = 0.0
    _link(tree, bsdf.outputs[0], out.inputs['Surface'])
    return tree, bsdf, out


def palette_material(sheet: bpy.types.Image) -> bpy.types.Material:
    mat = bpy.data.materials.new('M_ToonPreview')
    tree, bsdf, _ = _strip(mat)
    tex = tree.nodes.new('ShaderNodeTexImage')
    tex.image = sheet
    tex.interpolation = 'Closest'
    _link(tree, tex.outputs['Color'], bsdf.inputs['Base Color'])
    return mat


def flat_material(name: str, rgb: tuple[int, int, int], roughness: float) -> bpy.types.Material:
    mat = bpy.data.materials.new(name)
    _, bsdf, _ = _strip(mat)
    srgb = np.array(rgb) / 255.0
    lin = np.where(srgb <= 0.04045, srgb / 12.92, ((srgb + 0.055) / 1.055) ** 2.4)
    bsdf.inputs['Base Color'].default_value = (*lin.tolist(), 1.0)
    bsdf.inputs['Roughness'].default_value = roughness
    return mat


def _strokes_factor(tree: Any, strokes: dict, image: bpy.types.Image) -> Any:
    """Factor de color de M_TerrainToon: lerp(1, lerp(dark, light, máscara), fundido)."""
    nodes = tree.nodes
    pos = nodes.new('ShaderNodeNewGeometry')
    mask = None
    for tile, weight, turn in zip(strokes['tiles_m'], strokes['weights'], (0.0, strokes['second_rotation_deg']),
                                  strict=True):
        mapping = nodes.new('ShaderNodeMapping')
        mapping.inputs['Rotation'].default_value = (0.0, 0.0, math.radians(turn))
        mapping.inputs['Scale'].default_value = (1.0 / tile, 1.0 / tile, 1.0)
        _link(tree, pos.outputs['Position'], mapping.inputs['Vector'])
        tex = nodes.new('ShaderNodeTexImage')
        tex.image = image
        tex.extension = 'REPEAT'
        _link(tree, mapping.outputs['Vector'], tex.inputs['Vector'])
        scaled = nodes.new('ShaderNodeMath')
        scaled.operation = 'MULTIPLY'
        scaled.inputs[1].default_value = weight
        _link(tree, tex.outputs['Color'], scaled.inputs[0])
        if mask is None:
            mask = scaled
            continue
        add = nodes.new('ShaderNodeMath')
        add.operation = 'ADD'
        _link(tree, mask.outputs[0], add.inputs[0])
        _link(tree, scaled.outputs[0], add.inputs[1])
        mask = add
    tone = nodes.new('ShaderNodeMapRange')
    tone.inputs['To Min'].default_value = strokes['dark_factor']
    tone.inputs['To Max'].default_value = strokes['light_factor']
    _link(tree, cast(Any, mask).outputs[0], tone.inputs['Value'])
    normal_z = nodes.new('ShaderNodeSeparateXYZ')
    _link(tree, pos.outputs['Normal'], normal_z.inputs[0])
    slope = nodes.new('ShaderNodeMapRange')
    slope.inputs['From Min'].default_value, slope.inputs['From Max'].default_value = strokes['slope_fade']
    _link(tree, normal_z.outputs['Z'], slope.inputs['Value'])
    camera = nodes.new('ShaderNodeCameraData')
    far = nodes.new('ShaderNodeMapRange')
    far.inputs['From Min'].default_value, far.inputs['From Max'].default_value = strokes['distance_fade_m']
    far.inputs['To Min'].default_value, far.inputs['To Max'].default_value = 1.0, 0.0
    _link(tree, camera.outputs['View Distance'], far.inputs['Value'])
    fade = nodes.new('ShaderNodeMath')
    fade.operation = 'MULTIPLY'
    _link(tree, slope.outputs[0], fade.inputs[0])
    _link(tree, far.outputs[0], fade.inputs[1])
    factor = nodes.new('ShaderNodeMix')
    factor.data_type = 'FLOAT'
    _sock(factor.inputs, 'A_Float').default_value = 1.0
    _link(tree, fade.outputs[0], _sock(factor.inputs, 'Factor_Float'))
    _link(tree, tone.outputs[0], _sock(factor.inputs, 'B_Float'))
    return _sock(factor.outputs, 'Result_Float')


def terrain_material(color_layer: str, strokes: dict | None, image: bpy.types.Image | None) -> bpy.types.Material:
    mat = bpy.data.materials.new('M_TerrainToonPreview')
    tree, bsdf, _ = _strip(mat)
    color = tree.nodes.new('ShaderNodeVertexColor')
    color.layer_name = color_layer
    if strokes is None or image is None:
        _link(tree, color.outputs['Color'], bsdf.inputs['Base Color'])
        return mat
    factor = _strokes_factor(tree, strokes, image)
    tint = tree.nodes.new('ShaderNodeMix')
    tint.data_type = 'RGBA'
    tint.blend_type = 'MULTIPLY'
    _sock(tint.inputs, 'Factor_Float').default_value = 1.0
    _link(tree, color.outputs['Color'], _sock(tint.inputs, 'A_Color'))
    value = tree.nodes.new('ShaderNodeCombineColor')
    for channel in ('Red', 'Green', 'Blue'):
        _link(tree, factor, value.inputs[channel])
    _link(tree, value.outputs['Color'], _sock(tint.inputs, 'B_Color'))
    _link(tree, _sock(tint.outputs, 'Result_Color'), bsdf.inputs['Base Color'])
    return mat
