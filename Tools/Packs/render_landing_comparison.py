"""Renderiza una escena de comparacion (playa + palmeras + selva) con el set
low poly de Landing, para juzgar a ojo si la paleta y la escala leen bien
juntas antes de darlas por buenas.

Blender headless:
    "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" ^
        --background --factory-startup --python Tools/Packs/render_landing_comparison.py

Reutiliza el pipeline de process_landing_set.py (import + paleta + normalizado)
en la MISMA sesion de Blender en vez de reimportar los FBX ya exportados: el
FBX se exporta con path_mode='STRIP' (igual que el resto del pipeline del
proyecto, ver Tools/Blender/lib/common.py::export_fbx) y perderia el enlace a
la textura de paleta al reimportarlo, dando materiales grises. Construir la
escena a partir de los mismos objetos en memoria evita ese problema.

Salida: Saved/ArtPreview/lowpoly_<angulo>.png (3 angulos; Saved/ esta en
.gitignore, no se versiona).
"""
import math
import random
import sys
from pathlib import Path

import bpy
from mathutils import Vector

TOOLS_PACKS = Path(__file__).resolve().parent
REPO_ROOT = TOOLS_PACKS.parents[1]
OUT_DIR = REPO_ROOT / "Saved" / "ArtPreview"

sys.path.insert(0, str(TOOLS_PACKS))
import process_landing_set as pls  # noqa: E402

RNG = random.Random(20260927)


def build_template(entry_name: str):
    entry = next(e for e in pls.ENTRIES if e["name"] == entry_name)
    src_path = pls.CACHE_DIR / entry["pack"] / entry["src"]
    obj = pls.import_fbx(src_path)
    pls.remap_to_palette(obj, entry.get("force_role"))
    pls.normalize_scale_and_pivot(obj, entry["metric"], entry["target"])
    obj.hide_render = True
    obj.hide_set(True)
    return obj


def place(template, location, rotation_z=0.0, scale_jitter=0.0):
    inst = template.copy()
    inst.data = template.data  # comparte malla: instancia, no copia geometria
    bpy.context.collection.objects.link(inst)
    inst.hide_render = False
    inst.hide_set(False)
    jitter = 1.0 + RNG.uniform(-scale_jitter, scale_jitter)
    inst.scale = (jitter, jitter, jitter)
    inst.rotation_euler = (0.0, 0.0, rotation_z)
    inst.location = Vector(location)
    return inst


def make_flat_material(name, color, roughness=0.9):
    if name in bpy.data.materials:
        return bpy.data.materials[name]
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = next(n for n in mat.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Roughness"].default_value = roughness
    return mat


def build_ground():
    bpy.ops.mesh.primitive_plane_add(size=140.0, location=(0, 20, 0))
    ground = bpy.context.active_object
    ground.name = "SandGround"
    ground.data.materials.append(make_flat_material("M_Sand", (0.86, 0.75, 0.55), roughness=0.95))
    return ground


def build_sky():
    world = bpy.data.worlds.new("W_Tropical")
    bpy.context.scene.world = world
    world.use_nodes = True
    nodes = world.node_tree.nodes
    links = world.node_tree.links
    nodes.clear()
    out = nodes.new("ShaderNodeOutputWorld")
    bg = nodes.new("ShaderNodeBackground")
    grad = nodes.new("ShaderNodeTexGradient")
    grad.gradient_type = 'LINEAR'
    mapping = nodes.new("ShaderNodeMapping")
    mapping.inputs["Rotation"].default_value = (math.radians(90), 0, 0)
    coord = nodes.new("ShaderNodeTexCoord")
    ramp = nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position = 0.0
    ramp.color_ramp.elements[0].color = (0.86, 0.93, 0.85, 1.0)  # horizonte palido calido
    ramp.color_ramp.elements[1].position = 1.0
    ramp.color_ramp.elements[1].color = (0.30, 0.62, 0.86, 1.0)  # cenit azul tropical
    links.new(coord.outputs["Generated"], mapping.inputs["Vector"])
    links.new(mapping.outputs["Vector"], grad.inputs["Vector"])
    links.new(grad.outputs["Fac"], ramp.inputs["Fac"])
    links.new(ramp.outputs["Color"], bg.inputs["Color"])
    bg.inputs["Strength"].default_value = 1.15
    links.new(bg.outputs["Background"], out.inputs["Surface"])


def build_sun():
    light_data = bpy.data.lights.new("SunWarm", type='SUN')
    light_data.energy = 4.2
    light_data.angle = math.radians(2.5)  # sol algo blando, no linterna dura
    light_data.color = (1.0, 0.87, 0.68)
    sun = bpy.data.objects.new("SunWarm", light_data)
    bpy.context.collection.objects.link(sun)
    sun.rotation_euler = (math.radians(58), 0, math.radians(-40))  # tarde calida, oblicuo
    return sun


def point_camera(cam_obj, target):
    direction = Vector(target) - cam_obj.location
    cam_obj.rotation_euler = direction.to_track_quat('-Z', 'Y').to_euler()


def make_camera(name, location, target, lens=32.0):
    cam_data = bpy.data.cameras.new(name)
    cam_data.lens = lens
    cam = bpy.data.objects.new(name, cam_data)
    bpy.context.collection.objects.link(cam)
    cam.location = Vector(location)
    point_camera(cam, target)
    return cam


def set_eevee():
    scene = bpy.context.scene
    engine_items = [i.identifier for i in scene.render.bl_rna.properties["engine"].enum_items]
    eevee_id = next((i for i in engine_items if "EEVEE" in i), None)
    if not eevee_id:
        raise RuntimeError(f"Sin motor Eevee disponible: {engine_items}")
    scene.render.engine = eevee_id
    scene.render.resolution_x = 1280
    scene.render.resolution_y = 720
    scene.render.image_settings.file_format = 'PNG'
    # Blender 4.2+ (Eevee Next) usa render.samples bajo eevee; en versiones
    # con el Eevee clasico vive en scene.eevee.taa_render_samples. Se prueban
    # ambos sin asumir cual existe en esta build.
    if hasattr(scene, "eevee") and hasattr(scene.eevee, "taa_render_samples"):
        scene.eevee.taa_render_samples = 64
    if hasattr(scene.render, "samples"):
        try:
            scene.render.samples = 64
        except Exception:
            pass


def build_scene():
    pls.reset_scene()
    build_ground()
    build_sky()
    build_sun()

    templates = {}

    def tpl(name):
        if name not in templates:
            templates[name] = build_template(name)
        return templates[name]

    # --- Palmeras en la linea de costa: 15-25 m de alto, separadas de verdad
    # (una palmera real necesita 8-12 m para desplegar la copa; a 4-6 m se
    # solapan y tapan la camara entera). Arco irregular, lejos del eje de la
    # camara de nivel de jugador (ver make_camera mas abajo).
    palm_names = ["SM_LowPolyPalmShort_01", "SM_LowPolyPalmTall_01",
                  "SM_LowPolyPalmDetailedTall_01", "SM_LowPolyPalmBend_01"]
    palm_positions = [(-24, 6), (-15, 9), (-4, 5), (9, 8), (19, 6), (26, 10)]
    for i, (x, y) in enumerate(palm_positions):
        name = palm_names[i % len(palm_names)]
        place(tpl(name), (x, y + RNG.uniform(-1.5, 1.5), 0), rotation_z=RNG.uniform(0, math.tau), scale_jitter=0.1)

    # --- Selva detras: pared de copas anchas, bien atras (son arboles de
    # 14-18 m con 8-10 m de copa: de cerca son un muro, hace falta distancia
    # real para leerlos como "selva al fondo" en vez de "estoy dentro de un
    # tronco"). Dos filas escalonadas para dar profundidad sin amontonar.
    wide_names = ["SM_LowPolyJungleWidePlateau_01", "SM_LowPolyJungleWideFat_01",
                  "SM_LowPolyJungleWideDetailed_01"]
    for i, x in enumerate(range(-32, 34, 7)):
        name = wide_names[i % len(wide_names)]
        y = 42 + RNG.uniform(-4, 5) + (6 if i % 2 == 0 else 0)
        place(tpl(name), (x, y, 0), rotation_z=RNG.uniform(0, math.tau), scale_jitter=0.15)

    # --- Arbustos y helechos: banda de transicion entre palmeras y selva -----
    shrub_names = ["SM_LowPolyShrubLarge_01", "SM_LowPolyShrubDetailed_01", "SM_LowPolyShrub_01"]
    fern_names = ["SM_LowPolyFernTall_01", "SM_LowPolyFernSmall_01"]
    for i in range(14):
        x = RNG.uniform(-30, 30)
        y = 20 + RNG.uniform(-4, 10)
        name = shrub_names[i % len(shrub_names)] if i % 2 == 0 else fern_names[i % len(fern_names)]
        place(tpl(name), (x, y, 0), rotation_z=RNG.uniform(0, math.tau), scale_jitter=0.2)

    # --- Rocas de orilla, cerca del agua (Y baja) -----------------------------
    rock_names = ["SM_LowPolyRockShoreA_01", "SM_LowPolyRockShoreB_01", "SM_LowPolyRockShoreC_01",
                  "SM_LowPolyRockSandA_01", "SM_LowPolyRockSandB_01"]
    for i in range(10):
        x = RNG.uniform(-28, 28)
        y = RNG.uniform(-3, 6)
        name = rock_names[i % len(rock_names)]
        place(tpl(name), (x, y, 0), rotation_z=RNG.uniform(0, math.tau), scale_jitter=0.25)

    # --- Hierba y flores en primer plano -------------------------------------
    grass_names = ["SM_LowPolyGrassClumpLarge_01", "SM_LowPolyGrassClumpLeafs_01"]
    for i in range(24):
        x = RNG.uniform(-28, 28)
        y = RNG.uniform(-5, 14)
        place(tpl(grass_names[i % len(grass_names)]), (x, y, 0), rotation_z=RNG.uniform(0, math.tau), scale_jitter=0.3)

    flower_names = ["SM_LowPolyFlowerRed_01", "SM_LowPolyFlowerYellow_01", "SM_LowPolyFlowerPurple_01"]
    for i in range(12):
        x = RNG.uniform(-26, 26)
        y = RNG.uniform(-3, 12)
        place(tpl(flower_names[i % len(flower_names)]), (x, y, 0), rotation_z=RNG.uniform(0, math.tau))

    # --- Troncos y ramas caidas (recolectables), sueltas por la arena --------
    debris_names = ["SM_LowPolyLog_01", "SM_LowPolyLogLarge_01", "SM_LowPolyStump_01", "SM_LowPolyBranch_01"]
    debris_positions = [(-18, 3), (-2, -1), (12, 4), (22, 2)]
    for name, (x, y) in zip(debris_names, debris_positions):
        place(tpl(name), (x, y, 0), rotation_z=RNG.uniform(0, math.tau))


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    build_scene()
    set_eevee()
    scene = bpy.context.scene

    # Distancias pensadas para el tamano real de las mallas (palmeras de
    # 15-25 m, selva de 14-18 m): de cerca cualquiera de las dos llena el
    # encuadre entero. La camara a altura de jugador se aparta del eje central
    # para no quedar pegada al tronco de una palmera.
    cameras = [
        ("eyelevel", make_camera("Cam_EyeLevel", (2, -32, 1.7), (5, 50, 7), lens=28.0)),
        ("aerial", make_camera("Cam_Aerial", (0, -60, 24), (0, 25, 6), lens=24.0)),
        ("threequarter", make_camera("Cam_ThreeQuarter", (-40, -22, 7), (8, 32, 6), lens=28.0)),
    ]

    for suffix, cam in cameras:
        scene.camera = cam
        out_path = OUT_DIR / f"lowpoly_{suffix}.png"
        scene.render.filepath = str(out_path)
        bpy.ops.render.render(write_still=True)
        print(f"[ok] render -> {out_path}")


if __name__ == "__main__":
    main()
