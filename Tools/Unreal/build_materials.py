"""Crea los materiales base del juego por código (idempotente).

Se ejecuta con:
    UnrealEditor-Cmd Explored.uproject -run=pythonscript -script="Tools/Unreal/build_materials.py"

Materiales:
    /Game/Materials/M_Terrain  color por vértice (sRGB) con variación procedural y roca en alfa.
    /Game/Materials/M_Ocean    Single Layer Water con olas de Gerstner (mismas que FOceanWaves).
    /Game/Materials/M_Stars    cúpula de estrellas aditiva controlada por el parámetro «Night».
"""

import unreal

MATERIALS_PATH = "/Game/Materials"
MEL = unreal.MaterialEditingLibrary
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def recreate_material(name: str) -> unreal.Material:
    """Borra el material si existe y crea uno vacío."""
    full = f"{MATERIALS_PATH}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(full):
        unreal.EditorAssetLibrary.delete_asset(full)
    material = ASSET_TOOLS.create_asset(name, MATERIALS_PATH, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        raise RuntimeError(f"No se pudo crear {full}")
    return material


def expr(material, cls, x, y):
    return MEL.create_material_expression(material, cls, x, y)


def custom(material, x, y, code: str, inputs: list[str], output_type, description: str):
    node = expr(material, unreal.MaterialExpressionCustom, x, y)
    node.set_editor_property("code", code)
    node.set_editor_property("output_type", output_type)
    node.set_editor_property("description", description)
    custom_inputs = []
    for name in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        custom_inputs.append(ci)
    node.set_editor_property("inputs", custom_inputs)
    return node


def connect(src, src_out: str, dst, dst_in: str):
    if not MEL.connect_material_expressions(src, src_out, dst, dst_in):
        raise RuntimeError(f"No se pudo conectar {src.get_name()}.{src_out} -> {dst.get_name()}.{dst_in}")


def to_property(src, src_out: str, prop):
    if not MEL.connect_material_property(src, src_out, prop):
        raise RuntimeError(f"No se pudo conectar {src.get_name()}.{src_out} -> {prop}")


def finish(material):
    MEL.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    unreal.log(f"[Explored] Material listo: {material.get_path_name()}")


# ---------------------------------------------------------------------------
# Terreno
# ---------------------------------------------------------------------------

TERRAIN_HLSL = r"""
// Color por vértice en sRGB -> lineal.
float3 baseColor = pow(saturate(VC.rgb), 2.2);

// Ruido de valor barato en espacio de mundo (metros) a dos escalas.
float3 p = WP / 100.0;
float2 cell = floor(p.xy / 3.0);
float2 f = frac(p.xy / 3.0);
f = f * f * (3.0 - 2.0 * f);
float h00 = frac(sin(dot(cell, float2(127.1, 311.7))) * 43758.5453);
float h10 = frac(sin(dot(cell + float2(1, 0), float2(127.1, 311.7))) * 43758.5453);
float h01 = frac(sin(dot(cell + float2(0, 1), float2(127.1, 311.7))) * 43758.5453);
float h11 = frac(sin(dot(cell + float2(1, 1), float2(127.1, 311.7))) * 43758.5453);
float n1 = lerp(lerp(h00, h10, f.x), lerp(h01, h11, f.x), f.y);

float2 cell2 = floor(p.xy / 0.6);
float n2 = frac(sin(dot(cell2, float2(269.5, 183.3))) * 43758.5453);

// Estratos en la roca siguiendo la altura.
float strata = 0.5 + 0.5 * sin(p.z * 2.3 + n1 * 3.0);
float rock = saturate(VC.a);

float variation = lerp(0.9, 1.1, n1) * lerp(0.96, 1.04, n2);
float3 color = baseColor * variation;
color *= lerp(1.0, lerp(0.85, 1.08, strata), rock);
return color;
"""


def build_terrain():
    m = recreate_material("M_Terrain")
    vc = expr(m, unreal.MaterialExpressionVertexColor, -900, 0)
    wp = expr(m, unreal.MaterialExpressionWorldPosition, -900, 200)
    append = expr(m, unreal.MaterialExpressionAppendVector, -700, 0)
    connect(vc, "", append, "A")
    connect(vc, "A", append, "B")

    body = custom(m, -450, 0, TERRAIN_HLSL, ["VC", "WP"], unreal.CustomMaterialOutputType.CMOT_FLOAT3, "TerrainColor")
    connect(append, "", body, "VC")
    connect(wp, "", body, "WP")
    to_property(body, "", unreal.MaterialProperty.MP_BASE_COLOR)

    rough = expr(m, unreal.MaterialExpressionLinearInterpolate, -300, 300)
    c_soft = expr(m, unreal.MaterialExpressionConstant, -500, 300)
    c_soft.set_editor_property("r", 0.94)
    c_rock = expr(m, unreal.MaterialExpressionConstant, -500, 380)
    c_rock.set_editor_property("r", 0.72)
    connect(c_soft, "", rough, "A")
    connect(c_rock, "", rough, "B")
    connect(vc, "A", rough, "Alpha")
    to_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    spec = expr(m, unreal.MaterialExpressionConstant, -300, 460)
    spec.set_editor_property("r", 0.3)
    to_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    finish(m)


# ---------------------------------------------------------------------------
# Océano
# ---------------------------------------------------------------------------

# Debe coincidir con FOceanWaves::Displacement (cm, s).
GERSTNER_COMMON = r"""
float4 waves[4] = { W0, W1, W2, W3 };
float steep[4] = { S0, S1, S2, S3 };
float2 pos = WP.xy;
float3 offset = 0;
float3 tangent = float3(1, 0, 0);
float3 binormal = float3(0, 1, 0);
[unroll]
for (int i = 0; i < 4; ++i)
{
    float2 d = normalize(waves[i].xy);
    float k = 6.28318530718 / waves[i].z;
    float a = waves[i].w;
    float speed = sqrt(981.0 / k);
    float ph = k * (dot(d, pos) - speed * T);
    float q = steep[i] / (k * a * 4.0);
    float c = cos(ph);
    float s = sin(ph);
    offset += float3(q * a * d.x * c, q * a * d.y * c, a * s);
    tangent += float3(-q * d.x * d.x * k * a * s, -q * d.x * d.y * k * a * s, d.x * k * a * c);
    binormal += float3(-q * d.x * d.y * k * a * s, -q * d.y * d.y * k * a * s, d.y * k * a * c);
}
"""

OCEAN_WPO_HLSL = GERSTNER_COMMON + "return offset;"
OCEAN_NORMAL_HLSL = GERSTNER_COMMON + r"""
float3 n = normalize(cross(tangent, binormal));
// Detalle de ondas pequeñas.
float2 uv = pos / 350.0;
float r1 = sin(uv.x * 3.1 + T * 1.3) * cos(uv.y * 2.7 - T * 1.1);
float r2 = sin(uv.x * 7.3 - T * 2.1 + uv.y * 5.1) * 0.5;
n = normalize(n + float3(r1 * 0.035, r2 * 0.035, 0));
return n;
"""


def build_ocean():
    m = recreate_material("M_Ocean")
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_SINGLE_LAYER_WATER)
    m.set_editor_property("tangent_space_normal", False)
    m.set_editor_property("two_sided", False)

    wp = expr(m, unreal.MaterialExpressionWorldPosition, -1400, 0)
    # Se usa la posición sin desplazar para evaluar las olas.
    wp.set_editor_property("world_position_shader_offset", unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
    time = expr(m, unreal.MaterialExpressionScalarParameter, -1400, 120)
    time.set_editor_property("parameter_name", "WaveTime")

    defaults = [
        ((0.8, 0.6, 6000.0, 18.0), 0.35),
        ((0.6, 0.8, 3100.0, 9.0), 0.35),
        ((1.0, -0.15, 1700.0, 5.0), 0.35),
        ((-0.3, 0.95, 900.0, 2.5), 0.35),
    ]
    wave_params = []
    steep_params = []
    for i, (vec, steep) in enumerate(defaults):
        w = expr(m, unreal.MaterialExpressionVectorParameter, -1400, 240 + i * 180)
        w.set_editor_property("parameter_name", f"Wave{i}")
        w.set_editor_property("default_value", unreal.LinearColor(*vec))
        s = expr(m, unreal.MaterialExpressionScalarParameter, -1400, 320 + i * 180)
        s.set_editor_property("parameter_name", f"Steepness{i}")
        s.set_editor_property("default_value", steep)
        wave_params.append(w)
        steep_params.append(s)

    inputs = ["WP", "T", "W0", "W1", "W2", "W3", "S0", "S1", "S2", "S3"]

    def wire(node):
        connect(wp, "", node, "WP")
        connect(time, "", node, "T")
        for i in range(4):
            connect(wave_params[i], "", node, f"W{i}")
            connect(steep_params[i], "", node, f"S{i}")

    wpo = custom(m, -900, 0, OCEAN_WPO_HLSL, inputs, unreal.CustomMaterialOutputType.CMOT_FLOAT3, "GerstnerOffset")
    wire(wpo)
    to_property(wpo, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

    normal = custom(m, -900, 400, OCEAN_NORMAL_HLSL, inputs, unreal.CustomMaterialOutputType.CMOT_FLOAT3, "GerstnerNormal")
    wire(normal)
    to_property(normal, "", unreal.MaterialProperty.MP_NORMAL)

    base = expr(m, unreal.MaterialExpressionVectorParameter, -400, -300)
    base.set_editor_property("parameter_name", "BaseColor")
    base.set_editor_property("default_value", unreal.LinearColor(0.02, 0.12, 0.16, 1.0))
    to_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)

    rough = expr(m, unreal.MaterialExpressionConstant, -400, -150)
    rough.set_editor_property("r", 0.04)
    to_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    output = expr(m, unreal.MaterialExpressionSingleLayerWaterMaterialOutput, -100, 600)
    scattering = expr(m, unreal.MaterialExpressionVectorParameter, -400, 500)
    scattering.set_editor_property("parameter_name", "Scattering")
    scattering.set_editor_property("default_value", unreal.LinearColor(0.012, 0.07, 0.075, 1.0))
    absorption = expr(m, unreal.MaterialExpressionVectorParameter, -400, 650)
    absorption.set_editor_property("parameter_name", "Absorption")
    absorption.set_editor_property("default_value", unreal.LinearColor(0.42, 0.075, 0.05, 1.0))
    phase = expr(m, unreal.MaterialExpressionConstant, -400, 800)
    phase.set_editor_property("r", 0.35)
    connect(scattering, "", output, "ScatteringCoefficients")
    connect(absorption, "", output, "AbsorptionCoefficients")
    connect(phase, "", output, "PhaseG")
    finish(m)


# ---------------------------------------------------------------------------
# Estrellas
# ---------------------------------------------------------------------------

STARS_HLSL = r"""
float3 dir = normalize(-CV);
// Rotación lenta del firmamento.
float a = T * 0.002;
float2 xy = float2(dir.x * cos(a) - dir.y * sin(a), dir.x * sin(a) + dir.y * cos(a));
dir = float3(xy, dir.z);

float3 grid = dir * 220.0;
float3 cell = floor(grid);
float3 f = frac(grid) - 0.5;
float h = frac(sin(dot(cell, float3(12.9898, 78.233, 37.719))) * 43758.5453);
float h2 = frac(sin(dot(cell, float3(39.3468, 11.135, 83.155))) * 24634.6345);
float star = step(0.985, h) * smoothstep(0.32, 0.0, length(f + (h2 - 0.5) * 0.3));
float twinkle = 0.75 + 0.25 * sin(T * (2.0 + h2 * 4.0) + h * 40.0);
float3 tint = lerp(float3(0.75, 0.85, 1.0), float3(1.0, 0.9, 0.75), h2);

// Vía Láctea: banda difusa inclinada.
float band = exp(-pow(dot(dir, normalize(float3(0.3, 0.8, 0.52))) / 0.18, 2.0));
float dust = frac(sin(dot(floor(dir * 60.0), float3(7.1, 3.3, 5.7))) * 9751.3);
float3 milky = band * (0.35 + 0.65 * dust) * float3(0.55, 0.6, 0.8) * 0.18;

float horizon = smoothstep(-0.02, 0.15, dir.z);
return (star * twinkle * tint * (1.0 + 4.0 * h2) + milky) * horizon * Night * 6.0;
"""


def build_stars():
    m = recreate_material("M_Stars")
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("is_sky", True)

    cv = expr(m, unreal.MaterialExpressionCameraVectorWS, -800, 0)
    t = expr(m, unreal.MaterialExpressionTime, -800, 100)
    night = expr(m, unreal.MaterialExpressionScalarParameter, -800, 200)
    night.set_editor_property("parameter_name", "Night")
    night.set_editor_property("default_value", 0.0)

    body = custom(m, -450, 0, STARS_HLSL, ["CV", "T", "Night"], unreal.CustomMaterialOutputType.CMOT_FLOAT3, "Stars")
    connect(cv, "", body, "CV")
    connect(t, "", body, "T")
    connect(night, "", body, "Night")
    to_property(body, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)


def main():
    unreal.EditorAssetLibrary.make_directory(MATERIALS_PATH)
    build_terrain()
    build_ocean()
    build_stars()


main()
