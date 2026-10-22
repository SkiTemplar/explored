# Tools/Island — terreno de la Isla de Sal (GDD v3 §6, F3a)

Genera el terreno como **malla estática low poly** (relieve con detalle y lectura facetada moderada) (decisión de F3a que
sustituye al `Landscape` del GDD §6) a partir de `Content/Data/island.json`, y
valida `Content/Data/island_props.json`. Blender 5.2 como módulo de Python
(`bpy==5.2.2`, Python 3.13); reutiliza `Tools/Style` (`meshkit`, `palette`) sin
modificarlo.

```bash
uv sync
uv run island-terrain          # Art/Island/SM_IslaSal_Terrain.fbx + .json (cifras) + Content/Data/ground_field.bin
uv run island-props-check      # island_props.json frente al GDD (§5.3, §6, §11.1)
uv run island-decor            # Content/Data/island_decor.json: 1500-2500 instancias decorativas K1 + K6 (HISM, sin colisión)
uv run island-groundfield      # Content/Data/ground_field.bin: la malla exacta y el tipo de suelo (césped del juego)
uv run island-preview          # docs/v3/laminas/relieve-antes-despues.png (Blender, sin Unreal)
uv run ruff check . && uv run basedpyright && uv run pytest -q --cov
```

Después, en Unreal (PowerShell, raíz del repo):

```powershell
Tools\unreal_python.ps1 -Script Tools/Unreal/build_materials.py    # T_Paleta + M_Toon
Tools\unreal_python.ps1 -Script Tools/Unreal/materials_f6.py      # M_TerrainToon + T_TerrainStrokes (antes: cd Tools/Textures; uv run textures-strokes)
Tools\unreal_python.ps1 -Script Tools/Unreal/import_island.py      # /Game/Art/Island/SM_IslaSal_Terrain
Tools\unreal_python.ps1 -Script Tools/Unreal/create_map_islasal.py # Map_IslaSal
```

## Relieve

`explored_island.layout.height_at` es una copia exacta de
`FIslandLayout::SampleColumn` (C++): elipses con costa armónica (`coast`),
meseta con plataforma, colinas gaussianas de zona (`hill`, `hillSigma`) que se
desvanecen hacia la costa entre las distancias normalizadas 0,3 y 1, y rocas u
hondonadas (`rocks`, altura negativa = cala). Ese relieve analítico, filtrado
con una caja de 5 × 5 muestras (`terrain.smoothed_height`), es el **relieve
base**: el que usan el juego (audio, cámaras, fauna) y el terreno de 8d3fa23b.
El test `IslandDataSpec` comprueba que `island.json` y
`FIslandLayout::Provisional()` dan la misma altura.

Sobre el relieve base, `relief.py` suma detalle de media escala, determinista
(pasada «relieve y textura» de F6):

- ondulaciones y lomas (ruido de gradiente, λ 6-25 m) con amplitud por zona
  (0,6 m en las calas a 1,2 m en la colina; desviación típica en tierra 0,47 m);
- 13 surcos de escorrentía radiales que bajan de la Colina del Faro (0,8 m);
- rellanos rocosos (escalonado parcial de 1,3 m) en manchas de las laderas de
  más de ~14-24°;
- dunas alargadas (±0,45 m) y una berma paralela a la orilla en la playa;
- caminos hundidos 0,15 m con reborde, nivelados sin pendiente transversal a
  la altura de su eje (`paths.py`: perfil suavizado y con pendiente
  longitudinal ≤ 22°).

El detalle es nulo bajo la arena (la costa se mueve 0 m; test ≤ 1 m), alrededor
de `player_start`, `raft_site`, `lighthouse` y de las estructuras de
`island_props.json` (misma altura que el relieve base ± 0,3 m) y cerca del eje
de los caminos. La malla añade además el fondo marino recortado a
`terrain.seabed` (−6 m), la falda exterior a `terrain.skirt` (−30 m) y un
ruido de facetas de ±0,22 m por vértice en tierra firme (±0,2 m en las rocas).

## Malla

- Rejilla de `terrain.cell` = 1,8 m con los vértices desplazados ±12 % de
  celda y la diagonal de cada celda elegida por hash: 133 770 triángulos.
- Lectura low poly moderada: la normal de cada esquina mezcla la normal suave
  del vértice (90 %) y la de la cara (10 %, `terrain.FACET_MIX`; 5 % en las
  pendientes fuertes, `STEEP_FACET_MIX`, y 6 % en la
  arena y en cuatro celdas a su alrededor, `SAND_FACET_MIX`, donde con el sol
  bajo la faceta salía en triángulos oscuros); se exportan como normales
  personalizadas y `import_island.py` las importa tal cual.
- Color por vértice («Col», `colors.py`): tres tonos de la paleta por zona con
  variación de alta frecuencia (ruido de 3 y 6,5 m; racha media ≤ 4 celdas),
  fundidos con un núcleo 1-4-1 (transiciones de una o dos celdas). Arena
  clara con vetas oscuras en los valles de las dunas (la franja mojada de la
  orilla la pinta `M_TerrainToon` con `WetTopM`, no el color por vértice), hierba por zona (más oscura en el Palmar y el Bambú, seca en la
  colina, más verde en los surcos), roca en tres tonos en pendientes > 35° y
  en la huella de las rocas, tierra en los caminos (borde ondulado y reborde
  de hierba pisada) y en el fondo de los surcos, y hierba seca en una mancha
  pequeña (1,2 m) bajo cada palmera (decorativa o de juego). `M_TerrainToon` multiplica este
  color por la máscara de trazos de `Tools/Textures` (GDD §11).
- Pivote en el nivel del mar. El eje Y se invierte al pasar a Blender porque
  el importador FBX de Unreal lo vuelve a invertir; `import_island.py`
  comprueba la caja importada contra `SM_IslaSal_Terrain.json` (escala de
  importación 100: UE 5.6 ignora la unidad del FBX).
- Caminos (`paths`): Cala Varada → Palmar → faro y Playa Norte → faro, con
  pendiente ≤ 28° en todos sus triángulos (test
  `test_caminos_transitables_y_hundidos`; hoy 26,58°).

## Decoración

`island-decor` apoya cada instancia en la malla: la vegetación en el punto más
bajo de su huella (nada flota) y se descarta si la huella tiene más de 0,45 m
de desnivel (quedaría enterrada); los acantilados se entierran 1,6 m. El CLI
falla si `grounding_errors` encuentra algo flotando o enterrado.

## Vista previa sin Unreal

`island-preview` renderiza con Blender (EEVEE) la lámina
`docs/v3/laminas/relieve-antes-despues.png`: columnas F4B (c7b220c3), F6 SUAVE
(8d3fa23b) y el árbol de trabajo; filas cala, palmar y faro (cámaras de las
capturas f4b/f6) y un primer plano. Cada columna importa el FBX entregado, los
`island_decor.json` e `island_props.json` de su revisión (con `git lfs smudge`
si el FBX está en LFS), el mar como plano turquesa y las mallas K1/K2 de
`Art/Export`; el material del terreno replica `M_TerrainToon` con la máscara de
`Art/Export/Textures/T_TerrainStrokes.png` (generarla antes con
`cd ../Textures; uv run textures-strokes`).

```bash
uv run island-preview [--column ETIQUETA=REVISIÓN ...] [--view cala] [--percent 50]
```
