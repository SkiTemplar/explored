"""Materiales low poly del agua y del cielo (pasada visual de F4, GDD v3 §11).

    /Game/Materials/M_Ocean          Default Lit opaco, sin reflejos ni translucidez: superficie suave
                                     (normal por diferencias finitas de las mismas olas de Gerstner que
                                     FOceanWaves; «FacetSize» queda sin uso desde F6) y degradado
                                     continuo de paleta según la profundidad del fondo: orilla
                                     turquesa (agua_somera), media (agua_media) y profunda
                                     (agua_profunda), con banda blanda de espuma (blanco_nube, peso ShoreFoam: 0 desde F8, la dibuja M_TerrainToon) que late en la
                                     orilla y crestas blancas con mar de temporal. La profundidad sale
                                     del relieve analítico de island.json (tierras elípticas con costa
                                     armónica y rocas, como FIslandLayout), no de texturas ni de
                                     DistanceToNearestSurface.
    /Game/Art/Materials/M_SkyGradient  Cúpula de cielo (Unlit, «Is Sky»): degradado horizonte → franja
                                     media → cénit y halo del sol; colores que fija AExploredSkyController
                                     desde FSkyPaletteModel.
    /Game/Art/Materials/M_Nube       Nubes K5: Default Lit con la hoja de paleta, tinte de tormenta y un
                                     poco de emisivo para que la base no se vuelva roca.
    /Game/Art/Materials/M_SkyDisc    Discos de sol y luna: Unlit, hoja de paleta × «Brightness».
"""

import json
import math
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from material_utils import (  # noqa: E402 - necesita el sys.path de arriba
    ART_MATERIALS_PATH, ISLAND_JSON, connect, custom, expr, finish, hlsl_float3, palette_linear,
    recreate_material, recreate_material_fresh, rgb, scalar_param, to_property, vector_param,
)

# Constantes de FIslandLayout / Tools/Island (layout.py, terrain.py).
OCEAN_FLOOR_M = -30.0
SHELF_SLOPE = 0.12
PLATEAU_EDGE = 0.55
SEABED_M = -6.0

GERSTNER_LOOP = r"""
    {
        float3 off = 0;
        [unroll] for (int i = 0; i < 4; ++i)
        {
            float2 d = normalize(waves[i].xy);
            float k = 6.28318530718 / waves[i].z;
            float a = waves[i].w;
            float speed = sqrt(981.0 / k);
            float ph = k * (dot(d, pos) - speed * T);
            float q = steep[i] / (k * a * 4.0);
            off += float3(q * a * d.x * cos(ph), q * a * d.y * cos(ph), a * sin(ph));
        }
        RESULT
    }
"""

# Olas y su pendiente en el píxel (posición sin desplazar, cm). Desde F6 el mar no es facetado
# (Rodrigo pidió agua suave): la normal sale de diferencias finitas de las mismas olas de Gerstner.
WAVE_ARRAYS = r"""
float4 waves[4] = { W0, W1, W2, W3 };
float steep[4] = { S0, S1, S2, S3 };
"""

OCEAN_WPO_HLSL = r"""
float4 waves[4] = { W0, W1, W2, W3 };
float steep[4] = { S0, S1, S2, S3 };
float2 pos = WP.xy;
float3 result = 0;
""" + GERSTNER_LOOP.replace("RESULT", "result = off;") + "return result;"

OCEAN_NORMAL_HLSL = WAVE_ARRAYS + r"""
float e = 60.0;
float2 offs[3] = { float2(0, 0), float2(e, 0), float2(0, e) };
float3 pts[3];
[unroll] for (int k = 0; k < 3; ++k)
{
    float2 pos = WP.xy + offs[k];
""" + GERSTNER_LOOP.replace("RESULT", "pts[k] = float3(pos, 0) + off;") + r"""
}
float3 n = normalize(cross(pts[1] - pts[0], pts[2] - pts[0]));
n *= sign(n.z + 1e-5);
// Lejos, el oleaje fino parpadea: la normal se aplana hacia la vertical.
float fade = saturate((distance(WP.xy, Cam.xy) - 8000.0) / 25000.0);
return normalize(lerp(n, float3(0, 0, 1), fade * 0.8));
"""


def island_height_hlsl() -> str:
    """Altura analítica del fondo (m) en «p» (m): copia de layout.height_at sin colinas (solo mar)."""
    with open(ISLAND_JSON, "r", encoding="utf-8") as f:
        data = json.load(f)
    lines = [f"float height = {OCEAN_FLOOR_M:.3f};"]
    for isl in data["islands"]:
        cx, cy = isl["center"]
        radius = float(isl["radius"])
        minor = radius * min(max(float(isl.get("aspect", 1.0)), 1e-4), 1.0)
        rot = math.radians(float(isl.get("rotationDeg", 0.0)))
        coast = " + ".join(f"{float(a):.5f} * cos({int(k)}.0 * th + {math.radians(float(ph)):.5f})"
                           for k, a, ph in isl.get("coast", [])) or "0.0"
        lines.append(f"""{{
    float2 d = p - float2({cx:.3f}, {cy:.3f});
    float lx = d.x * {math.cos(rot):.6f} + d.y * {math.sin(rot):.6f};
    float ly = -d.x * {math.sin(rot):.6f} + d.y * {math.cos(rot):.6f};
    float ux = lx / {radius:.4f};
    float uy = ly / {minor:.4f};
    float th = atan2(uy, ux);
    float nd = sqrt(ux * ux + uy * uy) / max(1.0 + {coast}, 0.2);
    float shelf = max({OCEAN_FLOOR_M:.3f}, -(nd - 1.0) * {minor:.4f} * {SHELF_SLOPE:.4f});
    float land = {float(isl["maxHeight"]):.3f} * (1.0 - smoothstep({PLATEAU_EDGE}, 1.0, nd));
    height = max(height, nd >= 1.0 ? shelf : land);
}}""")
    for rock in data.get("rocks", []):
        rx, ry = rock["center"]
        lines.append(f"height += {float(rock['height']):.3f} * (1.0 - smoothstep(0.0, {float(rock['radius']):.3f}, "
                     f"distance(p, float2({rx:.3f}, {ry:.3f}))));")
    lines.append(f"float depth = -max(height, {SEABED_M:.1f});")
    return "\n".join(lines)


def ocean_color_hlsl() -> str:
    shallow, mid, deep = (hlsl_float3(palette_linear(n)) for n in ("agua_somera", "agua_media", "agua_profunda"))
    foam, lilac, storm = (hlsl_float3(palette_linear(n)) for n in ("blanco_nube", "gris_lila", "gris_tormenta"))
    return WAVE_ARRAYS + r"""
float2 pos = WP.xy;
float crest = 0;
""" + GERSTNER_LOOP.replace("RESULT", "crest = off.z;") + r"""
float2 p = WP.xy / 100.0;
""" + island_height_hlsl() + f"""
float storm = saturate((SeaState - 0.45) * 2.5);
// Degradado continuo de profundidad: orilla, medio y profundo fundidos.
float3 colour = lerp({shallow}, {mid}, smoothstep(0.6, 2.8, depth));
colour = lerp(colour, {deep}, smoothstep(3.2, 8.0, depth));
colour = lerp(colour, {storm}, storm * 0.55);
// Crestas blancas suaves con mar de temporal.
float crestN = crest / max(W0.w + W1.w + W2.w + W3.w, 1.0);
float whitecap = smoothstep(0.5, 0.8, crestN) * smoothstep(0.35, 0.6, SeaState);
colour = lerp(colour, lerp({foam}, {lilac}, storm), whitecap * 0.75);
// Espuma de orilla: una banda blanda que late con un periodo de 5 s.
float pulse = 0.5 + 0.5 * sin(T * 1.2566 + dot(WP.xy, float2(0.0011, 0.0007)));
float foamDepth = (0.18 + 0.22 * pulse) * (1.0 + SeaState);
float shore = (1.0 - smoothstep(foamDepth * 0.35, foamDepth, depth)) * step(-2.0, depth);
return lerp(colour, {foam}, shore * ShoreFoam);
"""


def build_ocean():
    m = recreate_material_fresh("M_Ocean")
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    m.set_editor_property("tangent_space_normal", False)
    # Visible también desde debajo del agua (nado).
    m.set_editor_property("two_sided", True)

    wp = expr(m, unreal.MaterialExpressionWorldPosition, -1600, 0)
    wp.set_editor_property("world_position_shader_offset", unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
    cam = expr(m, unreal.MaterialExpressionCameraPositionWS, -1600, 60)
    time = scalar_param(m, "WaveTime", 0.0, -1600, 120)
    sea_state = scalar_param(m, "SeaState", 0.15, -1600, 200)
    facet = scalar_param(m, "FacetSize", 380.0, -1600, 260)
    defaults = [((0.8, 0.6, 6000.0, 18.0), 0.35), ((0.6, 0.8, 3100.0, 9.0), 0.35),
                ((1.0, -0.15, 1700.0, 5.0), 0.35), ((-0.3, 0.95, 900.0, 2.5), 0.35)]
    waves, steeps = [], []
    for i, (vec, steep) in enumerate(defaults):
        w = vector_param(m, f"Wave{i}", vec, -1600, 300 + i * 180)
        rgba = expr(m, unreal.MaterialExpressionAppendVector, -1400, 300 + i * 180)
        connect(w, "", rgba, "A")
        connect(w, "A", rgba, "B")
        waves.append(rgba)
        steeps.append(scalar_param(m, f"Steepness{i}", steep, -1600, 380 + i * 180))
    gerstner_inputs = ["WP", "T", "W0", "W1", "W2", "W3", "S0", "S1", "S2", "S3"]

    def wire(node):
        connect(wp, "", node, "WP")
        connect(time, "", node, "T")
        for i in range(4):
            connect(waves[i], "", node, f"W{i}")
            connect(steeps[i], "", node, f"S{i}")

    wpo = custom(m, -1000, 0, OCEAN_WPO_HLSL, gerstner_inputs, unreal.CustomMaterialOutputType.CMOT_FLOAT3, "GerstnerOffset")
    wire(wpo)
    to_property(wpo, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

    normal = custom(m, -1000, 400, OCEAN_NORMAL_HLSL, gerstner_inputs + ["FacetSize", "Cam"],
                    unreal.CustomMaterialOutputType.CMOT_FLOAT3, "FacetNormal")
    wire(normal)
    connect(facet, "", normal, "FacetSize")
    connect(cam, "", normal, "Cam")
    to_property(normal, "", unreal.MaterialProperty.MP_NORMAL)

    colour = custom(m, -1000, 800, ocean_color_hlsl(), gerstner_inputs + ["FacetSize", "SeaState", "ShoreFoam"],
                    unreal.CustomMaterialOutputType.CMOT_FLOAT3, "FacetColour")
    wire(colour)
    connect(facet, "", colour, "FacetSize")
    connect(sea_state, "", colour, "SeaState")
    # La profundidad del mar sale del relieve analítico, que se separa de la malla del terreno hasta
    # ~3 m: en las playas de poca pendiente pintaba bandas blancas anchas y planas. La espuma de orilla
    # la dibuja ahora M_TerrainToon por la altura real del suelo; aquí queda a 0 salvo que se pida.
    connect(scalar_param(m, "ShoreFoam", 0.0, -1600, 1100), "", colour, "ShoreFoam")
    to_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)

    rough = expr(m, unreal.MaterialExpressionConstant, -300, 200)
    rough.set_editor_property("r", 1.0)
    to_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    spec = expr(m, unreal.MaterialExpressionConstant, -300, 280)
    spec.set_editor_property("r", 0.0)
    to_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    finish(m)


SKY_HLSL = r"""
float3 d = normalize(-CV);
float e = d.z;
float t1 = smoothstep(0.0, 0.32, e);
float t2 = smoothstep(0.22, 0.85, e);
float3 col = lerp(lerp(Horizon, Middle, t1), Zenith, t2);
col = e < 0.0 ? Horizon : col;
float3 s = normalize(SunDir + float3(0, 0, 1e-4));
float glow = pow(saturate(dot(d, s)), 5.0) * GlowStrength * (1.0 - smoothstep(0.0, 0.45, e));
return lerp(col, Glow, saturate(glow));
"""


def build_sky_gradient():
    m = recreate_material("M_SkyGradient", ART_MATERIALS_PATH)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("is_sky", True)
    cv = expr(m, unreal.MaterialExpressionCameraVectorWS, -900, 0)
    params = [("Horizon", "HorizonColor", palette_linear("cielo_dia")),
              ("Middle", "MiddleColor", palette_linear("cielo_dia")),
              ("Zenith", "ZenithColor", palette_linear("cielo_cenit")),
              ("Glow", "GlowColor", palette_linear("fuego")),
              ("SunDir", "SunDirection", (0.0, 0.0, 1.0))]
    sky = custom(m, -300, 0, SKY_HLSL, ["CV"] + [p[0] for p in params] + ["GlowStrength"],
                 unreal.CustomMaterialOutputType.CMOT_FLOAT3, "SkyGradient")
    connect(cv, "", sky, "CV")
    for i, (pin, name, value) in enumerate(params):
        connect(rgb(m, vector_param(m, name, value, -900, 100 + i * 120), -650, 100 + i * 120), "", sky, pin)
    connect(scalar_param(m, "GlowStrength", 0.0, -900, 760), "", sky, "GlowStrength")
    to_property(sky, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)


def palette_sample(m, palette, x: int, y: int):
    uv = expr(m, unreal.MaterialExpressionTextureCoordinate, x - 250, y)
    sample = expr(m, unreal.MaterialExpressionTextureSample, x, y)
    sample.set_editor_property("texture", palette)
    sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    connect(uv, "", sample, "UVs")
    return sample


def build_cloud(palette):
    m = recreate_material("M_Nube", ART_MATERIALS_PATH)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    sample = palette_sample(m, palette, -700, 0)
    tint = rgb(m, vector_param(m, "Tint", (1.0, 1.0, 1.0), -700, 250), -450, 250)
    base = expr(m, unreal.MaterialExpressionMultiply, -250, 0)
    connect(sample, "RGB", base, "A")
    connect(tint, "", base, "B")
    to_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    # Un poco de luz propia: la base gris lila no cae a negro con el sol bajo.
    glow = expr(m, unreal.MaterialExpressionMultiply, -100, 150)
    connect(base, "", glow, "A")
    connect(scalar_param(m, "SelfLight", 0.35, -400, 400), "", glow, "B")
    to_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    for value, prop in ((1.0, unreal.MaterialProperty.MP_ROUGHNESS), (0.0, unreal.MaterialProperty.MP_SPECULAR)):
        const = expr(m, unreal.MaterialExpressionConstant, -250, 450 + int(value) * 80)
        const.set_editor_property("r", value)
        to_property(const, "", prop)
    finish(m)


def build_sky_disc(palette):
    m = recreate_material("M_SkyDisc", ART_MATERIALS_PATH)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    sample = palette_sample(m, palette, -500, 0)
    bright = expr(m, unreal.MaterialExpressionMultiply, -150, 0)
    connect(sample, "RGB", bright, "A")
    connect(scalar_param(m, "Brightness", 4.0, -500, 250), "", bright, "B")
    to_property(bright, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)


def build_all(palette):
    build_ocean()
    build_sky_gradient()
    build_cloud(palette)
    build_sky_disc(palette)


if __name__ == "__main__":
    # Directo (Tools/unreal_python.ps1 -Script Tools/Unreal/materials_lowpoly.py): agua y cielo sin rehacer M_Toon.
    _palette = unreal.load_asset(f"{ART_MATERIALS_PATH}/T_Paleta")
    if _palette is None:
        raise RuntimeError(f"Falta {ART_MATERIALS_PATH}/T_Paleta: ejecuta antes build_materials.py")
    build_all(_palette)
