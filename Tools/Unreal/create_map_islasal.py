"""Crea /Game/Maps/Map_IslaSal: mapa provisional de la isla (F1, plan v3 §4).

Contenido: AExploredOcean, AExploredSkyController (ya trae sol, luna, atmósfera, luz de
cielo, niebla y nubes; por eso no se añade otra DirectionalLight), PostProcessVolume
infinito, una plataforma provisional de 40 x 40 m con la cara superior a 0,5 m y un
PlayerStart a 1 m sobre ella. Idempotente: si el mapa existe, lo recrea.

Uso: Tools\\unreal_python.ps1 -Script Tools/Unreal/create_map_islasal.py
"""

import unreal

MAP_PATH = "/Game/Maps/Map_IslaSal"
CUBE_PATH = "/Engine/BasicShapes/Cube.Cube"
PLATFORM_SIZE_CM = 4000.0
PLATFORM_THICKNESS_CM = 50.0
PLATFORM_TOP_CM = 50.0
PLAYER_START_HEIGHT_CM = 100.0


def load_class(path):
    cls = unreal.load_class(None, path)
    if cls is None:
        raise RuntimeError(f"No se encuentra la clase {path}")
    return cls


def spawn(actor_subsystem, cls, location, label):
    actor = actor_subsystem.spawn_actor_from_class(cls, location, unreal.Rotator(0.0, 0.0, 0.0))
    if actor is None:
        raise RuntimeError(f"No se pudo colocar {label}")
    actor.set_actor_label(label)
    return actor


def main():
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        unreal.EditorAssetLibrary.delete_asset(MAP_PATH)
    if not level_subsystem.new_level(MAP_PATH, False):
        raise RuntimeError(f"No se pudo crear {MAP_PATH}")

    spawn(actor_subsystem, load_class("/Script/Explored.ExploredOcean"), unreal.Vector(0, 0, 0), "ExploredOcean")
    spawn(actor_subsystem, load_class("/Script/Explored.ExploredSkyController"), unreal.Vector(0, 0, 0),
          "ExploredSkyController")

    post = spawn(actor_subsystem, unreal.PostProcessVolume, unreal.Vector(0, 0, 0), "PostProcess_Global")
    post.set_editor_property("unbound", True)

    cube = unreal.EditorAssetLibrary.load_asset(CUBE_PATH)
    if cube is None:
        raise RuntimeError(f"No se encuentra {CUBE_PATH}")
    # El cubo básico mide 100 cm y tiene el pivote en el centro.
    platform_center_z = PLATFORM_TOP_CM - PLATFORM_THICKNESS_CM / 2.0
    platform = spawn(actor_subsystem, unreal.StaticMeshActor, unreal.Vector(0, 0, platform_center_z),
                     "Plataforma_Provisional")
    component = platform.static_mesh_component
    component.set_static_mesh(cube)
    platform.set_actor_scale3d(unreal.Vector(PLATFORM_SIZE_CM / 100.0, PLATFORM_SIZE_CM / 100.0,
                                             PLATFORM_THICKNESS_CM / 100.0))

    spawn(actor_subsystem, unreal.PlayerStart,
          unreal.Vector(0, 0, PLATFORM_TOP_CM + PLAYER_START_HEIGHT_CM), "PlayerStart")

    if not level_subsystem.save_current_level():
        raise RuntimeError(f"No se pudo guardar {MAP_PATH}")
    unreal.log(f"Map_IslaSal creado: {len(actor_subsystem.get_all_level_actors())} actores")


main()
