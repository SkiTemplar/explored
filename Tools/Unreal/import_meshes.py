"""
Tools/Unreal/import_meshes.py

Script de Python del editor de Unreal Engine 5.6 (módulo `unreal` embebido)
que importa el kit de vegetación y rocas generado por
Tools/Blender/run_all.py, leyendo Art/Export/Meshes/manifest.json.

IMPORTANTE: este script NO se ejecuta como parte de este encargo. Lo lanza
el responsable principal desde dentro del editor de UE 5.6 (consola Python
del editor, o `-run=pythonscript`). Aquí queda preparado y comentado con las
APIs `unreal.*` que usa para que se puedan verificar contra la build real
de 5.6 antes de correrlo (el editor no estaba disponible al escribirlo).

Qué hace, en orden:
    1. Importa cada FBX a /Game/Generated/Meshes/<Familia>/ con
       unreal.AssetImportTask (automated=True, replace_existing=True,
       save=True) y unreal.FbxImportUI + unreal.FbxStaticMeshImportData
       para traer el color de vértice (VertexColorImportOption.REPLACE) sin
       generar materiales ni colisión automática (los del kit se aplican
       aparte).
    2. Crea o reutiliza 4 materiales simples en /Game/Generated/Materials/
       (M_Bark, M_Leaf, M_Rock, M_Grass) vía unreal.MaterialEditingLibrary:
       un color base constante (MaterialExpressionConstant3Vector)
       multiplicado (MaterialExpressionMultiply) por el atributo de color
       de vértice de la malla (MaterialExpressionVertexColor), conectado a
       Base Color; una constante simple a Roughness.
    3. Asigna esos materiales a los slots de cada malla importada, en el
       mismo orden que anota manifest.json (material_slots), vía
       unreal.EditorStaticMeshLibrary.set_material (con una vía alternativa
       de reserva por si el nombre exacto del método difiere en 5.6).
    4. Activa Nanite (StaticMesh.nanite_settings.enabled) en las mallas de
       las categorías 'rock' y 'tree' (dosel/copas: la geometría con más
       triángulos y más beneficio de Nanite).
    5. Genera colisión simple de tipo NDOP18 (envolvente de 18 caras, buena
       aproximación barata para formas orgánicas romas) en las rocas vía
       unreal.EditorStaticMeshLibrary.add_simple_collisions.

APIs de `unreal` usadas (para verificación rápida contra la build real):
    unreal.AssetImportTask, unreal.FbxImportUI, unreal.FbxStaticMeshImportData,
    unreal.VertexColorImportOption, unreal.AssetToolsHelpers.get_asset_tools(),
    unreal.EditorAssetLibrary, unreal.MaterialEditingLibrary,
    unreal.MaterialExpressionVertexColor, unreal.MaterialExpressionConstant3Vector,
    unreal.MaterialExpressionMultiply, unreal.MaterialExpressionConstant,
    unreal.MaterialProperty, unreal.EditorStaticMeshLibrary,
    unreal.ScriptingCollisionShapeType, unreal.LinearColor.
"""

import json
import os

import unreal

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
EXPORT_DIR = os.path.join(REPO_ROOT, 'Art', 'Export', 'Meshes')
MANIFEST_PATH = os.path.join(EXPORT_DIR, 'manifest.json')

MESH_DEST_ROOT = '/Game/Generated/Meshes'
MATERIAL_DEST_PATH = '/Game/Generated/Materials'

FAMILY_FOLDERS = {
    'palm': 'Palm',
    'tree': 'JungleTree',
    'shrub': 'Shrub',
    'rock': 'Rock',
    'grass': 'Grass',
    'debris': 'Debris',
}

# Nanite conviene sobre todo en la geometría más pesada del kit (rocas y
# árboles/palmeras con más triángulos); el sotobosque y la hierba no lo
# necesitan pero activarlo no hace daño si el responsable prefiere
# uniformidad. Se deja limitado a rock+tree por presupuesto de build time.
NANITE_CATEGORIES = {'rock', 'tree'}

MATERIAL_DEFS = {
    'M_Bark':  dict(base_color=(0.16, 0.10, 0.07), roughness=0.9),
    'M_Leaf':  dict(base_color=(0.07, 0.26, 0.10), roughness=0.5),
    'M_Rock':  dict(base_color=(0.36, 0.35, 0.33), roughness=0.85),
    'M_Grass': dict(base_color=(0.18, 0.42, 0.14), roughness=0.55),
    # Slots del kit de props (Tools/Blender/props, importado por import_props.py).
    # El color real lo pone el color de vértice: la base es casi blanca para no
    # oscurecerlo y solo matiza el material.
    'M_Wood':   dict(base_color=(0.95, 0.90, 0.85), roughness=0.8),
    'M_Stone':  dict(base_color=(0.92, 0.92, 0.90), roughness=0.9),
    'M_Metal':  dict(base_color=(0.90, 0.90, 0.92), roughness=0.45),
    'M_Fabric': dict(base_color=(0.95, 0.93, 0.90), roughness=0.95),
    'M_Paper':  dict(base_color=(0.97, 0.95, 0.90), roughness=0.9),
    'M_Glass':  dict(base_color=(0.80, 0.90, 0.95), roughness=0.1),
}


def _ensure_material(name):
    """Crea (si falta) uno de los 4 materiales estables del kit y lo deja
    en /Game/Generated/Materials/<name>: color base constante multiplicado
    por el atributo de color de vértice de la malla. Si ya existe, se
    reutiliza tal cual está (no se pisa un material que el responsable
    principal haya editado a mano)."""
    package_path = f'{MATERIAL_DEST_PATH}/{name}'
    if unreal.EditorAssetLibrary.does_asset_exist(package_path):
        return unreal.EditorAssetLibrary.load_asset(package_path)

    if name not in MATERIAL_DEFS:
        raise ValueError(f'Material fuera del kit estable: {name}')

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = asset_tools.create_asset(
        name, MATERIAL_DEST_PATH, unreal.Material, unreal.MaterialFactoryNew())

    cfg = MATERIAL_DEFS[name]

    vcol = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVertexColor, -420, 60)
    base = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant3Vector, -420, -100)
    base.set_editor_property('constant', unreal.LinearColor(*cfg['base_color'], 1.0))
    mul = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -160, 0)
    rough = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -420, 220)
    rough.set_editor_property('r', cfg['roughness'])

    unreal.MaterialEditingLibrary.connect_material_expressions(base, '', mul, 'A')
    unreal.MaterialEditingLibrary.connect_material_expressions(vcol, '', mul, 'B')
    unreal.MaterialEditingLibrary.connect_material_property(
        mul, '', unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(
        rough, '', unreal.MaterialProperty.MP_ROUGHNESS)

    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(package_path)
    return material


def _import_mesh(fbx_path, dest_path, mesh_name):
    """Importa un FBX como AssetImportTask automatizado. Devuelve el
    StaticMesh importado, o None si algo falló (se deja log de error, no
    se interrumpe el resto del lote)."""
    # Reimportar sobre un asset ya existente reutiliza los ajustes de import
    # GUARDADOS en su AssetImportData (incluida la escala) e ignora los de
    # esta tarea nueva: sin borrarlo antes, un cambio en import_uniform_scale
    # (o cualquier otra opción de import_mesh.py) no llega a aplicarse nunca
    # a una malla que ya se había importado una vez.
    full_path = f'{dest_path}/{mesh_name}'
    if unreal.EditorAssetLibrary.does_asset_exist(full_path):
        unreal.EditorAssetLibrary.delete_asset(full_path)

    task = unreal.AssetImportTask()
    task.set_editor_property('filename', fbx_path)
    task.set_editor_property('destination_path', dest_path)
    task.set_editor_property('destination_name', mesh_name)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', True)

    options = unreal.FbxImportUI()
    options.set_editor_property('import_mesh', True)
    options.set_editor_property('import_as_skeletal', False)
    options.set_editor_property('import_materials', False)
    options.set_editor_property('import_textures', False)

    smi = options.static_mesh_import_data
    smi.set_editor_property('combine_meshes', True)
    smi.set_editor_property('generate_lightmap_u_vs', True)
    smi.set_editor_property('auto_generate_collision', False)
    smi.set_editor_property('vertex_color_import_option',
                             unreal.VertexColorImportOption.REPLACE)
    # Sin esto, el importador de FBX de UE 5.6 ignora la unidad real del
    # fichero (Tools/Blender/lib/common.py exporta con apply_unit_scale=True,
    # 1 m Blender = 100 uu Unreal, verificado reimportando el FBX en Blender:
    # una palmera de 8,69 x 8,91 x 10,49 m) y deja la malla en centímetros
    # numéricamente iguales a los metros de origen: una palmera de ~10 m
    # queda de ~10 cm, invisible a la distancia normal de juego. Forzando el
    # factor de escala de importación se corrige ese «~100x» que faltaba.
    smi.set_editor_property('import_uniform_scale', 100.0)

    task.set_editor_property('options', options)

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    imported = task.get_editor_property('imported_object_paths')
    if not imported:
        unreal.log_error(f'[import_meshes] fallo al importar {fbx_path}')
        return None
    return unreal.load_asset(imported[0])


def _assign_materials(static_mesh, slot_names):
    """Asigna los 4 materiales estables a los slots de la malla, en el
    orden anotado en el manifest (mismo orden que se exportó desde
    Blender). Usa EditorStaticMeshLibrary.set_material; si esa función no
    existe con ese nombre exacto en 5.6, cae a reescribir la lista
    static_materials directamente."""
    # EditorStaticMeshLibrary.set_material (deprecado en 5.6) no falla pero tampoco asigna:
    # las mallas quedaban con WorldGridMaterial. StaticMesh.set_material sí lo hace.
    materials = [_ensure_material(name) for name in slot_names]
    slot_count = len(static_mesh.get_editor_property('static_materials'))
    if slot_count != len(materials):
        unreal.log_warning(f'[import_meshes] {static_mesh.get_name()}: {slot_count} slots en la malla, '
                           f'{len(materials)} en el manifiesto')
    for i, mat in enumerate(materials[:slot_count]):
        static_mesh.set_material(i, mat)
    assigned = [static_mesh.get_material(i) for i in range(slot_count)]
    if any(a is None or a.get_name() == 'WorldGridMaterial' for a in assigned):
        unreal.log_error(f'[import_meshes] {static_mesh.get_name()}: material sin asignar')


def _enable_nanite(static_mesh):
    settings = static_mesh.get_editor_property('nanite_settings')
    settings.set_editor_property('enabled', True)
    static_mesh.set_editor_property('nanite_settings', settings)


def _add_simple_collision(static_mesh):
    """NDOP18: envolvente de 18 caras, aproximación barata razonable para
    piedras romas. Si la función no existe con este nombre en 5.6, se deja
    constancia en el log en vez de reventar el resto de la importación."""
    try:
        unreal.EditorStaticMeshLibrary.add_simple_collisions(
            static_mesh, unreal.ScriptingCollisionShapeType.NDOP18)
    except Exception as exc:  # noqa: BLE001 - se registra y se continúa
        unreal.log_warning(f'[import_meshes] no se pudo generar colisión simple: {exc}')


def main():
    with open(MANIFEST_PATH, 'r', encoding='utf-8') as f:
        manifest = json.load(f)

    ok, failed = 0, 0
    for entry in manifest['meshes']:
        folder = FAMILY_FOLDERS[entry['category']]
        dest_path = f'{MESH_DEST_ROOT}/{folder}'
        fbx_path = os.path.join(EXPORT_DIR, entry['file'])

        static_mesh = _import_mesh(fbx_path, dest_path, entry['name'])
        if static_mesh is None:
            failed += 1
            continue

        _assign_materials(static_mesh, entry['material_slots'])

        if entry['category'] in NANITE_CATEGORIES:
            _enable_nanite(static_mesh)

        if entry['category'] in ('rock', 'debris'):
            _add_simple_collision(static_mesh)

        unreal.EditorAssetLibrary.save_loaded_asset(static_mesh)
        unreal.log(f"[import_meshes] {entry['name']} -> {dest_path} OK")
        ok += 1

    unreal.log(f'[import_meshes] importación completa: {ok} OK, {failed} fallidas.')


if __name__ == '__main__':
    main()
