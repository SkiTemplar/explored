# Lluvia en recipientes: integración en el motor

Nota para la sesión local con Unreal. El modelo puro ya está hecho y probado en
`Tools/HostTests` (`Explored.RainCatch`, 18 casos). Falta conectarlo a la capa de Unreal.
Las reglas y los números están en el GDD v2 §3.16 y en la biblia 02 §5.4.

| Modelo | Fichero | Qué decide |
|---|---|---|
| `FRainCatchModel` | `Weather/RainCatchModel.h` | Cuánta lluvia entra, cuánto se evapora o rebosa, y si el agua es de lluvia, sin tratar o salobre |

## Dónde se engancha

1. **Qué es un recipiente del mundo.** Cualquier objeto de catálogo con boca en
   `MouthAreaM2ForItem` que esté **soltado en el suelo o colocado en una balda**, derecho
   y fuera del inventario. En la mano o en la mochila no recoge nada.
   - El actor del objeto soltado (o la ranura de la balda) guarda un `FRainCatchState`.
   - `FRainCatchSpec` sale de `SpecForItem(DefinitionId, Recipiente, bSheltered)`.
   - `bSheltered` se calcula al soltarlo y cuando se construye o destruye un techo
     cerca: un trazo vertical de 10 m hacia arriba por el canal de visibilidad. Si
     choca con una pieza de techo o con la copa de un árbol, está a cubierto. No se
     recalcula por fotograma.
2. **Reloj.**
   - `NowMinute` es el minuto entero de juego de `UTimeOfDaySubsystem`, el mismo reloj
     que usan la tala y las ramas del suelo (`vegetationClock`).
   - Un `NowMinute` fuera de ±`MaxSupportedMinute` se ignora sin tomarlo como hora
     actual. Un `LastUpdateMinute` cargado más allá del tope se corrige a la hora actual
     en el primer `Advance`, sin simular el hueco. No hace falta sanearlo al cargar.
   - El `FWeatherModel` es el del `UExploredWeatherSubsystem`, con la semilla del
     mundo, envuelto en un `FRainCatchSky` que vive en el subsistema. **No** hay que pasar la muestra en vivo del cielo: el modelo muestrea él
     mismo por franjas de 10 min alineadas con el minuto 0, y así el resultado no
     depende de cuándo se llame.
3. **Solo los cercanos.**
   - Un `URainCatchSubsystem` (`UWorldSubsystem`, solo con autoridad) mantiene la lista
     de recipientes de los chunks cargados a menos de **64 m** de algún jugador.
   - Cada **10 s reales** llama a `Advance(State, Spec, Sky, NowMinute)` en esos
     recipientes y en ningún otro.
   - Un recipiente que entra en ese radio (o un chunk que se carga) hace un único
     `Advance` hasta el minuto actual. El spec «un recipiente lejano puesto al día al
     cargarse coincide con uno simulado en vivo» demuestra que da exactamente lo mismo
     que haberlo simulado siempre.
4. **Al cogerlo.**
   - `FItemInstance::LiquidLiters = State.TotalLiters()`. La capacidad ya coincide con
     la de `FInventoryModel`.
   - `FInventoryItem` no sabe qué líquido lleva. Hasta que lo sepa, al cogerlo se
     convierte con `Quality`: `Rain` es agua potable, `Untreated` es
     `agua_sin_tratar` y `Sea` es `agua_mar`. Al soltarlo de nuevo, se hace un
     `Pour` con ese tipo.
5. **Beber o verter en el sitio.** Se hace con `Take` (proporcional) y `Pour` con el
   tipo del líquido de origen. Los dos devuelven lo que de verdad se ha movido, así que
   la masa cuadra (hay spec).
6. **Efectos (solo cosmético, en cada cliente).**
   - La altura del agua es un parámetro del material del objeto: `TotalMicroL /
     CapacityMicroL`.
   - Mientras `RatesFor(...).InPerMinute > 0`, se muestran salpicaduras con un Niagara
     compartido y un sonido de goteo con prioridad baja, solo a menos de 15 m.

## Red

- Simula el servidor.
- Se replica el nivel en un `uint8` (0–255 sobre la capacidad) y la calidad en 2 bits.
  Solo se envía cuando el nivel cambia en al menos 1/255 o cambia la calidad, a menos
  de 1 Hz por recipiente. Con lluvia fuerte son unos pocos bytes por segundo en toda
  la zona.
- Sin RPC nuevos: beber y verter usan la interacción de objeto que ya existe, validada
  en el servidor.

## Persistencia

- Una capa nueva, `"raincatch"`, junto a `WorldDeltas`, con una entrada por objeto del
  mundo que lleve agua: `instanceId`, `rain` (int64 µL), `other` (int64 µL),
  `sea` (int64 µL, la parte de `other` que es agua de mar), `otherKind` (uint8) y
  `lastMinute` (int64). Son 41 B por recipiente. Los contadores
  `Caught`/`Spilled`/`Evaporated` no se guardan.
- Un guardado sin `sea` (anterior a este campo) con `otherKind` de mar se carga como si
  todo lo ajeno fuera mar: `Sanitize` nunca lo deja menos salado de lo que era.
- Un recipiente vacío y sin agua ajena no se escribe.
- Al cargar, `Sanitize` deja el estado dentro de rango: cantidades negativas o enormes
  y un tipo desconocido. Hay spec.
- Las partidas antiguas no tienen la capa: los recipientes empiezan vacíos.

## Coste por fotograma

Medido en el host (`RelWithDebInfo`, un núcleo):

- **Nada en `Tick`.**
- El subsistema guarda un único `FRainCatchSky` (caché de muestras por franja de 10 min,
  como mucho 9000 franjas, unos 470 KB) y lo pasa a todos los `Advance`. Así cada franja
  se muestrea una sola vez para todos los recipientes.
- Un `Advance` de 10 s reales avanza unos pocos minutos de juego: una franja y, con solo
  lluvia dentro, una cuenta cerrada. Son menos de 1 µs por recipiente. Con 200
  recipientes cercanos (una base grande), menos de 0,2 ms cada 10 s.
- **Ponerse al día 60 días** (`MaxCatchUpMinutes`, el tope):
  - Con la caché caliente, **0,16 ms** por recipiente.
  - Con la caché fría, **23 ms** la primera vez: son 8640 llamadas a
    `FWeatherModel::SampleAt` a ~2,7 µs cada una. Solo pasa una vez por sesión, al cargar
    una partida en la que hay recipientes lejanos sin actualizar.
  - Para que no se note, tras cargar la partida el subsistema calienta la caché hacia
    atrás desde el minuto actual, a 500 franjas por fotograma (~1,4 ms). Mientras tanto,
    los recipientes que todavía no se han puesto al día muestran su nivel guardado.
- Con agua ajena dentro (mar o sin tratar), el modelo va minuto a minuto hasta que la
  lluvia la desplaza del todo: como mucho 86 400 pasos enteros, menos de 0,5 ms.
