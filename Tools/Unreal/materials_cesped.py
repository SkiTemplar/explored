"""Material del césped continuo (kit K6, F7-F8), por código en el editor de UE 5.6:

    /Game/Art/Materials/MPC_Terreno   colección compartida: adelgazamiento del césped (Grass*, valores de
                                      grass.json «thinning»; AExploredGrassField los reescribe al empezar) y
                                      el sol (SunDirection, SunGlint; AExploredSkyController) para la arena.
    /Game/Art/Materials/M_ToonCesped  M_Toon de DOS CARAS para las tarjetas de una cara de K6, opaco y sin
                                      alfa: base color = T_Paleta (UV 0) con degradado base→punta
                                      (BaseShade → TipBright por VertexColor.A) y tinte por parche (dato por
                                      instancia 1), saturado (GRASS_SATURATION), que tiende al verde de la
                                      alfombra lejana al alejarse (FarBlend × (1 - f)); normal en espacio de
                                      mundo, igual en las dos caras y redondeada hacia arriba (UpBias), para
                                      que la alfombra se ilumine como el suelo y no por hojas; rugosidad 1 y
                                      especular 0. WPO: adelgazamiento continuo (dato por instancia 0 = rango:
                                      el parche encoge a 0 cuando f(d) baja de él y los que quedan crecen
                                      hasta GrassMaxGrow; misma f(d) que FGrassThinning), variación de altura
                                      por parche y viento × VertexColor.A que se apaga entre WindFadeStartM y
                                      WindFadeEndM (WindSway, WindStrength: AExploredGrassField). Uso con
                                      instancias (ISM/HISM).

Idempotente. Antes: build_materials.py (T_Paleta). Después: EXPLORED_KITS=K6_Cesped import_props.py y
materials_f6.py (M_TerrainToon lee MPC_Terreno).
Uso: powershell -File Tools/unreal_python.ps1 -Script Tools/Unreal/materials_cesped.py
build_materials.py también lo crea al reconstruir todos los materiales.
"""

import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from material_utils import (  # noqa: E402 - necesita el sys.path de arriba
    ART_MATERIALS_PATH, GRASS_BASE_SHADE, GRASS_FRACTION_HLSL, GRASS_SATURATE_HLSL, GRASS_SATURATION,
    GRASS_THINNING_KEYS, GRASS_TIP_BRIGHT, collection_param, connect,
    custom, expr, finish, grass_ground_colors, hlsl_float3, recreate_material, scalar_param, terrain_collection,
    to_property,
)

# Peso de la vertical en la normal de las hojas (0 = normal de la malla, 1 = como el suelo llano).
GRASS_UP_BIAS = 0.85

GRASS_WPO_HLSL = r"""
float d = length(ObjectPos.xy - CamPos.xy) / 100.0;
""" + GRASS_FRACTION_HLSL + r"""
float vis = GrassF > 0.0 ? saturate((GrassF * (1.0 + GrassBand) - Rank) / max(GrassBand * GrassF, 1e-4)) : 0.0;
float grow = clamp(rsqrt(max(GrassF, 1e-4)), 1.0, GrassMaxGrow);
float growZ = lerp(1.0, grow, GrassGrowZ) * lerp(1.0 - HeightVar, 1.0 + HeightVar, Var);
float3 local = WorldPos - ObjectPos;
float3 scaled = float3(local.xy * grow, local.z * growZ) * vis;
float phase = dot(ObjectPos.xy, float2(0.011, 0.008));
float s = sin(T * (1.6 + 1.4 * Strength) + phase) * 0.7 + sin(T * (3.3 + 2.0 * Strength) + phase * 2.3) * 0.25;
float windFade = 1.0 - saturate((d - WindFadeStartM) / max(WindFadeEndM - WindFadeStartM, 1e-3));
float amp = (3.0 + 12.0 * Strength) * Sway * saturate(Weight) * windFade * vis;
float3 wind = float3(s * amp, s * amp * 0.6, -abs(s) * amp * 0.25);
return scaled - local + wind;
"""

# Degradado base→punta y tinte por parche; al alejarse, las hojas tienden al verde de la alfombra lejana
# (el mismo del suelo bajo el césped en M_TerrainToon), así que los parches que quedan no se leen como matas.
GRASS_COLOR_HLSL = r"""
float d = length(ObjectPos.xy - CamPos.xy) / 100.0;
""" + GRASS_FRACTION_HLSL + r"""
// Lejos, sin la base oscura: los parches que quedan no se leen como matas sueltas sobre el suelo.
float g = lerp(lerp(BaseShade, TipBright, saturate(Weight)), 0.95, 1.0 - GrassF);
float3 tint = lerp(float3(0.90, 0.95, 0.92), float3(1.07, 1.05, 0.90), Var);
float3 c = saturate(Pal * g * tint);
""" + GRASS_SATURATE_HLSL + r"""return lerp(c, GRASS_FAR, FarBlend * (1.0 - GrassF));
"""

# La misma normal en las dos caras de la tarjeta (la del hemisferio de arriba, sin el signo de la cara) y
# muy redondeada hacia arriba: el sombreado lo lleva el degradado base→punta, no la orientación de cada
# triángulo (con UpBias 0,6 cada hoja salía con un tono plano distinto: facetado de papel).
GRASS_NORMAL_HLSL = r"""
float3 n = normalize(N);
n = n.z < 0.0 ? -n : n;
return normalize(lerp(n, float3(0.0, 0.0, 1.0), UpBias));
"""


def custom_data(m, index: int, x: int, y: int):
    node = expr(m, unreal.MaterialExpressionPerInstanceCustomData, x, y)
    node.set_editor_property("data_index", index)
    return node


def build_toon_cesped(palette: unreal.Texture2D) -> unreal.Material:
    mpc = terrain_collection()
    m = recreate_material("M_ToonCesped", ART_MATERIALS_PATH)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("tangent_space_normal", False)

    uv = expr(m, unreal.MaterialExpressionTextureCoordinate, -1100, 0)
    sample = expr(m, unreal.MaterialExpressionTextureSample, -900, 0)
    sample.set_editor_property("texture", palette)
    sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    connect(uv, "", sample, "UVs")
    vcolor = expr(m, unreal.MaterialExpressionVertexColor, -1100, 300)
    rank = custom_data(m, 0, -1100, 1500)
    var = custom_data(m, 1, -1100, 1580)

    far = hlsl_float3(grass_ground_colors()[1])
    color_inputs = ["Pal", "Weight", "Var", "BaseShade", "TipBright", "FarBlend", "ObjectPos", "CamPos",
                    "GrassDenseM", "GrassEndM", "GrassEndFadeM", "GrassExponent"]
    color_code = GRASS_COLOR_HLSL.replace("GRASS_FAR", far).replace("GRASS_SAT", f"{GRASS_SATURATION:.3f}")
    color = custom(m, -500, 0, color_code, color_inputs,
                   unreal.CustomMaterialOutputType.CMOT_FLOAT3, "GrassColor")
    connect(sample, "RGB", color, "Pal")
    connect(vcolor, "A", color, "Weight")
    connect(var, "", color, "Var")
    connect(scalar_param(m, "BaseShade", GRASS_BASE_SHADE, -800, 150), "", color, "BaseShade")
    connect(scalar_param(m, "TipBright", GRASS_TIP_BRIGHT, -800, 220), "", color, "TipBright")
    connect(scalar_param(m, "FarBlend", 0.75, -800, 290), "", color, "FarBlend")
    connect(expr(m, unreal.MaterialExpressionObjectPositionWS, -800, 360), "", color, "ObjectPos")
    connect(expr(m, unreal.MaterialExpressionCameraPositionWS, -800, 430), "", color, "CamPos")
    for i, name in enumerate(("GrassDenseM", "GrassEndM", "GrassEndFadeM", "GrassExponent")):
        connect(collection_param(m, mpc, name, -800, 500 + 60 * i), "", color, name)
    to_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)

    normal = custom(m, -500, 400, GRASS_NORMAL_HLSL, ["N", "UpBias"],
                    unreal.CustomMaterialOutputType.CMOT_FLOAT3, "GrassNormal")
    connect(expr(m, unreal.MaterialExpressionVertexNormalWS, -800, 400), "", normal, "N")
    connect(scalar_param(m, "UpBias", GRASS_UP_BIAS, -800, 520), "", normal, "UpBias")
    to_property(normal, "", unreal.MaterialProperty.MP_NORMAL)

    roughness = expr(m, unreal.MaterialExpressionConstant, -250, 250)
    roughness.set_editor_property("r", 1.0)
    to_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    specular = expr(m, unreal.MaterialExpressionConstant, -250, 330)
    specular.set_editor_property("r", 0.0)
    to_property(specular, "", unreal.MaterialProperty.MP_SPECULAR)

    inputs = [
        ("WorldPos", expr(m, unreal.MaterialExpressionWorldPosition, -1100, 700), ""),
        ("ObjectPos", expr(m, unreal.MaterialExpressionObjectPositionWS, -1100, 780), ""),
        ("CamPos", expr(m, unreal.MaterialExpressionCameraPositionWS, -1100, 860), ""),
        ("T", expr(m, unreal.MaterialExpressionTime, -1100, 940), ""),
        ("Sway", scalar_param(m, "WindSway", 1.0, -1100, 1020), ""),
        ("Strength", scalar_param(m, "WindStrength", 0.3, -1100, 1100), ""),
        ("Weight", vcolor, "A"),
        ("WindFadeStartM", scalar_param(m, "WindFadeStartM", 30.0, -1100, 1180), ""),
        ("WindFadeEndM", scalar_param(m, "WindFadeEndM", 45.0, -1100, 1260), ""),
        ("HeightVar", scalar_param(m, "HeightVariation", 0.15, -1100, 1340), ""),
        ("Rank", rank, ""),
        ("Var", var, ""),
    ]
    inputs += [(name, collection_param(m, mpc, name, -1100, 1700 + 70 * i), "")
               for i, name in enumerate(GRASS_THINNING_KEYS)]
    wpo = custom(m, -500, 900, GRASS_WPO_HLSL, [name for name, _, _ in inputs],
                 unreal.CustomMaterialOutputType.CMOT_FLOAT3, "GrassThinningWind")
    for name, node, out in inputs:
        connect(node, out, wpo, name)
    to_property(wpo, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    finish(m)
    return m


def main():
    palette = unreal.load_asset(f"{ART_MATERIALS_PATH}/T_Paleta")
    if palette is None:
        raise RuntimeError(f"Falta {ART_MATERIALS_PATH}/T_Paleta: ejecuta antes build_materials.py")
    build_toon_cesped(palette)
    return 0


if __name__ == "__main__":
    sys.exit(main())
