# Balance · espantapájaros

2026-09-27 · `building_pieces.json`, `plants.json` · Validado con `Tools/DataCheck`
(`uv run datacheck`; 0 errores y 0 avisos).

## Problema

`plants.json` marca `platanera` y `maracuya` con `birdsEat: true`, y `FFarmModel` les
quita parte de la cosecha si no hay un espantapájaros a menos de `ScarecrowRadius`
(15 m). El GDD §8.7 lo pide («Riego, compost, espantapájaros») y la biblia lo pone en
«Granja». Pero no había ninguna pieza construible: las aves siempre ganaban.

## Cambio

| Pieza | Tier | Coste | Herramientas | Trabajo | Integridad / ciclón | Malla |
|---|---|---|---|---|---|---|
| `espantapajaros` | palma | 2 palos rectos, 4 hojas de palma, 1 liana, 4 conchas pequeñas | — | 10 min | 15 / 0 | pendiente (`null`) |

- **Tier palma, sin herramientas:** el bancal también es de palma, así que las aves
  llegan con la primera cosecha. El remedio tiene que estar disponible a la vez.
- **Conchas:** hacen ruido con el viento. Se recogen en la playa, así que no retrasan
  nada.
- **Frágil (15 / ciclón 0):** es barato y el primer temporal lo tira. Reconstruirlo es
  un pequeño mantenimiento del huerto, como regar.
- **Malla:** no existe ninguna `SM_*` de espantapájaros. La pieza lleva `mesh: null` y
  entra en `meshes_pendientes.json/buildingPieces`
  (`uv run datacheck --write-pending`).

DataCheck exige ahora que `birdsEat` sea booleano y que exista la pieza
`espantapajaros` si alguna planta lo usa.

## Qué se pierde sin él

`FFarmModel`: en cada cosecha, con probabilidad `BirdChance` = 0,5, las aves se llevan
`max(1, ceil(n × BirdShare))` con `BirdShare` = 0,25.

| Planta | Cosecha media | Pérdida media por cosecha | Cosechas en un año (32 días) | Pérdida al año |
|---|---|---|---|---|
| `platanera` | 6 | ≈ 1 (≈ 17 %) | 4 (fruta el día 8, cada 6) | ≈ 4 plátanos |
| `maracuya` | 4,5 | ≈ 1 (≈ 22 %) | 8 (fruta el día 8, cada 3) | ≈ 8 maracuyás |

Con 10 minutos y materiales de playa, el espantapájaros se amortiza con la primera
cosecha.

## Pendiente en C++ (fuera del alcance de datos)

- **Conexión pieza → modelo:** hoy solo `AExploredPlantActor::bHasScarecrow` llama a
  `UFarmSubsystem::AddScarecrow`. Hay que hacer que `UBuildingSubsystem`, al colocar o
  destruir una pieza `espantapajaros`, llame a `AddScarecrow` / `RemoveScarecrow` con
  su posición. Hasta entonces la pieza se puede construir pero no protege.
- **Malla:** `SM_Base_Scarecrow` en `Tools/Blender/props/mobiliario_base.py` (cruz de
  palos, capa de hojas de palma, sartas de conchas). Al añadirla, cambiar `mesh` y
  regenerar los pendientes.
