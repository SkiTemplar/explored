"""Materiales de F6 (final y clima de Baliza Siete), por código en el editor de UE 5.6:

    /Game/Art/Materials/M_BeaconBeam   haz de la baliza: Unlit aditivo, color «papel» de la paleta,
                                       se apaga con la distancia a la linterna (BeamOrigin, BeamLength),
                                       en los bordes del cono y al cortar el mar (DepthFade).
    /Game/Art/Materials/M_RainStreak   trazo de lluvia: Unlit translúcido, color «gris_lila», para
                                       instancias (AExploredRainStreaks).
    /Game/Art/Materials/M_TerrainToon  terreno suave: Default Lit, base color = color por vértice
                                       (colores de paleta.json fundidos por Tools/Island), sin texturas.
    M_Toon                             + viento en palmas y bambú (material_utils.add_toon_wind).

Idempotente. Uso: powershell -File Tools/unreal_python.ps1 -Script Tools/Unreal/materials_f6.py
"""

import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from material_utils import (  # noqa: E402 - necesita el sys.path de arriba
    ART_MATERIALS_PATH, MEL, add_toon_wind, connect, custom, expr, finish, palette_linear, recreate_material,
    scalar_param, to_property, vector_param,
)

BEAM_HLSL = r"""
float d = length(WorldPos - Origin);
float along = pow(saturate(1.0 - d / max(Length, 1.0)), 1.4);
float edge = pow(saturate(abs(dot(normalize(Normal), normalize(CamVec)))), 2.0);
float nearFade = saturate(d / 150.0);
return along * edge * nearFade;
"""


def build_beam():
    m = recreate_material("M_BeaconBeam", ART_MATERIALS_PATH)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("two_sided", True)
    world = expr(m, unreal.MaterialExpressionWorldPosition, -900, 0)
    origin = vector_param(m, "BeamOrigin", (0.0, 0.0, 0.0), -900, 100)
    origin_rgb = expr(m, unreal.MaterialExpressionComponentMask, -700, 100)
    for channel in ("r", "g", "b", "a"):
        origin_rgb.set_editor_property(channel, channel != "a")
    connect(origin, "", origin_rgb, "")
    length = scalar_param(m, "BeamLength", 9000.0, -900, 200)
    normal = expr(m, unreal.MaterialExpressionVertexNormalWS, -900, 300)
    camera = expr(m, unreal.MaterialExpressionCameraVectorWS, -900, 400)
    shape = custom(m, -500, 150, BEAM_HLSL, ["WorldPos", "Origin", "Length", "Normal", "CamVec"],
                   unreal.CustomMaterialOutputType.CMOT_FLOAT1, "BeamShape")
    connect(world, "", shape, "WorldPos")
    connect(origin_rgb, "", shape, "Origin")
    connect(length, "", shape, "Length")
    connect(normal, "", shape, "Normal")
    connect(camera, "", shape, "CamVec")
    depth = expr(m, unreal.MaterialExpressionDepthFade, -300, 300)
    depth.set_editor_property("fade_distance_default", 250.0)
    color = vector_param(m, "BeamColor", palette_linear("papel"), -500, -150)
    intensity = scalar_param(m, "BeamIntensity", 0.6, -500, -50)
    tint = expr(m, unreal.MaterialExpressionMultiply, -250, -100)
    connect(color, "", tint, "A")
    connect(intensity, "", tint, "B")
    shaped = expr(m, unreal.MaterialExpressionMultiply, -100, 0)
    connect(tint, "", shaped, "A")
    connect(shape, "", shaped, "B")
    final = expr(m, unreal.MaterialExpressionMultiply, 50, 100)
    connect(shaped, "", final, "A")
    connect(depth, "", final, "B")
    to_property(final, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)


def build_rain():
    m = recreate_material("M_RainStreak", ART_MATERIALS_PATH)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("two_sided", True)
    color = vector_param(m, "RainColor", palette_linear("gris_lila"), -500, 0)
    brightness = scalar_param(m, "Brightness", 1.0, -500, 100)
    lit = expr(m, unreal.MaterialExpressionMultiply, -250, 50)
    connect(color, "", lit, "A")
    connect(brightness, "", lit, "B")
    to_property(lit, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    to_property(scalar_param(m, "Opacity", 0.45, -500, 250), "", unreal.MaterialProperty.MP_OPACITY)
    # Las gotas son instancias: sin este uso el juego dibuja el material por defecto.
    MEL.set_material_usage(m, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    finish(m)


SRGB_TO_LINEAR_HLSL = r"""
float3 c = saturate(C.rgb);
return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
"""


def build_terrain():
    m = recreate_material("M_TerrainToon", ART_MATERIALS_PATH)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    vc = expr(m, unreal.MaterialExpressionVertexColor, -700, 0)
    # El color por vértice llega al sombreador codificado en sRGB (comprobado en f6-suave: sin esta
    # conversión la hierba salía lavada): se pasa a lineal con la curva exacta.
    linear = custom(m, -450, 0, SRGB_TO_LINEAR_HLSL, ["C"], unreal.CustomMaterialOutputType.CMOT_FLOAT3, "SrgbToLinear")
    connect(vc, "", linear, "C")
    to_property(linear, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = scalar_param(m, "Roughness", 1.0, -500, 200)
    to_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    spec = scalar_param(m, "Specular", 0.0, -500, 300)
    to_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    finish(m)


def patch_toon():
    toon = unreal.EditorAssetLibrary.load_asset(f"{ART_MATERIALS_PATH}/M_Toon")
    if toon is None:
        raise RuntimeError("Falta M_Toon (Tools/Unreal/build_materials.py)")
    if add_toon_wind(toon):
        finish(toon)
    else:
        unreal.log("[Explored] M_Toon ya tenía el viento")


build_beam()
build_rain()
build_terrain()
patch_toon()
