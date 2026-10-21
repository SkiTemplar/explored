"""Crea los materiales base del juego por código (idempotente).

Se ejecuta con:
    powershell -File Tools/unreal_python.ps1 -Script Tools/Unreal/build_materials.py

Materiales:
    /Game/Materials/M_Ocean    agua low poly (materials_lowpoly.py): facetas planas, bandas de paleta por
                                profundidad y espuma de orilla; olas de Gerstner (mismas que FOceanWaves).
    M_SkyGradient, M_Nube, M_SkyDisc  cielo low poly (materials_lowpoly.py).
    /Game/Materials/M_Stars    cúpula de estrellas aditiva controlada por el parámetro «Night».
    /Game/Materials/M_PP_Body  postproceso del cuerpo: viñeta (color y fuerza) y desaturación
                                según FBodySignals (VignetteAmount, TintColor, DesaturationAmount).
    /Game/Art/Materials/T_Paleta  hoja de paleta de Tools/Style (Art/Export/Style/T_Paleta.png, 32 x 3):
                                filtro Nearest, sin mipmaps, sRGB, compresión sin pérdidas.
    /Game/Art/Materials/M_Toon  material del estilo: Default Lit, base color = T_Paleta muestreada con
                                el UV 0 (cada cara en el centro de su celda), rugosidad 1, especular 0.
                                «PaletteIndex» + «UsePaletteIndex» = 1 fuerzan una celda (primitivas de
                                reserva de AExploredPlaceholder).
    /Game/Art/Materials/M_PP_Toon  postproceso de sombreado por bandas (GDD §11): cuantiza la luz que
                                recibe cada píxel Default Lit (color de escena / base color) en pasos de
                                «BandStops» diafragmas; el océano (Single Layer Water), el cielo y lo
                                lejano (niebla) no se tocan. Lo usa el PostProcess_Global del mapa.
    /Game/Art/Materials/M_ToonCesped  césped K6: M_Toon de dos caras con viento × VertexColor.A y fundido
                                por distancia (materials_cesped.py).

El terreno, la vegetación y los materiales low poly del arte v1 se borraron en el reset v2;
el material maestro nuevo (M_Toon con hoja de paleta) llega en F2 (docs/v3/plan.md).
"""

import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from material_utils import (  # noqa: E402 - necesita el sys.path de arriba
    ART_MATERIALS_PATH, MATERIALS_PATH, REPO_ROOT, add_toon_wind, connect, custom, expr, finish, recreate_material,
    to_property,
)
import materials_cesped  # noqa: E402
import materials_lowpoly  # noqa: E402

PALETTE_PNG = os.path.join(REPO_ROOT, "Art", "Export", "Style", "T_Paleta.png")
# Columnas de la hoja; los índices 0-31 van en la fila central (v = 0,5) y los 32-63 en la superior.
PALETTE_SIZE = 32
PALETTE_ROWS = 3
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


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


# Bandas de sombreado: la luz recibida es la luminancia de la escena entre la del base color; se
# redondea en escala logarítmica (pasos de BandStops diafragmas) y se reescala el píxel. Sobre mallas
# de caras planas deja 2-3 tonos por objeto. Solo Default Lit (id 1): el agua (Single Layer Water) y
# el cielo (sin GBuffer) pasan intactos; se desvanece con la distancia para no dibujar curvas de
# nivel en la niebla.
PP_TOON_HLSL = r"""
float3 Lum = float3(0.2126, 0.7152, 0.0722);
float3 Color = SceneColor.rgb;
float Albedo = dot(max(BaseColor.rgb, 0.0), Lum);
float Light = dot(max(Color, 0.0), Lum);
if (abs(ShadingModel.r - 1.0) > 0.5 || Albedo < 0.02 || Light < 1e-5)
{
    return Color;
}
float Received = Light / Albedo;
float Steps = max(BandStops, 0.05);
// Paso suave (ancho 0,3 de banda) en el borde: sin él, el ruido de la iluminación global hace
// saltar los píxeles de una banda a otra y el borde se ve granulado.
float Level = log2(Received) / Steps;
float Base = floor(Level);
float Banded = exp2((Base + smoothstep(0.35, 0.65, Level - Base)) * Steps);
float Fade = 1.0 - saturate((Depth.r - FadeStartCm) / max(FadeRangeCm, 1.0));
return Color * lerp(1.0, Banded / Received, saturate(Strength) * Fade);
"""


def build_pp_toon():
    m = recreate_material("M_PP_Toon", ART_MATERIALS_PATH)
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    m.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_BEFORE_DOF)

    inputs = []
    for y, (name, scene_id) in enumerate((
            ("SceneColor", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0),
            ("BaseColor", unreal.SceneTextureId.PPI_BASE_COLOR),
            ("ShadingModel", unreal.SceneTextureId.PPI_SHADING_MODEL_ID),
            ("Depth", unreal.SceneTextureId.PPI_SCENE_DEPTH))):
        node = expr(m, unreal.MaterialExpressionSceneTexture, -700, y * 140)
        node.set_editor_property("scene_texture_id", scene_id)
        inputs.append((name, node, "Color"))
    for y, (name, value) in enumerate((("BandStops", 0.5), ("Strength", 1.0),
                                       ("FadeStartCm", 6000.0), ("FadeRangeCm", 12000.0))):
        node = expr(m, unreal.MaterialExpressionScalarParameter, -700, 600 + y * 100)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("default_value", value)
        inputs.append((name, node, ""))

    toon = custom(m, -300, 200, PP_TOON_HLSL, [name for name, _, _ in inputs],
                  unreal.CustomMaterialOutputType.CMOT_FLOAT3, "ToonBands")
    for name, node, out in inputs:
        # Las SceneTexture dan float4 (Color = RGBA): el HLSL toma .rgb o .r.
        connect(node, out, toon, name)
    to_property(toon, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)

# ---------------------------------------------------------------------------
# Estilo low poly (F3a): hoja de paleta y M_Toon provisional
# ---------------------------------------------------------------------------

def import_palette_texture() -> unreal.Texture2D:
    """Importa la hoja de paleta con muestreo exacto (cada texel es un color de paleta.json)."""
    if not os.path.isfile(PALETTE_PNG):
        raise RuntimeError(f"Falta {PALETTE_PNG} (uv run style-paleta en Tools/Style)")
    existing = f"{ART_MATERIALS_PATH}/T_Paleta"
    # Reimportar una textura que ya usan M_Toon y M_PP_Toon tumba el commandlet de UE 5.6
    # («Assertion failed: !IsRooted()» en MaterialEditor): si existe, solo se reajusta. Para
    # reimportarla tras cambiar la paleta: EXPLORED_REIMPORT_PALETTE=1.
    if unreal.EditorAssetLibrary.does_asset_exist(existing) and os.environ.get("EXPLORED_REIMPORT_PALETTE") != "1":
        return configure_palette_texture(unreal.load_asset(existing))
    release_palette_users()
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
    return configure_palette_texture(texture)


def release_palette_users():
    """Vacía los materiales que muestrean T_Paleta antes de reimportarla: con la textura en uso,
    la reimportación tumba el commandlet («!IsRooted()»). main() los reconstruye a continuación."""
    for name in ("M_Toon", "M_PP_Toon", "M_Nube", "M_SkyDisc"):
        path = f"{ART_MATERIALS_PATH}/{name}"
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            material = unreal.EditorAssetLibrary.load_asset(path)
            unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
            unreal.MaterialEditingLibrary.recompile_material(material)
            unreal.EditorAssetLibrary.save_loaded_asset(material)


def configure_palette_texture(texture: unreal.Texture2D) -> unreal.Texture2D:
    """Muestreo exacto: Nearest, sin mipmaps, sRGB y sin compresión por bloques."""
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
float Cell = clamp(floor(Index), 0.0, 2.0 * PaletteSize - 1.0);
float Extra = step(PaletteSize, Cell);
float2 Forced = float2((Cell - Extra * PaletteSize + 0.5) / PaletteSize, lerp(0.5, 0.5 / PaletteRows, Extra));
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

    rows = expr(m, unreal.MaterialExpressionConstant, -900, 400)
    rows.set_editor_property("r", float(PALETTE_ROWS))
    cell_uv = custom(m, -600, 100, TOON_UV_HLSL, ["UV", "Index", "UseIndex", "PaletteSize", "PaletteRows"],
                     unreal.CustomMaterialOutputType.CMOT_FLOAT2, "PaletteCellUV")
    connect(uv, "", cell_uv, "UV")
    connect(index, "", cell_uv, "Index")
    connect(use_index, "", cell_uv, "UseIndex")
    connect(size, "", cell_uv, "PaletteSize")
    connect(rows, "", cell_uv, "PaletteRows")

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
    add_toon_wind(m)
    finish(m)


def main():
    unreal.EditorAssetLibrary.make_directory(MATERIALS_PATH)
    build_stars()
    build_pp_body()
    unreal.EditorAssetLibrary.make_directory(ART_MATERIALS_PATH)
    palette = import_palette_texture()
    build_toon(palette)
    materials_cesped.build_toon_cesped(palette)
    build_pp_toon()
    materials_lowpoly.build_all(palette)


main()
