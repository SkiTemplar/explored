# Integración — 2026-09-28

Rutina de integración en la nube (sin Unreal). PR abiertas procesadas de la más antigua a
la más nueva, cada una rebasada sobre `origin/main` (`374d835`) antes de comprobarla.

## Ejecución 01:00 UTC

| PR | Rama | Decisión | Por qué |
|---|---|---|---|
| #44 | `nocturno/revision-2026-09-27c` | No fusionada: `necesita-unreal` | Toca `WorldGenCommandlet.cpp` (editor) y `M_Ocean` vía `build_materials.py`. Todo lo que se puede comprobar aquí está en verde. |
| #45 | `nube/packs-2026-09-27` | **Fusionada** (`9a51c41`) | Solo datos JSON, `Tools/Packs`, `Tools/DataCheck` y una hoja de contacto. |
| #46 | `nube/mundo-2026-09-27` | Cambios pedidos | `FSandModel` puro y con los tests en verde, pero contradice biblia 02 §5.2–5.3 y 08 §2.6 (ver abajo). |
| #48 | `nube/packs-2026-09-28` | **Fusionada** (`45c9f37`) | Incluye #45 más los lotes 2 y 3 y los descartes del kit de construcción. Se fusionó `main` en la rama (sin conflictos) para que GitHub la aceptara. |
| #49 | `nocturno/revision-2026-09-28` | No fusionada: `necesita-unreal` | Toca `M_Ocean` vía `build_materials.py`. El audio de clavar está en verde. |

### Comprobaciones

| PR | HostTests | HostTests + ASan/UBSan | DataCheck `--strict` + pytest | Otros |
|---|---|---|---|---|
| #44 | 653 / 0 fallos | 653 / 0 fallos | 0 errores · 108 passed | Audio: 55 passed (6 ficheros) · Textures: 290 passed |
| #45 | — | — | 0 errores · 115 passed | Packs: 7 passed |
| #46 | 682 / 0 fallos | 682 / 0 fallos | 0 errores · 108 passed | — |
| #48 | — | — | 0 errores · 119 passed | Packs: 7 passed |
| #49 | — | — | 0 errores · 108 passed | Audio: 53 passed (6 ficheros) · Textures: 290 passed |

Audio: se lanzaron `test_garden_cartography`, `test_loudness`, `test_basic_properties`,
`test_determinism`, `test_catalog_contents` y `test_ui_construction`, que cubren lo que
cambian las dos PR. La pasada completa tarda demasiado para esta rutina.
`Tools/Textures` no tiene `pyproject.toml`: se ejecutó con
`uv run --with pytest --with numpy --with pillow python -m pytest`.

### #46: contradicciones con la biblia

- **Oleaje:** la biblia 02 §5.2 rellena un 20 % (35 % en marea viva) por medio ciclo de
  marea, hacia la altura original. La PR difunde un 8 % de la diferencia cada 100 ms
  entre columnas vecinas: un hoyo se cierra en segundos, no en ciclos de marea.
- **Anclaje:** según 02 §5.3, nada se mueve a menos de 1 m de un tablón o un pilote. En la
  PR solo se congela la huella, y una columna (0,25 m) aguanta hasta 60°.
- **Red:** 08 §2.6 fija 80 m de radio, 64 celdas/s por chunk y el oleaje una vez por medio
  ciclo, con un presupuesto de 0,6 kbps. La PR usa 24 m, 4 096 columnas por paso de
  100 ms y oleaje continuo.
- Se piden dos alternativas: ajustar el modelo a la biblia, o cambiar la biblia en la
  misma PR con la aprobación del director.

### Duplicados y choques

- #48 contiene el commit de #45. Se fusionó primero #45 y después #48, y ninguna se
  cerró como duplicado.
- #44 y #49 tocan zonas distintas de `build_materials.py` (`M_Ocean`) y se fusionan
  entre sí sin conflicto (`git merge-tree`).
- #46 no choca con ninguna.

### 00-TODO.md

Sin casillas marcadas. Lo fusionado (#45 y #48) es la curación y normalización de
packs CC0 para herramientas, caza y comida. La casilla de arte de GDD §7.1 pide
vegetación, mobiliario y props seleccionados y retocados, y además importados en
`/Game/Packs`. Eso sigue pendiente en local (ver «Import en Unreal» en
`Tools/Packs/README.md`).

## Ejecución 03:00 UTC

Base: `origin/main` en `e6ebfe1`. #44, #46 y #49 no tienen commits ni respuestas nuevas
desde la ejecución de la 01:00, así que mantienen la decisión anterior y no se han vuelto a
comprobar.

| PR | Rama | Decisión | Por qué |
|---|---|---|---|
| #44 | `nocturno/revision-2026-09-27c` | Sin cambios: `necesita-unreal` | Ver la ejecución de la 01:00. |
| #46 | `nube/mundo-2026-09-27` | Sin cambios: cambios pedidos | Sigue sin respuesta a las contradicciones con la biblia 02 §5 y 08 §2.6. |
| #49 | `nocturno/revision-2026-09-28` | Sin cambios: `necesita-unreal` | Ver la ejecución de la 01:00. |
| #50 | `nocturno/datos-2026-09-28` | Cambios pedidos (dos arreglos subidos) | Desaparece el hacha de piedra; el pico rescatado pierde la chapa; la fauna no define su red (ver abajo). |
| #51 | `nube/mecanicas-2026-09-28` | Cambios pedidos | Está incluida en #53 con los mismos commits. Se propone cerrarla y seguir en #53. |
| #53 | `nube/mundo-2026-09-28` | Cambios pedidos (dos arreglos subidos) | Contradice la biblia 02 §8, se pone ella misma la etiqueta de aprobación y no define la replicación (ver abajo). |
| #54 | `nube/paleta-2026-09-28` | **Fusionada** (`d40a670`) | Solo `Tools/Textures` y `docs/art`. Solo añade las filas 11 y 12; las filas 0-10 y 13-15 del atlas salen idénticas byte a byte. |

### Comprobaciones

| PR | HostTests | HostTests + ASan/UBSan | DataCheck `--strict` + pytest | Otros |
|---|---|---|---|---|
| #50 | — | — | 0 errores · 149 passed | Localization: 27 passed · `l10n export --check` ok |
| #51 | 673 / 0 fallos | 673 / 0 fallos | — | — |
| #53 | 701 / 0 fallos | 701 / 0 fallos | — | Con los arreglos subidos (`24db08c`): 701 / 0 fallos |
| #54 | — | — | 0 errores · 119 passed | Textures: 292 passed |

### Arreglos subidos

Los arreglos se aplicaron con cherry-pick sobre la cabeza original de cada rama, sin
reescribir historia.

- **#54 `cb012e5`:** `to_json()` escribe también `recogible` en las familias `terreno` y
  `entorno`, que antes daban `KeyError`.
- **#53 `bbf5fa7`:** las constantes físicas del casco y del astillero pasan a ser alias de
  las de `FBoatModel`.
- **#53 `24db08c`:** `Moor` rechaza un poste que no sea finito. Antes, el NaN llegaba a la
  posición.
- **#50 `54c9175`:** la comprobación de progresión de picos respeta la fase del afloramiento
  y no deja saltar niveles. Añade dos tests de regresión.
- **#50 `f303776`:** inglés según el glosario de la biblia 07 §5.2.

### #50: fallos serios

- **Hacha de piedra.** La plantilla `pico` va antes que `hacha` y acepta cabezas
  `Contundente/Rigido ≥ 3` de piedra, así que cualquier piedra con mango da un pico. Eso
  contradice la biblia 01 (nivel 1 con hacha de piedra) y la biblia 02 §1.2 y §2.2.
- **Pico rescatado.** Pierde `chapa_fuselaje`, en contra de la biblia 02 §2.2 y del GDD v2 §4.
- **Red.** `fauna.json` y `fases_futuras.json` no dicen cómo va cada cosa en red, como pide
  la biblia 08 §2.7. Gaviota y fragata no encajan ni en la fauna de ambiente ni en la
  fauna replicada.
- Los binarios de localización pasan de UTF-16 a UTF-8, que es la convención documentada.
  Las 303 entradas son idénticas.

### #51 y #53: fallos serios

- El GDD marca §3.13 y §3.14 como aprobadas por el director sin que conste esa aprobación
  del detalle. El alcance (barcos por piezas, astillero) sí está aprobado.
- Contradicen la biblia 02 §8 en cinco puntos:
  - el catálogo de piezas;
  - el 95 % para anegarse y el 115 % para hundirse;
  - la integridad 1–100 dentro de `FBuildingModel`, sin un sistema aparte;
  - la vía de agua de 0,5 L/s al romperse una unión;
  - la botadura a 8 s/t. Con Coulomb, la balsa de 6 troncos necesita 9 personas en arena
    seca.
- Los pesos no cuadran con la biblia 03: `tronco_pequeno` pesa 8 kg y en la PR 72,6 kg.
  Además, `clavo` no existe.
- No hay sección de red. El amarre y la ficha no viajan en `FExploredBoatNetState`, pero el
  cliente extrapola con `FBoatModel::Step`.

### Duplicados y choques

- #53 contiene los tres commits de #51 sin cambios.
- #50 choca con `packs` de main en `checks.py` y `test_datacheck.py`. El conflicto se
  resolvió en el rebase de revisión conservando las dos partes, pero la rama remota aún
  tiene que fusionar `main`.
- #54 no choca con ninguna.

### 00-TODO.md

Sin casillas marcadas. #54 no cierra ninguna tarea del TODO. La casilla del `pico` (Crafteo
e inventario) la toca #50, que no se ha fusionado.
