# Mapeo de slots LowPoly a las reglas del scatter

Este set vive en `Art/Export/Meshes/LowPoly/<Slot>/` como comparativa: no
toca `Art/Export/Meshes/manifest.json` (el manifiesto en produccion, generado
por `Tools/Blender/run_all.py`). Si mas adelante se decide sustituir el set
procedural por este, cada slot mapea a una `FScatterRule` de
`Source/Explored/WorldGen/VegetationScatter.cpp::DefaultRules()` asi:

| Carpeta LowPoly/ | ManifestCategory | NameFilter esperado | Regla del scatter |
|---|---|---|---|
| `Palm/` | `palm` | (ninguno) | `Palm` |
| `JungleWide/` | `tree` | `Wide` | `JungleWide` |
| `JungleGiant/` | `tree` | `Giant` | `JungleGiant` |
| `Understory/` | `tree` | `Understory` | `Understory` |
| `Mangrove/` | `tree` | `Mangrove` | `Mangrove` |
| `Shrub/` | `shrub` | (ninguno) | `Shrub` |
| `Fern/` | `shrub` | (ninguno, mismo filtro que Shrub) | `Shrub` (los helechos comparten regla con arbustos: no hay `NameFilter` propio en `DefaultRules()`) |
| `Grass/` | `grass` | (ninguno) | `Grass` |
| `Flower/` | `grass` | (ninguno, mismo filtro que Grass) | `Grass` |
| `Rock/` | `rock` | (ninguno) | `Rock` |
| `Debris/` | `debris` | (ninguno) | `Debris` |

Para activar este set en produccion habria que:

1. Copiar (o apuntar) las entradas de `Art/Export/Meshes/LowPoly/lowpoly_manifest.json`
   dentro de `Art/Export/Meshes/manifest.json`, traduciendo `category` segun la
   tabla de arriba (los nombres de carpeta no coinciden 1:1 con
   `ManifestCategory`: `Fern` y `Flower` no son categorias propias del
   scatter, comparten regla con `Shrub`/`Grass`).
   IMPORTANTE (causa del follaje gris-marron diagnosticada 2026-09-28): TODO
   este set (incluidos `Rock/Debris`, remapeados a `M_LowPoly_Palette`) sale
   de `Tools/Packs/process_landing_set.py` con `material_slots: ["NATIVE"]`
   en `lowpoly_manifest.json` — cópialo tal cual al manifest.json de
   produccion, no lo traduzcas a `M_Bark`/`M_Rock`/etc. `Tools/Unreal/
   import_meshes.py` reconoce ese centinela y conserva el material y la
   textura que trae el FBX (ahora incrustada, ver `export_fbx`); cualquier
   otro valor le fuerza los 4 materiales estables de `MATERIAL_DEFS`
   (constante×color-de-vertice, pensados para el kit procedural), que es lo
   que dejaba el follaje plano y sin vida.
2. Importar los FBX a `/Game/Generated/Meshes/` en Unreal (el commandlet de
   worldgen resuelve las mallas por nombre desde ahi, no desde disco
   directamente: ver `ResolveScatterMeshes` en
   `Source/ExploredEditor/WorldGenCommandlet.cpp`).
3. Revalidar con `Tools/Blender/validate.py` — ojo: exige el set de
   materiales `{M_Bark, M_Leaf, M_Rock, M_Grass}`; ese validador todavia no
   sabe de `material_slots=["NATIVE"]` y habria que adaptarlo (o excluir del
   check a las categorias low poly) antes de integrar este set.

Esta tarea no hace ese ultimo paso a proposito (el encargo pide exportar y
renderizar una comparativa, no sustituir el pipeline en caliente mientras hay
otro agente horneando en el arbol principal). El fix de `import_meshes.py` y
de `process_landing_set.py::export_fbx` (texturas incrustadas en el FBX, ver
su docstring) si se incluyen ya, porque son la causa raiz del bug de color y
no dependen de tener el editor abierto para escribirlos, solo para probarlos.
