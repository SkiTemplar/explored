# Texturas procedurales

Todas las texturas salen de `Tools/Textures/gen_textures.py` (Python + numpy + pillow, sin
imágenes externas) y son **periódicas**: se repiten sin costura en u y v. La salida va a
`Art/Export/Textures/` (no se versiona) y `Tools/Unreal/import_textures.py` las importa a
`/Game/Generated/Textures`.

```bash
# Todo a 1024 px (unos 2 min; el legado tarda ~20 s)
uv run --with numpy --with pillow python Tools/Textures/gen_textures.py
# Solo algunos materiales, a 2048, sin tocar el legado, con hoja de contacto
uv run --with numpy --with pillow python Tools/Textures/gen_textures.py --size 2048 --no-legacy \
    --only SandDry Grass --sheet docs/art/texturas-AAAA-MM-DD.png
# Tests (tileado, rangos, determinismo; ~10 s a 256 px)
cd Tools/Textures && uv run --with numpy --with pillow --with pytest python -m pytest -q tests
```

Hoja de contacto actual: [`texturas-2026-09-27.png`](texturas-2026-09-27.png): vista iluminada
en 2×2 para comprobar el tileado y, debajo, la misma vista **en 4×4 reducida** (cómo se ve a
media distancia: delata la repetición) + miniaturas BC / N / ARH.

## Estilo

Low-poly suave y cartoon vivo: paletas por material con rampas de 4–5 colores saturados
pero limpios, formas grandes legibles (ondas, matas, losas, tablas) y poco ruido fino. El
albedo nunca llega a negro ni blanco puros (0.035–0.95), la AO tiene suelo en 0.3 para no
ensuciar, y cada textura trae **variación macro** (1–3 ciclos por tile, más cálida/fría y
más clara/oscura) para que la repetición no se note a media distancia.

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
| `SandDry` | 2 m | Arena seca de playa/dunas: rizos eólicos asimétricos, granos, conchas y guijarros. |
| `SandWet` | 2 m | Franja de orilla: caramelo saturado (no gris), rizos lavados, arena empapada alrededor de láminas de agua brillantes (rugosidad ~0.2, tinte leve de cielo), marcas de resaca con banda escurrida detrás, agujeritos de cangrejo y alguna concha. |
| `Grass` | 1.5 m | Césped cartoon: 7 capas de hojas afiladas que siguen un flujo suave + florecillas. |
| `Moss` | 1 m | Musgo en cojines (ruinas, rocas, suelo de selva). |
| `GardenSoil` | 2 m | Tierra de huerto labrada: surcos en el eje u, terrones, paja, surcos más húmedos. |
| `ForestFloor` | 1.5 m | Hojarasca del suelo de selva: 6 capas de hojas caídas en lanza (ocre, teja, marrón y ~10 % aún verdes) con nervio central y borde algo curvado; las capas de abajo más oscuras (profundidad sin negro), ramitas y tierra en los huecos. |
| `Ash` | 2 m | Ceniza del Humo: mantos claros gris lavanda con rizos de viento asimétricos, placas de costra grandes y biseladas (bandejas, no garabatos de grietas), pómez con volumen y poros, pocos carbones angulosos y alguna brasa con halo. |
| `VolcanicRock` | 3 m | Basalto pizarra azulado/violeta en bloques de ~60 cm con caras facetadas (low-poly) y cantos biselados que atrapan la luz, juntas estrechas, vesículas en racimos, óxido cálido cerca de las juntas y granos de olivino. |
| `Limestone` | 3 m | Caliza clara crema en losas grandes de canto redondeado y caras algo facetadas (low-poly), repisas de estrato suaves (sin grietas finas oscuras), alveolos de disolución en racimos y costras de liquen naranja/salvia con borde neto. |
| `PalmThatch` | 1 m | Techo de hoja de palma: 6 hileras de hebras largas y estrechas que cuelgan (dos capas desfasadas, sin huecos); cada hilera tapa la atadura de la de abajo y proyecta sombra con sus puntas desiguales, a veces rasgadas; tono por hebra (paja, dorado, ~10 % aún verdes, alguna tostada) y borde de hilera que ondula (atado a mano); **v = pendiente abajo**. |
| `PalmWeave` | 0.6 m | Estera de palma trenzada en diagonal (paredes, techos interiores, cestos). |
| `Bamboo` | 1 m | Cañas juntas con nudos; **v = a lo largo de la caña**; brillo (rugosidad ~0.4). |
| `WoodPlanks` | 2 m | Tablones largos (6 hileras, 1 o 2 juntas escalonadas), veta en arcos de corte plano que rodea los nudos (o recta), tono por tabla (miel, caramelo, rojizo, ~15 % gastadas por el sol), cantos redondeados y clavos; **u = a lo largo de la tabla**. |
| `StoneWall` | 2.5 m | Muro polinesio de piedra seca: basalto encajado con caras planas (low-poly), algún bloque de coral poroso, musgo en juntas y en la mitad baja de cada piedra. |
| `Canvas` | 0.5 m | Lona de vela/toldo: tafetán de 44 hilos/tile con matiz por hilo, zonas descoloridas por el sol y pocas manchas de sal con cerco fino. |
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
   azulado y no marrón barro, hojarasca cálida, ceniza clara y neutra, caliza clara y crema, tablones cálidos con tono distinto por hilera, techo de paja cálido hecho de hebras y no de escamas); genera la hoja de contacto y **mírala** (sobre todo la miniatura 4×4) antes de
   darlo por bueno.
