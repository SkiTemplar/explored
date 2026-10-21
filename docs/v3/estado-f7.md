# Baliza Siete: estado de F7 (pulido y paquete, tramo sin motor)

Fecha: 2026-10-21 · rama `reset/v2` · sin abrir Unreal (el PC estaba en uso): todo lo que necesita
el editor o el juego está en [`pendiente-motor.md`](pendiente-motor.md), sección «F7».

## Criterio de salida de F7 (plan.md §4)

| # | Criterio | Estado en este tramo |
|---|---|---|
| 1 | ≥ 60 fps de media y ≥ 45 de mínimo a 1080p (`Tools/bench.ps1`, 3 zonas y ciclón) | Recorrido y veredicto listos en código; medida PENDIENTE (motor) |
| 2 | `Tools/package.ps1` genera un Win64 que arranca y el bot completa 3 partidas sin fallo | Script y cocinado listos; Shipping compila; paquete y partidas PENDIENTE (motor) |
| 3 | `/codex:adversarial-review` sin CRITICAL ni HIGH | No abordado en este tramo |
| 4 | Cobertura ≥ 80 % en modelos nuevos; HostTests con sanitizers y CI verdes | HostTests 797 casos, 0 fallos (13 nuevos de `BenchBudgetSpec`); sanitizers solo en CI (en Windows falta `stl_asan.lib`) |
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

## Hallazgos sin corregir (decisión de diseño o de otro tramo)

- **Pesca antigua en F/R/T.** `AExploredCharacter` sigue creando `UFishingComponent` y mapea F
  (lanzar), R/T (sedal). `StartCast` solo pide agua de ≥ 0,3 m delante, sin caña ni punto de pesca:
  `UExploredFishingSubsystem` no filtra `ShouldCreateSubsystem`, así que existe en Map_IslaSal y
  esto es probablemente un atajo contra el GDD §5.4 (pesca solo en las Rocas del Pescador con caña,
  tecla E). Comprobar en juego con F junto al agua y, si pesca, quitar el mapeo o el componente.
- **H (reloj de pulsera) no muestra nada.** `SetWristWatchRaised` sigue mapeado, pero
  `SExploredWristWatch` se borró en F1 y nadie lee la lectura. Quitar el mapeo o reutilizarlo.
- **`Content/Audio` y `Content/Generated` sin versionar** (411 MB y 274 MB en disco local). Un
  clon limpio cocina sin música ni efectos. Decidir si van por LFS antes de entregar.
- **La balsa no responde al mando** (solo W, A, D, E); fuera de alcance según el GDD §9.
- **`Tools/Textures` no tiene `pyproject.toml`** aunque la sección «Tests» de `CLAUDE.md` lo cita;
  es carpeta del tramo «Relieve» y no se ha tocado.
- Del plan de F7 quedan sin abordar: optimización (LOD, distancia de dibujado), revisión de
  seguridad del guardado (`SaveArchive`), cangrejo ambiental y revisión adversarial.

## Pendiente de motor

Ver [`pendiente-motor.md`](pendiente-motor.md), sección «F7 — Pulido y paquete»: bench, paquete
Shipping y Development, arranque del paquete, tres partidas del bot sobre el paquete y tamaño.
