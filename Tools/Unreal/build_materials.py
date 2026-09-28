"""Crea los materiales base del juego por código (idempotente).

Se ejecuta con:
    UnrealEditor-Cmd Explored.uproject -run=pythonscript -script="Tools/Unreal/build_materials.py"

Materiales:
    /Game/Materials/M_Terrain  color por vértice (sRGB) con variación procedural y roca en alfa.
    /Game/Materials/M_Ocean    Single Layer Water con olas de Gerstner (mismas que FOceanWaves).
    /Game/Materials/M_Stars    cúpula de estrellas aditiva controlada por el parámetro «Night».
    /Game/Materials/M_PP_Body  postproceso del cuerpo: viñeta (color y fuerza) y desaturación
                                según FBodySignals (VignetteAmount, TintColor, DesaturationAmount).
    /Game/Generated/Materials/M_LowPoly  maestro de los packs CC0 low poly: color del atlas de
                                paleta T_Palette_<Isla> (parámetro «Palette») y una instancia
                                MI_LowPoly_<Isla> por isla. Ver docs/art/paleta.md.
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

# Muestreo triplanar compartido por color y normal (metros del mundo).
TRIPLANAR_COMMON = r"""
float3 n = normalize(VN);
float3 w = pow(abs(n), 4.0);
w /= (w.x + w.y + w.z);
float3 p = WP / 100.0;
float s1 = 1.0 / 2.7;
float s2 = 1.0 / 11.0;
"""

ROCK_COLOR_HLSL = TRIPLANAR_COMMON + r"""
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

ROCK_NORMAL_HLSL = TRIPLANAR_COMMON + r"""
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


# Capas del terreno: nombre de la entrada HLSL -> textura. BC en sRGB, N normal en espacio tangente.
TERRAIN_LAYERS = {
    "SandBC": "T_SandDry_BC", "SandN": "T_SandDry_N",
    "WetBC": "T_SandWet_BC", "WetN": "T_SandWet_N",
    "AshBC": "T_Ash_BC", "AshN": "T_Ash_N",
    "GrassBC": "T_Grass_BC", "GrassN": "T_Grass_N",
    "ForestBC": "T_ForestFloor_BC", "ForestN": "T_ForestFloor_N",
    "BasaltBC": "T_VolcanicRock_BC", "BasaltN": "T_VolcanicRock_N",
    "LimeBC": "T_Limestone_BC", "LimeN": "T_Limestone_N",
}

# Mezcla de capas. Los pesos vienen de FTerrainDensity::SurfaceLayers vía UV1 (arena, hojarasca) y
# UV2 (roca, volcánico); la hierba es el resto. El suelo blando se proyecta desde arriba y la roca en
# triplanar. Cada capa se muestrea a dos escalas (una girada) para romper la repetición, y la mezcla
# usa la altura aproximada (luminancia) para que la hierba asome entre la arena en vez de fundirse.
# La salida es float4: RGB = color base, A = rugosidad.
TERRAIN_LAYERS_COMMON = TRIPLANAR_COMMON + r"""
// Con 15 texturas no caben los samplers propios (16 por material, contando los del motor):
// todas se leen con el sampler compartido de mundo.
#define SHARED Material.Wrap_WorldGroupSettings
float2 top = p.xy;
float2x2 rot = float2x2(0.8, -0.6, 0.6, 0.8);
float sandW = saturate(L1.x);
float forestW = saturate(L1.y);
float rockW = saturate(L2.x);
float volcanic = saturate(L2.y);
float grassW = saturate(1.0 - sandW - forestW - rockW);
float depthM = -min(WP.z / 100.0, 0.0);
float wetW = saturate((0.6 - WP.z / 100.0) / 0.8) * sandW;
"""

TERRAIN_COLOR_HLSL = TERRAIN_LAYERS_COMMON + r"""
#define TOP2(T, S) lerp(Texture2DSample(T, SHARED, top / S), Texture2DSample(T, SHARED, mul(rot, top) / (S * 2.7)), 0.35)
#define TRI(T, S) (Texture2DSample(T, SHARED, p.yz / S) * w.x + Texture2DSample(T, SHARED, p.xz / S) * w.y + Texture2DSample(T, SHARED, p.xy / S) * w.z)
// Igual que TOP2 pero por eje triplanar: mezcla la muestra base con una segunda girada y a otra
// escala para romper la repetición. Solo para BasaltBC (ver más abajo): a la escala de tile de la
// roca volcánica (4 m), el basalto estilizado en caras planas escalonadas en 4 valores (ver
// volcanic_rock en texgen/stylized.py) repite su patrón de
// bandas de forma idéntica en cada tile y en las tres proyecciones triplanares comparten fase en
// los ejes que cruzan por la misma coordenada de mundo (yz y xz comparten Z): en formas redondeadas
// como una cumbre, donde dos proyecciones pesan parecido, esa repetición sincronizada se lee como
// una rejilla/cuadrícula regular en vez de roca natural.
#define TRI2(T, S) ( \
    lerp(Texture2DSample(T, SHARED, p.yz / S), Texture2DSample(T, SHARED, mul(rot, p.yz) / (S * 2.7)), 0.35) * w.x + \
    lerp(Texture2DSample(T, SHARED, p.xz / S), Texture2DSample(T, SHARED, mul(rot, p.xz) / (S * 2.7)), 0.35) * w.y + \
    lerp(Texture2DSample(T, SHARED, p.xy / S), Texture2DSample(T, SHARED, mul(rot, p.xy) / (S * 2.7)), 0.35) * w.z )

float4 sandDry = lerp(TOP2(SandBC, 3.2), TOP2(AshBC, 3.0), smoothstep(0.75, 1.0, volcanic));
float4 sand = lerp(sandDry, TOP2(WetBC, 3.2), saturate(wetW * 1.4));
float4 grass = TOP2(GrassBC, 2.4);
// La hojarasca estilizada (texgen/stylized.py) ya pinta con la paleta: sin corrección de color aquí.
float4 forest = TOP2(ForestBC, 2.8);
float4 rock = lerp(TRI(LimeBC, 4.5), TRI2(BasaltBC, 4.0), volcanic);

// Mezcla por altura: la luminancia hace de mapa de alturas aproximado.
float hS = dot(sand.rgb, 0.33) + sandW * 1.2;
float hG = dot(grass.rgb, 0.33) + grassW * 1.2;
float hF = dot(forest.rgb, 0.33) + forestW * 1.2;
float hR = dot(rock.rgb, 0.33) + rockW * 1.4;
float hMax = max(max(hS, hG), max(hF, hR)) - 0.25;
float4 b = float4(max(hS - hMax, 0.0), max(hG - hMax, 0.0), max(hF - hMax, 0.0), max(hR - hMax, 0.0));
b /= max(b.x + b.y + b.z + b.w, 1e-4);
float3 color = sand.rgb * b.x + grass.rgb * b.y + forest.rgb * b.z + rock.rgb * b.w;

// Tinte suave por isla (color de vértice) y variación macro para que no se vea el mosaico a distancia.
float3 vc = saturate(VC.rgb);
float3 hue = vc / max(dot(vc, 0.3333), 0.03);
color *= lerp(1.0.xxx, hue, 0.18);
float4 macro = Texture2DSample(Detail, SHARED, p.xy / 70.0 + 0.37);
color *= lerp(0.85, 1.1, macro.g) * lerp(float3(0.96, 1.0, 1.03), float3(1.05, 1.0, 0.94), macro.r);

// Fondo marino: más oscuro y azulado con la profundidad; cáusticas en someros.
if (depthM > 0.0)
{
    color *= lerp(1.0.xxx, float3(0.55, 0.68, 0.72), saturate(depthM / 22.0));
    float2 cp = WP.xy / 220.0;
    float c1 = sin(cp.x * 2.4 + sin(cp.y * 1.7 + Time * 0.35) * 1.6 + Time * 0.6);
    float c2 = sin(cp.y * 2.1 - sin(cp.x * 1.9 - Time * 0.28) * 1.6 - Time * 0.5);
    color += saturate(c1 * c2) * exp(-depthM / 9.0) * 0.22 * float3(0.7, 0.95, 0.9);
}
float rough = 0.88 * b.x + 0.92 * b.y + 0.9 * b.z + 0.78 * b.w;
rough = lerp(rough, 0.3, saturate(wetW * 1.4) * b.x);
return float4(color, rough);
"""

TERRAIN_NORMAL_HLSL = TERRAIN_LAYERS_COMMON + r"""
#define TOPN(T, S) (Texture2DSample(T, SHARED, top / S).rg * 2.0 - 1.0)
#define TRIN(T, S, AXIS) (Texture2DSample(T, SHARED, AXIS / S).rg * 2.0 - 1.0)

float2 nSoft = TOPN(SandN, 3.2) * sandW + TOPN(GrassN, 2.4) * grassW + TOPN(ForestN, 2.8) * forestW;
float2 rx = lerp(TRIN(LimeN, 4.5, p.yz), TRIN(BasaltN, 4.0, p.yz), volcanic);
float2 ry = lerp(TRIN(LimeN, 4.5, p.xz), TRIN(BasaltN, 4.0, p.xz), volcanic);
float2 rz = lerp(TRIN(LimeN, 4.5, p.xy), TRIN(BasaltN, 4.0, p.xy), volcanic);
// Perturbación por eje de proyección (estilo «whiteout» simplificado), en espacio de mundo.
float3 perturbRock = float3(0.0, rx.x, rx.y) * w.x + float3(ry.x, 0.0, ry.y) * w.y + float3(rz.x, rz.y, 0.0) * w.z;
float3 perturb = float3(nSoft, 0.0) * 0.8 + perturbRock * rockW * 1.1;
return normalize(n + perturb);
"""


def build_terrain():
    m = recreate_material("M_Terrain")
    m.set_editor_property("tangent_space_normal", False)

    vc = expr(m, unreal.MaterialExpressionVertexColor, -1400, 0)
    wp = expr(m, unreal.MaterialExpressionWorldPosition, -1400, 100)
    vn = expr(m, unreal.MaterialExpressionVertexNormalWS, -1400, 200)
    uv1 = expr(m, unreal.MaterialExpressionTextureCoordinate, -1400, 300)
    uv1.set_editor_property("coordinate_index", 1)
    uv2 = expr(m, unreal.MaterialExpressionTextureCoordinate, -1400, 400)
    uv2.set_editor_property("coordinate_index", 2)
    time = expr(m, unreal.MaterialExpressionTime, -1400, 500)
    detail = texture_object(m, "/Game/Generated/Textures/T_TerrainDetail", -1400, 600)

    layers = {}
    for i, (pin, texture) in enumerate(TERRAIN_LAYERS.items()):
        layers[pin] = texture_object(m, f"/Game/Generated/Textures/{texture}", -1700, i * 110)

    base_pins = [pin for pin in TERRAIN_LAYERS if pin.endswith("BC")]
    normal_pins = [pin for pin in TERRAIN_LAYERS if pin.endswith("N")]

    color = custom(m, -700, 0, TERRAIN_COLOR_HLSL,
                   ["VC", "WP", "VN", "L1", "L2", "Time", "Detail"] + base_pins,
                   unreal.CustomMaterialOutputType.CMOT_FLOAT4, "TerrainColor")
    for pin, node in (("VC", vc), ("WP", wp), ("VN", vn), ("L1", uv1), ("L2", uv2), ("Time", time), ("Detail", detail)):
        connect(node, "", color, pin)
    for pin in base_pins:
        connect(layers[pin], "", color, pin)

    rgb = expr(m, unreal.MaterialExpressionComponentMask, -400, 0)
    for channel in ("r", "g", "b"):
        rgb.set_editor_property(channel, True)
    connect(color, "", rgb, "")
    to_property(rgb, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = expr(m, unreal.MaterialExpressionComponentMask, -400, 120)
    rough.set_editor_property("a", True)
    connect(color, "", rough, "")
    to_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    normal = custom(m, -700, 500, TERRAIN_NORMAL_HLSL, ["WP", "VN", "L1", "L2"] + normal_pins,
                    unreal.CustomMaterialOutputType.CMOT_FLOAT3, "TerrainNormal")
    for pin, node in (("WP", wp), ("VN", vn), ("L1", uv1), ("L2", uv2)):
        connect(node, "", normal, pin)
    for pin in normal_pins:
        connect(layers[pin], "", normal, pin)
    to_property(normal, "", unreal.MaterialProperty.MP_NORMAL)

    spec = expr(m, unreal.MaterialExpressionConstant, -400, 300)
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
//
// «Detail» es el RGB de T_FoliageAtlas_BC (Tools/Textures/texgen/materials.py:
// foliage_atlas) muestreado con la UV real de cada tarjeta de hoja/fronda/pétalo:
// a diferencia del antiguo T_LeafNoise (un patrón sin silueta, solo variación), esta
// textura lleva nervadura/veteado/AO por hoja de verdad, así que su multiplicador se
// deja con más rango (antes 0.88-1.08 / 0.94-1.04, casi imperceptible) para que esa
// forma se LEA en el render en vez de quedar aplastada por el tinte de vértice.
float3 base = saturate(VC);
// Tinte por instancia: unas plantas algo más amarillas y otras más oscuras.
float3 tintA = float3(1.22, 1.06, 0.62);
float3 tintB = float3(0.72, 0.98, 1.10);
float3 color = base * lerp(tintA, tintB, R) * lerp(0.85, 1.1, frac(R * 7.31));
color *= lerp(0.72, 1.25, Detail.r) * lerp(0.85, 1.12, Detail.b);
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
    """M_Leaf / M_Grass: Masked, con OpacityMask + Normal de
    T_FoliageAtlas_BC/_N (Tools/Textures/texgen/materials.py, generado por
    `gen_textures.py --only FoliageAtlas`) — cada tarjeta de hoja/fronda/
    pétalo del kit de Blender (Tools/Blender/lib/common.py: make_leaf_blade/
    make_frond_object/make_leaf_cluster_cards) lleva UV apuntando a la celda
    del atlas que le corresponde, así que el recorte alfa cae justo sobre la
    silueta en vez de un cuadrado sólido."""
    m = rebuild_material(GENERATED_MATERIALS, name)
    if two_sided_foliage:
        m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
        m.set_editor_property("two_sided", True)
    else:
        m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
        m.set_editor_property("two_sided", False)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property("opacity_mask_clip_value", 0.35)

    vc = expr(m, unreal.MaterialExpressionVertexColor, -1100, 0)
    rnd = expr(m, unreal.MaterialExpressionPerInstanceRandom, -1100, 150)
    uv = expr(m, unreal.MaterialExpressionTextureCoordinate, -1300, 300)
    atlas_tex = expr(m, unreal.MaterialExpressionTextureSample, -1100, 300)
    atlas_tex.set_editor_property("texture", unreal.EditorAssetLibrary.load_asset("/Game/Generated/Textures/T_FoliageAtlas_BC"))
    connect(uv, "", atlas_tex, "UVs")

    color = custom(m, -700, 0, FOLIAGE_COLOR_HLSL, ["VC", "R", "Detail"], unreal.CustomMaterialOutputType.CMOT_FLOAT3, "FoliageColor")
    connect(vc, "", color, "VC")
    connect(rnd, "", color, "R")
    connect(atlas_tex, "RGB", color, "Detail")
    to_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    to_property(atlas_tex, "A", unreal.MaterialProperty.MP_OPACITY_MASK)

    normal_tex = expr(m, unreal.MaterialExpressionTextureSample, -1100, 380)
    normal_tex.set_editor_property("texture", unreal.EditorAssetLibrary.load_asset("/Game/Generated/Textures/T_FoliageAtlas_N"))
    normal_tex.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    connect(uv, "", normal_tex, "UVs")
    to_property(normal_tex, "RGB", unreal.MaterialProperty.MP_NORMAL)

    if two_sided_foliage:
        sss = expr(m, unreal.MaterialExpressionMultiply, -400, 200)
        k = expr(m, unreal.MaterialExpressionConstant3Vector, -600, 250)
        # Transmisión contenida: con más, las copas vistas a contraluz desde abajo se lavaban a blanco.
        k.set_editor_property("constant", unreal.LinearColor(0.35, 0.45, 0.12, 1.0))
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
    rim_amount.set_editor_property("default_value", 0.07)
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


BARK_COLOR_HLSL = r"""
// VC ya trae el veteado/streaks por instancia (Tools/Blender/lib/common.py:
// bark_streaks_tint), en espacio lineal igual que FOLIAGE_COLOR_HLSL. BC es el
// detalle real de corteza (T_BarkTropical_BC, textura tileable fisurada con placas y
// liquen — Tools/Textures/texgen/materials.py: bark()), AO su oclusión (canal R
// de T_BarkTropical_ARH). Igual que en el follaje, VC fija el tono por instancia y BC
// aporta el detalle de superficie: sin BC el tronco es un cilindro de un marrón
// plano, exactamente la «cutrada poligonal» que pedía sustituir el encargo.
float3 col = saturate(VC) * (BC * 0.75 + 0.25) * lerp(0.65, 1.0, AO);
return col;
"""


def build_bark():
    """M_Bark: opaco, con la corteza tileable T_BarkTropical_BC/_N/_ARH —
    variante «pintada a mano» (más saturada, con cuantización suave de
    valor) del mismo generador que usa el kit de props para troncos/postes,
    Tools/Textures/texgen/materials.py: bark()/bark_tropical() — multiplicada
    por el veteado de color de vértice. Los troncos/ramas/raíces del kit de
    vegetación llevan UV cilíndrica real (Tools/Blender/lib/common.py:
    cylindrical_bark_uv, dentro de make_curved_trunk) en vez del color de
    vértice puro de antes."""
    m = rebuild_material(GENERATED_MATERIALS, "M_Bark")
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property("two_sided", False)

    vc = expr(m, unreal.MaterialExpressionVertexColor, -1100, 0)
    uv = expr(m, unreal.MaterialExpressionTextureCoordinate, -1300, 300)

    bc_tex = expr(m, unreal.MaterialExpressionTextureSample, -1100, 300)
    bc_tex.set_editor_property("texture", unreal.EditorAssetLibrary.load_asset("/Game/Generated/Textures/T_BarkTropical_BC"))
    connect(uv, "", bc_tex, "UVs")

    arh_tex = expr(m, unreal.MaterialExpressionTextureSample, -1100, 480)
    arh_tex.set_editor_property("texture", unreal.EditorAssetLibrary.load_asset("/Game/Generated/Textures/T_BarkTropical_ARH"))
    arh_tex.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    connect(uv, "", arh_tex, "UVs")

    n_tex = expr(m, unreal.MaterialExpressionTextureSample, -1100, 660)
    n_tex.set_editor_property("texture", unreal.EditorAssetLibrary.load_asset("/Game/Generated/Textures/T_BarkTropical_N"))
    n_tex.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    connect(uv, "", n_tex, "UVs")
    to_property(n_tex, "RGB", unreal.MaterialProperty.MP_NORMAL)

    color = custom(m, -700, 0, BARK_COLOR_HLSL, ["VC", "BC", "AO"], unreal.CustomMaterialOutputType.CMOT_FLOAT3, "BarkColor")
    connect(vc, "", color, "VC")
    connect(bc_tex, "RGB", color, "BC")
    connect(arh_tex, "R", color, "AO")
    to_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)

    to_property(arh_tex, "G", unreal.MaterialProperty.MP_ROUGHNESS)
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

    color = custom(m, -500, 0, ROCK_COLOR_HLSL, ["VC", "WP", "VN", "Detail", "Time"], unreal.CustomMaterialOutputType.CMOT_FLOAT3, "RockColor")
    connect(append, "", color, "VC")
    connect(wp, "", color, "WP")
    connect(vn, "", color, "VN")
    connect(detail, "", color, "Detail")
    connect(time, "", color, "Time")
    to_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)

    normal = custom(m, -500, 400, ROCK_NORMAL_HLSL, ["WP", "VN", "NormalTex", "Rock"], unreal.CustomMaterialOutputType.CMOT_FLOAT3, "RockNormal")
    connect(wp, "", normal, "WP")
    connect(vn, "", normal, "VN")
    connect(normal_tex, "", normal, "NormalTex")
    connect(one, "", normal, "Rock")
    to_property(normal, "", unreal.MaterialProperty.MP_NORMAL)

    rough = expr(m, unreal.MaterialExpressionConstant, -300, 700)
    rough.set_editor_property("r", 0.8)
    to_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    finish(m)


# ---------------------------------------------------------------------------
# Low poly (packs CC0: Kenney, KayKit, Quaternius) — atlas de paleta por isla
# ---------------------------------------------------------------------------

# Rutas literales (tests/test_contract.py las busca con una regex; no uses f-strings aquí).
PALETTE_TEXTURES = {
    "Landing": "/Game/Generated/Textures/T_Palette_Landing",
    "Esmeralda": "/Game/Generated/Textures/T_Palette_Esmeralda",
    "Humo": "/Game/Generated/Textures/T_Palette_Humo",
    "Dientes": "/Game/Generated/Textures/T_Palette_Dientes",
}

LOWPOLY_COLOR_HLSL = r"""
// Atlas de paleta (Tools/Textures/texgen/palette.py): celdas de 32 px con 4 px de margen.
// Con mips por promedio 2×2, el bilineal de un punto dentro de la zona útil de la celda no
// toca la vecina hasta el mip 3; se limita ahí (MaxMip) para objetos lejanos.
float lod = min(Palette.CalculateLevelOfDetail(PaletteSampler, UV), MaxMip);
float3 c = Texture2DSampleLevel(Palette, PaletteSampler, UV, lod).rgb;
// Packs que traen color por vértice (algunos de Kenney): se multiplica si UseVertexColor = 1.
return c * lerp(1.0.xxx, saturate(VC.rgb), UseVertexColor);
"""


def build_lowpoly():
    """M_LowPoly + MI_LowPoly_<Isla>. Las mallas de los packs llevan UV0 apuntando a su celda
    del atlas (u = centro de columna, v dentro de la celda; ver paleta.json)."""
    m = rebuild_material(GENERATED_MATERIALS, "M_LowPoly")
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)

    palette = expr(m, unreal.MaterialExpressionTextureObjectParameter, -1100, 0)
    palette.set_editor_property("parameter_name", "Palette")
    default_palette = unreal.EditorAssetLibrary.load_asset(PALETTE_TEXTURES["Landing"])
    if default_palette is None:
        raise RuntimeError("Falta T_Palette_Landing; ejecuta Tools/Textures/gen_textures.py e import_textures.py")
    palette.set_editor_property("texture", default_palette)
    uv = expr(m, unreal.MaterialExpressionTextureCoordinate, -1100, 200)
    vc = expr(m, unreal.MaterialExpressionVertexColor, -1100, 300)
    max_mip = expr(m, unreal.MaterialExpressionScalarParameter, -1100, 450)
    max_mip.set_editor_property("parameter_name", "MaxMip")
    max_mip.set_editor_property("default_value", 3.0)
    use_vc = expr(m, unreal.MaterialExpressionScalarParameter, -1100, 550)
    use_vc.set_editor_property("parameter_name", "UseVertexColor")
    use_vc.set_editor_property("default_value", 0.0)

    color = custom(m, -700, 0, LOWPOLY_COLOR_HLSL, ["Palette", "UV", "VC", "MaxMip", "UseVertexColor"],
                   unreal.CustomMaterialOutputType.CMOT_FLOAT3, "LowPolyColor")
    for pin, node in (("Palette", palette), ("UV", uv), ("VC", vc), ("MaxMip", max_mip), ("UseVertexColor", use_vc)):
        connect(node, "", color, pin)
    to_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)

    rough = expr(m, unreal.MaterialExpressionScalarParameter, -400, 300)
    rough.set_editor_property("parameter_name", "Roughness")
    rough.set_editor_property("default_value", 0.85)
    to_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    spec = expr(m, unreal.MaterialExpressionConstant, -400, 400)
    spec.set_editor_property("r", 0.3)
    to_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    finish(m)

    # Una instancia por isla: el «parámetro de isla» es la textura de paleta.
    for island, texture_path in PALETTE_TEXTURES.items():
        name = f"MI_LowPoly_{island}"
        full = f"{GENERATED_MATERIALS}/{name}"
        if unreal.EditorAssetLibrary.does_asset_exist(full):
            mi = unreal.EditorAssetLibrary.load_asset(full)
        else:
            mi = ASSET_TOOLS.create_asset(name, GENERATED_MATERIALS, unreal.MaterialInstanceConstant,
                                          unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(mi, m)
        texture = unreal.EditorAssetLibrary.load_asset(texture_path)
        if texture is None:
            raise RuntimeError(f"Falta la textura {texture_path}; ejecuta Tools/Unreal/import_textures.py")
        MEL.set_material_instance_texture_parameter_value(mi, "Palette", texture)
        unreal.EditorAssetLibrary.save_loaded_asset(mi)
        unreal.log(f"[Explored] Instancia lista: {full}")


def build_vegetation_materials():
    build_foliage("M_Leaf", wind_strength=0.35, two_sided_foliage=True)
    build_foliage("M_Grass", wind_strength=0.25, two_sided_foliage=True)
    build_bark()
    build_rock()


def main():
    unreal.EditorAssetLibrary.make_directory(MATERIALS_PATH)
    build_terrain()
    build_ocean()
    build_stars()
    build_pp_body()
    build_vegetation_materials()
    build_lowpoly()


main()
