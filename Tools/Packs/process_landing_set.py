"""Procesa el set de vegetacion de Landing: normaliza escala/pivote, aplica la
paleta compartida y exporta a Art/Export/Meshes/LowPoly/<Slot>/.

Blender headless (no abre ventana):
    "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" ^
        --background --factory-startup --python Tools/Packs/process_landing_set.py

Fuente: Kenney Nature Kit + Kenney Survival Kit (CC0), descargados por
download_packs.py a Tools/Packs/.cache/. Paleta: Tools/Packs/palette_lowpoly.png
(generada por make_palette.py). Ver Tools/Packs/packs.json para la licencia y
que malla de cada pack se usa para que slot del scatter.

Convencion de mallas: <Categoria>/<Nombre> donde Categoria coincide con
FScatterRule.ManifestCategory (Source/Explored/WorldGen/VegetationScatter.cpp)
y Nombre lleva el filtro esperado por NameFilter cuando aplica (ver mapping.md
en esta misma carpeta). Este set queda en LowPoly/ como comparativa: no toca
el manifest.json en produccion (Art/Export/Meshes/manifest.json), que sigue
generado por Tools/Blender/run_all.py.
"""
import json
import math
import os
import sys
from pathlib import Path

import bpy
from mathutils import Vector

TOOLS_PACKS = Path(__file__).resolve().parent
REPO_ROOT = TOOLS_PACKS.parents[1]
CACHE_DIR = TOOLS_PACKS / ".cache"
PALETTE_PNG = TOOLS_PACKS / "palette_lowpoly.png"
OUT_DIR = REPO_ROOT / "Art" / "Export" / "Meshes" / "LowPoly"

sys.path.insert(0, str(TOOLS_PACKS))
from make_palette import SWATCHES, cell_uv_center  # noqa: E402

# material de origen (Kenney OBJ/FBX) -> nombre de swatch en la paleta.
ROLE_BY_MATERIAL = {
    "leafsGreen": "LeafMid",
    "grass": "GrassGreen",
    "woodBark": "BarkWarm",
    "woodInner": "BarkPale",
    "dirt": "SandTan",
    "colorRed": "FlowerRed",
    "colorYellow": "FlowerYellow",
    "colorPurple": "FlowerPurple",
    "_defaultMat": "LeafMid",
}

# name -> (row, col) desde SWATCHES (name, rgb) -> construimos el indice inverso.
CELL_BY_ROLE = {name: (row, col) for (row, col), (name, _rgb) in SWATCHES.items()}


def role_uv(role: str) -> tuple:
    row, col = CELL_BY_ROLE[role]
    return cell_uv_center(row, col)


# ---------------------------------------------------------------------------
# Manifiesto de origen -> slot. metric='height' normaliza por Z; 'length' por
# la mayor dimension horizontal (troncos y ramas tumbados). force_role fija
# una unica swatch para mallas que llegan con textura UV propia (colormap.png
# del Survival Kit) en vez de materiales con Kd por nombre.
# ---------------------------------------------------------------------------
ENTRIES = [
    # --- Palmeras cocoteras: 15-25 m, 4 variantes, una curvada -------------
    dict(slot="Palm", name="SM_LowPolyPalmShort_01", pack="kenney-nature-kit",
         src="Models/OBJ format/tree_palmShort.obj", metric="height", target=15.0),
    dict(slot="Palm", name="SM_LowPolyPalmTall_01", pack="kenney-nature-kit",
         src="Models/OBJ format/tree_palmTall.obj", metric="height", target=22.0),
    dict(slot="Palm", name="SM_LowPolyPalmDetailedTall_01", pack="kenney-nature-kit",
         src="Models/OBJ format/tree_palmDetailedTall.obj", metric="height", target=25.0),
    dict(slot="Palm", name="SM_LowPolyPalmBend_01", pack="kenney-nature-kit",
         src="Models/OBJ format/tree_palmBend.obj", metric="height", target=18.0),
    # --- Arboles de selva de copa ancha -------------------------------------
    dict(slot="JungleWide", name="SM_LowPolyJungleWidePlateau_01", pack="kenney-nature-kit",
         src="Models/OBJ format/tree_plateau.obj", metric="height", target=18.0),
    dict(slot="JungleWide", name="SM_LowPolyJungleWideFat_01", pack="kenney-nature-kit",
         src="Models/OBJ format/tree_fat.obj", metric="height", target=14.0),
    dict(slot="JungleWide", name="SM_LowPolyJungleWideDetailed_01", pack="kenney-nature-kit",
         src="Models/OBJ format/tree_detailed.obj", metric="height", target=16.0),
    # --- Arbustos ------------------------------------------------------------
    dict(slot="Shrub", name="SM_LowPolyShrubLarge_01", pack="kenney-nature-kit",
         src="Models/OBJ format/plant_bushLarge.obj", metric="height", target=2.2),
    dict(slot="Shrub", name="SM_LowPolyShrubDetailed_01", pack="kenney-nature-kit",
         src="Models/OBJ format/plant_bushDetailed.obj", metric="height", target=1.6),
    dict(slot="Shrub", name="SM_LowPolyShrub_01", pack="kenney-nature-kit",
         src="Models/OBJ format/plant_bush.obj", metric="height", target=1.2),
    # --- Helechos (sustituto documentado: Kenney no trae "fern" explicito) --
    dict(slot="Fern", name="SM_LowPolyFernTall_01", pack="kenney-nature-kit",
         src="Models/OBJ format/plant_flatTall.obj", metric="height", target=0.9),
    dict(slot="Fern", name="SM_LowPolyFernSmall_01", pack="kenney-nature-kit",
         src="Models/OBJ format/plant_bushSmall.obj", metric="height", target=0.6),
    # --- Hierba en matas -------------------------------------------------------
    dict(slot="Grass", name="SM_LowPolyGrassClumpLarge_01", pack="kenney-nature-kit",
         src="Models/OBJ format/grass_large.obj", metric="height", target=0.6),
    dict(slot="Grass", name="SM_LowPolyGrassClumpLeafs_01", pack="kenney-nature-kit",
         src="Models/OBJ format/grass_leafsLarge.obj", metric="height", target=0.5),
    # --- Flores ------------------------------------------------------------
    dict(slot="Flower", name="SM_LowPolyFlowerRed_01", pack="kenney-nature-kit",
         src="Models/OBJ format/flower_redA.obj", metric="height", target=0.35),
    dict(slot="Flower", name="SM_LowPolyFlowerYellow_01", pack="kenney-nature-kit",
         src="Models/OBJ format/flower_yellowA.obj", metric="height", target=0.35),
    dict(slot="Flower", name="SM_LowPolyFlowerPurple_01", pack="kenney-nature-kit",
         src="Models/OBJ format/flower_purpleA.obj", metric="height", target=0.35),
    # --- Rocas de orilla -----------------------------------------------------
    dict(slot="Rock", name="SM_LowPolyRockShoreA_01", pack="kenney-nature-kit",
         src="Models/OBJ format/rock_smallA.obj", metric="height", target=0.4),
    dict(slot="Rock", name="SM_LowPolyRockShoreB_01", pack="kenney-nature-kit",
         src="Models/OBJ format/rock_smallB.obj", metric="height", target=0.5),
    dict(slot="Rock", name="SM_LowPolyRockShoreC_01", pack="kenney-nature-kit",
         src="Models/OBJ format/rock_smallC.obj", metric="height", target=0.35),
    dict(slot="Rock", name="SM_LowPolyRockSandA_01", pack="kenney-survival-kit",
         src="Models/OBJ format/rock-sand-a.obj", metric="height", target=0.6, force_role="RockGrey"),
    dict(slot="Rock", name="SM_LowPolyRockSandB_01", pack="kenney-survival-kit",
         src="Models/OBJ format/rock-sand-b.obj", metric="height", target=0.5, force_role="RockGrey"),
    # --- Troncos y ramas caidas (recolectables) -----------------------------
    dict(slot="Debris", name="SM_LowPolyLog_01", pack="kenney-nature-kit",
         src="Models/OBJ format/log.obj", metric="length", target=2.5),
    dict(slot="Debris", name="SM_LowPolyLogLarge_01", pack="kenney-nature-kit",
         src="Models/OBJ format/log_large.obj", metric="length", target=4.0),
    dict(slot="Debris", name="SM_LowPolyStump_01", pack="kenney-nature-kit",
         src="Models/OBJ format/stump_round.obj", metric="height", target=0.8),
    dict(slot="Debris", name="SM_LowPolyBranch_01", pack="kenney-survival-kit",
         src="Models/OBJ format/tree-log-small.obj", metric="length", target=1.2, force_role="BarkWarm"),
]


def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.scale_length = 1.0


def get_palette_material():
    name = "M_LowPoly_Palette"
    if name in bpy.data.materials:
        return bpy.data.materials[name]
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    nodes.clear()
    out = nodes.new("ShaderNodeOutputMaterial")
    bsdf = nodes.new("ShaderNodeBsdfPrincipled")
    tex = nodes.new("ShaderNodeTexImage")
    img = bpy.data.images.load(str(PALETTE_PNG))
    img.colorspace_settings.name = 'sRGB'
    tex.image = img
    tex.interpolation = 'Closest'  # celdas solidas: sin bleed entre swatches
    bsdf.inputs["Roughness"].default_value = 0.85
    links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    out.location = (400, 0)
    bsdf.location = (150, 0)
    tex.location = (-150, 0)
    return mat


def import_fbx(path: Path):
    """Importa el .obj de origen (los .fbx de Kenney son ASCII: Blender 5.x
    dejo de soportarlos como importador nativo). El nombre de la funcion se
    mantiene por lo que hace en el pipeline, no por el formato de entrada."""
    before = set(bpy.data.objects.keys())
    bpy.ops.wm.obj_import(filepath=str(path), forward_axis='NEGATIVE_Z', up_axis='Y')
    after = [o for o in bpy.data.objects if o.name not in before]
    meshes = [o for o in after if o.type == 'MESH']
    if not meshes:
        raise RuntimeError(f"sin objetos de malla tras importar {path}")
    if len(meshes) > 1:
        bpy.ops.object.select_all(action='DESELECT')
        for o in meshes:
            o.select_set(True)
        bpy.context.view_layer.objects.active = meshes[0]
        bpy.ops.object.join()
        meshes = [bpy.context.view_layer.objects.active]
    # limpia objetos no-malla que haya podido traer el FBX (vacios, camaras...)
    for o in after:
        if o.type != 'MESH' and o.name in bpy.data.objects:
            bpy.data.objects.remove(o, do_unlink=True)

    obj = meshes[0]
    # El importador de OBJ deja la conversion Y-up -> Z-up como una ROTACION a
    # nivel de objeto (no la hornea en los vertices). Object.dimensions en
    # Blender es local (bound_box local * scale, ignora esa rotacion), asi que
    # sin hornearla aqui, dims.z de un arbol da su radio de copa (eje local
    # sin rotar) en vez de su altura real. Se hornea ya mismo para que local
    # == world de aqui en adelante y normalize_scale_and_pivot lea el eje que
    # toca.
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.context.view_layer.update()
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return obj


def remap_to_palette(obj, force_role: str | None):
    me = obj.data
    uv_layer = me.uv_layers.active or me.uv_layers.new(name="UVMap")

    if force_role:
        u, v = role_uv(force_role)
        for loop in me.loops:
            uv_layer.data[loop.index].uv = (u, v)
    else:
        mat_role = []
        for slot in obj.material_slots:
            src_name = slot.material.name.split(".")[0] if slot.material else ""
            role = ROLE_BY_MATERIAL.get(src_name, "LeafMid")
            mat_role.append(role_uv(role))
        if not mat_role:
            mat_role = [role_uv("LeafMid")]
        for poly in me.polygons:
            uv = mat_role[poly.material_index] if poly.material_index < len(mat_role) else mat_role[0]
            for li in poly.loop_indices:
                uv_layer.data[li].uv = uv

    me.materials.clear()
    me.materials.append(get_palette_material())
    for poly in me.polygons:
        poly.material_index = 0


def normalize_scale_and_pivot(obj, metric: str, target: float):
    bpy.context.view_layer.update()
    dims = obj.dimensions
    if metric == "height":
        current = dims.z
    else:  # length: mayor dimension horizontal (troncos/ramas tumbados)
        current = max(dims.x, dims.y)
    if current <= 1e-6:
        raise RuntimeError(f"{obj.name}: dimension {metric} nula, no se puede escalar")
    factor = target / current
    obj.scale = (factor, factor, factor)
    bpy.context.view_layer.update()

    corners = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
    min_z = min(c.z for c in corners)
    center_x = sum(c.x for c in corners) / 8.0
    center_y = sum(c.y for c in corners) / 8.0
    obj.location -= Vector((center_x, center_y, min_z))
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)


def export_fbx(obj, out_path: Path):
    out_path.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(
        filepath=str(out_path),
        check_existing=False,
        use_selection=True,
        global_scale=1.0,
        apply_unit_scale=True,
        apply_scale_options='FBX_SCALE_ALL',
        axis_forward='-Z',
        axis_up='Y',
        object_types={'MESH'},
        use_mesh_modifiers=True,
        mesh_smooth_type='FACE',
        use_triangles=True,
        bake_anim=False,
        path_mode='STRIP',
        embed_textures=False,
    )


def main():
    if not PALETTE_PNG.exists():
        raise SystemExit(f"Falta la paleta: {PALETTE_PNG}. Ejecuta make_palette.py primero.")

    results = []
    for entry in ENTRIES:
        reset_scene()
        src_path = CACHE_DIR / entry["pack"] / entry["src"]
        if not src_path.exists():
            print(f"[skip] {entry['name']}: no existe {src_path} (¿bajaste el pack?)")
            continue
        obj = import_fbx(src_path)
        remap_to_palette(obj, entry.get("force_role"))
        normalize_scale_and_pivot(obj, entry["metric"], entry["target"])
        out_path = OUT_DIR / entry["slot"] / f"{entry['name']}.fbx"
        export_fbx(obj, out_path)

        tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
        dims = obj.dimensions
        results.append(dict(
            name=entry["name"], slot=entry["slot"], file=f"{entry['slot']}/{entry['name']}.fbx",
            source_pack=entry["pack"], source_file=entry["src"], triangles=tris,
            dimensions_m={"x": round(dims.x, 3), "y": round(dims.y, 3), "z": round(dims.z, 3)},
        ))
        print(f"[ok] {entry['name']}: {tris} tris, dims={dims.x:.2f}x{dims.y:.2f}x{dims.z:.2f} m -> {out_path}")

    manifest_path = OUT_DIR / "lowpoly_manifest.json"
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    with open(manifest_path, "w", encoding="utf-8") as f:
        json.dump({"generated_by": "Tools/Packs/process_landing_set.py", "mesh_count": len(results),
                    "meshes": results}, f, indent=2, ensure_ascii=False)
    print(f"[ok] {len(results)} mallas procesadas. Manifiesto: {manifest_path}")


if __name__ == "__main__":
    main()
