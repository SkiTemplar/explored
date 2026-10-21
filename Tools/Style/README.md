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
uv run style-kit-k2                      # Art/Export/K2_Estructuras/*.fbx + manifest.json + docs/v3/laminas/k2.png
uv run style-kit-k3                      # Art/Export/K3_Objetos/*.fbx + docs/v3/laminas/k3.png
uv run style-kit-k5                      # Art/Export/K5_Cielo/*.fbx + manifest.json + docs/v3/laminas/k5.png
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
fuera de la huella XY) y triángulos por encima del presupuesto. Excepción
documentada: con `--budget manos` el pivote va en el codo (origen dentro de la
caja envolvente y x mínima ≥ -0,1 m), porque los antebrazos cuelgan de la cámara. Avisos: caras
con sombreado suave y nombre sin `SM_`. Salida 2: error de uso.

| `--budget` | Tris | GDD §11 |
|---|---|---|
| `objeto_mano` | 300 | objeto de mano |
| `vegetacion` | 400 | hierba, arbusto, roca pequeña |
| `palmera` | 1.500 | palmera, bambú, roca grande |
| `estructura` | 800 | refugio, barril, hoguera, balsa |
| `faro` | 3.000 | torre, linterna, casa del faro |
| `carguero` | 4.000 | casco y puente del carguero |
| `manos` | 3.000 | `SM_ManoIria`: 2 antebrazos × 1.500; pivote en el codo |

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
## Kit K2 · Estructuras (+ manos)

`uv run style-kit-k2` (`explored_style/kits/k2.py`; formas en `k2_faro`,
`k2_carguero`, `k2_props` y `k2_manos`) exporta `SM_<Nombre>_01.fbx` a
`Art/Export/K2_Estructuras/` con `manifest.json` (tris, medidas, colores,
presupuesto, pivote y ensamblajes) y renderiza `docs/v3/laminas/k2.png`.
Frente de todas las piezas: -Y en Blender (la puerta, el grifo y el lado
abierto del refugio miran ahí).

| Malla | Tris | Tope | Medidas (m) |
|---|---|---|---|
| `SM_FaroTorre_01` | 1.632 | 3.000 | 12,0 de alto, zócalo Ø 5,6 |
| `SM_FaroLinterna_01` | 1.434 | 3.000 | 4,6 de alto, octógono Ø 3,0 |
| `SM_FaroCasa_01` | 1.336 | 3.000 | 6,7 × 5,7 × 5,7 |
| `SM_CajaSuministros_01` | 644 | 800 | 0,9 × 0,6 × 0,66 (con asas 1,09) |
| `SM_CargueroCasco_01` | 1.766 | 4.000 | 40,2 de eslora, 15,9 con palos |
| `SM_CargueroPuente_01` | 1.692 | 4.000 | 9,3 × 8,4 × 8,8 |
| `SM_RefugioPalma_01` | 728 | 800 | 4,0 × 3,2 × 2,5 |
| `SM_BarrilLluvia_01` | 794 | 800 | 0,98 de alto, Ø 0,70 |
| `SM_Hoguera_01` | 720 | 800 | Ø 1,4, 0,58 de alto |
| `SM_Balsa_01` | 788 | 800 | 3,0 × 1,3 (1,5 con cabos) |
| `SM_ManoIria_01` | 1.384 | 3.000 | 656 + 728 tris por mano (tope 1.500) |

Ensamblajes (en `manifest.json` → `ensamblaje`, coordenadas de Blender):

- `SM_FaroLinterna_01` sobre `SM_FaroTorre_01` en (0, 0, 11) m. Ranuras de juego
  en `ranuras_m` (aro de la lente, dos tarros, portamecha), en espacio local de
  la linterna; la puerta (lado abierto) mira a -Y.
- `SM_CargueroPuente_01` sobre `SM_CargueroCasco_01`: el casco se exporta ya
  escorado 8° (estribor, -Y, abajo) y con la proa 2° arriba; el puente se
  exporta recto y se coloca con `ubicacion_m` y `rotacion_grados`
  (Euler XYZ de Blender). En Unreal la Y cambia de signo: conviene montarlo
  como hijo del casco y comprobarlo a ojo.
- `SM_ManoIria_01`: origen en el punto medio de los codos, antebrazos hacia +X
  (adelante en Unreal), mano derecha en -Y de Blender (derecha en Unreal).
- La caja de mapas del carguero es `SM_CajaSuministros_01` con otra instancia
  de `M_Toon`. El fuego de la hoguera lo pone el motor.
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

## Kit K5 Cielo

`uv run style-kit-k5` (`explored_style/kits/k5.py`; formas en `k5_formas.py`,
lámina en `k5_lamina.py`) genera 5 nubes de cúmulo y los discos del sol y la
luna, exporta `Art/Export/K5_Cielo/SM_<Nombre>_01.fbx` con `manifest.json` y
renderiza `docs/v3/laminas/k5.png`: arriba, la vista desde la isla (ojos a
1,7 m, base de las nubes a 120 m, a 270-520 m); abajo, la fila a la misma
escala vista desde abajo. `--sin-lamina` solo exporta.

| Malla | Tris | Tope | Medidas X × Y × Z (m) | Lóbulos |
|---|---|---|---|---|
| `SM_Nube_A_01` | 256 | 400 | 20,0 × 11,6 × 10,4 | 3 |
| `SM_Nube_B_01` | 296 | 400 | 30,0 × 16,1 × 15,0 | 4 |
| `SM_Nube_C_01` | 328 | 400 | 40,0 × 16,5 × 18,2 | 5 |
| `SM_Nube_D_01` | 360 | 400 | 50,0 × 17,1 × 19,5 | 6 |
| `SM_Nube_E_01` | 392 | 400 | 60,0 × 18,0 × 21,0 | 7 |
| `SM_DiscoSol_01` | 48 | 64 | Ø 1,0, plano | — |
| `SM_DiscoLuna_01` | 48 | 64 | Ø 1,0, plano | — |

- Nubes: lóbulos cerrados y solapados (cúpulas de revolución con base
  plana), pivote en el centro de la base. Arriba `papel`, abajo (base y
  primer tramo de los lóbulos apoyados) `acero`. `check-style --budget 400`.
- Discos: en el plano XY con normal +Z y pivote en el centro; el motor
  orienta la Z local hacia la cámara (Y local arriba) y escala
  Ø = 2 · distancia · tan(diámetro angular / 2). Sol: `laton` con halo
  `cielo_ocaso`; luna: `papel` con media luna de sombra `acero` (luz arriba a
  la izquierda). `check-style --budget 64`.
- Material: las nubes piden una instancia `MI_ToonNube` con bandas suaves
  (0,86 / 0,93 / 1,0; la lámina las usa) y los discos, sin sombreado; ni
  nubes ni discos proyectan sombra.
- Propuesta de color (no aplicada; `paleta.json` intacto): la paleta no tiene
  blanco frío ni gris lila. Se usan `papel` y `acero`; si se amplía la paleta,
  `blanco_nube #FAF7F2` y `gris_lila #C7C3D6` (detalle en `manifest.json`).

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
