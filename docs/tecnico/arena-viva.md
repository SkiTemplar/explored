# Arena viva: cómo enganchar `FSandModel` en el motor

Estado: el modelo es puro y tiene spec en el host (`Explored.Sand`, 30 casos). Falta la
integración con Unreal, que tiene que hacer una sesión con el editor. Diseño de juego:
GDD v2 §3.13.

## Qué hay

- `Source/Explored/WorldGen/SandModel.h`: un campo de alturas de deltas, en milímetros
  enteros, sobre el suelo de la isla. La rejilla es de **0,25 m** y cada chunk tiene
  32 columnas, así que mide **8 m**: es la misma que la de `FTerrainEditModel`, y un
  chunk de arena cubre el mismo suelo que un chunk de edición.
- Herramientas: `Dig` y `Pile` (pincel en cono). `SetAnchor` sirve para las estructuras
  y cuenta referencias.
- Simulación: `Advance(DeltaMs, Env, Base)` avanza en pasos fijos de 100 ms, con un
  máximo de 20 por llamada. `FSandEnvironment` lleva el nivel del agua, la lluvia y los
  focos (jugadores) con su radio activo.
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
  - `ActiveColumns` y `DormantColumns`, para medir el coste.
- Guardado: `ToValue()` y `FromValue()`. Los anclajes **no** se guardan, porque los
  vuelve a poner el sistema de construcción al cargar cada pieza.

## Enganche propuesto

1. **Subsistema de mundo `USandSubsystem`.** Es dueño de un `FSandModel` y, en su
   `Tick`, llama a `Advance(DeltaMs, Env, Base)` en el hilo de juego. El entorno se
   rellena así:
   - `SeaLevel`: nivel medio del mar más `FOceanTide::Level(TotalDays)` ×
     `SpringNeapFactorAt(TotalDays)` × la amplitud de marea en metros. Hoy ningún
     sistema aplica una amplitud en metros; propuesta: 0,8 m. Las olas no se suman,
     porque el modelo ya las promedia.
   - `bRaining`: lo da `UWeatherSubsystem` cuando la intensidad de lluvia pasa de 0,3.
   - `Focus` y `ExtraFoci`: la posición de cada jugador. En cooperativo solo simula el
     servidor, con todos los jugadores como focos. Una columna cerca de dos jugadores se
     simula una sola vez.
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
   - Los `DirtyChunks` de cada `Advance` se remallan como mucho cada 0,1 s por chunk.
     Una rejilla de 33×33 son unos 2 000 triángulos: se vuelca en el hilo de juego sin
     tarea de fondo.
   - Colisión: solo hace falta un heightfield simple por chunk. Se actualiza cada vez
     que el chunk se asienta, no en cada paso.
4. **Estructuras.** Al colocar una pieza con encaje a terreno (tablón, pilote, muelle,
   saco), `UBuildingSubsystem` llama a `SetAnchor(Min, Max, true, Base)` con su huella
   en planta. Al destruirla, con `false`. Al cargar la partida, cada pieza vuelve a
   anclar su huella antes del primer `Advance`.
5. **Persistencia en `WorldDeltas`.** Una capa opaca `"sand"` en `FSaveWorldDeltas`,
   junto a `"terrain"`: el mismo patrón, un `FSaveValue` guardado con `ToValue()`. Si
   `FromValue` falla, la playa se queda sin cambios y se avisa en el registro, pero la
   carga no se aborta. Las columnas sucias se guardan, así que un montón que se estaba
   derrumbando sigue haciéndolo al cargar. Tamaño: unos 5 caracteres por columna
   editada, unos 3 KB por un hoyo de 1 m².

## Coste por fotograma

- Solo se simulan las columnas **sucias** a menos de `ActiveRadius` (24 m) de algún
  foco. El spec `Explored.Sand` lo demuestra: con un montón a 100 m del foco, ese montón
  no cambia y sigue sucio, y en cuanto el foco llega se asienta. Con todo asentado, un
  paso simula **0** columnas.
- Cada columna activa cuesta 4 parejas: unas pocas búsquedas en mapas y sumas enteras.
  Medido en el host (`-O2`, shim de Core), con un paso de 100 ms:

  | Caso | Columnas activas (máx.) | ms por paso (medio / máx.) | Hasta asentarse |
  |---|---|---|---|
  | 3 montones, 9 m³ | 357 | 0,18 / 0,50 | 14 s |
  | 12 montones, 36 m³ | 1 317 | 0,53 / 1,63 | 15 s |

  Una pasada de pala mueve menos de 0,1 m³, así que lo normal es el primer caso o menos.
  `TMap` de Unreal debería ir igual o mejor que el del shim, pero **hay que medirlo en
  PIE**.
- Tope duro: `MaxActiveColumnsPerTick` = 4 096 columnas por paso. Si hay más, se simulan
  primero las más cercanas a un jugador y el resto espera al paso siguiente. Con el
  coste medido son unos 5 ms en el peor caso.
- Lo que salió al medir: quitar las columnas dormidas del `TMap` una a una costaba
  26 ms por paso con 1 300 activas en el host, porque el `Remove` del shim es lineal.
  En Unreal `Remove` es barato, pero el conjunto se rehace entero cada paso y así no
  depende de ello.
- La marea y la lluvia despiertan la arena editada cerca de los focos cuando el agua
  sube o baja 5 cm, o cuando empieza o deja de llover. Solo se recorren los chunks que
  tienen deltas.
- Memoria: 3 arrays de 1024 elementos por chunk tocado (delta, base y anclaje), unos
  9 KB. Un chunk se crea aunque solo se lea como vecino. Si en una partida larga pasa
  de unos cientos, conviene soltar los chunks sin deltas ni anclajes al alejarse.

## Riesgos a verificar en PIE

- **Juntura con el terreno volumétrico:** el `UDynamicMeshComponent` de 8 m tiene que
  casar con el borde del chunk horneado de 64 m. Se tapa con un faldón de una celda,
  igual que en `docs/tecnico/terreno-editable.md`.
- **Arena que se mueve bajo los pies:** si una avalancha baja la columna del jugador
  más de 10 cm en un paso, hay que dejar que el `CharacterMovement` caiga en vez de
  teletransportarlo.
- **La marea sin nadie cerca:** la arena de una playa sin jugadores no se entera de
  que ha subido la marea hasta que alguien llega. Es aceptable, porque nadie lo ve
  pasar, pero hay que comprobar que, al llegar, no se vea un «salto» brusco. Si se
  viera, bastaría con avanzar el tiempo perdido en unos pocos pasos al despertar.
