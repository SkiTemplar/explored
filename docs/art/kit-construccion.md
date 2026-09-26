# Kit de construcción modular de la base

Generado por código en `Tools/Blender/props/kit_construccion.py` (Blender 5.2
headless). 12 piezas × 4 materiales = **48 mallas**, exportadas por
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

`validate.py` comprueba presupuesto, geometría degenerada, color de vértice «Col»
y materiales estables (48/48 en verde a 2026-09-26).
