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

## Ejecución 05:00 UTC

Base: `origin/main` en `99778d0` al empezar y en `37c090a` después de fusionar #56. Cada
rama se rebasó en local sobre `origin/main` para comprobarla. Los arreglos se subieron
encima de la cabeza remota, sin reescribir historia.

| PR | Rama | Decisión | Por qué |
|---|---|---|---|
| #44 | `nocturno/revision-2026-09-27c` | Sin cambios: `necesita-unreal` | No tiene commits nuevos desde la 01:00. |
| #46 | `nube/mundo-2026-09-27` | A la espera de que el director confirme (arreglo y merge de `main` subidos) | Todo lo pedido está resuelto (ver abajo). Solo falta que el director confirme dos notas de la biblia que se le atribuyen. |
| #49 | `nocturno/revision-2026-09-28` | Sigue `necesita-unreal` (arreglo subido) | Los commits nuevos de audio y packs están bien. `M_Ocean` sigue sin poder verificarse aquí. |
| #50 | `nocturno/datos-2026-09-28` | Pendiente del director (arreglo y merge de `main` subidos) | Los tres fallos serios están resueltos. `33f2370` reescribe la biblia 02 §2.2 sin aprobación del director. |
| #51 | `nube/mecanicas-2026-09-28` | Cambios pedidos, duplicada | #53 la contiene entera. Se propone cerrarla. |
| #53 | `nube/mundo-2026-09-28` | Cambios pedidos (arreglo subido) | El modelo sigue contradiciendo la biblia 02 §8 y la 03. Solo está documentado como «pendiente de decisión». |
| #56 | `nube/packs-2026-09-28` | **Fusionada** (`37c090a`) | Solo toca datos JSON, `Tools/Packs`, `Tools/DataCheck` y una hoja de contacto. |
| #57 | `nube/mecanicas-2026-09-28-railes` | **Fusionada** (`7641441`) | Es un modelo puro `FTramwayModel` [F2] con su spec y documentación. Los fallos serios se arreglaron en la rama. |

### Comprobaciones

| PR | HostTests | HostTests + ASan/UBSan | DataCheck `--strict` + pytest | Otros |
|---|---|---|---|---|
| #46 | 727 / 0 fallos (con #57 y `2ce1cc4`) | 727 / 0 fallos | — | — |
| #49 | — | — | 0 errores · 119 passed | Audio: 32 passed, más loudness, pico y determinismo de los 5 sonidos tocados · Packs: 7 passed · Textures: 292 passed |
| #50 | — | — | 0 errores · 162 passed | Localization: 27 passed · `export --check` ok |
| #51 | 673 / 0 fallos | 673 / 0 fallos | — | — |
| #53 | 702 / 0 fallos | 702 / 0 fallos | — | — |
| #56 | — | — | 0 errores · 122 passed | Packs: 7 passed · sha256 y licencia CC0 del Nature Kit verificados |
| #57 | 680 / 0 fallos | 680 / 0 fallos | — | — |

### Arreglos subidos

- **#46 `b75af89`:** si la pleamar bajaba o paraba la lluvia sin nadie a menos de 80 m,
  el chunk daba el cambio por visto y un montón húmedo seguía a 45° al volver alguien.
  Ahora queda pendiente, y un spec nuevo lo comprueba.
- **#49 `ff2cabc`:** el informe mandaba regenerar `lote1-herramientas`, pero `arco` y
  `flecha` están en `lote2-caza`.
- **#50 `e7efed5`:** el C++ no lee `station`, así que las cabezas de pico tapaban
  `lasca_por_golpeo` y `recipiente_de_coco` (20 combinaciones distintas de main). Se
  reordenan las plantillas.
- **#50 `0680cac`:** merge de `main` tras #56, con el conflicto en `test_datacheck.py`
  resuelto.
- **#53 `fab93f3`:** añade el balancín y el ritmo de anegado a la tabla de
  contradicciones.
- **#56 `a1d5337`:** nombres de malla reales en los pendientes del huerto.
- **#57 `baad02a`:** `FromValue` rechazaba el guardado del propio modelo cuando quedaba un
  torno sin vía debajo, y vaciaba la vía entera.
- **#57 `3206bb3`:** la red del vagón queda solo en el servidor, sin predicción en el
  cliente (biblia 08 §1.2). Se quita un «con carga» que el GDD atribuía a la biblia.
- **#57 `092cc6e`:** `Step` simula como mucho 5 s por llamada y reinicia un acumulador
  NaN. Además, los ajustes se sanean: `MaxRiseSteps` cabe en `int8` y no hay divisiones
  por cero.
- **#57 `e21f8f9`:** la §3.5 deja de atribuir al director el detalle del modelo.

### #46: estado

- **Primera respuesta de la integración:** se pidieron dos cosas. Una era llevar la rampa
  del oleaje (hasta el 60 %/75 %) a la biblia con la aprobación del director, o volver al
  20 % plano. La otra era definir el paquete de red de la arena. También se señalaron la
  falta de ráfaga al volver un jugador y el tope de 2000 mm.
- **El autor lo resolvió** con `31dfba8` y `2ce1cc4`:
  - paquete versión 2 con capa de arena, validado entero antes de aplicar;
  - la ráfaga de hasta 4 revisiones;
  - el tope de la avalancha.
- **Merge de `main` tras #57 (`229d853`):** conflicto trivial en `pure_*.txt`.
- **Bloqueo:** `2ce1cc4` añade a la biblia 02 §5.1 y §5.2 dos notas «[director,
  2026-09-27]» que citan un encargo que no está en el repo. Falta que el director las
  confirme. Con eso, se fusiona en la próxima ejecución.

### #57: notas para el director

- Los números que no vienen de la biblia están marcados como propuesta:
  - vagón de 60 kg;
  - 280/900 N;
  - cuerda de 60 m;
  - 17,4° de pendiente máxima.
- Un tramo dañado hace descarrilar al vagón, incluso si está parado encima. La biblia
  02 §9 solo dice «no navegable».

### Duplicados y choques

- #51 está contenida en #53.
- #50 chocaba con #56 en `test_datacheck.py`. Ya se resolvió con `0680cac`.
- #49 y #56 no chocan.
- #46 y #57 añadían cada una una línea a `pure_*.txt`. Después de fusionar #57, se resolvió
  en #46 (`229d853`).

### 00-TODO.md

Sin casillas marcadas:
- #56 solo avanza en parte la de arte de vegetación (L549), porque cubre el huerto.
- #57 es el modelo puro de `Tramway`, pero la casilla de raíles («Nuevo módulo
  `Tramway`: pieza de vía (socket `via`), grafo de tramos, vagón sobre spline con colisión
  contra terreno editable») pide también la pieza, la spline y la integración en el motor.

## Ejecución 07:00 UTC

Base: `origin/main` en `7a3e678` al empezar y en `7593c02` después de fusionar #58 (`e649a96`) y #60.
#44, #49 y #53 no tienen commits ni respuestas nuevas desde la ejecución de las 05:00, así que
mantienen la decisión anterior y no se han vuelto a comprobar. En #46 tampoco ha respondido el
director.

| PR | Rama | Decisión | Por qué |
|---|---|---|---|
| #44 | `nocturno/revision-2026-09-27c` | Sin cambios: `necesita-unreal` | No tiene commits nuevos. |
| #46 | `nube/mundo-2026-09-27` | Sin cambios: falta que el director confirme (merge de `main` subido) | Siguen sin confirmar las dos notas «[director, 2026-09-27]» de la biblia 02 §5.1 y §5.2. |
| #49 | `nocturno/revision-2026-09-28` | Sin cambios: `necesita-unreal` | No tiene commits nuevos. |
| #50 | `nocturno/datos-2026-09-28` | Sin cambios: falta que el director apruebe (merge de `main` subido) | El compost nuevo está bien. Sigue bloqueada por la «Cabeza con Punta» de la biblia 02 §2.2, y además duplica el registro de fauna de #60 (ver abajo). |
| #53 | `nube/mundo-2026-09-28` | Sin cambios: cambios pedidos | No tiene commits nuevos. Ahora también choca con `main` en `gdd_v2.md` (§3.15 de #58). |
| #58 | `nube/mundo-2026-09-28-incendio` | **Fusionada** (`e649a96`) | Es un modelo puro `FWildfireModel` con su spec y documentación. Cumple la biblia 02 §6 y define la red. |
| #60 | `nube/packs-2026-09-28` | **Fusionada** (`7593c02`) | Solo toca datos JSON, `Tools/DataCheck`, `Tools/Packs` y la hoja de contacto. |

### Comprobaciones

| PR | HostTests | HostTests + ASan/UBSan | DataCheck `--strict` + pytest | Otros |
|---|---|---|---|---|
| #46 | 749 / 0 fallos | 749 / 0 fallos | — | Solo se comprobó tras fusionar `main` |
| #50 | — | — | 0 errores · 180 passed | Localization: 27 passed · `export --check` ok |
| #58 | 702 / 0 fallos | 702 / 0 fallos | — | 22 casos de `Explored.Wildfire` |
| #60 | — | — | 0 errores · 131 passed | Packs: 7 passed · `normalize.py` (Blender) solo se ha leído |

### Arreglos subidos

- **#46 `6810c3e`:** merge de `main` después de fusionar #58. Se conservaron las dos líneas en
  `pure_sources.txt` y `pure_specs.txt`.
- **#46 `f42e85f`:** el merge había dejado la §3.15 (incendio) delante de la §3.13 (arena
  viva) en el GDD, y además con una línea horizontal repetida. Se reordena.
- **#50 `2f0a2da`:** merge de `main` después de fusionar #60. En `checks.py` y
  `test_datacheck.py` se conservaron las dos partes: `fauna.json`, `fases_futuras.json` y
  `fauna_terrestre.json` en `DATA_FILES`, y los tests de red de fauna y del `rig` de packs.
- **#50 `a86a43e`:** después del merge, DataCheck daba 6 errores. El aislamiento de
  `fases_futuras.json` tomaba como fase 1 a `cerdo`, `cabra` y `gallina`, que en
  `fauna_terrestre.json` llevan `phase: F2` y en el catálogo de packs están en `discarded`
  o `pending`. Ahora esas partes quedan fuera. Dos tests nuevos: uno comprueba que los datos
  reales pasan y el otro que una especie `AA` con un id del borrador sigue fallando.

### #58: notas

- La biblia 08 §2.6 pide recuperar hasta 4 iteraciones al acercarse un jugador. Aquí el
  fuego de un chunk congelado sigue donde estaba, sin ponerse al día. Es defendible, pero
  hay que decirlo en la biblia 08 o añadir la recuperación por chunk.
- El viento se aplica como ±25 puntos y no como ×1,25. Es una interpretación y está
  señalada en el GDD §3.15.
- Sigue abierta la contradicción entre los 25 días del matorral y el rebrote de tala de
  3 + 2 días. La decide el director.

### #50 y #60: fauna duplicada

- `fauna.json` (#50) y `fauna_terrestre.json` (#60, ya en `main`) registran las mismas
  especies terrestres y no coinciden en todo:
  - la cabra es `cabra_montes` en #50 y `cabra_salvaje` en #60;
  - el cerdo salvaje se llama en inglés «Wild pig» en #50 y «Wild boar» en #60.
- La biblia 02 §11.2 dice «cabra montés» y la biblia 04 dice «cabra salvaje».
- Hay que dejar una sola fuente. `packs.py` tiene que leer los ids de fauna de ella, así que
  `fauna_terrestre.json` debería desaparecer o quedar como vista derivada, y hay que
  alinear el id de la cabra. Queda pedido en #50.

### Duplicados y choques

- #50 chocaba con #60 en `checks.py` y `test_datacheck.py`. Ya se resolvió con el merge.
- #46 chocaba con #58 en `pure_*.txt`. Ya se resolvió con el merge.
- #53 choca con #58 en `gdd_v2.md`. No se resuelve aquí porque la PR tiene cambios pedidos
  sin atender. Además, #53 usa §3.13 y §3.14, y #46 también usa §3.13. Se le ha pedido
  que renumere.

### 00-TODO.md

Sin casillas marcadas:
- #58 cubre el modelo de «contagio de fuego entre celdas» (Fuego y clima), pero falta
  conectarlo al motor. La casilla del rebrote pide además compartir el temporizador con la
  tala, y eso no está hecho.
- #60 solo avanza el arte de la fauna, porque el cerdo salvaje aún no está importado en
  Unreal.

## Ejecución 09:00 UTC

Base: `origin/main` en `6053172` al empezar y en `2b6d17c` después de fusionar #65. Solo había
una PR abierta.

| PR | Rama | Decisión | Por qué |
|---|---|---|---|
| #65 | `fix/amarre-nan` | **Fusionada** (`2b6d17c`) | Solo toca el modelo puro `FBoatModel::Moor` y su spec. Sustituye la comparación en positivo `!(x > 0)`, que con matemáticas rápidas no descarta NaN, por `FMath::IsFinite` sobre el cabo y el poste. Es un arreglo y no añade mecánica, así que no hay replicación nueva que definir. |

### Comprobaciones

| PR | HostTests | HostTests + ASan/UBSan | DataCheck `--strict` + pytest | Otros |
|---|---|---|---|---|
| #65 | 819 / 0 fallos | 819 / 0 fallos | — | El autor dice que en el editor pasan 880/880 con `Tools/test.ps1` |

### Arreglos subidos

- #65: el cambio también rechaza un cabo infinito con un poste finito, que antes amarraba.
  Se añaden los casos «cabo infinito» y «poste infinito» a `BoatSpec.cpp`. Con el
  `BoatModel.cpp` anterior, «cabo infinito» falla, así que el test caza el cambio.
  Commit `4c4793b`.

### Nota para el director

- Una posición del barco con NaN ya no la descarta `Moor`: la distancia NaN hace falsa la
  comparación `>` y el barco amarraría. Antes tampoco se descartaba en el editor, porque las
  matemáticas rápidas anulaban el truco. Si interesa, se puede comprobar `IsFinite` sobre
  `State.LocationCm` o sobre la distancia.

### 00-TODO.md

Sin casillas marcadas: #65 corrige un fallo y no completa ninguna tarea de la lista.
