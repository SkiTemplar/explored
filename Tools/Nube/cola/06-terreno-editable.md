TAREA: construye el pipeline puro de terreno editable de H0. Referencia: 00-TODO H0, apartado «Minería y terreno». La PR #40 ya trajo el modelo del pico y la pala: parte de él.

Requisitos:
- `FTerrainEdits`: capa dispersa de ediciones por chunk encima de la densidad procedural. `FTerrainDensity::Density` la consulta antes que nada.
- Picado por esfera, con el radio y el tiempo por golpe según material y herramienta (biblia 02).
- Cálculo de los chunks a invalidar, incluida la esfera que cruza bordes y esquinas de chunk.
- Capa `"terrain"` en `FSaveWorldDeltas`:
  - serialización versionada;
  - el round-trip devuelve exactamente lo que entró;
  - una entrada truncada no provoca crash.
- Item `tierra_suelta`, paralelo a `arena`.
- Prueba de estrés con 50.000 ediciones que mida en el propio test el tamaño del guardado y el tiempo.

Si tocas `TerrainChunkBuilder` u otro código de motor, ponle a la PR la etiqueta `necesita-unreal`.
