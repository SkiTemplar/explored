# Tools/Style — guía de estilo ejecutable (GDD v3 §11)

Fija el estilo de «Baliza Siete» antes de producir kits: paleta única, validador
de mallas y tres pilotos generados por script. Blender 5.2 como módulo de Python
(`bpy==5.2.2` de PyPI, Python 3.13); nada se edita a mano.

```bash
uv sync
uv run style-paleta                      # Art/Export/Style/T_Paleta.png + docs/v3/laminas/paleta-ampliada.png
uv run style-pilotos                     # Art/Export/Style/SM_*.fbx + manifest.json
uv run style-lamina                      # docs/v3/laminas/f2-pilotos.png (EEVEE)
uv run style-kit-k1                      # Art/Export/K1_Naturaleza/*.fbx + manifest.json + docs/v3/laminas/k1.png
uv run check-style <fbx|blend...> --budget <tipo|número>
uv run ruff check . && uv run basedpyright && uv run pytest -q --cov
```

## Método de color: UV a celda de paleta (no color de vértice)

- `paleta.json`: 32 colores con índice, nombre, familia y uso. Los índices 0-11
  son los 12 del GDD §11 y no se reordenan (el validador lo comprueba).
- Cada cara pone **todas** sus UV (canal 0) en el centro de una celda de la hoja
  32 × 1: `u = (índice + 0,5) / 32`, `v = 0,5`. El validador acepta UV dentro de
  la celda a más de ¼ de celda del borde.
- Se descarta el color de vértice porque cambiar la hoja (día/noche, variantes)
  recolorea todo sin reexportar, y evita la conversión sRGB/lineal de los
  atributos de color en el FBX.

## Reglas de check-style

Fallan (salida 1): texturas (nodos `TEX_*` o imágenes en el fichero), número de
materiales distinto de 1, UV fuera de una única celda, n-gones, escala o
rotación sin aplicar, pivote fuera de la base (z mínima ≠ 0 ± 5 mm u origen
fuera de la huella XY) y triángulos por encima del presupuesto. Avisos: caras
con sombreado suave y nombre sin `SM_`. Salida 2: error de uso.

| `--budget` | Tris | GDD §11 |
|---|---|---|
| `objeto_mano` | 300 | objeto de mano |
| `vegetacion` | 400 | hierba, arbusto, roca pequeña |
| `palmera` | 1.500 | palmera, bambú, roca grande |
| `estructura` | 800 | refugio, barril, hoguera, balsa |
| `faro` | 3.000 | torre, linterna, casa del faro |
| `carguero` | 4.000 | casco y puente del carguero |

## Pilotos (F2)

| Malla | Tris | Tope | Medidas | Colores |
|---|---|---|---|---|
| `SM_PalmA` | 1.386 | 1.500 | 6,0 m de alto, copa de 4,8 m | madera_media, madera_oscura, verde_palma, hierba_oscura |
| `SM_Cuchillo` | 220 | 300 | 0,28 m | acero, papel, roca_media, latón, madera_oscura |
| `SM_BarrilLluvia` | 744 | 800 | 0,98 m de alto, Ø 0,70 m (0,80 con grifo) | madera_clara, madera_media, madera_oscura, hierro, agua_media, latón |

La geometría es determinista; los FBX cambian de bytes en cada exportación
porque el exportador de Blender graba la fecha.

## Kit K1 Naturaleza (`explored_style/kits/k1.py`)

| Malla | Tris | Tope | Medidas (m) |
|---|---|---|---|
| `SM_PalmA_01` | 1.418 | 1.500 | 6,3 de alto, copa 4,8 |
| `SM_PalmB_01` | 1.188 | 1.500 | 4,3 de alto, inclinada 2,3 |
| `SM_Bamboo_01` | 1.372 | 1.500 | 4,6 de alto, 7 cañas |
| `SM_Shrub_01` | 400 | 400 | 1,6 × 1,3 × 1,0 |
| `SM_GrassClump_01` | 360 | 400 | 0,9 × 1,0 × 0,6 |
| `SM_RockBig_01` | 580 | 1.500 | 2,7 × 1,8 × 1,7 |
| `SM_RockSmall_01` | 260 | 400 | 0,7 × 0,6 × 0,5 |
| `SM_CliffRock_01` | 1.008 | 1.500 | 8 × 4,1 × 6 |

`SM_CliffRock_01` es modular: el frente mira a -Y y los perfiles de x = ±4 m
coinciden (lo comprueba `test_acantilado_modular`), así que se encadena cada
8 m sin junta; la trasera (+Y) es plana para apoyarla contra el terreno.
Colores y medidas exactas de cada malla en `Art/Export/K1_Naturaleza/manifest.json`.

## Importación en Unreal 5.6

- FBX: metros con `apply_unit_scale` (1 m = 100 uu), ejes FBX -Z adelante /
  Y arriba que el importador convierte a Z arriba / X adelante. Import Uniform
  Scale 1,0; *Convert Scene* activado; normales **importadas** (son planas por
  cara); sin importar materiales ni texturas; asignar `M_Toon` (o una instancia).
  Colisión: simple (caja o cápsula) generada en el editor; sin LOD (por debajo
  del presupuesto).
- `T_Paleta.png`: filtro *Nearest*, sin mipmaps, sRGB activado, compresión
  sin pérdidas (`UserInterface2D` o `VectorDisplacementmap`) para que los
  colores sean exactos; `M_Toon` la muestrea con el canal UV 0.

## Lámina

`style-lamina` importa los FBX exportados (revisa lo entregado), los sombrea
como `M_Toon` (color de la hoja × 3 bandas: 0,6 / 0,8 / 1,0, sin especular)
con un único sol y los compone con una cápsula de 1,75 m y la tira de la
paleta. Los tests la renderizan con Workbench a resolución reducida.
