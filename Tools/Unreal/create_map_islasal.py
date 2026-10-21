"""Crea /Game/Maps/Map_IslaSal: la Isla de Sal de F3a (docs/v3/plan.md, GDD v3 §6).

Contenido: AExploredOcean, AExploredSkyController (ya trae sol, luna, atmósfera, luz de cielo,
niebla y nubes; por eso no se añade otra DirectionalLight), PostProcessVolume infinito, el terreno
SM_IslaSal_Terrain (malla facetada de Tools/Island, importada con import_island.py) en el origen,
el PlayerStart en el punto «player_start» de island.json (Cala Varada) apoyado en el terreno con
una traza, y un AExploredIslandPopulator que en BeginPlay crea las colocaciones de
island_props.json. Idempotente: si el mapa existe, lo recrea.

Uso: Tools\\unreal_python.ps1 -Script Tools/Unreal/create_map_islasal.py
"""

import json
import os

import unreal

MAP_PATH = "/Game/Maps/Map_IslaSal"
TERRAIN_PATH = "/Game/Art/Island/SM_IslaSal_Terrain"
REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
ISLAND_JSON = os.path.join(REPO_ROOT, "Content", "Data", "island.json")
# El PlayerStart va a esta altura sobre el suelo (la cápsula mide 1,8 m).
PLAYER_START_LIFT_CM = 100.0
TRACE_TOP_CM = 20000.0
TRACE_BOTTOM_CM = -5000.0


def load_class(path):
    cls = unreal.load_class(None, path)
    if cls is None:
        raise RuntimeError(f"No se encuentra la clase {path}")
    return cls


def spawn(actor_subsystem, cls, location, label, yaw=0.0):
    actor = actor_subsystem.spawn_actor_from_class(cls, location, unreal.Rotator(0.0, 0.0, yaw))
    if actor is None:
        raise RuntimeError(f"No se pudo colocar {label}")
    actor.set_actor_label(label)
    return actor


def island_point(point_id):
    with open(ISLAND_JSON, "r", encoding="utf-8") as f:
        data = json.load(f)
    for point in data["points"]:
        if point["id"] == point_id:
            return point
    raise RuntimeError(f"island.json no tiene el punto {point_id}")


def ground_z(world, x_cm, y_cm):
    hit = unreal.SystemLibrary.line_trace_single(
        world, unreal.Vector(x_cm, y_cm, TRACE_TOP_CM), unreal.Vector(x_cm, y_cm, TRACE_BOTTOM_CM),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        raise RuntimeError(f"La traza no encuentra el terreno en ({x_cm}, {y_cm})")
    location = hit.to_tuple()[4]  # (blocking_hit, initial_overlap, time, distance, location, ...)
    return location.z


def main():
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

    terrain_mesh = unreal.EditorAssetLibrary.load_asset(TERRAIN_PATH)
    if terrain_mesh is None:
        raise RuntimeError(f"Falta {TERRAIN_PATH}: ejecuta antes import_island.py")

    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        unreal.EditorAssetLibrary.delete_asset(MAP_PATH)
    if not level_subsystem.new_level(MAP_PATH, False):
        raise RuntimeError(f"No se pudo crear {MAP_PATH}")

    spawn(actor_subsystem, load_class("/Script/Explored.ExploredOcean"), unreal.Vector(0, 0, 0), "ExploredOcean")
    spawn(actor_subsystem, load_class("/Script/Explored.ExploredSkyController"), unreal.Vector(0, 0, 0),
          "ExploredSkyController")
    post = spawn(actor_subsystem, unreal.PostProcessVolume, unreal.Vector(0, 0, 0), "PostProcess_Global")
    post.set_editor_property("unbound", True)

    terrain = spawn(actor_subsystem, unreal.StaticMeshActor, unreal.Vector(0, 0, 0), "IslaSal_Terrain")
    component = terrain.static_mesh_component
    component.set_static_mesh(terrain_mesh)
    component.set_editor_property("mobility", unreal.ComponentMobility.STATIC)

    world = editor_subsystem.get_editor_world()
    start = island_point("player_start")
    x_cm, y_cm = start["position"][0] * 100.0, start["position"][1] * 100.0
    start_z = ground_z(world, x_cm, y_cm) + PLAYER_START_LIFT_CM
    spawn(actor_subsystem, unreal.PlayerStart, unreal.Vector(x_cm, y_cm, start_z), "PlayerStart", start.get("yaw", 0.0))

    spawn(actor_subsystem, load_class("/Script/Explored.ExploredIslandPopulator"), unreal.Vector(0, 0, 0),
          "IslandPopulator")

    if not level_subsystem.save_current_level():
        raise RuntimeError(f"No se pudo guardar {MAP_PATH}")
    unreal.log(f"Map_IslaSal creado: {len(actor_subsystem.get_all_level_actors())} actores, "
               f"PlayerStart en ({x_cm:.0f}, {y_cm:.0f}, {start_z:.0f}) cm")


main()
