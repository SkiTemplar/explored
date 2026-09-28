# Arena viva: cómo enganchar `FSandModel` en el motor

Estado: el modelo es puro y tiene spec en el host (`Explored.Sand`, 47 casos). Falta la
integración con Unreal, que tiene que hacer una sesión con el editor. Diseño de juego:
biblia 02 §5 (reglas), biblia 08 §2.6 (red y presupuesto) y GDD v2 §3.13 (números que
la biblia deja abiertos).

## Qué hay

- `Source/Explored/WorldGen/SandModel.h`: un campo de alturas de deltas, en milímetros
  enteros, sobre el suelo de la isla. La rejilla es de **0,25 m** y cada chunk tiene
  32 columnas, así que mide **8 m**: es la misma que la de `FTerrainEditModel`, y un
  chunk de arena cubre el mismo suelo que un chunk de edición.
- Herramientas: `Dig` y `Pile` (pincel en cono). `SetAnchor` sirve para las estructuras:
  la caja es la huella (no se cava) y todo lo que queda a ≤ 1 m queda sujeto (no desliza
  ni lo rellena el oleaje). Cuenta referencias.
- Revisión de pendiente: `Advance(DeltaMs, Env, Base)` hace una revisión por segundo,
  con un máximo de 4 por llamada; el tiempo que sobra se descarta (biblia 08 §2.6).
  `FSandEnvironment` lleva la pleamar del día (qué arena está húmeda), la lluvia y los
  focos (jugadores) con su radio activo de 80 m.
- Relleno por oleaje: `ApplyHalfTide(Tide, Base)`, una vez por medio ciclo de marea.
  `FSandTide` lleva la pleamar, la bajamar y si es marea viva.
- Todas las llamadas reciben el suelo base como `TFunctionRef<double(double X, double Y)>`,
  en metros. En el juego es
  `[&](double X, double Y) { return Density.SampleColumn(X, Y).Height; }`.
  La altura se cachea en mm la primera vez que se toca un chunk (1024 llamadas a
  `SampleColumn`, una sola vez por chunk).
- Cada llamada devuelve `FSandResult`:
  - `DirtyChunks`: chunks que hay que remallar, ordenados y sin repetir. Un chunk
    lee una columna de margen para las normales, así que en un borde salen 2 y en una
    esquina 4.
  - `Mass` en mm·columna, para el inventario. `MassToCubicMeters` lo pasa a m³:
    1 m³ = 16 000.
  - `SeaMass` en un medio ciclo: la arena que ha puesto el mar (negativa si se la ha
    llevado). `SeaBankMass()` es el acumulado, y `TotalMass() + SeaBankMass()` solo
    cambia con la pala.
  - `ColumnsChanged` (cambio neto: es lo que sale por la red), `ActiveColumns`,
    `DormantColumns`, `DeferredColumns` (esperan por el tope de 64 por chunk) y
    `ActiveChunks`, para medir el coste.
- Guardado: `ToValue()` y `FromValue()`. Los anclajes **no** se guardan, porque los
  vuelve a poner el sistema de construcción al cargar cada pieza.

## Enganche propuesto

1. **Subsistema de mundo `USandSubsystem`, solo en el servidor.** Es dueño de un
   `FSandModel`. El cliente nunca simula arena: recibe deltas de terreno (08 §2.6).
   - Cada fotograma llama a `Advance(DeltaMs, Env, Base)`. Casi siempre devuelve 0
     revisiones; una vez por segundo hace una. El entorno se rellena así:
     - `HighTide`: la pleamar del día, es decir, nivel medio del mar +
       `SpringNeapFactorAt(TotalDays)` × la amplitud de marea en metros. Hoy ningún
       sistema aplica una amplitud en metros; propuesta: 0,8 m. Solo decide qué arena
       está húmeda y se revisa cuando cambia 5 cm o más.
     - `bRaining`: lo da `UWeatherSubsystem` cuando la intensidad de lluvia pasa de 0,3.
       Regar arena a mano, si algún día existe, entra por aquí.
     - `Focus` y `ExtraFoci`: la posición de cada jugador. Un chunk cerca de dos
       jugadores se revisa una sola vez.
   - Al pasar cada pleamar y cada bajamar de `FOceanTide` (≈ 6 h de juego, 10 minutos
     reales), llama a `ApplyHalfTide` con la pleamar y la bajamar de ese medio ciclo y
     `bSpring` en luna llena o nueva. Toca todas las columnas editadas de la isla, no
     solo las cercanas.
   - **Salida por la red.** Cada llamada devuelve `ChangedColumns` (ordenadas y sin
     repetir). Por cada chunk tocado, `EncodePackets(Chunk, ChangedColumns)` da los
     paquetes `FExploredTerrainDeltaPacket` versión 2, capa 1 (arena), de 512 B como
     máximo y con valores absolutos (biblia 08 §2.2). Entran en la **misma cola** de
     08 §2.2, con **prioridad más baja** que las ediciones de los jugadores. Si el chunk
     ya está en la cola, basta con guardar la unión de columnas y codificar al salir: el
     valor final gana. Un cliente que llega recibe `EncodeFullChunk` (el primer paquete
     vacía el chunk). El cliente llama a `ApplyPacket`, que valida el paquete entero
     antes de tocar nada y no marca columnas sucias, porque el cliente no simula.
     `ChunkChecksum` es el FNV-1a de la comprobación de cada 30 s. Si la cola va llena
     porque alguien está cavando, la arena llega un poco más tarde. El tope de 64
     columnas cambiadas por chunk y revisión (`MaxChangedColumnsPerChunk`) es lo que
     mantiene la arena en ≈ 0,6 kbps comprimidos por avalancha en curso. Un medio ciclo
     de marea son unos pocos cientos de bytes por playa editada.
2. **Pala.** Si el material bajo el golpe es arena (`SurfaceLayers(...).X > 0,5`), la pala
   llama a `Dig` o a `Pile` en vez de a `FTerrainEditModel::Shovel`. El cubo guarda la
   masa (`int64`), no m³, para que cavar y echar cuadre al milímetro. En tierra o roca
   sigue mandando la edición volumétrica de §3.4.
3. **Malla: desplazamiento vertical, no remallado volumétrico.**
   - La playa sigue siendo la malla del terreno. Cada chunk de arena de 8 m con
     `EditedChunks()` sustituye su trozo por un `UDynamicMeshComponent` de rejilla
     regular: 33×33 vértices a 0,25 m, con Z = `Height(Columna)`. Normales por
     diferencias centrales: por eso un chunk lee una columna de margen.
   - Para el cambio de arena seca a mojada no hace falta nada nuevo: el material ya
     oscurece por altura sobre el agua.
   - Con el mismo desplazamiento en un chunk que ya tenga edición volumétrica, la
     densidad combinada es `Base + DeltaVolumétrico − DeltaArena(X, Y)`. El campo es
     aproximadamente una distancia, así que restar la altura sube la superficie. Se
     puede meter en `BuildChunkGrid` si alguna vez se cruzan, pero en una playa no
     debería pasar.
   - Los `DirtyChunks` se remallan al llegar (una vez por segundo como mucho en una
     avalancha, más el medio ciclo de marea). El cliente puede interpolar la altura
     de cada vértice durante ese segundo para que la arena se vea escurrir, no saltar.
     Una rejilla de 33×33 son unos 2 000 triángulos: se vuelca en el hilo de juego sin
     tarea de fondo.
   - Colisión: solo hace falta un heightfield simple por chunk. Se actualiza cada vez
     que el chunk se asienta, no en cada paso.
4. **Estructuras.** Al colocar una pieza con encaje a terreno (`tablon_contencion`,
   `pilote_bambu`, `pilote_madera`, muelle, saco), `UBuildingSubsystem` llama a
   `SetAnchor(Min, Max, true, Base)` con su huella en planta (sin sumar el metro: lo
   añade el modelo). Al destruirla, con `false`. Al cargar la partida, cada pieza vuelve
   a anclar su huella antes del primer `Advance` y del primer `ApplyHalfTide`. El
   `tablon_contencion` es una pieza nueva (biblia 02 §5.3) que todavía no está en los
   datos de construcción.
5. **Persistencia en `WorldDeltas`.** Una capa opaca `"sand"` en `FSaveWorldDeltas`,
   junto a `"terrain"`: el mismo patrón, un `FSaveValue` guardado con `ToValue()`. Si
   `FromValue` falla, la playa se queda sin cambios y se avisa en el registro, pero la
   carga no se aborta. Las columnas sucias se guardan, así que un montón que se estaba
   derrumbando sigue haciéndolo al cargar. El banco del mar se guarda en `"sea"`, para
   que la cuenta de masa siga cuadrando. Tamaño: unos 5 caracteres por columna
   editada, unos 3 KB por un hoyo de 1 m².

## Coste por fotograma

- Solo se revisan las columnas **sucias** de los chunks cuyo borde está a 80 m o menos
  de algún jugador (`DistanceToChunk`). El spec `Explored.Sand` lo demuestra con dos
  montones: uno en el chunk que empieza a 80 m justos, que se asienta, y otro en el
  que empieza a 88 m, que no cambia y sigue sucio hasta que el jugador llega. Con todo
  asentado, una revisión toca **0** columnas.
- Una revisión por segundo con hasta 4 pasadas. Medido en el host (`-O2`, shim de
  Core), con todos los montones a menos de 80 m y cada uno de 3 m³, que es mucho más
  que lo que mueve la pala (0,06 m³ por pasada):

  | Caso | Columnas activas (máx.) | Cambiadas por revisión (máx.) | ms por revisión (medio / máx.) | Hasta asentarse | Medio ciclo de marea |
  |---|---|---|---|---|---|
  | 3 montones, 9 m³ | 399 | 192 (3 chunks × 64) | 1,1 / 1,7 | 35 s | 0,06 ms |
  | 12 montones, 36 m³ | 1 555 | 831 | 4,4 / 7,0 | 35 s | 0,3 ms |
  | 40 montones, 120 m³ | 5 273 | 3 282 | 15 / 31 | 35 s | 1,0 ms |

  El coste llega **todo junto una vez por segundo**, no repartido. El primer caso cabe
  en el hilo de juego. Para los otros, dos salidas, y hay que medir en PIE cuál hace
  falta:
  - **Tarea de fondo.** El modelo no toca `UObject` y `FTerrainDensity` es seguro entre
    hilos. Se lanza `Tick` en una tarea (`UE::Tasks`) sobre el `FSandModel` del
    servidor y se recogen los `DirtyChunks` en el fotograma siguiente. Mientras tanto,
    la pala no puede tocar el modelo: se encolan sus ediciones.
  - **Escalonar chunks.** Revisar cada chunk en su propia fase del segundo (por
    ejemplo, `hash(chunk) % 10` décimas). Pide partir `Tick` por chunks y vigilar las
    parejas que cruzan un borde. Hoy no está hecho.
- `TMap` de Unreal debería ir igual o mejor que el del shim, pero **hay que medirlo en
  PIE**.
- El tope de 64 columnas por chunk es de red, pero también limita la CPU: una avalancha
  enorme en un chunk no puede tocar más de 64 columnas por segundo.
- Lo que salió al medir la primera versión: quitar las columnas dormidas del `TMap` una
  a una costaba 26 ms por paso con 1 300 activas en el host, porque el `Remove` del
  shim es lineal. El conjunto se rehace entero en cada revisión y así no depende de ello.
- La pleamar y la lluvia despiertan la arena editada de los chunks activos cuando la
  pleamar cambia 5 cm o cuando empieza o deja de llover. Solo se recorren los chunks que
  tienen deltas. Los chunks con deltas que en ese momento están lejos de todos los
  jugadores quedan pendientes y se despiertan cuando alguien se acerca: una playa que se
  ha secado sin nadie cerca se derrumba a 34° al volver, no se queda a 45°.
- Memoria: 4 arrays de 1 024 elementos por chunk tocado (delta y base en `int32`,
  huella y sujeción en `uint8`), unos 10 KB. Un chunk se crea aunque solo se lea como
  vecino o quede dentro del metro de una estructura. Si en una partida larga pasa de unos
  cientos, conviene soltar los chunks sin deltas ni anclajes al alejarse.

## Riesgos a verificar en PIE

- **Juntura con el terreno volumétrico:** el `UDynamicMeshComponent` de 8 m tiene que
  casar con el borde del chunk horneado de 64 m. Se tapa con un faldón de una celda,
  igual que en `docs/tecnico/terreno-editable.md`.
- **Arena que se mueve bajo los pies:** si una avalancha baja la columna del jugador
  más de 10 cm en una revisión, hay que dejar que el `CharacterMovement` caiga en vez
  de teletransportarlo.
- **Medio ciclo de marea delante del jugador:** el relleno llega de golpe cada 10
  minutos reales. Si alguien está mirando el hoyo, conviene fundir la altura nueva en
  unos segundos en el cliente, al ritmo de las olas que suben.
- **Arena congelada:** una playa a más de 80 m no se revisa, pero cada chunk cuenta las
  revisiones que se salta (`FrozenRevisions`, hasta 4). Al volver alguien, `Tick` las
  recupera de golpe solo en esos chunks (`CatchUpRevisions`) y después la arena sigue a
  su ritmo (1 por segundo). Esa ráfaga puede sacar hasta 5 × 64 columnas de un chunk en
  un segundo; entra en la ráfaga de 16 KB/s de 08 §2.2. El contador no se guarda: al
  cargar, la arena empieza sin deuda. Hay que comprobar en PIE que el arranque no se
  note.
- **Tope del montón:** la avalancha no deja que una columna pase de 2 m sobre su suelo,
  ni siquiera al pie de un escalón. La arena que no cabe se queda arriba, así que el
  guardado nunca contiene un delta que `FromValue` rechace.
