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
                                       (parámetros en Tools/Textures/strokes.json; GDD §11). El alfa del
                                       color por vértice es el tipo de suelo (1 hierba, 0 arena, 0,5 resto;
                                       Tools/Island export.surface_alpha). Bajo el césped, el suelo se funde
                                       con el verde de la base de las hojas cerca y con el de la alfombra
                                       lejos, con la misma f(d) que el césped (MPC_Terreno). En la arena
                                       (sin texturas fotográficas): pesos de arena y hierba con un borde
                                       de ruido en espacio de mundo (no el degradado de cada triángulo),
                                       ondas de viento (bandas perpendiculares a WindX/WindY con la fase
                                       deformada por ruido de 3-6 m, ≈ 6 % de contraste, solo en arena,
                                       apagadas a 40 m y bajo ~3 px de banda: sin moiré), grano de 1,5 cm que
                                       se apaga a GrainFadeEndM, brillos escasos en el color base (celdas
                                       de 2,5 cm, solo de cara al sol; la luz los apaga en la sombra;
                                       SunDirection y SunGlint de MPC_Terreno) y orilla por ALTURA sobre el
                                       mar: arena mojada más oscura y algo especular hasta WetTopM, borde
                                       claro y espuma continua de 0,25-0,6 m (máx. 40 %) que late.
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
    ART_MATERIALS_PATH, ASSET_TOOLS, GRASS_FRACTION_HLSL, MEL, NOISE_HLSL, REPO_ROOT, add_toon_glow, add_toon_wind,
    collection_param, connect, custom, expr, finish, grass_ground_colors, hlsl_float3, palette_linear,
    recreate_material, scalar_param, terrain_collection, to_property, vector_param,
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


# Mezcla de suelos y ondas del viento. Los pesos de arena y de hierba salen del alfa del color por vértice
# (lineal entre vértices) desplazado por un ruido suave de 1,3 y 0,6 m y afilado: el borde arena-tierra-hierba
# es una línea orgánica en espacio de mundo y no el degradado de cada triángulo de 1,8 m. Las ondas siguen
# WindX/WindY con la fase deformada por ruido de 3-6 m (crestas que serpentean, sin trama de pana), contraste
# RippleDark-RippleLight (≈ 6 % de luminancia), nada en tierra (× peso de arena), y se apagan del todo a
# RippleFadeEndM y cuando la banda baja de ~3 píxeles (tamaño del píxel en el suelo: sin moiré).
RIPPLE_HLSL = NOISE_HLSL + r"""
float2 p = WP.xy / 100.0;
float2 w = normalize(float2(WindX, WindY) + 1e-5);
float edgeN = Noise.Smooth(p / 1.3 + 3.7) * 0.7 + Noise.Smooth(p / 0.6 + 9.1) * 0.3;
float sandLin = saturate(1.0 - A * 2.0);
float grassLin = saturate(A * 2.0 - 1.0);
float sandW = smoothstep(0.3, 0.7, sandLin + (edgeN - 0.5) * 0.7);
float grassW = smoothstep(0.3, 0.7, grassLin + (edgeN - 0.5) * 0.7);
float warp = (Noise.Smooth(p / 4.5) - 0.5) * 7.0 + (Noise.Smooth(p / 1.7 + 17.3) - 0.5) * 1.2;
float phase = dot(p, w) / max(RippleWavelength, 0.01) + warp;
float fr = frac(phase);
float crest = smoothstep(0.0, 0.7, fr) * (1.0 - smoothstep(0.7, 1.0, fr));
float h = WP.z / 100.0;
float d = length(WP.xy - Cam.xy) / 100.0;
float shoreNoise = Noise.Smooth(p / 1.8 + 5.1);
float wetTop = WetTopM + (shoreNoise - 0.5) * 0.12;
float dry = smoothstep(wetTop, wetTop + 0.25, h);
float flatW = saturate((N.z - 0.8) / 0.15);
// Bandas por píxel sin ddx/fwidth (no compilan en todos los pases): el píxel mide ≈ d / 900 m a 1080p y
// se estira con la incidencia rasante.
float3 toCam = Cam - WP;
float grazing = max(abs(dot(normalize(toCam), normalize(N))), 0.12);
float footprint = length(toCam) / 100.0 / (900.0 * grazing);
float aa = 1.0 - smoothstep(0.18, 0.35, footprint / max(RippleWavelength, 0.01));
float rippleFade = (1.0 - saturate((d - RippleFadeStartM) / max(RippleFadeEndM - RippleFadeStartM, 1e-3))) * aa;
float patchy = lerp(0.45, 1.0, smoothstep(0.3, 0.7, Noise.Smooth(p / 5.0 + 41.0)));
float rip = sandW * flatW * dry * RippleStrength * patchy;
"""

TERRAIN_COLOR_HLSL = RIPPLE_HLSL + r"""
float3 col = C * lerp(Strokes, 1.0 + (Strokes - 1.0) * 0.4, sandW);
// Donde el ruido ya dice arena pero el triángulo aún mezcla, arena (del tono de la franja de orilla).
float tone = lerp(0.94, 1.04, Noise.Smooth(p / 3.0 + 2.2));
col = lerp(col, SAND_EDGE_COLOR * tone, saturate(sandW - sandLin));
// Suelo bajo el césped: el verde de la base de las hojas cerca y el de la alfombra lejos (misma f(d)); lejos
// se iguala del todo, para que el final de la alfombra no deje un borde de otro verde.
""" + GRASS_FRACTION_HLSL + r"""
float3 under = lerp(GRASS_FAR, GRASS_BASE, GrassF) * lerp(1.0, Strokes, 0.5 * GrassF);
col = lerp(col, under, grassW * lerp(1.0, GroundMatch, GrassF));
// Dos tonos de arena en las ondas; de lejos, su tono medio (sin cambio de brillo al apagarlas).
col *= lerp(1.0, lerp(RippleDark, RippleLight, crest), rip * rippleFade);
col *= lerp(1.0, (RippleDark + RippleLight) * 0.5, rip * (1.0 - rippleFade));
// Grano de 1,5 cm (hash entero: sin patrones a cientos de metros del origen).
uint2 gq = uint2(int2(floor(WP.xy / GrainCm)) + 1048576);
uint gh = (gq.x * 1664525u) ^ (gq.y * 1013904223u + 0x9E3779B9u);
gh ^= gh >> 16; gh *= 0x7feb352du; gh ^= gh >> 15; gh *= 0x846ca68bu; gh ^= gh >> 16;
float grain = (gh & 0xFFFFu) / 65535.0;
float grainFade = 1.0 - saturate(d / max(GrainFadeEndM, 1e-3));
col *= lerp(1.0, lerp(1.0 - GrainAmount, 1.0 + GrainAmount * 0.6, grain), sandW * grainFade);
// Orilla por altura sobre el mar: mojada y borde claro.
float notGrass = 1.0 - grassW;
float wet = (1.0 - smoothstep(wetTop - 0.12, wetTop, h)) * notGrass;
col = lerp(col, col * WetDarken * float3(0.94, 0.97, 1.03), wet);
float rim = smoothstep(wetTop - 0.02, wetTop + 0.01, h) * (1.0 - smoothstep(wetTop + 0.01, wetTop + 0.08, h));
col = lerp(col, col * RimLight, rim * sandW);
// Espuma: banda continua de FoamWidthMin-FoamWidthMax metros en horizontal desde la línea del agua (altura
// / pendiente de la normal suave), con ruido de baja frecuencia en el ancho y en la opacidad (máx.
// FoamOpacity) y fundida hacia la arena seca; sin escalones ni máscaras de onda.
float tanS = sqrt(saturate(1.0 - N.z * N.z)) / max(N.z, 0.05);
float fromWater = h / max(tanS, 0.03);
float foamN = Noise.Smooth(p / 3.5 + float2(T * 0.04, 0.0));
float foamWidth = lerp(FoamWidthMin, FoamWidthMax, foamN);
float pulse = 0.5 + 0.5 * sin(T * 1.2566 + dot(WP.xy, float2(0.0011, 0.0007)));
float foam = smoothstep(-0.6, -0.1, fromWater) * (1.0 - smoothstep(0.0, foamWidth * lerp(0.75, 1.0, pulse), fromWater));
foam *= smoothstep(0.15, 0.75, Noise.Smooth(p / 1.4 + float2(7.7, T * 0.03)));
col = lerp(col, FOAM_COLOR, foam * FoamOpacity * notGrass);
// Brillos de la arena: celdas escasas de GlitterCm cuya microfaceta refleja el sol hacia la cámara. Van en el
// color base (no en el emisivo): la luz los apaga en la sombra. Solo en arena seca, llana y cerca.
uint2 cq = uint2(int2(floor(WP.xy / GlitterCm)) + 1048576);
uint c0 = (cq.x * 747796405u) ^ (cq.y * 2891336453u + 0x632BE5ABu);
c0 ^= c0 >> 16; c0 *= 0x7feb352du; c0 ^= c0 >> 15; c0 *= 0x846ca68bu; c0 ^= c0 >> 16;
uint c1 = c0 * 1664525u + 1013904223u;
uint c2 = c1 * 1664525u + 1013904223u;
float present = step((c0 & 0xFFFFu) / 65535.0, GlitterDensity);
float2 tilt = float2((c1 >> 16) / 65535.0, (c2 >> 16) / 65535.0) * 2.0 - 1.0;
float3 G = normalize(float3(tilt * 0.35, 1.0));
float3 V = normalize(Cam - WP);
float3 L = normalize(SunDir.xyz + 1e-5);
float spark = smoothstep(GlitterThreshold, 1.0, dot(reflect(-L, G), V));
float glitterFade = 1.0 - saturate((d - 2.0) / max(GlitterFadeEndM - 2.0, 1e-3));
col *= 1.0 + GlitterStrength * spark * present * glitterFade * sandW * dry * flatW * SunGlint * saturate(L.z * 4.0);
return col;
"""

TERRAIN_NORMAL_HLSL = RIPPLE_HLSL + r"""
// Pendiente del perfil de la onda (derivada de los dos smoothstep): la cara a barlovento sube despacio
// y la de sotavento cae rápido.
float t0 = saturate(fr / 0.7);
float t1 = saturate((fr - 0.7) / 0.3);
float slope = fr < 0.7 ? 6.0 * t0 * (1.0 - t0) / 0.7 : -6.0 * t1 * (1.0 - t1) / 0.3;
float3 n = normalize(N);
n.xy -= w * slope * RippleNormal * rip * rippleFade;
return normalize(n);
"""

TERRAIN_WET_HLSL = RIPPLE_HLSL + r"""
return (1.0 - smoothstep(wetTop - 0.12, wetTop, h)) * (1.0 - grassW);
"""

TERRAIN_PARAMS = {
    "GroundMatch": 0.8, "WindX": 0.8, "WindY": 0.6, "RippleWavelength": 0.15, "RippleStrength": 1.0,
    "RippleDark": 0.955, "RippleLight": 1.02, "RippleFadeStartM": 12.0, "RippleFadeEndM": 40.0,
    "RippleNormal": 0.06, "GrainCm": 1.5, "GrainAmount": 0.07, "GrainFadeEndM": 9.0, "WetTopM": 0.32,
    "WetDarken": 0.76, "RimLight": 1.08, "FoamWidthMin": 0.25, "FoamWidthMax": 0.6, "FoamOpacity": 0.4,
    "GlitterCm": 2.5, "GlitterDensity": 0.008, "GlitterThreshold": 0.995, "GlitterFadeEndM": 10.0,
    "GlitterStrength": 1.6, "WetRoughness": 0.5, "WetSpecular": 0.25,
}
RIPPLE_INPUTS = ["WP", "Cam", "A", "N", "WindX", "WindY", "RippleWavelength", "WetTopM", "RippleFadeStartM",
                 "RippleFadeEndM", "RippleStrength"]


def terrain_constants(code: str) -> str:
    """Sustituye los colores de paleta (lineales) en el HLSL."""
    base, far = grass_ground_colors()
    return (code.replace("GRASS_BASE", hlsl_float3(base))
            .replace("GRASS_FAR", hlsl_float3(far))
            .replace("FOAM_COLOR", hlsl_float3(palette_linear("blanco_nube")))
            .replace("SAND_EDGE_COLOR", hlsl_float3(palette_linear("arena_oscura"))))


def build_terrain_surface(m, vc, linear, strokes, world):
    """Suelo bajo el césped, arena (ondas, grano, brillo) y orilla mojada: nodos de color, normal (en
    espacio de mundo) y rugosidad/especular de la orilla. Los brillos de la arena van en el color base."""
    mpc = terrain_collection()
    m.set_editor_property("tangent_space_normal", False)
    params = {name: scalar_param(m, name, value, -1700, 1700 + 60 * i)
              for i, (name, value) in enumerate(TERRAIN_PARAMS.items())}
    cam = expr(m, unreal.MaterialExpressionCameraPositionWS, -1700, 1600)
    normal_ws = expr(m, unreal.MaterialExpressionVertexNormalWS, -1700, 1540)
    time = expr(m, unreal.MaterialExpressionTime, -1700, 1480)
    grass = {name: collection_param(m, mpc, name, -1700, 3400 + 60 * i)
             for i, name in enumerate(("GrassDenseM", "GrassEndM", "GrassEndFadeM", "GrassExponent"))}
    sun = {"SunDir": collection_param(m, mpc, "SunDirection", -1700, 3700),
           "SunGlint": collection_param(m, mpc, "SunGlint", -1700, 3760)}
    sources = {"WP": (world, ""), "Cam": (cam, ""), "A": (vc, "A"), "N": (normal_ws, ""),
               "T": (time, ""), "C": (linear, ""), "Strokes": (strokes, "")}
    sources.update({name: (node, "") for name, node in {**grass, **sun, **params}.items()})

    def node(code: str, extra: list[str], y: int, out, label: str):
        names = RIPPLE_INPUTS + extra
        n = custom(m, -900, y, code, names, out, label)
        for name in names:
            src, pin = sources[name]
            connect(src, pin, n, name)
        return n

    out3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3
    color_extra = ["C", "Strokes", "T", "GroundMatch", "RippleDark", "RippleLight", "GrainCm", "GrainAmount",
                   "GrainFadeEndM", "WetDarken", "RimLight", "FoamWidthMin", "FoamWidthMax", "FoamOpacity",
                   "GlitterCm", "GlitterDensity", "GlitterThreshold", "GlitterFadeEndM", "GlitterStrength", *grass,
                   *sun]
    color = node(terrain_constants(TERRAIN_COLOR_HLSL), color_extra, 1700, out3, "TerrainColor")
    to_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    normal = node(TERRAIN_NORMAL_HLSL, ["RippleNormal"], 2100, out3, "TerrainNormal")
    to_property(normal, "", unreal.MaterialProperty.MP_NORMAL)
    wet = node(TERRAIN_WET_HLSL, [], 2900, unreal.CustomMaterialOutputType.CMOT_FLOAT1, "ShoreWet")
    for prop, dry_value, wet_param, y in ((unreal.MaterialProperty.MP_ROUGHNESS, 1.0, "WetRoughness", 2950),
                                         (unreal.MaterialProperty.MP_SPECULAR, 0.0, "WetSpecular", 3100)):
        dry_node = expr(m, unreal.MaterialExpressionConstant, -600, y)
        dry_node.set_editor_property("r", dry_value)
        blend = expr(m, unreal.MaterialExpressionLinearInterpolate, -400, y)
        connect(dry_node, "", blend, "A")
        connect(params[wet_param], "", blend, "B")
        connect(wet, "", blend, "Alpha")
        to_property(blend, "", prop)


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

    build_terrain_surface(m, vc, linear, factor, world)
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
