# Baliza Siete: estado de F3 (cierre, F3e)

Fecha: 2026-10-21 · rama `reset/v2` · medido con `Tools/playtest_v3.ps1 -Clock 8` (ruta `competente`, Map_IslaSal en blockout).

## Criterio de salida de F3 (plan.md §4)

| # | Criterio | Medida | Estado |
|---|---|---|---|
| 1 | `PlaytestBot` completa la partida hasta la pantalla de epílogo | 139/139 pasos, 10/10 etapas, epílogo alcanzado, 8/8 páginas | VERDE |
| 1b | Duración estimada a 1× entre 47 y 65 min | 48,5 min (47,5 de partida y 60 s de créditos); 13,7 min reales | VERDE (con el día de 24 min) |
| 1c | Desmayos en la ruta competente | 0 (2 sueños: D1 20:03 y D3 02:03) | VERDE |
| 2 | `PlaytestAuditor`: 0 softlocks (orden de tareas, página 1 sin leer, recursos agotados con caja) | 9/9 escenarios se pueden terminar, incluidos los 24 órdenes de la historia | VERDE |
| 3 | Specs nuevos verdes y `HostTests` verde | 733 casos, 0 fallos; specs nuevos: PlaytestRun, BeaconSoftlock (más casos en Raft, StormClock, BodyEvents, SupplyCrate, Backpack) | VERDE |
| 3b | `HostTests` con sanitizers | No ejecutable en Windows local (`stl_asan.lib` no existe); lo corre CI (`host-tests.yml`) tras el push | PENDIENTE (CI) |
| 4 | Guardar y cargar en las 10 etapas devuelve el mismo estado | En juego: 10/10 (historia, mochila, cuerpo y reloj iguales); en el host: `BeaconSaveSpec` | VERDE |
| 5 | Desmayo y dormir verificados por Spec con caso negativo | `BodyEventsSpec` (con el nuevo caso del ciclón) | VERDE |
| — | Errores en el log durante la partida | 0 errores, 41 avisos | VERDE |
| — | Build del editor, `datacheck --strict`, `l10n export --check` | verdes | VERDE |

Hitos a 1× de la ruta competente: Faro 4,1 min · Balsa 13,0 · Carguero 21,4 · lente 21,7 · Ciclón 26,0 · Preparación 33,0 · Linterna montada 39,8 · Final 47,2.

## Bugs que encontró el bot y su arreglo

1. **Softlock: la lente sin recoger al cerrarse el mar.** La balsa no se podía usar nunca después del D2 18:00. Arreglo: el mar reabre el D3 a las 06:00 (`storm_curve.json` `raftReopen`; el cruce forzoso solo actúa hasta entonces).
2. **Dormir en el ciclón saltaba el acto entero.** A las 20:00 del D2 en el faro se dormía hasta el D3. Arreglo: no se duerme durante el ciclón («Con este temporal no hay quien duerma.»).
3. **La caja de suministros ignoraba el progreso** (pasaba el estado inicial) y se gastaba el primer día. Arreglo: progreso real y recursos que quedan en la isla; solo entrega lo que ya no se puede conseguir.
4. **Softlock: toda la leña quemada.** Sin leña no hay hoguera para el aceite. Arreglo: la caja da 3 leñas si faltan tarros.
5. **Mochila llena sin poder soltar nada** (kit, caña, remo, mecha, cocos vacíos): no cabían los peces del aceite. Arreglo: G con las manos libres suelta la última pila que no sea del kit.
6. **Duración: 36,7 min a 1× con días de 18 min** (primera medida). La duración la fija el reloj (dormir a las 20:00, ciclón de 19:00 a 02:00, encender de noche el D3), no las tareas. Arreglo: día de 24 min (1 h = 60 s).

## Desviaciones del GDD (anotadas en gdd.md §3 y plan.md F3)

- Día de 24 min reales, no 18. El ojo sigue durando 65 s (≈ 1,08 h) y el chubasco dura 2 h de juego (120 s; antes 90 s). El barril sigue dando 2 cargas con el chubasco.
- El mar reabre el D3 a las 06:00 (GDD §5.7 decía que la balsa no se usa después del D2).
- No se duerme durante el ciclón (el GDD no lo prohibía).
- La caja de suministros también da leña (tope 3) y no entrega nada mientras la isla tenga lo necesario.
- G suelta objetos (el GDD §9 no lo listaba en los controles).

## Pendiente

- **Cerrado en F5:** quemar la leña dos veces ya no es softlock: el mar deja leña de deriva en Cala Varada al amanecer si faltan tarros y no hay fuego (`FSupplyCrateModel::DriftwoodAtDawn`).
- Rutas «lenta» y «sin páginas opcionales» del criterio de F6: el guion admite `-BalizaRun=<ruta>`, pero solo existe la competente.
- **Cerrado en F5:** `ExploredPlaytestBotRun.cpp` partido en dos ficheros (611 + 249 líneas).
- El auditor antiguo marca 240 «flotando o enterrado» en las primitivas de la isla facetada: hay que revisarlo cuando entren las mallas de F4.
- `HostTests` con sanitizers en CI tras el push.
- El margen de duración es justo (48,5 min frente a 47): si F6 acorta trayectos o tareas, revisar con el bot.

## Siguiente paso exacto

Empujar `reset/v2` y comprobar `host-tests.yml` (con sanitizers); después, F4/F5. Para repetir la medida: `powershell -File Tools/playtest_v3.ps1 -Clock 8` (≈ 14 min reales; informe en `Saved/Shots/playtest_report.txt`).
