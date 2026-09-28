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

## Ejecución 11:00 UTC (larga, solapada con las siguientes)

Empezó con `origin/main` en `266a8cc` y 23 PR abiertas (#70–#92). Cada PR la revisó un
subagente en un worktree propio, rebasada en local sobre `origin/main`. La máquina (4 CPU)
estuvo saturada durante horas, con cargas de 40 a 80, así que esta ejecución se ha solapado
con las de las 13:00 y las 15:00. Esas ejecuciones fusionaron por su cuenta #83, #99 (la
primera parte de #70), #76, #77, #100, #97, #71 y #94. Aquí no se han vuelto a tocar. Antes
de cada fusión se volvió a comprobar la cabeza de la rama y se fusionó con `expectedHeadSha`.
Las PR #93 y siguientes quedan para la próxima ejecución.

| PR | Rama | Decisión | Por qué |
|---|---|---|---|
| #70 | `nocturno/revision-2026-09-28` | No fusionada: la lleva otra ejecución | Main ya tiene la primera parte por #99. Lo que queda toca `M_Terrain` a través de `build_materials.py` (material), así que necesita Unreal. |
| #71 | `nube/mundo-2026-09-28-cocos` | Fusionada en otra ejecución (`b7037a9`) | Aquí se pidieron cambios porque la tala soltaba `coco_verde` (biblia 02 §1.2 y §13.1) y las rachas no persistían. El autor lo corrigió en `ef8b2d6` y `eda1d95`: 850/0 con y sin sanitizer. |
| #72 | `debug/playtest-automatico` | `necesita-unreal` | El auditor, el bot y el `ShotSubsystem` son código de motor. Los fallos funcionales están en la revisión (ver abajo). |
| #73 | `nube/packs-2026-09-28` | **Fusionada** (`4a5b1ad`) | Solo datos, `Tools/Packs`, `Tools/DataCheck` y hojas de contacto. Ahora trae también el lote 7 (cuarzo y taro). |
| #74 | `nocturno/datos-2026-09-28b` | **Fusionada** (`d184e03`) | Solo datos, DataCheck y docs. Coincide con la biblia 02 §2.4, §2.7 y §10.1. |
| #75 | `arte/fondo-marino` | `necesita-unreal`, con fallos serios | Toca `WorldGenCommandlet.cpp` (ver abajo). |
| #76 | `nube/mecanicas-2026-09-28-entradas` | Fusionada en otra ejecución (`a6108d3`) | Aquí salía verde: 822/0 con y sin sanitizer. |
| #77 | `claude/update-todo-checklist-g6y9m0` | Fusionada en otra ejecución (`77e3f53`) | Lleva `5466c99` de esta revisión: la casilla de riego de `FFarmModel` vuelve a «en parte», porque `DryDaysToDie = 4` mata la planta y la biblia 02 §10.1 dice que no muere. |
| #78 | `claude/implement-combat-model-h1-qweffn` | **Fusionada** (`b030518`) | `FCombatModel` puro con su spec, `combat.json` y DataCheck. Los números cuadran con la biblia 05 §3 y §5. |
| #79 | `claude/h4-text-content-iytug5` | `necesita-unreal` (tres arreglos subidos) | `AchievementsDataSpec.cpp` solo corre en el editor. |
| #80 | `claude/complete-h5-achievements-localization-be97tx` | `necesita-unreal`, pendiente del director | Toca `AchievementsSubsystem.cpp` y `Game.archive`. Reescribe la biblia 07 sin aprobación (ver abajo). |
| #81 | `claude/livestock-pens-phase-2-be63wr` | `necesita-unreal` (dos arreglos subidos) | `AchievementsDataSpec.cpp`, y tres piezas F2 en `building_pieces.json` que el menú aún no filtra por fase. |
| #82 | `claude/harvesting-h0-complete-lqtnfp` | `necesita-unreal` (arreglo subido) | Toca `ExploredWiringSubsystem` y `VegetationHarvestState.h`. |
| #83 | `claude/stylized-terrain-textures-xvyy77` | Fusionada en otra ejecución (`8410a6d`) | Aquí salía verde: Textures 374 passed. |
| #84 | `claude/h0-survival-conditions-2z1wec` | `necesita-unreal` | Componentes, HUD, `Game.archive`. En la nube todo en verde: 878/0 y DataCheck 201 passed. |
| #85 | `claude/climb-model-h1-t20vxc` | `necesita-unreal` | Contiene #72 entera y toca los binarios de `Content/Localization/Game`. Además reescribe recetas de la biblia sin aprobación. |
| #86 | `claude/h1-inventory-completion-y9tnyv` | `necesita-unreal` | `CarryComponent`, `SExploredInventoryPanel`, `Game.archive`. En la nube todo en verde: 869/0 y DataCheck 204 passed. |
| #87 | `claude/fix-beach-terrain-profile-tvvrs3` | `necesita-unreal` | El código es puro y está en verde (838/0), pero cambia el terreno horneado. El encargo `23-perfil-playas.md` pide rehornear el mapa y el HLOD y mirar las playas en local. |
| #88 | `claude/terrain-edits-pipeline-alwur4` | **Fusionada** (`c807d8d`), con arreglo al integrar | `FTerrainEdits` puro, con su spec, datos y docs. Git la fusionaba con #76 sin conflicto, pero salía `IsFiniteVector` definida dos veces y no compilaba. |
| #89 | `claude/h0-network-codec-queue-o6u5gq` | **Fusionada** (`b6a9503`) | Modelos puros de red: códec de deltas, cola, checksum y `NetBudget`. Cumplen la biblia 08 §2.2. |
| #90 | `claude/boat-pieces-pure-models-1vfxhd` | Cambios pedidos (sigue `necesita-unreal`) | Duplica lo que #53 dejó en main (ver abajo). |
| #91 | `claude/add-h2-game-data-l4x8pd` | `necesita-unreal` (arreglo subido) | `AchievementsDataSpec.cpp` y el orden de `templates.json`, del que depende `Explored.Crafting` en el editor. |
| #92 | `claude/phase-3-pure-models-65r78m` | Pendiente del director (arreglo subido) | Los modelos están en verde (895/0), pero la PR añade tres `[Decisión]` a la biblia 05 sin constancia de aprobación. |

### Comprobaciones

| PR | HostTests | HostTests + ASan/UBSan | DataCheck `--strict` + pytest | Otros |
|---|---|---|---|---|
| #72 | 825 / 0 | 825 / 0 | — | — |
| #73 + #74 sobre main (`77e3f53`) | — | — | 0 errores · 212 passed | Packs: 7 passed · `l10n export --check`: 0 errores |
| #75 | 825 / 0 | no se ejecutó (sin memoria) | 0 errores · 183 passed | Packs: 11 passed · Textures: 296 passed |
| #78 sobre main (`d184e03`) | 878 / 0 | 878 / 0 | 0 errores · 227 passed | — |
| #79 | 819 / 0 | 819 / 0 | 0 errores · 251 passed | l10n: 31 passed · `export --check` ok |
| #80 | 825 / 0 | 825 / 0 | 0 errores · 190 passed | l10n: 56 passed · `--strict` sin avisos |
| #81 | 851 / 0 | 851 / 0 | 0 errores · 196 passed | l10n: 27 passed |
| #82 | 869 / 0 | 869 / 0 | — | — |
| #84 | 878 / 0 | 878 / 0 | 0 errores · 201 passed | l10n: 27 passed |
| #84 + #86 juntas | 928 / 0 | 928 / 0 | 0 errores · 222 passed | — |
| #85 | 870 / 0 | no se ejecutó (sin memoria) | 0 errores · 183 passed | l10n: 27 passed · `export --check` ok |
| #86 | 869 / 0 | 869 / 0 | 0 errores · 204 passed | l10n: 27 passed |
| #87 | 838 / 0 | 838 / 0 | — | — |
| #88 sobre main (`b6a9503`) | 1054 / 0 | 1054 / 0 | 0 errores · 231 passed | — |
| #89 sobre main (`7d26939`) | 1031 / 0 | 1031 / 0 | — | — |
| #90 | 859 / 0 | 859 / 0 | 0 errores · 200 passed | — |
| #91 | 819 / 0 | 819 / 0 | 0 errores · 296 passed | l10n: 27 passed · `export --check` ok |
| #92 | 895 / 0 | 895 / 0 | 0 errores · 189 passed | — |

DataCheck `pytest` tardó 15 minutos con la máquina libre y entre 1,5 y 2 horas con la
máquina saturada.

### Arreglos subidos

- **#77 `5466c99`:** la casilla de riego de `FFarmModel` vuelve a «en parte» y se corrige
  el recuento.
- **#78 `017f61d`, `597a19e`:** merges de `main` en la rama.
  - Conflicto en `00-TODO.md` con #77: las cinco casillas de combate y la de estadísticas
    de fauna quedan «en parte», porque `FCombatModel` es solo un modelo puro, sin
    componente ni RPC. Es el mismo criterio que #77 aplica a #40, #46, #57 y #58.
  - Conflictos en `checks.py` (un import) y `test_datacheck.py`: se conservan los dos
    lados.
- **#89 `9f4104f`, `04bd858`, `dc9f70a`:** merges de `main` en la rama. Las casillas del
  paquete de deltas, la cola de salida y la comprobación por chunk quedan «en parte», por
  la misma razón.
- **#88 `be9cace`:** merge de `main` en la rama.
  - Se quita la copia de `TerrainEditDetail::IsFiniteVector` que también trae #76.
  - `ForEachSampleInBox` se queda con la versión de #97, que además acota el número de
    muestras.
  - `00-TODO.md` adopta el formato de #77.
- **#79 `7c84878`, `f7382da`, `17252a2`:**
  - inglés británico;
  - `phase`, `rarity` y `coopScope` en dos logros (biblia 07 §2.3 y 08 §5.7);
  - «display case» según el glosario.
  - `catalogo.json` se regeneró con `l10n export`.
- **#81 `b7ac254`, `b8d5019`:**
  - coste de `gallinero`, `pocilga` y `corral` según la biblia 03 §3.7;
  - DataCheck pide «al menos 30» logros del acceso anticipado en vez de «exactamente
    30», que chocaba con #90.
- **#82 `a2abf2b`:** `FTreeFallModel::Resolve` dejaba que un árbol atravesara una pared o
  una loma más alta que el arco del tronco.
  - Dos casos del spec daban el fallo por bueno. Se han corregido y se ha añadido uno
    nuevo.
  - Con el código anterior, fallan 2 casos.
- **#91 `a073348`:** faltaba el `nameEn` del nivel de fundición en `fuels.json`.
- **#92 `6289ddd`:** el alcance del machete del Saqueador y del Capitán baja de 1,5 m a
  1,2 m, como dice la biblia 05 §3.1 (`ReachShortM` de #78). Se añade la comprobación al
  spec.

**No subidos (push denegado al subagente):** hay dos arreglos listos en local.
- #85: `DrivePiton` devuelve `EClimbReject::PitonAlreadyThere` cuando ya hay una clavija
  (biblia 02 §13.4), y «sledge» pasa a «travois».
- #75: el modelo descarta columnas NaN o infinitas del fondo marino, con su test.

Quedan anotados en las revisiones de cada PR para que los aplique su autor.

### Fallos serios y decisiones pendientes del director

- **#72 (playtest):**
  - el bot apunta con `Camera->SetWorldRotation`, pero la cámara usa
    `bUsePawnControlRotation`, así que no mira al punto;
  - con `-ExploredShots=playtest`, el bot y el `ShotSubsystem` se disputan el pawn y
    `FScreenshotRequest`;
  - `WriteReport` no espera al bot;
  - `AuditGroundClearance` recorre el océano, el cielo y las manos, y dará falsos
    positivos.
- **#75 (fondo marino):**
  - donde se solapan los halos de dos islas, las celdas se generan dos veces y salen
    instancias duplicadas;
  - el presupuesto de 2000 instancias por celda se lo come el coral, y no salen praderas,
    kelp ni roca;
  - los assets son CC-BY, cuando el GDD §7.1 solo recoge CC0, así que hace falta
    aprobación y crédito;
  - repite el número de lote 6 con #73.
- **#80:**
  - retoca 7 textos de los 30 logros originales, aunque la biblia 07 §2.2 dice que no se
    tocan;
  - pone rarezas a esos 30 logros;
  - reescribe §3.5 y §4.2.
  - Es el superconjunto de logros; se recomienda fusionarla primero y adaptar a ella #79,
    #81, #90 y #91.
- **#81:** reescribe dos textos de logros de granja de la biblia 07 §2.3.
- **#82:**
  - la nota nueva de la biblia 02 §1.2 decide por su cuenta que los golpes y el botín
    siguen el GDD §3.12;
  - en las partidas antiguas, los árboles talados «para siempre» rebrotarán.
- **#85:** cambia la receta de `pie_de_palmera` (ya no es «cuerda ×1»), da dos roles a la
  clavija y pone números nuevos en la biblia 02 §13.6.
- **#90 (barcos por piezas):**
  - duplica lo que #53 dejó en main: dos catálogos (`EHullPieceType` y
    `EBoatPieceType`), dos formas de sacar la ficha y dos sistemas de uniones con ritmos
    de daño distintos. Hay que unificarlos o decir cuál manda;
  - repite dos objeciones de #53: reparar suma puntos, cuando la biblia 02 §8.3 pide
    reemplazar la pieza; y las masas no cuadran con la biblia 03 (el tablón pesa 5 kg y
    sale de 0,3 kg de `madera_blanda`);
  - la red no dice cómo llegan al cliente la lista de piezas de un casco libre ni el
    amarre;
  - marca como hecha `barco_limon` aunque no consume `canoa_balancin`.
- **#91:** añade «Decisiones 2026-09-28» a la biblia 03 y 07. Cambia la carretilla y el
  banco de chatarra, y renombra «Cangrejo ladrón» y «Running Out of Air».
- **#92:** hay que confirmar:
  - que la batata violeta, la cerámica decorada y la sal refinada pasan de Neutral a
    Cauta (biblia 05 §1.4);
  - que la canoa de Cala Rota flota en vez de estar varada (§2.5);
  - que «a veces 2 Saqueadores» es un 50 % (§2.3).
- **#89 (ya en main):** `FTerrainEditModel` admite `MaxDeltaMm` de ±1000 m, pero el
  `int16` del cable solo lleva ±32,767 m. Antes de cablear la red de H2 hay que decidir si
  se acota el delta o se cambia el formato. `FTerrainChunkChecksumModel` duplica el FNV-1a
  de `FTerrainEditModel::ChunkChecksum`, con la misma disposición de bytes.
- **#74 (ya en main), para la sesión local:** en `FarmSpec.cpp` (`DataPlants()`), la
  piña pasa de `1, 1, 0` a `1, 1, 8`. Siguen abiertas tres decisiones: los días sin riego
  (biblia contra modelo), el radio del espantapájaros (4 m contra 15 m) y la carne del
  cerdo salvaje.

### Duplicados y choques

- #85 contiene #72 entera: #72 va primero.
- Varias PR repiten los mismos logros con textos distintos: #80 comparte 8 con #91 y 2 con
  #79, y además #81 y #90 repiten algunos.
- El tope de logros es incompatible entre PR: 54 frente a 60. La biblia 07 da de 40 a 60.
- Hay tres comprobadores anti-IA: `datacheck/estilo.py` (#79), `datacheck/textos.py`
  (#91) y `l10n/estilo.py` (#80).
- #91 trae una `viga_apoyo` idéntica a la de #74, que ya está en main: hay que quitar la
  copia de #91.
- #84 y #86 fusionan juntas sin conflicto de código. `catalogo.json` y
  `localizacion-informe.md` hay que regenerarlos con `l10n export` después de la segunda.
- #87 y #88 chocan en texto en `TerrainDensity.h`: hay que quedarse con las dos partes.
  Probadas juntas, están en verde.

### 00-TODO.md

Las casillas se ajustaron en los merges de `main` en #78, #89 y #88, con el criterio de
#77 (el modelo puro sin cablear cuenta como «en parte»). No hace falta una PR aparte.

- **Pasa a `[x]`:** «`FTerrainDensity::Density` consulta primero la capa de ediciones»
  (#88: `SetEdits` y `DensityWithColumn`, con spec).
- **Siguen `[x]`, con la nota de #88 añadida:** `FTerrainEdits`, la capa `"terrain"` del
  guardado y `tierra_suelta`.
- **Quedan «en parte»:**
  - las cinco casillas de combate y la de estadísticas de fauna (#78);
  - el paquete de deltas, la cola de salida y la comprobación por chunk (#89);
  - el picado por esfera y el remallado (#88).
- #73 y #74 no cierran ninguna casilla.
- Recuento: 16 hechas, 44 en parte y 148 sin empezar, de 208. Un 8 % hecho y un 18 %
  ponderado.

### Para la próxima ejecución

- #93–#98 siguen abiertas y no se han revisado aquí.
- #70: termina de integrarla la ejecución que abrió #99.
- La máquina no aguanta 7 subagentes a la vez compilando HostTests y ejecutando DataCheck:
  conviene como mucho 2 o 3.

## Ejecución 15:00 UTC

Base: `origin/main` en `4e6a7b3` al empezar y en `b6a9503` al terminar. Había 22 PR abiertas.
Mientras corría esta pasada, la sesión local del director también estaba fusionando: fusionó
#94 (squash, `7d26939`) y #89 (`b6a9503`). En cuanto se vio, esta pasada dejó de fusionar para
no pisarse con ella. Las PR con etiqueta `necesita-unreal` y las que tienen cambios pedidos
sin commits nuevos desde la pasada de las 13:00 mantienen la decisión y no se han vuelto a
comprobar.

| PR | Rama | Decisión | Por qué |
|---|---|---|---|
| #70 | `nocturno/revision-2026-09-28` | Sin cambios: `necesita-unreal` | La rama nocturna sigue recibiendo commits (14:57). Choca con `main`. |
| #71 | `nube/mundo-2026-09-28-cocos` | **Fusionada** (`b7037a9`) | Es el modelo puro `FCoconutPalmModel` con su spec, `pure_*.txt` y docs. `ef8b2d6` y `eda1d95` resuelven lo pedido a las 13:00: la tala no suelta `coco_verde` y deja entre 1 y 3 maduros, y las rachas son deterministas. |
| #72, #75, #79, #80, #81, #82, #84, #85, #86, #87, #90, #91, #96, #101 | — | Sin cambios: `necesita-unreal` | No hay commits nuevos desde la etiqueta. Todas salvo #72 chocan ahora con `main`, así que habrá que fusionarles `main` en la sesión local. |
| #88 | `claude/terrain-edits-pipeline-alwur4` | Sin cambios: cambios pedidos | No hay commits nuevos. Sigue duplicando la tabla de herramientas y la vía de picado de #95, con otros radios y otro botín. La revisión de las 14:30 propone fusionarla quitando el `IsFiniteVector` repetido, pero no resuelve el duplicado. Eso lo decide el director. |
| #89 | `claude/h0-network-codec-queue-o6u5gq` | Fusionada por el director (`b6a9503`) | Esta pasada la fusionó con `main` y la comprobó: 1031 casos, 0 fallos, también con ASan/UBSan y `-ffast-math`. El push llegó tarde porque el director ya había subido su propio merge de `main` y la había fusionado. |
| #92 | `claude/phase-3-pure-models-65r78m` | Sin cambios: cambios pedidos | `6289ddd` solo arregla el alcance del machete (1,2 m). Siguen abiertos el alcance F3, el NaN con matemáticas rápidas, las casillas mal marcadas y los bytes de red. |
| #93 | `claude/network-pieces-h1-h3-6jsk6r` | Sin cambios: cambios pedidos | No hay commits nuevos. Sigue el choque de ODR con #82 (`FVegetationNetKey`/`FVegetationNetState`). |
| #94 | `audio/soundfont-acustico` | Fusionada por el director (`7d26939`) | Entró con el bloqueo de las 13:00 sin resolver (ver abajo). |
| #95 | `claude/mining-terrain-h2-models-fnc9sw` | Sin cambios: cambios pedidos | No hay commits nuevos. Siguen el duplicado con #88, el guardado de vetas y `CompactStrip` sin topes. |
| #98 | `claude/improve-python-tools-quality-duaxgr` | Sin cambios: espera a ser la última de `Tools/` | No hay commits nuevos. Choca ya con `compose.py`, `sequencer.py` y `build.py` de #94. |

### Comprobaciones

| Qué | HostTests | HostTests + ASan/UBSan | Otros |
|---|---|---|---|
| #71 fusionada con `main` (`4e6a7b3`) | 965 / 0 fallos | 965 / 0 fallos | — |
| #89 fusionada con `main` (`b7037a9`) | 1031 / 0 fallos | 1031 / 0 fallos | `HOST_TESTS_FASTMATH=ON`: 1031 / 0 |
| `main` en `b6a9503` | 1031 / 0 fallos | — | Audio: sin `fluidsynth`, **error**; con `apt install fluidsynth`, 170 passed |

### `Tools/Audio` falla en la nube desde #94

- `tests/conftest.py`: la fixture de sesión `rendered` renderiza todo el catálogo, y eso
  incluye la música y la muestra de flauta, que ahora salen de FluidSynth.
- Sin `fluidsynth` en el PATH, `soundfont.ensure_fluidsynth()` lanza `RuntimeError`.
  Entonces caen también los tests de efectos y ambientes, no solo los de música.
- Arreglos posibles, para el director:
  1. instalar `fluidsynth` en el script de configuración del entorno de la nube;
  2. o que los tests que dependen de FluidSynth se salten con `pytest.skip` cuando falte,
     sin arrastrar a los demás.
- La casilla de 00-TODO «pipeline de música **y efectos** con soundfont acústico» quedó en
  `[x]`, pero su propia nota dice que los efectos y ambientes siguen siendo síntesis.
  No se toca aquí porque la marcó el director al fusionar. Si los efectos entran en esa
  casilla, habría que volver a dejarla en «en parte».

### 00-TODO.md

- #71: se añade «en parte» a la casilla del `coco_verde` de la copa (`PickFromCrown`). Es un
  modelo puro y todavía no está enganchado a la escalada.
- #89: el director ya lo dejó bien al fusionar. El códec está en `[x]` porque la casilla
  pide el spec de host. La cola, la comprobación de chunk y NetBudget no se marcan.
- La tabla de recuento ya iba una casilla por detrás en «hechas» y en «en parte» antes de
  esta pasada, así que no se ha tocado.

## Ejecución 13:00 UTC

Base: `origin/main` en `8410a6d` al empezar y en `7891b2f` (más esta PR) al acabar. Había 28 PR abiertas (#70–#98, sin #83) y durante la pasada llegaron #100 y #101. Las revisiones se hicieron en paralelo con subagentes de solo lectura, y las comprobaciones, sobre cada rama con `main` fusionado.

**Aviso: esta ejecución se solapó con otras tres fuentes de fusiones.** La larga de las 11:00 fusionó #73, #74, #78 y #88. La de las 15:00 fusionó #71. La sesión local del director fusionó #94 y #89. En cada caso, antes de fusionar volví a leer la cabeza de la rama y `main` (`expectedHeadSha`). #73, #74 y #78 ya las había probado y daba el mismo veredicto. En #78 me quedo con la resolución de `00-TODO.md` de la otra ejecución: todo el combate como «en parte». Con una pasada de más de 3 h en una máquina de 4 núcleos, las ejecuciones cada 2 h se pisan. Conviene espaciarlas o usar un cerrojo: una etiqueta o una rama `nube/integrando`.

| PR | Rama | Decisión | Por qué |
|---|---|---|---|
| #70 | `nocturno/revision-2026-09-28` | **Fusionada en parte** (#99, `a83d987`) + `necesita-unreal` | Su parte hasta `160e6fe` está verde y sin fallos, y entró por #99 apuntando a ese mismo commit, sin reescribir nada. Incluye: `Advance` del incendio en uint64, cotas de `Load`, arena con 4 revisiones como máximo, agua de lluvia ajena que no se vuelve potable, amarre al cargar, flotación del astillero y chapoteos. Después, el autor subió `eb71200`, que cambia el HLSL de `M_Terrain` (un material, así que se verifica en Unreal); eso y el paso en arena siguen en #70. |
| #71 | `nube/mundo-2026-09-28-cocos` | Cambios pedidos → fusionada a las 15:00 (`b7037a9`) | Al talar soltaba `coco_verde` y repartía de 0 a 6 cocos, contra la biblia 02 §1.2 y §13.1. El autor lo corrigió en `ef8b2d6` y `eda1d95`. |
| #72 | `debug/playtest-automatico` | `necesita-unreal` + cambios | Hay dos cosas que moverían el pawn y harían capturas a la vez: el bot y `ExploredShotSubsystem`. Además, el bot apunta con `SetWorldRotation`, que pisa la rotación de control. |
| #73 | `nube/packs-2026-09-28` | Fusionada (a las 11:00, `4a5b1ad`) | La comprobé con `main`, con los lotes 6 y 7: DataCheck 0 errores, pytest 189 passed, Packs 7 passed. |
| #74 | `nocturno/datos-2026-09-28b` | Fusionada (a las 11:00, `d184e03`) | DataCheck 0 errores, pytest 206 passed, Localization 27 passed. Los peligros coinciden con la biblia 02 §2.4. |
| #75 | `arte/fondo-marino` | `necesita-unreal` + cambios | Toca el commandlet. Problemas: el tope de 2000 por celda se reparte mal por especie, hay celdas sembradas dos veces y «lote6» choca con #73. |
| #76 | `nube/mecanicas-2026-09-28-entradas` | **Fusionada** (`a6108d3`) | Validación de entradas del terreno editable: `InWorld`, `ValidExtent`, topes de escalera y `bRejected`. Modelo puro con spec. |
| #77 | `claude/update-todo-checklist-g6y9m0` | **Fusionada** (`77e3f53`) | Auditoría de `00-TODO.md`. Se resolvió el choque con #83 en H4, se rehizo el recuento y se cambiaron por «anterior a #41» los hashes `22ec751` y `af649e9`, que eran la raíz del clon superficial. |
| #78 | `claude/implement-combat-model-h1-qweffn` | Fusionada (a las 11:00, `b030518`) | La comprobé: 878/0 en normal y con ASan, DataCheck 0 errores y 198 passed. La replicación está definida según la biblia 08. |
| #79 | `claude/h4-text-content-iytug5` | `necesita-unreal` | `AchievementsDataSpec` depende de un subsistema. Los textos cumplen la guía anti-IA. Va después de #80. |
| #80 | `claude/complete-h5-achievements-localization-be97tx` | `necesita-unreal` | Toca `AchievementsSubsystem`. Es el catálogo canónico de logros y tiene que entrar antes que #79, #81, #90 y #91. Quedan decisiones abiertas: fase publicada, «mil» cocos frente a 500 y la rareza de `buscador_de_vetas`. |
| #81 | `claude/livestock-pens-phase-2-be63wr` | `necesita-unreal` + cambios | Las piezas F2 (gallinero, pocilga y corral) acabarían en el menú del acceso anticipado, porque nadie lee `phase`. Además es trabajo de F2. |
| #82 | `claude/harvesting-h0-complete-lqtnfp` | `necesita-unreal` + cambios | Toca `ExploredWiringSubsystem`. `FVegetationNetKey`/`FVegetationNetState` están duplicados con #93 y con semántica opuesta. |
| #84 | `claude/h0-survival-conditions-2z1wec` | `necesita-unreal` | Toca componentes y el HUD. Nadie llama a `ApplyContactBurn`, y la cesta, la estantería y el arcón no son contenedores. |
| #85 | `claude/climb-model-h1-t20vxc` | `necesita-unreal` | Está apilada sobre #72. Lo suyo es puro: si se rebasa sin #72, puede entrar por la nube. |
| #86 | `claude/h1-inventory-completion-y9tnyv` | `necesita-unreal` | Toca `CarryComponent` y el panel de inventario. Sin fallos serios. Choca con #84. |
| #87 | `claude/fix-beach-terrain-profile-tvvrs3` | `necesita-unreal` | El código es puro y correcto, pero el terreno está horneado: hay que fusionarla y rehornear en la misma sesión local. |
| #88 | `claude/terrain-edits-pipeline-alwur4` | Cambios pedidos → fusionada a las 11:00 (`c807d8d`) | Aquí pedí cambios por el cuelgue de la pala con `Center = (1e10, 0, 0.3)`. En `main` ya no pasa: `Shovel` rechaza con `InWorld` (#76) y `ForEachSampleInBox` corta las cajas desmesuradas (#97). Sigue abierto el duplicado de la tabla de herramientas con #95 (0,35 frente a 0,50 m), que decide el director. |
| #89 | `claude/h0-network-codec-queue-o6u5gq` | Fusionada (por el director, `b6a9503`) + arreglo en la PR de esta integración | Códec de deltas, cola, checksum y NetBudget, todo puro; el formato coincide con la biblia 08 §2.2. Entró sin mi arreglo (`IsNaN` → `!IsFinite` en la distancia de la cola), así que va aparte en la PR de este informe. |
| #90 | `claude/boat-pieces-pure-models-1vfxhd` | `necesita-unreal` + cambios | Duplica `FHullAssemblyModel`/`FRaftYardModel`, que ya están en `main`. Tiene que decidirlo el director. |
| #91 | `claude/add-h2-game-data-l4x8pd` | `necesita-unreal` + cambios | «Sal con vida» contradice el aire no letal. Batir chapa hace inútil fundir chatarra. `viga_apoyo`, `tablon_contencion` y `clavija_roca` están repetidas en otras PR. |
| #92 | `claude/phase-3-pure-models-65r78m` | Cambios pedidos | F3 queda fuera del acceso anticipado (GDD v2 §6.2). Hay NaN con matemáticas rápidas en `BarterModel.cpp:62` y en `RaiderCampsModel`. El machete tiene 1,5 m y la biblia dice 1,2. Marca `[x]` sin estar hecho. |
| #93 | `claude/network-pieces-h1-h3-6jsk6r` | Cambios pedidos | Los tipos de vegetación chocan con #82. `EntryBytes = 13`, pero son 12 (lo demuestra #86). Repite constantes de NetBudget de #89. |
| #94 | `audio/soundfont-acustico` | Cambios pedidos → fusionada por el director (`7d26939`) | Entró con el bloqueo sin resolver: sin `fluidsynth` cae la suite de Audio (ver abajo). Las licencias están bien (MIT). |
| #95 | `claude/mining-terrain-h2-models-fnc9sw` | Cambios pedidos | Duplica la tabla de herramientas de #88. El guardado de vetas no es idempotente (`ExhaustedDay`). `CompactStrip` no tiene topes. |
| #96 | `claude/early-access-crafting-recipes-vwbw8s` | `necesita-unreal` + cambios | `wear` promete efectos que no existen. Filtrar y destilar usan la técnica «secar». Hay un superlativo vacío en `templates.json:116`. |
| #97 | `claude/audit-pure-models-quality-l5fwix` | **Fusionada** (`4e6a7b3`) | Auditoría de NaN, cuelgues y desbordes, y `PropertyFuzzSpec`. Resolví los conflictos con #70 y #76 (detalle abajo). |
| #98 | `claude/improve-python-tools-quality-duaxgr` | Esperando (la última) | Está bien: salida de audio idéntica al bit y ninguna regla relajada. Pero su CI pondría en rojo unas 12 PR abiertas, así que tiene que entrar la última y rebasada. Hay que añadir `permissions: contents: read`. |
| #100 | `nube/paleta-2026-09-28` | **Fusionada** (`2c74aea`) | Siete muestras de comida en celdas libres y un test cruzado con el catálogo de packs. Textures: 375 passed. |
| #101 | `claude/open-licensed-soundtrack-2v46dz` | Sin decidir: revisión en curso; mantiene `necesita-unreal` | Llegó durante la pasada. No toca `Source`, `Config` ni `Content`: son `Tools/Audio` y docs. La etiqueta la puso su autor. La revisión de licencias (`music_sources.json`) y la suite de Audio no acabaron a tiempo; queda para la próxima pasada. |

### Comprobaciones

| PR | HostTests | + ASan/UBSan | + matemáticas rápidas | DataCheck `--strict` + pytest | Otros |
|---|---|---|---|---|---|
| #70 (#99) | 821 / 0 | 821 / 0 | — | — | Audio: 109 passed |
| #73 | `nube/packs-2026-09-28` | Fusionada (a las 11:00, `4a5b1ad`) | La comprobé con `main`, con los lotes 6 y 7: DataCheck 0 errores, pytest 189 passed, Packs 7 passed. |
| #74 | `nocturno/datos-2026-09-28b` | Fusionada (a las 11:00, `d184e03`) | DataCheck 0 errores, pytest 206 passed, Localization 27 passed. Los peligros coinciden con la biblia 02 §2.4. |
| #76 | 824 / 0 | 824 / 0 | — | — | |
| #78 | `claude/implement-combat-model-h1-qweffn` | Fusionada (a las 11:00, `b030518`) | La comprobé: 878/0 en normal y con ASan, DataCheck 0 errores y 198 passed. La replicación está definida según la biblia 08. |
| #89 | `claude/h0-network-codec-queue-o6u5gq` | Fusionada (por el director, `b6a9503`) + arreglo en la PR de esta integración | Códec de deltas, cola, checksum y NetBudget, todo puro; el formato coincide con la biblia 08 §2.2. Entró sin mi arreglo (`IsNaN` → `!IsFinite` en la distancia de la cola), así que va aparte en la PR de este informe. |
| #97 | 939 / 0 | 939 / 0 | 939 / 0 | — | Sobre `main` con #78 |
| #100 | — | — | — | — | Textures: 375 passed |

### Arreglos subidos

- **#77 `9e56f9d`:** merge de `main`. Se conservan en H4 la nota de rugosidad y la casilla de texturas de #83, se rehace el recuento y se cambian los hashes de la raíz del clon por «anterior a #41».
- **#97 `cf2e949`:** resolución de los arreglos duplicados con #70 y #76.
  - Amarre: me quedo con el de #70, medido desde la posición ya saneada.
  - Clave de arena: tiene que cumplir las dos cotas.
  - Minutos del incendio: hasta 1e12.
  - `TerrainEditModel`: me quedo con las validaciones de #76 y quito la segunda definición de `IsFiniteVector`, `InReach` y `MaxToolReach`. Añado `IsFinite` explícito en la escalera.
  - `PlaceSoil`: vuelve al orden de #76. Sin ese cambio, un presupuesto infinito no daba `bRejected`, y lo detectó el spec de #76.
- **#89 (en esta PR, `5d066e0`):** `SanitizeDistance` usaba `FMath::IsNaN`, que con matemáticas rápidas puede desaparecer al compilar. Además, −∞ pasaba como 0, es decir, como chunk prioritario. Ahora usa `!FMath::IsFinite`, y el spec añade ±∞: con el código anterior, el caso −∞ sale de la cola y falla.
- **#89:** los merges de `main` (`833143c` y `3750771`) no llegaron a subir, porque el director la fusionó antes con su propio merge.
- **`00-TODO.md` (en esta PR, `4ac213e`):** la casilla de `FExploredTerrainDeltaPacket` estaba `[x]` con una nota «En parte», y la tabla no cuadraba. Queda con su línea «Hecho»: la casilla pide el formato y el spec de host. Rehago el recuento contando las casillas del fichero.
- **Audio:** probé un `conftest.py` que salta solo lo que necesita FluidSynth. No basta: 12 tests más renderizan música directamente. Lo deshice y lo dejo como tarea (ver abajo).

### Duplicados y choques

- **Tabla de herramientas de picado:** #88 y #95 (pala tosca de 0,35 m frente a 0,50 m; botín de 6/m³ frente a 1 por golpe). Tiene que quedar una sola, y lo más limpio es que `FTerrainEdits` lea `FMiningModel::ToolInfo`.
- **Estado de red de la vegetación:** #82 y #93 definen los mismos tipos con semántica opuesta. Propuesta: el dueño es #82.
- **Logros:** #80 es el catálogo canónico. #79, #81, #90 y #91 tienen que quedarse con su versión al rebasar.
- **Piezas de mina:** `viga_apoyo` está en #74 (ya en `main`), #91 y #95, y `tablon_contencion` en #91 y #95. Al rebasar, #91 y #95 tienen que quitarlas.
- **Objetos de escalada:** `clavija_roca` está en #85 y #91; `pie_de_palmera` en #85 y #96.
- **Barcos:** #90 duplica `FHullAssemblyModel`/`FRaftYardModel` de `main`.
- **#85** está apilada sobre #72.
- **«lote6»:** lo usan #73 (ya en `main`) y #75.

### Notas para el director

- **`Tools/Audio` desde #94:** sin `fluidsynth`, fallan 12 tests y dan error 68. Con `apt install fluidsynth` pasan los 170. La forma más sencilla de arreglarlo es instalar `fluidsynth` en el script de configuración del entorno de la nube. La otra es marcar con un `skip` los tests que dependen de FluidSynth: la fixture `rendered`, la muestra de flauta, `test_determinism`, `test_build_and_manifest`, `test_music_*` y `test_new_content`.

- **#70/#99:** el amarre de un guardado con posición NaN ya queda descartado. Con #97, la posición se sanea antes de medir la distancia al poste.
- **#80:** hay que decidir si los 5 logros del GDD §16 que dependen de fases posteriores quedan fuera del acceso anticipado (`las_siete_islas`, `el_mapa_entero`, `limon_zarpa`, `naufrago_de_verdad`, `sin_mapa`).
- **#74** marca como propuesta que la piña vuelva a dar fruto cada 8 días. Además deja anotadas dos contradicciones entre `FFarmModel` y la biblia 02 §10.1, que tiene que decidir el director: la planta muere a los 4 días sin riego y el espantapájaros cubre 15 m frente a los 4 m de la biblia.
- **Choque interno de la biblia:** la 02 §2.5 pone las cavernas bioluminiscentes en Manglar y Meseta, y la 04 §2.3 al fondo de los tubos de lava de Humo. `mining.json` sigue a la 04.
- **Máquina:** con 4 núcleos, el pytest completo de DataCheck tarda unos 20 min y una pasada limpia de HostTests, unos 10. #96 dice que acelera DataCheck 8 veces.

### 00-TODO.md

- **#89:** la casilla «`FExploredTerrainDeltaPacket`…» queda `[x]` con su línea «Hecho». Pide el formato y el spec de host, y `FTerrainDeltaCodecModel` lo cumple entero. La cola, la comprobación de integridad y `Explored.NetBudget` siguen «en parte», como las dejó el director.
- **#78:** se mantiene lo que dejó la ejecución de las 11:00: las casillas de combate y las estadísticas de fauna, «en parte». Es coherente con el criterio de #77 (modelo puro sin componente ni RPC).
- **#76, #97, #99, #73, #74 y #100** no completan ninguna casilla: son endurecimiento, arreglos, datos o arte sin importar.
- El recuento se rehace contando las casillas del fichero: 19 hechas, 44 en parte y 148 sin empezar, de 211.

## Ejecución 17:00 UTC

Base: `origin/main` en `eb79756` al empezar y en `8bf25cb` al acabar, sin contar esta PR. Había 23 PR abiertas (#70–#108). Solo se revisa lo que ha cambiado desde la pasada de las 13:00:

- las PR nuevas #102, #105, #106 y #108;
- #101, que quedó a medias;
- los commits nuevos de #70 y #93.

Las revisiones las hicieron subagentes de solo lectura. Las comprobaciones se pasaron sobre cada rama con `main` fusionado, y se repitieron tras cada fusión cuando la PR tocaba el mismo código.

| PR | Rama | Decisión | Motivo |
|---|---|---|---|
| #105 | `nube/packs-2026-09-28` | **Fusionada** (`713aa65`) | Lote 8 de Quaternius (piedras de suelo y madera flotante). Las licencias son CC0 y ya estaban en `packs.json`. El esquema es el de los lotes 6 y 7, sin duplicados. |
| #101 | `claude/open-licensed-soundtrack-2v46dz` | **Fusionada** con arreglo (`f01b9eb`) | Banda sonora grabada: 11 piezas CC0 y 10 CC BY 4.0, sin NC, ND ni SA. Solo toca `Tools/Audio` y docs, así que le quité `necesita-unreal`. `pin` descargaba sin validar el `id` ni la URL (arreglado). Resolví el choque de `00-TODO.md` con `main`. |
| #108 | `nube/mecanicas-2026-09-28-guardado-casco` | **Fusionada** con arreglo (`06e9e0e`) | El casco se guarda y se recarga (clave `hull` opcional). Faltaba el tope de uniones al cargar (carga cuadrática) y el tope de piezas al construir. |
| #102 | `nube/mundo-2026-09-28-surco` | **Fusionada** con arreglo (`86c0606`) | Surco de la balsa arrastrada. La superficie se tomaba bajo el centro, así que cavaba la roca bajo la popa. `WetSand` se hundía como arena seca y las piezas desmesuradas colgaban el bucle. En la GDD queda como propuesta pendiente del director. |
| #106 | `nocturno/datos-2026-09-28c` | **Fusionada** con arreglo (`8bf25cb`) | Fauna por nivel de detalle, población por isla y guano de Los Dientes. El validador se rompía con datos mal formados (arreglado). Coherente con la biblia 04 §2.4 y la 08 §2.3 y §2.7. |
| #70 | `nocturno/revision-2026-09-28` | Sigue `necesita-unreal` | Commits nuevos: huella mínima de la escalera y `MaxVolume` negativo, ambos puros y correctos, y pasos en arena en Audio. También toca el HLSL de `M_Terrain` en `build_materials.py`, que hay que ver en el editor. Durante la pasada llegaron `a39b2cb` (merge de `main` con la misma resolución de `CarveStairs` que la mía), `08dbb7b` (racha de rápidos en combate) y `ecfb1ae` (l10n). Esos tres no los he probado. |
| #93 | `claude/network-pieces-h1-h3-6jsk6r` | Cambios pedidos (siguen) | `9d64173` arregla `EntryBytes = 12` y los techos de NetBudget. Sigue el choque de ODR con #82 en `FVegetationNetKey`/`FVegetationNetState`, que decide el director. |
| #72, #75, #79–#82, #84–#87, #90–#92, #95, #96, #98 | — | Sin cambios | No tienen commits nuevos desde la pasada de las 13:00. Siguen como allí. |

### Comprobaciones

| PR | HostTests | + ASan/UBSan | DataCheck `--strict` | Otros |
|---|---|---|---|---|
| #105 | — | — | 0 errores | Packs 7 passed; `l10n export --check` limpio |
| #101 | — | — | — | Audio 239 passed, 106 skipped (con FluidSynth) |
| #108 | 1062 / 0 | 1062 / 0 | 0 errores | `l10n --check` limpio |
| #102 | 1084 / 0 | 1084 / 0 | 0 errores | `l10n --check` limpio; sobre `main` con #108 |
| #106 | — | — | 0 errores; pytest 257 passed | Localization 27 passed |
| #70 (hasta `97f3d37` + `main`) | 1084 / 0 | 1084 / 0 | — | Audio 172 passed |

### Arreglos subidos

- **#101:**
  - `0a1c6cd`: `safe_to_fetch` comprueba el `id` y el dominio antes de `ensure_original` en `pin`, que se salta `validate()` porque las piezas nuevas no tienen sha256. Añado un test parametrizado con `../`, `file://` y otro dominio; falla sin el arreglo.
  - `2130a9c`: merge de `main`, con `00-TODO.md` sobre la tabla de seis columnas y H5 recontado (4 hechas, 22 sin empezar, 26 en total).
- **#108 `2433d3a`:**
  - `MaxSavedJoints = 4 × MaxSavedPieces` al cargar.
  - `AddPiece` rechaza la pieza 257 y las que pasan de `MaxSavedExtentCm`, así que lo que se arma se guarda entero.
  - Specs de los dos topes.
- **#102 `d3e19b0`:**
  - La superficie se toma en `S + LX` de cada columna.
  - `WetSand` cuenta como mojada.
  - Las piezas de más de 50 m de lado no se marcan.
  - Tres specs nuevos; los tres fallan sin el arreglo.
  - En la GDD, el surco queda como «[propuesta, pendiente del director]». Se corrige que la marea lo borre por encima de la pleamar, y `QueueChanged` queda como API por hacer.
- **#106 `139a135`:** con un nivel `null`, una entrada de población que no es objeto o `groups: true`, `fauna.py` ahora da error de validación en vez de lanzar una excepción o aceptarlo. Hay un test para cada caso; los tres fallan sin el arreglo.
- **#70:** preparé el merge de `main` (en `CarveStairs`, los `IsFinite` de `main` más la huella mínima `StairGrid` de la rama), pero no lo subí: su rutina había fusionado ya `main` con la misma resolución.
- **Entorno:** instalé `fluidsynth` con apt en esta sesión. Con él, `Tools/Audio` pasa entero.

### Notas para el director

- **#102:** hay que decidir si el surco entra en el alcance y, si entra, cómo se borra por encima de la pleamar. Hoy se queda para siempre en la capa `sand` del guardado.
- **#101:**
  - La autoría de «Romance anónimo» está disputada (la reclamó Narciso Yepes). Conviene revisarla legalmente o cambiar la pieza.
  - Los OGG (unos 60 MB) no se versionan, y los −16 LUFS no se han comprobado en CI porque no hay caché de audio.
- **#105:** la arenisca, el pedernal y la madera flotante salen más grandes que las mallas propias de `kit-construccion.md`. Lo tiene que confirmar dirección de arte.
- **#106:** en PIE, comprobar que las fragatas que pasan a replicarse al lanzarse en picado no se saltan el tope de replicados.
- **#90** frente a **#108:** la clave `hull` del guardado pertenece al casco de `FRaftYardModel`. Si entra `FBoatPiecesModel`, tendrá que tener su propia clave.
- **`Tools/Audio`:** sigue haciendo falta `fluidsynth` en el script de configuración de la nube (ver la pasada de las 15:00).

### 00-TODO.md

- Ninguna PR de esta pasada completa una casilla de la lista original.
- **#101** trae su propia casilla `[x]`: las herramientas de la banda sonora grabada existen de verdad. Añade además dos `[ ]`: importar los OGG en el editor y los créditos en el juego y en Steam.
- **#106** se anota en la nota «en parte» de la fauna terrestre (datos de LOD y población; faltan la especie C++, el LOD aplicado y la navegación).
- Recuento: 20 hechas, 44 en parte y 150 sin empezar, de 214.

## Ejecución 19:00 UTC

Base: `origin/main` en `1385bc7` al empezar y en `29c4ac0` después de fusionar #110. Solo
#110 y #111 son nuevas o tienen commits desde la pasada de las 17:00. Las demás mantienen
la decisión anterior y no se han vuelto a comprobar.

| PR | Rama | Decisión | Por qué |
|---|---|---|---|
| #110 | `nube/packs-2026-09-28` | **Fusionada** (`29c4ac0`) | Lote 9: seis iconos de UI de Kenney (CC0, con sha256 y licencia verificada) y la comprobación `check_icons` en DataCheck, con 9 tests. Solo toca datos JSON, `Tools/` y docs. Las hojas de contacto de los lotes 8 y 9 pasan a LFS, como las de los lotes 1 a 7; comprobé que los dos objetos existen en el servidor LFS. |
| #111 | `arte/vegetacion-unificada` | `necesita-unreal` + cambios pedidos | Toca `WorldGenCommandlet.cpp`, `SExploredCredits.cpp` e `import_meshes.py`. Al juntarla con `main` choca en `packs.json`, y con el conflicto resuelto DataCheck da 5 errores: `sha256` «n/a», fuente `polypizza` desconocida y licencia CC-BY-3.0. Mete 11 modelos CC-BY 3.0 sin su crédito en el juego, y la excepción a la política CC0 del GDD v2 §7.1 no está escrita en el GDD. |
| #70, #72, #75, #79–#82, #84–#87, #90–#93, #95, #96, #98 | — | Sin cambios | No tienen commits nuevos desde la pasada de las 17:00. Siguen como allí. |

### Comprobaciones

| PR | HostTests | HostTests + ASan/UBSan | DataCheck `--strict` + pytest | Otros |
|---|---|---|---|---|
| #110 (ya al día con `main`) | — | — | 0 errores · 266 passed | Packs: 12 passed · Localization: 27 passed · CI de GitHub en verde |
| #111 + `main` (conflicto de `packs.json` resuelto en local) | 1086 / 0 | 1086 / 0 | **5 errores** | — |

### Aviso para las PR que tocan `achievements.json`

Desde #110, DataCheck exige que cada pista `icon` de `achievements.json` tenga icono o
esté en `iconsPending` de `packs_catalogo.json`. #79, #80, #81, #90 y #91 añaden pistas
nuevas: #80 añade `arco`, `bandera`, `cangrejo`, `canoa`, `corral`, `empalizada`,
`escudo`, `herramienta`, `huevo`, `mina`, `muralla`, `obsidiana`, `pala`, `pico`,
`pueblo`, `templo`, `trueque`, `vagon` y `viga`. Al fusionarles `main`, tienen que añadir
esas pistas a `iconsPending` o DataCheck se pondrá en rojo.

### Notas para el director

- **#111:** la excepción CC-BY para el dosel de selva tiene que quedar escrita en el GDD v2
  §7.1. El crédito en ES/EN y la localización tienen que entrar con las mallas, no en una
  PR aparte.

### 00-TODO.md

- Ninguna casilla nueva en `[x]`. #110 se anota en la nota «en parte» de la casilla de
  arte del GDD §7.1: iconos de UI, con 25 pistas de logro aún pendientes.

## Ejecución 21:00 UTC

Base: `origin/main` en `fdb0983` al empezar. Durante la pasada, el director fusionó en local
#79, #80, #115, #92 y #98, y `main` llegó a `d68f84b`. Solo se revisa lo que es nuevo o tiene
commits desde la pasada de las 19:00: #70, #112 y #114. #79 y #92 también tenían commits
nuevos, pero se fusionaron fuera de esta pasada.

| PR | Rama | Decisión | Por qué |
|---|---|---|---|
| #112 | `nocturno/datos-2026-09-28d` | **Fusionada** (`bb04f04`, por el director a la vez que yo) | Añade el aloe y la cúrcuma silvestre como cultivos medicinales del huerto (GDD v2 §3.6, que remite al GDD v3 §8.7: «especias y plantas medicinales»; biblia 01 para el gel de aloe y la pasta de cúrcuma). Añade el objeto `rizoma_curcuma`, las mallas pendientes, la regla de DataCheck «todo cultivo da comida o medicina» con 3 tests, y el informe de balance. Solo toca datos JSON, `Tools/` y docs. No duplica nada de otra PR. Le integré `main` tres veces porque el director fusionaba mientras tanto. `catalogo.json` y `localizacion-informe.md` chocaban y los regeneré con `l10n export`, nunca a mano. |
| #114 | `worldgen/realismo-terreno` | Cambios pedidos; sigue `necesita-unreal` | Los modelos son puros y los HostTests pasan (1269 / 0). Pero hay cuatro problemas serios. (1) La erosión es caótica sobre una base en `float` y ahora se aplica también a las islas del acceso anticipado: en cooperativo, dos máquinas pueden generar bases distintas (biblia 08 §2.6). (2) Cambia la Meseta F2 por torres a plomo y una laguna salada que no están en la biblia 04 §2.7. (3) Contradice el GDD v2 §7.3, que dice que el terreno no guarda rejillas, y cambia el significado de los deltas guardados. (4) Choca con #87 en `TerrainDensity`. Detalle en la revisión de la PR. |
| #70 | `nocturno/revision-2026-09-28` | Sigue `necesita-unreal` | Siete arreglos de robustez nuevos en modelos puros, todos correctos: `AddJoint` con tope, aturdimiento en int64, acumulador de ramas acotado, `Abs` en int64, `Accrue`, `DigSphere` fuera del mundo y `WetUntil`. Sigue tocando el HLSL de `M_Terrain`. Le dejé un comentario con lo que hay que comprobar en PIE. |
| #72, #75, #81, #82, #84–#87, #90, #91, #96, #98, #111 | — | Sin cambios | No tienen commits nuevos desde la pasada de las 19:00. Siguen como allí. |

### Comprobaciones

| PR | HostTests | HostTests + ASan/UBSan | DataCheck `--strict` + pytest | Otros |
|---|---|---|---|---|
| #112 + `main` (`c164885`, luego `d61fb14`) | — | — | 0 errores · 356 passed | Localization: `export --check` al día, 60 passed · CI de GitHub en verde en `6ab6655` (siete trabajos, con ruff y basedpyright de #98) |
| #70 + `main` (`fdb0983`) | 1231 / 0 | 1231 / 0 | — | — |
| #114 + `main` (`fdb0983`) | 1269 / 0 | — | — | Rendimiento en el host: la columna pasa de 1,65 a 2,91 µs y el constructor, de 0,6 a 4,3 s |

### Notas para el director

- **#114 frente a #87:** hay que decidir el orden. Propuesta: primero #114 y después #87
  rebasada, con el perfil de playa aplicado después de la erosión.
- **#114:** si se quieren las torres de El Nido en la Meseta, hay que escribirlo antes en la
  biblia 04 §2.7. La biblia dice «farallones sueltos y cenotes».
- **DataCheck:** `pytest` tarda unos 23 minutos en la nube, lo que obliga a repetirlo cada
  vez que `main` avanza durante una pasada. Pasará lo mismo en cuanto se fusione #96, que
  dice acelerarlo 8 veces.

### 00-TODO.md

- Ninguna casilla nueva en `[x]`. #112 se anota junto a la casilla ya hecha de los cultivos
  de Landing: el aloe y la cúrcuma silvestre completan las «especias y plantas medicinales»
  del huerto a nivel de datos. Faltan las mallas y dónde se encuentra el primer rizoma.

## Ejecución 23:00 UTC

Base: `origin/main` en `bde0f36` al empezar. Solo se revisa lo que tiene commits desde la
pasada de las 21:00: #91, #116, #117 y #118. #114 no tiene commits nuevos desde su revisión.

| PR | Rama | Decisión | Por qué |
|---|---|---|---|
| #91 | `claude/add-h2-game-data-l4x8pd` | **Fusionada** (`725ff1c`) | Ya no toca `Source/`: los logros y `AchievementsDataSpec.cpp` entraron con #80. Solo toca datos JSON, `Tools/` y docs, así que se le quita `necesita-unreal`. Los duplicados `viga_apoyo` y `tablon_contencion` están resueltos. `cuerda_fija` en `terreno` y el cobre detrás del hierro coinciden con la biblia 02 §13.3 y la biblia 03 §2.2. El recuento de 00-TODO.md lo he contado otra vez con un script y cuadra; además corrige la fila F3, que estaba mal. |
| #116 | `nube/packs-2026-09-28b` | **Fusionada** (`b034c9a`) | Lote 10 de packs: mitad de coco, pescado de arrecife y espina (Kenney Food Kit, CC0). Solo toca el catálogo, el README y la hoja de contacto. Le integré `main` después de fusionar #91. |
| #117 | `feat/pico-pala-runtime` | `necesita-unreal` | Toca `Build.cs` (`DeveloperSettings`), un subsistema, componentes, `UDeveloperSettings`, el personaje y el PlayerController. Los modelos puros están bien y la replicación sigue la biblia 08. Aviso: el servidor se fía de la herramienta y de la acción que manda el cliente. Choca con `main` solo en `localizacion-informe.md`, que se resuelve regenerándolo con `l10n export`. |
| #118 | `worldgen/terreno-jugable` | `necesita-unreal` + cambios | Está apilada sobre #114 y hereda sus cuatro problemas serios. Añade un acantilado en Landing que contradice la biblia 04 §2.1 («la isla más llana y segura»). Emerald y Smoke pasan a tener un 23 % y un 30 % de costa acantilada, y eso lo tiene que confirmar el director. Mueve los cayos, así que hay que revisar los POI. |
| #70, #72, #75, #81, #82, #84–#87, #90, #96, #111, #114 | — | Sin cambios | No tienen commits nuevos desde la pasada de las 21:00. |

### Comprobaciones

| PR | HostTests | HostTests + ASan/UBSan | DataCheck `--strict` + pytest | Otros |
|---|---|---|---|---|
| #91 + `main` (`17dd997`) | — | — | 0 errores, 0 avisos · 550 passed | Localization: 103 passed · `export --check` al día |
| #116 + `main` (`e8c49b2`) | — | — | 0 errores, 0 avisos · 550 passed | Packs: 64 passed, 1 skipped |
| #117 + `main` (`bde0f36`, en local) | 1340 / 0 | 1340 / 0 | — | `l10n export --check` al día tras regenerar |
| #118 + `main` (`bde0f36`, en local) | 1375 / 0 | 1375 / 0 | — | — |

### Notas para el director

- **#91:** la biblia 03 se contradice con el aluminio. §2.2 dice que el banco de chatarra
  lo recupera «sin fundir» y §4.3 pone carbón en las dos estaciones. Con la decisión de la
  PR, `fundir_chapa` y `fundir_tubo` no sirven para nada mientras el banco dé lo mismo sin
  carbón.
- **#85:** al integrar `main` tiene que quitar su `clavija_roca`, que ya entró con #91.
- **#118:** hay que decidir si Landing lleva acantilado. Si se quiere, primero hay que
  escribirlo en la biblia 04 §2.1.

### 00-TODO.md

- Ninguna casilla nueva en `[x]` aparte de las que marca #91 con su propia PR: los datos
  de H2 del metal, las piezas de mina y escalada, la carretilla y la regla de DataCheck.
  #116 se anota en la nota «en parte» de la casilla de arte del GDD §7.1.
