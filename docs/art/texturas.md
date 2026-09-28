# Texturas procedurales

Todas las texturas salen de `Tools/Textures/gen_textures.py` (Python + numpy + pillow) y
son **periódicas**: se repiten sin costura en u y v. La salida va a
`Art/Export/Textures/` (no se versiona) y `Tools/Unreal/import_textures.py` las importa a
`/Game/Generated/Textures`.

Nada sale de fotografías: el juego del terreno (`Grass`, `SandDry`, `SandWet`, `Dirt`,
`VolcanicRock`, `Limestone` y `ForestFloor`) está **pintado por código** en
`texgen/stylized.py` con los colores de [`paleta.json`](paleta.md), para que case con el
low poly; ver [«Juego estilizado del terreno»](#juego-estilizado-del-terreno-2026-09-28).
Hasta el 2026-09-28 la roca se fotobasheaba desde Poly Haven y la hierba imitaba un césped
fotográfico; el director las descartó («foto plana de juego de móvil que parece realista y
no lo es»).

```bash
# Todo a 1024 px (unos 2 min; el legado tarda ~20 s)
uv run --with numpy --with pillow python Tools/Textures/gen_textures.py
# Solo algunos materiales, a 2048, sin tocar el legado, con hoja de contacto
uv run --with numpy --with pillow python Tools/Textures/gen_textures.py --size 2048 --no-legacy \
    --only SandDry Grass --sheet docs/art/texturas-AAAA-MM-DD.png
# Juego estilizado del terreno con una hoja de contacto por material en docs/art/
uv run --with numpy --with pillow python Tools/Textures/gen_textures.py --no-legacy --only-stylized \
    --stylized-sheets docs/art
# Tests (tileado, rangos, paleta por isla, determinismo; ~25 s a 256 px)
cd Tools/Textures && uv run --with numpy --with pillow --with pytest python -m pytest -q tests
```

Hojas de contacto del terreno (una por material, 2026-09-28):
[`Grass`](texturas-estilizadas-Grass.png), [`SandDry`](texturas-estilizadas-SandDry.png),
[`SandWet`](texturas-estilizadas-SandWet.png), [`Dirt`](texturas-estilizadas-Dirt.png),
[`VolcanicRock`](texturas-estilizadas-VolcanicRock.png),
[`Limestone`](texturas-estilizadas-Limestone.png),
[`ForestFloor`](texturas-estilizadas-ForestFloor.png).

Hoja del catálogo completo: [`texturas-2026-09-27d.png`](texturas-2026-09-27d.png) (anterior
al juego estilizado: su hierba, arenas, hojarasca y rocas ya no son las actuales);
[`texturas-2026-09-27c.png`](texturas-2026-09-27c.png),
[`texturas-2026-09-27b.png`](texturas-2026-09-27b.png) y
[`texturas-2026-09-27.png`](texturas-2026-09-27.png) quedan como referencia histórica. Vista
iluminada en 2×2 para comprobar el tileado y, debajo, la misma vista **en 4×4 reducida**
(cómo se ve a media distancia: delata la repetición) + miniaturas BC / N / ARH. Si pasa de
~1.9 MB se guarda con 6 (o 5) bits por canal: una paleta global de 256 colores falseaba los
tonos.

## Estilo

Low-poly suave y cartoon vivo: paletas por material con rampas de 4–5 colores saturados
pero limpios, formas grandes legibles (ondas, matas, losas, tablas) y poco ruido fino. El
albedo nunca llega a negro ni blanco puros (0.035–0.95), la AO tiene suelo en 0.3 para no
ensuciar, y cada textura trae **variación macro** (1–3 ciclos por tile, más cálida/fría y
más clara/oscura) para que la repetición no se note a media distancia. El juego estilizado
del terreno la pone en 2–4 ciclos, sin el de 1 (ver su sección).

**Armonía con la paleta low poly.** El albedo de `SandDry`, `SandWet`, `Grass`,
`VolcanicRock`, `Limestone` y `Ash` sale armonizado con [`paleta.md`](paleta.md): su media
Oklab se lleva al tono y croma de la fila `terreno` de la paleta (el detalle y el tileado no
cambian), para que el terreno no desentone junto a los props de los packs CC0. El cambio
más visible es la hierba, que baja de croma 0.14 a 0.115.

## Convenciones

| Sufijo | Contenido | Importación |
|---|---|---|
| `_BC` | Color base, sRGB | sRGB, `TC_Default` |
| `_N` | Normal tangente, **DirectX** (verde = +V, la de Unreal) | lineal, `TC_Normalmap`, sin voltear verde |
| `_ARH` | R = oclusión, G = rugosidad, B = altura (0–1) | lineal, `TC_Masks` |
| `_M` | Máscaras RGBA (espuma) | lineal, `TC_Masks` |

El generador escribe `textures.json` (nombre → `kind`, `srgb`) y el importador lo usa, así
que no hay que tocar `import_textures.py` al añadir materiales.

## Catálogo

| Material | Tile | Uso y notas |
|---|---|---|
| `SandDry` | 2 m | **Estilizada.** Crema pintada en 3 valores siguiendo rizos de viento anchos (cresta clara, sotavento algo más tostado), trazo a lo largo del rizo y alguna concha rosada o de coral. Rugosidad 0,91–0,95. |
| `SandWet` | 2 m | **Estilizada.** Arena de orilla en bandas de resaca **paralelas a la orilla (a lo largo de u)**, dos por tile: línea fina de espuma, película oscura de agua (rugosidad 0,85, la más lisa del juego) que se seca hacia una franja escurrida donde asoma el tono de la arena seca. Caramelo tostado, nunca barro. **Orienta u paralelo a la línea de costa.** |
| `Grass` | 1.5 m | **Estilizada.** Hierba pintada: manchas de verde en 4 valores, trazo inclinado por el viento y 5 capas de matas en abanico (3 hojas en lanza por mata, base verde hondo y punta lima; alguna de paja). Sin la pelusa de césped fotográfico. Rugosidad 0,90–0,95. |
| `Dirt` | 2 m | **Estilizada, nueva.** Tierra de camino, claro o talud: pardo cálido en terrones redondeados con cara plana (sombreado low poly), juntas de corteza oscura, vetas de arcilla roja y migas sueltas. Aún no la usa ningún material (ver «Juego estilizado»). |
| `Moss` | 1 m | Musgo en cojines (ruinas, rocas, suelo de selva). |
| `GardenSoil` | 2 m | Tierra de huerto labrada: 5 camellones anchos y redondeados a lo largo de u (algo ondulados, anchura por hilera) entre surcos estrechos húmedos; terrones redondos en las laderas, migas, pocas pajas y guijarros y algún brote de dos hojitas. Cacao cálido (nunca negro ni gris). |
| `ForestFloor` | 1.5 m | **Estilizada.** Hojarasca pintada: hojas grandes en almendra en cuatro capas (las de abajo en sombra), dos tonos a cada lado del nervio como una hoja plegada; paja seca, dorada, teja y ~8 % aún verdes sobre tierra de corteza. |
| `Ash` | 2 m | Ceniza del Humo: mantos claros gris lavanda cálido (sin valles oscuros) con rizos de viento legibles y grumos redondeados y sueltos donde la lluvia apelmazó la ceniza (nunca una red de grietas ni losetas), pómez con volumen y poros, carbones escasos y alguna brasa con halo. |
| `VolcanicRock` | 3 m | **Estilizada.** Basalto low poly: caras planas grandes (Voronoi deformado con un plano inclinado por cara, escalonado en 4 valores) y una talla más pequeña encima; grietas finas de obsidiana y cantos altos en basalto claro. Gris violáceo frío. Sustituye al fotobasheado de `rock_face_03`. |
| `Limestone` | 3 m | **Estilizada.** Caliza en 5 estratos ondulados por tile partidos en bloques: repisa de arriba iluminada, base en sombra, chorreones de canto gris bajo las repisas y algo de musgo. Clara y cálida. Sustituye al fotobasheado de `marble_cliff_04`. |
| `PalmThatch` | 1 m | Techo de hoja de palma: 6 hileras de hebras largas y estrechas que cuelgan (dos capas desfasadas, sin huecos); cada hilera tapa la atadura de la de abajo y proyecta sombra con sus puntas desiguales, a veces rasgadas; tono por hebra (paja, dorado, ~10 % aún verdes, alguna tostada) y borde de hilera que ondula (atado a mano); **v = pendiente abajo**. |
| `PalmWeave` | 0.6 m | Estera de palma trenzada en diagonal (paredes, techos interiores, cestos). |
| `Bamboo` | 1 m | Cañas juntas con nudos; **v = a lo largo de la caña**; brillo (rugosidad ~0.4). |
| `WoodPlanks` | 2 m | Tablones largos (6 hileras, 1 o 2 juntas escalonadas), veta en arcos de corte plano que rodea los nudos (o recta), tono por tabla (miel, caramelo, rojizo, ~15 % gastadas por el sol), cantos redondeados y clavos; **u = a lo largo de la tabla**. |
| `StoneWall` | 2.5 m | Muro polinesio de piedra seca: basalto encajado con caras planas (low-poly), algún bloque de coral poroso, musgo en juntas y en la mitad baja de cada piedra. |
| `Canvas` | 0.5 m | Lona de vela/toldo: tafetán de 44 hilos/tile con matiz por hilo, una costura de paño a lo largo de u (solape elevado con sombra y doble pespunte, en v ≈ 0.37), descolorido del sol en bandas suaves y motas de sal; sin manchas grandes (se veían repetidas). Orienta u a lo largo de los paños. |
| `Rope` | 0.25 m | Cuerda de 3 cabos redondos con hilos en torsión contraria (arcos en «S»): **u = alrededor, v = a lo largo** (UV de cilindro). |
| `MapPaper` | 0.6 m | Papel del mapa: fibras cortas y largas, pulpa, ondulación, pocas manchas de agua de borde irregular con cerco, foxing de tamaño variable. |
| `Bark` | 1.5 m | Corteza de placas alargadas (4:1) con fisuras en V, grietas finas, crestas curtidas y liquen; **v = a lo largo**. |
| `WaterWaves` | 6 m | Solo `_N`: oleaje fino con dirección de viento y crestas algo afiladas. |
| `SeaFoam` | 6 m | Solo `_M`: R encaje de espuma, G burbujas, B masa suave, A estelas. |

«Tile» es el tamaño en mundo recomendado para un tile (en Unreal, 1 m = 100 uu); las
escalas están definidas en unidades de tile, así que 1024 y 2048 dan el mismo diseño.

## Texturas legado (no cambian)

`build_materials.py` usa hoy `T_TerrainDetail`, `T_TerrainNormal`, `T_LeafNoise`,
`T_WaterFoam` y `T_WaterRipple`. Se generan con el código original
(`Tools/Textures/texgen/legacy.py`) y salen **idénticas byte a byte** a 1024 px; los tests
comprueban nombres y canales. Ojo: `T_TerrainNormal` y `T_WaterRipple` tienen el verde en
convención OpenGL (a pesar de lo que decía el comentario antiguo). No importa mientras
`M_Terrain`/`M_Ocean` los lean con su HLSL propio (`.rg * 2 - 1`), pero no los conectes a
un pin Normal estándar sin invertir el verde.

## Juego estilizado del terreno (2026-09-28)

Encargo del director: fuera las fotos planas sin normales en hierba, hojas y suelo, pero
tampoco color mate liso; siempre algo de textura estilizada (variación pintada a mano,
trazo, relieve suave, normales sutiles).

### Inventario: qué texturas usan terreno, vegetación y rocas

| Material (`build_materials.py`) | Texturas | Origen | Estado |
|---|---|---|---|
| `M_Terrain` | `T_SandDry_BC/_N`, `T_SandWet_BC/_N`, `T_Grass_BC/_N`, `T_ForestFloor_BC/_N`, `T_VolcanicRock_BC/_N`, `T_Limestone_BC/_N`, `T_Ash_BC/_N` | `texgen/stylized.py` (salvo `Ash`, `texgen/materials.py`) | **Estilizadas.** Antes: césped fotográfico, hojarasca otoñal y roca fotobasheada de Poly Haven (`fetch_polyhaven.py`, `photobash.py`: eliminados). |
| `M_Terrain` (macro de mundo) | `T_TerrainDetail` | `texgen/legacy.py` | Sin cambios (ruido de variación, no se ve como textura). |
| `M_Rock` (rocas sueltas y acantilados) | `T_TerrainDetail`, `T_TerrainNormal` | `texgen/legacy.py` | Sin cambios: color por vértice y ruido; las mallas son del kit de `Tools/Blender`. |
| `M_Foliage*` | `T_FoliageAtlas_BC/_N` | `texgen/materials.py` | Sin cambios: atlas de cards ya pintado por código, con normal. |
| `M_Bark` | `T_BarkTropical_BC/_N/_ARH` | `texgen/materials.py` | Sin cambios: corteza pintada a mano por código. |
| `M_LowPoly` | `T_Palette_<Isla>` | `texgen/palette.py` | Sin cambios: es la paleta de la que beben las texturas nuevas. |
| — | `T_Dirt_BC/_N/_ARH` | `texgen/stylized.py` | **Nueva**; ningún material la carga todavía. |

### Cómo están hechas

- **Solo colores de la paleta.** Cada material pinta con unas pocas muestras de
  `paleta.json` (`PALETTES` en `stylized.py`; la hoja de cada material las enseña). Una
  muestra es su tramo Oklab abajo → medio → arriba, el mismo degradado que el atlas
  `T_Palette_<Isla>`. Cada píxel elige muestra y punto del tramo; el detalle, el trazo y la
  macro mueven ese punto, nunca suman ruido de color, así que el albedo no sale de la paleta.
- **Por isla.** Una textura sirve a todas las islas: `M_Terrain` la tiñe con el color de
  vértice de cada isla (`vertex_tint`). Por eso las capas del terreno usan la muestra
  `terreno.*` **sin teñir** (el objetivo de la paleta) y, ya teñidas, caen sobre la muestra
  `terreno.*` de cada isla. Los tests lo comprueban isla por isla.
- **Formas grandes y planos**, poca frecuencia fina: manchas escalonadas en 3–4 valores
  (`soft_bands`), matas en abanico, rizos de viento, terrones y facetas de roca con un plano
  inclinado por cara (el sombreado plano del low poly).
- **Trazo** anisótropo suave encima de todo (`brush`): nunca un color mate liso.
- **Normal suave.** Sale de una altura suave de pocos milímetros (z medio 0,97–0,998): se lee
  la forma, no el poro.
- **Rugosidad de suelo mate**, 0,85–0,95 en los siete; el extremo bajo solo en la película
  de agua de `SandWet`.
- **Macro-variación en 2–4 ciclos por tile, sin el de 1 ciclo.** A 50 m un tile de 2 m ocupa
  unos 40 px de pantalla y se ven decenas a la vez; una mancha por tile (1 ciclo) se leería
  como una rejilla. La variación de más escala la pone `M_Terrain` en coordenadas de mundo
  (`T_TerrainDetail` a 70 m y la segunda lectura girada de `TOP2`/`TRI2`). La hoja de cada
  material enseña un campo de 30 m a 5 cm/px, la escala de pantalla a unos 50 m.
- **Resolución independiente.** El ruido es `band_noise` (`texgen/noise.py`): los
  coeficientes se sortean en una rejilla canónica de 512² y se copian al tamaño de salida, así
  que 256, 1024 y 2048 px dan la misma textura. Con `spectral_noise`, que sortea el ruido
  blanco al tamaño de salida, los tests a 256 px validaban otra textura que la de 1024 px que
  se publica (mismas estadísticas, otras formas).

### Tests (`tests/test_stylized.py` y los comunes)

- Tileado sin costura en todos los canales, también tras cuantizar a 8 bits (`test_tiling.py`).
- Paleta por isla: el 98 % de los píxeles a menos de ΔE Oklab 0,025 de algún tramo de su
  paleta en la isla de referencia y 0,045 en las demás (el grado de la isla, ≤ 0,03, entra en
  el margen); con control negativo (hierba violeta, arena gris).
- Rugosidad en 0,85–0,95 antes y después de cuantizar; la resaca es la zona menos rugosa.
- Normales unitarias, suaves y presentes (ni foto plana ni roca esculpida).
- Nunca color mate liso; macro presente en 2–4 ciclos y < 25 % de la energía baja en 1 ciclo.
- Determinismo: misma semilla, mismos mapas; independiente del estado global de NumPy;
  otra semilla, otra textura; misma forma a 128 y 256 px.
- La roca ya no toca la red ni la caché de fotos.

### Reimportar y reconstruir en Unreal (local)

Los nombres `T_*` del terreno no cambian, así que `build_materials.py` no se toca; solo hay
que regenerar, reimportar y recompilar:

Todo de una vez (texturas, importación y materiales, sin mallas, audio ni mundo):

```powershell
powershell -File Tools\build_content.ps1 -Skip meshes,audio,world
```

O paso a paso:

1. `uv run --with numpy --with pillow python Tools/Textures/gen_textures.py` (1024 px, escribe
   `Art/Export/Textures/` y `textures.json`).
2. `powershell -File Tools\unreal_python.ps1 -Script Tools/Unreal/import_textures.py`:
   reimporta sobre los mismos assets de `/Game/Generated/Textures`; `T_Dirt_*` entra nueva.
3. `powershell -File Tools\unreal_python.ps1 -Script Tools/Unreal/build_materials.py` para
   recompilar `M_Terrain` con las texturas nuevas.
4. Comprobar en el editor, con capturas antes/después en las cuatro islas:
   - que la hierba, la arena y la hojarasca se leen pintadas y con relieve suave, sin
     rejilla a 50 m;
   - que `T_*_N` sigue importándose como `TC_Normalmap` sin voltear verde (DirectX);
   - que la roca en acantilado (triplanar `TRI`/`TRI2`) no enseña costuras entre ejes;
   - `M_Terrain` fija hoy la rugosidad por capa en HLSL (0,88–0,92; 0,3 en la arena
     mojada) y no lee `_ARH`: la rugosidad nueva de las texturas solo se verá cuando el
     material la lea (tarea «material del terreno con Roughness 0,85–0,95» del TODO).
   - `T_Dirt_*` no la usa ningún material: para caminos y claros hay que añadir su capa en
     `TERRAIN_LAYERS`/`TERRAIN_COLOR_HLSL` (escala `p / 2.0`), ojo con el tope de 16 samplers.

## Cómo conectarlas en los materiales

En `build_materials.py` las texturas se cargan con `texture_object(m, ruta, x, y)` y se
muestrean dentro de nodos `Custom` con HLSL. Patrón para un material de superficie (malla
con UV, p. ej. `M_Thatch`, `M_Planks`, `M_Bamboo`):

```python
bc = texture_object(m, "/Game/Generated/Textures/T_PalmThatch_BC", -1100, 0)
nm = texture_object(m, "/Game/Generated/Textures/T_PalmThatch_N", -1100, 200)
arh = texture_object(m, "/Game/Generated/Textures/T_PalmThatch_ARH", -1100, 400)
```

```hlsl
// Entradas: UV, BC, N, ARH (TextureObject) y Macro (escala lenta, p. ej. 0.23).
float4 c  = Texture2DSample(BC, BCSampler, UV);
// Romper la repetición: segunda lectura a escala lenta y girada, solo para el tono.
float2 uv2 = float2(UV.x * 0.8 - UV.y * 0.6, UV.x * 0.6 + UV.y * 0.8) * Macro;
float3 tone = Texture2DSample(BC, BCSampler, uv2).rgb;
float3 color = c.rgb * lerp(1.0, tone / max(dot(tone, 0.333), 1e-3), 0.35);
float3 a = Texture2DSample(ARH, ARHSampler, UV).rgb;   // a.r AO, a.g rugosidad, a.b altura
```

- `_BC` → Base Color (el sampler ya da lineal porque la textura es sRGB).
- `_N` → Normal: con `tangent_space_normal = True` conecta el `TextureSample` directo (tipo
  de muestreo *Normal*). En materiales triplanares con `tangent_space_normal = False`
  (como `M_Terrain`) usa el mismo esquema de `TERRAIN_NORMAL_HLSL`: `rg * 2 - 1` por eje de
  proyección; al ser DirectX, el componente y es +V.
- `_ARH.g` → Roughness; `_ARH.r` → Ambient Occlusion; `_ARH.b` → mezcla por altura.

**Mezcla por altura (suelos del terreno).** Para pasar de `SandDry` a `SandWet` en la orilla
(la máscara `WetSand` que ya calcula `M_Terrain`) o de `Grass` a `GardenSoil`, usa la altura
para que la transición siga el relieve en vez de un fundido plano:

```hlsl
float hA = Texture2DSample(ARH_A, ARH_ASampler, UV).b;
float hB = Texture2DSample(ARH_B, ARH_BSampler, UV).b;
float k = saturate((Mask * 1.6 - 0.3) + (hB - hA) * 0.8);   // Mask = WetSand, capa pintada...
k = smoothstep(0.35, 0.65, k);
```

**Terreno triplanar.** Para sustituir el detalle genérico de `M_Terrain` por suelos con
color propio, muestrea `_BC`/`_N` de cada capa con los mismos pesos `w` y escalas de
`TRIPLANAR_COMMON` (escala recomendada `p / tile_m`) y multiplica por el color de vértice
en vez de reemplazarlo, para respetar las paletas por isla.

**Selva.** `ForestFloor` va bajo los árboles: mézclalo con `Grass`/`Moss` con la máscara de
dosel (o la capa pintada de selva) usando la mezcla por altura de arriba; las hojas tienen
altura alta, así que asoman por encima de la hierba de forma natural. Escala `p / 1.5`.

**Agua.** `T_WaterWaves_N` puede sustituir a `T_WaterRipple` en `OCEAN_NORMAL_HLSL` (mismo
esquema de tres capas desplazándose; al ser DirectX, invierte `r.y` para igualar la
convención actual o simplemente ajusta `RippleStrength`). `T_SeaFoam_M` puede sustituir a
`T_WaterFoam` en `OCEAN_FOAM_HLSL`: `pattern = saturate(f.r * 0.6 + f.b * 0.4)` para la
orilla y `f.g` (burbujas) / `f.a` (estelas) para detalle en crestas.

## Atlas de paleta y `M_LowPoly` (packs CC0)

Los packs low poly (Kenney, KayKit, Quaternius) no usan estos juegos PBR: se colorean con
un atlas de paleta por isla, `T_Palette_<Isla>` (`Landing`, `Esmeralda`, `Humo`, `Dientes`),
definido en [`paleta.md`](paleta.md) y generado por `gen_palette.py` / `gen_textures.py`.
Los nombres de textura existentes **no cambian**: el atlas es una textura nueva y el
terreno armonizado conserva `T_SandDry_BC`, `T_Grass_BC`, etc. (mismos canales y resolución).

1. `gen_textures.py` escribe `T_Palette_<Isla>.png` y los registra en `textures.json` con
   `kind: "palette"`. `import_textures.py` los importa en sRGB, `TC_EditorIcon` (RGBA8 sin
   compresión por bloques), `TMGS_SimpleAverage`, `TF_Bilinear` y sin streaming.
2. `build_materials.py › build_lowpoly()` crea `/Game/Generated/Materials/M_LowPoly`:
   - `TextureObjectParameter` **`Palette`** (por defecto `T_Palette_Landing`): el parámetro
     de isla.
   - Nodo Custom que muestrea con UV0 limitando el mip a **`MaxMip`** (3, ver «sin mip
     bleeding» en `paleta.md`):
     ```hlsl
     float lod = min(Palette.CalculateLevelOfDetail(PaletteSampler, UV), MaxMip);
     float3 c = Texture2DSampleLevel(Palette, PaletteSampler, UV, lod).rgb;
     return c * lerp(1.0.xxx, saturate(VC.rgb), UseVertexColor);
     ```
   - `UseVertexColor` (0 por defecto) multiplica por el color de vértice para los packs
     que lo traen; `Roughness` 0.85, especular 0.3. Uso Nanite e instanciado como el resto
     (`finish`).
   - Una instancia por isla, `MI_LowPoly_<Isla>`, con su `T_Palette_<Isla>` en `Palette`.
     Un prop se coloca con la instancia de su isla; si un actor tiene que cambiar de isla
     en tiempo de ejecución, basta un `MaterialInstanceDynamic` que cambie `Palette`.
3. Las mallas de los packs llevan UV0 dentro de su celda (`paleta.json`: `uv` para el color
   exacto, `v_rango` para el degradado). Lo hace `Tools/Packs/normalize.py` en Blender
   (reglas `recolor` de `packs_catalogo.json`); `alias_packs` da la correspondencia por
   nombre de material.

Los paths de `build_lowpoly()` están escritos literales (`PALETTE_TEXTURES`) para que
`tests/test_contract.py` los valide contra el generador.

**Pendiente de verificar en Unreal** (en la nube no hay editor): la compilación del nodo
Custom y las propiedades de importación (`mip_gen_settings`, `filter`, `never_stream`).

## Añadir un material

1. Escribe `def mi_material(size, seed) -> Material` en `texgen/materials.py` usando las
   primitivas periódicas de `texgen/noise.py` (`spectral_noise`, `voronoi`,
   `scatter_dots`, `blur`, `ramp`, `macro_variation`). Para celdas alargadas (placas,
   vetas) usa `voronoi(..., nx, ny, isotropic=False)`: mide en unidades de celda y la
   celda sale con la proporción de la rejilla; `_facet_plane(vo, a, b)` da un plano
   inclinado por celda para caras planas tipo low-poly. Define todo en coordenadas de tile y
   no pongas juntas estructurales justo en el borde (usa una fase fraccionaria).
2. Regístralo en `MATERIALS` con su tamaño de tile y uso.
3. `pytest` comprueba tileado, rangos, determinismo, el contrato de nombres con
   `build_materials.py` (`tests/test_contract.py`: toda `T_*` que cargue un material debe
   salir del generador) y que la variación macro del albedo
   esté en rango (`tests/test_macro.py`: ni plano a lo lejos ni manchas que dominen el
   tile); `tests/test_palette.py` fija la intención de color de algunos materiales (basalto
   azulado y no marrón barro, hojarasca cálida, ceniza clara y neutra, caliza clara y
   cálida (no fría/azulada), tablones cálidos con tono distinto por hilera, techo de paja
   cálido hecho de hebras y no de escamas, tierra de huerto cacao que no llega a casi
   negro y se lee en hileras, lona clara con costura, césped donde el detalle de hoja domina
   sobre la mancha macro); `tests/test_palette_atlas.py` cubre la paleta, el atlas y la armonización del terreno;
`tests/test_sheet.py` comprueba que la hoja de contacto monta
   una tarjeta para cada material (también `FoliageAtlas`, con BC RGBA y sin ARH); genera la hoja de contacto y
   **mírala** (sobre todo la miniatura 4×4) antes de darlo por bueno. Para una capa del
   terreno, mejor en `texgen/stylized.py` con su entrada en `PALETTES` (y `band_noise`
   en vez de `spectral_noise`): así entra sola en `tests/test_stylized.py`.
