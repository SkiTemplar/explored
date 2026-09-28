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
2. Importar los FBX a `/Game/Generated/Meshes/` en Unreal (el commandlet de
   worldgen resuelve las mallas por nombre desde ahi, no desde disco
   directamente: ver `ResolveScatterMeshes` en
   `Source/ExploredEditor/WorldGenCommandlet.cpp`).
3. Revalidar con `Tools/Blender/validate.py` — ojo: exige el set de
   materiales `{M_Bark, M_Leaf, M_Rock, M_Grass}`. Desde el v2 (Quaternius
   via Poly Pizza para follaje, ver `packs.json`) esto ya no es un unico
   material compartido: `Palm/JungleWide/Shrub/Fern/Grass/Flower` conservan
   el material nativo de cada `.glb` (atlas con degradado propio, sin
   tocar), y solo `Rock/Debris` (Kenney, `.obj`) siguen remapeados al
   material plano `M_LowPoly_Palette`. Habria que adaptar el validador o
   remapear ambos grupos a los nombres canonicos antes de integrarlo.

Esta tarea no hace ese ultimo paso a proposito (el encargo pide exportar y
renderizar una comparativa, no sustituir el pipeline en caliente mientras hay
otro agente horneando en el arbol principal).
