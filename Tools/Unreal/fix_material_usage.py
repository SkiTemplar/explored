"""
Marca los materiales generados (/Game/Generated/Materials, sin instancias) como usables con
mallas instanciadas y Nanite. Sin esos usos, en el juego (no en el editor) la vegetación y las
formaciones de roca se dibujan con el material por defecto.

Uso: Tools/unreal_python.ps1 -Script Tools/Unreal/fix_material_usage.py
"""
import unreal

USAGES = ('used_with_instanced_static_meshes', 'used_with_nanite')


def main() -> None:
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    changed = 0
    for data in registry.get_assets_by_path('/Game/Generated/Materials', recursive=True):
        if str(data.asset_class_path.asset_name) != 'Material':
            continue
        material = data.get_asset()
        missing = [u for u in USAGES if not material.get_editor_property(u)]
        if not missing:
            continue
        for usage in missing:
            material.set_editor_property(usage, True)
        unreal.MaterialEditingLibrary.recompile_material(material)
        unreal.EditorAssetLibrary.save_loaded_asset(material)
        unreal.log(f'[usage] {data.asset_name}: {", ".join(missing)}')
        changed += 1
    unreal.log(f'[usage] {changed} materiales actualizados')


main()
