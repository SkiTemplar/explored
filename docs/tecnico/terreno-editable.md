# Terreno editable: cómo enganchar `FTerrainEditModel` en el motor

Estado: el pipeline puro está hecho y tiene specs en el host (`Explored.TerrainEdit` y
`Explored.TerrainEdits`). Falta la integración con Unreal, que tiene que hacer una
sesión con el editor. Diseño de juego: GDD v2 §3.4 y biblia 02 §2; pipeline: GDD v2 §7.3.

## Qué hay

- `Source/Explored/WorldGen/TerrainEditModel.h`: deltas dispersos de densidad sobre
  `FTerrainDensity`. La rejilla de edición es de **0,25 m** y cada chunk de edición
  tiene 32 celdas, así que mide **8 m** y cabe 8×8×8 veces en un chunk horneado de 64 m.
- Herramientas: `Pickaxe`, `Shovel`, `PlaceSoil` y `CarveStairs` (con `SnapStairs`).
  Todas reciben el campo base como `TFunctionRef<float(const FVector&)>`: en el juego,
  `[&](const FVector& P) { return Density.ProceduralDensity(P); }` (nunca `Density`,
  que ya suma las ediciones). Las unidades son metros.
- Cada edición devuelve `FTerrainEditResult`: `DirtyChunks` (coordenadas de chunk de
  edición, ordenadas y sin repetir), `VolumeRemoved` y `VolumeAdded` en m³ para el
  inventario de tierra y roca, y `bRejected` cuando la herramienta no llega al material
  o la petición no es válida.
- **Entradas validadas.** Cada herramienta rechaza entera (`bRejected`, sin chunks
  sucios) una petición con valores no finitos, con coordenadas fuera de
  ±`MaxWorldCoordinate` (100 km) o con un pincel de pala o de tierra de más de
  `MaxBrushExtent` (4 m). `CarveStairs` rechaza lo que pase de los topes de
  `SnapStairs` (64 peldaños, contrahuella 0,45 m, huella 0,9 m, ancho 3 m, altura libre
  3 m), y `SnapStairs` devuelve `false` con valores no finitos. Antes, un solo golpe
  con el punto de impacto a NaN escribía deltas en muestras sin sentido y el guardado
  entero del terreno dejaba de cargar, y una pala de 200 m de radio tardaba ~30 s.
  Por eso el servidor del cooperativo puede pasar al modelo las peticiones de los
  clientes tal como llegan: el rechazo se responde con el sonido de rebote.
- `BuildChunkGrid(Chunk, Base, Grid)` rellena un `FDensityGrid` de N + 2 muestras por eje
  con el mismo convenio que `FTerrainChunkBuilder`. Esa rejilla se pasa tal cual a
  `FSurfaceNets::Polygonize`.
- Guardado: `ToValue()` y `FromValue()` se guardan en `FSaveWorldDeltas::Terrain`, que
  en la partida es la clave `"terrain"` de la sección `world`. `ToValue` escribe la
  versión 2 (binaria en base64, formato en `TerrainEditModel.h`); `FromValue` lee la 1 y
  la 2 y, ante cualquier entrada truncada o manipulada, deja el modelo vacío.

## `FTerrainEdits`: la capa que consulta el mundo

`Source/Explored/WorldGen/TerrainEdits.h` envuelve el modelo con las reglas de minería
de H0 (biblia 02 §2):

- **Densidad del mundo.** `FTerrainDensity::SetEdits(TSharedPtr<const FTerrainEdits>)`
  engancha la capa. Desde ahí, `Density`, `DensityWithColumn` (y con ellas `Normal` y
  `FTerrainChunkBuilder::Build`) miran primero la capa: lejos de una edición cuesta una
  búsqueda en un mapa y nada más. `ProceduralDensity` sigue dando el campo sin editar.
  La capa se edita en el hilo de juego; quien remalle en otro hilo trabaja sobre una
  copia (`FTerrainEdits` es copiable) o espera a que no haya golpes en curso.
- **Picado por esfera.** `Dig(FTerrainDigHit, Density)` vacía una esfera centrada en el
  punto de impacto con el radio de la herramienta (`ToolInfo`: pala tosca 0,35 m y
  1,2 s; pico de piedra 0,40 m y 1,3 s; tallado 0,42 m y 1,2 s; obsidiana 0,50 m y
  1,0 s; rescatado 0,55 m y 1,1 s; espejo de `mining.json/tools`, lo compara DataCheck).
  Cada golpe arranca como mucho 1 / golpes por m³ del material con esa herramienta. Con
  herramienta insuficiente rebota: no toca nada, pero el golpe dura lo mismo.
  `TakeLootUnits` convierte el volumen en unidades de `LootItemId` (`tierra_suelta`,
  `arena`, `caliza`, `basalto`, `obsidiana`) a 6 por m³.
- **Invalidación.** `Dig` devuelve `Edit.DirtyChunks` (chunks de edición de 8 m) y
  `RenderChunks` (chunks de `FTerrainChunkBuilder` de 64 m). `ChunksToInvalidate` da lo
  mismo antes del golpe, como cota superior, para reservar o bloquear chunks.
  `EditedRenderChunks` lista los chunks de render con ediciones: hay que cargarlos
  aunque `FindCandidateChunks` no los proponga (una mina por debajo del margen de
  alturas).
- **Red.** El servidor comprueba la cadencia con `IsCadenceValid` (85 % de la duración
  del golpe), cava y replica las muestras de `DirtyChunks`. `ChunkChecksum` es el FNV-1a
  de la comprobación de cada 30 s (biblia 08 §2.2).
- **Guardado.** `SaveTo(FSaveWorldDeltas&)` y `LoadFrom(const FSaveWorldDeltas&)`: sin
  ediciones no se escribe la capa; una capa ilegible deja el mundo sin cavar y devuelve
  false para avisar en el registro.

## Integración en el juego (hecha: pico, pala, remallado, red y guardado)

Código de motor en `Source/Explored/Mining/`; la lógica nueva está en modelos puros de
`WorldGen/` con sus specs en el host.

- **`UTerrainEditSubsystem`** (subsistema de mundo, partida y PIE). Dueño de la capa
  `FTerrainEdits` de esta máquina y de una `FTerrainDensity` sin ediciones enganchadas (las
  tareas de fondo solo leen `ProceduralDensity`, que es inmutable). API en metros:
  `Dig`, `Shovel`, `PlaceSoil` (solo con autoridad), `ApplyNetworkPacket` (solo cliente),
  `Density`, `MaterialAt`, `SaveTo`/`LoadFrom`, `FlushRemeshing`.
- **Herramientas (`UTerrainToolComponent`, en el personaje).** Clic principal con el pico en
  la mano: picado por esfera (`FTerrainEdits::Dig`). Pala: el principal aplana hacia el
  plano de los pies (`FTerrainEditModel::Shovel`) y el secundario echa la tierra que se
  lleva (`PlaceSoil`). Traza de 3 m desde la cámara contra actores con la etiqueta
  `ExploredTerrain`. Reglas puras en `FTerrainToolModel`: objeto → herramienta, botón →
  acción, estrato por isla y profundidad (tabla de `mining.json/strata` medida desde la
  superficie sin editar), validación del servidor y tierra transportada (m³, tope 2 m³).
- **Remallado (`UTerrainRuntimeMesher`).** La primera edición en un chunk horneado de 64 m
  lanza un sondeo en una tarea (`FTerrainRemeshModel::SurfaceEditChunks`, retícula de
  2 m) que dice qué chunks de 8 m pueden tener superficie; se mallan todos a 0,25 m en
  tareas (`UE::Tasks`) y, cuando están todos, el horneado se oculta de golpe (antes, la
  malla fina crece debajo sin verse). Su colisión se apaga cuando la fina ya está cocinada
  (o a los 2 s). Cada chunk sucio se remalla en una tarea con el campo base cacheado (LRU
  de 96 chunks editados) más los deltas que copia el hilo de juego
  (`FTerrainRemeshModel::GatherDeltas`); normales del gradiente de la rejilla, color y
  capas de `FTerrainDensity`, y la `FProcMeshSection` ya montada. El hilo de juego solo
  vuelca secciones en `UProceduralMeshComponent` (colisión compleja, cocinado asíncrono)
  con un presupuesto de `explored.Terrain.RemeshBudgetMs` (1,0 ms): el primer volcado del
  fotograma siempre entra y los siguientes solo si caben según una estimación del coste por
  chunk (sube al momento con un pico, baja despacio). Cola pura en
  `FTerrainRemeshQueueModel` (un chunk no se relanza hasta que vuelve su tarea ni antes de
  0,15 s; prioridad por distancia) y estado de sustitución en `FTerrainReplacementModel`.
  Se usa `UProceduralMeshComponent` y no `UDynamicMeshComponent`: ya es dependencia del
  módulo, admite los cuatro canales de UV y el color de vértice que lee `M_Terrain` y
  cocina la colisión fuera del hilo de juego.
- **Material de las mallas finas.** Ajuste de proyecto `UExploredTerrainSettings`
  (Project Settings → Game → Explored Terreno, `RuntimeTerrainMaterial` en
  `DefaultGame.ini`, por defecto `M_Terrain`). El subsistema lo pide por referencia blanda
  con carga asíncrona (`FStreamableManager`) y lo pasa con `UTerrainRuntimeMesher::SetMaterial`,
  que también lo aplica a las mallas ya creadas. Si el ajuste está vacío, el remallador usa
  el material del primer chunk horneado que se registra. Nada se carga por ruta en
  `Initialize`.
- **Red (autoridad del anfitrión).** El cliente predice solo el sonido y las partículas
  (`OnToolCue`) y pide el uso con `ServerUseTool`, que solo lleva el botón (principal o
  secundario) y el punto. El servidor saca la herramienta de las manos del personaje
  (`UCarryComponent`), deduce la acción con `FTerrainToolModel::ActionFor` y valida primero
  lo barato (`ValidateUse`: acción de la herramienta, alcance de 3 m + 1,5 m y cadencia del
  85 % de la duración) y solo entonces la superficie (|densidad| ≤ 0,75 m); después edita.
  Mientras el inventario no se replique, el servidor solo ve las manos de sus propios
  personajes: un cliente remoto no puede cavar hasta que `UCarryComponent` tenga autoridad
  en el servidor. Cada edición
  sale por `UTerrainEditSubsystem::OnPatches` con el valor final de las muestras cambiadas
  (`FTerrainEditResult::ChangedSamples` → `FTerrainNetSyncModel::PatchesForSamples`) y
  `UTerrainSyncComponent` (en el PlayerController) lo encola por cliente en
  `FTerrainDeltaQueueModel` y lo manda por RPC fiable. Un cliente que entra tarde recibe el
  estado completo. El cliente aplica con `FTerrainNetSyncModel::ApplyPacket` y remalla.
- **Guardado.** `UExploredWiringSubsystem::SaveWorld/LoadWorld` pasan la capa «terrain» por
  `UTerrainEditSubsystem`; al cargar se vuelven a sustituir los chunks editados.
- **Test del editor:** `Explored.TerrainRuntime.EditRemeshCollisionSave` (mundo de juego
  vacío): sustituye un chunk de Landing con el presupuesto del juego y cocinado asíncrono,
  cava 16 golpes, comprueba malla, colisión (traza y esfera de 25 cm dentro del hueco), coste
  por fotograma (< 2 ms en el hilo de juego), guardado → mundo sin cavar → carga con el mismo
  hueco, tierra echada que sube el fondo, pasadas de pala que devuelven el suelo al plano y
  el material del ajuste de proyecto en las mallas ya creadas.
- **Coste medido** (editor sin RHI, Development, 2026-09-29): sustituir un chunk de 64 m son
  146 chunks de 8 m en ~50 fotogramas (0,25 s), con 0,72–0,82 ms máx por fotograma en el hilo
  de juego y 0,38–0,41 ms máx por volcado; cada tarea de fondo tarda 14–15 ms de media (máx
  ~32 ms). Un golpe ya sustituido remalla 1 chunk: 0,2–0,3 ms máx por fotograma. Ningún
  fotograma llegó a 2 ms. Con el presupuesto anterior (1,5 ms) y la máquina cargada se vieron
  picos de 2–3 ms.

Borde con los chunks horneados vecinos (faldón): Surface Nets reparte las caras de modo que
la malla de un chunk entra una celda en el chunk de abajo de cada eje y acaba en su propia
cara alta. En la cara alta del chunk sustituido, la malla horneada del vecino ya se mete
una celda de 2 m y solapa con la fina. En la cara baja, el vecino horneado acaba dentro de
su última celda y la malla fina empieza a 0,25 m del borde: entre las dos quedaría una
rendija de hasta 2 m por la que se ve el vacío. Por eso los chunks de edición de la cara
baja de un chunk de render amplían su rejilla 8 muestras (una celda horneada) hacia fuera
(`FTerrainRemeshModel::ChunkWindow` con faldón): solapan con el vecino horneado y, si el
vecino también está sustituido, sus triángulos coinciden exactamente con los del vecino.
Donde las dos aproximaciones difieren puede verse un escalón de centímetros. Una edición a
menos de 3 m del borde sustituye también al vecino.

Pendiente de esta integración: comprobar en PIE ese escalón, capa de camino en el color del vértice,
réplica de la compactación de caminos, botín en el inventario (hoy la tierra se cuenta en
m³ en el componente), desgaste de la herramienta, comprobar en el servidor el objeto de
la mano (el inventario aún no se replica), deltas de más de ±32 m (minas muy profundas: el
códec de red usa `int16`), campos de distancia de Lumen para las mallas finas y navegación
de la fauna.

## Enganche propuesto (diseño original)

1. **Subsistema de mundo `UTerrainEditSubsystem`.** Es dueño de un `FTerrainEditModel` y
   de una referencia al `FTerrainDensity` del generador. Al golpear, el componente de
   herramienta del jugador traza contra el terreno, elige el material con
   `SurfaceLayers` y la profundidad, y llama a `Pickaxe`, `Shovel`, etc. en el hilo de
   juego. El modelo no es seguro entre hilos para escribir, pero sí para leer.
2. **Remallado en runtime con `UDynamicMeshComponent`.**
   - La primera vez que un chunk horneado de 64 m recibe una edición se oculta su malla
     Nanite y su colisión. En su lugar se crea un actor con un `UDynamicMeshComponent`
     por cada chunk de edición de 8 m que tenga superficie. Los chunks de 8 m que no
     tienen edición también se mallan desde el campo base, a 0,25 m, para no dejar
     grietas dentro del chunk sustituido.
   - Cada golpe de escalera usa `MaxVolume = 1 / DesignHitsPerCubicMeter(material, nivel)`.
   - Cada `DirtyChunks` se encola. Una tarea de fondo (`UE::Tasks`) hace
     `BuildChunkGrid` + `Polygonize`, con normales y colores de `FTerrainDensity`,
     igual que en `FTerrainChunkBuilder::Build`. El hilo de juego vuelca el resultado
     en `FDynamicMesh3` con `EditMesh` y actualiza la colisión compleja (Chaos, trimesh).
   - Conviene agrupar los golpes: remallar como mucho cada 0,15 s por chunk. Mientras
     tanto se muestran partículas y el sonido del golpe, porque el terreno ya ha cambiado
     en el modelo.
   - Borde con los chunks horneados vecinos (a 2 m): hay que añadir un faldón de una
     celda hacia dentro del sólido en las caras exteriores del chunk de 64 m sustituido.
     Así se tapan las grietas de resolución sin tocar los vecinos.
3. **Capa de camino.** `Compaction(X, Y)` (0–100) se escribe en el canal de capas del
   vértice (`FTerrainMeshData::Layers`) como hierba = 0 y suelo = 1, escalado. El material
   del terreno ya mezcla por capas. Marcar un camino devuelve chunks sucios aunque la
   forma no cambie.
4. **Carga de partida.** Se hace `FromValue` y, para cada chunk de `EditedChunks()`,
   se sustituye su chunk horneado antes de mostrar el mundo. Si `FromValue` falla
   (otra rejilla o datos manipulados), el terreno se queda sin ediciones y se avisa en
   el registro. Nunca se aborta la carga.

## Riesgos a verificar en PIE

- **Coste del remallado:** un chunk de 8 m a 0,25 m son unas 34³ ≈ 39 000 evaluaciones
  de densidad, y el ruido de `FTerrainDensity` no es barato. Si pasa de ~5 ms por
  chunk en una tarea, hay que cachear la densidad base del chunk mientras siga sucio.
- **Escaleras:** una contrahuella de 30 cm ocupa 1,2 celdas de 0,25 m. Surface Nets
  suaviza las aristas y los peldaños se verán redondeados. Si en PIE no se leen como
  escalera, se puede bajar la rejilla a 0,125 m solo en los chunks con escalera o pasar
  a dual contouring con normales. El modelo ya admite otra `FTerrainEditSettings`.
- **Tamaño del guardado:** medido en la prueba de estrés del host (`Explored.TerrainEdits
  la prueba de estrés`): 50 000 golpes, unos 4 000 m³ de galerías y 400 000 muestras
  editadas en 416 chunks ocupan **≈ 580 KB** en la versión 2 (1,45 B por muestra), frente
  a 3 MB en la versión 1 con sangría. Cavar cuesta ≈ 6 µs por golpe; escribir y leer la
  partida entera, menos de 0,1 s (en la máquina de la nube, sin sanitizers).
