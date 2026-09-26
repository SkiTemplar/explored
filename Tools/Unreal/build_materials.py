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
    """Reutiliza el material si existe (vaciando su grafo) para no romper referencias; si no, lo crea."""
    full = f"{MATERIALS_PATH}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(full):
        material = unreal.EditorAssetLibrary.load_asset(full)
        MEL.delete_all_material_expressions(material)
        return material
    material = ASSET_TOOLS.create_asset(name, MATERIALS_PATH, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        raise RuntimeError(f"No se pudo crear {full}")
    return material


def recreate_material_fresh(name: str) -> unreal.Material:
    """Borra el asset y lo crea desde cero. delete_all_material_expressions no limpia el nodo
    de salida especial de un material (p. ej. SingleLayerWaterMaterialOutput): reutilizar ese
    material deja dos salidas de agua y el shader no compila («solo puede haber un nodo Single
    Layer Water»), cayendo al Default Material sin avisar más que en el log de shaders."""
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

# Muestreo triplanar compartido por color y normal (metros del mundo).
TRIPLANAR_COMMON = r"""
float3 n = normalize(VN);
float3 w = pow(abs(n), 4.0);
w /= (w.x + w.y + w.z);
float3 p = WP / 100.0;
float s1 = 1.0 / 2.7;
float s2 = 1.0 / 11.0;
"""

TERRAIN_COLOR_HLSL = TRIPLANAR_COMMON + r"""
float4 d1 = Texture2DSample(Detail, DetailSampler, p.yz * s1) * w.x
          + Texture2DSample(Detail, DetailSampler, p.xz * s1) * w.y
          + Texture2DSample(Detail, DetailSampler, p.xy * s1) * w.z;
float4 d2 = Texture2DSample(Detail, DetailSampler, p.yz * s2) * w.x
          + Texture2DSample(Detail, DetailSampler, p.xz * s2) * w.y
          + Texture2DSample(Detail, DetailSampler, p.xy * s2) * w.z;
float4 macro = Texture2DSample(Detail, DetailSampler, p.xy / 70.0 + 0.37);

// Color por vértice en sRGB -> lineal.
float3 base = pow(saturate(VC.rgb), 2.2);
float rock = saturate(VC.a);

// Variación macro: manchas algo más cálidas o frías y más claras u oscuras.
float3 warm = base * float3(1.08, 1.02, 0.86);
float3 cool = base * float3(0.9, 1.0, 1.04);
float3 color = lerp(cool, warm, macro.r);
color *= lerp(0.82, 1.12, macro.g);

// Detalle: grano fino y medio; guijarros en suelo blando, vetas en roca.
color *= lerp(0.86, 1.1, d1.r) * lerp(0.9, 1.08, d2.g);
color *= lerp(lerp(0.9, 1.06, d1.b), lerp(0.78, 1.12, d2.a), rock);

// Cáusticas bajo el agua: dos rejillas distorsionadas que se desplazan y se cruzan,
// solo bajo el nivel del mar y más marcadas cerca de la superficie (agua somera).
float depthM = -min(WP.z / 100.0, 0.0);
if (depthM > 0.0)
{
    float2 cp = WP.xy / 220.0;
    float c1 = sin(cp.x * 2.4 + sin(cp.y * 1.7 + Time * 0.35) * 1.6 + Time * 0.6);
    float c2 = sin(cp.y * 2.1 - sin(cp.x * 1.9 - Time * 0.28) * 1.6 - Time * 0.5);
    float caustics = saturate(c1 * c2) * exp(-depthM / 9.0);
    color += caustics * 0.22 * float3(0.7, 0.95, 0.9);
}
return color;
"""

TERRAIN_NORMAL_HLSL = TRIPLANAR_COMMON + r"""
float2 nx = Texture2DSample(NormalTex, NormalTexSampler, p.yz * s1).rg * 2.0 - 1.0;
float2 ny = Texture2DSample(NormalTex, NormalTexSampler, p.xz * s1).rg * 2.0 - 1.0;
float2 nz = Texture2DSample(NormalTex, NormalTexSampler, p.xy * s1).rg * 2.0 - 1.0;
float2 mz = Texture2DSample(NormalTex, NormalTexSampler, p.xy * s2).rg * 2.0 - 1.0;
float strength = lerp(0.45, 1.1, saturate(Rock));
// Perturbación aproximada por eje de proyección (estilo «whiteout» simplificado).
float3 perturb = float3(0.0, nx.x, nx.y) * w.x
               + float3(ny.x, 0.0, ny.y) * w.y
               + float3(nz.x + mz.x * 0.6, nz.y + mz.y * 0.6, 0.0) * w.z;
return normalize(n + perturb * strength);
"""


def texture_object(material, path: str, x: int, y: int):
    node = expr(material, unreal.MaterialExpressionTextureObject, x, y)
    texture = unreal.EditorAssetLibrary.load_asset(path)
    if texture is None:
        raise RuntimeError(f"Falta la textura {path}; ejecuta Tools/Unreal/import_textures.py")
    node.set_editor_property("texture", texture)
    return node


def build_terrain():
    m = recreate_material("M_Terrain")
    m.set_editor_property("tangent_space_normal", False)

    vc = expr(m, unreal.MaterialExpressionVertexColor, -1100, 0)
    wp = expr(m, unreal.MaterialExpressionWorldPosition, -1100, 200)
    vn = expr(m, unreal.MaterialExpressionVertexNormalWS, -1100, 300)
    detail = texture_object(m, "/Game/Generated/Textures/T_TerrainDetail", -1100, 420)
    normal_tex = texture_object(m, "/Game/Generated/Textures/T_TerrainNormal", -1100, 620)
    time = expr(m, unreal.MaterialExpressionTime, -1100, 780)

    append = expr(m, unreal.MaterialExpressionAppendVector, -850, 0)
    connect(vc, "", append, "A")
    connect(vc, "A", append, "B")

    color = custom(m, -500, 0, TERRAIN_COLOR_HLSL, ["VC", "WP", "VN", "Detail", "Time"],
                   unreal.CustomMaterialOutputType.CMOT_FLOAT3, "TerrainColor")
    connect(append, "", color, "VC")
    connect(wp, "", color, "WP")
    connect(vn, "", color, "VN")
    connect(detail, "", color, "Detail")
    connect(time, "", color, "Time")
    to_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)

    normal = custom(m, -500, 400, TERRAIN_NORMAL_HLSL, ["WP", "VN", "NormalTex", "Rock"],
                    unreal.CustomMaterialOutputType.CMOT_FLOAT3, "TerrainNormal")
    connect(wp, "", normal, "WP")
    connect(vn, "", normal, "VN")
    connect(normal_tex, "", normal, "NormalTex")
    connect(vc, "A", normal, "Rock")
    to_property(normal, "", unreal.MaterialProperty.MP_NORMAL)

    # Arena mojada junto a la orilla (mismo umbral que FTerrainDensity::SurfaceColor): brillo sutil.
    rough = expr(m, unreal.MaterialExpressionLinearInterpolate, -300, 700)
    c_soft = expr(m, unreal.MaterialExpressionConstant, -500, 700)
    c_soft.set_editor_property("r", 0.82)
    c_rock = expr(m, unreal.MaterialExpressionConstant, -500, 780)
    c_rock.set_editor_property("r", 0.72)
    connect(c_soft, "", rough, "A")
    connect(c_rock, "", rough, "B")
    connect(vc, "A", rough, "Alpha")

    wet = custom(m, -300, 900, "return saturate(1.0 - abs(WP.z) / 70.0);",
                ["WP"], unreal.CustomMaterialOutputType.CMOT_FLOAT1, "WetSand")
    connect(wp, "", wet, "WP")
    rough_wet = expr(m, unreal.MaterialExpressionLinearInterpolate, -100, 750)
    connect(rough, "", rough_wet, "A")
    wet_value = expr(m, unreal.MaterialExpressionConstant, -300, 980)
    wet_value.set_editor_property("r", 0.35)
    connect(wet_value, "", rough_wet, "B")
    connect(wet, "", rough_wet, "Alpha")
    to_property(rough_wet, "", unreal.MaterialProperty.MP_ROUGHNESS)

    spec = expr(m, unreal.MaterialExpressionConstant, -300, 1080)
    spec.set_editor_property("r", 0.35)
    to_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    finish(m)


# ---------------------------------------------------------------------------
# Océano — agua cartoon a lo Tortunabo: bandas de color por profundidad real
# (laguna turquesa -> arrecife azul -> talud azul profundo), espuma de orilla
# que avanza y se retira con las olas, estelas de espuma en las crestas de
# Gerstner y líneas de brillo. Sigue siendo Single Layer Water (cáusticas y
# niebla bajo el agua las da el propio SLW con Scattering/Absorption reales).
# ---------------------------------------------------------------------------

# Debe coincidir con FOceanWaves::Displacement (cm, s).
GERSTNER_COMMON = r"""
float4 waves[4] = { W0, W1, W2, W3 };
float steep[4] = { S0, S1, S2, S3 };
float2 pos = WP.xy;
float3 offset = 0;
float3 tangent = float3(1, 0, 0);
float3 binormal = float3(0, 1, 0);
float sumAmp = 0.0001;
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
    sumAmp += a;
}
"""

OCEAN_WPO_HLSL = GERSTNER_COMMON + "return offset;"

OCEAN_NORMAL_HLSL = GERSTNER_COMMON + r"""
float3 n = normalize(cross(tangent, binormal));
// Detalle fino: tres capas de la misma textura de oleaje a escalas y velocidades distintas.
float2 uv = pos / 600.0;
float3 r1 = Texture2DSample(Ripple, RippleSampler, uv + T * float2(0.010, 0.004)).xyz * 2.0 - 1.0;
float3 r2 = Texture2DSample(Ripple, RippleSampler, uv * 2.3 - T * float2(0.007, 0.011)).xyz * 2.0 - 1.0;
float3 r3 = Texture2DSample(Ripple, RippleSampler, uv * 0.35 + T * float2(0.003, -0.002)).xyz * 2.0 - 1.0;
float2 detail = (r1.xy + 0.5 * r2.xy + 0.7 * r3.xy) * RippleStrength;
return normalize(n + float3(detail, 0.0));
"""

# Espuma de cresta: donde la ola de Gerstner sube más, whitecaps que crecen con el mar de fondo.
OCEAN_CREST_HLSL = GERSTNER_COMMON + r"""
float crest = smoothstep(0.45, 0.9, offset.z / (sumAmp * 0.55));
return saturate(crest) * saturate(SeaState * 1.4);
"""

# Espuma de orilla (late con las olas: avanza y se retira) y líneas de brillo que se desplazan.
# D = profundidad real del agua (SceneDepth - PixelDepth): 0 en la orilla, crece mar adentro.
OCEAN_FOAM_HLSL = r"""
float2 uv = P.xy / 900.0;
float n1 = Texture2DSample(Foam, FoamSampler, uv + T * float2(0.010, 0.004)).r;
float n2 = Texture2DSample(Foam, FoamSampler, uv * 1.9 - T * float2(0.006, 0.012)).r;
float pattern = saturate(n1 * 0.6 + n2 * 0.4);
float wave = 0.5 + 0.5 * sin(T * 6.2831853 / 5.0);
float band = (35.0 + 85.0 * wave) * (0.6 + 0.6 * SeaState);
float shore = 1.0 - saturate(D / max(band, 1.0));
float foam = saturate(shore * shore * (0.45 + 0.7 * pattern));
float glint = step(0.965, sin(dot(P.xy, float2(0.004, 0.0027)) + T * 0.9) * (0.55 + 0.45 * pattern));
return float2(foam, glint * (0.5 + 0.5 * SeaState));
"""


def build_ocean():
    # Single Layer Water compila como material opaco: SceneDepth/PixelDepth solo se pueden leer
    # en materiales translúcidos o de postproceso («Only transparent or postprocess materials
    # can read from scene depth»), así que la profundidad real de agua no vale aquí. Además,
    # reutilizar el material (recreate_material) no borra el nodo de salida de Single Layer
    # Water: con uno viejo y otro nuevo el shader no compila («solo puede haber un nodo Single
    # Layer Water») y cae al Default Material sin más aviso que una línea en el log de shaders
    # — exactamente lo que pasó aquí. Se recrea desde cero y la profundidad se aproxima con
    # DistanceToNearestSurface (campo de distancia del terreno, válido en material opaco).
    m = recreate_material_fresh("M_Ocean")
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_SINGLE_LAYER_WATER)
    m.set_editor_property("tangent_space_normal", False)
    m.set_editor_property("two_sided", False)

    wp = expr(m, unreal.MaterialExpressionWorldPosition, -1600, 0)
    # Se usa la posición sin desplazar para evaluar las olas.
    wp.set_editor_property("world_position_shader_offset", unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
    time = expr(m, unreal.MaterialExpressionScalarParameter, -1600, 120)
    time.set_editor_property("parameter_name", "WaveTime")
    sea_state = expr(m, unreal.MaterialExpressionScalarParameter, -1600, 200)
    sea_state.set_editor_property("parameter_name", "SeaState")
    sea_state.set_editor_property("default_value", 0.15)

    defaults = [
        ((0.8, 0.6, 6000.0, 18.0), 0.35),
        ((0.6, 0.8, 3100.0, 9.0), 0.35),
        ((1.0, -0.15, 1700.0, 5.0), 0.35),
        ((-0.3, 0.95, 900.0, 2.5), 0.35),
    ]
    wave_params = []
    steep_params = []
    for i, (vec, steep) in enumerate(defaults):
        w = expr(m, unreal.MaterialExpressionVectorParameter, -1600, 300 + i * 180)
        w.set_editor_property("parameter_name", f"Wave{i}")
        w.set_editor_property("default_value", unreal.LinearColor(*vec))
        s = expr(m, unreal.MaterialExpressionScalarParameter, -1600, 380 + i * 180)
        s.set_editor_property("parameter_name", f"Steepness{i}")
        s.set_editor_property("default_value", steep)
        # El parámetro vectorial sale como float3; se añade el alfa para tener float4.
        rgba = expr(m, unreal.MaterialExpressionAppendVector, -1400, 300 + i * 180)
        connect(w, "", rgba, "A")
        connect(w, "A", rgba, "B")
        wave_params.append(rgba)
        steep_params.append(s)

    gerstner_inputs = ["WP", "T", "W0", "W1", "W2", "W3", "S0", "S1", "S2", "S3"]

    def wire_gerstner(node):
        connect(wp, "", node, "WP")
        connect(time, "", node, "T")
        for i in range(4):
            connect(wave_params[i], "", node, f"W{i}")
            connect(steep_params[i], "", node, f"S{i}")

    wpo = custom(m, -1000, 0, OCEAN_WPO_HLSL, gerstner_inputs, unreal.CustomMaterialOutputType.CMOT_FLOAT3, "GerstnerOffset")
    wire_gerstner(wpo)
    to_property(wpo, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

    ripple_tex = texture_object(m, "/Game/Generated/Textures/T_WaterRipple", -1600, 1200)
    ripple_strength = expr(m, unreal.MaterialExpressionScalarParameter, -1600, 1320)
    ripple_strength.set_editor_property("parameter_name", "RippleStrength")
    ripple_strength.set_editor_property("default_value", 0.6)

    normal = custom(m, -1000, 500, OCEAN_NORMAL_HLSL, gerstner_inputs + ["Ripple", "RippleStrength"],
                    unreal.CustomMaterialOutputType.CMOT_FLOAT3, "GerstnerNormal")
    wire_gerstner(normal)
    connect(ripple_tex, "", normal, "Ripple")
    connect(ripple_strength, "", normal, "RippleStrength")
    to_property(normal, "", unreal.MaterialProperty.MP_NORMAL)

    crest = custom(m, -1000, 950, OCEAN_CREST_HLSL, gerstner_inputs + ["SeaState"],
                   unreal.CustomMaterialOutputType.CMOT_FLOAT1, "CrestFoam")
    wire_gerstner(crest)
    connect(sea_state, "", crest, "SeaState")

    # Aproximación de la profundidad de agua: distancia (cm) al campo de distancia del terreno
    # más cercano desde la posición de la propia superficie. Cerca de la orilla esa distancia
    # es pequeña (el fondo está justo debajo); mar adentro crece con la profundidad real.
    foam_tex = texture_object(m, "/Game/Generated/Textures/T_WaterFoam", -1600, 1450)
    water_depth = expr(m, unreal.MaterialExpressionDistanceToNearestSurface, -1000, 1450)

    foam_glint = custom(m, -600, 1450, OCEAN_FOAM_HLSL, ["D", "P", "T", "SeaState", "Foam"],
                        unreal.CustomMaterialOutputType.CMOT_FLOAT2, "ShoreFoam")
    connect(water_depth, "", foam_glint, "D")
    connect(wp, "", foam_glint, "P")
    connect(time, "", foam_glint, "T")
    connect(sea_state, "", foam_glint, "SeaState")
    connect(foam_tex, "", foam_glint, "Foam")

    foam_mask = expr(m, unreal.MaterialExpressionComponentMask, -350, 1400)
    for channel in ("r", "g", "b", "a"):
        foam_mask.set_editor_property(channel, channel == "r")
    connect(foam_glint, "", foam_mask, "")
    glint_mask = expr(m, unreal.MaterialExpressionComponentMask, -350, 1500)
    for channel in ("r", "g", "b", "a"):
        glint_mask.set_editor_property(channel, channel == "g")
    connect(foam_glint, "", glint_mask, "")

    total_foam = expr(m, unreal.MaterialExpressionMax, -150, 1350)
    connect(foam_mask, "", total_foam, "A")
    connect(crest, "", total_foam, "B")

    # Factores de mezcla por profundidad: 0 en la laguna somera, 1 en el talud profundo.
    t1 = custom(m, -800, 300, "return saturate((D - 100.0) / 250.0);", ["D"],
               unreal.CustomMaterialOutputType.CMOT_FLOAT1, "ShallowToMid")
    connect(water_depth, "", t1, "D")
    t2 = custom(m, -800, 400, "return saturate((D - 900.0) / 900.0);", ["D"],
               unreal.CustomMaterialOutputType.CMOT_FLOAT1, "MidToDeep")
    connect(water_depth, "", t2, "D")

    # Color por profundidad: laguna turquesa -> arrecife azul -> talud azul profundo.
    def blend3(name, color_a, color_b, color_c, x, y):
        pa = expr(m, unreal.MaterialExpressionVectorParameter, x, y)
        pa.set_editor_property("parameter_name", f"{name}Shallow")
        pa.set_editor_property("default_value", unreal.LinearColor(*color_a))
        pb = expr(m, unreal.MaterialExpressionVectorParameter, x, y + 90)
        pb.set_editor_property("parameter_name", f"{name}Mid")
        pb.set_editor_property("default_value", unreal.LinearColor(*color_b))
        pc = expr(m, unreal.MaterialExpressionVectorParameter, x, y + 180)
        pc.set_editor_property("parameter_name", f"{name}Deep")
        pc.set_editor_property("default_value", unreal.LinearColor(*color_c))
        lerp1 = expr(m, unreal.MaterialExpressionLinearInterpolate, x + 260, y)
        connect(pa, "", lerp1, "A")
        connect(pb, "", lerp1, "B")
        connect(t1, "", lerp1, "Alpha")
        lerp2 = expr(m, unreal.MaterialExpressionLinearInterpolate, x + 420, y + 45)
        connect(lerp1, "", lerp2, "A")
        connect(pc, "", lerp2, "B")
        connect(t2, "", lerp2, "Alpha")
        return lerp2

    base_blend = blend3("BaseColor", (0.30, 0.86, 0.80), (0.05, 0.45, 0.62), (0.02, 0.10, 0.22), -700, -450)
    scatter_blend = blend3("Scattering", (0.02, 0.14, 0.13), (0.012, 0.07, 0.075), (0.004, 0.03, 0.045), -700, 100)
    absorb_blend = blend3("Absorption", (0.20, 0.05, 0.02), (0.42, 0.075, 0.05), (0.65, 0.20, 0.10), -700, 700)

    foam_color = expr(m, unreal.MaterialExpressionVectorParameter, -150, -300)
    foam_color.set_editor_property("parameter_name", "FoamColor")
    foam_color.set_editor_property("default_value", unreal.LinearColor(0.94, 0.97, 0.98, 1.0))
    glint_color = expr(m, unreal.MaterialExpressionVectorParameter, -150, -180)
    glint_color.set_editor_property("parameter_name", "GlintColor")
    glint_color.set_editor_property("default_value", unreal.LinearColor(0.85, 0.95, 1.0, 1.0))

    base_with_foam = expr(m, unreal.MaterialExpressionLinearInterpolate, 100, -350)
    connect(base_blend, "", base_with_foam, "A")
    connect(foam_color, "", base_with_foam, "B")
    connect(total_foam, "", base_with_foam, "Alpha")
    to_property(base_with_foam, "", unreal.MaterialProperty.MP_BASE_COLOR)

    glint_amount = expr(m, unreal.MaterialExpressionMultiply, -150, -60)
    connect(glint_mask, "", glint_amount, "A")
    glint_amount.set_editor_property("const_b", 0.6)
    glint_emissive = expr(m, unreal.MaterialExpressionMultiply, 100, -180)
    connect(glint_color, "", glint_emissive, "A")
    connect(glint_amount, "", glint_emissive, "B")
    to_property(glint_emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    rough_calm = expr(m, unreal.MaterialExpressionConstant, -150, 20)
    rough_calm.set_editor_property("r", 0.05)
    rough_foam = expr(m, unreal.MaterialExpressionConstant, -150, 60)
    rough_foam.set_editor_property("r", 0.6)
    rough = expr(m, unreal.MaterialExpressionLinearInterpolate, 100, 40)
    connect(rough_calm, "", rough, "A")
    connect(rough_foam, "", rough, "B")
    connect(total_foam, "", rough, "Alpha")
    to_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    output = expr(m, unreal.MaterialExpressionSingleLayerWaterMaterialOutput, 300, 400)
    connect(scatter_blend, "", output, "ScatteringCoefficients")
    connect(absorb_blend, "", output, "AbsorptionCoefficients")
    phase = expr(m, unreal.MaterialExpressionConstant, 100, 500)
    phase.set_editor_property("r", 0.4)
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
float dust = 0.5 + 0.5 * sin(dir.x * 23.0 + sin(dir.y * 17.0) * 2.0) * sin(dir.y * 19.0 + dir.z * 13.0 + sin(dir.x * 31.0));
float3 milky = band * (0.3 + 0.7 * dust * dust) * float3(0.55, 0.6, 0.8) * 0.06;

float horizon = smoothstep(-0.02, 0.15, dir.z);
return (star * twinkle * tint * (1.0 + 4.0 * h2) + milky) * horizon * Night * 6.0;
"""


def build_stars():
    m = recreate_material("M_Stars")
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("two_sided", True)
    # Cúpula translúcida aditiva normal: un material «de cielo» debe cubrir toda la pantalla.

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


# ---------------------------------------------------------------------------
# Vegetación y rocas (se reconstruyen en su sitio para conservar las referencias)
# ---------------------------------------------------------------------------

GENERATED_MATERIALS = "/Game/Generated/Materials"

WIND_HLSL = r"""
// Viento: balanceo lento global + ráfagas + aleteo fino, modulado por la máscara (alfa del color de vértice).
float3 p = WP / 100.0;
float phase = dot(p.xy, float2(0.07, 0.05));
float sway = sin(T * 0.9 + phase) * 0.6 + sin(T * 1.7 + phase * 1.9) * 0.25;
float gust = saturate(sin(T * 0.23 + p.x * 0.004) * 0.5 + 0.5);
float flutter = sin(T * 7.0 + dot(p, float3(3.1, 2.3, 4.7))) * 0.15;
float amount = Mask * Mask * Strength * (0.6 + gust * 0.8);
float3 dir = normalize(float3(0.8, 0.45, 0.0));
return (dir * (sway + flutter) + float3(0, 0, -abs(sway) * 0.25)) * amount;
"""

FOLIAGE_COLOR_HLSL = r"""
// El color de vértice del kit de vegetación sale de Blender ya en espacio lineal
// (Tools/Blender/lib/common.py: export_fbx usa colors_type='LINEAR' precisamente para
// no tener que reconvertir). A diferencia del terreno (generado en C++ con paleta en
// sRGB, que sí necesita pow(VC,2.2)), aplicar aquí la misma corrección de gamma sobre un
// valor que YA es lineal lo oscurece dos veces: un verde razonable (p. ej. 0.3, 0.6, 0.15)
// cae a (0.07, 0.31, 0.01), casi negro. Se usa el color de vértice tal cual.
float3 base = saturate(VC);
// Tinte por instancia: unas plantas algo más amarillas y otras más oscuras.
float3 tintA = float3(1.10, 1.04, 0.80);
float3 tintB = float3(0.82, 0.95, 0.90);
float3 color = base * lerp(tintA, tintB, R) * lerp(0.85, 1.1, frac(R * 7.31));
color *= lerp(0.88, 1.08, Noise.r) * lerp(0.94, 1.04, Noise.b);
return color;
"""


def rebuild_material(path: str, name: str) -> unreal.Material:
    """Reutiliza el material si existe (vaciando su grafo); si no, lo crea."""
    full = f"{path}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(full):
        material = unreal.EditorAssetLibrary.load_asset(full)
        MEL.delete_all_material_expressions(material)
        return material
    unreal.EditorAssetLibrary.make_directory(path)
    return ASSET_TOOLS.create_asset(name, path, unreal.Material, unreal.MaterialFactoryNew())


def build_foliage(name: str, wind_strength: float, two_sided_foliage: bool):
    m = rebuild_material(GENERATED_MATERIALS, name)
    if two_sided_foliage:
        m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
        m.set_editor_property("two_sided", True)
    else:
        m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
        m.set_editor_property("two_sided", False)

    vc = expr(m, unreal.MaterialExpressionVertexColor, -1100, 0)
    rnd = expr(m, unreal.MaterialExpressionPerInstanceRandom, -1100, 150)
    uv = expr(m, unreal.MaterialExpressionTextureCoordinate, -1300, 300)
    noise_tex = expr(m, unreal.MaterialExpressionTextureSample, -1100, 300)
    noise_tex.set_editor_property("texture", unreal.EditorAssetLibrary.load_asset("/Game/Generated/Textures/T_LeafNoise"))
    noise_tex.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    connect(uv, "", noise_tex, "UVs")

    color = custom(m, -700, 0, FOLIAGE_COLOR_HLSL, ["VC", "R", "Noise"], unreal.CustomMaterialOutputType.CMOT_FLOAT3, "FoliageColor")
    connect(vc, "", color, "VC")
    connect(rnd, "", color, "R")
    connect(noise_tex, "RGBA", color, "Noise")
    to_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)

    if two_sided_foliage:
        sss = expr(m, unreal.MaterialExpressionMultiply, -400, 200)
        k = expr(m, unreal.MaterialExpressionConstant3Vector, -600, 250)
        k.set_editor_property("constant", unreal.LinearColor(0.9, 1.0, 0.45, 1.0))
        connect(color, "", sss, "A")
        connect(k, "", sss, "B")
        to_property(sss, "", unreal.MaterialProperty.MP_SUBSURFACE_COLOR)

    rough = expr(m, unreal.MaterialExpressionConstant, -400, 350)
    rough.set_editor_property("r", 0.62 if two_sided_foliage else 0.88)
    to_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    # Borde de luz sutil (rim/fresnel): remarca la silueta contra el cielo, cálido y discreto.
    fresnel = expr(m, unreal.MaterialExpressionFresnel, -700, 550)
    fresnel.set_editor_property("exponent", 2.6)
    fresnel.set_editor_property("base_reflect_fraction", 0.02)
    rim_tint = expr(m, unreal.MaterialExpressionConstant3Vector, -700, 650)
    rim_tint.set_editor_property("constant", unreal.LinearColor(1.0, 0.86, 0.6, 1.0))
    rim_amount = expr(m, unreal.MaterialExpressionScalarParameter, -700, 750)
    rim_amount.set_editor_property("parameter_name", "RimIntensity")
    rim_amount.set_editor_property("default_value", 0.22)
    rim_colored = expr(m, unreal.MaterialExpressionMultiply, -300, 650)
    connect(fresnel, "", rim_colored, "A")
    connect(rim_tint, "", rim_colored, "B")
    rim_scaled = expr(m, unreal.MaterialExpressionMultiply, -150, 650)
    connect(rim_colored, "", rim_scaled, "A")
    connect(rim_amount, "", rim_scaled, "B")
    to_property(rim_scaled, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    if wind_strength > 0.0:
        wp = expr(m, unreal.MaterialExpressionWorldPosition, -1100, 500)
        t = expr(m, unreal.MaterialExpressionTime, -1100, 600)
        strength = expr(m, unreal.MaterialExpressionScalarParameter, -1100, 700)
        strength.set_editor_property("parameter_name", "WindStrength")
        strength.set_editor_property("default_value", wind_strength)
        wind = custom(m, -700, 500, WIND_HLSL, ["WP", "T", "Mask", "Strength"], unreal.CustomMaterialOutputType.CMOT_FLOAT3, "Wind")
        connect(wp, "", wind, "WP")
        connect(t, "", wind, "T")
        connect(vc, "A", wind, "Mask")
        connect(strength, "", wind, "Strength")
        to_property(wind, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    finish(m)


def build_rock():
    m = rebuild_material(GENERATED_MATERIALS, "M_Rock")
    m.set_editor_property("tangent_space_normal", False)
    vc = expr(m, unreal.MaterialExpressionVertexColor, -1100, 0)
    wp = expr(m, unreal.MaterialExpressionWorldPosition, -1100, 200)
    vn = expr(m, unreal.MaterialExpressionVertexNormalWS, -1100, 300)
    detail = texture_object(m, "/Game/Generated/Textures/T_TerrainDetail", -1100, 420)
    normal_tex = texture_object(m, "/Game/Generated/Textures/T_TerrainNormal", -1100, 620)
    time = expr(m, unreal.MaterialExpressionTime, -1100, 780)
    one = expr(m, unreal.MaterialExpressionConstant, -1100, 800)
    one.set_editor_property("r", 1.0)

    append = expr(m, unreal.MaterialExpressionAppendVector, -850, 0)
    connect(vc, "", append, "A")
    connect(one, "", append, "B")  # Alfa = 1: se trata todo como roca.

    color = custom(m, -500, 0, TERRAIN_COLOR_HLSL, ["VC", "WP", "VN", "Detail", "Time"], unreal.CustomMaterialOutputType.CMOT_FLOAT3, "RockColor")
    connect(append, "", color, "VC")
    connect(wp, "", color, "WP")
    connect(vn, "", color, "VN")
    connect(detail, "", color, "Detail")
    connect(time, "", color, "Time")
    to_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)

    normal = custom(m, -500, 400, TERRAIN_NORMAL_HLSL, ["WP", "VN", "NormalTex", "Rock"], unreal.CustomMaterialOutputType.CMOT_FLOAT3, "RockNormal")
    connect(wp, "", normal, "WP")
    connect(vn, "", normal, "VN")
    connect(normal_tex, "", normal, "NormalTex")
    connect(one, "", normal, "Rock")
    to_property(normal, "", unreal.MaterialProperty.MP_NORMAL)

    rough = expr(m, unreal.MaterialExpressionConstant, -300, 700)
    rough.set_editor_property("r", 0.8)
    to_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    finish(m)


def build_vegetation_materials():
    build_foliage("M_Leaf", wind_strength=0.35, two_sided_foliage=True)
    build_foliage("M_Grass", wind_strength=0.25, two_sided_foliage=True)
    build_foliage("M_Bark", wind_strength=0.0, two_sided_foliage=False)
    build_rock()


def main():
    unreal.EditorAssetLibrary.make_directory(MATERIALS_PATH)
    build_terrain()
    build_ocean()
    build_stars()
    build_vegetation_materials()


main()
