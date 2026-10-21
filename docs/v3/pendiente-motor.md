# Pendiente de motor (Baliza Siete)

Trabajo que necesita abrir Unreal (editor, `-game`, commandlets o MCP) y que se dejó apuntado porque
el PC estaba en uso. Cada tramo indica el comando exacto, en orden, y qué comprobar. Todos los
comandos se lanzan desde la raíz del repositorio en PowerShell. Antes de empezar:
`powershell -NoProfile -File Tools/build.ps1` (debe terminar en `Result: Succeeded`).

## F6 — Cierre (2026-10-21)

Estado del código: final escenificado completo y compilado (`Tools/build.ps1` verde); la secuencia
(tiempos, fases, llama, haz, epílogo según páginas, créditos) vive en `FFinaleSequenceModel` y la
cota de duración en `FRunPacingModel`, ambos con spec en los host tests. Textos repasados y
localización regenerada (`l10n export --check` verde).

### 1. Materiales de F6

```powershell
powershell -NoProfile -File Tools/unreal_python.ps1 -Script Tools/Unreal/materials_f6.py
```

Reconstruye `M_BeaconBeam`, `M_RainStreak`, `M_TerrainToon` y el viento de `M_Toon`. Comprobar:

- El log termina sin `Error` y lista los cuatro materiales.
- `M_BeaconBeam` tiene los parámetros `BeamOrigin` (vector), `BeamLength` y `BeamIntensity`
  (escalares): `ALighthouseLamp::UpdateFlame` los escribe por nombre y, si falta alguno, el cono
  se ve a intensidad fija sin apagarse con la distancia.

Si `sfx_ship_horn` o `sfx_lens_rotation_loop` no están en `Content/Generated/Audio/Efectos`
(son locales y no se versionan), generarlos e importarlos antes:

```powershell
cd Tools/Audio; uv run explored-audio build --only sfx_ship_horn,sfx_lens_rotation_loop; cd ../..
powershell -NoProfile -File Tools/unreal_python.ps1 -Script Tools/Unreal/import_audio.py
```

(`build` escribe en `Art/Export/Audio`; `import_audio.py` lo lleva a `Content/Generated/Audio`.)

### 2. Capturas del final

```powershell
powershell -NoProfile -File Tools/shots.ps1 -Set f6 -Width 1920 -Height 1080
```

Deja en `Saved/Shots` cinco capturas; copiarlas a `docs/v3/capturas/`:

| Captura | Qué comprobar |
|---|---|
| `f6-final-encender.png` | A los 2 s de los 3: llama visible y a medio crecer en la linterna; sin cono todavía |
| `f6-final-haz.png` | Plano del final detrás del faro; cono del haz legible sobre el mar de noche, sin cortar en seco al tocar el agua |
| `f6-final-mistral.png` | Luces del Mistral más cerca que en la anterior (captura en el instante de la bocina, ~9,6 s) |
| `f6-final-epilogo.png` | Pantalla de epílogo con la línea variable y «Isla de Sal. Jueves, 23:40.»; fundido terminado |
| `f6-final-creditos.png` | Créditos del final con la música de créditos |

### 3. Las tres partidas del bot (criterios 1 y 2 de F6)

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

Además: `reached_epilogue: true` en las tres, 0 `Error` en el log, y la secuencia
`Luces del Mistral` → `Bocina del Mistral` (~9,6 s después) → `Pantalla de epílogo` (~7 s después) →
`Créditos del final` → menú principal.

Cálculo de la cota (sin motor, `Tests/RunPacingSpec.cpp`): despierto D1 07:00-20:00 (13 h), D2
06:00-02:00 con el ciclón (20 h) y D3 06:00-20:00 (14 h) = 47 h × 65 s = 50,9 min; más el final
hasta el epílogo (3 s de encender + 16,6 s) y 60 s de créditos = 52,2 min. Con el día de 24 min de F3e
daba 48,3 (fuera del rango), y con 32 min se pasaría de 60.

### 4. Mezcla de audio en juego

Una partida a mano (o la ruta competente con `-Window`) escuchando:

- Bocina (`sfx_ship_horn`, tono 1): se oye por encima de la música del final sin saturar.
- Lente (`sfx_lens_rotation_loop`, 0,9, atenuada en la linterna): audible arriba, casi nada desde la
  cámara del final; si no se oye en el plano del final, subirla o hacerla 2D durante el final.
- Transición de música: final al encender, créditos al pulsar «Continuar», y silencio limpio al
  volver al menú.
- Ambiente por zona y lluvia del ciclón sin cortes al entrar en la base del faro.

Apuntar en `docs/creditos-musica.md` cualquier cambio de pista.

## F7 — Pulido y paquete (2026-10-21)

Estado del código: recorrido de rendimiento por Map_IslaSal con veredicto (`FBenchBudgetModel`),
cocinado configurado (`Config/DefaultGame.ini`), `Tools/package.ps1` con comprobación del paquete y
Shipping compilado (con y sin Unity build). Detalle en [`estado-f7.md`](estado-f7.md).

### 1. Rendimiento a 1080p (criterio 1 de F7)

```powershell
powershell -NoProfile -File Tools/bench.ps1                 # editor en -game, fuera de pantalla
powershell -NoProfile -File Tools/bench.ps1 -Window         # con ventana, para mirarlo
```

- Objetivo: `[Bench] Resumen: … OK` con **≥ 60 fps de media y ≥ 45 fps de mínimo** en cada una de
  las cinco vistas (`bench-cala`, `bench-palmar`, `bench-faro`, `bench-faro-galeria`,
  `bench-ciclon`). El script sale con error si alguna falla o si no llega el resumen.
- Apuntar en `estado-f7.md` la línea `[Bench] fps …` de cada vista y el `GPU=… ms` de su línea
  `[Bench] <vista>`.
- Comprobar en `Saved/Shots/bench-ciclon.png` que el ciclón está activo (lluvia y oleaje); si sale
  un cielo despejado, la vista no fija el clima y su cifra no vale.
- Repetir sobre el paquete Development (paso 2), que es la cifra que cuenta para la entrega:
  `powershell -NoProfile -File Tools/bench.ps1 -Exe Saved/Package/Windows/Explored.exe`.
- Si no llega: palancas en orden de coste visual, `r.ScreenPercentage.Default` (hoy 87),
  `r.Nanite.MaxPixelsPerEdge` (hoy 2), distancia de dibujado de la vegetación HISM y sombras
  virtuales de la lluvia.

### 2. Paquete (criterios 2 y 5 de F7)

```powershell
powershell -NoProfile -File Tools/package.ps1 -Configuration Development
powershell -NoProfile -File Tools/package.ps1                              # Shipping
```

- Las dos terminan con `Paquete <Configuración>: … (N MB)`; apuntar el tamaño de Shipping
  (orientativo ≤ 1,2 GB con audio).
- Si UAT rechaza `-AdditionalCookerOptions=-DisablePlugins=UnrealMCPython`, quitarlo del script
  (no se ha podido probar sin motor).
- En el log del cocinado (`%APPDATA%/Unreal Engine/AutomationTool/Logs/…/Cook-*.txt`), ningún
  `Error`. Es esperable un aviso por `/Game/Generated/Meshes/Embarcaciones` (barco antiguo, la
  balsa no lo usa).
- Arrancar `Saved/Package/Windows/Explored.exe` (Shipping): título «Baliza Siete», menú principal
  con música, Nueva partida, Iria en la Cala con HUD, ayuda inicial y textos (los JSON sueltos
  cargan), lluvia y faro con sus materiales (no grises). Salir desde el menú.
- Con el paquete Development, las tres partidas del bot sobre el paquete:

  ```powershell
  powershell -NoProfile -File Tools/playtest_v3.ps1 -Exe Saved/Package/Windows/Explored.exe -Route competente
  powershell -NoProfile -File Tools/playtest_v3.ps1 -Exe Saved/Package/Windows/Explored.exe -Route lenta
  powershell -NoProfile -File Tools/playtest_v3.ps1 -Exe Saved/Package/Windows/Explored.exe -Route sin_paginas
  ```

  Mismos criterios que la tabla de F6 §3; 0 `Error` en `Saved/Logs/PlaytestV3.log`.

### 3. Comprobaciones de juego que dejó F7

- Junto al agua, pulsar F: si Iria lanza un sedal sin caña ni punto de pesca, quitar la pesca
  antigua (`UFishingComponent` y el mapeo F/R/T de `AExploredCharacter`).
- Mantener H: hoy no muestra nada (reloj de pulsera sin vista); decidir si se quita el mapeo.

## F6 — Relieve y textura del suelo (2026-10-21)

Estado sin motor: terreno regenerado con relieve de media escala (ondulaciones, lomas, surcos de la
colina, rellanos rocosos, dunas, berma y caminos hundidos), normales 60 % suaves / 40 % de cara y
color por vértice en tres tonos por zona (`Art/Island/SM_IslaSal_Terrain.fbx` + `.json`,
133 770 triángulos, pendiente máxima en caminos 26,58°); `island_decor.json` reapoyado (643
instancias, ninguna flotando ni enterrada); `M_TerrainToon` reescrito con la máscara
`T_TerrainStrokes` (`Tools/Unreal/materials_f6.py`). Vista previa en Blender:
`docs/v3/laminas/relieve-antes-despues.png`. Falta llevarlo al motor, en este orden:

```powershell
cd Tools/Textures; uv sync; uv run textures-strokes; cd ../..
powershell -NoProfile -File Tools/unreal_python.ps1 -Script Tools/Unreal/materials_f6.py
powershell -NoProfile -File Tools/unreal_python.ps1 -Script Tools/Unreal/import_island.py
powershell -NoProfile -File Tools/shots.ps1 -Set f4b -Width 1920 -Height 1080
```

(`textures-strokes` escribe `Art/Export/Textures/T_TerrainStrokes.png`, que no se versiona; si
`T_TerrainStrokes` ya existe en el proyecto y se cambia `strokes.json`, lanzar `materials_f6.py` con
`$env:EXPLORED_REIMPORT_STROKES = '1'`.) Copiar de `Saved/Shots` `f4b-cenital`, `f4b-cala`,
`f4b-palmar`, `f4b-faro`, `f4b-islote` y `f4b-atardecer` a `docs/v3/capturas/` como
`f6-relieve-<vista>.png`. Comprobar:

- `materials_f6.py` termina sin `Error`; `M_TerrainToon` compila y tiene los parámetros
  `DarkFactor`, `LightFactor`, `StrokesTile0/1`, `FadeStartM`/`FadeEndM`; `T_TerrainStrokes` es
  lineal (sRGB desactivado), `TC_Grayscale` y con repetición.
- `import_island.py` termina con la caja dentro de tolerancia (`bounds_max_m` z = 35,882) y la malla
  conserva las normales importadas (sombreado con facetas suaves, sin costuras en las aristas).
- En `f6-relieve-cala/palmar/faro`, frente a `f6-suave-*`: lomas y surcos legibles, trazos visibles
  de cerca y apagados a lo lejos (sin moiré ni repetición evidente a 20-40 m), caminos de tierra
  hundidos con borde marcado, manchas de tierra bajo las palmeras; que no se vea más facetado que
  `f4b-*`.
- Nada de la vegetación (HISM de `island_decor.json`) flota ni se hunde, y el faro, la casa, los
  refugios y la balsa siguen apoyados en llano.
- Recorrer los dos caminos hasta el faro (o `Tools/playtest_v3.ps1 -Route competente -Clock 6`): sin
  atascos en los rebordes ni en los taludes de los caminos y sin que el bot se salga del camino.

## Césped y vegetación media (2026-10-21)

Estado sin motor: kit K6 fusionado (`Art/Export/K6_Cesped`, 8 mallas de 25-100 triángulos con el
peso del viento en el alfa del color por vértice); césped alrededor de la cámara
(`AExploredGrassField` sobre `FGrassFieldModel`, `Content/Data/grass.json`), que apoya cada mata en
la malla exacta del terreno (`Content/Data/ground_field.bin`, `uv run island-groundfield`; no usa
`SampleColumn`); `island_decor.json` con 1 940 instancias (643 de K1 + 1 297 de K6: 314 helechos,
406 hierbas altas, 577 flores), ninguna flotando ni enterrada. Estimación con los datos reales (test
del host `GrassRealData`, sin las exclusiones de `island_props.json`): 29 791 matas en el Palmar,
23 773 en la colina del faro, 13 406 en la Cala Varada y 10 026 en la aparición; 0,1-0,25 ms por
celda generada (4 celdas por frame como mucho). Falta llevarlo al motor, en este orden:

```powershell
powershell -NoProfile -File Tools/unreal_python.ps1 -Script Tools/Unreal/materials_cesped.py
$env:EXPLORED_KITS = 'K6_Cesped'
powershell -NoProfile -File Tools/unreal_python.ps1 -Script Tools/Unreal/import_props.py
Remove-Item Env:EXPLORED_KITS
powershell -NoProfile -File Tools/shots.ps1 -Set f4b -Width 1920 -Height 1080
powershell -NoProfile -File Tools/bench.ps1
powershell -NoProfile -File Tools/playtest_v3.ps1 -Route competente -Clock 6
```

Copiar de `Saved/Shots` `f4b-cala`, `f4b-palmar`, `f4b-faro` y `f4b-cenital` a `docs/v3/capturas/`
como `f7-cesped-<vista>.png`. Comprobar:

- `materials_cesped.py` termina sin `Error`; `M_ToonCesped` es de dos caras, opaco, con uso de
  instancias y los parámetros `BaseShade`, `WindSway`, `WindStrength`, `FadeStartCm`, `FadeEndCm`.
- `import_props.py` (solo K6) importa 8 mallas en `/Game/Art/K6_Cesped` con `M_ToonCesped`, colisión
  «ninguna», color por vértice importado y caja/triángulos dentro de tolerancia
  (`Saved/Logs/import_props.json`).
- En el log del juego: `[Decor] 1940 instancias decorativas en 11 mallas`, `[Césped] ... vértices de
  suelo, ... exclusiones, ... palmeras` y `[Césped] N matas en M celdas` con N entre 15 000 y 40 000
  en el Palmar; ningún aviso `[Césped] falta` ni `[Decor] falta la malla`. Si `ground_field.bin` no
  se lee, el log lo dice y la isla queda sin césped (no falla).
- Capturas: ninguna hoja desaparece (dos caras), las matas se mecen desde la punta y no desde la base,
  el césped no tapa caminos, objetos recogibles, la balsa, el faro ni la casa (1,5 m libres), no hay
  césped en arena, roca ni agua, y nada flota ni se hunde en las laderas; en el borde (45-70 m) las
  matas encogen sin un corte brusco.
- `bench.ps1`: el Palmar sigue en ≥ 60 fps de media y ≥ 45 de mínimo a 1080p. Si no llega, por
  orden: `castShadow` false en `tall` y `fern`; `radiusM` 55 con `fade` 35-55; densidades × 0,7.
  Mirar también tirones al caminar (`stat unit`): cada frame con celdas nuevas reescribe ranuras de
  los ISM y marca su estado de render; si se ven, bajar `cellsPerFrame` a 2.
- `playtest_v3.ps1`: el bot llega al faro y a la balsa sin bloqueos (el césped y la decoración no
  tienen colisión ni responden a ningún canal: las trazas de interacción los atraviesan).

Problemas detectados con `SampleColumn` (relieve analítico, hasta ~1,5 m de la malla), sin tocar:
`UExploredPlaytestAuditor` decide «bajo el terreno» con `SampleColumn` y 0,1 m de tolerancia (falsos
positivos y negativos en laderas y caminos hundidos); `UExploredShotSubsystem` y `ExploredPlaytestBot`
sitúan cámaras y objetivos con él. Todos pueden pasar a `FGroundField` (mismo fichero que el césped).
