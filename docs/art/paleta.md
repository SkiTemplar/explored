# Paleta low poly

Decisión vigente (GDD v2 §7.1): props low poly estilizados con packs CC0 (Kenney, KayKit,
Quaternius) coloreados con **una textura de paleta** y un único material, `M_LowPoly`. El
terreno volumétrico, el agua y las rocas fotobasheadas se conservan; sus texturas se
**armonizan** con esta paleta (mismo tono medio y croma) para que el terreno no desentone
junto a los packs.

- Fuente de verdad: `Tools/Textures/texgen/palette.py`.
- Datos versionados: [`Tools/Textures/paleta.json`](../../Tools/Textures/paleta.json)
  (sRGB en hex y lineal, celda, UV y rango de v de cada muestra, por isla). Lo escribe
  `gen_palette.py`; un test falla si el JSON no coincide con el generador.
- Atlas: `T_Palette_Landing`, `T_Palette_Esmeralda`, `T_Palette_Humo`, `T_Palette_Dientes`
  (512×512, en `Art/Export/Textures/`, no versionados).
- Hoja de contacto: [`paleta-2026-09-27.png`](paleta-2026-09-27.png) — atlas de cada isla,
  una muestra aplicada a formas low poly a 1-3 m sobre el suelo armonizado de la isla (con
  cielo y mar actuales), y el antes/después del terreno.

```bash
# paleta.json + atlas (+ hoja de contacto)
uv run --with numpy --with pillow python Tools/Textures/gen_palette.py --sheet docs/art/paleta-AAAA-MM-DD.png
# gen_textures.py también escribe los atlas y los registra en textures.json (kind "palette")
cd Tools/Textures && uv run --with numpy --with pillow --with pytest python -m pytest -q tests
```

## Criterios

Los colores se manipulan en **Oklab** (perceptual: ΔE ≈ 0.02 apenas visible, 0.1 claro).
`tests/test_palette_atlas.py` fija estos criterios:

| Criterio | Regla | Por qué |
|---|---|---|
| Albedo | sRGB en [0.035, 0.95] en medio, arriba y abajo | Igual que las texturas: ni negro ni blanco puros. |
| Degradado | ΔL entre arriba y abajo de 0.08 a 0.2 | Se lee volumen sin ensuciar ni quedar plano. |
| Distinguibles | ΔE > 0.035 entre muestras de una familia | Piezas contiguas de un objeto a 1-2 m (mango y hoja de un hacha, aros de un barril). |
| Comida | ΔE > 0.06 contra **todos** los suelos de la isla | Lo recogible tiene que saltar a la vista en arena, hierba, basalto, caliza y ceniza. |
| Croma del entorno | C ≤ 0.14 (salvo acentos: comida, UI, flores) | El agua del arrecife (C ≈ 0.11) es lo más saturado del paisaje; el entorno no le roba protagonismo. |
| Coherencia con el terreno | `piedra.basalto`, `piedra.caliza`, `vegetacion.hierba` a ΔE < 0.05 del terreno teñido de su isla | Una roca KayKit junto al acantilado escaneado no desentona. |
| Identidad | comida, metal, tela y UI iguales en las 4 islas | Un mango es el mismo mango en todas partes; solo cambia el entorno. |

## Islas

Las familias de **entorno** (madera, palma, bambú, piedra, vegetación) llevan un grado por
isla en Oklab (`dL`, factor de croma, `da`, `db`). La fila `terreno` no se gradúa: es el
tono medio de la textura armonizada multiplicado por el tinte de color de vértice que aplica
`M_Terrain` (`lerp(1, vc / media(vc), 0.18)`), con la paleta de `FTerrainDensity::PaletteFor`
(un test comprueba que `paleta.json` y `TerrainDensity.cpp` coinciden).

| Isla | Textura | Grado (dL, croma, da, db) | Ambiente |
|---|---|---|---|
| Isla del Amaraje | `T_Palette_Landing` | 0, ×1.00, 0, +0.004 | Luz de playa cálida y limpia: la referencia. |
| Esmeralda | `T_Palette_Esmeralda` | −0.015, ×1.08, −0.006, 0 | Selva húmeda: algo más oscura, verdes más ricos. |
| Isla del Humo | `T_Palette_Humo` | −0.025, ×0.85, +0.004, −0.004 | Volcán: apagada y cenicienta sin llegar a gris sucio (con ×0.78 la hierba de los props se separaba demasiado de la del terreno). |
| Los Dientes | `T_Palette_Dientes` | +0.015, ×0.88, −0.002, −0.008 | Caliza batida por el mar: fría, salina, clara. |

## Atlas

512×512, rejilla de 16×16 celdas de 32 px (una fila por familia; columnas libres en gris
`#808080`, filas 11-15 libres para crecer).

```
fila 0 madera · 1 palma · 2 bambú · 3 piedra · 4 metal · 5 tela · 6 vegetación · 7 comida
     8 UI · 9 terreno (referencia) · 10 entorno (agua y cielo, referencia)
celda (32 px, de arriba abajo):
  4 px margen = color «arriba»      ← v_rango[0]
 10 px rampa arriba → medio (Oklab, suavizada)
  4 px color «medio» exacto         ← uv (centro de la celda)
 10 px rampa medio → abajo
  4 px margen = color «abajo»       ← v_rango[1]
todas las columnas de la celda son iguales
```

- **UV de los packs:** u en el centro de la columna (`(col + 0.5) / 16`); v en
  `v_rango` de la celda. Una cara con v = centro recibe el color exacto; repartir v por la
  altura del objeto da el degradado (arriba claro y cálido, abajo oscuro y frío), como los
  atlas de degradado de KayKit.
- **Sin mip bleeding:** celdas alineadas a potencia de 2 y mips por promedio 2×2
  (`TMGS_SimpleAverage`): ningún mip hasta el 5 (celda = 1 texel) mezcla celdas. Con
  bilineal, un punto dentro de la zona útil no toca la celda vecina mientras
  `2^(k-1) ≤ margen`, es decir, hasta el **mip 3**; `M_LowPoly` limita el mip ahí
  (`MaxMip = 3`). Los tests comprueban ambas cosas.
- **Importación** (`import_textures.py`, kind `palette`): sRGB, `TC_EditorIcon`
  (UserInterface2D, RGBA8 sin compresión: BC1 comprime bloques de 4×4 que, en mips con
  celdas de menos de 4 px, mezclarían colores), `TMGS_SimpleAverage`, `TF_Bilinear`,
  `NeverStream` (1 MB por isla).
- **Alias de los packs:** `paleta.json › alias_packs` propone la muestra para los nombres de
  material habituales de los packs (`Wood`, `Leaves`, `Stone`, `Metal`…), como punto de
  partida para el remapeo de UV al importar.

## Terreno armonizado

`texgen.materials.generate()` pasa el albedo de `SandDry`, `SandWet`, `Grass`,
`VolcanicRock`, `Limestone` y `Ash` por `harmonize_albedo()`: desplaza la media Oklab
(L, a, b) al objetivo y conserva el detalle (desviaciones respecto a la media). Es una
operación por píxel, así que el tileado sin costura no cambia (lo comprueba
`test_tiling.py`) y los nombres y canales de las texturas tampoco.

| Material | Objetivo (L, C, h°) | Antes (L, C) | Cambio visible |
|---|---|---|---|
| `SandDry` | 0.840, 0.075, 80 | 0.844, 0.084 | Arena algo menos amarilla. |
| `SandWet` | 0.665, 0.066, 74 | 0.664, 0.073 | Casi igual. |
| `Grass` | 0.600, 0.115, 134 | 0.610, 0.141 | La hierba pintada era lo más saturado del paisaje (se leía fluorescente junto a los props). |
| `VolcanicRock` | 0.440, 0.032, −52 | 0.435, 0.031 | Igual (ya estaba en la paleta). |
| `Limestone` | 0.730, 0.030, 85 | 0.729, 0.028 | Igual. |
| `Ash` | 0.745, 0.012, 40 | 0.743, 0.011 | Igual. |

## Muestras

Color medio por isla (`=`: igual que Landing). Arriba/abajo y lineal en `paleta.json`.

| Muestra | Celda | Landing | Esmeralda | Humo | Dientes | Uso |
|---|---|---|---|---|---|---|
| `madera.clara` | 0,0 | `#caa572` | `#c3a16c` | `#be9e79` | `#c7ab87` | Tabla nueva, mangos, cajas KayKit claras. |
| `madera.miel` | 1,0 | `#b17e46` | `#ab7a3f` | `#a47851` | `#ae855e` | Madera por defecto (tablones, WoodPlanks). |
| `madera.caramelo` | 2,0 | `#905a30` | `#8b562a` | `#83553a` | `#8d6146` | Vigas, postes, muebles oscuros. |
| `madera.oscura` | 3,0 | `#5f3b24` | `#593820` | `#543528` | `#5d4134` | Detalles, juntas, madera mojada. |
| `madera.rojiza` | 4,0 | `#8e4a30` | `#89452a` | `#804637` | `#8b5343` | Maderas nobles tropicales, remos. |
| `madera.deriva` | 5,0 | `#a69c8a` | `#9e9987` | `#9d948b` | `#a6a199` | Madera de deriva gris plata (playa). |
| `madera.corteza` | 6,0 | `#6d563e` | `#66533b` | `#634f41` | `#6c5b4e` | Troncos sin labrar, leña. |
| `madera.quemada` | 7,0 | `#3f2e24` | `#392b21` | `#372824` | `#3e3330` | Carbón, madera quemada, hoguera. |
| `palma.paja` | 0,1 | `#dbb866` | `#d4b45d` | `#cfb074` | `#d8be81` | Techo de paja (PalmThatch). |
| `palma.dorada` | 1,1 | `#c8973f` | `#c29333` | `#ba9052` | `#c59e5f` | Paja al sol, cestería. |
| `palma.seca` | 2,1 | `#a88a4e` | `#a18648` | `#9d8358` | `#a69065` | Hoja seca caída, estera vieja. |
| `palma.verde` | 3,1 | `#8ea247` | `#849f3e` | `#889855` | `#8fa662` | Hoja de palma recién cortada. |
| `palma.tierna` | 4,1 | `#bac568` | `#b0c25f` | `#b3bb76` | `#baca83` | Brote, hoja joven. |
| `palma.fibra` | 5,1 | `#c1a071` | `#ba9c6b` | `#b59878` | `#bfa685` | Cuerda de coco, bramante (Rope). |
| `palma.coco` | 6,1 | `#7c5431` | `#76502c` | `#704e37` | `#7a5a44` | Cáscara de coco. |
| `palma.pulpa` | 7,1 | `#f0e6cf` | `#e7e2cc` | `#e7ddd0` | `#f0ecdf` | Pulpa de coco, interior. |
| `bambu.verde` | 0,2 | `#8ba644` | `#80a339` | `#869c54` | `#8caa61` | Caña viva. |
| `bambu.verde_oscuro` | 1,2 | `#607a30` | `#557728` | `#5c713b` | `#617e48` | Caña vieja, sombra. |
| `bambu.maduro` | 2,2 | `#cab05d` | `#c3ac54` | `#bfa86b` | `#c8b678` | Caña curada (Bamboo). |
| `bambu.seco` | 3,2 | `#b1985c` | `#aa9456` | `#a69066` | `#af9e73` | Caña seca, balsa gastada. |
| `bambu.nudo` | 4,2 | `#7d6937` | `#766632` | `#736240` | `#7c6e4c` | Nudos y anillos. |
| `bambu.interior` | 5,2 | `#e3d5a6` | `#dbd2a1` | `#d9ccab` | `#e2dbb9` | Corte de caña, tablillas. |
| `piedra.basalto` | 0,3 | `#5c5360` | `#54505f` | `#554c5b` | `#5c586a` | Basalto del Humo (= terreno VolcanicRock). |
| `piedra.basalto_claro` | 1,3 | `#7d7481` | `#757180` | `#756c7c` | `#7d798b` | Caras al sol del basalto, lajas. |
| `piedra.caliza` | 2,3 | `#b1a893` | `#a8a590` | `#a89f94` | `#b1ada3` | Caliza de Los Dientes (= terreno Limestone). |
| `piedra.caliza_clara` | 3,3 | `#d1c9b5` | `#c8c6b2` | `#c8c0b5` | `#d1cec5` | Caliza lavada, bloques tallados. |
| `piedra.arenisca` | 4,3 | `#c19b6f` | `#ba9769` | `#b59475` | `#bfa183` | Arenisca, cerámica sin cocer. |
| `piedra.coral` | 5,3 | `#cbb99f` | `#c3b59b` | `#c1b1a1` | `#cabfb0` | Bloques de coral del muro polinesio. |
| `piedra.canto` | 6,3 | `#8d8a80` | `#84877e` | `#86827f` | `#8d8f8e` | Cantos rodados, piedra de afilar. |
| `piedra.obsidiana` | 7,3 | `#3d3741` | `#353440` | `#36303d` | `#3d3b4a` | Obsidiana, sílex oscuro (hachas). |
| `metal.hierro` | 0,4 | `#6f6f74` | = | = | = | Hierro forjado, clavos. |
| `metal.oxido` | 1,4 | `#8b5339` | = | = | = | Hierro oxidado, restos del avión. |
| `metal.cobre` | 2,4 | `#b9744b` | = | = | = | Cobre, cazos. |
| `metal.verdin` | 3,4 | `#6fa396` | = | = | = | Cobre con verdín, pecios. |
| `metal.bronce` | 4,4 | `#b6974b` | = | = | = | Latón y bronce, instrumentos. |
| `metal.acero` | 5,4 | `#9ba1a9` | = | = | = | Acero pulido, filos. |
| `metal.oro` | 6,4 | `#d7ae4c` | = | = | = | Oro, tesoros. |
| `metal.plomo` | 7,4 | `#46484f` | = | = | = | Plomo, hierro colado oscuro. |
| `tela.lona` | 0,5 | `#e5d6b3` | = | = | = | Vela y toldo (Canvas). |
| `tela.crudo` | 1,5 | `#cbbb97` | = | = | = | Lino crudo, sacos. |
| `tela.rojo` | 2,5 | `#b44b3d` | = | = | = | Tela teñida roja, banderas. |
| `tela.indigo` | 3,5 | `#405b87` | = | = | = | Tela índigo, ropa del náufrago. |
| `tela.ocre` | 4,5 | `#c88f3b` | = | = | = | Tela ocre, cúrcuma. |
| `tela.verde` | 5,5 | `#5b7f50` | = | = | = | Tela verde, mochila. |
| `tela.tapa` | 6,5 | `#b98f66` | = | = | = | Tapa (corteza batida polinesia). |
| `tela.cuero` | 7,5 | `#8a5a3b` | = | = | = | Cuero, correas. |
| `vegetacion.hoja` | 0,6 | `#418443` | `#2c823d` | `#447a4a` | `#468757` | Hoja por defecto. |
| `vegetacion.hoja_oscura` | 1,6 | `#38632a` | `#286126` | `#375a31` | `#3a663e` | Sombra de copa, selva. |
| `vegetacion.hoja_clara` | 2,6 | `#85a953` | `#79a64b` | `#829f5f` | `#87ad6c` | Hoja al sol, brotes. |
| `vegetacion.hierba` | 3,6 | `#619046` | `#538e40` | `#608650` | `#65945d` | Matas de hierba (= terreno Grass). |
| `vegetacion.hierba_seca` | 4,6 | `#b8a85d` | `#b0a555` | `#aea069` | `#b7ae76` | Hierba seca, cañizo. |
| `vegetacion.tronco_palma` | 5,6 | `#8d7454` | `#867150` | `#836d58` | `#8b7a65` | Tronco de palmera. |
| `vegetacion.musgo` | 6,6 | `#858a38` | `#7c872f` | `#7e8146` | `#858f53` | Musgo, liquen. |
| `vegetacion.flor_roja` | 7,6 | `#d04a3d` | `#cd4136` | `#bd4c48` | `#ca5954` | Hibisco (acento). |
| `vegetacion.flor_amarilla` | 8,6 | `#e6c13a` | `#e0bd22` | `#d9b95a` | `#e3c867` | Flor amarilla (acento). |
| `comida.mango` | 0,7 | `#e8983a` | = | = | = | Mango, papaya. |
| `comida.platano` | 1,7 | `#f0c04a` | = | = | = | Plátano maduro. |
| `comida.lima` | 2,7 | `#9cc24b` | = | = | = | Lima, fruta verde. |
| `comida.limon` | 3,7 | `#d6d940` | = | = | = | Limón (el del barco «Limón» y el escorbuto). |
| `comida.carne_cruda` | 4,7 | `#d9787a` | = | = | = | Carne y pescado crudos. |
| `comida.asado` | 5,7 | `#8b4a2f` | = | = | = | Carne asada, pan de fruta tostado. |
| `comida.pescado` | 6,7 | `#7b97ad` | = | = | = | Pescado plateado. |
| `comida.cangrejo` | 7,7 | `#d0563b` | = | = | = | Cangrejo y langosta cocidos. |
| `comida.taro` | 8,7 | `#9a7b8c` | = | = | = | Taro, raíces moradas. |
| `ui.tinta` | 0,8 | `#302b27` | = | = | = | Texto, tinta del mapa. |
| `ui.papel` | 1,8 | `#eee3c7` | = | = | = | Papel del mapa (MapPaper). |
| `ui.acento` | 2,8 | `#e0a041` | = | = | = | Resaltado, selección. |
| `ui.peligro` | 3,8 | `#c8443b` | = | = | = | Peligro, salud baja. |
| `ui.ok` | 4,8 | `#5ea05a` | = | = | = | Correcto, comestible. |
| `ui.info` | 5,8 | `#4f8ec0` | = | = | = | Información, agua. |
| `ui.neutro` | 6,8 | `#8c8579` | = | = | = | Deshabilitado. |
| `ui.hueso` | 7,8 | `#f1f0ec` | = | = | = | Blanco roto de la UI. |
| `terreno.arena_seca` | 0,9 | `#e7c691` | `#e7c691` | `#e6c592` | `#e6c692` | `T_SandDry_BC` teñido por M_Terrain |
| `terreno.arena_mojada` | 1,9 | `#ae8e65` | `#ae8e65` | `#ad8e66` | `#ad8e65` | `T_SandWet_BC` teñido por M_Terrain |
| `terreno.hierba` | 2,9 | `#639243` | `#629343` | `#649143` | `#639143` | `T_Grass_BC` teñido por M_Terrain |
| `terreno.basalto` | 3,9 | `#584e60` | `#574f60` | `#594e60` | `#584e60` | `T_VolcanicRock_BC` teñido por M_Terrain |
| `terreno.caliza` | 4,9 | `#b2a792` | `#b0a892` | `#b3a691` | `#b1a792` | `T_Limestone_BC` teñido por M_Terrain |
| `terreno.ceniza` | 5,9 | `#b5aaa4` | `#b6aaa4` | `#b5aaa6` | `#b4aaa6` | `T_Ash_BC` teñido por M_Terrain |

Fila 10 (`entorno.*`, solo referencia, lineal del motor): laguna `(0.30, 0.86, 0.80)`,
arrecife `(0.05, 0.45, 0.62)`, profundo `(0.02, 0.10, 0.22)`, espuma `(0.94, 0.97, 0.98)`
(`M_Ocean`) y cielo `(0.32, 0.52, 0.80)` (niebla de día de `ExploredSkyController`).

## Pendiente

- Verificar en Unreal: importación con kind `palette`, compilación de `M_LowPoly` (nodo
  Custom con `CalculateLevelOfDetail`/`Texture2DSampleLevel`) y las `MI_LowPoly_<Isla>`.
- Remapeo de UV de cada pack a las celdas (script de Blender o de importación) usando
  `alias_packs`.
- Comprobar la legibilidad en juego con Lumen (la hoja usa un sombreado simple).
