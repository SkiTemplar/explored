# Tala universal: integración en el motor

Nota para la sesión local con Unreal. Los modelos puros ya están hechos y probados en
`Tools/HostTests` (`Explored.Felling`, `Explored.GroundBranch`); falta conectarlos a la capa
de Unreal. Reglas y números: GDD v2 §3.12.

| Modelo | Fichero | Qué decide |
|---|---|---|
| `FFellingModel` | `WorldGen/FellingModel.h` | Golpes por herramienta, dirección de caída, dónde cae cada unidad, etapas del tocón, arrancar con pala |
| `FGroundBranchModel` | `WorldGen/GroundBranchModel.h` | Ramas sueltas por celda: capacidad, reaparición, posición, recogida |
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
     jugador la recoge con `Pick(Serial)`.
   - Una rama puede quedar fuera del cuadrado de su celda, porque la copa lo cruza.
     Sigue perteneciendo a la celda de su árbol: la clave de guardado es la celda del
     árbol, no `CellOf(posición)`.

## Persistencia en WorldDeltas

Hoy `FSaveScatterDeltas` solo guarda conjuntos de índices, sin tiempo. Por eso el rebrote
actual es de sesión y no se guarda (ver el comentario en `HarvestInstance`). Con el tocón
que rebrota eso ya no basta:

- **Capa `felled:<componente>`**: índices talados de cada celda, igual que ahora.
- **Capa `uprooted:<componente>`**: índices arrancados. Nunca rebrotan.
- **Tiempo de tala.** Se guarda en una sección nueva `vegetationClock` con la forma
  `[[X, Y, "<componente>", Índice, FelledAtMinute], ...]`, ordenada como
  `FSaveScatterDeltas`, y se carga en `FStumpState`.
  - Si se descarta la sección porque no se puede leer, el árbol se toma como talado
    ahora: rebrota más tarde, nunca antes.
  - Cuando la etapa llega a `Mature`, el índice sale de `felled` y del reloj, así que el
    guardado no crece sin límite.
- **Ramas del suelo.** No se guarda cada rama. Se guardan `LastUpdateMinute`,
  `Accumulator`, `NextSerial` y la lista de series presentes; las posiciones se vuelven a
  calcular con `PlaceBranch` desde las fuentes. Una celda que nunca se ha tocado no se
  guarda: al cargarla, `Initialize` la deja llena.
- **Hora del juego.** Todo va en minutos de juego enteros (`int64`), con el reloj de
  `Sky`. No se usan segundos reales.

## Coste por frame

- **Golpe.** Son operaciones O(1) enteras, sin asignaciones. `ComputeFellDrops` asigna una
  sola vez, al caer el árbol, entre 3 y 12 unidades.
- **Rebrote.**
  - No se hace tick por instancia. Un barrido cada 10 s de juego recorre solo los tocones
    de las celdas cargadas (`VegetationRuntime`, decenas o cientos) y llama a
    `GrowthScaleAt`, que es O(1).
  - Solo se actualiza el transform de las instancias cuya escala ha cambiado más de 1 %.
  - La presupuestamos por debajo de 0,05 ms por barrido.
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
