"""Materiales de F6 (final y clima de Baliza Siete), por código en el editor de UE 5.6:

    /Game/Art/Materials/M_BeaconBeam   haz de la baliza: Unlit aditivo, color «papel» de la paleta,
                                       se apaga con la distancia a la linterna (BeamOrigin, BeamLength),
                                       en los bordes del cono y al cortar el mar (DepthFade).
    /Game/Art/Materials/M_RainStreak   trazo de lluvia: Unlit translúcido, color «gris_lila», para
                                       instancias (AExploredRainStreaks).
    /Game/Art/Materials/M_TerrainToon  terreno: Default Lit, base color = color por vértice (colores de
                                       paleta.json fundidos por Tools/Island) × trazos estilizados: la
                                       máscara T_TerrainStrokes (Tools/Textures, escala de grises) en
                                       espacio de mundo a dos escalas, que oscurece o aclara el color entre
                                       DarkFactor y LightFactor y se apaga en pendientes y a distancia
                                       (parámetros en Tools/Textures/strokes.json; GDD §11).
    /Game/Art/Materials/T_TerrainStrokes  la máscara (Art/Export/Textures/T_TerrainStrokes.png): lineal,
                                       TC_Grayscale, con mips y repetición.
    M_Toon                             + viento en palmas y bambú (material_utils.add_toon_wind) y cristal
                                       encendido de la linterna (material_utils.add_toon_glow).

Idempotente. Antes: cd Tools/Textures; uv run textures-strokes (genera la máscara). Para reimportar la
máscara si ya existe en el proyecto: EXPLORED_REIMPORT_STROKES=1.
Uso: powershell -File Tools/unreal_python.ps1 -Script Tools/Unreal/materials_f6.py
"""

import json
import math
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from material_utils import (  # noqa: E402 - necesita el sys.path de arriba
    ART_MATERIALS_PATH, ASSET_TOOLS, MEL, REPO_ROOT, add_toon_glow, add_toon_wind, connect, custom, expr, finish,
    palette_linear, recreate_material, scalar_param, to_property, vector_param,
)

STROKES_JSON = os.path.join(REPO_ROOT, "Tools", "Textures", "strokes.json")
STROKES_PNG = os.path.join(REPO_ROOT, "Art", "Export", "Textures", "T_TerrainStrokes.png")

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


STROKES_UV_HLSL = r"""
float2 p = WP.xy / (TileM * 100.0);
float s = sin(Turn);
float c = cos(Turn);
return float2(c * p.x - s * p.y, s * p.x + c * p.y);
"""

# Factor de color de los trazos (mismo cálculo que la vista previa de Tools/Island, preview_materials.py):
# lerp(1, lerp(Dark, Light, máscara), fundido por pendiente y distancia).
STROKES_FACTOR_HLSL = r"""
float mask = W0 * M0 + W1 * M1;
float tone = lerp(Dark, Light, mask);
float slope = saturate((N.z - SlopeA) / max(SlopeB - SlopeA, 1e-3));
float dist = distance(WP, Cam) / 100.0;
float far = 1.0 - saturate((dist - FarA) / max(FarB - FarA, 1e-3));
return lerp(1.0, tone, slope * far);
"""


def import_strokes_texture(name: str) -> unreal.Texture2D:
    """Importa la máscara de trazos (lineal, escala de grises, con mips y repetición)."""
    full = f"{ART_MATERIALS_PATH}/{name}"
    exists = unreal.EditorAssetLibrary.does_asset_exist(full)
    if not exists or os.environ.get("EXPLORED_REIMPORT_STROKES") == "1":
        if not os.path.isfile(STROKES_PNG):
            raise RuntimeError(f"Falta {STROKES_PNG} (cd Tools/Textures; uv run textures-strokes)")
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", STROKES_PNG)
        task.set_editor_property("destination_path", ART_MATERIALS_PATH)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", False)
        ASSET_TOOLS.import_asset_tasks([task])
    texture = unreal.load_asset(full)
    if texture is None:
        raise RuntimeError(f"No se pudo importar {full}")
    texture.set_editor_property("srgb", False)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_GRAYSCALE)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    texture.set_editor_property("address_x", unreal.TextureAddress.TA_WRAP)
    texture.set_editor_property("address_y", unreal.TextureAddress.TA_WRAP)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    return texture


def strokes_sample(m, texture, world, tile_m: float, turn_deg: float, index: int, y: int):
    """UV en espacio de mundo (cm -> teselas de tile_m metros, girada turn_deg) y muestra de la máscara."""
    uv = custom(m, -1100, y, STROKES_UV_HLSL, ["WP", "TileM", "Turn"], unreal.CustomMaterialOutputType.CMOT_FLOAT2,
                f"StrokesUV{index}")
    connect(world, "", uv, "WP")
    connect(scalar_param(m, f"StrokesTile{index}", tile_m, -1350, y + 40), "", uv, "TileM")
    turn = expr(m, unreal.MaterialExpressionConstant, -1350, y + 120)
    turn.set_editor_property("r", math.radians(turn_deg))
    connect(turn, "", uv, "Turn")
    sample = expr(m, unreal.MaterialExpressionTextureSample, -850, y)
    sample.set_editor_property("texture", texture)
    sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
    connect(uv, "", sample, "UVs")
    return sample


def build_terrain():
    m = recreate_material("M_TerrainToon", ART_MATERIALS_PATH)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    with open(STROKES_JSON, "r", encoding="utf-8") as f:
        strokes = json.load(f)
    texture = import_strokes_texture(strokes["texture"])
    vc = expr(m, unreal.MaterialExpressionVertexColor, -700, 0)
    # El color por vértice llega al sombreador codificado en sRGB (comprobado en f6-suave: sin esta
    # conversión la hierba salía lavada): se pasa a lineal con la curva exacta.
    linear = custom(m, -450, 0, SRGB_TO_LINEAR_HLSL, ["C"], unreal.CustomMaterialOutputType.CMOT_FLOAT3, "SrgbToLinear")
    connect(vc, "", linear, "C")

    world = expr(m, unreal.MaterialExpressionWorldPosition, -1350, 400)
    turns = (0.0, strokes["second_rotation_deg"])
    samples = [strokes_sample(m, texture, world, tile, turn, i, 300 + 260 * i)
               for i, (tile, turn) in enumerate(zip(strokes["tiles_m"], turns))]
    factor = custom(m, -500, 400, STROKES_FACTOR_HLSL,
                    ["M0", "M1", "W0", "W1", "Dark", "Light", "N", "SlopeA", "SlopeB", "WP", "Cam", "FarA", "FarB"],
                    unreal.CustomMaterialOutputType.CMOT_FLOAT1, "StrokesFactor")
    connect(samples[0], "R", factor, "M0")
    connect(samples[1], "R", factor, "M1")
    scalars = {
        "W0": ("StrokesWeight0", strokes["weights"][0]), "W1": ("StrokesWeight1", strokes["weights"][1]),
        "Dark": ("DarkFactor", strokes["dark_factor"]), "Light": ("LightFactor", strokes["light_factor"]),
        "SlopeA": ("SlopeFadeStart", strokes["slope_fade"][0]), "SlopeB": ("SlopeFadeEnd", strokes["slope_fade"][1]),
        "FarA": ("FadeStartM", strokes["distance_fade_m"][0]), "FarB": ("FadeEndM", strokes["distance_fade_m"][1]),
    }
    for row, (pin, (name, value)) in enumerate(scalars.items()):
        connect(scalar_param(m, name, float(value), -800, 860 + 70 * row), "", factor, pin)
    connect(expr(m, unreal.MaterialExpressionVertexNormalWS, -800, 1450), "", factor, "N")
    connect(world, "", factor, "WP")
    connect(expr(m, unreal.MaterialExpressionCameraPositionWS, -800, 1530), "", factor, "Cam")

    tinted = expr(m, unreal.MaterialExpressionMultiply, -200, 100)
    connect(linear, "", tinted, "A")
    connect(factor, "", tinted, "B")
    to_property(tinted, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = scalar_param(m, "Roughness", 1.0, -500, 200)
    to_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    spec = scalar_param(m, "Specular", 0.0, -500, 300)
    to_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    finish(m)


def patch_toon():
    toon = unreal.EditorAssetLibrary.load_asset(f"{ART_MATERIALS_PATH}/M_Toon")
    if toon is None:
        raise RuntimeError("Falta M_Toon (Tools/Unreal/build_materials.py)")
    added_wind = add_toon_wind(toon)
    added_glow = add_toon_glow(toon)
    if added_wind or added_glow:
        finish(toon)
    if not added_wind:
        unreal.log("[Explored] M_Toon ya tenía el viento")
    if not added_glow:
        unreal.log("[Explored] M_Toon ya tenía el cristal encendido")


build_beam()
build_rain()
build_terrain()
patch_toon()
