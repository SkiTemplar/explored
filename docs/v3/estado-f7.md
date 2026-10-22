# Baliza Siete: estado de F7 (pulido y paquete, tramo sin motor)

Fecha: 2026-10-21 · rama `reset/v2` · sin abrir Unreal (el PC estaba en uso): todo lo que necesita
el editor o el juego está en [`pendiente-motor.md`](pendiente-motor.md), una sola lista ordenada
(pasos 10-12 para rendimiento y paquete).

## Criterio de salida de F7 (plan.md §4)

| # | Criterio | Estado en este tramo |
|---|---|---|
| 1 | ≥ 60 fps de media y ≥ 45 de mínimo a 1080p (`Tools/bench.ps1`, 3 zonas y ciclón) | Recorrido y veredicto listos en código; medida PENDIENTE (motor) |
| 2 | `Tools/package.ps1` genera un Win64 que arranca y el bot completa 3 partidas sin fallo | Script y cocinado listos; Shipping compila; paquete y partidas PENDIENTE (motor) |
| 3 | `/codex:adversarial-review` sin CRITICAL ni HIGH | No abordado en este tramo |
| 4 | Cobertura ≥ 80 % en modelos nuevos; HostTests con sanitizers y CI verdes | HostTests 812 casos, 0 fallos (13 de `BenchBudgetSpec`; 2 de la entrada del final en `FinaleSequenceSpec`); sanitizers solo en CI (en Windows falta `stl_asan.lib`) |
| 5 | Tamaño del paquete registrado (orientativo ≤ 1,2 GB con audio) | PENDIENTE (motor) |

## Hecho

### Rendimiento: recorrido `-ExploredBench`

- `UExploredShotSubsystem`, conjunto `bench` (solo con `-ExploredBench`): cinco vistas de
  Map_IslaSal, con los encuadres de la pasada f4b donde coinciden: `bench-cala` (Cala Varada desde
  el mar, 10:30), `bench-palmar` (a la altura de los ojos, 11:00), `bench-faro` (base del faro desde
  la Colina, 15:00), `bench-faro-galeria` (galería del faro mirando a la Cala por encima del Palmar,
  el plano con más isla y mar, 17:00) y `bench-ciclon` (muro 1 del ciclón, D2 20:30, desde la orilla
  de la Cala). Sustituye a las tres posiciones genéricas anteriores (aparición, orilla, aérea).
- En cada vista, tras el asentamiento de streaming de siempre, mide 12 s con un barrido de cámara
  de ±30° (`FApp::GetDeltaTime`, el tiempo real del fotograma). La medida no se para si el barrido
  pide streaming: esos tirones cuentan. Luego vuelca `stat unit/gpu/rhi/streaming`,
  `r.Nanite.ShowStats`, VRAM de texturas y `memreport -full`, y guarda la captura.
- `FBenchBudgetModel` (modelo puro, `Debug/BenchBudgetModel.*`, spec `Tests/BenchBudgetSpec.cpp`
  en los host tests): media = fotogramas / tiempo (no la media de los fps), mínimo = fotograma más
  lento, 1 % bajo informativo, mínimo de 30 fotogramas por vista y veredicto por vista y global.
  Líneas de log: `[Bench] fps <vista>: media …, mínimo …, 1 % …, OK|FALLA` y
  `[Bench] Resumen: … OK|FALLA (fuera de presupuesto: …)`.
- `Tools/bench.ps1`: falla si no llega el resumen o dice `FALLA`; `-Exe` para medir sobre un
  paquete Development. Comando y objetivo en `pendiente-motor.md`.

### Paquete

- `Config/DefaultGame.ini`: `MapsToCook` con Map_IslaSal (también mapa por defecto y de arranque
  en `DefaultEngine.ini`); `DirectoriesToAlwaysCook` añade `/Game/Art` (kits K1-K5, `Art/Island`,
  `Art/Materials` con `M_Toon`, `M_RainStreak` y `M_BeaconBeam` cargados por ruta, las mallas de
  `island_decor.json` y el futuro `/Game/Art/K6_Cesped`) a `/Game/Maps`, `/Game/Generated/Audio`,
  `/Game/Audio` y `/Game/Materials`; `DirectoriesToNeverCook` para `/Game/Developers`. `Data` sigue
  como NonUFS (todos los lectores usan `FFileHelper` sobre `ProjectContentDir()/Data`, así que
  `grass.json` entrará solo) y `Slate` como UFS.
- Título de la ventana y del proyecto: «Baliza Siete» (`ProjectDisplayedTitle`, descripción del
  `.uproject`); traducción y localización regeneradas (`l10n export --check` y `l10n --strict`
  verdes; se marcaron con `// loc: ignorar` dos literales del bot que solo van al log).
- `Tools/package.ps1`: DataCheck `--strict`, compila editor y juego con `-WaitMutex` y llama a
  `BuildCookRun -skipbuild` (con `-build` choca con el mutex de UBT si otro proceso compila), con
  `-map=/Game/Maps/Map_IslaSal`, `-prereqs`, `-nodebuginfo` en Shipping y
  `-AdditionalCookerOptions=-DisablePlugins=UnrealMCPython`. Al final comprueba ejecutable,
  `.pak`/`.utoc` y JSON sueltos, y escribe el tamaño.
- `Tools/playtest_v3.ps1 -Exe`: el bot juega sobre un paquete Development (criterio 2).

### Compilación Shipping

- `Tools/build.ps1 -Target Game -Configuration Shipping`: `Result: Succeeded`, 0 errores y 0 avisos
  en código del proyecto (los C4996 son de cabeceras del motor).
- Mismo objetivo sin Unity build (`UnrealBuildTool … -DisableUnity`): 224 ficheros, 0 errores; ningún
  `.cpp` depende de includes de sus vecinos de bloque.
- Barrido de símbolos en espacios de nombres anónimos y `static` de fichero entre `.cpp`: 69
  símbolos, ninguno repetido. Los espacios de nombres con nombre repetidos (`ExploredSaveStates`)
  son definiciones de funciones declaradas en cabecera, no choques.
- Código solo de editor: `SetActorLabel` está tras `WITH_EDITOR`; depuración (bot, auditor,
  capturas, smoke tests, comandos `Baliza*`, vuelo F8) tras `UE_BUILD_SHIPPING`. Ningún `check()`
  lleva efectos secundarios (en Shipping desaparecen).
- Development Editor: `Tools/build.ps1` verde.

En resumen: no apareció ningún error de Shipping que corregir; el tramo deja la comprobación
(Unity, sin Unity y barrido) como hecha.

### Documentación

- `README.md`: premisa, controles (sacados de `AExploredCharacter::RebuildKeyMappings`,
  `SettingsLogic::GetRemappableActions`, `AExploredPlayerController::SetupInputComponent` y
  `AExploredRaft::BuildInputAssets`), requisitos, compilar, probar y empaquetar.
- `CLAUDE.md`: «Qué es», «Stack» y «Cómo compilar / ejecutar» describen el juego real; «Tests» sin
  tocar.

### Revisión final (corregido sin motor)

- **Final:** el epílogo quitaba `bCutsceneActive` y `HandleEscape` no miraba `bShowingEnding`; Start
  del mando abría la pausa, `HideOverlay` borraba el epílogo y al reanudar la cámara quedaba fija.
  Ahora `FFinaleSequenceModel::CanOpenPause` bloquea la pausa en la cámara del final, el epílogo y
  los créditos, y `SExploredEnding` consume Escape, B y Start (`IsEpilogueContinueKey`), con spec.
- **Pesca de v1 (F/R/T):** lanzaba `UFishingComponent`, que generaba ids de v1 inexistentes (actores
  invisibles huérfanos) y escribía en `FFishingSaveState`. Borrados el componente, `IA_Fish`,
  `IA_Reel` y sus mapeos (también los gatillos del mando). La pesca es `AExploredFishingSpot` +
  `FCastFishingModel` (tecla E en las Rocas del Pescador); `UExploredFishingSubsystem` se queda para
  cargar partidas guardadas.
- **H (reloj de pulsera):** solo ponía `bWristWatchRaised`, que no leía nadie. Borrados `IA_Watch`,
  su mapeo (H y cruceta arriba) y el estado. F, R, T y H ya no son teclas reservadas: se pueden
  asignar en Ajustes → Controles. Siguen fijas B, X y Z (barco antiguo `AExploredBoat`, que no está
  en Map_IslaSal; también usa R, T y F en su propio contexto).
- **Linterna:** si el encendido se interrumpía o no prendía, el tick se quedaba cada fotograma;
  vuelve a `IdleTickSeconds`.
- **«Duración del día»:** se guardaba pero no se aplicaba (el día dura siempre 26 min). Quitada de
  Ajustes → Juego; `DayLengthMinutes` sigue en `UExploredGameUserSettings` para leer los ini antiguos.
- **Lluvia:** `UpdateRain` creaba cada fotograma un array de 900 transformaciones; ahora reutiliza un
  búfer miembro.
- **Altura del suelo:** el auditor del bot, los puntos de la ruta del bot y las capturas usaban
  `SampleColumn` (hasta 3,4 m de la malla). Ahora usan `ExploredGround::HeightAtMeters`
  (`ground_field.bin`, el mismo lector que el césped; trazado contra lo estático si falta).

Los controles del `README.md` no cambian: no listaban F, R, T ni H.

## F8: césped continuo y arena estilizada (2026-10-22)

Encargo: el césped K6 se leía como matas sueltas sobre suelo desnudo y la arena como una superficie lisa
(`investigacion-cesped-arena.md`). Hecho:

- **Alfombra de parches.** `FGrassFieldModel` siembra `SM_GrassPatch_A/B/C_01` a 5,5-6 por m² (zonas en
  `grass.json`, versión 2), con flores, tréboles, helechos y hierba alta como acentos (≈ 4 % en la mezcla
  común, 29 % de hierba alta en bordes de camino, 20 % de helechos al pie de las palmeras). Escala
  0,8-1,3, giro al azar. Mismas exclusiones que antes.
- **Adelgazamiento continuo sin LOD** (`FGrassThinning`, `WorldGen/GrassThinning.*`): f(d) = 1 hasta
  25 m, (25/d)² hasta 75 m y fundido a 0 en 75-100 m. Cada parche lleva un rango (dato por instancia 0);
  `M_ToonCesped` lo encoge a 0 cuando f(d)·1,2 baja de él y agranda los que quedan (hasta ×2,6 en
  planta y ×1,6 en altura). Las celdas (8 m) guardan niveles f_k = 0,5^k (8 niveles): suben 4 m antes de
  hacer falta generando solo la franja de rangos que falta y bajan con 3 m más de histéresis quitando
  del final, así que nada aparece ni desaparece con tamaño (spec «Sin saltos»: 1 800 frames caminando,
  0 parches que saltan). Una LOD por malla (`import_props.py` falla si hay más) y sin Nanite.
- **Suelo bajo el césped** (`M_TerrainToon`, alfa del color por vértice = tipo de suelo desde
  `Tools/Island`): verde de la base de las hojas cerca y verde de la alfombra lejos con la misma f(d)
  (`MPC_Terreno`); las hojas lejanas tienden a ese mismo verde. Medido en `f8-cesped-lejos`: alfombra
  (124-130, 157-164, 84-88) frente a suelo (120, 153, 85) en sRGB.
- **Sin sombras** en todo el césped (también hierba alta y helecho; DataCheck lo exige), sin colisión,
  viento apagado entre 30 y 45 m. El actor ya no recrea el proxy del ISM en cada cambio (solo sube las
  ranuras tocadas).
- **Arena** (`M_TerrainToon`, sin texturas): ondas de viento de 14 cm con crestas onduladas por ruido
  suave y repartidas en manchas, dos tonos (0,91-1,02) y su tono medio de lejos (12-30 m); grano de
  1,5 cm hasta 9 m; brillos escasos (8 % de celdas de 1,2 cm, umbral R·V 0,992, más fuertes de cara al
  sol; `AExploredSkyController` escribe `SunDirection`/`SunGlint` en `MPC_Terreno`); orilla por altura:
  mojada hasta 0,32 m (×0,76, algo especular), borde claro y espuma fina que late, reducida en los bajíos
  planos.
- **Manchas blancas de la playa:** eran la espuma de orilla de `M_Ocean`, calculada con la profundidad del
  relieve analítico (se separa de la malla hasta ~3 m): en las playas de poca pendiente pintaba bandas
  blancas planas de varios metros. `ShoreFoam` = 0 en `M_Ocean`; la espuma la dibuja el terreno por su
  altura real (`f8-arena-cala` frente a `f6-relieve-cala`).

Cifras (Development Editor, `-game`, 1080p):

| Medida | Valor |
|---|---|
| Triángulos de césped a la vista (peor dirección, host `GrassRealData`) | 1,10-1,26 M (tope 1,5 M) |
| Parches y matas vivos | 22 000-55 000 según la zona, en ~590 celdas |
| Siembra al caminar (host, 5 m/s) | 0,10 ms de media, peor frame 1,7 ms |
| Siembra al caminar (juego, `f8-cesped-paseo-*`) | 0,015 ms de media, máx. 0,81 ms (con la secuencia nueva de 8 fotogramas: 0,023-0,031 ms) |
| Relleno tras un teletransporte | 80-175 frames, 0,3-0,45 ms de media; el primer frame llega a 6-8 ms (oculta las ranuras de todas las celdas que se van) |
| `bench.ps1` | cala 197,0 / 99,8 · palmar 158,7 / 93,5 · faro 182,0 / 109,8 · galería 137,5 / 88,4 · ciclón 228,2 / 127,7 (media / mínimo); resumen 180,7 / 88,4: OK |

No hay cifra de bench anterior con el césped viejo (el paso 10 de `pendiente-motor.md` no se había
medido), así que no hay «antes» que comparar; no hizo falta bajar densidad ni radio.

### F8 tras la crítica de arte (2026-10-22)

La crítica de las capturas f8 pedía diez cambios (tres bloqueantes). Hecho, con las capturas `f8-*` rehechas
(tres vueltas) y mirado cada encuadre:

- **Hojas (bloqueante).** `SM_GrassPatch_A/B/C_01` rehechos en `k6_parches.py` (d797d953: semiancho de
  1,5-3 cm, 8:1, inclinación más curvatura < 65°) y reimportados en K6. De cerca ya se lee pasto
  (`f8-cesped-cerca`).
- **Facetado de las hojas.** `M_ToonCesped`: la misma normal en las dos caras (la del hemisferio de arriba,
  sin el signo de la cara) y `UpBias` 0,85 (antes 0,6). El sombreado lo lleva el degradado base→punta.
- **Color.** La punta baja de 1,04 a 0,92 y el césped se satura un 25 % (`GRASS_SATURATION`), también en el
  suelo bajo la hierba. Las matas K1 (`SM_GrassClump_01`) dejan de colocarse en `island_decor.json`: se
  leían como manchas de otra especie y, en la arena, como matas sueltas.
- **Bordes de la alfombra.** `FGrassFieldModel`: junto a caminos, exclusiones y suelo que no es hierba
  (anillos de 6 muestras a 0,5, 1 y 1,5 m del campo de suelo), la densidad baja de 1 a 0 y la altura de
  1 a 0,3 en `edgeFadeM` = 1,5 m (`edgeMinHeight` 0,3, en `grass.json`, `FGrassInstance::HeightScale`).
  Los claros se funden en su borde. Spec «el borde de la alfombra se funde en densidad y altura». Lejos,
  el suelo bajo la hierba se iguala del todo al verde de la alfombra (`GroundMatch` → 1 cuando f(d) → 0).
- **Espuma (bloqueante).** `M_TerrainToon`: banda continua de 0,25-0,6 m en horizontal desde la línea del
  agua (altura / pendiente de la normal suave), con ruido suave girado (`Noise.Smooth`, sin la cuadrícula del
  ruido de valor que, umbralizado, salía en rectángulos) y opacidad máxima del 40 %.
- **Duna → hierba (bloqueante).** Los triángulos oscuros de borde recto eran **sombras del propio terreno**
  con el sol bajo (lomas de triángulos de 1,8 m): `map_post.py` quita la sombra proyectada de
  `SM_IslaSal_Terrain` (la sigue recibiendo). Además, la normal de cara pesa un 25 % (antes 40 %) y un 6 % en
  la arena y cuatro celdas a su alrededor (`SAND_FACET_MIX`); los pesos de arena y hierba se afilan con
  ruido de mundo (borde orgánico, no por triángulo), y las ondas solo van donde hay arena.
- **Ondas.** Fase deformada por ruido de 4,5 y 1,7 m, contraste del 6,5 % (0,955-1,02), apagadas a 40 m y
  cuando la banda baja de ~3 px (estimación del píxel en el suelo; `fwidth` hacía fallar la compilación de
  `M_TerrainToon` en SM6 y el juego usaba el material por defecto).
- **Brillos.** En el color base (la luz los apaga en la sombra), celdas de 2,5 cm, densidad 0,008 (una
  décima), microfaceta de ±19° y umbral R·V 0,995: solo de cara al sol, sobre arena seca.
- **Capturas.** f8 con cielo despejado (las vistas de las 15:00 caían en el chubasco del día 1: ahora 13:30),
  `f8-cesped-f7cala`, `-f7faro` y `-f7cenital` con los encuadres de f7, y `f8-cesped-paseo-1…8`: 8 fotogramas
  cada 3 m por un prado llano y despejado (0,4 m de desnivel en 60 m); no se ve nada aparecer ni saltar.
- **Rocas de caja.** Los módulos `SM_CliffRock_01` sueltos (8 × 4 × 6 m, trasera plana) se leían como cajas
  de lados azul oscuro y tapa verde. En la colina ya no se colocan; en las Rocas del Pescador son peñascos
  `SM_RockBig_01` (escala 1,6-2,3). Los cuatro «acantilado» de `island_props.json` (farallones del Pescador
  y del islote) usan `SM_RockBig_01` con `meshScale` [1,6, 1,6, 2,1] (campo nuevo de tipo, opcional).
- **Lámina gris de `f8-cesped-cerca`.** No se ha identificado con certeza: lo más probable es una espiga
  `hierba_seca` de `SM_GrassTall_01`. Con la saturación nueva ya no se lee gris.

Cifras tras los cambios:

| Medida | Valor |
|---|---|
| Triángulos de césped a la vista (host `GrassRealData`) | 1,02-1,18 M (antes 1,10-1,26) |
| `bench.ps1` | cala 198,7 / 110,1 · palmar 156,9 / 91,3 · faro 192,7 / 111,3 · galería 144,0 / 93,2 · ciclón 235,3 / 130,7 (media / mínimo); resumen 185,5 / 91,3: OK |
| Host tests | 822 casos, 0 fallos |

Pendiente de criterio: la orilla de `f8-arena-orilla` deja una franja de arena mojada entre el agua y la
espuma cuando la ola baja (el mar se mueve y la espuma va por altura fija); la arena de `f8-arena-sol`
sigue con ondas visibles a contraluz.

## Hallazgos sin corregir (decisión de diseño o de otro tramo)

- **`Content/Audio` y `Content/Generated` sin versionar** (411 MB y 274 MB en disco local). Un
  clon limpio cocina sin música ni efectos. Decidir si van por LFS antes de entregar.
- **La balsa no responde al mando** (solo W, A, D, E); fuera de alcance según el GDD §9.
- **`Tools/Textures` no tiene `pyproject.toml`** aunque la sección «Tests» de `CLAUDE.md` lo cita;
  es carpeta del tramo «Relieve» y no se ha tocado.
- Del plan de F7 quedan sin abordar: optimización (LOD, distancia de dibujado), revisión de
  seguridad del guardado (`SaveArchive`), cangrejo ambiental y revisión adversarial.

## Pendiente de motor

Ver [`pendiente-motor.md`](pendiente-motor.md), pasos 10-12 de la lista única: bench, paquetes
Shipping y Development, arranque de ambos, tres partidas del bot y bench sobre el paquete, y tamaño.
