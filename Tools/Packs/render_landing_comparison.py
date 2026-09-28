"""Renderiza una escena de comparacion (playa + palmeras + selva + mar) con
el set low poly de Landing, para juzgar a ojo si de verdad se acerca a
Sea of Thieves/Muck o si sigue leyendo como generico ("cutre").

Blender headless:
    "C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe" ^
        --background --factory-startup --python Tools/Packs/render_landing_comparison.py

Reutiliza el pipeline de process_landing_set.py (import + normalizado; la
paleta plana solo se aplica a las piezas Kenney: rocas, troncos y setas) en
la MISMA sesion de Blender en vez de reimportar los FBX ya exportados: el
FBX se exporta con path_mode='STRIP' (igual que el resto del pipeline del
proyecto, ver Tools/Blender/lib/common.py::export_fbx) y perderia el enlace
a las texturas nativas del follaje Quaternius al reimportarlo.

Composicion (v2, tras el rechazo del primer pase): el mar queda al fondo en
+Y con un plano turquesa y una franja de espuma en la orilla; la arena tiene
un ligero ondulado; la selva queda a mas de 25 m detras de la camara (-Y); la
linea de palmeras se inclina hacia el mar y deja libre el pasillo central
para que la camara de nivel de jugador mire al mar con palmeras a los lados,
no de frente. Cielo con Sky Texture (dispersion atmosferica), sol a 35°,
sombras suaves, e iluminacion global con trazado de rayos (Eevee) para AO.

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

SUN_ELEVATION_DEG = 35.0
SUN_AZIMUTH_DEG = -35.0  # tarde, oblicuo; coherente entre el objeto Sol y el cielo


def build_template(entry_name: str):
    entry = next(e for e in pls.ENTRIES if e["name"] == entry_name)
    if entry["kind"] == "glb":
        src_path = pls.CACHE_DIR_POLYPIZZA / entry["src"]
        obj = pls.import_glb(src_path)
    else:
        pack = entry.get("pack")
        if not pack:
            raise ValueError(f"{entry['name']}: kind=\"obj\" requiere \"pack\"")
        src_path = pls.CACHE_DIR / pack / entry["src"]
        obj = pls.import_obj(src_path)
        pls.remap_to_palette(obj, entry.get("force_role"))
    pls.normalize_scale_and_pivot(obj, entry["metric"], entry["target"])
    obj.hide_render = True
    obj.hide_set(True)
    return obj


def place(template, location, rotation_z=0.0, rotation_x=0.0, scale_jitter=0.0):
    inst = template.copy()
    inst.data = template.data  # comparte malla: instancia, no copia geometria
    bpy.context.collection.objects.link(inst)
    inst.hide_render = False
    inst.hide_set(False)
    jitter = 1.0 + RNG.uniform(-scale_jitter, scale_jitter)
    inst.scale = (jitter, jitter, jitter)
    inst.rotation_euler = (rotation_x, 0.0, rotation_z)
    inst.location = Vector(location)
    return inst


def srgb_to_linear(c: float) -> float:
    """Blender espera los sockets Base Color en LINEAL. Pasar un hex/0-255
    directamente (sin decodificar la gamma sRGB) queda mucho mas CLARO de lo
    que parece en pantalla -era la causa real de que la arena y el agua
    quemaran a blanco por mucho que se bajara la luz de la escena-."""
    if c <= 0.04045:
        return c / 12.92
    return ((c + 0.055) / 1.055) ** 2.4


def make_flat_material(name, color, roughness=0.9, base_color_boost=1.0):
    if name in bpy.data.materials:
        return bpy.data.materials[name]
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = next(n for n in mat.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
    linear = [srgb_to_linear(min(1.0, c * base_color_boost)) for c in color]
    bsdf.inputs["Base Color"].default_value = (*linear, 1.0)
    bsdf.inputs["Roughness"].default_value = roughness
    return mat


def build_ground():
    """Arena calida (~#E8D5A8) con un ondulado suave: una rejilla densa con
    un modificador Displace + textura de ruido, no un plano perfectamente
    liso (una de las quejas del primer pase)."""
    bpy.ops.mesh.primitive_grid_add(x_subdivisions=120, y_subdivisions=120, size=160.0, location=(0, -5, 0))
    ground = bpy.context.active_object
    ground.name = "SandGround"
    sand_hex = (0xE8 / 255, 0xD5 / 255, 0xA8 / 255)
    # boost<1: el hex pedido (~#E8D5A8) es un tono ya claro de por si; con la
    # luz de la escena a plena intensidad saturaba a blanco (via Standard,
    # sin rolloff de highlights) y la arena dejaba de leerse como calida.
    ground.data.materials.append(make_flat_material("M_Sand", sand_hex, roughness=0.95, base_color_boost=1.2))

    noise_tex = bpy.data.textures.new("SandNoise", type='CLOUDS')
    noise_tex.noise_scale = 6.0
    disp = ground.modifiers.new("Undulation", type='DISPLACE')
    disp.texture = noise_tex
    disp.strength = 0.22
    disp.mid_level = 0.5
    return ground


def build_water():
    """Mar turquesa al fondo (+Y) con una franja de espuma en la orilla."""
    bpy.ops.mesh.primitive_plane_add(size=160.0, location=(0, 110, -0.05))
    water = bpy.context.active_object
    water.name = "Water"
    # roughness alta a proposito: con roughness baja (mas especular), Eevee
    # reflejaba tanto cielo brillante que el turquesa quedaba lavado a un
    # celeste palido casi blanco (misma familia de problema que la arena).
    mat = make_flat_material("M_Water", (0.05, 0.62, 0.6), roughness=0.4, base_color_boost=1.0)
    bsdf = next(n for n in mat.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
    bsdf.inputs["Specular IOR Level"].default_value = 0.35
    water.data.materials.clear()
    water.data.materials.append(mat)

    bpy.ops.mesh.primitive_plane_add(size=1.0, location=(0, 41, 0.02))
    foam = bpy.context.active_object
    foam.name = "Foam"
    foam.scale = (80.0, 3.5, 1.0)
    foam.data.materials.append(make_flat_material("M_Foam", (0.88, 0.93, 0.90), roughness=0.5, base_color_boost=1.1))
    return water, foam


def build_sky():
    world = bpy.data.worlds.new("W_Tropical")
    bpy.context.scene.world = world
    world.use_nodes = True
    nodes = world.node_tree.nodes
    links = world.node_tree.links
    nodes.clear()
    out = nodes.new("ShaderNodeOutputWorld")
    bg = nodes.new("ShaderNodeBackground")
    sky = nodes.new("ShaderNodeTexSky")
    sky.sky_type = 'MULTIPLE_SCATTERING'  # dispersion atmosferica fisica (sucesora de "Nishita" en esta build)
    sky.sun_disc = True
    sky.sun_size = math.radians(1.2)
    sky.sun_intensity = 1.1
    sky.sun_elevation = math.radians(SUN_ELEVATION_DEG)
    sky.sun_rotation = math.radians(SUN_AZIMUTH_DEG)
    sky.sun_intensity = 0.6
    sky.air_density = 1.1
    sky.aerosol_density = 1.3  # algo mas de calima calida tropical
    sky.ozone_density = 1.0
    links.new(sky.outputs["Color"], bg.inputs["Color"])
    bg.inputs["Strength"].default_value = 0.5
    links.new(bg.outputs["Background"], out.inputs["Surface"])
    # AgX comprime altas luces en vez de recortarlas a blanco puro (lo que
    # pasaba con 'Standard' incluso con la luz de escena ya bajada: la arena,
    # clara de por si, seguia quemando). Con las intensidades ya ajustadas
    # abajo, AgX es lo que deja arena calida y cielo azul en vez de blanco.
    bpy.context.scene.view_settings.view_transform = 'AgX'


def build_sun():
    light_data = bpy.data.lights.new("SunWarm", type='SUN')
    light_data.energy = 3.2
    light_data.angle = math.radians(4.5)  # sombras suaves, no linterna dura
    light_data.color = (1.0, 0.9, 0.75)
    sun = bpy.data.objects.new("SunWarm", light_data)
    bpy.context.collection.objects.link(sun)
    # elevacion 35°: 0° = horizonte, 90° = cenit -> pitch de la luz = -(90-35) desde la horizontal
    sun.rotation_euler = (math.radians(90.0 - SUN_ELEVATION_DEG), 0, math.radians(SUN_AZIMUTH_DEG))
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
    if hasattr(scene, "eevee"):
        # Trazado de rayos: en esta build de Eevee es lo que aporta AO/GI de
        # verdad en vez del "flat" clasico (una de las quejas del primer pase).
        if hasattr(scene.eevee, "use_raytracing"):
            scene.eevee.use_raytracing = True
        if hasattr(scene.eevee, "use_shadows"):
            scene.eevee.use_shadows = True
        if hasattr(scene.eevee, "taa_render_samples"):
            scene.eevee.taa_render_samples = 96
    if hasattr(scene.render, "samples"):
        try:
            scene.render.samples = 96
        except Exception:
            pass


def build_scene():
    pls.reset_scene()
    build_ground()
    build_water()
    build_sky()
    build_sun()

    templates = {}

    def tpl(name):
        if name not in templates:
            templates[name] = build_template(name)
        return templates[name]

    # --- Palmeras: linea de costa entre la selva y el mar, inclinadas hacia
    # el mar (+Y), y apartadas del pasillo central |x|<7 para que la camara
    # de nivel de jugador mire al mar sin un tronco delante. -------------
    palm_names = ["SM_LowPolyPalmA_01", "SM_LowPolyPalmB_01", "SM_LowPolyPalmC_01", "SM_LowPolyPalmD_01"]
    palm_x = [-30, -24, -17, -11, -6.5, 6.5, 11, 17, 24, 31]
    for i, x in enumerate(palm_x):
        name = palm_names[i % len(palm_names)]
        y = 6 + RNG.uniform(-4, 10)
        lean = math.radians(RNG.uniform(8, 16))
        place(tpl(name), (x, y, 0), rotation_z=RNG.uniform(0, math.tau), rotation_x=lean, scale_jitter=0.12)

    # --- Selva: pared de copas anchas a mas de 25 m detras de la camara
    # (camara sobre Y=0-8; selva en Y=-33..-52: nunca menos de ~30 m). ------
    wide_names = ["SM_LowPolyJungleWideA_01", "SM_LowPolyJungleWideB_01",
                  "SM_LowPolyJungleWideC_01", "SM_LowPolyJungleWideD_01"]
    for i, x in enumerate(range(-38, 40, 7)):
        name = wide_names[i % len(wide_names)]
        y = -40 + RNG.uniform(-6, 6) + (5 if i % 2 == 0 else 0)
        place(tpl(name), (x, y, 0), rotation_z=RNG.uniform(0, math.tau), scale_jitter=0.15)

    # --- Suelo de selva: hierba y helechos densos junto a la selva, bajos
    # (todos <1.6 m: no tapan la vista de una camara a 1.7 m). ---------------
    shrub_names = ["SM_LowPolyShrubA_01", "SM_LowPolyShrubB_01",
                   "SM_LowPolyShrubFlowering_01", "SM_LowPolyShrubBanana_01"]
    fern_names = ["SM_LowPolyFernA_01", "SM_LowPolyFernB_01"]
    for i in range(16):
        x = RNG.uniform(-36, 36)
        y = -32 + RNG.uniform(-10, 10)
        name = shrub_names[i % len(shrub_names)] if i % 3 == 0 else fern_names[i % len(fern_names)]
        place(tpl(name), (x, y, 0), rotation_z=RNG.uniform(0, math.tau), scale_jitter=0.2)

    # --- Rocas de orilla, cerca del agua ---------------------------------------
    rock_names = ["SM_LowPolyRockShoreA_01", "SM_LowPolyRockShoreB_01", "SM_LowPolyRockShoreC_01",
                  "SM_LowPolyRockSandA_01", "SM_LowPolyRockSandB_01"]
    for i in range(10):
        x = RNG.uniform(-36, 36)
        y = 30 + RNG.uniform(-6, 8)
        name = rock_names[i % len(rock_names)]
        place(tpl(name), (x, y, 0), rotation_z=RNG.uniform(0, math.tau), scale_jitter=0.25)

    # --- Hierba y flores repartidas por la playa (fuera del pasillo central) --
    grass_names = ["SM_LowPolyGrassA_01", "SM_LowPolyGrassB_01", "SM_LowPolyGrassC_01"]
    for i in range(22):
        x = RNG.uniform(-36, 36)
        if -6 < x < 6:
            x += 10 if x >= 0 else -10
        y = RNG.uniform(-10, 28)
        place(tpl(grass_names[i % len(grass_names)]), (x, y, 0), rotation_z=RNG.uniform(0, math.tau), scale_jitter=0.3)

    flower_names = ["SM_LowPolyFlowerA_01", "SM_LowPolyFlowerB_01", "SM_LowPolyFlowerC_01"]
    for i in range(12):
        x = RNG.uniform(-34, 34)
        if -6 < x < 6:
            x += 9 if x >= 0 else -9
        y = RNG.uniform(-8, 26)
        place(tpl(flower_names[i % len(flower_names)]), (x, y, 0), rotation_z=RNG.uniform(0, math.tau))

    # --- Troncos, ramas y setas sueltas por la arena (recolectables) --------
    debris_names = ["SM_LowPolyLog_01", "SM_LowPolyLogLarge_01", "SM_LowPolyStump_01",
                     "SM_LowPolyBranch_01", "SM_LowPolyMushroomRed_01", "SM_LowPolyMushroomTan_01"]
    debris_positions = [(-22, 12), (-15, -2), (14, 15), (24, 3), (-28, 20), (30, -3)]
    for name, (x, y) in zip(debris_names, debris_positions):
        place(tpl(name), (x, y, 0), rotation_z=RNG.uniform(0, math.tau))


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    build_scene()
    set_eevee()
    scene = bpy.context.scene

    # La camara de nivel de jugador esta sobre la arena, entre la selva
    # (Y<-25) y el mar (Y>40), mirando al mar con las palmeras a los lados
    # (|x|>9), no de frente.
    cameras = [
        ("eyelevel", make_camera("Cam_EyeLevel", (1, -9, 1.7), (2, 42, 2.2), lens=24.0)),
        ("aerial", make_camera("Cam_Aerial", (-60, -20, 24), (5, 15, 4), lens=24.0)),
        ("threequarter", make_camera("Cam_ThreeQuarter", (38, -20, 8), (-8, 12, 4), lens=28.0)),
    ]

    for suffix, cam in cameras:
        scene.camera = cam
        out_path = OUT_DIR / f"lowpoly_{suffix}.png"
        scene.render.filepath = str(out_path)
        bpy.ops.render.render(write_still=True)
        print(f"[ok] render -> {out_path}")


if __name__ == "__main__":
    main()
