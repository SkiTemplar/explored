# Auditoría de los modelos puros — 2026-09-28

Alcance: los 64 ficheros de `Tools/HostTests/pure_sources.txt` (unas 29 000 líneas).
Criterios: divisiones por cero, NaN que se propagan, índices fuera de rango, desbordes,
dependencia del orden de iteración y no determinismo. Rama
`claude/audit-pure-models-quality-l5fwix`, un commit por arreglo, cada uno con el test que
lo caza en el `*Spec.cpp` del modelo.

## Resumen

- **HostTests con ASan/UBSan sobre `main`:** 819 casos, 0 fallos. Los sanitizers no
  encontraban nada porque ningún test metía las entradas que rompen los modelos.
- **Causa del fallo del 2026-09-28 (`FBoatModel::Moor`):** HostTests compilaba con IEEE
  estricto y el editor con `/fp:fast`. Se añade `HOST_TESTS_FASTMATH=ON` (`-ffast-math`), y
  con él **4 specs que ya existían fallaban sobre `main`**: RaftYard «daño NaN», RainCatch
  «NaN no llueve», Tramway «SetLoad NaN» y SaveArchive «menos infinito». Es el mismo fallo
  que el de `Moor`, en otros sitios.
- **Shim:**
  - `FMath::IsFinite`/`IsNaN` miran los bits, como `FGenericPlatformMath`. `std::isfinite`
    se pliega a una constante con `-ffast-math`.
  - `FMath::Clamp(NaN)` devuelve el máximo, como `UnrealMathUtility.h`; antes devolvía el
    NaN.
  - `Min`/`Max` ya coincidían con Unreal: con NaN devuelven el segundo argumento.
- **CI:** `host-tests.yml` pasa también la batería con matemáticas rápidas.
- **Hallazgos:** arreglados en 54 commits `fix`, uno por arreglo (algunos cubren varias entradas de la misma función). Hay 7 cuelgues reproducidos,
  conversiones float→int con UB, desbordes de int32/int64, 2 reservas de memoria de varios GB
  a partir de datos y 1 fallo de determinismo (caché kárstica).
- **Vía de entrada principal:** el guardado. `FSaveValue` lee `"NaN"`, `"Infinity"` y
  `"-Infinity"` como reales y acepta cualquier `int64`. El checksum FNV no tiene clave, así
  que un guardado editado a mano lo supera.
- **Tests de propiedades nuevos:** `Tests/PropertyFuzzSpec.cpp` (`Explored.Fuzz`), con
  semilla fija y 24 semillas por propiedad. El fuzz de valores hostiles encontró 2 fallos
  más, que ya están arreglados.
- **Orden de iteración:** no hay resultados guardados ni deterministas que dependan del
  orden de un `TMap`/`TSet`. Todos los guardados ordenan sus claves y `FSaveValue` ordena las
  de los objetos. El fuzz del tranvía lo comprueba: guardar da el mismo texto con cualquier
  orden de colocación y con retiradas intermedias.

Regla que sale de esta auditoría (queda en `Tools/HostTests/README.md`): **toda entrada real
que venga de fuera se comprueba con `FMath::IsFinite`**, antes de compararla y antes de
convertirla a entero. Eso incluye guardados, red, `DeltaSeconds`, datos de tablas y
posiciones del mundo. Las formas `!(x > 0)`, `x == x`, `x <= 0`, `Max(0, x)` y
`Clamp(x, …)` no descartan NaN con matemáticas rápidas; varias no lo descartan ni con IEEE.

## Verificación

| Batería | Resultado |
|---|---|
| `Tools/HostTests/run.sh` | 880 casos, 0 fallos |
| `HOST_TESTS_FASTMATH=ON Tools/HostTests/run.sh` | 880 casos, 0 fallos |
| `HOST_TESTS_SANITIZE=ON Tools/HostTests/run.sh` | 880 casos, 0 fallos, sin informes de ASan ni UBSan |
| `Tools/DataCheck`: `datacheck --strict` · `pytest -q` | 0 errores · 183 passed |

Cada test nuevo falla sin su arreglo (en modo normal, con matemáticas rápidas o en los dos).
Si sin el arreglo el test se colgaría, se comprobó con `timeout` o se razonó.

## Hallazgos y arreglos

Columnas: commit · hallazgo (entrada → efecto) · arreglo · test (spec y `It`).

### Comprobaciones de NaN que no funcionan con matemáticas rápidas

| Commit | Hallazgo | Arreglo | Test |
|---|---|---|---|
| `e3e3206` | `RaftYardDetail::IsFiniteValue` usaba `V == V`. `ApplyJointDamage`, `DamageJoint`, `Push` y `WaterSupport01` filtraban con `!(x > 0)`. En el editor, un `DeltaSeconds` NaN empujaba la balsa 64 cm y un daño NaN rompía uniones. | `FMath::IsFinite` | RaftYardModelSpec «un astillero vacío, un camino sin tramos y entradas no finitas no rompen nada» (tiempo NaN/inf, roce NaN, golpe infinito) |
| `b993adf` | `RainMmPerHour(NaN)` daba 50 mm/h, la lluvia máxima. | `IsFinite` en la intensidad y en el área | RainCatchModelSpec «NaN no llueve» (ya existía y fallaba con fast-math) |
| `a5ef6f2` | Tranvía: con un radio NaN, `DamageInSphere` dañaba **toda** la vía. `SetLoad`/`PlaceCart` aceptaban NaN y un guardado con `"load": "NaN"` entraba. | `IsFinite` | TramwayModelSpec «solo dañan los tramos…», «la carga admite hasta 200 kg…», «rechaza guardados rotos…» |
| `b91cfec` | `FBoatModel::Step(NaN)` metía el NaN en el acumulador de tiempo. | `IsFinite`; un paso infinito también se ignora | BoatSpec «ignora pasos de tiempo nulos, negativos o no finitos sin estropear el estado» |
| `db70d4a` | `FallDamage(NaN)` daba daño NaN, que acababa en la salud. | `IsFinite` | BodySpec «la arena y el agua amortiguan…» |
| `81c0b7c` | `Cartography::Sample` con posición NaN la guardaba como última posición y la llevaba a los trazos. `!(Distancia <= Alcance)` dejaba grabar con distancia NaN. | Se ignora la muestra y se corta el trazo | CartographySpec «ignora posiciones y distancias a la orilla no finitas…» |
| `ac71243` | `FFellingModel::CellOf`: `FloorToInt32` de un NaN o de una coordenada enorme (UB). | Origen para no finitos; recorte a ±1e9 | FellingModelSpec «CellOf usa suelo…» |
| `a49a6ef` | El spec de SaveArchive comparaba `Read < -Max`, que `-ffast-math` pliega a falso. | Comprobar por bits | SaveArchiveSpec «guarda NaN e infinitos…» |

### Cuelgues

| Commit | Hallazgo | Arreglo | Test |
|---|---|---|---|
| `e22d55f` | `CollectTrap(Trap, NaN)` o `simulatedToDays` NaN/enorme hacían que `FloorToInt` diera INT_MIN y el bucle de horas diera ~2³¹ vueltas. | `IsFinite`, horas en int64, recuperación máxima de 60 días | FishingSpec «un tiempo NaN, infinito o enorme no cuelga la trampa» |
| `1941f74` | `TickWetness`: con `InkRunProgress` ≥ 3e7 o infinito, `-= 1` no cambia el valor y el `while` no terminaba. | `IsFinite`; como mucho 4 pasadas por llamada | CartographySpec «el agua con tiempos o progreso no finitos o enormes no cuelga el mapa» |
| `1f7f040` | Tranvía: con `S`, `V` o `LoadKg` NaN, el vagón cruzaba nodos sin fin en una vía cerrada. | Saneado y tope de `Nodes.Num()+2` cruces; después descarrila | TramwayModelSpec «una carga, velocidad o posición NaN en el vagón no cuelga el paso en una vía cerrada» |
| `b6a899c` | Pelea con el pez: con un paso de 1e30, restar 1/60 no lo reduce. | Tope de 5 s por llamada | FishingSpec «un paso NaN o enorme no cuelga la pelea» |
| `a4a06ca` | Incendio: con `lastSecond` = INT64_MIN, la resta desbordaba y daba ~2⁶³ pasos. Minutos y celdas cargados desbordaban. | `Load` exige rangos; se compara antes de restar | WildfireModelSpec «minutos y celdas extremos no desbordan», «rechaza partidas mal formadas…» |
| `83a803f` | Arena: `ColumnOf` envolvía a int32 y `for (X = Lo; X <= INT32_MAX; ++X)` no terminaba (`Dig` en x = 536870911,5). `dirty` sin tope. Una pleamar NaN la convertía en INT64_MIN. | Rango ±1e9 columnas; tope de 2¹⁸ columnas sucias; la pleamar no finita conserva la anterior | SandModelSpec «pinceles degenerados…», «rechaza datos…», «una pleamar no finita en la revisión no seca la arena mojada» |
| `7ec33b0` | `HeightBounds`: un bucle en `float` no avanza en x = 1e8 ni con paso 0; sin muestras, `FloorToInt32(FLT_MAX)` (UB). | Contador entero, posición en double, paso y rectángulo validados | WorldGenSpec «acota la altura con cualquier rectángulo y paso, sin colgarse» |

### NaN, infinitos y cantidades absurdas desde el guardado o desde fuera

| Commit | Hallazgo | Arreglo | Test |
|---|---|---|---|
| `fa78b4c` | `TryAddCargo(NaN)` devolvía true (fallaba también con IEEE). | `IsFinite` | BoatSpec «rechaza cargas no finitas sin tocar la carga» |
| `852fcb4` | `FBoatModel::FromSaveData` no comprobaba posición, rumbo ni amarre; aceptaba un cabo infinito y un daño NaN daba el barco por destrozado. `SetVelocityCmS` no comprobaba nada. | Valores por defecto | BoatSpec «un guardado con NaN o infinitos vuelve a valores por defecto» |
| `aa41be4` | `SetVelocityCmS(1e30, …)` (finito) dejaba en NaN la posición y el rumbo al paso siguiente. **Lo encontró el fuzz.** | Recorte a 50 m/s | BoatSpec «un guardado con NaN o infinitos…» (arrancada enorme) |
| `f2a7e2d` | `Butcher`: `RoundToInt` del peso NaN o enorme (UB). | Peso acotado a 0,05–2000 kg antes de convertir | FishingSpec «un peso NaN, infinito o enorme da un despiece acotado» |
| `420df5c` | `Cartography::LoadState`: con `Drift` NaN no se volvía a dibujar costa nunca; las listas de cobertura y boceto desparejadas se leían fuera de rango. | Saneado y redimensionado | CartographySpec «sanea al cargar la deriva no finita…» |
| `84066e0` | `SwimModel::Tick`: `Max(0, NaN)` dejaba el oxígeno en NaN para siempre y el jugador no se ahogaba nunca. | `IsFinite`; `SetOxygen(NaN)` se ignora | SwimSpec «un paso de tiempo o un oxígeno NaN no dejan el oxígeno en NaN» |
| `5e175f5` | `WaitForBite`: una tasa NaN pasaba `<= 0` y picaba siempre; una espera enorme daba un bucle de hasta 2³¹ vueltas. | `IsFinite`; espera máxima de 3600 s | FishingSpec «una espera enorme o un instante o condiciones NaN no rompen la picada» |
| `8ccc090` | `ZoneKeyAt`, `SpotKeyAt`, `LowTideIndex` y `CastNet` hacían `FloorToInt` de NaN o de valores enormes (UB); `GatherTidePool` con tiempo NaN daba marisco. | `SafeFloorToInt` | FishingSpec «instantes y posiciones no finitos o enormes no dan claves con UB ni marisco» |
| `252d351` | `SnapNode`: `RoundToInt32` de una posición NaN o enorme. | Recorte a ±1e9 | TramwayModelSpec «ajusta a la rejilla también en coordenadas negativas» |
| `5c1a80a` | `PlaceOnPath` guardaba el camino sin comprobarlo. | Valores por defecto | RaftYardModelSpec «un camino con valores no finitos toma los valores por defecto» |
| `cba3d2e` | `AddPiece`/`AddLoad` aceptaban NaN y todo el casco daba NaN. | Se rechazan | HullAssemblyModelSpec «rechaza piezas y cargas con valores no finitos» |
| `befbf60` | Una ola de amplitud 0 hacía Q infinito y el desplazamiento NaN. | `Q = 0` con amplitud 0 | OceanCurrentsSpec «una ola de amplitud cero no da desplazamientos NaN» |
| `eeeb3ed` | Inventario: `"weightKg": -1000` permitía coger una roca de 500 kg; con NaN, el peso corporal daba NaN. | `ValidateState` exige cantidades finitas y ≥ 0 | InventorySpec «El guardado rechaza pesos, volúmenes y líquidos no finitos o negativos» |
| `6d92e2d` | `FillLiquid(NaN)` llenaba el recipiente (Clamp → máximo); `DrinkFrom(NaN)` lo vaciaba; `ShrinkItem` aceptaba NaN. | `IsFinite` | InventorySpec «El peso no llena ni vacía con litros no finitos», «Gastar materiales no mengua a cantidades no finitas o negativas» |
| `d618aa6` | Una mochila a medida con comodidad −15 daba un ratio de carga 0/0. | Validación y divisor mínimo | InventorySpec «El guardado rechaza una comodidad de mochila negativa o no finita» |
| `3be187a` | Con `nextInstanceId` = INT64_MAX, `Id + 1` desbordaba (UB). Un id negativo en un contenedor del mundo hacía rechazar el inventario entero. | Ids en [1, 2⁶²]; se descarta solo el objeto malo | InventorySpec «El guardado no deja que los ids de instancia desborden»; SaveSystemsSpec «Inventario descarta los objetos con id negativo o enorme sin perder el resto» |
| `f3e2ba9` | `LoadBuilding`: `RoundToInt` de una celda con 1e30 o NaN (UB). | Se descarta la pieza | SaveSystemsSpec «Construcción descarta una pieza con celda no finita o fuera de int32» |
| `0f03f1e` | `LoadSurvival` no comprobaba los valores del cuerpo; `Max(0, NaN)` en los estados; heridas con profundidad 2 o sangrado −50. | `SaneFloat`; heridas acotadas | SaveSystemsSpec «Cuerpo sanea valores no finitos o fuera de rango» |
| `0f00dd5` | Una hora NaN se cargaba como 23,999; `forcedUntilDays: "Infinity"` forzaba el tiempo para siempre. | Por defecto / sin forzar | SaveSystemsSpec «Barcos, pesca y reloj no carga una hora NaN ni un clima forzado para siempre» |
| `5a97c70` | Logros: `Max(v, NaN)` machacaba el perfil; `Infinity` desbloqueaba todo; `["a","a"]` inflaba los conjuntos. | Se saltan los no finitos; `AddUnique` | SaveSystemsSpec «Logros ignora números no finitos y repetidos en los conjuntos» |
| `6d06f12` | Deltas del mundo: `"r:1048575"` (11 bytes) reserva 128 KiB, así que 90 000 celdas piden unos 11 GB. | Tope de 16 MiB por capa y en total | SaveSlotsSpec «acota la memoria total de muchas celdas con el índice máximo» |
| `5f8b540` | Un `playTimeSeconds` NaN rompía el orden estricto de «Continuar». | NaN → 0 | SaveSlotsSpec «el almacén lee un tiempo de juego no finito o negativo como 0» |
| `d4b05df` | `DaysSurvived` hacía `FloorToInt` de NaN o 1e30 (UB); el odómetro a vela y el ratio de carga dejaban pasar NaN. | `IsFinite`; saturación | SystemLinksSpec «Estadísticas no se rompe con valores no finitos» |
| `63124f3` | Construcción: `Tick(NaN)` hacía `Max(NaN, 0) = 0` y **todas las piezas se rompían y se derrumbaban**; `ApplyDamage(NaN)` destruía la pieza. | `IsFinite` | BuildingSpec «un paso de tiempo no finito no rompe ni derrumba nada», «un daño no finito se ignora» |
| `1a1de35` | Un fuego cargado con `FuelHours` NaN no se apagaba nunca. En `Tick`, `CeilToInt(NaN)` es UB y 1e9 h desborda. | `FFireModel::Sanitize` al cargar; tope de 4096 subpasos | FireSpec «las entradas no finitas» (3 `It`) |
| `56dd880` | Boids y fauna marina: `CeilToInt(NaN)` (UB) y sin tope de subpasos (3600 s son 72 000 pasadas de vecinos); la cohesión dividía por un radio de 0. | `IsFinite`; como mucho 40 subpasos; `Max(1, radio)` | BoidsSpec «con radio de vecinos 0…», «ignora un paso no finito y topa uno enorme…»; MarineFaunaSpec «un Tick no finito no hace nada…» |
| `1a5acec` | Supervivencia: `Tick(NaN)` llenaba necesidades y salud (Clamp → máximo); un estado con tiempo NaN sangraba para siempre sin mostrarse; `Consume` con NaN. | `IsFinite` | SurvivalSpec (3 `It` de entradas no finitas) |
| `5dee017` | Una herida con profundidad > 1 sangraba sin tope; con sangrado negativo, curaba. | Acotado en `FBodyModel::Tick` | BodySpec «una herida fuera de rango se acota…» |
| `76903fa` | Huerto: `Max(0, NaN)` en el riego o la lluvia contaba el día como seco y el cultivo moría; con `GrowthDays` NaN no crecía nunca. | `IsFinite`; saneado en `SetState` | FarmSpec «un riego o una lluvia NaN…» |
| `8bd9fce` | Cocina: con un paso NaN la olla se atascaba y la comida no se estropeaba. | `IsFinite`; `SanitizePot` al cargar | CookingSpec «un paso NaN no atasca la olla…» |
| `20e3be2` | `RecipeMatches` reservaba `Count` punteros antes de comparar: `Count: 1e9` son unos 8 GB en cada `FindRecipe`. | Contar en int64 antes de reservar | CookingSpec «una receta con una cantidad enorme…» |
| `d4357e5` | Clima: `FloorToInt32` de un reloj NaN o enorme (UB). Eventos: un reloj de 1e9 generaba 1e9 días (cuelgue) y `++Day` desbordaba. | `MaxSupportedDays = 10000` | WeatherSpec «un reloj no finito o fuera de partida no desborda»; WorldEventsSpec «acota relojes no finitos…» |
| `c9216ec` | `BuildingModel::LoadState`: las celdas se enmascaraban a 16/8 bits (X = 65536 compartía hueco con X = 0), Z no se comprobaba contra `MaxLevels` y `Id + 1` desbordaba. | Rangos al cargar | BuildingSpec «descarta al cargar celdas, bases e ids fuera de rango» |
| `2d78796` | `FaunaAnimation`: `Max(0, NaN)` dejaba la fase en NaN para siempre. | `IsFinite` | MarineFaunaSpec «un paso o una fase no finitos…» |
| `e72351c` | Terreno: un pico en NaN escribía en X = INT_MIN y, al cargar, **se perdían todas las ediciones**; un radio o un número de peldaños enormes no tenían tope. | Entradas rechazadas; alcance de 16 m; 64 peldaños; cajas de ≤ 2²² muestras | TerrainEditModelSpec «las entradas no válidas se rechazan sin tocar la rejilla y el guardado sigue cargando» |
| `f24b252` | Con un presupuesto de tierra o de volumen infinito o NaN, la tierra y el tallado salían gratis. | Presupuesto no finito = nada | TerrainEditModelSpec «un presupuesto de tierra o de volumen no finito no da tierra ni tallado gratis» |
| `bad3692` | Ramas del suelo y tocones: con INT64_MIN guardado, la resta desbordaba. | Comparar antes de restar | GroundBranchModelSpec «un reloj o un acumulador guardados extremos no desbordan»; FellingModelSpec «con minutos extremos guardados no desborda» |
| `c274d90` | Con un layout vacío, `RangeInt(0, -1)` y `% 0` en Shipping. | Salida temprana | WorldGenSpec «un layout sin islas no tiene puntos de interés» |
| `652833f` | `FallDamage(1e30)` daba daño infinito. **Lo encontró el fuzz.** | Altura acotada a 1000 m | BodySpec «la arena y el agua amortiguan…» (altura enorme) |

### Determinismo

| Commit | Hallazgo | Arreglo | Test |
|---|---|---|---|
| `c6b8a2b` | La caché del macizo kárstico usaba solo la semilla como clave: dos layouts con la misma semilla y distinta altura o radio compartían la rejilla, y el resultado dependía de cuál se generaba antes. | Clave (semilla, altura, radio) | WorldGenSpec «no comparte el macizo kárstico…» |

## Tests de propiedades (`Tests/PropertyFuzzSpec.cpp`)

Todos usan `FExploredRandom` con semillas fijas (24 por propiedad) y, si fallan, indican la
semilla y el paso.

- **Inventario, 300 operaciones al azar por semilla** (coger, mover, equipar, arcón, soltar,
  líquidos, comer, guardar y cargar). Tras cada paso se comprueba que:
  - `ValidateState` pasa;
  - ningún objeto se crea ni se pierde (conservación de ids entre cuerpo, arcón y suelo);
  - una operación que falla no cambia nada;
  - el peso y los ratios son finitos;
  - la carga en un modelo nuevo reproduce el estado exacto.
- **Tranvía, orden:** el texto guardado es idéntico al colocar la vía en orden, barajada y
  con tramos al revés, o tras quitar y volver a poner la mitad. Cargar y guardar es
  idempotente.
- **Tranvía, vagón:** con órdenes al azar y fotogramas irregulares, el vagón no sale nunca de
  su tramo, no da valores no finitos y dos ejecuciones iguales dan el mismo vagón.
- **Valores hostiles:** la mitad de los valores son normales y la otra mitad NaN, ±inf,
  ±1e30, `FLT_MAX`, −1, 0 o denormales. Van a barco (pasos, carga, velocidad, amarre), nado
  (pasos, oxígeno), vagón (carga, colocación, pasos), mapa (muestras, agua) y a funciones
  puras (`FallDamage`, `RainMmPerHour`, `CellOf`, `CellAt`). El estado debe seguir finito y
  en rango, sin UB (UBSan). Encontró `aa41be4` y `652833f`.

## Pendiente (no arreglado, con motivo)

Decisiones de diseño:
- **`FTramwayModel::CartFromValue`:** acepta cargas de hasta 1e6 kg, mientras que `SetLoad`
  limita a `CapacityKg` (200). La función es estática y no ve `Settings`.
- **`FWildfireModel`:** un `lastSecond` por delante del reloj congela el fuego. `Load` no
  conoce el reloj y el contrato de la cabecera pide conservar `LastSecond`.
- **`FSandModel::Brush`:** con radios finitos enormes (≤ 1e8 m) los bucles terminan, pero
  hacen muchísimo trabajo. Falta un radio máximo de pincel.
- **Inventario:**
  - `ValidateState` no aplica el límite de peso, así que un guardado con más peso del
    permitido se carga.
  - Los campos de una mochila a medida (`MaxSlots`, `MaxSize`, `AcceptedTags`,
    `bWaterproof`) se cargan tal cual.
- **`RuinsModel.cpp:186`:** los ids de sitio salen del arquetipo de la isla. Si dos islas
  compartieran arquetipo, completar una ruina completaría las dos.

Fuera del alcance de esta pasada:
- **`FSavePlayerState::Load`** (`SaveWorldDeltas.cpp`): lee salud, hambre y demás sin
  comprobar, el mismo fallo que ya se arregló en `LoadSurvival`.
- **`FBodyModel::AddCut(NaN)`:** `Clamp` convierte el NaN en la herida más profunda.
- **Datos del tiempo:** los que llegan a `FSurvivalInputs` y `FBuildingWeather` no se sanean.
- **`FindCandidateChunks`:** con un rectángulo de mundo NaN o enorme sigue haciendo
  `FloorToInt32`. `SurfaceNets::Polygonize` con densidad NaN no se ha evaluado, y
  `SampleKarstGrid` con NaN propaga una altura NaN (sin salirse de rango).
- **`FindOrCreateBase`:** durante la partida no limita `NextBaseId` a `MaxBaseId`.
- **`UExploredWiringSubsystem::SaveInventory`** (motor, no puro): escribe los `payloads` en
  el orden de un `TMap<int64, …>`. El texto del guardado puede variar tras retiradas, aunque
  carga bien.

## Cambios de comportamiento con entradas válidas extremas

Todos los topes elegidos están en constantes con nombre y se pueden revisar:
- **Nasas:** con más de 60 días sin recoger, solo se simulan los 60 últimos.
- **`WaitForBite`:** espera como mucho 3600 s.
- **Pelea con el pez:** 5 s por llamada.
- **Boids y fauna marina:** 2 s por llamada (40 subpasos); el resto se descarta.
- **Fuego:** 4096 subpasos por `Tick`, unas 200 h.
- **Tinta corrida:** 4 pasadas por llamada.
- **Herramientas de terreno:** 16 m de alcance y 64 peldaños.
- **Arena:** 2¹⁸ columnas sucias guardadas.
- **Deltas del mundo:** 16 MiB por capa.
- **Clima y eventos:** 10 000 días.
- **Construcción:** celdas en ±32 000.
- **Arrancada del barco:** 50 m/s.
- **Caídas:** 1000 m.
