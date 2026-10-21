# Tools/Island — terreno de la Isla de Sal (GDD v3 §6, F3a)

Genera el terreno como **malla estática facetada low poly** (decisión de F3a que
sustituye al `Landscape` del GDD §6) a partir de `Content/Data/island.json`, y
valida `Content/Data/island_props.json`. Blender 5.2 como módulo de Python
(`bpy==5.2.2`, Python 3.13); reutiliza `Tools/Style` (`meshkit`, `palette`) sin
modificarlo.

```bash
uv sync
uv run island-terrain          # Art/Island/SM_IslaSal_Terrain.fbx + .json (cifras)
uv run island-props-check      # island_props.json frente al GDD (§5.3, §6, §11.1)
uv run island-decor            # Content/Data/island_decor.json: 400-900 instancias decorativas (HISM, sin colisión)
uv run ruff check . && uv run basedpyright && uv run pytest -q --cov
```

Después, en Unreal (PowerShell, raíz del repo):

```powershell
Tools\unreal_python.ps1 -Script Tools/Unreal/build_materials.py    # T_Paleta + M_Toon
Tools\unreal_python.ps1 -Script Tools/Unreal/import_island.py      # /Game/Art/Island/SM_IslaSal_Terrain
Tools\unreal_python.ps1 -Script Tools/Unreal/create_map_islasal.py # Map_IslaSal
```

## Relieve

`explored_island.layout.height_at` es una copia exacta de
`FIslandLayout::SampleColumn` (C++): elipses con costa armónica (`coast`),
meseta con plataforma, colinas gaussianas de zona (`hill`, `hillSigma`) que se
desvanecen hacia la costa entre las distancias normalizadas 0,3 y 1, y rocas u
hondonadas (`rocks`, altura negativa = cala). El juego y la malla comparten
así el mismo relieve; la malla solo añade el fondo marino recortado a
`terrain.seabed` (−6 m), la falda exterior a `terrain.skirt` (−30 m) y un
ruido de facetas de ±0,25 m en tierra firme (±0,45 m en las rocas). El test
`IslandDataSpec` comprueba que `island.json` y `FIslandLayout::Provisional()`
dan la misma altura.

## Malla

- Rejilla de `terrain.cell` = 2,6 m con los vértices desplazados ±30 % de celda
  y la diagonal de cada celda elegida por hash: triángulos de 2,6-3,7 m,
  ≈ 64 000 en total, normales planas.
- Color por triángulo en la hoja de paleta (UV al centro de la celda, como
  Tools/Style): arena húmeda en la orilla (−0,6 a 0,35 m), arena clara hasta
  1,5 m, hierba clara/oscura en manchas (más oscura en el Bosque de Bambú, más
  clara en la colina), tierra (`arena_oscura`) en los caminos, roca en
  pendientes > 35° y en la huella de las rocas, roca oscura > 50°, fondo
  claro/oscuro bajo el agua.
- Pivote en el nivel del mar. El eje Y se invierte al pasar a Blender porque
  el importador FBX de Unreal lo vuelve a invertir; `import_island.py`
  comprueba la caja importada contra `SM_IslaSal_Terrain.json` (escala de
  importación 100: UE 5.6 ignora la unidad del FBX).
- Caminos (`paths`): Cala Varada → Palmar → faro y Playa Norte → faro, con
  pendiente ≤ 30° en todos sus triángulos (test `test_caminos_transitables`;
  hoy 28,1°).
