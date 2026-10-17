"""
Asigna materiales de color plano a los slots del set low poly (/Game/Generated/Meshes/LowPoly)
según el nombre del slot que trae el FBX (Wood, Bark_NormalTree, Leaves, Flowers…). Las
texturas incrustadas de esos FBX no sobreviven a la importación y los slots quedaban con
WorldGridMaterial (tablero). El estilo de Quaternius es de colores planos, así que basta un
color por material. Crea los materiales en /Game/Generated/Materials/LowPolyFlat si faltan.

Uso: Tools/unreal_python.ps1 -Script Tools/Unreal/fix_lowpoly_materials.py
"""
import unreal

MATERIAL_DIR = '/Game/Generated/Materials/LowPolyFlat'
# Nombre -> (color lineal, rugosidad, dos caras). Tonos saturados pero no planos de más.
PALETTE = {
    'M_LP_Wood': ((0.16, 0.075, 0.035), 0.9, False),
    'M_LP_Leaf': ((0.11, 0.38, 0.07), 0.8, True),
    'M_LP_LeafDark': ((0.055, 0.22, 0.05), 0.8, True),
    'M_LP_Flower': ((0.75, 0.12, 0.25), 0.7, True),
    'M_LP_Yellow': ((0.85, 0.6, 0.06), 0.7, True),
    'M_LP_Cyan': ((0.08, 0.45, 0.55), 0.7, True),
}
# Palabra clave del slot (minúsculas) -> material. El orden importa: «darkgreen» antes que «green».
KEYWORDS = [
    ('darkgreen', 'M_LP_LeafDark'),
    ('wood', 'M_LP_Wood'), ('bark', 'M_LP_Wood'), ('brown', 'M_LP_Wood'),
    ('flower', 'M_LP_Flower'), ('yellow', 'M_LP_Yellow'), ('cyan', 'M_LP_Cyan'),
    ('leaves', 'M_LP_Leaf'), ('leaf', 'M_LP_Leaf'), ('green', 'M_LP_Leaf'), ('grass', 'M_LP_Leaf'),
    ('atlas', 'M_LP_Leaf'),
]


def get_or_create_material(name: str) -> unreal.Material:
    path = f'{MATERIAL_DIR}/{name}'
    color, roughness, two_sided = PALETTE[name]
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        # Ya existe: solo se actualiza el color (para retocar la paleta sin recrear nada).
        material = unreal.EditorAssetLibrary.load_asset(path)
        expression = unreal.MaterialEditingLibrary.get_material_property_input_node(
            material, unreal.MaterialProperty.MP_BASE_COLOR)
        if isinstance(expression, unreal.MaterialExpressionConstant3Vector):
            expression.set_editor_property('constant', unreal.LinearColor(color[0], color[1], color[2], 1.0))
            unreal.MaterialEditingLibrary.recompile_material(material)
            unreal.EditorAssetLibrary.save_loaded_asset(material)
        return material
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = tools.create_asset(name, MATERIAL_DIR, unreal.Material, unreal.MaterialFactoryNew())
    lib = unreal.MaterialEditingLibrary
    base = lib.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -400, 0)
    base.set_editor_property('constant', unreal.LinearColor(color[0], color[1], color[2], 1.0))
    lib.connect_material_property(base, '', unreal.MaterialProperty.MP_BASE_COLOR)
    rough = lib.create_material_expression(material, unreal.MaterialExpressionConstant, -400, 200)
    rough.set_editor_property('r', roughness)
    lib.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    material.set_editor_property('two_sided', two_sided)
    lib.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def material_for_slot(slot_name: str):
    lowered = slot_name.lower()
    for keyword, material in KEYWORDS:
        if keyword in lowered:
            return material
    return None


def main() -> None:
    materials = {name: get_or_create_material(name) for name in PALETTE}
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    changed = 0
    for data in registry.get_assets_by_path('/Game/Generated/Meshes/LowPoly', recursive=True):
        if str(data.asset_class_path.asset_name) != 'StaticMesh':
            continue
        mesh = data.get_asset()
        dirty = False
        for index, slot in enumerate(mesh.static_materials):
            current = slot.material_interface
            if current is not None and current.get_name() != 'WorldGridMaterial':
                continue
            target = material_for_slot(str(slot.material_slot_name))
            if target is None:
                unreal.log_warning(f'[lpfix] {data.asset_name}: slot «{slot.material_slot_name}» sin regla')
                continue
            mesh.set_material(index, materials[target])
            dirty = True
        if dirty:
            unreal.EditorAssetLibrary.save_loaded_asset(mesh)
            changed += 1
    unreal.log(f'[lpfix] {changed} mallas con materiales nuevos')


main()
