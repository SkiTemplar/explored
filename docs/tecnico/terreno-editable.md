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
  o la petición no es válida. Los volúmenes son los realizados tras redondear cada
  muestra al milímetro hacia su densidad de partida: se pueden restar y sumar al
  inventario tal cual, sin que el jugador acabe con tierra negativa. El test de host
  `Tools/HostTests/tests/TerrainEditPropertyTest.cpp` lo comprueba con ediciones
  aleatorias junto con el alcance del pincel, los chunks sucios, el guardado y la
  repetición.
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

## Enganche propuesto

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
