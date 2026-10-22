# Pendiente de motor (Baliza Siete)

Trabajo que necesita abrir Unreal (editor, `-game`, commandlets o MCP) y que se dejó apuntado porque
el PC estaba en uso. Es **una sola lista para hacer de una sentada**, en el orden de abajo: cada paso
deja listo lo que necesita el siguiente. Todos los comandos se lanzan desde la raíz del repositorio en
PowerShell. Si un paso falla, se arregla antes de seguir; no hace falta repetir los anteriores salvo
que el arreglo toque lo que generaron.

## Estado del código antes de empezar (2026-10-21)

- **F6, final escenificado:** completo y compilado. La secuencia (tiempos, fases, llama, haz, epílogo
  según páginas, créditos) vive en `FFinaleSequenceModel` y la cota de duración en `FRunPacingModel`,
  ambos con spec en los host tests. Textos repasados y localización regenerada
  (`l10n export --check` verde). El epílogo consume Escape, B y Start, y la pausa no se abre ni en la
  cámara del final ni en el epílogo o los créditos (`FFinaleSequenceModel::CanOpenPause`).
- **F6, relieve y textura del suelo:** terreno regenerado con relieve de media escala (ondulaciones,
  lomas, surcos de la colina, rellanos rocosos, dunas, berma y caminos hundidos), normales 60 % suaves
  / 40 % de cara y color por vértice en tres tonos por zona (`Art/Island/SM_IslaSal_Terrain.fbx` +
  `.json`, 133 770 triángulos, pendiente máxima en caminos 26,58°); `island_decor.json` reapoyado;
  `M_TerrainToon` reescrito con la máscara `T_TerrainStrokes` (`Tools/Unreal/materials_f6.py`). Vista
  previa en Blender: `docs/v3/laminas/relieve-antes-despues.png`.
- **Césped y vegetación media (K6):** kit fusionado (`Art/Export/K6_Cesped`, 8 mallas de 25-100
  triángulos con el peso del viento en el alfa del color por vértice); césped alrededor de la cámara
  (`AExploredGrassField` sobre `FGrassFieldModel`, `Content/Data/grass.json`), que apoya cada mata en
  la malla exacta del terreno (`Content/Data/ground_field.bin`, `uv run island-groundfield` en
  `Tools/Island`). `island_decor.json` con 1 940 instancias (643 de K1 + 1 297 de K6: 314 helechos,
  406 hierbas altas, 577 flores), ninguna flotando ni enterrada. Estimación con los datos reales
  (test del host `GrassRealData`, sin las exclusiones de `island_props.json`): 29 791 matas en el
  Palmar, 23 773 en la colina del faro, 13 406 en la Cala Varada y 10 026 en la aparición;
  0,1-0,27 ms por celda generada (4 celdas por frame como mucho).
- **Altura del suelo:** `ExploredGround` (`WorldGen/GroundHeight.h`) es el único lector de
  `ground_field.bin`; lo comparten el césped, el auditor del bot («bajo el terreno»), los puntos de la
  ruta del bot y las capturas. Antes usaban `SampleColumn` (relieve analítico, hasta ~3,4 m de la
  malla). Si el campo no se lee, la altura sale de un trazado contra lo estático.
- **F7, pulido y paquete:** recorrido de rendimiento por Map_IslaSal con veredicto
  (`FBenchBudgetModel`), cocinado configurado (`Config/DefaultGame.ini`), `Tools/package.ps1` con
  comprobación del paquete y Shipping compilado (con y sin Unity build). Quitados la pesca de v1
  (F/R/T, `UFishingComponent`), el reloj de H y la opción «Duración del día». Detalle en
  [`estado-f7.md`](estado-f7.md).

## 1. Compilar

**[x] Hecho 2026-10-21: OK.** `Result: Succeeded` (también tras los arreglos de d852aa40).

```powershell
powershell -NoProfile -File Tools/build.ps1
```

- Termina en `Result: Succeeded` (Development Editor). Sin esto, ningún script de Unreal arranca con
  el código actual.

## 2. Generar los assets locales

**[x] Hecho 2026-10-21: OK.** `textures-strokes` regeneró la máscara (512 × 512, media 0,500); `sfx_ship_horn` y `sfx_lens_rotation_loop` ya estaban en `Content/Generated/Audio/Efectos`, así que no se generó audio.

No se versionan (`Art/Export/…`, `Content/Generated/…`, `Content/Audio/…`): en este PC ya existen a
2026-10-21, pero hay que regenerarlos si faltan o si cambió su fuente.

```powershell
cd Tools/Textures; uv sync; uv run textures-strokes; cd ../..
```

- Escribe `Art/Export/Textures/T_TerrainStrokes.png` (máscara de trazos del terreno). Si
  `T_TerrainStrokes` ya existe en el proyecto y se cambió `strokes.json`, el paso 4 se lanza con
  `$env:EXPLORED_REIMPORT_STROKES = '1'`.

Audio del final: si `sfx_ship_horn` o `sfx_lens_rotation_loop` no están en
`Content/Generated/Audio/Efectos`:

```powershell
cd Tools/Audio; uv run explored-audio build --only sfx_ship_horn,sfx_lens_rotation_loop; cd ../..
```

- `build` escribe en `Art/Export/Audio`; el paso 3 lo importa. En un clon limpio (sin
  `Content/Audio` ni `Content/Generated`) se genera el catálogo entero con
  `uv run explored-audio build` y se importa igual.

## 3. Importar el audio generado

**[x] 2026-10-21: no aplica** (el paso 2 no generó audio).

Solo si el paso 2 generó audio:

```powershell
powershell -NoProfile -File Tools/unreal_python.ps1 -Script Tools/Unreal/import_audio.py
```

- Lleva `Art/Export/Audio` a `Content/Generated/Audio`; el log termina sin `Error`.

## 4. Materiales

**[x] Hecho 2026-10-21: OK (d81d53d3), ampliado en d852aa40.** Los dos scripts sin `Error`. Comprobado por script: `M_BeaconBeam` con `BeamOrigin`/`BeamColor` y `BeamLength`/`BeamIntensity`; `M_TerrainToon` con `DarkFactor`, `LightFactor`, `StrokesTile0/1`, `FadeStartM`/`FadeEndM`; `T_TerrainStrokes` sRGB desactivado, `TC_Grayscale`, `TA_Wrap`; `M_ToonCesped` de dos caras, opaco, con ISM y `BaseShade`, `WindSway`, `WindStrength`, `FadeStartCm`/`FadeEndCm`. `materials_f6.py` añade ahora a `M_Toon` el cristal encendido (`GlowCell`, `GlowStrength`, `GlowColor`).

Van antes de importar la isla y el césped: `import_island.py` y `import_props.py` fallan a propósito
si no encuentran `M_TerrainToon` o `M_ToonCesped` («ejecuta antes materials_…»).

```powershell
powershell -NoProfile -File Tools/unreal_python.ps1 -Script Tools/Unreal/materials_f6.py
powershell -NoProfile -File Tools/unreal_python.ps1 -Script Tools/Unreal/materials_cesped.py
```

- `materials_f6.py` reconstruye `M_BeaconBeam`, `M_RainStreak`, `M_TerrainToon` y el viento de
  `M_Toon`; el log termina sin `Error` y lista los cuatro materiales.
- `M_BeaconBeam` tiene los parámetros `BeamOrigin` (vector), `BeamLength` y `BeamIntensity`
  (escalares): `ALighthouseLamp::UpdateFlame` los escribe por nombre y, si falta alguno, el cono se ve
  a intensidad fija sin apagarse con la distancia.
- `M_TerrainToon` compila y tiene `DarkFactor`, `LightFactor`, `StrokesTile0/1` y
  `FadeStartM`/`FadeEndM`; `T_TerrainStrokes` es lineal (sRGB desactivado), `TC_Grayscale` y con
  repetición.
- `materials_cesped.py` termina sin `Error`; `M_ToonCesped` es de dos caras, opaco, con uso de
  instancias y los parámetros `BaseShade`, `WindSway`, `WindStrength`, `FadeStartCm` y `FadeEndCm`.

## 5. Importar la isla y el césped (K6)

**[x] Hecho 2026-10-21: OK (d81d53d3).** Isla con caja z máx. 3588,2 cm (35,882 m) y normales importadas; K6: 8 mallas en `/Game/Art/K6_Cesped` (25-100 triángulos, colisión ninguna, color por vértice), 0 errores.

```powershell
powershell -NoProfile -File Tools/unreal_python.ps1 -Script Tools/Unreal/import_island.py
$env:EXPLORED_KITS = 'K6_Cesped'
powershell -NoProfile -File Tools/unreal_python.ps1 -Script Tools/Unreal/import_props.py
Remove-Item Env:EXPLORED_KITS
```

- `import_island.py` termina con la caja dentro de tolerancia (`bounds_max_m` z = 35,882) y la malla
  conserva las normales importadas (sombreado con facetas suaves, sin costuras en las aristas).
- `import_props.py` (solo K6) importa 8 mallas en `/Game/Art/K6_Cesped` con `M_ToonCesped`, colisión
  «ninguna», color por vértice importado y caja/triángulos dentro de tolerancia
  (`Saved/Logs/import_props.json`).

## 6. Automation tests

**[x] Hecho 2026-10-21: OK.** 859 tests, 0 fallos (858 en verde y 1 con avisos de `GConfig` en `UExploredGameUserSettings`), repetido tras d852aa40; incluye «Entrada del final» y `SettingsLogic`. Host tests: 812 casos, 0 fallos.

```powershell
powershell -NoProfile -File Tools/test.ps1
```

- Sale con código 0 y ningún test en rojo (filtro por defecto `Explored`: specs de modelos y tests que
  necesitan el motor). Incluye los casos nuevos de `FinaleSequenceSpec` («Entrada del final») y de
  `SettingsLogicSpec` (F, R, T y H ya no son teclas reservadas).

## 7. Capturas

**[x] Hecho 2026-10-21: arreglado en d852aa40.** En la primera tanda, `f6-final-encender` no enseñaba la llama (cristal opaco), el haz salía fuera de plano, el Mistral se capturaba 2 s antes de la bocina y el epílogo a medio fundir. Repetidas y conformes. Log: `[Decor] 1940 instancias decorativas en 11 mallas`, `[Césped] 274 × 246 vértices de suelo, 129 exclusiones, 36 palmeras` y 21 225 matas en el Palmar; ningún aviso de `falta` ni de `ground_field.bin`. Islote arreglado después (2026-10-22, commit de `Tools/Island`): la roca de `f6-relieve-islote` salía en triángulos grises grandes porque la regla «pendiente > 35° = roca» se evaluaba con la normal de cada vértice, inclinada ±14° por el ruido de facetas, y el color cambiaba a saltos de celda. Ahora la pendiente se funde en la rejilla, el color pasa de suelo a roca en rampa (35 ± 6°) y se funde 1-2 celdas junto a la roca, y la mezcla de normal de cara baja de 0,4 a 0,12 entre 25° y 45° de pendiente. Isla reimportada, capturas `f6-relieve-*` y `f7-cesped-*` repetidas y conformes (21 225 matas en el Palmar). Sigue igual, y ya salía en `f6-suave-*`: los bloques de roca con techo verde junto al faro.

```powershell
powershell -NoProfile -File Tools/shots.ps1 -Set f6 -Width 1920 -Height 1080
powershell -NoProfile -File Tools/shots.ps1 -Set f4b -Width 1920 -Height 1080
```

Las cámaras se apoyan ahora en la malla del terreno (`ExploredGround`): ninguna debe salir dentro de
una ladera ni flotando. Copiar de `Saved/Shots` a `docs/v3/capturas/`:

- Del conjunto `f6`, las cinco del final con su nombre:

  | Captura | Qué comprobar |
  |---|---|
  | `f6-final-encender.png` | A los 2 s de los 3: llama visible y a medio crecer en la linterna; sin cono todavía |
  | `f6-final-haz.png` | Plano del final detrás del faro; cono del haz legible sobre el mar de noche, sin cortar en seco al tocar el agua |
  | `f6-final-mistral.png` | Luces del Mistral más cerca que en la anterior (captura en el instante de la bocina, ~9,6 s) |
  | `f6-final-epilogo.png` | Pantalla de epílogo con la línea variable y «Isla de Sal. Jueves, 23:40.»; fundido terminado |
  | `f6-final-creditos.png` | Créditos del final con la música de créditos |

- Del conjunto `f4b`, `f4b-cenital`, `f4b-cala`, `f4b-palmar`, `f4b-faro`, `f4b-islote` y
  `f4b-atardecer` como `f6-relieve-<vista>.png`, y `f4b-cala`, `f4b-palmar`, `f4b-faro` y
  `f4b-cenital` también como `f7-cesped-<vista>.png` (una sola tanda sirve para las dos series).

Comprobar en `f6-relieve-*` (frente a `f6-suave-*`):

- Lomas y surcos legibles, trazos visibles de cerca y apagados a lo lejos (sin moiré ni repetición
  evidente a 20-40 m), caminos de tierra hundidos con borde marcado, manchas de tierra bajo las
  palmeras; que no se vea más facetado que `f4b-*`.
- Nada de la vegetación (HISM de `island_decor.json`) flota ni se hunde, y el faro, la casa, los
  refugios y la balsa siguen apoyados en llano.

Comprobar en `f7-cesped-*`:

- Ninguna hoja desaparece (dos caras), las matas se mecen desde la punta y no desde la base, el césped
  no tapa caminos, objetos recogibles, la balsa, el faro ni la casa (1,5 m libres), no hay césped en
  arena, roca ni agua, y nada flota ni se hunde en las laderas; en el borde (45-70 m) las matas
  encogen sin un corte brusco.

En el log del juego de cualquiera de las dos tandas (`Saved/Logs/`):

- `[Decor] 1940 instancias decorativas en 11 mallas`, `[Césped] ... vértices de suelo, ...
  exclusiones, ... palmeras` y `[Césped] N matas en M celdas` con N entre 15 000 y 40 000 en el
  Palmar; ningún aviso `[Césped] falta`, `[Decor] falta la malla` ni `[Suelo] ground_field.bin no se
  puede usar`. Si `ground_field.bin` no se lee, el log lo dice y la isla queda sin césped (no falla).

## 8. Las tres partidas del bot en el editor (criterios 1 y 2 de F6)

**[x] Hecho 2026-10-22: OK.** Competente, lenta y sin_paginas en VERDE en el editor (52,4 min a 1×,
0 desmayos, 8/8, 8/8 y 1/8 páginas, vuelta al menú tras los créditos) y repetidas sobre el paquete
Development (paso 12). Detalle en `estado-f7.md`, «Partidas del bot en el editor».

A 1× real (lo que mide el criterio) cada una tarda ~53 min; con `-Clock 6` el bot acelera el reloj y
el informe sigue dando los minutos equivalentes a 1×.

```powershell
powershell -NoProfile -File Tools/playtest_v3.ps1 -Route competente -Clock 6 -TimeoutMinutes 50
powershell -NoProfile -File Tools/playtest_v3.ps1 -Route lenta -Clock 6 -TimeoutMinutes 50
powershell -NoProfile -File Tools/playtest_v3.ps1 -Route sin_paginas -Clock 6 -TimeoutMinutes 50
```

Comprobar en cada informe (`Saved/Shots/.../playtest.md` y `playtest.json`) y en
`Saved/Logs/PlaytestV3.log`:

| Ruta | Duración a 1× | Desmayos | Páginas | Epílogo (línea del log `[Baliza] Epílogo …`) |
|---|---|---|---|---|
| competente | 50-60 min (previsto por `FRunPacingModel`: 52,2; medido en 918c2ba0: 52,4) | 0 | 8/8 | `completo (8/8 páginas)` |
| lenta | ≤ 65 min | ≤ 1 | 8/8 | `completo (8/8 páginas)` |
| sin_paginas | 47-65 min | 0 | < 8 (solo las del camino crítico que lea el guion) | `parcial (N/8 páginas)`, N < 8 |

Además:

- `reached_epilogue: true` en las tres, 0 `Error` en el log, y la secuencia `Luces del Mistral` →
  `Bocina del Mistral` (~9,6 s después) → `Pantalla de epílogo` (~7 s después) →
  `Créditos del final` → menú principal. El bot pulsa «Continuar» del epílogo y el auditor espera la
  vuelta al menú tras escribir el informe; `playtest_v3.ps1` falla si no está la línea
  `[Playtest] Menú principal tras los créditos del final`.
- El bot llega al faro y a la balsa sin bloqueos ni atascos en los rebordes y taludes de los caminos
  hundidos, y no se sale del camino (el césped y la decoración no tienen colisión ni responden a
  ningún canal: las trazas de interacción los atraviesan).
- `[Playtest] Física a la deriva: N defecto(s)`: con la altura de la malla, un defecto «bajo el
  terreno» es real (antes había falsos positivos y negativos en laderas y caminos hundidos).

Cálculo de la cota (sin motor, `Tests/RunPacingSpec.cpp`): despierto D1 07:00-20:00 (13 h), D2
06:00-02:00 con el ciclón (20 h) y D3 06:00-20:00 (14 h) = 47 h × 65 s = 50,9 min; más el final
hasta el epílogo (3 s de encender + 16,6 s) y 60 s de créditos = 52,2 min. Con el día de 24 min de
F3e daba 48,3 (fuera del rango), y con 32 min se pasaría de 60.

## 9. Mezcla de audio y comprobaciones a mano

**[ ] Pendiente de Rodrigo (2026-10-22).** Necesita oído y mando: ningún agente puede cerrarlo.

Una partida a mano (o la ruta competente con `-Window`) escuchando:

- Bocina (`sfx_ship_horn`, tono 1): se oye por encima de la música del final sin saturar.
- Lente (`sfx_lens_rotation_loop`, 0,9, atenuada en la linterna): audible arriba, casi nada desde la
  cámara del final; si no se oye en el plano del final, subirla o hacerla 2D durante el final.
- Transición de música: final al encender, créditos al pulsar «Continuar», y silencio limpio al
  volver al menú.
- Ambiente por zona y lluvia del ciclón sin cortes al entrar en la base del faro.

Apuntar en `docs/creditos-musica.md` cualquier cambio de pista. En la misma partida:

- Con el mando, Start en el epílogo pasa a los créditos (como Escape y B) y no abre la pausa; en los
  créditos Start tampoco la abre.
- F, R, T y H no hacen nada en tierra (la pesca es con E en las Rocas del Pescador); en Ajustes →
  Juego no aparece «Duración del día».
- Empezar a encender la linterna y apartarse antes de los 3 s: la llama se apaga sola.

## 10. Rendimiento a 1080p en el editor (criterio 1 de F7)

**[x] Hecho 2026-10-22 con el césped continuo (F8): OK.** `[Bench] Resumen: 5 vistas, media 180,7 fps,
mínimo 88,4 fps: OK` (cala 197,0 / 99,8; palmar 158,7 / 93,5; faro 182,0 / 109,8; galería 137,5 / 88,4;
ciclón 228,2 / 127,7). Detalle y coste del césped en [`estado-f7.md`](estado-f7.md) § F8. Repetido el 2026-10-22 de noche: 187,2 / 93,6 fps y 433.000-459.000 triángulos del RHI por vista (estado-f7.md, «Huecos de completitud»). El 2026-10-22 por la mañana falló una vez (Palmar, un fotograma de 22,8 ms): reserva de ranuras del césped al cargar y recolección de basura en pausas y fundidos; después, 199,8 / 116,7 fps: OK (estado-f7.md, «Rendimiento en el editor, tanda del 2026-10-22 por la mañana»).

```powershell
powershell -NoProfile -File Tools/bench.ps1                 # editor en -game, fuera de pantalla
powershell -NoProfile -File Tools/bench.ps1 -Window         # con ventana, para mirarlo
```

- Objetivo: `[Bench] Resumen: … OK` con **≥ 60 fps de media y ≥ 45 fps de mínimo** en cada una de
  las cinco vistas (`bench-cala`, `bench-palmar`, `bench-faro`, `bench-faro-galeria`,
  `bench-ciclon`), ya con el césped. El script sale con error si alguna falla o si no llega el
  resumen.
- Apuntar en `estado-f7.md` la línea `[Bench] fps …` de cada vista y el `GPU=… ms` de su línea
  `[Bench] <vista>`.
- Comprobar en `Saved/Shots/bench-ciclon.png` que el ciclón está activo (lluvia y oleaje); si sale un
  cielo despejado, la vista no fija el clima y su cifra no vale.
- Mirar tirones al caminar (`stat unit`): cada frame con celdas nuevas reescribe ranuras de los ISM
  del césped y marca su estado de render; si se ven, bajar `cellsPerFrame` a 2 en `grass.json`.
- Si no llega, primero el césped (por orden: `castShadow` false en `tall` y `fern`; `radiusM` 55 con
  `fade` 35-55; densidades × 0,7) y después, por coste visual: `r.ScreenPercentage.Default` (hoy 87),
  `r.Nanite.MaxPixelsPerEdge` (hoy 2), distancia de dibujado de la vegetación HISM y sombras virtuales
  de la lluvia.

## 11. Paquetes Development y Shipping y su arranque (criterios 2 y 5 de F7)

**[x] Hecho 2026-10-22.** Shipping: 604 MB, arranca al menú (`estado-f7.md`, «Paquete Shipping»;
copia en `Saved/Package-Shipping.zip`). Development: 1.193 MB, arranca al menú con 0 `Error` en el log
(`estado-f7.md`, «Paquete Development, bot y bench sobre el paquete»).

Los dos se archivan en `Saved/Package/Windows` y el segundo pisa al primero: se empaqueta y se
arranca Shipping primero, y Development al final, porque es el que usa el paso 12.

```powershell
powershell -NoProfile -File Tools/package.ps1                              # Shipping
```

- Arrancar `Saved/Package/Windows/Explored.exe` (Shipping): título «Baliza Siete», menú principal con
  música, Nueva partida, Iria en la Cala con HUD, ayuda inicial y textos (los JSON sueltos cargan,
  también `grass.json` y `ground_field.bin`: hay césped), lluvia y faro con sus materiales (no
  grises). Salir desde el menú.

```powershell
powershell -NoProfile -File Tools/package.ps1 -Configuration Development
```

- Las dos terminan con `Paquete <Configuración>: … (N MB)`; apuntar el tamaño de Shipping
  (orientativo ≤ 1,2 GB con audio).
- Si UAT rechaza `-AdditionalCookerOptions=-DisablePlugins=UnrealMCPython`, quitarlo del script (no se
  ha podido probar sin motor).
- En el log del cocinado (`%APPDATA%/Unreal Engine/AutomationTool/Logs/…/Cook-*.txt`), ningún
  `Error` ni avisos de `/Game/Generated/Meshes/Embarcaciones` (el barco antiguo se quitó en
  129141eb y el juego ya no lo pide).
- Arrancar también el Development y comprobar lo mismo que en Shipping (además carga los comandos de
  depuración que usan el bot y el bench).

## 12. Bot y rendimiento sobre el paquete Development

**[x] Hecho 2026-10-22.** Competente, sin_paginas y lenta en VERDE sobre el paquete; bench
`191,7 / 98,4 fps: OK` (el editor, medido a continuación, 183,4 / 92,6). Detalle en `estado-f7.md`,
«Paquete Development, bot y bench sobre el paquete».

```powershell
powershell -NoProfile -File Tools/playtest_v3.ps1 -Exe Saved/Package/Windows/Explored.exe -Route competente
powershell -NoProfile -File Tools/playtest_v3.ps1 -Exe Saved/Package/Windows/Explored.exe -Route lenta
powershell -NoProfile -File Tools/playtest_v3.ps1 -Exe Saved/Package/Windows/Explored.exe -Route sin_paginas
powershell -NoProfile -File Tools/bench.ps1 -Exe Saved/Package/Windows/Explored.exe
```

- Las tres partidas: mismos criterios que la tabla del paso 8; 0 `Error` en
  `Saved/Logs/PlaytestV3.log`.
- El bench sobre el paquete es la cifra que cuenta para la entrega: mismos objetivos que el paso 10;
  apuntarla en `estado-f7.md`.

## 13. F8: césped continuo y arena estilizada

**[x] Hecho 2026-10-22.** Orden (cada paso deja listo el siguiente; los dos scripts de materiales crean o
actualizan `MPC_Terreno` con los valores de `grass.json`, así que da igual cuál vaya primero):

```powershell
cd Tools/Island; uv run island-terrain; cd ../..        # FBX con el tipo de suelo en el alfa del color
powershell -NoProfile -File Tools/build.ps1
powershell -NoProfile -File Tools/unreal_python.ps1 -Script Tools/Unreal/materials_cesped.py
powershell -NoProfile -File Tools/unreal_python.ps1 -Script Tools/Unreal/materials_f6.py
powershell -NoProfile -File Tools/unreal_python.ps1 -Script Tools/Unreal/materials_lowpoly.py
powershell -NoProfile -File Tools/unreal_python.ps1 -Script Tools/Unreal/import_island.py
$env:EXPLORED_KITS = 'K6_Cesped'
powershell -NoProfile -File Tools/unreal_python.ps1 -Script Tools/Unreal/import_props.py
Remove-Item Env:EXPLORED_KITS
powershell -NoProfile -File Tools/shots.ps1 -Set f8 -Width 1920 -Height 1080
powershell -NoProfile -File Tools/bench.ps1
```

- Los cuatro scripts terminan sin `Error`; `import_props.py` importa 11 mallas de K6 (los tres parches de
  61-74 triángulos) con una sola LOD.
- Capturas en `docs/v3/capturas/f8-cesped-*.png` (Palmar, colina, Cala, de cerca, el adelgazamiento desde
  la ladera y dos pasos separados 1 s) y `f8-arena-*.png` (orilla, de cerca, de cara al sol y el encuadre
  de `f6-relieve-cala`). Log: `[Césped] … denso hasta 25 m y adelgazado hasta 100 m (8 niveles)` y
  `[Shots] césped <vista>: … triángulos a la vista, siembra media …`; ningún aviso `[Césped] falta`.
