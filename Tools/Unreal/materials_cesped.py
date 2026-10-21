"""Material del césped (kit K6, F7), por código en el editor de UE 5.6:

    /Game/Art/Materials/M_ToonCesped  M_Toon de DOS CARAS para las tarjetas de una cara de K6 (sin
                                      two_sided desaparece la mitad de las hojas): base color = T_Paleta
                                      con el UV 0, oscurecido hacia la base (BaseShade × VertexColor.A),
                                      rugosidad 1 y especular 0. WPO = viento × VertexColor.A (0 en la
                                      base, 1 en la punta; tréboles ≤ 0,35), con WindSway (1 por defecto)
                                      y WindStrength (0-1, AExploredGrassField lo copia del clima), más el
                                      fundido por distancia: la mata encoge hacia su pivote entre
                                      FadeStartCm y FadeEndCm (grass.json «fade»; el componente la corta en
                                      FadeEndCm). Uso con instancias (ISM/HISM).

Idempotente. Antes: build_materials.py (T_Paleta). Después: EXPLORED_KITS=K6_Cesped import_props.py.
Uso: powershell -File Tools/unreal_python.ps1 -Script Tools/Unreal/materials_cesped.py
build_materials.py también lo crea al reconstruir todos los materiales.
"""

import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from material_utils import (  # noqa: E402 - necesita el sys.path de arriba
    ART_MATERIALS_PATH, connect, custom, expr, finish, recreate_material, scalar_param, to_property,
)

GRASS_WIND_HLSL = r"""
float phase = dot(ObjectPos.xy, float2(0.011, 0.008));
float s = sin(T * (1.6 + 1.4 * Strength) + phase) * 0.7 + sin(T * (3.3 + 2.0 * Strength) + phase * 2.3) * 0.25;
float amp = (3.0 + 12.0 * Strength) * Sway * saturate(Weight);
float3 wind = float3(s * amp, s * amp * 0.6, -abs(s) * amp * 0.25);
float d = length(ObjectPos.xy - CamPos.xy);
float shrink = saturate((d - FadeStartCm) / max(FadeEndCm - FadeStartCm, 1.0));
return wind * (1.0 - shrink) + (ObjectPos - WorldPos) * shrink;
"""


def build_toon_cesped(palette: unreal.Texture2D) -> unreal.Material:
    m = recreate_material("M_ToonCesped", ART_MATERIALS_PATH)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    m.set_editor_property("two_sided", True)

    uv = expr(m, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    sample = expr(m, unreal.MaterialExpressionTextureSample, -650, 0)
    sample.set_editor_property("texture", palette)
    sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    connect(uv, "", sample, "UVs")

    vcolor = expr(m, unreal.MaterialExpressionVertexColor, -900, 300)
    shade = scalar_param(m, "BaseShade", 0.75, -900, 200)
    one = expr(m, unreal.MaterialExpressionConstant, -900, 120)
    one.set_editor_property("r", 1.0)
    tip = expr(m, unreal.MaterialExpressionLinearInterpolate, -450, 150)
    connect(shade, "", tip, "A")
    connect(one, "", tip, "B")
    connect(vcolor, "A", tip, "Alpha")
    base = expr(m, unreal.MaterialExpressionMultiply, -250, 50)
    connect(sample, "RGB", base, "A")
    connect(tip, "", base, "B")
    to_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)

    roughness = expr(m, unreal.MaterialExpressionConstant, -250, 250)
    roughness.set_editor_property("r", 1.0)
    to_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    specular = expr(m, unreal.MaterialExpressionConstant, -250, 330)
    specular.set_editor_property("r", 0.0)
    to_property(specular, "", unreal.MaterialProperty.MP_SPECULAR)

    inputs = [
        ("WorldPos", expr(m, unreal.MaterialExpressionWorldPosition, -900, 500), ""),
        ("ObjectPos", expr(m, unreal.MaterialExpressionObjectPositionWS, -900, 600), ""),
        ("CamPos", expr(m, unreal.MaterialExpressionCameraPositionWS, -900, 700), ""),
        ("T", expr(m, unreal.MaterialExpressionTime, -900, 800), ""),
        ("Sway", scalar_param(m, "WindSway", 1.0, -900, 900), ""),
        ("Strength", scalar_param(m, "WindStrength", 0.3, -900, 1000), ""),
        ("Weight", vcolor, "A"),
        ("FadeStartCm", scalar_param(m, "FadeStartCm", 4500.0, -900, 1100), ""),
        ("FadeEndCm", scalar_param(m, "FadeEndCm", 7000.0, -900, 1200), ""),
    ]
    wind = custom(m, -450, 800, GRASS_WIND_HLSL, [name for name, _, _ in inputs],
                  unreal.CustomMaterialOutputType.CMOT_FLOAT3, "GrassWindFade")
    for name, node, out in inputs:
        connect(node, out, wind, name)
    to_property(wind, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
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
