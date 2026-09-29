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
