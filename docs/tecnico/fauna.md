# Fauna (P-FAUNA)

Fauna del GDD §10: solo vida marina y aves **siempre en vuelo**; nada camina ni se posa y
nada lleva esqueleto (§12). La lógica está en modelos puros de `Source/Explored/Fauna/`
(solo `CoreMinimal.h`, validados en `Tools/HostTests`) y la capa de Unreal solo los conecta.

`ProceduralGait` (cuadrúpedos, cangrejo, ave posada) es anterior a esta regla: se conserva,
pero la fauna nueva no lo usa.

## Modelos puros

| Fichero | Qué hace |
|---|---|
| `FaunaTypes.h` | `EFaunaSpecies` y su tabla (malla, estilo de animación, velocidades, profundidad mínima), consultas del mundo (`FFaunaWorldQuery`), estímulos (`FFaunaStimuli`), ciclo diario (`FFaunaActivity`) y percepción (`FFaunaPerception`: vista en cono, oído por ruido, olfato que deriva con la corriente) |
| `BoidsModel.h` | `FBoidsModel`: separación, alineación, cohesión, objetivo, huida, obstáculos, franja vertical, límites y topes de velocidad y aceleración; hash espacial de celdas; paso determinista |
| `FaunaGroups.h` | `FFishSchoolModel` (arrecife: se dispersa y se rehace; mar abierto: migra y gira antes de la costa), `FBirdFlockModel` (huye del jugador, sigue a la canoa con pescado, roba pescado al aire, vuelve a tierra al atardecer) y `FGullTheft` |
| `MarineCreatureBrain.h` | `FMarineCreatureBrain`: máquinas de estados de raya, medusa, tiburón de arrecife, tiburón tigre, delfín, tortuga y ballena |
| `FaunaSpawning.h` | `FFaunaSpawnRules` (población determinista por celda de 100 m según zona, profundidad y hora) y `FFaunaLod` (LOD de actualización) |
| `FaunaAnimation.h` | `FFaunaAnimation`: fase, amplitud y frecuencia para el shader |

Unidades: centímetros y espacio de mundo, nivel del mar Z = 0. WorldGen trabaja en metros:
convierte quien consulta (`AExploredFaunaManager`).

### Garantías (comprobadas en los specs)

- Peces: nunca por encima de `superficie − holgura` (con olas) ni por debajo de
  `fondo + holgura`; nunca entran donde no queda franja de agua (tierra, orilla seca).
- Aves: nunca por debajo de `max(agua, terreno) + MinAltitudeCm` ni de su velocidad mínima;
  las alas nunca se pliegan (`WingFold = 0`).
- Tiburón tigre: solo en agua de más de 30 m; solo ataca si el jugador está en ese agua (el
  límite natural del mundo). Tiburón de arrecife: curioso, da vueltas y rara vez ataca
  (3 % por encuentro, 25 % con sangre, 0 % en modo Explorador).
- Ballena: visible solo a más de 150 m del jugador; si la persiguen, se sumerge y reaparece lejos.
- Medusa: su deriva horizontal es exactamente la corriente.
- Aves al atardecer (17:00–19:30): la bandada vuela hacia la isla más cercana
  (`FFaunaWorldQuery::NearestLand`), lo que hace legible la técnica de wayfinding del GDD §6.2.
- Todo es determinista (misma semilla y entradas → mismo resultado bit a bit).

### Estados

| Especie | Estados |
|---|---|
| Raya | `Buried` → (jugador cerca; antes si arrastra los pies) `Flee` pegada al fondo → `Settle` → `Buried`. Pisarla enterrada: picadura |
| Medusa | `Drift` con la corriente; picadura en 150 cm |
| Tiburón de arrecife | `Wander` → (vista, oído u olor a sangre) `Curious` → `Circle` 15 s → `Retreat` (o `Attack` si la tirada lo decide) |
| Tiburón tigre | `Wander` → `Stalk` (círculos que se cierran; la mitad de tiempo con sangre) → `Attack` → `Retreat` → `Stalk`; si el jugador sale del agua profunda, `Retreat` |
| Delfín | `Wander` → `Accompany` (canoa a más de 1,5 m/s) con `Jump` cada 5–11 s |
| Tortuga | `Wander` en la laguna, `Breathe` cada 60–120 s, `Flee` lenta si te acercas; `Nesting` al llamar a `BeginNesting` (gancho de P-EVENTS) |
| Ballena | `Travel` cerca de la superficie con soplidos cada 25–45 s; `Sounding` (invisible) si el jugador se acerca |

## Capa de Unreal (sin compilar: verificar en local)

- `UExploredFaunaSubsystem` crea un `AExploredFaunaManager` al empezar la partida.
- El gestor puebla las celdas en un radio de 3 alrededor del jugador (se revisa cada 2 s),
  mueve bancos y bandadas con un `UInstancedStaticMeshComponent` por grupo (4 datos por
  instancia) y las criaturas sueltas como `AExploredMarineCreature` (sin tick propio ni
  colisión, 4 datos de primitiva). El LOD sigue `FFaunaLod` por grupo.
- Entradas: `ReportPlayerNoise`, `AddBloodInWater`, `SetPlayerBoatState`,
  `RegisterStealableFish`, `SetWhalePassage`, `SetPeaceful`, `StartTurtleNesting`.
- Salidas (delegados nativos): `OnFaunaDamage(especie, daño, lugar)`,
  `OnFaunaMoment(especie, "Jump" | "Blow" | "Startled" | "Steal", lugar)`,
  `OnFishStolen(actor)`, `OnTurtleNesting(lugar)`. Nadie los escucha todavía: Survival
  (daño) y Audio/Events deben suscribirse.
- Mallas: `/Game/Generated/Meshes/Fauna/<Especie>/SM_<Especie>_Body`. `import_meshes.py`
  aún no importa la familia de fauna (`Art/Export/Meshes/Fauna`, `animals.json`); hasta
  entonces los grupos existen pero no se ven.

## Parámetros para el material (`Tools/Unreal/build_materials.py`)

Cada malla trae el color de vértice **«Anim»** de `Tools/Blender/run_animals.py`
(importar en lineal): `R = spine_t` (0 cola → 1 cabeza), `G = máscara` (1 apéndice
deformable), `B = lado` (0 izquierda, 0,5 centro, 1 derecha).

Datos por instancia (`PerInstanceCustomData`, bancos y bandadas) o de primitiva
(`CustomPrimitiveData`, criaturas sueltas), mismos índices:

| Índice | Nombre | Significado |
|---|---|---|
| 0 | `AnimPhase` | Fase del ciclo en [0, 1) (acumulada en CPU: no salta al cambiar la velocidad) |
| 1 | `AnimAmplitude` | Onda: fracción de la longitud del cuerpo. Aleteo: grados. Pulso: contracción de la campana |
| 2 | `AnimFrequency` | Hz (informativo; la fase ya lo integra) |
| 3 | `AnimSecondary` | Aves: planeo 0–1 (alas quietas). Delfín y ballena: 1 = onda vertical. Medusa: contracción instantánea 0–1 |

Materiales propuestos (un parámetro estático `UseInstanceData` elige la fuente de datos):

| Material | Especies | Desplazamiento en el shader de vértices |
|---|---|---|
| `M_Fauna_SpineWave` | peces, tiburones, delfín, ballena, raya, tortuga | `offset = sin(2π·(AnimPhase − spine_t·WavesPerBody)) · AnimAmplitude · BodyLengthCm · (1 − spine_t)^TailFalloff`, en Y (lateral) o en Z si `AnimSecondary = 1`; la raya usa `B` (lado) como envolvente de las «alas» en Z; la tortuga mueve solo lo que tiene `G = 1` |
| `M_Fauna_Flap` | gaviota, fragata | giro de lo que tiene `G = 1` alrededor del eje del cuerpo: `sin(2π·AnimPhase) · AnimAmplitude · (B − 0,5) · 2`, atenuado por `AnimSecondary` |
| `M_Fauna_Pulse` | medusa | escala radial de la campana `1 − AnimAmplitude · AnimSecondary`; tentáculos (`G = 1`) con retardo `sin(2π·(AnimPhase − spine_t))` |

Parámetros escalares del material: `WavesPerBody` (≈ 1 peces, 0,6 mamíferos, 1,5 raya),
`BodyLengthCm` (de `FFaunaSpeciesInfo::BodyLengthCm`), `TailFalloff` (≈ 1,5). Los valores por
especie van en instancias de material.

## Pendiente

- Importar la fauna en `import_meshes.py` y crear los tres materiales.
- Conectar `OnFaunaDamage` con Survival y `OnFaunaMoment` con Audio.
- Embarcaciones reales (P-BOATS) en lugar de `SetPlayerBoatState`; paso de ballenas y desove
  desde P-EVENTS; sobrepesca por zona (biblia §4.5) con P-FISH.
