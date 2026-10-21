"""Pone el sombreado por bandas (M_PP_Toon, build_materials.py) en el PostProcess_Global de Map_IslaSal.

create_map_islasal.py llama a apply_toon_post al crear el mapa; ejecutado solo, abre el mapa
versionado, lo aplica y lo guarda (idempotente).

Uso: Tools/unreal_python.ps1 -Script Tools/Unreal/map_post.py
"""

import unreal

MAP_PATH = "/Game/Maps/Map_IslaSal"
POST_LABEL = "PostProcess_Global"
TOON_POST_PATH = "/Game/Art/Materials/M_PP_Toon"


def apply_toon_post(post_volume):
    material = unreal.load_asset(TOON_POST_PATH)
    if material is None:
        raise RuntimeError(f"Falta {TOON_POST_PATH}: ejecuta antes build_materials.py")
    settings = post_volume.get_editor_property("settings")
    blendables = unreal.WeightedBlendables()
    blendables.set_editor_property("array", [unreal.WeightedBlendable(1.0, material)])
    settings.set_editor_property("weighted_blendables", blendables)
    post_volume.set_editor_property("settings", settings)


def main():
    if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH):
        raise RuntimeError(f"No se pudo abrir {MAP_PATH}")
    world = unreal.EditorLevelLibrary.get_editor_world()
    volumes = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PostProcessVolume)
               if a.get_actor_label() == POST_LABEL]
    if len(volumes) != 1:
        raise RuntimeError(f"{MAP_PATH} debe tener un único {POST_LABEL} (hay {len(volumes)})")
    apply_toon_post(volumes[0])
    if not unreal.EditorLoadingAndSavingUtils.save_current_level():
        raise RuntimeError(f"No se pudo guardar {MAP_PATH}")
    unreal.log(f"[map_post] {TOON_POST_PATH} aplicado a {POST_LABEL} de {MAP_PATH}")


if __name__ == "__main__":
    main()
