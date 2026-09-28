# Contenido de texto de H4: diarios, pistas, museo y ruinas

Datos del acceso anticipado que la biblia ya había escrito en prosa (04 §6–§7, 07 §2–§4) y
que ahora viven en `Content/Data`. Esta entrega es solo de datos y de validación: ningún
sistema de juego los lee todavía (ver «Pendiente en el motor»).

## Ficheros

| Fichero | Qué guarda | Fuente |
|---|---|---|
| `halden_diaries.json` | 5 cuadernos de la expedición Halden de 1974: texto ES/EN, isla, `EPoiType` y `ContentId` del POI | biblia 04 §7.1 |
| `journal_entries.json` | 15 entradas del diario del náufrago con `{Day}`, fase y disparador; lista `events` de sucesos sin estadística | biblia 07 §4 |
| `map_clues.json` | 6 pistas en prosa de tesoros raros o únicos: isla, POI opcional, escondite, quién da la pista | biblia 04 §6 |
| `ruins.json` → `sites` | `island`, `teaches` y `starPathTarget` de las 8 ruinas | biblia 04 §2 |
| `museum_collections.json` | Las 7 colecciones + el subconjunto «tesoros»: fichero, recuento, muebles y recompensa | biblia 07 §3.2–3.4 |
| `shells.json`, `herbarium.json`, `insects.json`, `fossils.json`, `minerals.json` | Piezas nuevas del museo: nombre de vitrina, ficha, islas, hábitat, cómo se consigue, fase | biblia 07 §3.5–3.6 |
| `fish.json` | `nameEn` de las 17 piezas de la pecera | biblia 07 §3.6 |
| `artifacts.json` → `treasureRarities` | `["raro", "unico"]`: qué artefactos son «tesoros», sin catálogo aparte | biblia 07 §3.2 |
| `achievements.json` | Logros `juego_de_anzuelos` y `bajo_el_templo`, estadísticas `artifact_ids_found` y `underground_treasure_found` | biblia 07 §2.1, §2.3 |
| `building_pieces.json` | `pecera_museo`, `bandeja_conchas`, `marco_herbario`, `atril_cuaderno`, `vitrina_minerales`, `panel_fosiles` (categoría `museo`, malla pendiente) | biblia 07 §3.3 |

Islas: siempre el `LexToString(EIslandArchetype)` en minúsculas (`landing`, `emerald`,
`smoke`, `teeth`, `mangrove`, `whitesands`, `mesa`), igual que `mining.json` y `fauna.json`;
`hidden` es la isla oculta. POI: el nombre del `EPoiType`.

### Disparadores del diario

`trigger` es una condición de `achievements.json` (`{stat, op, value}`, `{stat, contains}`,
`{flag}`, `all`/`any`/`not`) o `{event}` con un id de `events`. Los sucesos cubren hitos que aún
no tienen estadística (`run_start`, `ruin_discovered`, `storm_survived`, `terrain_dug`,
`cave_opened`, `palisade_built`, `first_barter`); cada uno dice quién lo informará y, si
existe, a qué estadística pasará. Una entrada no puede depender de un suceso de una fase
posterior a la suya.

## Red (biblia 08)

| Dato | Dónde vive | Replicación |
|---|---|---|
| Texto de todo lo anterior | Datos locales de cada máquina | Ninguna: cada cliente lo lee de su `Content/Data` |
| Diario del náufrago (qué entradas se han escrito, con qué día) | Cliente, perfil del jugador | **Individual, sin red** (08 §2.11); viaja en `"players"/<SteamID64>/journal` del guardado (08 §4.4) |
| Cuaderno Halden leído | Servidor | Estado del mundo en el `GameState`, compartido (08 §2.11) |
| Pista leída | Servidor | Del grupo: se apunta en el mapa compartido (08 §5.3) |
| Tesoro recogido | Servidor | Único en el mundo; lo coge quien llega primero (08 §5.5) |
| Museo y colecciones | Servidor | Uno por mundo, en el `GameState` (08 §2.11, §5.5) |
| Técnica de ruina aprendida | Servidor | La aprende el grupo a menos de 15 m (08 §5.5) |
| `artifact_ids_found`, `underground_treasure_found` | Cada cliente | Estadísticas de logro individuales; alcance cooperativo por definir con el resto (casilla `coopScope` de H4) |

## Decisiones respecto a la biblia

- **Dónde están los cuadernos.** La biblia 04 §2.5 pone el campamento Halden en el Manglar,
  pero `PointsOfInterest.cpp` coloca cuatro campamentos (`camp_halden_1` en Esmeralda, `_2` en
  el Humo, `_3` en Arenas Blancas y `_4` en La Meseta) y la estación de radio en el Manglar.
  Los datos siguen al código: la radio para `halden_01`, las tallas del tubo de lava en el
  campamento del Humo (`halden_02`), el barómetro en Esmeralda (`halden_03`), el café en Arenas
  Blancas (`halden_04`) y la despedida junto al observatorio de mareas de La Meseta
  (`halden_05`). Dos de los cinco caen en islas del acceso anticipado.
- **Diario del náufrago.** Varios ejemplos de biblia 07 §4.2 no pasaban su propio checklist
  §1.5 y se reescribieron: el refugio («No es una casa. Es un techo» es un «no es X, es Y»), el
  primer fuego (tríada «chispa, yesca, humo»), la primera ruina, el limón (daba por hecho que se
  plantó el día 1), la captura legendaria («le llaman El Viejo»: no hay nadie que se lo llame,
  y el disparador vale para las cinco legendarias) y la primera noche en inglés. La lectura del
  oleaje se dispara solo con `swell_reading`, porque el texto habla del oleaje.
- **Ruinas.** Los datos llevan la asignación de biblia 04 §2 (Amaraje→Esmeralda, brújula del
  Humo→Los Dientes, Arenas Blancas→La Meseta, La Meseta→isla oculta; oleaje en Esmeralda, aves
  en el tubo de lava, nubes en Los Dientes, color del agua en el Manglar). `FRuinsLayout` todavía
  baraja las técnicas por semilla y su brújula apunta a la isla oculta; DataCheck lo anota.
- **Pistas.** Seis tesoros raros o únicos: los cuatro de las fichas de isla de biblia 04 §2 más
  `anzuelo_ceremonial` (Esmeralda, para que `juego_de_anzuelos` tenga sentido en el acceso
  anticipado) y `carta_varillas` (Amaraje, el ejemplo de biblia 07 §3.5).
- **Recuentos.** `artifacts.json` tiene 16 artefactos y 12 tesoros, no 15 y 10 como dice biblia
  07 §3.2; manda el fichero.
- **Ids.** `abeja_isleña` pasa a `abeja_islena` (los ids son ASCII).
- **Fichas.** Sin rayas de muletilla (las de los ejemplos de §3.5 pasan a coma o punto) y el
  nombre de vitrina cuenta como primera frase de las dos que admite §1.3.
- **Farol de luciérnagas.** Los insectos nunca se capturan (§3.3), así que la recompensa es un
  farol que las atrae, no que las encierra.
- **Número de logros.** DataCheck y `AchievementsDataSpec` aceptan de 30 a 60 (biblia 07 §2
  planea 54 dentro de 40–60) en vez de exactamente 30.

## Verificación

```bash
cd Tools/DataCheck && uv run datacheck --strict && uv run pytest -q
cd Tools/Localization && uv run l10n --strict && uv run pytest -q
```

DataCheck valida las referencias cruzadas (islas, POI, `ContentId`, disparadores, ruinas,
tesoros, piezas y muebles) y la guía anti-IA de biblia 07 §1 (`estilo.py`). `l10n` cubre ya los
ficheros nuevos, `ruins.json` y `fish.json`; `achievements.json` y `artifacts.json` quedan fuera
porque seis de sus textos antiguos darían avisos de longitud. En main `l10n --strict` ya falla
con 25 avisos de longitud anteriores a esta entrega, y ninguno sale de los textos nuevos.

## Pendiente en el motor

- Leer `halden_diaries.json` en un `HaldenLoreSubsystem` (o en `URuinsSubsystem`) y mostrar el
  cuaderno al interactuar con su POI por `ContentId`.
- Leer `journal_entries.json`, evaluar los disparadores con `FAchievementsModel` y los sucesos
  de `events`, y guardar el diario por jugador.
- Hacer que `FRuinsLayout` use `teaches`/`starPathTarget` de `ruins.json` en la semilla oficial.
- Mostrar las pistas de `map_clues.json` desde su ruina o cuaderno y enterrar el tesoro.
- Informar `artifact_ids_found` al recoger un tesoro y `underground_treasure_found` en el templo
  enterrado de La Meseta.
- Mallas de las seis piezas de museo nuevas (`meshes_pendientes.json`).
