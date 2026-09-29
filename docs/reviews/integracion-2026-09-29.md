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
