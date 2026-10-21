# Tools/Style — guía de estilo ejecutable (GDD v3 §11)

Fija el estilo de «Baliza Siete» antes de producir kits: paleta única, validador
de mallas y tres pilotos generados por script. Blender 5.2 como módulo de Python
(`bpy==5.2.2` de PyPI, Python 3.13); nada se edita a mano.

```bash
uv sync
uv run style-paleta                      # Art/Export/Style/T_Paleta.png + docs/v3/laminas/paleta-ampliada.png
uv run style-pilotos                     # Art/Export/Style/SM_*.fbx + manifest.json
uv run style-lamina                      # docs/v3/laminas/f2-pilotos.png (EEVEE)
uv run style-kit-k3                      # Art/Export/K3_Objetos/*.fbx + docs/v3/laminas/k3.png
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

## Kit K3 Objetos

`uv run style-kit-k3` genera las 15 mallas de objeto del GDD §11.1, las
variantes `SM_CocoVacio` y `SM_PezAsado` y la fauna estática (`SM_FaunaPez`,
morro en +X; `SM_FaunaGaviota`, alas abiertas; ambas se animan por código),
exporta `Art/Export/K3_Objetos/SM_<Nombre>_01.fbx` con `manifest.json` (tris,
medidas y colores por malla) y renderiza `docs/v3/laminas/k3.png`.
Código: `explored_style/kits/k3.py` (catálogo y exportación), `k3_objetos.py`,
`k3_largos.py`, `k3_fauna.py` y `k3_lamina.py`. `--sin-lamina` solo exporta.

Los objetos de mano yacen en el plano XY centrados en el origen; los que se
ven de frente (cantimplora, barómetro, lente) miran a -Y. Presupuestos:
`objeto_mano` (300), `estructura` (800) para tronco, remo y lente, 200 para
el pez de fauna y 400 para la gaviota.

| Malla | Tris | Tope | Medidas X × Y × Z (m) |
|---|---|---|---|
| `SM_Cuchillo_01` | 220 | 300 | 0,28 × 0,05 × 0,03 |
| `SM_Cantimplora_01` | 288 | 300 | 0,20 × 0,10 × 0,24 |
| `SM_Barometro_01` | 268 | 300 | 0,10 × 0,03 × 0,13 |
| `SM_Coco_01` | 144 | 300 | 0,22 × 0,17 × 0,16 |
| `SM_CocoVacio_01` | 140 | 300 | 0,17 × 0,16 × 0,08 |
| `SM_Lena_01` | 188 | 300 | 0,44 × 0,28 × 0,14 |
| `SM_Fibra_01` | 232 | 300 | 0,51 × 0,16 × 0,09 |
| `SM_Cuerda_01` | 280 | 300 | 0,32 × 0,31 × 0,08 |
| `SM_CanaBambu_01` | 232 | 300 | 1,47 × 0,21 × 0,11 |
| `SM_CanaPescar_01` | 258 | 300 | 1,86 × 0,07 × 0,25 |
| `SM_Tronco_01` | 496 | 800 | 2,53 × 0,44 × 0,42 |
| `SM_Remo_01` | 280 | 800 | 1,49 × 0,18 × 0,05 |
| `SM_Pez_01` | 204 | 300 | 0,34 × 0,12 × 0,05 |
| `SM_PezAsado_01` | 264 | 300 | 0,47 × 0,12 × 0,05 |
| `SM_TarroAceite_01` | 280 | 300 | 0,12 × 0,12 × 0,16 |
| `SM_Mecha_01` | 258 | 300 | 0,31 × 0,06 × 0,04 |
| `SM_Lente_01` | 660 | 800 | 0,50 × 0,17 × 0,53 |
| `SM_FaunaPez_01` | 164 | 200 | 0,32 × 0,05 × 0,12 |
| `SM_FaunaGaviota_01` | 336 | 400 | 0,62 × 1,17 × 0,18 |

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
