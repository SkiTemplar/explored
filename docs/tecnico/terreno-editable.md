# Terreno editable: cómo enganchar `FTerrainEditModel` en el motor

Estado: el modelo es puro y tiene spec en el host (`Explored.TerrainEdit`). Falta la
integración con Unreal, que tiene que hacer una sesión con el editor. Diseño de juego:
GDD v2 §3.4; pipeline: GDD v2 §7.3.

## Qué hay

- `Source/Explored/WorldGen/TerrainEditModel.h`: deltas dispersos de densidad sobre
  `FTerrainDensity`. La rejilla de edición es de **0,25 m** y cada chunk de edición
  tiene 32 celdas, así que mide **8 m** y cabe 8×8×8 veces en un chunk horneado de 64 m.
- Herramientas: `Pickaxe`, `Shovel`, `PlaceSoil` y `CarveStairs` (con `SnapStairs`).
  Todas reciben el campo base como `TFunctionRef<float(const FVector&)>`: en el juego,
  `[&](const FVector& P) { return Density.Density(P); }`. Las unidades son metros.
- Cada edición devuelve `FTerrainEditResult`: `DirtyChunks` (coordenadas de chunk de
  edición, ordenadas y sin repetir), `VolumeRemoved` y `VolumeAdded` en m³ para el
  inventario de tierra y roca, y `bRejected` cuando la herramienta no llega al material.
- `BuildChunkGrid(Chunk, Base, Grid)` rellena un `FDensityGrid` de N + 2 muestras por eje
  con el mismo convenio que `FTerrainChunkBuilder`. Esa rejilla se pasa tal cual a
  `FSurfaceNets::Polygonize`.
- Guardado: `ToValue()` y `FromValue()` se guardan en `FSaveWorldDeltas::Terrain`, que
  en la partida es la clave `"terrain"` de la sección `world`.

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
- **Tamaño del guardado:** una galería de 100 m³ son unas 6 400 muestras. Cada una ocupa
  unos 5 caracteres en la partida (tramos de índices consecutivos), así que salen
  unos 35 KB. Falta la prueba de estrés de M3 (GDD v2 §7.4).
