# Baliza Siete: estado de F7 (pulido y paquete, tramo sin motor)

Fecha: 2026-10-21 · rama `reset/v2` · sin abrir Unreal (el PC estaba en uso): todo lo que necesita
el editor o el juego está en [`pendiente-motor.md`](pendiente-motor.md), una sola lista ordenada
(pasos 10-12 para rendimiento y paquete).

## Criterio de salida de F7 (plan.md §4)

| # | Criterio | Estado en este tramo |
|---|---|---|
| 1 | ≥ 60 fps de media y ≥ 45 de mínimo a 1080p (`Tools/bench.ps1`, 3 zonas y ciclón) | Editor: OK (2026-10-22 por la mañana, 199,8 / 116,7 fps tras quitar dos tirones de un fotograma; ver «Rendimiento en el editor, tanda del 2026-10-22 por la mañana»). Sobre el paquete Development: OK (2026-10-22, 191,7 / 98,4 fps, por encima del editor medido a continuación, 183,4 / 92,6; ver «Paquete Development, bot y bench sobre el paquete») |
| 2 | `Tools/package.ps1` genera un Win64 que arranca y el bot completa 3 partidas sin fallo | Shipping: OK (2026-10-22, empaqueta y arranca al menú; ver «Paquete Shipping»). Development: OK (2026-10-22, arranca al menú y el bot completa competente, lenta y sin_paginas en VERDE sobre el paquete; ver «Paquete Development, bot y bench sobre el paquete») |
| 3 | `/codex:adversarial-review` sin CRITICAL ni HIGH | NO CUMPLIDO: `codex exec` no puede ejecutar nada en el sandbox de Windows (ver «Huecos de completitud»); revisión manual del guardado hecha |
| 4 | Cobertura ≥ 80 % en modelos nuevos; HostTests con sanitizers y CI verdes | HostTests 836/836 normal, con ASan + UBSan y con matemáticas rápidas (WSL); cobertura C++ 91,8 % de líneas, modelos v3 92-100 %. CI sin ejecutar: `reset/v2` sin empujar |
| 5 | Tamaño del paquete registrado (orientativo ≤ 1,2 GB con audio) | OK: Shipping 604 MB con audio (2026-10-22, rehecho en el cierre desde `HEAD`: `Saved/Package/Windows`); Development 1.193 MB, de ellos 385 MB de `.pdb` (`Saved/Package-Development/Windows`) |

Resultado del cierre (2026-10-22, ver «Cierre de la entrega»): criterios 1, 2 y 5 cumplidos; 3 sin
cumplir y 4 a medias (sin CI). La tarjeta de F7 del kanban se queda en curso.

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

## Pasada de diseño P1-P10 (2026-10-22)

Decisiones aceptadas por el diseñador, un commit por elemento (detalle en cada mensaje):

- **P1 objetivos** (`2633cb62`): textos del HUD con materiales y hora, ≤ 60 caracteres. La receta de la
  mecha no depende de la etapa, así que la noche del ciclón invita a trenzarla.
- **P2 gradación** (`e71cd171`): saturación 1,0, césped 1,08, exposición fija de día (1,45 EV100). ΔE frente a
  la paleta: arena 8,0, hierba 14,5, cénit 9,8; el objetivo < 8 no se alcanza en hierba ni cénit.
- **P3 terreno** (`f7ba8336`): sin banda húmeda por vértice, mancha de palmera en hierba seca de 1,2 m, facetado
  al 10 % / 5 %. Fuera de alcance: pendiente mínima de la orilla y espuma ligada a la ola (A1 pasos 2-3).
- **P4 haz** (`cf9a4cf2`), **P5 Mistral** (`d59fc330`), **P6 créditos** (`6e5a5ddf`), **P7 nubes**
  (`83685574`).
- **P8 bocina** (`7eaac9a3`): 1,6 recortaba (pico de la grabación −1,9 dBFS); se dejó en 0,95 con la música
  agachada a 0,6 durante 8 s: ~+5 LU sobre el canon y pico −2,3 dBFS.
- **P9 mezcla** (`d92f2066`): solo volúmenes en C++.
- **P10 menús**: botones con foco (pastilla `cielo_ocaso` y «›», porque Lora no trae «▸»), W/S además de flechas
  y Enter; Continuar y Cargar atenuados y no enfocables sin guardado; «Salir al menú» pregunta Sí/No si hay
  más de 20 s de juego desde el último guardado o carga (`FSaveSlotPolicy::HasUnsavedProgress`, con spec);
  línea fija de teclas en la pausa. Capturas `f8-menu-sin-guardado` y `f8-menu-pausa`. La navegación con
  teclado está cubierta por spec (`MenuNavigationStep`) y por la navegación nativa de Slate, pero no se ha
  probado pulsando teclas en una sesión real.

## Revisión final: hallazgos corregidos (2026-10-22)

Los 14 hallazgos confirmados de la revisión final, todos reales; un commit por grupo:

- **Leña de deriva enterrada** (`70482a52`): se colocaba con suelo 0, unos 2 m bajo la arena de la
  Cala Varada. Ahora usa `ExploredGround::HeightAtMeters`. Al cargar, los objetos soltados
  enterrados suben a ras de suelo y los incoherentes (id desconocido, cantidad imposible, posición
  no finita o fuera de la isla) se descartan con aviso; barril saneado (`FRainBarrelModel::Sanitize`,
  NaN e infinito) y cocos acotados por palma. Specs en `BeaconSaveSpec` y `RainBarrelSpec`.
- **Rendimiento** (`9081a250`): nubes, lluvia y bancos de peces ya no recrean su proxy cada
  fotograma; el sol y la luna giran a pasos de 0,1° (`FSkyPaletteModel::ShouldRotateLight`) y la
  luna solo hace sombra con el sol fuera. Sin medir con `stat VirtualShadowMapCache` ni con
  `bench.ps1`: queda para el paso de bench de `pendiente-motor.md`.
- **Efectos puntuales** (`5807fd27`): atenuación y clase antes de `Play` y `bAutoDestroy`.
- **Datos que faltan** (`399548b9`): «<fichero>: no se puede leer» en el log (spec de editor).
- **Teclas reservadas** (`70b0977c`): J, 1, 2 y 3.
- **Tiempos del final** (`1c4f26da`): créditos, rollo y fundidos leen `UBeaconSubsystem::FinaleTiming()`;
  `PhaseAt` sin uso, fuera.
- **Salir del juego** (`292f969a`): pide confirmación como «Salir al menú». Decisión: confirmar,
  no autoguardar al salir (un guardado silencioso en mitad de la noche del día 3 podría dejar al
  jugador en un punto peor que su último guardado manual).
- **Inglés de menús y HUD** (`023cc33b`): `.locres` compilados y versionados; spec
  `Explored.Localization.Locres` (existe, trae el C++ y está al día con el español).
- **Empaquetado y CI** (`8a1293bf`): `package.ps1` exige el audio local, recompila los `.locres` y
  comprueba el audio cocinado y el `.locres` en el paquete; CI con `reset/**` y `Tools/Style`.

Verificación: build del editor `Succeeded`; host tests 827 casos y 0 fallos; specs de editor
`Locres` e `IslandData` 6/6; DataCheck `--strict` y `l10n export --check` sin errores; `Tools/Style`
ruff, basedpyright (0 errores) y pytest 161/161 con 99 % de cobertura; partida del bot `competente`
VERDE (52,4 min a 1×, 10/10 etapas, 10/10 guardar/cargar, 0 errores ni avisos en el informe).

Con los `.locres` el juego arranca en cultura `es` (antes caía a `en` por no haber localización), y
el smoke test del motor `Core::Logging::StructuredLogFormat`, que depende del formato de números,
escribe 2-3 «LogAutomationTest: Error: Condition failed» al arrancar en Development. Es del motor,
no del juego, y el informe del bot no lo cuenta.

Pendiente de Rodrigo: añadir `Tools/Style` a la sección «Tests» de `CLAUDE.md` (no se ha tocado) y
empujar `reset/v2` para que corra CI con ASan, UBSan y matemáticas rápidas.

## Segunda revisión final (2026-10-22): 5 hallazgos corregidos

- Luna: sin Lumen, con la sombra apagada en el crepúsculo se colaba en los interiores. Decisión:
  no alumbra hasta que el sol se apaga del todo y entra con un fundido corto
  (`FSkyPaletteModel::MoonLightFade`, SunZ -0,08 → -0,12), en vez de pagar dos VSM a la vez.
- `package.ps1`: los `.locres` se compilan después de compilar el editor (clon limpio).
- Tests de regresión con modelo puro: carga de objetos soltados
  (`ExploredSaveStates::LoadDroppedItems`), posición de la leña de deriva
  (`FSupplyCrateModel::DriftwoodPileLocation`) y confirmación de salir de la pausa
  (`ExploredScreens::Pause*`). Los 7 casos nuevos fallaban con el código anterior; host tests
  835/835.

## Huecos de completitud (2026-10-22, noche)

Repaso de los criterios sin evidencia y las carencias que señaló un crítico independiente. Commits
del tramo: `b50d5710` (pistas), `651d8a2d` (créditos y licencias), `30d75b45` (icono y 1.0.0),
`129141eb` (fuera el barco de v1 y `SaveWorldDeltas`), `5dcfbb3f` y `644bb30b` (guardado),
`076fc41f` (triángulos en el bench), `450d5379` (commandlet de estilo), `0f6e9f24` y `5f4f4e51`
(capturas), `71aadbe4` (datos de v1) y `5d409ef3` (`.locres`).

### Cerrados, con evidencia

- **F1.2, F3.3 y F7.4 (sanitizers y cobertura).** En WSL (Ubuntu 24.04, clang 18): host tests
  836/836 con ASan + UBSan y 836/836 con `-ffast-math` (tras `644bb30b`: `HasPlausibleLocation`
  dejaba pasar un NaN con matemáticas rápidas, el mismo modo que el `/fp:fast` del editor); en
  Windows, 836/836 normal. Cobertura C++ de los modelos del host (`llvm-cov`, sin tests ni shim):
  91,8 % de líneas, 90,8 % de regiones y 79,9 % de ramas. Modelos de v3: `BeaconProgress` 99,2 %,
  `StormClock` 94,1 %, `Notebook` 100 %, `SupplyCrate` 94,9 %, `TutorialHint` 100 %,
  `IslandLayout` 92,3 %, `FinaleSequence` 100 %, `BenchBudget` 96,2 %, `RunPacing` 100 % y
  `SavePlayerState` 100 %. Receta en `Tools/HostTests/README.md`.
- **F1.5 (Wiring < 500 líneas).** `ExploredWiringSubsystem.cpp` tiene 293 líneas; el guardado está
  en `ExploredWiringSave.cpp` (378).
- **F2 (commandlet de validación).** `-run=ExploredStyle` (`450d5379`): 56 mallas, 0 fallos y 0
  avisos (`Saved/Logs/StyleCheck.log`). `Tools/Blender/validate.py` se queda: valida la fauna
  animada de `run_animals.py`, que `check-style` no cubre.
- **F4.2 (triángulos de escena).** `bench.ps1` (cinco vistas, 1080p): el RHI dibuja de media
  446.588 (cala), 454.390 (palmar), 443.868 (faro), 446.775 (galería) y 433.192 (ciclón)
  triángulos, todas las pasadas y con el césped, sin Nanite. El GDD §11 separa ahora el césped
  (≤ 1,5 M) del resto (< 400.000) y deja el rendimiento como criterio que manda.
- **F4.3 (texturas).** `count_textures.py` con la lista blanca nueva: 2 texturas en `/Game`
  (`T_Paleta` y `T_TerrainStrokes`) y 0 fuera de la lista.
- **Rendimiento en el editor.** Segunda pasada: 187,2 fps de media y 93,6 de mínimo, las cinco
  vistas OK. La primera dio un único fotograma de 38 ms en `bench-ciclon` (mínimo 25,9; 1 % bajo
  146,9) que no se repitió; si vuelve a salir sobre el paquete, mirar el relámpago del ciclón.
- **Editor.** Automation tests: 884 de 885 en verde, 0 fallos (uno con avisos de `GConfig`, como
  antes).
- **Carencias de juego y paquete.** Pistas contextuales (ocho, de una sola vez); avisos de Unreal,
  del soundfont MIT y enlaces CC BY en los créditos, más `Content/Licenses/THIRD_PARTY_LICENSES.txt`;
  icono propio y versión 1.0.0; fuera `AExploredBoat`, las teclas B, X y Z y `SaveWorldDeltas`;
  fuera `fauna.json`, `fauna_terrestre.json` y `boats.json` (no los leía nada) y
  `packs_catalogo.json` movido a `Tools/Packs`. El aviso de malla de
  `/Game/Generated/Meshes/Embarcaciones` ya no sale en los logs del juego (0 en `Shots.log` y
  `Explored.log`); falta comprobarlo en el log del cocinado (paso 11).
- **Revisión de seguridad del guardado (entregable de F7), a mano.** Tope de 4 M caracteres antes
  de reservar y un fichero mayor ni se carga; parser recursivo con `MaxDepth` y sin leer fuera del
  texto; NaN e infinitos con `IsFinite` explícito (corregido el caso de `644bb30b`); una posición
  fuera de la isla devuelve al jugador al punto de partida; escritura atómica (temporal + `Move`)
  con copia de seguridad. Sin hallazgos abiertos.
- **Smoke test `StructuredLogFormat`.** No afecta al paquete: el motor solo ejecuta los smoke tests
  si `!FPlatformProperties::RequiresCookedData()` (`AutomationTest.cpp`), falso en un juego
  cocinado. En el editor con `-game` sigue escribiendo 3 «LogAutomationTest: Error» que el informe
  del bot no cuenta.
- **Alcance de entrada e idioma.** README y GDD §9: entrega «teclado y ratón»; español original e
  inglés en Ajustes → Idioma (379 textos, 0 sin inglés).
- **Capturas.** f5 rehechas (`0f6e9f24`) y final de F6 rehecho (`5f4f4e51`): el epílogo ya no
  tiene estrellas sobre el texto. Las de f3 y f4 son el registro de su fase, no de la entrega.

### No cerrados

- **F1.6 y CI.** `reset/v2` no está empujada (decisión de Rodrigo): `host-tests.yml` no ha corrido.
  Las mismas pasadas (normal, sanitizers y matemáticas rápidas) están verdes en local.
- **F7.3 (revisión adversarial).** `codex exec` 0.162.1 con `-s read-only` o `workspace-write` no
  puede lanzar ningún proceso en Windows («setup refresh had errors») y devuelve una revisión
  vacía; no se ha saltado el sandbox. Queda pendiente con `/codex:adversarial-review` desde una
  sesión de Rodrigo. Apuntado en `papercuts.md`.
- **F2.2 y F4.4 (aprobación de láminas).** Las láminas existen (`docs/v3/laminas`), pero no hay
  constancia escrita de que Rodrigo aprobara los pilotos de F2 ni K1-K3. Solo él puede cerrarlo.
- **F6.4 y paso 9 de `pendiente-motor.md`.** Falta una partida humana completa y la mezcla de audio
  a oído.
- **Paquete (pasos 11 y 12 de `pendiente-motor.md`).** Hecho el 2026-10-22: ver «Paquete
  Development, bot y bench sobre el paquete». Tras el cierre, `Saved/Package/Windows`
  tiene el Shipping rehecho desde `HEAD` y `Saved/Package-Development/Windows` el Development.
- **Arte con crítica pendiente** (paso 7): franja de arena mojada entre el agua y la espuma, ondas
  a contraluz, bloques de roca con techo verde junto al faro y ΔE de hierba y cénit por encima de
  8. No se ha tocado esta noche: son cambios de material y de malla que piden revisión visual de
  Rodrigo.
- **`Content/Audio` y `Content/Generated` sin versionar** (411 MB y 274 MB, solo en este PC). El
  README explica cómo regenerarlos y `package.ps1` no empaqueta sin ellos. Decidir si van por LFS.
- ~~**`Tools/Textures` sin `pyproject.toml`**~~: ya lo tiene (8 tests, cobertura 100 % en el cierre).
- **Opcionales de F7 sin abordar:** cangrejo ambiental y optimización de LOD y distancia de
  dibujado (el bench va con 30-50 fps de margen sobre el mínimo).

## Paquete Shipping (paso 11 de `pendiente-motor.md`, 2026-10-22)

`powershell -NoProfile -File Tools/package.ps1 -Configuration Shipping`: `BUILD SUCCESSFUL`,
`ExitCode=0`; el cocinado, 0 errores y 1 aviso único (`r.VolumetricFog` por escalabilidad, inocuo).

- **Ruta y tamaño:** `Saved/Package/Windows` (lanzador `Explored.exe`), **604 MB**: `.ucas` 346 MB,
  `Explored-Win64-Shipping.exe` 151 MB, `.pak` 13 MB y el resto, prerrequisitos y DLL del motor.
- **Dentro:** los 17 ficheros de `Content/Data` sueltos (16 JSON, `grass.json` entre ellos, y
  `ground_field.bin`); en el
  `.pak`, las fuentes Lora, `T_IconosUI.png` y los `.locres` es/en; cocinados, las 11 mallas de
  `/Game/Art/K6_Cesped`, el suelo de la isla y el audio de muestra (clases, música, efectos, ambiente).
- **Arranque:** `Explored.exe -windowed` abre la ventana «Baliza Siete» con el menú principal
  (título, isla con césped y camino con su material, faro, botones con la fuente Lora); se cerró a
  los 35 s, sin volcados en `%LOCALAPPDATA%/Explored/Saved/Crashes` y sin procesos vivos. Shipping
  no escribe registro (`bUseLoggingInShipping` exige un motor compilado desde el código fuente): la
  comprobación de `Error` en el registro queda para el paquete Development.

Corregido por el camino:

- **`M_TerrainToon` se cocinaba como el material por defecto** (suelo gris en el paquete): el nodo
  Custom `SrgbToLinear` usaba `?:` con condición `float3`, que HLSL 2021 rechaza en el pase Nanite de
  cómputo de SM6 (`TBasePassCS…`). Ahora es `lerp(…, step(c, 0.04045))` en
  `Tools/Unreal/materials_f6.py`; el material se regeneró con ese script.
- **`package.ps1`:** vacía `Saved/Package/Windows` antes de archivar (UAT no lo hace: quedaban el
  `.pdb` de 245 MB y 27 JSON de v1, y el tamaño salía 843 MB); falla si el registro del cocinado
  trae `Failed to compile Material` (el cocinado da «Success» igualmente) y comprueba también
  `grass.json`, `ground_field.bin`, Slate en el manifiesto del `.pak` y las mallas de K6 cocinadas.

## Paquete Development, bot y bench sobre el paquete (pasos 11 y 12, 2026-10-22)

Binarios y contenido de `6ac833f9`. El Shipping verificado se guardó antes en
`Saved/Package-Shipping.zip` (471 MB comprimido; `package.ps1` vacía `Saved/Package/Windows`).

- **Empaquetado:** `Tools/package.ps1 -Configuration Development`: `BUILD SUCCESSFUL`, `ExitCode=0`,
  8 min; cocinado con 0 errores y el mismo aviso único de `r.VolumetricFog`; ningún `Failed to compile
  Material` ni referencias a `/Game/Generated/Meshes/Embarcaciones`. **1.193 MB**: `Explored.exe`
  313 MB y su `.pdb` 385 MB (solo en Development), `.ucas` 346 MB.
- **Arranque:** `Explored.exe -windowed` abre «Baliza Siete (64-bit Development PCD3D_SM6)» con el menú
  principal (título y subtítulo en Lora, faro con su material, suelo con caminos, césped, palmeras y
  mar; captura en `Saved/Shots/pkg-dev-menu.png`). En el log, `[Explored] Menú principal`, 0 `Error` y
  un solo aviso (`r.VolumetricFog`).
- **Bot sobre el paquete** (`-Clock 6`): ninguna diferencia con el editor.

| Ruta | Tiempo real | Duración a 1× | Desmayos | Páginas | Epílogo | Pasos | Resultado |
|---|---|---|---|---|---|---|---|
| competente | 17,4 min | 52,4 min | 0 | 8/8 | `completo (8/8 páginas)` | 140 `ok` | VERDE |
| sin_paginas | 17,2 min | 52,4 min | 0 | 1/8 | `parcial (1/8 páginas)` | 127 `ok` | VERDE |
| lenta | 25,9 min | 52,4 min | 0 | 8/8 | `completo (8/8 páginas)` | 188 `ok` | VERDE |

  En todas: 10/10 etapas, 10/10 guardar/cargar, 9/9 softlocks resolubles, física a la deriva 0, 0
  errores y 0 avisos en el informe; en `PlaytestV3.log` 0 `Error` (en el paquete no corre el smoke test
  del motor que daba los 3 «LogAutomationTest: Error» del editor) y solo el aviso de `r.VolumetricFog`.
  Final: `Luces del Mistral` → `Bocina` 9,6 s → `Pantalla de epílogo` → `Créditos del final (60 s)` →
  `[Playtest] Menú principal tras los créditos del final`; `playtest_v3.ps1` sale con 0. Logs copiados en
  `Saved/Logs/PkgBot-<ruta>.*`.
- **Bench sobre el paquete** (`Tools/bench.ps1 -Exe`, 1080p): OK en las tres pasadas. Para comparar con
  el editor en las mismas condiciones (portátil con RTX 4060 Laptop, la cifra baila ±5 % según la
  temperatura y el estado de energía), se midió el editor justo después:

| Vista | Paquete, 1.ª | Paquete, 2.ª | Editor, a continuación | GPU paquete 2.ª |
|---|---|---|---|---|
| bench-cala | 203,9 / 111,5 | 208,6 / 114,3 | 196,6 / 109,5 | 4,33 ms |
| bench-palmar | 159,9 / 94,3 | 163,3 / 98,4 | 160,1 / 92,6 | 5,24 ms |
| bench-faro | 192,8 / 101,3 | 196,5 / 106,6 | 186,1 / 110,1 | 3,96 ms |
| bench-faro-galeria | 153,4 / 90,3 | 160,2 / 103,1 | 155,6 / 99,7 | 7,84 ms |
| bench-ciclon | 225,7 / 131,2 | 230,1 / 130,7 | 218,7 / 136,0 | 3,34 ms |
| Resumen | 187,1 / 90,3: OK | **191,7 / 98,4: OK** | 183,4 / 92,6: OK | |

  Tercera pasada del paquete, con `-LogCmds="LogGarbage Log"`: 195,2 / 117,2 fps, OK. El paquete rinde
  igual o mejor que el editor medido a continuación (+4,5 % de media, +6 % de mínimo); los 199,8 / 116,7
  del editor de esta mañana salieron con la GPU en otro estado y no son comparables. Cifra de entrega:
  **191,7 fps de media y 98,4 de mínimo** (todas las vistas por encima de 150 / 90).
- **Recolección de basura en el paquete** (hueco de la tanda de la mañana): la marca recorre
  ~1.550 objetos en 0,34-0,96 ms (en el editor, ~44.000 y 16,4 ms). La recolección periódica de cada
  300 s ya no puede dar un tirón visible en el juego entregado.

## Partidas del bot en el editor (paso 8 de `pendiente-motor.md`)

| Ruta | Fecha y commit | Duración a 1× | Desmayos | Páginas | Epílogo | Resultado |
|---|---|---|---|---|---|---|
| competente | 2026-10-22, este commit (sobre `9c880015`) | 52,4 min (51,4 + 60 s de créditos) | 0 | 8/8 | `completo (8/8 páginas)` | VERDE |
| lenta | 2026-10-22, binarios de `c3eaa7bc` | 52,4 min (51,4 + 60 s de créditos) | 0 | 8/8 | `completo (8/8 páginas)` | VERDE |
| sin_paginas | 2026-10-22, binarios de `e3c46e2e` (código de `c3eaa7bc`) | 52,4 min (51,4 + 60 s de créditos) | 0 | 1/8 | `parcial (1/8 páginas)` | VERDE |

Ruta competente, `-Clock 6`: 16,6 min reales; 10/10 etapas, 10/10 guardar/cargar, `reached_epilogue`
true, 0 errores y 0 avisos en el informe; física a la deriva 0, puntos de aparición bloqueados 0,
9/9 softlocks resolubles y ningún paso fuera de tiempo (sin atascos). En `PlaytestV3.log`, 0 `Error`
propios (los 3 «LogAutomationTest: Error» son el smoke test del motor ya documentado). Final:
`Luces del Mistral` → `Bocina` 9,6 s → `Pantalla de epílogo` 7,0 s → `Créditos del final (60 s)` →
`Vuelta al menú principal` 60,0 s → `Menú principal` 0,6 s.

Ruta lenta, `-Clock 6`: 25,6 min reales (24,4 de prueba; la competente, 16,6), con una pausa de
«entretenerse» tras cada trayecto; 10/10 etapas, 10/10 guardar/cargar, `reached_epilogue` true,
2 sueños, 0 errores y 0 avisos en el informe; física a la deriva 0, puntos de aparición bloqueados
0, 9/9 softlocks resolubles y todos los pasos `ok` (sin atascos ni pasos fuera de tiempo). En
`PlaytestV3.log`, 0 `Error` propios (solo los 3 «LogAutomationTest: Error» del smoke test del
motor). Final: `Luces del Mistral` → `Bocina` 9,6 s → `Pantalla de epílogo` 7,0 s → `Créditos del
final (60 s)` → `Vuelta al menú principal` 60,0 s → `Menú principal` 0,6 s; `playtest_v3.ps1`
termina con «Créditos del final -> menú principal: ok». La duración a 1× coincide con la competente
porque la partida la cierra el reloj (el Mistral pasa el D3 a las 20:05), no el ritmo del jugador:
la lentitud se absorbe en las esperas (la ruta lenta pesca y hierve más tarde el D3, pero llega a la
linterna a las 14:20). `Luces del Mistral` sale dos veces en el log porque el bot guarda y carga en la
etapa Final; no es un fallo. El auditor marca 283 instancias «flotando/enterradas» (no es criterio).

Hallazgo y decisión: el bot se paraba en la pantalla de epílogo, así que «créditos → menú
principal» no se comprobaba nunca. Al añadirlo salió un fallo real de la vuelta al menú: con
`-SkipMenu` en la línea de comandos, «Salir al menú» y el fin de los créditos recargaban el mapa y
entraban otra vez a jugar (el `BeaconSubsystem` empezaba partida nueva). Ahora `ExitToMainMenu`
recarga con la opción de URL `ExploredMenu` y `ExploredSettingsLogic::StartsInMainMenu` hace que
`-SkipMenu` solo valga en el arranque (spec nuevo; con el código anterior falla). El bot pulsa
«Continuar» del epílogo, el auditor escribe el informe con los créditos rodando y, ya en el mapa
recargado, deja `[Playtest] Menú principal tras los créditos del final` (o un `Error` si no llega);
`playtest_v3.ps1` exige esa línea. El auditor sigue marcando 276 instancias «flotando/enterradas»
(283 en la pasada anterior): no es criterio del paso 8 y no se ha tocado.

Ruta sin_paginas, `-Clock 6`: 17,2 min reales de prueba (18,5 con el arranque); 10/10 etapas, 10/10
guardar/cargar, `reached_epilogue` true, 2 sueños, 0 errores y 0 avisos en el informe; física a la
deriva 0, 9/9 softlocks resolubles y todos los pasos `ok` (sin atascos ni pasos fuera de tiempo). Solo
lee la página 1 (camino crítico del guion, D1 en la casa del farero); el log da `[Baliza] Epílogo
parcial (1/8 páginas)`. En `PlaytestV3.log`, 0 `Error` propios (solo los 3 «LogAutomationTest: Error»
del smoke test del motor). Final: `Luces del Mistral` → `Bocina` 9,6 s → `Pantalla de epílogo` 7,0 s →
`Créditos del final (60 s)` → `Vuelta al menú principal` 60,0 s → `Menú principal` 0,6 s;
`playtest_v3.ps1` termina con «Créditos del final -> menú principal: ok». La duración a 1× es la misma
que en las otras dos rutas porque la cierra el reloj (el Mistral pasa el D3 a las 20:05): saltarse las
7 páginas opcionales se absorbe en la espera a la noche del D3 (linterna montada a las 13:31).

Paso 8 completo: las tres rutas en verde.

## Rendimiento en el editor, tanda del 2026-10-22 por la mañana (paso 10)

`Tools/bench.ps1`, 1080p, editor en `-game` fuera de pantalla. Media / mínimo en fps:

| Vista | Antes (`0ac5c871`) | Después (este commit) | GPU después |
|---|---|---|---|
| bench-cala | 212,7 / 119,8 | 211,4 / 130,9 | 3,97 ms |
| bench-palmar | 165,4 / **43,9 FALLA** | 165,0 / 117,4 | 5,43 ms |
| bench-faro | 205,6 / 137,4 | 204,8 / 134,4 | 4,39 ms |
| bench-faro-galeria | 162,0 / 110,8 | 162,0 / 116,7 | 5,98 ms |
| bench-ciclon | 256,9 / 161,7 | 255,7 / 169,3 | 3,40 ms |
| Resumen | 200,5 / 43,9: FALLA | 199,8 / 116,7: OK | |

La media iba sobrada; lo que fallaba era **un único fotograma de 22,8 ms** en el Palmar (1 % bajo,
125 fps). Con `stat dumphitches` (umbral de 20 ms) y el log de `LogGarbage` salieron dos causas de
tirones de un fotograma, ninguna de densidad del césped:

1. **Crecimiento de los ISM del césped.** Al llegar al Palmar (45.313 matas frente a ~4.800 en la Cala)
   los componentes crecían ranura a ranura y GPUScene recreaba su búfer de instancias (disposición
   SOA, potencias de dos): `UpdatePrimitive → CreateBuffer` de 22,4 ms en el hilo de render (fotograma
   de 26,6 ms). Arreglo: `FGrassFieldModel::EstimateSlotHighWater` (modelo puro, spec nuevo) asienta un
   modelo sin tope de trabajo en el centro de cada zona de `island.json` y el actor reserva al cargar
   esa marca de agua × 1,2 en ranuras ocultas (67.954 ranuras, 337 ms en la carga); si aun así hace
   falta crecer, crece un 25 % de golpe. Con la reserva, la Cala no pierde nada (211,4 frente a 212,7)
   y el tirón del Palmar desaparece.
2. **Recolección de basura periódica (cada 61 s).** En el editor marca ~44.000 objetos sin clústeres
   (27.000 son reflexión del editor: `Function`, `Class`, `ScriptStruct`): 16,4 ms de `GC Mark` en el
   hilo de juego, fotograma de 20 ms. Sumado a los 3,3 ms del hilo de juego del Palmar da los ~22,8 ms
   del fallo; es una lotería de en qué vista cae. La partida apenas genera basura (0-360 objetos por
   pasada), así que ahora se recoge cuando no se ve: pantalla en negro al dormir o desmayarse
   (`UIslandBodyComponent::PlayBlackout`) y al abrir la pausa (`AExploredPlayerController::SetUIMode`).
   La periódica queda de red de seguridad cada 300 s (`gc.TimeBetweenPurgingPendingKillObjects` en
   `Config/DefaultEngine.ini`).

Además, el bench escribe `[Bench] tirón <vista>: X ms a los Y s de la medida` por cada fotograma por
debajo de 45 fps, para cruzarlo con el resto del log; en la pasada final no salió ninguno.

Configuración final: césped sin cambios (`grass.json`: denso hasta 25 m, adelgazado hasta 100 m,
densidad 5,5 por m² y 6,0 en el Palmar, sin sombras), `r.ScreenPercentage.Default=87`,
`r.Nanite.MaxPixelsPerEdge=2.0`, sin Lumen, VSM y lluvia sin cambios. No ha hecho falta ninguna
ronda de recorte visual (densidad, sombras, materiales, decorado ni partículas).

Hueco que queda: una recolección periódica puede seguir cayendo en pleno juego si el jugador pasa
5 minutos sin pausar ni dormir; en el editor cuesta ~20 ms (50 fps ese fotograma) y en el paquete
debería costar bastante menos (las clases del motor van al pool que la recolección no recorre), sin
medir todavía: mirarlo en el bench sobre el paquete (paso 12) con `-LogCmds="LogGarbage Log"`.
**Medido el 2026-10-22:** en el paquete la marca tarda 0,34-0,96 ms (ver «Paquete Development, bot y
bench sobre el paquete»); hueco cerrado.

## Cierre de la entrega (2026-10-22, 09:45-10:30)

- **Shipping rehecho desde `HEAD`.** El Shipping de `Saved/Package-Shipping.zip` se cocinó a las
  08:11, antes del commit `6ac833f9` (08:16) que versiona el `M_TerrainToon` corregido. Para que el
  paquete entregado sea exactamente el código etiquetado, `package.ps1 -Configuration Shipping`
  otra vez: `ExitCode=0`, cocinado con 0 `Error:` y 0 `Failed to compile Material`, **604 MB** en
  `Saved/Package/Windows`. El Development (de las 08:19, posterior al último commit de código) se
  movió a `Saved/Package-Development/Windows` sin rehacerlo: sus tres partidas del bot y el bench
  siguen valiendo.
- **Arranque del Shipping:** ventana «Baliza Siete» con el menú principal a los 40 s
  (`Saved/Shots/pkg-shipping-final-menu.png`), 0 volcados en `%LOCALAPPDATA%/Explored/Saved/Crashes`.
  Hallazgo: en el primer arranque de este ejecutable Windows pregunta si permite el acceso a la red
  (diálogo del cortafuegos). El juego es para un jugador; no se ha averiguado qué abre el socket.
  No se ha pulsado nada en el diálogo.
- **Verificación final:** build del editor y Shipping `Succeeded`; host tests 838 casos, 0 fallos;
  DataCheck `--strict` 0 errores; `l10n export --check` 379 textos, 0 errores; Python con ruff y
  basedpyright sin errores y cobertura: Island 99,4 % (104), Textures 100 % (8), Audio 93,8 % (406),
  DataCheck 95,4 % (94), Localization 95,7 % (96), Style 99,2 % (165), Blender 85,7 % (111),
  Packs 90,8 % (78).
- Informe para Rodrigo: [`informe-final.md`](informe-final.md).

## Tráiler (2026-10-22)

- **Cómo se genera:** `Tools/trailer.ps1 [-Preview] [-Shot <plano>]` lanza el juego fuera de
  pantalla, con prioridad BelowNormal, con `-ExploredTrailer=Tools/Trailer/trailer.json`
  (`UExploredTrailerSubsystem`, fuera de Shipping). Paso de tiempo fijo de 1/30 s y un PNG por
  fotograma con el índice global (`Saved/Trailer/frames/f_00000.png`…); cada plano se asienta 4 s
  de juego sin grabar. El final se graba en un segundo proceso: la baliza se enciende en su primer
  fotograma y el final del juego corre de verdad. La cámara sale de `FTrailerModel` (Catmull-Rom
  centrípeto por longitud de arco, slerp de la mirada, suelo de `ground_field.bin` suavizado y
  fundidos) y `TrailerSpec` falla si el ojo salta, gira de golpe o da tirones dentro de un plano.
- **Grabación final:** 1920×1080, 2250 fotogramas (75,0 s), 0 faltan, 0 repetidos, 0 idénticos
  al anterior y 0 avisos de movimiento; 4,2 GB; unos 3 min de grabación en total. Solo salen
  oscuros los fundidos previstos (fotograma 0 y 2241-2249). Ensayo a 640×360 en 3 vueltas.
- **Tiempos reales** (también en `trailer.json`): mar 0-4 s, Cala 4-12, Palmar 12-14,7,
  Rocas 14,7-17, interfaz 17-20, cenital 20-22, camino 22-27, galería 27-32, ciclón orilla 32-37,
  ciclón refugio 37-42, carguero 42-50, final 50-68, título 68-75. En el final: luces del Mistral en
  el fotograma 1590 (53,0 s), bocina en el 1878 (62,6 s); el fundido del juego empieza en el
  2028 (67,6 s) y es interfaz, así que no sale en los fotogramas.
- Hojas de contacto: `Saved/Trailer/contact/<plano>.png`; informe: `Saved/Trailer/report.txt`.

## Pendiente de motor

Pasos 8 y 10-12 de [`pendiente-motor.md`](pendiente-motor.md) hechos el 2026-10-22. Queda lo que pide a
Rodrigo: partida a mano y mezcla de audio a oído (paso 9) y la revisión visual del arte (paso 7).
