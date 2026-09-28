# Tala universal: integración en el motor

Nota para la sesión local con Unreal. Los modelos puros están hechos y probados en
`Tools/HostTests` (`Explored.Felling`, `Explored.TreeFall`, `Explored.GroundBranch`,
`Explored.VegetationState`). El tocón con rebrote guardado ya está cableado en
`UExploredWiringSubsystem`, pero **sin compilar todavía**; la caída animada, las ramas
del suelo y la replicación faltan por conectar. Reglas y números: biblia 02 §1.2, §1.3 y
§1.6; GDD v2 §3.12 para los golpes por herramienta.

| Modelo | Fichero | Qué decide |
|---|---|---|
| `FFellingModel` | `WorldGen/FellingModel.h` | Golpes por herramienta, golpe final, dónde cae cada unidad, días de brote y rebrote, arrancar con pala |
| `FTreeFallModel` | `WorldGen/TreeFallModel.h` | Dirección con viento; qué construcción aplasta y en qué se apoya el tronco |
| `FGroundBranchModel` | `WorldGen/GroundBranchModel.h` | Ramas sueltas por celda: 2–4 por árbol cada 6 h, tope 6, posición, recogida, guardado |
| `FVegetationStateModel` | `WorldGen/VegetationStateModel.h` | Etapa por instancia, clave y estado de red, tope de 4096, reloj de tala del guardado |
| `FHarvestModel` | `WorldGen/HarvestModel.h` | Hierba y roca, `PerHitDrops` y horas de rebrote de lo que no tiene perfil de tala |

## Hecho en esta rama (verificar en local)

- `FVegetationRuntimeState` (`WorldGen/VegetationHarvestState.h`) lleva `Instance`
  (`FVegetationInstanceState`: golpes, minuto de tala, de brote y de rebrote) y `Species`.
- `HarvestInstance`: al talar, el índice va a la capa del componente en `WorldDeltas` y la
  hora a `VegetationClock` (sección `vegetationClock`). Los días salen de
  `FFellingModel::RegrowMinutes` si la especie tiene perfil; si no, de `RegrowHours`.
- `EnsureVegetationDeltasApplied`: al cargar, cada índice talado toma su hora del reloj
  (`ResolveFelledAt`: si falta, es negativa o es futura, cuenta como talado ahora). Lo que
  ya debía haber rebrotado sale de los deltas y se queda en pie.
- `TickVegetationRegrowth`: el brote escala de `SaplingStartScale` a 1 con
  `UpdateInstanceTransform(..., bMarkRenderStateDirty = false)` y una
  `MarkRenderStateDirty` por componente; al rebrotar vuelve el transform original y la
  instancia sale de los deltas y del reloj. El brote no se puede golpear (`bHidden`).

Qué probar en PIE:

1. Talar una palmera, dormir hasta el día 6 (asoma el brote) y hasta el 18 (talable).
2. Talar, guardar, salir al menú, cargar: el tocón sigue y rebrota a la misma hora.
3. Cargar una partida anterior a esta rama: lo talado sigue talado y rebrota 18 o 24 días
   después de cargar.
4. `Tools/test.ps1`: `Explored.Felling`, `Explored.TreeFall`, `Explored.GroundBranch`,
   `Explored.VegetationState` y `Explored.Save`.

## Falta por conectar

1. **Clase de herramienta.** Sustituir `HasHarvestTool(Instigator, RequiredTag)` por una
   función que devuelva `EFellingTool`: la pala es el objeto `pala`, `Filo` la etiqueta de
   filo y `Contundente` la contundente; si no hay nada, `Hands`. Si tiene filo y es
   contundente, gana `Edge`. Entonces `FVegetationRuntimeState` guarda un
   `FFellingProgress` y se llama a `FFellingModel::ApplyHit` con
   `HitDirection = (Tronco − Jugador).XY`.
2. **Caída.** Cuando `ApplyHit` devuelve true:
   - `Downhill`: gradiente del terreno con diferencias centrales a ±50 cm de la base
     (`FTerrainDensity`), cambiado de signo y en tangente.
   - `Wind`: dirección del viento del cielo y `FWeatherSample::Wind` (0–1) como fuerza.
   - `Dir = FTreeFallModel::ResolveDirection(Profile, Progress, Downhill, Wind, Seed)`,
     con `Seed = Hash3D(Seed, Cell.X, Cell.Y, Index)`.
   - Obstáculos: las piezas de `UBuildingSubsystem` a menos de la altura del árbol, como
     cilindros (`TierOrder` del nivel de material, `MaxIntegrity` de la pieza), y unas
     muestras del terreno a lo largo de `Dir` que se alcen sobre la base (`Kind = Terrain`).
   - `Result = FTreeFallModel::Resolve(Profile, Base, Dir, Obstacles)`: aplicar
     `ApplyDamage(PieceId, Hit.Damage)` a cada `Result.Crushed` y animar el actor temporal
     `AExploredFallingTree` hasta `Result.RestAngleDeg` en ~1,5 s.
   - Al terminar, `ComputeFellDrops(Profile, Base, Dir, HarvestRandom, Result.ReachFraction)`
     y un `AExploredItemActor` por unidad (Z por traza hacia abajo).
3. **Pala.** `ApplyUprootHit` sobre el tocón; si devuelve true, `UprootDrops`, el índice
   sale del reloj y el estado queda con `RegrowAtMinute = -1` (no rebrota).
4. **Ramas del suelo.**
   - Cada `AExploredVegetationCell` lleva un `FGroundBranchCell` con semilla
     `Hash2D(Seed, Cell.X, Cell.Y)`. Sus fuentes (`FFellingModel::MakeBranchSource`) son
     **todos** sus árboles, talados o en pie (biblia 02 §1.3), en orden de índice de
     instancia; un tocón arrancado pasa a `Capacity = 0` sin cambiar el orden.
   - Cada rama es una instancia de un HISM `GroundBranch` de la celda; se recoge con
     `Pick(Serial)` y da `rama_seca`.
   - Una rama puede caer fuera del cuadrado de su celda; sigue siendo de la celda de su
     árbol.
   - `Advance` al cargar la celda y en un barrido lento (cada minuto de juego) de las
     celdas a menos de 150 m. Avanzar de golpe da lo mismo que paso a paso.
5. **Red** (después de los cimientos de red de H0, biblia 08 §2.3): un
   `FFastArraySerializer` en el `GameState` con entradas `FVegetationNetEntry`. La tabla de
   huecos de especie sale de `FVegetationStateModel::SpeciesSlots` con los nombres de los
   HISM de la celda; `ToNet` da el estado a partir de `FVegetationRuntimeState::Instance`;
   al pasar de 4096 entradas, `Compact` saca los tocones sin rebrote al snapshot de celda.

## Persistencia en WorldDeltas

- **Capa `<componente>`**: índices talados de cada celda, como siempre.
- **Sección `vegetationClock`**: `[[X, Y, "<componente>", Índice, MinutoDeTala], …]`,
  ordenada por (Y, X, componente, índice). Una entrada rota se descarta sola; una
  repetida se queda con la tala más reciente. Al rebrotar, el índice sale de la capa y
  del reloj, así que el guardado no crece sin límite.
- **Ramas del suelo** (cuando se conecten): `FGroundBranchModel::ToValue` guarda
  `last`, `next` y las parejas (serie, árbol); las posiciones se recalculan con
  `PlaceBranch`. Una celda que nunca se ha tocado no se guarda: `Initialize` al cargarla.
- **Hora del juego.** Minutos enteros (`int64`) desde `UTimeOfDaySubsystem::GetTotalDays`
  con `FVegetationStateModel::MinuteFromDays` (por suelo). No se usan segundos reales.

## Coste por frame

- **Golpe.** O(1) entero, sin asignaciones. `ComputeFellDrops` asigna una vez al caer.
- **Caída.** `FTreeFallModel::Resolve` es O(obstáculos · log): decenas de piezas cerca.
- **Rebrote.** `TickVegetationRegrowth` recorre solo `VegetationRuntime` (instancias
  golpeadas, decenas o cientos); solo toca el transform si la escala cambia más de 1 %.
- **Ramas del suelo.** `Advance` es O(ciclos cruzados × árboles), con salida temprana en
  cuanto todos los árboles están llenos (3 ciclos como mucho con 2 por ciclo y tope 6).
- **HISM.** Nunca `RemoveInstance`, porque desplaza índices y rompería los deltas: se usa
  escala 0, como `HideVegetationInstance`.
