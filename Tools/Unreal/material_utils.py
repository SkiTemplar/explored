"""Utilidades comunes para crear materiales por código en el editor de Unreal (build_materials.py,
materials_lowpoly.py): creación idempotente, nodos, conexiones y colores de la paleta única.
"""

import json
import os

import unreal

MATERIALS_PATH = "/Game/Materials"
ART_MATERIALS_PATH = "/Game/Art/Materials"
REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
PALETTE_JSON = os.path.join(REPO_ROOT, "Tools", "Style", "paleta.json")
ISLAND_JSON = os.path.join(REPO_ROOT, "Content", "Data", "island.json")
MEL = unreal.MaterialEditingLibrary
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()

MESH_USAGES = (
    # Terreno, árboles y rocas pueden usar Nanite: sin este flag el juego (no el editor) sustituye el
    # material por el Default Material y lo avisa solo en el log («missing bUsedWithNanite»).
    unreal.MaterialUsage.MATUSAGE_NANITE,
    # La vegetación decorativa y las nubes se dibujan con componentes instanciados.
    unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES,
)


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


def recreate_material_fresh(name: str, folder: str = MATERIALS_PATH) -> unreal.Material:
    """Borra el asset y lo crea desde cero: delete_all_material_expressions no limpia los nodos de
    salida especiales (p. ej. SingleLayerWaterMaterialOutput) y el shader dejaría de compilar."""
    full = f"{folder}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(full):
        unreal.EditorAssetLibrary.delete_asset(full)
    material = ASSET_TOOLS.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
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


def scalar_param(material, name: str, value: float, x: int, y: int):
    node = expr(material, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", value)
    return node


def vector_param(material, name: str, color, x: int, y: int):
    node = expr(material, unreal.MaterialExpressionVectorParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", unreal.LinearColor(*color, 1.0) if len(color) == 3 else unreal.LinearColor(*color))
    return node


def rgb(material, src, x: int, y: int):
    """ComponentMask a 3 canales (los VectorParameter salen como float4 por su pin por defecto)."""
    mask = expr(material, unreal.MaterialExpressionComponentMask, x, y)
    for channel in ("r", "g", "b", "a"):
        mask.set_editor_property(channel, channel != "a")
    connect(src, "", mask, "")
    return mask


def finish(material):
    # Los flags de uso (Nanite, instanciado) son de material de superficie opaca.
    is_surface = material.get_editor_property("material_domain") == unreal.MaterialDomain.MD_SURFACE
    if is_surface and material.get_editor_property("blend_mode") == unreal.BlendMode.BLEND_OPAQUE:
        for usage in MESH_USAGES:
            MEL.set_material_usage(material, usage)
    MEL.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    unreal.log(f"[Explored] Material listo: {material.get_path_name()}")


def srgb_to_linear(channel: int) -> float:
    f = channel / 255.0
    return f / 12.92 if f <= 0.04045 else ((f + 0.055) / 1.055) ** 2.4


def palette_linear(name: str) -> tuple[float, float, float]:
    """Color de Tools/Style/paleta.json en lineal (los VectorParameter y las constantes HLSL lo son)."""
    with open(PALETTE_JSON, "r", encoding="utf-8") as f:
        colors = {c["nombre"]: c["hex"] for c in json.load(f)["colores"]}
    if name not in colors:
        raise RuntimeError(f"paleta.json no tiene el color {name}")
    h = colors[name].lstrip("#")
    return tuple(srgb_to_linear(int(h[i:i + 2], 16)) for i in (0, 2, 4))


def hlsl_float3(color) -> str:
    return "float3({:.5f}, {:.5f}, {:.5f})".format(*color)


# Viento en palmas y bambú (F6): desplazamiento de vértices (WPO) que crece con la altura local de la
# malla. Apagado por defecto (WindSway = 0): solo lo encienden las instancias dinámicas de
# AExploredPlaceholder para SM_Palm* y SM_Bamboo*; WindStrength (0-1) lo pone el clima.
TOON_WIND_HLSL = r"""
float h = saturate((WorldPos.z - ObjectPos.z) / 750.0);
float mask = h * h;
float phase = dot(WorldPos.xy, float2(0.0012, 0.0009));
float s = sin(T * (1.1 + Strength) + phase) * 0.65 + sin(T * (2.7 + 2.0 * Strength) + phase * 1.9) * 0.2;
float amp = (10.0 + 70.0 * Strength) * Sway * mask;
return float3(s * amp, s * amp * 0.55, 0.0);
"""


def add_toon_wind(material) -> bool:
    """Añade WindSway/WindStrength y el WPO de viento a M_Toon. Devuelve False si ya los tenía."""
    if "WindSway" in [str(n) for n in MEL.get_scalar_parameter_names(material)]:
        return False
    world = expr(material, unreal.MaterialExpressionWorldPosition, -900, 600)
    # Altura sobre el pivote (en la base de la malla): la copa se mueve y el pie no.
    local = expr(material, unreal.MaterialExpressionObjectPositionWS, -900, 700)
    time = expr(material, unreal.MaterialExpressionTime, -900, 800)
    sway = scalar_param(material, "WindSway", 0.0, -900, 900)
    strength = scalar_param(material, "WindStrength", 0.3, -900, 1000)
    wind = custom(material, -500, 750, TOON_WIND_HLSL, ["WorldPos", "ObjectPos", "T", "Sway", "Strength"],
                  unreal.CustomMaterialOutputType.CMOT_FLOAT3, "ToonWind")
    connect(world, "", wind, "WorldPos")
    connect(local, "", wind, "ObjectPos")
    connect(time, "", wind, "T")
    connect(sway, "", wind, "Sway")
    connect(strength, "", wind, "Strength")
    to_property(wind, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    return True
