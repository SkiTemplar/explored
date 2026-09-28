# Cocos que caen al sacudir: integración en el motor

Nota para la sesión local con Unreal. El modelo puro ya está hecho y probado en
`Tools/HostTests` (`Explored.CoconutPalm`, 24 casos, también con ASan/UBSan). Falta
conectarlo a la capa de Unreal. Las reglas y los números están en el GDD v2 §3.18.

| Modelo | Fichero | Qué decide |
|---|---|---|
| `FCoconutPalmModel` | `WorldGen/CoconutPalmModel.h` | Qué cocos hay en la copa, cuáles caen solos, al sacudir, con las rachas o al talar, dónde caen y cuándo se pudren |

## Dónde se engancha

1. **Qué palmeras.** Las instancias de la especie `Palm` de `FVegetationScatter` en pie
   (y las que vuelven a ser adultas por rebrote, `FFellingModel::StageAt == Mature`).
   - La semilla es la de la instancia (la misma que usa la tala). Para una palmera
     rebrotada, `Hash32(semilla ^ número de rebrotes)`, para que no repita la copa anterior.
   - `TrunkPosition` es la posición de la instancia en el plano, en centímetros.
2. **Palmeras sin tocar: nada guardado.** Una palmera que nadie ha sacudido, trepado,
   talado ni recogido debajo se reconstruye al cargar su celda con
   `Initialize(semilla, pos, perfil, 0, true)` y `Advance(estado, perfil, NowMinute)`. El
   spec «avanzar 30 días de golpe, por horas o minuto a minuto da lo mismo» es lo que
   permite hacerlo.
3. **Reloj.** `NowMinute` es el minuto entero de `UTimeOfDaySubsystem`, el mismo que
   usan la tala, las ramas del suelo y la lluvia en recipientes.
4. **Verbos del tronco** (biblia 02 §8.3, «máximo tres verbos»):
   - Golpe con herramienta: talar (`FFellingModel`).
   - **E corto sin herramienta: sacudir.** `Shake(estado, perfil,
     HandShakeStrength(perfil), posición del jugador, NowMinute, Drops)`. Enfriamiento
     de 1,5 s reales para que no se machaque la tecla.
   - E mantenido: trepar; arriba, `PickFromCrown(..., bWantGreen, NowMinute)`.
5. **Rachas.** El `UExploredWeatherSubsystem` avisa al cruzar cada hora de juego; el
   subsistema de cocos llama a `ApplyGust(estado, perfil, Wind, Hora, Drops)` solo en las
   palmeras de celdas cargadas. Las lejanas no reciben rachas (es un efecto que se ve);
   `ApplyGust` ignora horas repetidas o anteriores a la última actualización, así que
   llamarla de más no hace daño.
6. **Tala.** Cuando `FFellingModel::ApplyHit` tumba una palmera, llamar a
   `Fell(estado, perfil, dirección de caída, NowMinute, Drops)` y soltar esos objetos
   como objetos del mundo. **Quitar** de ese mismo `ComputeFellDrops` los rendimientos
   `Fruit` de la palmera (`coco_maduro`, `coco_verde`, `cascara_coco`): si no, sacudir y
   luego talar vuelve a dar cocos. Hay spec que lo comprueba del lado del modelo.
7. **Coco en la cabeza.** Si un `FCoconutDrop` trae `bHitsShaker`, aplicar el daño
   cuando el coco llega al suelo (propuesta: 5 de salud, sin esguince; pendiente de la
   biblia 01).

## Actores y HISM

- **En la copa.** Un HISM `Coconut_Crown` por celda de vegetación, con una instancia por
  hueco ocupado en un socket fijo de la malla de la palmera (6 sockets `Coco_0..5`). El
  color va en un dato por instancia (0 verde, 1 maduro). Se reconstruye cuando cambia la
  etapa de algún hueco: como mucho unas pocas veces al día de juego por palmera.
- **En el suelo.** Un HISM `Coconut_Ground` por celda, con una instancia por
  `FFallenCoconut`. El índice de instancia se traduce a `(palmera, Id)` con un mapa en la
  celda, como las ramas del suelo. Recoger es `PickFromGround(estado, Id)`.
- **Caída.** Sin física: un actor temporal ligero (o un `UProjectileMovementComponent`
  sin colisión) baja la malla del coco desde el socket hasta `Position` en
  √(2·9 m / g) ≈ 1,4 s, suena el golpe y se añade la instancia al HISM del suelo. Los que
  caen solos mientras la celda está cargada hacen lo mismo; los de celdas sin cargar
  aparecen directamente en el suelo.
- La copa se agita al sacudir con el mismo balanceo por material que la tala.

## Persistencia en WorldDeltas

- Sección nueva `coconuts`, con una entrada **solo por palmera tocada**, indexada por el
  id de la instancia de vegetación:
  - `lastMinute` (int64), `shakeSerial` (uint32), `lastGustHour` (int64), `felled` (uint8);
  - por hueco, `generation` (int32) y `cycleStart` (int64): 72 B con 6 huecos;
  - por coco en el suelo, `id` (uint32) y `landed` (int64). **La posición no se guarda**:
    sale de `FallPosition(estado, perfil, hueco, generación)`, que se deduce del `id`.
  - En total, unos 100–150 B por palmera tocada.
- Los contadores (`FCoconutCounters`) no hacen falta para el juego; sirven para los
  logros (`coconuts_opened`, biblia 07 §2.3) y para los specs.
- Al talar, la entrada se queda (los cocos del suelo siguen pudriéndose) hasta que el
  suelo esté vacío; entonces se borra. Al rebrotar se crea otra con `Initialize(..., false)`.
- Las partidas antiguas no tienen la sección: todas las palmeras se reconstruyen sin tocar.

## Coste por frame

Medido en el host (`-O2`, un núcleo, 2000 palmeras):

- **Nada en `Tick`.** Solo hay trabajo al cargar una celda, al interactuar y una vez por
  hora de juego con las rachas.
- `Advance` de 10 minutos: **0,23 µs** por palmera. Ponerse al día 60 días: **1,5 µs**.
  Una celda con 40 palmeras cuesta ~60 µs al cargar.
- Reconstruir desde el minuto 0 crece con la edad de la partida (~27 ciclos por hueco y
  año): en una partida de un año de juego, unos 10 µs por palmera. Si llegara a notarse,
  guardar un «ancla» (estado completo) cada 30 días de juego en la celda.
- `Shake`: **0,67 µs**. `ApplyGust` en 200 palmeras cargadas con temporal: < 0,2 ms una
  vez por hora de juego.
- La lista del suelo está acotada: como mucho un coco vivo por hueco con los números
  por defecto (hay spec de diez años seguidos).

## Red

- Simula el servidor (biblia 08).
- Se replica por palmera cargada una máscara de 12 bits (2 bits por hueco: vacío, verde,
  maduro) y los `Id` del suelo cuando cambian. La caída de cada coco es un RPC multicast
  no fiable con `(palmera, Id, bHitsShaker)`: el cliente ya puede calcular la posición.
