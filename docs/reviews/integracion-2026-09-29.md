# Integración del 2026-09-29

## Ejecución 01:00 UTC

Base: `origin/main` en `20ed387` al empezar. Solo #121 es nueva. Las demás PR abiertas no
tienen commits desde su última revisión: #117 (`b819a28`) se revisó a las 23:22 y #118
(`f108840`) a las 23:41.

| PR | Rama | Decisión | Por qué |
|---|---|---|---|
| #121 | `nube/packs-2026-09-29` | **Fusionada** (`5fb68df`) | Lote 11 de packs: clavos y yunque (KayKit RPG Tools Bits, ya declarado en `Tools/Packs/packs.json`). Solo toca el catálogo y la hoja de contacto. Los `gameId` existen (`items.json` y `building_pieces.json`) y la nota del yunque cuadra con su coste (4 basalto y 1 lingote de hierro). Le añadí la fila del lote 11 en `Tools/Packs/README.md`, que faltaba (`f833e26`). |
| #70, #72, #75, #81, #82, #84–#87, #90, #96, #111, #114, #117, #118 | — | Sin cambios | Siguen con `necesita-unreal` y sin commits nuevos. |

### Comprobaciones

| PR | DataCheck `--strict` + pytest | Otros |
|---|---|---|
| #121 (al día con `main`) | 0 errores, 0 avisos · 550 passed | Packs: 64 passed, 1 skipped · ruff sin avisos |

La PR no tiene checks de CI en GitHub (no toca rutas que los disparen), así que las
comprobaciones son las locales.

### 00-TODO.md

- Ninguna casilla nueva en `[x]`. #121 se anota en la nota «en parte» de la casilla de arte
  del GDD §7.1.

## Ejecución 03:00 UTC

Base: `origin/main` en `f8a3fb3` al empezar. Son nuevas #123 y #124. Las demás PR abiertas
(#70, #72, #75, #81, #82, #84–#87, #90, #96, #111, #114, #117 y #118) no tienen commits
desde su última revisión y siguen con `necesita-unreal`.

| PR | Rama | Decisión | Por qué |
|---|---|---|---|
| #123 | `nube/mundo-2026-09-29` | **Fusionada** (`6d5c1b7`) | `FFellingModel::ApplyWindToFall` y `ComputeCrush`, con sus specs. Solo toca el modelo puro, su spec y documentación. Los números cuadran con la biblia 02 §1.2: ±20° por viento (±15° la palmera) y 40 % de integridad a la construcción de palma y bambú. La replicación sigue la biblia 08 (la tala es del servidor): lo resuelve el servidor y los clientes reciben la dirección ya calculada, como explica `docs/tecnico/tala-integracion.md`. Los specs cubren los bordes de verdad: el borde estricto del radio, la punta, detrás del tocón, las entradas no finitas, los ids repetidos y la posición lejos del origen. |
| #124 | `nocturno/revision-2026-09-29` | **Fusionada** (`5d4a3b2`) | Arreglos de la revisión nocturna en modelos puros, cada uno con su spec: `RoundClamped` ya no tiene UB en los topes de int64; un derribado que se queda solo cierra el estado; `CompactStrip` exige puntos dentro del mundo y una huella acotada; minería rechaza enums fuera de rango; y la revisión de vigas de `FMineHazardModel` ya no depende de las consultas const (con `bBeamsDirty` aparte). El sonido de tallar madera pasa a saltos de adherencia y deslizamiento a través de la vara, con tests de espectro y de sonoridad. |

### Comprobaciones

| PR | HostTests | HostTests ASan/UBSan | DataCheck `--strict` + pytest | Otros |
|---|---|---|---|---|
| #123 (al día con `main`) | 1319 casos, 0 fallos | 1319 casos, 0 fallos, sin avisos | 0 errores, 0 avisos · 550 passed | CI en verde (7/7) |
| #124 (al día con `main`) | 1312 casos, 0 fallos | 1312 casos, 0 fallos, sin avisos | — (no toca datos) | Audio: ruff y basedpyright sin avisos · 296 passed, 106 skipped, cobertura del 93 % · CI en verde (7/7) |

Las dos PR tocan ficheros distintos, así que no hubo conflicto entre ellas. Nota sobre el
entorno: los tests de música de `Tools/Audio` necesitan `fluidsynth` en el PATH. La nube no
lo traía y tuve que instalarlo con apt antes de ejecutarlos.

### 00-TODO.md

- Ninguna casilla nueva en `[x]`. La de viento y colisión al talar (biblia 02 §1.2) sigue en
  «en parte» porque falta engancharla al actor y a `UBuildingSubsystem`. Su nota ahora apunta
  a `6d5c1b7` (PR #123) en vez de a la rama.

## Ejecución 05:00 UTC

Base: `origin/main` en `0902e94` al empezar. Son nuevas #126, #127 y #128. Las demás PR
abiertas (#70, #72, #75, #81, #82, #84–#87, #90, #96, #111, #114, #117 y #118) no tienen
commits desde su última revisión y siguen con `necesita-unreal`.

| PR | Rama | Decisión | Por qué |
|---|---|---|---|
| #126 | `nube/packs-2026-09-29b` | **Fusionada** (`7edef53`) | Lote 12 de packs: lingotes de cobre, hierro y aluminio (KayKit Resource Bits, ya declarado en `Tools/Packs/packs.json`). Solo toca el catálogo, la hoja de contacto y el README de Packs. Los tres `gameId` existen en `items.json`. En la hoja, el recoloreado a la paleta se lee bien. |
| #128 | `nube/mecanicas-2026-09-29` | **Fusionada** (`7a51a70`) | Spec nuevo en `FTerrainEditModelSpec`: echar tierra en la esquina de cuatro chunks no cambia nada fuera de la esfera más una celda, el modelo guarda solo lo cambiado y marca sucios justo los chunks que leen esas muestras, a los dos lados de la frontera. También añade la frase correspondiente en el GDD v2 §3.4. |
| #127 | `nube/mundo-2026-09-29` | **Fusionada** (`59a121a`) | `FFelledDriftModel`, un modelo puro con su spec (`Explored.FelledDrift`), para lo que cae al agua al talar (biblia 02 §1.5). Decide si flota o se hunde según el calado, lo deja derivar con la corriente, lo vara en el punto de contacto buscado por bisección y sin oscilar, lo refloata con la marea y lo entrega como `madera_flotante`. El paso fijo es de 0,5 s y tiene topes de pasos, de piezas y de velocidad. La replicación está definida según la biblia 08: solo simula el servidor y el actor usa el movimiento replicado normal. Los specs cubren casos reales: independencia de los fotogramas, la barra de arena de una columna, coordenadas negativas, entradas no finitas y el coste por consultas. Le añadí `fafc1b4`: la biblia dice «`Flota` heredado del material», pero en `items.json` esa propiedad de crafteo solo la llevan `madera_blanda` y `madera_flotante`, mientras que el modelo usa una tabla de densidades en la que también flotan `tronco_pequeno`, los cocos, las hojas y demás. Lo dejé escrito en el modelo y en el GDD §3.12 como decisión pendiente del director. No es un fallo del modelo: el GDD da 500 kg/m³ para el tronco. Durante la revisión, la rutina autora subió `3bdd86d` (reusa el hueco de las piezas resueltas para que el array no crezca en una sesión larga, con su spec), así que revisé y probé de nuevo ese head antes de fusionar. |

### Comprobaciones

| PR | HostTests | HostTests ASan/UBSan | DataCheck `--strict` + pytest | Otros |
|---|---|---|---|---|
| #126 (rebasada sobre `main`) | — (no toca Source) | — | 0 errores, 0 avisos · 550 passed | Packs: 64 passed, 1 skipped · ruff sin avisos |
| #128 (rebasada sobre `main`) | 1324 casos, 0 fallos | 1324 casos, 0 fallos, sin avisos | — (no toca datos) | CI en verde (7/7) |
| #127 `d7ec547` (rebasada) | 1346 casos, 0 fallos | 1346 casos, 0 fallos, sin avisos | — | — |
| #127 `24dedd4` + `main` con #128 | 1349 casos, 0 fallos | 1349 casos, 0 fallos, sin avisos | — | CI en verde (7/7) |

#127 y #128 tocan secciones distintas de `gdd_v2.md` y se fusionaron sin conflicto.

### 00-TODO.md

- Ninguna casilla nueva en `[x]`.
- Nueva casilla «en parte» en Tala y recolección para la caída al agua (biblia 02 §1.5,
  PR #127): falta el subsistema del motor y la decisión sobre `Flota`.
- #126 se anota en la nota «en parte» de la casilla de arte del GDD §7.1.

## Ejecución 07:00 UTC

Base: `origin/main` en `f6a9c8f` al empezar. Es nueva #130. #70 tiene dos commits nuevos
(`71fd0ff` y `0c9ee8b`, de las 05:44 UTC). Las demás PR abiertas (#72, #75, #81, #82,
#84–#87, #90, #96, #111, #114, #117 y #118) no tienen commits desde su última revisión y
siguen con `necesita-unreal`.

| PR | Rama | Decisión | Por qué |
|---|---|---|---|
| #130 | `nube/packs-2026-09-29c` | **Fusionada** (`4d2a46c`), solo con la mesa | Lote 13 de packs (kit de construcción). La mesa de cartografía (KayKit `table_medium`, 1,7 × 1,7 × 0,85 m) está bien. Añadí `0dd29dd`, que pasa a descartes dos piezas que no cuadran con `docs/art/kit-construccion.md`. **Suelo de madera:** el `floor.glb` de Kenney es un palé con huecos de 20 cm de canto y el kit pide `FLOOR_T = 0,15 m`, así que las paredes se hundirían 5 cm y quedaría un escalón junto a los suelos de bambú y piedra. **Puerta de madera:** `wall-wood-door.glb` es un panel de pared entero de 2 × 2 m con la hoja fija, pero el socket `puerta` es solo la hoja de 0,96 × 2,02 m con bisagra en x = 0, dentro de una pared de 2,50 m. `size_factor` solo escala de forma uniforme, así que ninguna de las dos se arregla desde el catálogo. Añadí también la fila del lote 13 al README de Packs. |
| #70 | `nocturno/revision-2026-09-28` | Sin fusionar (`necesita-unreal`) | Revisé los commits nuevos. `71fd0ff` es correcto: `DigSphere` rechaza un `MaxVolume` NaN con `IsFinite` explícito, porque con matemáticas rápidas `!(x >= 0)` lo dejaba pasar, y rechaza radios mayores que `MaxBrushExtent`. Lleva su spec. El fallo sigue en `main`, así que comenté en la PR que conviene sacarlo en una PR aparte de modelo puro para poder fusionarlo desde la nube. |

### Comprobaciones

| PR | HostTests | HostTests ASan/UBSan | DataCheck `--strict` + pytest | Otros |
|---|---|---|---|---|
| #130 (rebasada sobre `main`, con `0dd29dd`) | — (no toca Source) | — | 0 errores, 0 avisos · 550 passed | Packs: ruff sin avisos · 64 passed, 1 skipped · CI en verde (7/7) |

### 00-TODO.md

- Ninguna casilla nueva en `[x]`. #130 se anota en la nota «en parte» de la casilla de arte
  del GDD §7.1.

## Ejecución 09:00 UTC

Base: `origin/main` en `0781d67` al empezar. Es nueva #132. Las demás PR abiertas (#70, #72,
#75, #81, #82, #84–#87, #90, #96, #111, #114, #117 y #118) no tienen commits desde su última
revisión y siguen con `necesita-unreal`.

| PR | Rama | Decisión | Por qué |
|---|---|---|---|
| #132 | `nocturno/revision-2026-09-29` | **Fusionada** (`c301ee7`) | Solo toca `Tools/Audio` y un informe en `docs/reviews/`. `amb_rain_on_thatch` deja de ser granos de ruido en 0,7-3 kHz y pasa a golpes sordos contra la paja (`_thatch_hit`), escorrentía grave que crece con el cuadrado de la intensidad, goterones de árbol sobre el armazón (`_heavy_drop`) y una gotera fija en una cáscara de coco (`_shell_drip`). Los goteos del alero conservan las burbujas de Minnaert y ahora aceleran cuando arrecia. Revisé los límites de los índices (`rub_at`, `rub_len` frente a `dn`) y las divisiones por la intensidad, que está acotada a ≥ 0,55: no hay desbordes. Los dos tests nuevos miden cosas que la versión anterior no cumplía (≥ 30 % en 150-800 Hz, antes un 3 %; tono fijo de la cáscara en 650-900 Hz) y son deterministas porque el generador usa `rng_for(name)`. El informe de la revisión nocturna solo lista notas LOW sin cambios de código. |

### Comprobaciones

| PR | HostTests | HostTests ASan/UBSan | DataCheck `--strict` + pytest | Otros |
|---|---|---|---|---|
| #132 (rebasada sobre `main`, sin conflictos) | — (no toca Source) | — | — (no toca datos) | Audio: ruff sin avisos · basedpyright 0 errores · pytest 298 passed, 106 skipped, cobertura 93 % (`test_fire_rain_palms.py`: 9 passed, sin saltos; hizo falta instalar `fluidsynth` con apt) · CI en verde (7/7) |

### 00-TODO.md

- Ninguna casilla nueva en `[x]`: el TODO no tiene casilla para los ambientes de lluvia.

## Ejecución 11:00 UTC

Base: `origin/main` en `6305d52` al empezar. Hay dos PR nuevas, #134 y #135. Las demás PR
abiertas (#70, #72, #75, #81, #82, #84–#87, #90, #96, #111, #114, #117 y #118) no tienen
commits desde su última revisión y siguen con `necesita-unreal`.

| PR | Rama | Decisión | Por qué |
|---|---|---|---|
| #134 | `nube/packs-2026-09-29d` | **Fusionada** (`d9b5eca`), con `66395e6` | Lote 14 de packs: 8 iconos de logro de Kenney (arco, bandera, escudo, muralla, herramienta, viga, trueque y pueblo). Los ocho logros existen en `achievements.json`, y en la hoja de contacto las siluetas se leen bien con los dos tintes. `structure_church` se usa para «pueblo» y se descarta para «templo». No es una contradicción: sin cruz, el icono se ve como un grupo de casas. Añadí `66395e6` porque el motivo de corral y empalizada era falso. Decía «se cataloga cuando el logro entre en `achievements.json`», pero esos logros ya están ahí (F2). Lo que falta es que las piezas dejen de tener `propuesta: true` en `fases_futuras.json`. El alcance del lote también ponía vagón como pieza de fase futura, cuando lo que le falta es una silueta que encaje. La hoja PNG todavía conserva la frase antigua en el subtítulo. |
| #135 | `nube/mundo-2026-09-29-persistencia` | **Fusionada** (`f2b228e`) | Añade `FVegetationClockModel`, un modelo puro con solo `CoreMinimal.h` y dependencias de `Save/` y `WorldGen/` que ya están en `pure_sources.txt`. Es la sección `vegetationClock`, que guarda la hora de tala y el trabajo de pala de cada tocón. También añade `NeedsSave`, `SaveCell` y `LoadCell` en `FGroundBranchModel`. Lo revisé así: la búsqueda binaria usa un orden que no distingue mayúsculas, igual que el `==` de `FName`; `Load` comprueba los rangos antes de estrechar a `int32` y rechaza claves repetidas; `LoadCell` exige series crecientes por debajo de `NextSerial`, con lo que `Pick` no puede recoger la rama equivocada; `Reconcile` aplica «más tarde, nunca antes». Los 42 días de la palmera que cita el GDD (12 + 30) coinciden con la tabla de la §3.12. Los specs cubren el orden estable del texto, que guardar y cargar no mueva el rebrote, las ramas que no se mueven tras talar y el rechazo de ficheros manipulados. Es solo guardado en el servidor, sin estado replicado nuevo. |

### Comprobaciones

| PR | HostTests | HostTests ASan/UBSan | DataCheck `--strict` + pytest | Otros |
|---|---|---|---|---|
| #134 (al día con `main`, con `66395e6`) | — (no toca Source) | — | 0 errores, 0 avisos · 550 passed | Packs: ruff sin avisos · 64 passed, 1 skipped · CI en verde (7/7) |
| #135 (al día con `main`) | 1366 casos, 0 fallos | 1366 casos, 0 fallos, sin avisos | — (no toca datos) | CI en verde (7/7) |

### 00-TODO.md

- No se marca ninguna casilla nueva como `[x]`.
- #135 se anota en la nota «en parte» del tocón con rebrote. Falta engancharlo a
  `USaveSubsystem`.
- #134 se anota en la nota «en parte» de la casilla de arte del GDD §7.1.

## Ejecución 13:00 UTC

Base: `origin/main` en `5bfa5dd` al empezar. Hay dos PR nuevas, #137 y #138. Las demás PR
abiertas (#70, #72, #75, #81, #82, #84–#87, #90, #96, #111, #114, #117 y #118) no tienen
commits desde su última revisión y siguen con `necesita-unreal`.

| PR | Rama | Decisión | Por qué |
|---|---|---|---|
| #137 | `nube/mundo-2026-09-29-robustez` | **Fusionada** (`8ed2845`), por otra sesión mientras corrían mis pruebas | Revisada y aprobada por mi parte. Solo toca modelos puros, sus specs y documentación. `FSandModel::Brush` rechaza un radio mayor que `MaxBrushRadius` (8 m, el lado de un chunk; unas 3 200 columnas en celdas de 0,25 m). Un radio NaN ya lo paraba `!(Radius > 0)`. Los specs comprueban que por encima del tope no se consulta ni una vez la altura base, y que en el tope, cruzando cuatro chunks en coordenadas negativas, la masa neta vuelve a cero. `FFelledDriftModel::Release` entrega lo que flota con `HandOver` y deja lo quieto en el estado nuevo `Released`, que no cuenta como activo (`IsActive`), así que `Advance` no lo refloata ni gasta consultas y `AddPiece` reutiliza su hueco. Es solo del servidor, sin estado replicado nuevo. El merge usó el head original `02ee6ab`; el árbol resultante es idéntico al de mi rebase `aaf7f7b`, que es el que probé. |
| #138 | `nube/packs-2026-09-29e` | **Fusionada** (`dd56886`), con `5e19070` | Cuatro pendientes nuevos en `packs_catalogo.json`: carretilla, escalera de mano, espantapájaros y horno de fundición. Comprobé que los cuatro ids existen en `building_pieces.json`/`items.json`, que no estaban ya en el catálogo y que los packs citados (Fantasy Town Kit, Restaurant Bits) están en `Tools/Packs/packs.json`. Añadí `5e19070`: la carretilla citaba un «lote 15» que no existe en `lotes` (el último es el 14), y las otras tres entradas no citan lote. |

### Comprobaciones

| PR | HostTests | HostTests ASan/UBSan | DataCheck `--strict` + pytest | Otros |
|---|---|---|---|---|
| #137 (rebasada sobre `main`) | 1370 casos, 0 fallos | 1370 casos, 0 fallos, sin avisos | — (no toca datos) | CI en verde (7/7) en el head original |
| #138 (al día con `5bfa5dd`, con `5e19070`) | — (no toca Source) | — | 0 errores, 0 avisos · 550 passed | Packs: 64 passed, 1 skipped · CI en verde (7/7) |

### 00-TODO.md

- No se marca ninguna casilla nueva como `[x]`: #137 cierra pendientes de revisión, no
  tareas del TODO.
- #137 se anota en las notas «en parte» de la caída al agua de la tala y de la arena viva.
