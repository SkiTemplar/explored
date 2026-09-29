# Tala universal: integración en el motor

Nota para la sesión local con Unreal. Los modelos puros ya están hechos y probados en
`Tools/HostTests` (`Explored.Felling`, `Explored.GroundBranch`, `Explored.FelledDrift`); falta conectarlos a la capa
de Unreal. Reglas y números: GDD v2 §3.12.

| Modelo | Fichero | Qué decide |
|---|---|---|
| `FFellingModel` | `WorldGen/FellingModel.h` | Golpes por herramienta, dirección de caída, dónde cae cada unidad, etapas del tocón, arrancar con pala |
| `FGroundBranchModel` | `WorldGen/GroundBranchModel.h` | Ramas sueltas por celda: capacidad, reaparición, posición, recogida |
| `FFelledDriftModel` | `WorldGen/FelledDriftModel.h` | Lo que cae al agua: flota o se hunde, deriva, vara, se refloata con la marea y se entrega como madera flotante |
| `FHarvestModel` | `WorldGen/HarvestModel.h` | Sigue igual para hierba y roca, y para `PerHitDrops` (lo que suelta cada golpe) |

## Dónde se engancha

Todo pasa hoy por `UExploredWiringSubsystem::HarvestInstance` (`Core/ExploredWiringSubsystem.cpp`).

1. **Clase de herramienta.** Hay que sustituir `HasHarvestTool(Instigator, RequiredTag)` por una
   función que devuelva `EFellingTool`: la pala es el objeto `pala` (template), `Filo` la
   etiqueta de filo y `Contundente` la contundente; si no hay nada, `Hands`. Si el objeto
   tiene filo y es contundente, gana `Edge`.
2. **Golpe.** Si la especie tiene `FFellingProfile`, en `FVegetationRuntimeState` se cambia
   `int32 Hits` por `FFellingProgress` y se llama a `FFellingModel::ApplyHit` con
   `HitDirection = (Tronco − Jugador).XY`. Los `PerHitDrops` de `FHarvestModel` se siguen
   soltando.
3. **Caída.** Cuando `ApplyHit` devuelve true:
   - `Downhill`: el gradiente del terreno se saca de `FTerrainDensity` con diferencias
     centrales a ±50 cm alrededor de la base, cambiadas de signo y en tangente (sin
     normalizar).
   - `InstanceSeed = Hash3D(Seed, Cell.X, Cell.Y, Index)`, igual que el resto de
     semillas por instancia.
   - La dirección sale de `ResolveFallDirectionWithWind(Profile, Progress, Downhill,
     InstanceSeed, WindDirection, Wind)`. `Wind` es `FWeatherSample::Wind` (0–1) del
     subsistema de tiempo en el momento del último golpe, y `WindDirection` la misma
     dirección en el plano que ya recibe `FWildfireConditions`. Solo el servidor la
     calcula; los clientes reciben la dirección ya resuelta con el evento de caída, así
     que el viento no puede desincronizar la caída entre jugadores.
   - **Aplastamiento.** Antes de lanzar la animación, el servidor pide a
     `UBuildingSubsystem` las piezas cuya huella toca la caja del tronco (base, dirección,
     `HeightMeters` y 1,5 m de margen) y las pasa como `FFellingObstacle` (`PieceId`,
     centro XY, media diagonal como radio, `TierOrderOf` e `Integrity` de la ficha).
     `ComputeCrush` devuelve el daño de cada pieza; se aplica restando de
     `FBuildingPieceState::Integrity` al final de la animación, cuando el tronco toca el
     suelo, y después se llama a `CollapseUnsupported` como con cualquier otro daño.
     Las piezas de madera y piedra no reciben nada: el tronco se queda encima (el actor
     temporal puede detener la rotación al tocarlas, pero eso es solo visual).
   - La animación es un actor temporal `AExploredFallingTree`: coge la malla de la
     instancia, la hace rotar sobre la base hacia la dirección de caída en unos 1,5 s y
     después se destruye.
   - Al terminar se llama a `ComputeFellDrops(Profile, Base, Dir, HarvestRandom)` y se
     genera un `AExploredItemActor` por unidad: la Z sale de una traza hacia abajo y la
     posición XY viene del modelo.
   - No hace falta simulación física del tronco. Si más adelante se quiere que ruede por
     la pendiente, se añade en el actor; no toca el modelo.
4. **Tocón.**
   - La instancia no se oculta: se pasa a escala `GrowthScaleAt(...)`. Con escala 0 el
     tocón es un HISM aparte, `Stump_<Especie>`, con una instancia por tocón en la misma
     celda.
   - Entre `Sapling` y `Mature`, la instancia original se escala de `SaplingStartScale`
     a 1 con `UpdateInstanceTransform(..., bMarkRenderStateDirty = false)`, y se marca
     sucia una vez por barrido, no una vez por instancia.
   - Arrancar con pala llama a `ApplyUprootHit`. Si devuelve true, se quita la instancia
     del tocón, se sueltan los `UprootDrops` y queda `Uprooted`.
5. **Ramas del suelo.**
   - Cada `AExploredVegetationCell` lleva un `FGroundBranchCell`, con semilla
     `Hash2D(Seed, Cell.X, Cell.Y)`. Sus fuentes (`FFellingModel::MakeBranchSource`) se
     sacan de las instancias en pie de las especies leñosas; se reconstruyen al talar y al
     rebrotar a `Mature`.
   - Cada rama se pinta como una instancia de un HISM `GroundBranch` de la celda, y el
     jugador la recoge con `Pick(Celda, Fuentes, NowMinute, Serial, &Rama, &Nuevas)`.
     `Pick` avanza antes la celda hasta `NowMinute`, así que da igual que el barrido de
     `Advance` no haya pasado por ella; si en ese avance aparecen ramas (`Nuevas > 0`),
     son las últimas de `Present` y hay que añadir sus instancias igual que tras `Advance`.
   - Una rama puede quedar fuera del cuadrado de su celda, porque la copa lo cruza.
     Sigue perteneciendo a la celda de su árbol: la clave de guardado es la celda del
     árbol, no `CellOf(posición)`.

## Persistencia en WorldDeltas

Hoy `FSaveScatterDeltas` solo guarda conjuntos de índices, sin tiempo. Por eso el rebrote
actual es de sesión y no se guarda (ver el comentario en `HarvestInstance`). Con el tocón
que rebrota eso ya no basta. Los dos modelos puros que faltaban ya están hechos:
`FVegetationClockModel` (`WorldGen/VegetationClockModel.h`, spec
`Explored.VegetationClock`) y el guardado de celdas de `FGroundBranchModel` (spec
`Explored.GroundBranch`, bloque «guardado»).

- **Capa `felled:<componente>`**: índices talados de cada celda, igual que ahora.
- **Capa `uprooted:<componente>`**: índices arrancados. Nunca rebrotan.
- **Sección `vegetationClock`** = `FVegetationClockModel::Save()`:
  `{"version": 1, "stumps": [[X, Y, "<componente>", Índice, FelledAtMinute, UprootWork], ...]}`.
  - Ordenada por (Y, X), componente (sin distinguir mayúsculas, como `FName`) e índice:
    el texto no depende del orden de tala.
  - `UprootWork` guarda los golpes de pala a medias; `bUprooted` se deduce
    (`UprootWork ≥ WorkToFell`).
  - `Load` rechaza la sección entera (y deja el reloj vacío) ante versión, tipo, rango o
    clave repetida. Tope de 2^20 entradas.
- **Al cargar la partida**, en este orden:
  1. `Clock.Load(Sección)`. Si devuelve false, se sigue con el reloj vacío.
  2. Por cada capa `felled:<componente>`, `Clock.Reconcile(Componente, Capa, Ahora)`.
     Un índice talado sin hora entra como talado ahora: rebrota más tarde, nunca antes.
     Una entrada cuyo índice ya no está en `felled` se descarta. Una hora futura (reloj
     manipulado) se acota a la de carga.
- **En partida.**
  - Al tumbar un ejemplar: `felled.Add(Celda, Índice)` y `Clock.RecordFelled(Clave, Ahora)`.
    Talar un brote reinicia su hora.
  - El golpe de pala usa `FFellingModel::ApplyUprootHit(Perfil, *Clock.FindMutable(Clave), ...)`.
    Si arranca: `uprooted.Add`, `felled.Remove` y `Clock.Remove(Clave)`.
  - En el barrido de rebrote (cada 10 s de juego), `Clock.PruneMature(ProfileOf, Ahora, &Maduras)`
    y `felled.Remove` de cada madura: el guardado no crece sin límite. `ProfileOf` traduce el
    componente HISM a su `FFellingProfile`; un componente sin perfil no se toca.
- **Ramas del suelo.**
  - Al guardar, solo las celdas con `FGroundBranchModel::NeedsSave(Celda, Fuentes)`. Una
    celda que `Initialize` reproduciría exactamente (llena, series 0…capacidad−1 en su
    sitio) no ocupa nada.
  - Forma: `SaveCell` → `{"version", "lastMinute", "accumulator", "nextSerial",
    "branches": [[Serie, X, Y, "objeto", SourceIndex], ...]}`, bajo la clave de la celda
    del árbol (no `CellOf(posición)`).
  - **Se guardan las posiciones** (reales exactos). No se recalculan con `PlaceBranch`,
    porque al talar cambian las fuentes y las ramas ya caídas saltarían de sitio al cargar
    (lo prueba un spec). Son unos 60 B por rama y, como mucho, unas decenas de ramas por
    celda tocada.
  - `LoadCell` rechaza series repetidas o desordenadas y un `nextSerial` que no quede por
    encima de todas, porque se repetirían al avanzar. Si falla, `Initialize` (celda llena).
- **Hora del juego.** Todo va en minutos de juego enteros (`int64`), con el reloj de
  `Sky`. No se usan segundos reales.

## Coste por frame

- **Golpe.** Son operaciones O(1) enteras, sin asignaciones. `ComputeFellDrops` asigna una
  sola vez, al caer el árbol, entre 3 y 12 unidades.
- **Viento y aplastamiento.** Una sola vez por árbol caído: `ApplyWindToFall` es O(1) y
  `ComputeCrush` es O(piezas candidatas), que tras el filtro por caja son unas pocas
  decenas como mucho. El daño no se guarda aparte: queda en la integridad de la pieza,
  que ya persiste `FBuildingPieceState`.
- **Rebrote.**
  - No se hace tick por instancia. Un barrido cada 10 s de juego recorre solo los tocones
    de las celdas cargadas (`VegetationRuntime`, decenas o cientos) y llama a
    `GrowthScaleAt`, que es O(1).
  - Solo se actualiza el transform de las instancias cuya escala ha cambiado más de 1 %.
  - La presupuestamos por debajo de 0,05 ms por barrido.
- **Reloj de tocones.** `RecordFelled`, `Find` y `Remove` hacen una búsqueda binaria
  (O(log n) comparaciones, cada una con dos `ToString` del componente) y `RecordFelled`
  inserta con un desplazamiento O(n). Con los cientos o pocos miles de tocones vivos de
  una partida son microsegundos, y solo ocurren al talar o arrancar. `PruneMature` es
  O(n) y va en el barrido de 10 s; `Reconcile` es O(n + talados) y solo al cargar.
- **Ramas del suelo.**
  - `Advance` es O(ramas nuevas). Solo se llama al cargar la celda y en un barrido lento
    (cada minuto de juego) de las celdas a menos de 150 m del jugador.
  - Una celda lejana no avanza hasta que se vuelve a cargar. Como el tiempo es entero,
    avanzar de golpe da lo mismo que ir paso a paso (lo prueba un spec).
- **HISM.**
  - `UpdateInstanceTransform` con `bMarkRenderStateDirty = false` y una sola
    `MarkRenderStateDirty` por componente y barrido.
  - Nunca `RemoveInstance`, porque desplaza índices y rompería los deltas. Se usa escala
    0, como ya hace `HideVegetationInstance`.

## Caída al agua (`FFelledDriftModel`)

Biblia 02 §1.5: lo que suelta un árbol que cae al agua flota y, si nadie lo recoge, pasa
a ser madera flotante normal. Modelo puro `WorldGen/FelledDriftModel.h`, spec
`Explored.FelledDrift`. Reglas y números en el GDD v2 §3.12 («Caída al agua»).

### Dónde se engancha

- **Al caer.** En el paso 3, después de `ComputeFellDrops`, el servidor mira cada unidad:
  - `FindFloatSpec(Specs, ItemId)` y `NeedsTracking(Spec, ProfundidadConPleamar)`.
  - La profundidad con la pleamar sale de `FOceanTide` (pleamar del día) menos la Z del
    suelo, que da la misma traza hacia abajo que ya coloca el `AExploredItemActor`.
  - Lo que no hace falta seguir se genera como siempre, un `AExploredItemActor` en el
    suelo. Lo que sí, entra en el `FFelledDriftModel` de un `UFelledDriftSubsystem`
    (`UWorldSubsystem`, solo en el servidor) con `AddPiece`, y su actor lleva el índice.
- **Consultas.** `WaterDepth(P)` = `AExploredOcean::GetWaterHeightAt` (sin olas; basta
  con la marea) menos la altura del suelo. La altura del suelo se toma de la arena viva
  si el chunk tiene capa `sand` y, si no, de `FTerrainDensity`, con caché por celda de
  0,25 m. `Current(P)` = `FOceanCurrents::CurrentAt(Straits, P, Flow, Spring, Wind)`, la
  misma que ya recibe `FBoatModel`.
- **Tick.** `Advance(DeltaSeconds, ...)` en el tick del subsistema. Después se mueve cada
  actor que flota a `Drop.Position` (Z = superficie del agua más el balanceo de
  `FOceanWaves::HeightAt`, que es solo visual). Los que varan se quedan con Z del suelo.
- **Recoger.** La interacción normal del `AExploredItemActor`. Antes de dar el objeto se
  llama a `Collect(Índice)`. Lo hundido (`madera_dura`, `resina`) se recoge buceando,
  sin cambios en la interacción.
- **Entregar.**
  - `Report.HandedOver`: se destruye el actor y se pasa `HandoverItemId` a la madera
    flotante de las playas (`FBeachDebrisModel`, categoría `debris`). El coco pasa como
    coco.
  - `Report.Decayed`: se destruye el actor sin más.
  - Si ningún jugador está a menos de 80 m, el mismo radio activo de la arena viva, o
    se descarga el chunk de la pieza, se llama a `Release` en lugar de congelarla (sin
    el chunk no hay suelo para `WaterDepth`). Justo después se lee el estado:
    `HandedOver`/`Decayed` se tratan como arriba; `Released` deja el actor como un
    `AExploredItemActor` normal (suelta el índice) que se guarda con su chunk. Al volver
    a cargar el chunk, si cumple `NeedsTracking`, vuelve a entrar con `AddPiece`, igual
    que al cargar la partida.
- **Red.** Solo simula el servidor. El actor replica su posición con el movimiento
  replicado normal, sin nada propio, porque las piezas son pocas y lentas.

### Persistencia en WorldDeltas

- Las piezas que flotan no se guardan. Al guardar, cada pieza `Floating` se entrega
  (`HandOver`), porque un tronco a la deriva no tiene un sitio estable.
- Las piezas `Resting` son objetos sueltos normales y se guardan como cualquier
  `AExploredItemActor`. Al cargar, las que cumplan `NeedsTracking` vuelven a entrar con
  `AddPiece`. `FloatingSteps` vuelve a 0: lo peor que pasa es que un tronco tarde un poco
  más en convertirse en madera flotante.

### Coste por frame

- A 2 Hz. Por paso y pieza que flota: una consulta de corriente y de 2 a 11 de
  profundidad (la del sitio y una por tramo de 25 cm; a 1 m/s son 3), más 8 al varar.
- Por paso y pieza varada que flota: una consulta de profundidad para ver si sube el
  agua. Lo hundido, lo recogido y lo entregado no cuesta nada (lo prueba el spec).
- Un gigante talado al agua son unas 10 piezas: menos de 0,01 ms por paso. El tope de
  `MaxPieces` (1024) cuenta solo las piezas activas y protege de un guardado manipulado.
  Lo recogido, entregado o deshecho deja su hueco a la siguiente pieza, así que el array
  no crece en una sesión larga. Por eso el subsistema procesa `Report.HandedOver` y
  `Report.Decayed` (lee `HandoverItemId` y destruye el actor) antes de la siguiente
  llamada a `AddPiece`, y el actor suelta su índice al recogerse.
- Si el subsistema se queda atrás, `MaxStepsPerAdvance` (2 min) descarta el resto de
  pasos, como la arena viva al acercarse.
