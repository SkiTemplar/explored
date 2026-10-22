"""Pone el sombreado por bandas (M_PP_Toon, build_materials.py) en el PostProcess_Global de Map_IslaSal y
quita la sombra que proyecta el terreno (SM_IslaSal_Terrain): con el sol bajo, cada loma de triángulos de
1,8 m proyectaba polígonos oscuros de borde recto sobre la duna y la hierba (crítica de f8-arena-sol); el
relieve se sigue leyendo por su sombreado suave, y el terreno sigue recibiendo las sombras del resto.

create_map_islasal.py llama a apply_toon_post al crear el mapa; ejecutado solo, abre el mapa
versionado, lo aplica y lo guarda (idempotente).

Uso: Tools/unreal_python.ps1 -Script Tools/Unreal/map_post.py
"""

import unreal

MAP_PATH = "/Game/Maps/Map_IslaSal"
POST_LABEL = "PostProcess_Global"
TOON_POST_PATH = "/Game/Art/Materials/M_PP_Toon"
TERRAIN_MESH_PATH = "/Game/Art/Island/SM_IslaSal_Terrain"


def apply_toon_post(post_volume):
    material = unreal.load_asset(TOON_POST_PATH)
    if material is None:
        raise RuntimeError(f"Falta {TOON_POST_PATH}: ejecuta antes build_materials.py")
    settings = post_volume.get_editor_property("settings")
    blendables = unreal.WeightedBlendables()
    blendables.set_editor_property("array", [unreal.WeightedBlendable(1.0, material)])
    settings.set_editor_property("weighted_blendables", blendables)
    post_volume.set_editor_property("settings", settings)


def terrain_without_cast_shadow(world) -> int:
    """El terreno no proyecta sombra (sí la recibe). Devuelve cuántos componentes se han tocado."""
    terrain = unreal.load_asset(TERRAIN_MESH_PATH)
    changed = 0
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.StaticMeshActor):
        component = actor.get_editor_property("static_mesh_component")
        if component is None or component.get_editor_property("static_mesh") != terrain:
            continue
        component.set_editor_property("cast_shadow", False)
        changed += 1
    if changed != 1:
        raise RuntimeError(f"{MAP_PATH} debe tener un único actor con {TERRAIN_MESH_PATH} (hay {changed})")
    return changed


def main():
    if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH):
        raise RuntimeError(f"No se pudo abrir {MAP_PATH}")
    world = unreal.EditorLevelLibrary.get_editor_world()
    volumes = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PostProcessVolume)
               if a.get_actor_label() == POST_LABEL]
    if len(volumes) != 1:
        raise RuntimeError(f"{MAP_PATH} debe tener un único {POST_LABEL} (hay {len(volumes)})")
    apply_toon_post(volumes[0])
    terrain_without_cast_shadow(world)
    if not unreal.EditorLoadingAndSavingUtils.save_current_level():
        raise RuntimeError(f"No se pudo guardar {MAP_PATH}")
    unreal.log(f"[map_post] {TOON_POST_PATH} aplicado a {POST_LABEL} de {MAP_PATH}; el terreno no proyecta sombra")


if __name__ == "__main__":
    main()
