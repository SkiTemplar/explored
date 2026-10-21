"""Crea los materiales base del juego por código (idempotente).

Se ejecuta con:
    powershell -File Tools/unreal_python.ps1 -Script Tools/Unreal/build_materials.py

Materiales:
    /Game/Materials/M_Ocean    Single Layer Water con olas de Gerstner (mismas que FOceanWaves).
    /Game/Materials/M_Stars    cúpula de estrellas aditiva controlada por el parámetro «Night».
    /Game/Materials/M_PP_Body  postproceso del cuerpo: viñeta (color y fuerza) y desaturación
                                según FBodySignals (VignetteAmount, TintColor, DesaturationAmount).
    /Game/Art/Materials/T_Paleta  hoja de paleta de Tools/Style (Art/Export/Style/T_Paleta.png, 32 x 1):
                                filtro Nearest, sin mipmaps, sRGB, compresión sin pérdidas.
    /Game/Art/Materials/M_Toon  material provisional del estilo (F3a): Default Lit, base color = T_Paleta
                                muestreada con el UV 0 (cada cara en el centro de su celda), rugosidad 1,
                                especular 0. «PaletteIndex» + «UsePaletteIndex» = 1 fuerzan una celda
                                (primitivas de reserva de AExploredPlaceholder). La integración de arte lo
                                refina (3 bandas de sombreado, GDD §11).

El terreno, la vegetación y los materiales low poly del arte v1 se borraron en el reset v2;
el material maestro nuevo (M_Toon con hoja de paleta) llega en F2 (docs/v3/plan.md).
"""

import os

import unreal

MATERIALS_PATH = "/Game/Materials"
ART_MATERIALS_PATH = "/Game/Art/Materials"
REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
PALETTE_PNG = os.path.join(REPO_ROOT, "Art", "Export", "Style", "T_Paleta.png")
PALETTE_SIZE = 32
MEL = unreal.MaterialEditingLibrary
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
# Las texturas de detalle del agua (T_WaterRipple, T_WaterFoam) eran arte v1: se usan texturas
# neutras del motor (normal plana y sin espuma) hasta que el estilo nuevo defina las suyas.
OCEAN_RIPPLE_TEXTURE = "/Engine/EngineMaterials/DefaultNormal"
OCEAN_FOAM_TEXTURE = "/Engine/EngineResources/Black"


def recreate_material(name: str, folder: str = MATERIALS_PATH) -> unreal.Material:
    """Reutiliza el material si existe (vaciando su grafo) para no romper referencias; si no, lo crea."""
    full = f"{folder}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(full):
        material = unreal.EditorAssetLibrary.load_asset(full)
        MEL.delete_all_material_expressions(material)
        return material
    material = ASSET_TOOLS.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
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


MESH_USAGES = (
    # Terreno, árboles y rocas usan Nanite: sin este flag el juego (no el editor) sustituye el
    # material por el Default Material y lo avisa solo en el log («missing bUsedWithNanite»).
    unreal.MaterialUsage.MATUSAGE_NANITE,
    # La vegetación dispersa se dibuja con componentes instanciados.
    unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES,
)


def finish(material):
    # Los flags de uso (Nanite, instanciado) son de material de superficie: en un postproceso
    # (M_PP_Body) set_material_usage no aplica y solo ensuciaría el log.
    is_surface = material.get_editor_property("material_domain") == unreal.MaterialDomain.MD_SURFACE
    if is_surface and material.get_editor_property("blend_mode") == unreal.BlendMode.BLEND_OPAQUE:
        for usage in MESH_USAGES:
            MEL.set_material_usage(material, usage)
    MEL.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    unreal.log(f"[Explored] Material listo: {material.get_path_name()}")


# ---------------------------------------------------------------------------
# Terreno
# ---------------------------------------------------------------------------

def texture_object(material, path: str, x: int, y: int):
    node = expr(material, unreal.MaterialExpressionTextureObject, x, y)
    texture = unreal.load_asset(path)
    if texture is None:
        raise RuntimeError(f"Falta la textura {path}")
    node.set_editor_property("texture", texture)
    return node


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
# Además, subsuperficie de cresta: verde turquesa translúcido cuando la cara de la ola que da la
# espalda al sol queda entre la cámara y el sol (luz "wrap" clásica de translucencia barata).
# Sale en un único FLOAT4 para no repetir la suma de Gerstner en un segundo nodo Custom:
# .r = máscara de cresta (alimenta la espuma), .gba = color emisivo de subsuperficie ya ponderado.
OCEAN_CREST_HLSL = GERSTNER_COMMON + r"""
// La altura de cresta normalizada (offset.z / sumAmp) no depende de SeaState: la suma de las 4
// olas de Gerstner siempre tiene picos y valles aunque el mar esté en calma (SeaState baja solo
// encoge la amplitud real, no la forma relativa). Con «saturate(SeaState * 1.4)» como único freno,
// un SeaState de reposo (0.15) ya deja pasar un 21% de opacidad de espuma en cada pico — como las
// olas de fondo son solo 4 direcciones coherentes, esos picos se alinean en frentes anchos y se leen
// como bandas blancas paralelas por todo el océano, no solo cerca de la costa. El mar en calma real
// no hace whitecaps: se retrasa el arranque a partir de SeaState~0.3 (mar picado, Beaufort 3+) para
// que a 0.15 la máscara de cresta salga a 0 y solo dejen espuma visible la orilla y la marejada.
float crestShape = saturate(smoothstep(0.45, 0.9, offset.z / (sumAmp * 0.55)));
float crest = crestShape * saturate((SeaState - 0.3) * 3.0);

float3 n = normalize(cross(tangent, binormal));
float3 camDir = normalize(V);
float3 lightDir = normalize(L);
// Cámara y sol enfrentados a través de la cresta: dot(camDir, lightDir) cercano a -1.
float backlight = saturate(-dot(camDir, lightDir));
// Más fuerte en la cara de la ola que mira hacia el sol desde atrás (normal opuesta a la luz).
float facing = saturate(dot(n, lightDir) * -0.5 + 0.5);
// La translucidez a contraluz sí existe con mar de fondo (ola lisa, sin espuma): usa la forma de la
// cresta con el freno antiguo, no la máscara de espuma, que ahora vale 0 por debajo de SeaState 0.3.
float sss = crestShape * saturate(SeaState * 1.4) * backlight * facing;
float3 sssColor = sss * SubsurfaceColor * SubsurfaceStrength;
return float4(crest, sssColor);
"""

# Espuma de orilla (late con las olas: avanza y se retira) y líneas de brillo que se desplazan.
# D = aproximación de la profundidad de agua (DistanceToNearestSurface, ver build_ocean): 0 en la
# orilla, crece mar adentro. Es un campo de distancias por voxels (Global Distance Field), así que
# sus escalones se notan como un borde de costa con "dientes"; se dithera con un hash de P para que
# ese escalón deje de leerse como una línea recta y quede dentro del ruido del propio patrón de espuma.
OCEAN_FOAM_HLSL = r"""
float ditherFoam = frac(sin(dot(P.xy, float2(12.9898, 78.233))) * 43758.5453) - 0.5;
float Dd = max(D + ditherFoam * 60.0, 0.0);
float2 uv = P.xy / 900.0;
float n1 = Texture2DSample(Foam, FoamSampler, uv + T * float2(0.010, 0.004)).r;
float n2 = Texture2DSample(Foam, FoamSampler, uv * 1.9 - T * float2(0.006, 0.012)).r;
float pattern = saturate(n1 * 0.6 + n2 * 0.4);
float wave = 0.5 + 0.5 * sin(T * 6.2831853 / 5.0);
float band = (35.0 + 85.0 * wave) * (0.6 + 0.6 * SeaState);
float shore = 1.0 - saturate(Dd / max(band, 1.0));
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

    ripple_tex = texture_object(m, OCEAN_RIPPLE_TEXTURE, -1600, 1200)
    ripple_strength = expr(m, unreal.MaterialExpressionScalarParameter, -1600, 1320)
    ripple_strength.set_editor_property("parameter_name", "RippleStrength")
    ripple_strength.set_editor_property("default_value", 0.6)

    normal = custom(m, -1000, 500, OCEAN_NORMAL_HLSL, gerstner_inputs + ["Ripple", "RippleStrength"],
                    unreal.CustomMaterialOutputType.CMOT_FLOAT3, "GerstnerNormal")
    wire_gerstner(normal)
    connect(ripple_tex, "", normal, "Ripple")
    connect(ripple_strength, "", normal, "RippleStrength")
    to_property(normal, "", unreal.MaterialProperty.MP_NORMAL)

    # Sol real de la escena (mismo Directional Light que orbita en ExploredSkyController) y
    # dirección de cámara, para el retro-iluminado de la subsuperficie en OCEAN_CREST_HLSL.
    cam_vector = expr(m, unreal.MaterialExpressionCameraVectorWS, -1600, 900)
    light_dir = expr(m, unreal.MaterialExpressionSkyAtmosphereLightDirection, -1600, 1000)
    sss_color = expr(m, unreal.MaterialExpressionVectorParameter, -1600, 1100)
    sss_color.set_editor_property("parameter_name", "SubsurfaceColor")
    sss_color.set_editor_property("default_value", unreal.LinearColor(0.25, 0.95, 0.55, 1.0))
    sss_strength = expr(m, unreal.MaterialExpressionScalarParameter, -1600, 1180)
    sss_strength.set_editor_property("parameter_name", "SubsurfaceStrength")
    sss_strength.set_editor_property("default_value", 1.4)

    crest = custom(m, -1000, 950, OCEAN_CREST_HLSL,
                   gerstner_inputs + ["SeaState", "V", "L", "SubsurfaceColor", "SubsurfaceStrength"],
                   unreal.CustomMaterialOutputType.CMOT_FLOAT4, "CrestFoamAndSSS")
    wire_gerstner(crest)
    connect(sea_state, "", crest, "SeaState")
    connect(cam_vector, "", crest, "V")
    connect(light_dir, "", crest, "L")
    connect(sss_color, "", crest, "SubsurfaceColor")
    connect(sss_strength, "", crest, "SubsurfaceStrength")

    # ComponentMask no arranca en todo-False: por defecto trae los 4 canales a True, así que hay
    # que apagar explícitamente los que no tocan a cada máscara (no basta con encender los que sí).
    # Sin esto crest_mask y crest_sss salían los dos como float4 (r,g,b,a=True), y el Add de más
    # abajo (glint_emissive + crest_sss) exigía el mismo número de componentes en A y B: con
    # crest_sss a 4 canales y glint_emissive a 3, el material no compilaba («Arithmetic between
    # types float3 and float4 are undefined») y todo el océano caía al Default Material — la
    # superficie entera salía como un color plano sin agua.
    crest_mask = expr(m, unreal.MaterialExpressionComponentMask, -750, 900)
    for channel in ("r", "g", "b", "a"):
        crest_mask.set_editor_property(channel, channel == "r")
    connect(crest, "", crest_mask, "")
    crest_sss = expr(m, unreal.MaterialExpressionComponentMask, -750, 990)
    for channel in ("r", "g", "b", "a"):
        crest_sss.set_editor_property(channel, channel != "r")
    connect(crest, "", crest_sss, "")

    # Aproximación de la profundidad de agua: distancia (cm) al campo de distancia del terreno
    # más cercano desde la posición de la propia superficie. Cerca de la orilla esa distancia
    # es pequeña (el fondo está justo debajo); mar adentro crece con la profundidad real. Es un
    # campo por voxels (Global Distance Field): sin ditherar, sus escalones se leen como rayas
    # rectas paralelas en el color del agua. Se compensa en los propios nodos que la consumen.
    foam_tex = texture_object(m, OCEAN_FOAM_TEXTURE, -1600, 1450)
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
    connect(crest_mask, "", total_foam, "B")

    # Factores de mezcla por profundidad: 0 en la laguna somera, 1 en el talud profundo. Bandas
    # más anchas que antes y ditheradas con un hash de la posición para que el escalón del campo
    # de distancia (voxelado) no se lea como una raya recta repetida por toda la superficie.
    t1 = custom(m, -800, 300, r"""
float dith = frac(sin(dot(P.xy, float2(12.9898, 78.233))) * 43758.5453) - 0.5;
return saturate((D + dith * 220.0 - 100.0) / 450.0);
""", ["D", "P"], unreal.CustomMaterialOutputType.CMOT_FLOAT1, "ShallowToMid")
    connect(water_depth, "", t1, "D")
    connect(wp, "", t1, "P")
    t2 = custom(m, -800, 400, r"""
float dith = frac(sin(dot(P.xy, float2(39.3468, 11.135)) + 7.0) * 24634.6345) - 0.5;
return saturate((D + dith * 260.0 - 900.0) / 1400.0);
""", ["D", "P"], unreal.CustomMaterialOutputType.CMOT_FLOAT1, "MidToDeep")
    connect(water_depth, "", t2, "D")
    connect(wp, "", t2, "P")

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
    # Scattering + Absorption son la extinción real (1/cm) que usa el compositing de Single Layer
    # Water para el color transmitido del fondo: los valores previos (p. ej. Absorption R=0.20 en
    # la orilla) daban una longitud de atenuación de ~5 cm, así que el agua se volvía opaca a
    # centímetros de la orilla y el fondo dejaba de verse pase lo que pasara con BaseColor/D.
    # Se bajan un orden de magnitud en la franja somera (longitud de atenuación ~1-2 m: se ve la
    # arena de los bajíos) y se suavizan las otras dos franjas para una transición continua hasta
    # el talud, que sigue siendo opaco a pocos centímetros como corresponde al azul profundo.
    scatter_blend = blend3("Scattering", (0.002, 0.004, 0.004), (0.010, 0.014, 0.013), (0.020, 0.030, 0.035), -700, 100)
    absorb_blend = blend3("Absorption", (0.006, 0.0025, 0.0015), (0.020, 0.012, 0.007), (0.090, 0.050, 0.025), -700, 700)

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
    # El pin por defecto de un VectorParameter arrastra el alfa (float4) y el Add de más abajo
    # exige que A y B tengan el mismo número de componentes que crest_sss (float3, viene de un
    # ComponentMask a 3 canales). Sin este truncado a propósito el material no compilaba
    # («Arithmetic between types float3 and float4 are undefined») y todo el océano caía al
    # Default Material: la superficie entera salía como un color plano sin agua. VectorParameter
    # no tiene salida nombrada "RGB" (a diferencia de un TextureSample): hace falta un
    # ComponentMask explícito para truncar a 3 canales.
    glint_color_rgb = expr(m, unreal.MaterialExpressionComponentMask, -20, -180)
    for channel in ("r", "g", "b", "a"):
        glint_color_rgb.set_editor_property(channel, channel != "a")
    connect(glint_color, "", glint_color_rgb, "")

    glint_emissive = expr(m, unreal.MaterialExpressionMultiply, 100, -180)
    connect(glint_color_rgb, "", glint_emissive, "A")
    connect(glint_amount, "", glint_emissive, "B")

    # Brillo del sol (glint) + subsuperficie de cresta (crest_sss) comparten la emisiva.
    total_emissive = expr(m, unreal.MaterialExpressionAdd, 220, -60)
    connect(glint_emissive, "", total_emissive, "A")
    connect(crest_sss, "", total_emissive, "B")
    to_property(total_emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

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
# Postproceso del cuerpo (UBodySignalsComponent, docs/tecnico/cuerpo.md)
# ---------------------------------------------------------------------------

PP_BODY_HLSL = r"""
// Viñeta y desaturación de FBodySignals (docs/tecnico/cuerpo.md): sin iconos, el jugador
// diagnostica por sensaciones (GDD §8.3). UBodySignalsComponent::ApplyPostProcess fija
// BodyVignette, BodyVignetteTint y BodyDesaturation cada fotograma en el MID de esta
// instancia; aquí solo se declara el grafo con sus valores neutros (0 = pantalla limpia).
// BodyBlur, BodyBleedPulse, BodyHeartRateHz y BodyHallucination también los fija el
// componente pero no están conectados todavía (SetParameterValue sobre un nombre que el
// material no declara no falla: queda para un siguiente hito de postproceso).
float3 base = SceneColor;
float gray = dot(base, float3(0.299, 0.587, 0.114));
float3 desaturated = lerp(base, float3(gray, gray, gray), saturate(BodyDesaturation));

// Distancia al centro en UV de pantalla, 0 en el centro y ~1 ya en la esquina.
float2 centered = ScreenUV - float2(0.5, 0.5);
float dist = length(centered) * 1.4142135;
float vignetteMask = saturate(pow(dist, 2.2) * BodyVignette);

return lerp(desaturated, BodyVignetteTint, vignetteMask);
"""


def build_pp_body():
    m = recreate_material("M_PP_Body")
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    m.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)

    scene_color = expr(m, unreal.MaterialExpressionSceneTexture, -700, 0)
    scene_color.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)

    screen_pos = expr(m, unreal.MaterialExpressionScreenPosition, -700, 200)

    vignette_param = expr(m, unreal.MaterialExpressionScalarParameter, -700, 380)
    vignette_param.set_editor_property("parameter_name", "BodyVignette")
    vignette_param.set_editor_property("default_value", 0.0)

    desaturation_param = expr(m, unreal.MaterialExpressionScalarParameter, -700, 480)
    desaturation_param.set_editor_property("parameter_name", "BodyDesaturation")
    desaturation_param.set_editor_property("default_value", 0.0)

    tint_param = expr(m, unreal.MaterialExpressionVectorParameter, -700, 580)
    tint_param.set_editor_property("parameter_name", "BodyVignetteTint")
    tint_param.set_editor_property("default_value", unreal.LinearColor(0.0, 0.0, 0.0, 1.0))

    body = custom(m, -350, 200, PP_BODY_HLSL, ["SceneColor", "ScreenUV", "BodyVignette", "BodyDesaturation", "BodyVignetteTint"],
                  unreal.CustomMaterialOutputType.CMOT_FLOAT3, "BodyPostProcess")
    connect(scene_color, "Color", body, "SceneColor")
    connect(screen_pos, "ViewportUV", body, "ScreenUV")
    connect(vignette_param, "", body, "BodyVignette")
    connect(desaturation_param, "", body, "BodyDesaturation")
    connect(tint_param, "", body, "BodyVignetteTint")
    to_property(body, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)


# ---------------------------------------------------------------------------
# Vegetación y rocas (se reconstruyen en su sitio para conservar las referencias)
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# Low poly (packs CC0: Kenney, KayKit, Quaternius) — atlas de paleta por isla
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# Estilo low poly (F3a): hoja de paleta y M_Toon provisional
# ---------------------------------------------------------------------------

def import_palette_texture() -> unreal.Texture2D:
    """Importa la hoja de paleta con muestreo exacto (cada texel es un color de paleta.json)."""
    if not os.path.isfile(PALETTE_PNG):
        raise RuntimeError(f"Falta {PALETTE_PNG} (uv run style-paleta en Tools/Style)")
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", PALETTE_PNG)
    task.set_editor_property("destination_path", ART_MATERIALS_PATH)
    task.set_editor_property("destination_name", "T_Paleta")
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    ASSET_TOOLS.import_asset_tasks([task])
    texture = unreal.load_asset(f"{ART_MATERIALS_PATH}/T_Paleta")
    if texture is None:
        raise RuntimeError("No se pudo importar T_Paleta")
    texture.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("srgb", True)
    # UserInterface2D (BGRA8 sin compresión por bloques): los 32 colores salen exactos.
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_PIXELS2D)
    texture.set_editor_property("never_stream", True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    unreal.log(f"[Explored] Textura lista: {texture.get_path_name()}")
    return texture


TOON_UV_HLSL = r"""
float Cell = clamp(floor(Index), 0.0, PaletteSize - 1.0);
float2 Forced = float2((Cell + 0.5) / PaletteSize, 0.5);
return lerp(UV, Forced, saturate(UseIndex));
"""


def build_toon(palette: unreal.Texture2D):
    m = recreate_material("M_Toon", ART_MATERIALS_PATH)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)

    uv = expr(m, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    index = expr(m, unreal.MaterialExpressionScalarParameter, -900, 120)
    index.set_editor_property("parameter_name", "PaletteIndex")
    index.set_editor_property("default_value", 0.0)
    use_index = expr(m, unreal.MaterialExpressionScalarParameter, -900, 220)
    use_index.set_editor_property("parameter_name", "UsePaletteIndex")
    use_index.set_editor_property("default_value", 0.0)
    size = expr(m, unreal.MaterialExpressionConstant, -900, 320)
    size.set_editor_property("r", float(PALETTE_SIZE))

    cell_uv = custom(m, -600, 100, TOON_UV_HLSL, ["UV", "Index", "UseIndex", "PaletteSize"],
                     unreal.CustomMaterialOutputType.CMOT_FLOAT2, "PaletteCellUV")
    connect(uv, "", cell_uv, "UV")
    connect(index, "", cell_uv, "Index")
    connect(use_index, "", cell_uv, "UseIndex")
    connect(size, "", cell_uv, "PaletteSize")

    sample = expr(m, unreal.MaterialExpressionTextureSample, -300, 0)
    sample.set_editor_property("texture", palette)
    sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    connect(cell_uv, "", sample, "UVs")
    to_property(sample, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)

    roughness = expr(m, unreal.MaterialExpressionConstant, -300, 250)
    roughness.set_editor_property("r", 1.0)
    to_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    specular = expr(m, unreal.MaterialExpressionConstant, -300, 330)
    specular.set_editor_property("r", 0.0)
    to_property(specular, "", unreal.MaterialProperty.MP_SPECULAR)
    finish(m)


def main():
    unreal.EditorAssetLibrary.make_directory(MATERIALS_PATH)
    build_ocean()
    build_stars()
    build_pp_body()
    unreal.EditorAssetLibrary.make_directory(ART_MATERIALS_PATH)
    build_toon(import_palette_texture())


main()
