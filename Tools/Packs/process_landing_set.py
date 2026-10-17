"""Procesa el set de vegetacion de Landing: normaliza escala/pivote y exporta
a Art/Export/Meshes/LowPoly/<Slot>/.

Blender headless (no abre ventana):
    "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" ^
        --background --factory-startup --python Tools/Packs/process_landing_set.py

Fuentes (v3, 2026-09-28: el director marco el follaje del v2 -Quaternius via
Poly Pizza generico- como de aspecto europeo/templado -robles redondos, setos
de jardin, margaritas de prado- y pidio especies tropicales reconocibles):
  - Poly Pizza, CC0 o CC-BY 3.0 (descargados por download_polypizza.py a
    Tools/Packs/.cache/quaternius-polypizza/): palmeras (Quaternius, CC0),
    dosel de selva y emergentes (Poly by Google / Zacharylll, CC-BY 3.0: sin
    equivalente CC0 con especie reconocible), sotobosque (bananero, monstera,
    heliconia, bromelia, bambu: mezcla CC0/CC-BY), manglar, helecho y hierba
    (CC0). Se importan como .glb y se conserva su material nativo (atlas con
    degradado propio ya horneado) sin tocarlo. Los modelos CC-BY exigen
    atribucion: ver Tools/Packs/creditos_cc_by.md (pendiente de integrar en
    Source/Explored/UI/Widgets/SExploredCredits.cpp con el editor abierto).
  - Kenney Nature Kit + Survival Kit (CC0, download_packs.py), solo para lo
    que el director pidio mantener: rocas de orilla, troncos/ramas caidas y
    setas. Estas SI se remapean a Tools/Packs/palette_lowpoly.png (paleta
    plana): son props pequenos y solidos, no follaje, donde un color plano
    no lee como generico.

Especies retiradas en el v3 y el porque: ver DISCARDED en
Tools/Packs/download_polypizza.py.

Ver Tools/Packs/packs.json para licencia, autor y URL verificable de cada
modelo, y Tools/Packs/mapping.md para como encajan estos slots con
FScatterRule.ManifestCategory (Source/Explored/WorldGen/VegetationScatter.cpp).
Este set queda en LowPoly/ como comparativa: no toca el manifest.json de
produccion (generado por Tools/Blender/run_all.py).
"""
import json
import sys
from pathlib import Path
from typing import NotRequired, TypedDict

import bpy  # type: ignore[import-not-found]
from mathutils import Vector  # type: ignore[import-not-found]


class LandingEntry(TypedDict):
    """Una fila de ``ENTRIES``: origen y normalizacion de una malla del set."""

    slot: str
    name: str
    kind: str  # "glb" (Quaternius/Poly Pizza) u "obj" (Kenney)
    src: str
    metric: str  # "height" o "length"
    target: float
    pack: NotRequired[str]  # solo kind="obj": id en packs.json / carpeta de cache
    force_role: NotRequired[str]  # solo kind="obj": swatch fija en vez de remapeo por material

TOOLS_PACKS = Path(__file__).resolve().parent
REPO_ROOT = TOOLS_PACKS.parents[1]
CACHE_DIR = TOOLS_PACKS / ".cache"
CACHE_DIR_POLYPIZZA = CACHE_DIR / "quaternius-polypizza"
PALETTE_PNG = TOOLS_PACKS / "palette_lowpoly.png"
OUT_DIR = REPO_ROOT / "Art" / "Export" / "Meshes" / "LowPoly"

sys.path.insert(0, str(TOOLS_PACKS))
from make_palette import SWATCHES, cell_uv_center  # noqa: E402

# material de origen (Kenney OBJ) -> nombre de swatch en la paleta plana.
# Solo se usa para las entradas kind="obj" (rocas, troncos, setas): el
# follaje (kind="glb", Quaternius) conserva su propio material.
ROLE_BY_MATERIAL = {
    "leafsGreen": "LeafMid",
    "grass": "GrassGreen",
    "woodBark": "BarkWarm",
    "woodInner": "BarkPale",
    "dirt": "SandTan",
    "colorRed": "FlowerRed",
    "colorYellow": "FlowerYellow",
    "colorPurple": "FlowerPurple",
    "colorTan": "BarkPale",
    "_defaultMat": "FlowerWhite",  # tallo/base pálida (p.ej. de las setas), no follaje
}

# name -> (row, col) desde SWATCHES (name, rgb) -> construimos el indice inverso.
CELL_BY_ROLE = {name: (row, col) for (row, col), (name, _rgb) in SWATCHES.items()}


def role_uv(role: str) -> tuple:
    row, col = CELL_BY_ROLE[role]
    return cell_uv_center(row, col)


# ---------------------------------------------------------------------------
# Manifiesto de origen -> slot. kind="glb" (Quaternius/Poly Pizza, material
# nativo) o kind="obj" (Kenney, remapeado a la paleta plana). metric='height'
# normaliza por Z; 'length' por la mayor dimension horizontal (troncos/ramas
# tumbados). force_role fija una unica swatch (solo obj).
# ---------------------------------------------------------------------------
ENTRIES: list[LandingEntry] = [
    # --- Palmeras cocoteras: 15-25 m, 4 variantes (Quaternius/Poly Pizza) ---
    LandingEntry(slot="Palm", name="SM_LowPolyPalmA_01", kind="glb", src="palm_a.glb", metric="height", target=18.0),
    LandingEntry(slot="Palm", name="SM_LowPolyPalmB_01", kind="glb", src="palm_b.glb", metric="height", target=22.0),
    LandingEntry(slot="Palm", name="SM_LowPolyPalmC_01", kind="glb", src="palm_c.glb", metric="height", target=25.0),
    LandingEntry(slot="Palm", name="SM_LowPolyPalmD_01", kind="glb", src="palm_d.glb", metric="height", target=15.0),
    # --- Dosel de selva, 14-18 m (CC-BY 3.0, ver Tools/Packs/creditos_cc_by.md) --
    # 2026-09-28: sustituyen las 4 "Tree" de Quaternius (copas en bola sobre
    # tronco en Y, aspecto de roble de jardin) que el director marco como
    # europeas/templadas. Quaternius no tiene dosel tropical reconocible en
    # Poly Pizza.
    LandingEntry(slot="JungleWide", name="SM_LowPolyJungleWideA_01", kind="glb", src="jungle_wide_a.glb", metric="height", target=18.0),
    LandingEntry(slot="JungleWide", name="SM_LowPolyJungleWideB_01", kind="glb", src="jungle_wide_b.glb", metric="height", target=16.0),
    LandingEntry(slot="JungleWide", name="SM_LowPolyJungleWideC_01", kind="glb", src="jungle_wide_c.glb", metric="height", target=15.0),
    LandingEntry(slot="JungleWide", name="SM_LowPolyJungleWideD_01", kind="glb", src="jungle_wide_d.glb", metric="height", target=14.0),
    # --- Emergentes del dosel, 22-28 m (CC-BY 3.0): especies reales de selva,
    # mas altas que el dosel medio. Sustituyen SM_JungleTreeGiant_01 (procedural).
    LandingEntry(slot="JungleGiant", name="SM_LowPolyJungleGiantA_01", kind="glb", src="jungle_giant_a.glb", metric="height", target=28.0),
    LandingEntry(slot="JungleGiant", name="SM_LowPolyJungleGiantB_01", kind="glb", src="jungle_giant_b.glb", metric="height", target=24.0),
    # --- Sotobosque alto, 6-8 m: sustituyen SM_JungleTreeUnderstory_01. -----
    # UnderstoryA reusa el mismo .glb que JungleWideA (Vine Covered Tree) a
    # menor escala: es literalmente "JungleWide escalado" que pide el brief.
    LandingEntry(slot="Understory", name="SM_LowPolyUnderstoryA_01", kind="glb", src="jungle_wide_a.glb", metric="height", target=8.0),
    LandingEntry(slot="Understory", name="SM_LowPolyUnderstoryB_01", kind="glb", src="understory_bamboo.glb", metric="height", target=6.0),
    # --- Manglar, 2-4 m: sustituyen SM_JungleTreeMangrove_01 (procedural, -----
    # tronco cuadrado). MangroveA es el arbol (tronco en Y, CC0); MangroveB es
    # una raiz arqueada aislada (CC-BY) para plantar junto al tronco o sola
    # como detalle de raices en la orilla -sin ella ninguna malla de manglar
    # mostraba raices, que es justo lo que pidio el director-.
    LandingEntry(slot="Mangrove", name="SM_LowPolyMangroveA_01", kind="glb", src="mangrove_a.glb", metric="height", target=4.0),
    LandingEntry(slot="Mangrove", name="SM_LowPolyMangroveRoots_01", kind="glb", src="mangrove_roots.glb", metric="length", target=2.0),
    # --- Sotobosque: platanero, plantas de hoja grande y bambu ---------------
    # 2026-09-28: sustituyen shrub_a/shrub_b (bolas de seto), shrub_flowering
    # (flor lila de jardin) y shrub_banana (que era la fruta suelta, no la
    # planta). Todo CC0 salvo donde se anota CC-BY.
    LandingEntry(slot="Shrub", name="SM_LowPolyShrubBanana_01", kind="glb", src="shrub_banana.glb", metric="height", target=3.0),  # CC-BY
    LandingEntry(slot="Shrub", name="SM_LowPolyShrubMonstera_01", kind="glb", src="shrub_monstera.glb", metric="height", target=1.2),
    LandingEntry(slot="Shrub", name="SM_LowPolyShrubHeliconia_01", kind="glb", src="shrub_heliconia.glb", metric="height", target=1.5),  # CC-BY
    LandingEntry(slot="Shrub", name="SM_LowPolyShrubBromeliad_01", kind="glb", src="shrub_bromeliad.glb", metric="height", target=0.6),  # CC-BY
    LandingEntry(slot="Shrub", name="SM_LowPolyShrubPineappleTop_01", kind="glb", src="shrub_pineapple_top.glb", metric="height", target=0.7),
    LandingEntry(slot="Shrub", name="SM_LowPolyShrubBamboo_01", kind="glb", src="shrub_bamboo.glb", metric="height", target=3.5),
    # --- Helechos de suelo (Quaternius/Poly Pizza) ---------------------------
    # fern_b (racimo de petalos morados mal etiquetado "Fern") se retira: no
    # hay un segundo helecho CC0/CC-BY adecuado, ver DISCARDED en
    # download_polypizza.py. Limitacion conocida, queda un unico helecho.
    LandingEntry(slot="Fern", name="SM_LowPolyFernA_01", kind="glb", src="fern_a.glb", metric="height", target=0.7),
    # --- Hierba en matas altas, no cesped de prado (Quaternius/Poly Pizza) ---
    LandingEntry(slot="Grass", name="SM_LowPolyGrassA_01", kind="glb", src="grass_a.glb", metric="height", target=0.55),
    LandingEntry(slot="Grass", name="SM_LowPolyGrassB_01", kind="glb", src="grass_b.glb", metric="height", target=0.4),
    LandingEntry(slot="Grass", name="SM_LowPolyGrassC_01", kind="glb", src="grass_c.glb", metric="height", target=0.5),
    # --- Flor (CC-BY 3.0) -----------------------------------------------------
    # Flower_A/B/C (margaritas de prado) se retiran sin reemplazo 1:1: unico
    # sustituto tropical sin maceta encontrado en Poly Pizza. Ver DISCARDED.
    LandingEntry(slot="Flower", name="SM_LowPolyFlowerHibiscus_01", kind="glb", src="flower_hibiscus.glb", metric="height", target=0.5),  # CC-BY
    # --- Rocas de orilla (Kenney, mantenidas por peticion del director) ----
    LandingEntry(slot="Rock", name="SM_LowPolyRockShoreA_01", kind="obj", pack="kenney-nature-kit",
         src="Models/OBJ format/rock_smallA.obj", metric="height", target=0.4),
    LandingEntry(slot="Rock", name="SM_LowPolyRockShoreB_01", kind="obj", pack="kenney-nature-kit",
         src="Models/OBJ format/rock_smallB.obj", metric="height", target=0.5),
    LandingEntry(slot="Rock", name="SM_LowPolyRockShoreC_01", kind="obj", pack="kenney-nature-kit",
         src="Models/OBJ format/rock_smallC.obj", metric="height", target=0.35),
    LandingEntry(slot="Rock", name="SM_LowPolyRockSandA_01", kind="obj", pack="kenney-survival-kit",
         src="Models/OBJ format/rock-sand-a.obj", metric="height", target=0.6, force_role="RockGrey"),
    LandingEntry(slot="Rock", name="SM_LowPolyRockSandB_01", kind="obj", pack="kenney-survival-kit",
         src="Models/OBJ format/rock-sand-b.obj", metric="height", target=0.5, force_role="RockGrey"),
    # --- Troncos, ramas caidas y setas (Kenney, mantenidas) -----------------
    LandingEntry(slot="Debris", name="SM_LowPolyLog_01", kind="obj", pack="kenney-nature-kit",
         src="Models/OBJ format/log.obj", metric="length", target=2.5),
    LandingEntry(slot="Debris", name="SM_LowPolyLogLarge_01", kind="obj", pack="kenney-nature-kit",
         src="Models/OBJ format/log_large.obj", metric="length", target=4.0),
    LandingEntry(slot="Debris", name="SM_LowPolyStump_01", kind="obj", pack="kenney-nature-kit",
         src="Models/OBJ format/stump_round.obj", metric="height", target=0.8),
    LandingEntry(slot="Debris", name="SM_LowPolyBranch_01", kind="obj", pack="kenney-survival-kit",
         src="Models/OBJ format/tree-log-small.obj", metric="length", target=1.2, force_role="BarkWarm"),
    LandingEntry(slot="Debris", name="SM_LowPolyMushroomRed_01", kind="obj", pack="kenney-nature-kit",
         src="Models/OBJ format/mushroom_red.obj", metric="height", target=0.35),
    LandingEntry(slot="Debris", name="SM_LowPolyMushroomTan_01", kind="obj", pack="kenney-nature-kit",
         src="Models/OBJ format/mushroom_tan.obj", metric="height", target=0.3),
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


def _finish_import(after, before):
    """Comun a import_obj/import_glb: une las mallas nuevas si son varias,
    quita lo que no sea malla (vacios, camaras) y hornea rotacion/escala/
    parentesco para que local == world de aqui en adelante.

    Algunos .glb de Quaternius (p.ej. las flores, que llegan como 5-7
    "clumps" separados) traen varias mallas colgadas del mismo RootNode.
    bpy.ops.object.join() libera las que no son la activa: sus referencias
    de Python en «after» quedan invalidas (ReferenceError al tocarlas), asi
    que la limpieza de vacios se hace por NOMBRE contra bpy.data.objects
    tras el join, nunca reutilizando esas referencias."""
    after_names = [o.name for o in after]
    meshes = [o for o in after if o.type == 'MESH']
    if not meshes:
        raise RuntimeError("sin objetos de malla tras importar")

    bpy.ops.object.select_all(action='DESELECT')
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    bpy.ops.object.parent_clear(type='CLEAR_KEEP_TRANSFORM')
    if len(meshes) > 1:
        bpy.ops.object.join()
    active_name = bpy.context.view_layer.objects.active.name

    for name in after_names:
        if name == active_name:
            continue
        stale = bpy.data.objects.get(name)
        if stale is not None:
            bpy.data.objects.remove(stale, do_unlink=True)

    obj = bpy.data.objects[active_name]
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.context.view_layer.update()
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return obj


def import_obj(path: Path):
    """Importa un .obj de Kenney (sus .fbx son ASCII: Blender 5.x dejo de
    soportarlos). El importador deja la conversion Y-up -> Z-up como una
    ROTACION a nivel de objeto (no la hornea en los vertices), y
    Object.dimensions en Blender es local (bound_box local * scale, ignora
    esa rotacion): sin hornearla, dims.z de un arbol da su radio de copa en
    vez de su altura real. _finish_import se encarga de hornearla."""
    before = set(bpy.data.objects.keys())
    bpy.ops.wm.obj_import(filepath=str(path), forward_axis='NEGATIVE_Z', up_axis='Y')
    after = [o for o in bpy.data.objects if o.name not in before]
    return _finish_import(after, before)


def import_glb(path: Path):
    """Importa un .glb de Quaternius (via Poly Pizza). Llegan colgados de un
    Empty 'RootNode' con object.scale=100 (malla modelada en cm, reescalada
    a metros por el objeto, no por los vertices): _finish_import limpia el
    parentesco conservando la transform mundial y hornea esa escala."""
    before = set(bpy.data.objects.keys())
    bpy.ops.import_scene.gltf(filepath=str(path))
    after = [o for o in bpy.data.objects if o.name not in before]
    return _finish_import(after, before)


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
    """Exporta a FBX con las texturas incrustadas.

    Causa del follaje gris-marron en el mapa horneado (diagnosticado 2026-09-28):
    esta funcion exportaba con path_mode='STRIP' (tira la ruta de la textura,
    no deja ni un path relativo) y embed_textures=False (tampoco la incrusta).
    El follaje "kind=glb" conserva su material nativo -un atlas con degradado
    ya horneado, ver remap_to_palette()/get_palette_material() mas arriba para
    contraste con "kind=obj"-, asi que el FBX resultante llegaba a Unreal SIN
    ninguna textura: ni incrustada ni referenciada por ruta. El importador de
    Unreal crea igualmente el material (no falla), pero con el Base Color sin
    conectar -de ahi el gris-marron plano en vez del degradado de la paleta
    por isla-. path_mode='COPY' es requisito de Blender para que
    embed_textures surta efecto (si no, la opcion se ignora en silencio)."""
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
        path_mode='COPY',
        embed_textures=True,
    )


def main():
    if not PALETTE_PNG.exists():
        raise SystemExit(f"Falta la paleta: {PALETTE_PNG}. Ejecuta make_palette.py primero.")

    results = []
    for entry in ENTRIES:
        reset_scene()
        if entry["kind"] == "glb":
            src_path = CACHE_DIR_POLYPIZZA / entry["src"]
        else:
            pack = entry.get("pack")
            if not pack:
                raise ValueError(f"{entry['name']}: kind=\"obj\" requiere \"pack\"")
            src_path = CACHE_DIR / pack / entry["src"]
        if not src_path.exists():
            print(f"[skip] {entry['name']}: no existe {src_path} (¿bajaste el pack?)")
            continue

        obj = import_glb(src_path) if entry["kind"] == "glb" else import_obj(src_path)
        if entry["kind"] == "obj":
            remap_to_palette(obj, entry.get("force_role"))
        normalize_scale_and_pivot(obj, entry["metric"], entry["target"])
        out_path = OUT_DIR / entry["slot"] / f"{entry['name']}.fbx"
        export_fbx(obj, out_path)

        tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
        dims = obj.dimensions
        # material_slots=["NATIVE"] para TODO este set: kind="glb" conserva
        # el material propio del .glb (sin vertex color) y kind="obj" lleva
        # el material M_LowPoly_Palette ya creado y asignado en Blender por
        # remap_to_palette/get_palette_material. Ambos casos exportan ahora
        # con la textura incrustada en el FBX (export_fbx: embed_textures,
        # ver su docstring), asi que en los dos basta con que
        # Tools/Unreal/import_meshes.py conserve el material que trae el FBX
        # en vez de forzarle los 4 materiales estables de MATERIAL_DEFS
        # (pensados para el vertex color del kit procedural, no para este set).
        material_slots = ["NATIVE"]
        results.append(dict(
            name=entry["name"], slot=entry["slot"], file=f"{entry['slot']}/{entry['name']}.fbx",
            source_kind=entry["kind"], source_file=entry["src"], triangles=tris,
            dimensions_m={"x": round(dims.x, 3), "y": round(dims.y, 3), "z": round(dims.z, 3)},
            material_slots=material_slots,
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
