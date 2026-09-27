# Kit de construcción modular de la base

Generado por código en `Tools/Blender/props/kit_construccion.py` (Blender 5.2
headless). 14 piezas × 4 materiales = **56 mallas**, exportadas por
`run_props.py` a `Art/Export/Props/Kit<Material>/SM_Kit_<Material>_<Pieza>.fbx`
(grupos `KitPalma`, `KitBambu`, `KitMadera`, `KitPiedra` en el manifiesto).

Previsualización (escala real, sin normalizar):

| Lámina | Contenido |
|---|---|
| `docs/art/modelos/kit-construccion-montaje.png` | Una cabaña montada por material con las mismas cotas |
| `docs/art/modelos/kit-construccion-palma.png` | Catálogo hoja de palma |
| `docs/art/modelos/kit-construccion-bambu.png` | Catálogo bambú |
| `docs/art/modelos/kit-construccion-madera.png` | Catálogo madera |
| `docs/art/modelos/kit-construccion-piedra.png` | Catálogo piedra |

Regenerar: `blender -b --factory-startup --python Tools/Blender/props/preview_kit.py`
(Cycles en CPU; `-- --mode=catalog --materials=Stone` para iterar una sola).

## Módulo de rejilla

Todas las piezas comparten estas cotas (constantes al principio del script;
si se cambian allí, se cambian aquí):

| Cota | Valor | Uso |
|---|---|---|
| `GRID` | **2,00 m** | Lado de la celda (suelo, pared, tejado) |
| `FOUND_H` | 0,60 m | Altura del pilote/cimiento: el suelo se apoya aquí |
| `FLOOR_T` | 0,15 m | Grosor del suelo: su cara superior es la base de las paredes |
| `WALL_H` | 2,50 m | Pared completa |
| `HALF_WALL_H` | 1,25 m | Media pared (+ remate propio de 5-6 cm) |
| `STOREY` | 2,65 m | Cara de suelo a cara de suelo (`WALL_H + FLOOR_T`) |
| Hueco de puerta | 1,00 × 2,05 m | Centrado en la pared |
| Hoja de puerta | 0,96 × 2,02 m | Cabe en el hueco con 2 cm de holgura |
| Hueco de ventana | 0,90 m, de z 0,95 a 1,75 | Centrado |
| `ROOF_PITCH` | 35° | Todas las vertientes |
| `ROOF_OVERHANG` | 0,35 m | Alero en el lado bajo, medido sobre la pendiente |
| Escalera | 1,00 × 4,00 m, 13 peldaños | Sube un piso (2,65 m) en 2 celdas |
| Barandilla | 2,00 × 1,00 m | Sobre el borde del suelo |

Grosor de pared por material (centrado sobre la línea de rejilla): palma 12 cm,
bambú 12 cm, madera 14 cm, piedra 30 cm. Por eso las paredes de piedra
"engordan" 15 cm hacia cada lado y los materiales se pueden mezclar sin mover la
rejilla.

## Pivotes y encaje

Todas las piezas tienen el pivote en su base:

- **Cimiento/pilote**: centro de su huella (≤ 0,52 m) en z = 0. Se coloca en las
  **esquinas** de celda (x, y múltiplos de 2 m + 1 m respecto al centro de celda).
- **Suelo**: centro de la celda, z = 0 en su cara inferior. Se coloca a
  z = `FOUND_H` sobre pilotes, o a ras de terreno.
- **Pared, pared con puerta/ventana, media pared, barandilla**: centro del lado de
  la celda, corren a lo largo de X local, z = 0 en la cara superior del suelo.
  Rotar 90° para los lados en Y.
- **Tejados**: centro de la celda, z = 0 en la coronación de la pared
  (`FOUND_H + FLOOR_T + WALL_H` sobre pilotes = 3,25 m). El alero sobresale
  0,35 m por el lado bajo y **baja 0,20 m por debajo de z = 0** (vierte por
  fuera de la pared). Los tejados miden exactamente 2 m a lo ancho del alero, así
  que se encadenan sin solape a lo largo de X.
  - *A un agua*: alero en −Y, lado alto en +Y (sube 1,40 m).
  - *A dos aguas*: aleros en ±Y, cumbrera en y = 0 (sube 0,70 m).
  - *Esquina (limatesa)*: aleros en −Y y −X, punto alto en (+1, +1). Empalma en su
    borde +X con un tejado a un agua normal y en su borde +Y con uno girado −90°.
    Altura = tan 35° · min(x+1, y+1).
- **Hastiales** (`Gable`, `GableShed`): cierran el triángulo que el tejado deja
  en su testero. Como una pared (corren a lo largo de X local, pivote en el
  centro del lado) pero con z = 0 en la **coronación de pared**, igual que los
  tejados. Se colocan en el lado de la celda perpendicular a la cumbrera, girados
  90°, sobre una pared completa.
  - `Gable` (dos aguas): triángulo de 2 m × 0,67 m, vértice en x = 0.
  - `GableShed` (un agua): triángulo rectángulo, bajo en −X, alto en +X
    (1,37 m). Con el giro de +90° su +X apunta al lado alto (+Y) del tejado; en
    los dos testeros va con el mismo giro.
  - Quedan 3 cm por debajo de la cara inferior de la cobertura (sin
    z-fighting) y llevan un remate de pendiente (rama, caña, tabla de canto o
    dintel de madera en piedra) que tapa el corte. Se generan recortando la
    pared genérica del material por el perfil del tejado, así que heredan su
    aparejo (hiladas de piedra, filas de hoja, tablas, cañas).
- **Escalera**: centro del arranque en z = 0, sube hacia +Y hasta (y = 4, z = 2,65).
- **Puerta**: bisagra en x = 0, la hoja se extiende hacia +X; colocarla en
  (−0,48, 0) del hueco para que abra girando sobre Z.

## Materiales (progresión hoja → bambú → madera → piedra)

| Material | Estructura | Relleno de pared | Tejado | Suelo |
|---|---|---|---|---|
| Hoja de palma | Rollizos de rama con atados de fibra | Filas de manojos de hoja dorada con fleco, solapadas | Manojos de hoja en hileras + cumbrera enrollada | Rollizos sobre durmientes + esterilla |
| Bambú | Cañas gruesas con nudos | Cañas verticales + travesaños a ambos lados | Cañas enteras a lo largo de la pendiente, alternas | Cañas sobre durmientes de bambú |
| Madera | Postes escuadrados oscuros | Tablas verticales + travesaños interiores | Tejuelas a matajuntas + cumbrera en V | Tablas sobre viguetas |
| Piedra | Sillarejo almohadillado con dinteles | Hiladas a matajuntas alineadas a huecos, musgo en la base | Lajas de pizarra y piedra | Losas irregulares |

Todos usan los materiales estables del kit de props (`M_Wood`, `M_Stone`,
`M_Leaf`); el color real va por vértice, con un tono propio por elemento (cada
tabla, caña, piedra o manojo) dentro de la paleta del material. Los valores de
paleta son **lineales**: un 0,8 lineal ya se ve casi blanco en el render.

## Presupuestos (triángulos)

| Pieza | Máx. | Real (palma / bambú / madera / piedra) |
|---|---|---|
| Cimiento | 1 200 | 384 / 580 / 324 / 648 |
| Suelo | 4 000 | 2 766 / 3 464 / 1 080 / 1 836 |
| Pared | 6 000 | 5 454 / 4 612 / 1 296 / 4 320 |
| Tejado (3 tipos) | 7 000 | ≤ 6 432 |
| Escalera | 4 000 | ≤ 3 404 |
| Puerta, barandilla | 3 000 | ≤ 2 264 |
| Hastial dos aguas | 4 000 | 1 882 / 2 582 / 1 286 / 938 |
| Hastial un agua | 5 000 | 2 690 / 2 892 / 1 236 / 1 648 |

`validate.py` comprueba presupuesto, geometría degenerada, color de vértice «Col»
y materiales estables (56/56 del kit en verde a 2026-09-26; 169/169 props en total a 2026-09-27).

## Mobiliario de base (`mobiliario_base.py`, grupo `MobiliarioBase`)

Lámina: `docs/art/modelos/mobiliario-base.png`. Reutiliza primitivas, paletas y
biseles del kit. Pivote en la base.

| Malla | Medidas aprox. | Notas |
|---|---|---|
| `SM_Base_Bed` | 2,0 × 1,0 m | Cabecero de cañas, colchón de hoja, tapa a rayas |
| `SM_Base_Workbench` | 1,6 × 0,8 × 0,9 m | Balda con leña, mazo de piedra, cuenco |
| `SM_Base_DisplayShelf` | 1,9 × 0,48 × 2,1 m | 3 × 4 huecos para tesoros (museo) |
| `SM_Base_DisplayCase` | 1,0 × 0,6 × 1,3 m | Urna de cristal del Albatros (`M_Glass`) |
| `SM_Base_RainCollector` | Ø 1,4 × 1,8 m | Barril de duelas, embudo de palma, grifo de caña |
| `SM_Base_DryingRack` | 2,3 × 1,4 × 1,7 m | Caballetes en A con pescado abierto |
| `SM_Base_Smokehouse` | 2,7 × 2,7 × 2,8 m | Paredes de palma del kit (escaladas), fogón, respiradero |
| `SM_Base_Dock` / `_DockEnd` | 2 × 4 m | Cubierta a **+2,0 m** sobre el pivote (fondo); se encadena en Y |
| `SM_Base_GardenPlot_Logs` / `_Stones` | 2 × 2 m | Encaja en una celda de la rejilla |
| `SM_Base_MuseumPanel` | 2,06 × 0,24 × 2,4 m | Panel de pared del museo (`panel_museo`): bastidor de un lado de celda, estera trenzada en damero con rombos teñidos, dos ganchos a z 1,06 m (hueco Grande en (0, −6, 110) cm) y disco estelar en la cresta. Frente en −Y, la cara +Y va contra la pared |

## Producción, huerto y estructura (`produccion_base.py`, grupo `ProduccionBase`)

Lámina: `docs/art/modelos/produccion-base.png`. Piezas de
`Content/Data/building_pieces.json` que seguían sin malla. Reutilizan
primitivas, paletas y biseles del kit; pivote en la base, escala real.

| Malla | Pieza de datos | Medidas aprox. | Notas |
|---|---|---|---|
| `SM_Base_WorkStone` | `piedra_trabajo` | 1,4 × 1,0 × 0,5 m | Basalto facetado con liquen, yunque, percutor, azuela a medio pulir, lascas |
| `SM_Base_ClayOven` | `horno_barro` | Ø 1,9 × 1,5 m | Cúpula de barro con hollín en la boca, arco de adobe, brasas, respiradero |
| `SM_Base_Chimney` | `chimenea` | 1,5 × 0,9 × 4,25 m | Hogar con boca y dintel, campana y cañón de sillarejo; va contra una pared |
| `SM_Base_Loom` | `telar` | 1,3 × 0,9 × 1,7 m | Bastidor de bambú inclinado, urdimbre, tela a rayas, lanzadera, cesto |
| `SM_Base_Trellis` | `espaldera` | 2,0 × 0,6 × 2,1 m | Celosía de bambú con maracuyá (hojas, flores y frutos); ocupa un lado de celda |
| `SM_Base_LemonBed` | `arriate_limonero` | Ø 1,9 m | Anillo de piedras con tierra, alcorque y marca; el árbol va aparte (plants.json) |
| `SM_Base_ChartTable` | `mesa_cartografia` | 1,6 × 1,0 × 0,92 m | Mapa pintado por vértice (islas, costa, rumbo), carta de varillas, rollos |

La chimenea sube 4,25 m desde la cara del suelo: supera la cumbrera de un
tejado a dos aguas (2,5 + 0,70 m) con margen. El cuerpo acaba en
y = +0,45 (la repisa sobresale 6 cm más): esa cara va contra la pared.

### Enlace con los datos

`building_pieces.json` apunta ya a mallas existentes en las 26 piezas que
estaban en `meshes_pendientes.json/buildingPieces` (estructura → kit modular
`SM_Kit_<Material>_<Pieza>`; producción y huerto → `SM_Base_*`). `Tools/DataCheck`
reconoce ahora los nombres del kit, que se generan cruzando `MATERIALS` ×
`PIECES` en `kit_construccion.py`.

## Ruinas polinesias (`ruinas_polinesias.py`, grupos `RuinasMarae` y `RuinasTallas`)

Láminas: `docs/art/modelos/ruinas-marae.png`, `docs/art/modelos/ruinas-tallas.png`.
Basalto cálido oscuro con musgo en las caras que miran arriba y manchas de
liquen (amarillo y naranja); las estatuas en toba rojiza.

Módulo del yacimiento (para montar marae de cualquier tamaño con piezas):

| Cota | Valor |
|---|---|
| Tramo de terraza | 2 m de ancho (en X), se encadena |
| Escalón `TIER_H` | 0,45 m (el segundo apoya 2 cm más bajo para no dejar rendija) |
| Retranqueo `TIER_INSET` | 0,8 m |
| Escalinata | 1,6 m de ancho, 2 peldaños, sube un escalón (0,45 m) en 0,7 m |
| Enlosado | 2 × 2 m, se encadena en X/Y |
| Muro de piedra seca | 2 m × 1 m + albardilla, talud 0,75 → 0,5 m; esquina en L |

Estatuas: diseño **propio** del pueblo navegante (no moai ni tiki): cuerpo
rechoncho, cabeza ancha de ojos redondos que mira al cielo, diadema de olas,
orejas pequeñas y manos con un disco estelar de 8 puntas.

| Malla | Notas |
|---|---|
| `Ruin_Marae_Small` / `_Large` | 2 y 3 escalones, enlosado arriba, piedras erguidas; el grande con escalinata |
| `Ruin_Marae_TerraceStraight` / `_TerraceCorner` / `_Paving` / `_Steps` | Piezas modulares |
| `Ruin_DryWall_Straight` / `_Ruined` / `_Corner` | Muro de piedra seca |
| `Ruin_StandingStone_A/B/C` | 1,5 / 2,3 / 3,1 m con calzos |
| `Ruin_Statue_Navigator` / `_Seated` / `_HeadFallen` | 2,5 m / 1,8 m / cabeza caída semienterrada |
| `Ruin_Petroglyph_Honu/Canoe/Star/Bird` | Losa de 1,4 m con relieve hundido real (malla densa) |
| `Ruin_Canoe_DoubleWreck` | Canoa doble de 6,5 m, un casco partido, dunas; recortada bajo z = 0 |
| `Ruin_Altar_Table` / `_OfferingStone` | Mesa sobre dos piedras con ofrendas / piedra de cazoletas |

## Tesoros (`tesoros.py`, grupo `Tesoros`)

Lámina: `docs/art/modelos/tesoros.png` (cada pieza normalizada a la misma
dimensión mayor **solo en la lámina**; las mallas van a escala real). Las
figuras reutilizan la cabeza de las estatuas para que el estilo del pueblo se
reconozca a cualquier tamaño. Todas interactuables (se exponen en
`SM_Base_DisplayShelf` / `SM_Base_DisplayCase`).

| Malla | Tamaño | Notas |
|---|---|---|
| `SM_Treasure_FishHook_Bone` | 11 cm | Anzuelo de hueso con lengüeta y atadura roja |
| `SM_Treasure_FishHook_Shell` | 21 cm | Señuelo de nácar con punta de hueso y borla |
| `SM_Treasure_ShellPendant` | 28 cm | Disco de nácar con estrella incisa, cordón y cuentas |
| `SM_Treasure_ShellNecklace` | 41 cm | Collar de cauris con disco central |
| `SM_Treasure_StoneFigure_Navigator` | 22 cm | Piedra verde, mira al cielo |
| `SM_Treasure_StoneFigure_Twins` | 18 cm | Gemelos espalda con espalda |
| `SM_Treasure_StickChart` | 82 cm | Rejilla de varillas, frentes de oleaje curvos, conchas = islas |
| `SM_Treasure_CeremonialPaddle` | 1,7 m | Tumbado; hoja con bandas de dientes y ojo estelar |
| `SM_Treasure_Tapa` | 1,2 × 0,85 m | Tela de corteza ondulada con marco y rombos pintados |

### Piezas únicas y raras

Lámina: `docs/art/modelos/tesoros-unicos.png` (modo `tiles` de
`preview_kit.py`: cada pieza sola a cámara cercana 3/4). Los siete tesoros de
`artifacts.json` que compartían malla con su versión común tienen ahora la suya:

| Malla | Tesoro | Tamaño | Notas |
|---|---|---|---|
| `SM_Treasure_FishHook_Whalebone` | `anzuelo_ceremonial` | 19 cm | Hueso de ballena sin lengüeta, muescas, borla roja y cabecita del pueblo que mira arriba |
| `SM_Treasure_Breastplate_Pearl` | `pectoral_nacar` | 30 cm | Media luna de nácar de labio negro (irisado rosa/oro/verde agua), fila incisa, 5 dientes de hueso, cordón |
| `SM_Treasure_TurtlePendant` | `colgante_carey` | 20 cm | Tortuga de carey (placas y moteado ámbar), cordón con cuentas |
| `SM_Treasure_StoneFigure_SkyGazer` | `figura_mira_cielo` | 22 cm | Basalto arrodillado, cabeza atrás, alza el disco estelar; algas y coral (ruina sumergida) |
| `SM_Treasure_StickChart_Swell` | `carta_oleaje` | 99 cm | Marco hexagonal, 3 ejes, frentes de mar de fondo y anillos de rebote en 7 islas (la central de nácar) |
| `SM_Treasure_Tapa_Stars` | `tapa_estrellas` | 1,3 × 0,9 m | Fondo oscuro, 7 estrellas de rumbo de 8 puntas, orla de dientes, banda de olas, esquina doblada |
| `SM_Treasure_Paddle_DoubleCanoe` | `remo_canoa_doble` | 1,97 m | Remo de gobierno: hoja de laurel con nervio, canoa doble pintada, bellotas de mar |

Regenerar la lámina:
`blender -b --factory-startup --python Tools/Blender/props/preview_kit.py -- --mode=tiles --module=tesoros --out=tesoros-unicos --only=<nombres separados por comas>`
