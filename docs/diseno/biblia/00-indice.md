# EXPLORED — Biblia de diseño: índice

Versión 2 · 2026-09-27 · Punto de entrada a `docs/diseno/biblia/`. Escrito tras revisar
las 7 secciones enteras contra `docs/diseno/gdd_v2.md` y resolver las contradicciones
encontradas entre ellas (ver «Contradicciones resueltas» más abajo). La versión 2 añade
la **sección 08, cooperativo y red**, por decisión del director de la tarde del
2026-09-27.

## Precedencia

**El GDD manda en el alcance; la biblia manda en el detalle.** `docs/diseno/gdd_v2.md`
decide qué entra en cada fase (acceso anticipado, F2, F3), qué pilares rigen el juego y
qué se descarta explícitamente (la regla «todo por código», «sin fauna terrestre»). Las
8 secciones de esta biblia desarrollan **cómo** funciona cada sistema una vez el GDD ya
decidió que existe: números exactos, fórmulas, ids de objeto, textos ES/EN, pantallas.
Si una sección de la biblia y el GDD chocan en una cifra o un nombre, gana la biblia
(está escrita después y más cerca del código real); si chocan en si algo entra o no en
una fase, gana el GDD. Donde esta regla no bastaba para resolver una contradicción real
entre dos secciones, se decidió aquí y se dejó la justificación en el propio fichero,
marcada `[Decisión]`.

## Qué es cada sección

| # | Fichero | Qué cubre | Decisiones que mandan |
|---|---|---|---|
| 01 | `01-nucleo-y-estados.md` | Pilares del jugador, bucle por escala de tiempo, inicio de partida y kit, ciclo de día/noche/estaciones/mareas, los 19 estados del cuerpo con sus fórmulas exactas, muerte y reaparición, dificultad y modos, guardado del cuerpo. | El cuerpo es la única interfaz de supervivencia (sin HUD de números); la partida arranca siempre día 4, 07:30; morir no pierde objetos ni resetea limpio (mitad de salud, ánimo −15, heridas curadas). |
| 02 | `02-mecanicas-del-mundo.md` | Recolección y tala, minería y edición de terreno, pala y caminos, escaleras picadas, arena viva, fuego, construcción libre, barcos por piezas, raíles (F2), granja (cultivos AA / animales F2), fauna salvaje e IA, tesoros enterrados, **escalada (nueva, §13)**. | Minar es restar densidad sobre `FTerrainDensity` con una capa de ediciones nueva, no un sistema de vóxel aparte; los raíles quedan fuera del acceso anticipado por riesgo técnico no verificado; escalada añadida el 2026-09-27 reutiliza Energía y caída del cuerpo (01), no un sistema propio. |
| 03 | `03-crafteo-inventario-y-objetos.md` | Inventario completo (manos, mochila, contenedores, transporte, muerte), estaciones de crafteo y sus recetas exactas, tabla maestra de objetos con id/peso/durabilidad/fase. | El objeto `pico` es la pieza que faltaba para que la minería sea jugable — definición única fijada en 02 §2.2 (ver contradicción resuelta); el inventario nunca se vacía al morir salvo permadeath. |
| 04 | `04-mapa-islas-y-exploracion.md` | Geometría del archipiélago, ficha completa de cada una de las 7 islas + la oculta, cartografía (instrumentos, niveles de detalle, niebla de guerra), navegación, exploración recompensada, tesoros, historia ambiental (diarios Halden, petroglifos). | Orden geográfico de la cadena volcánica (independiente del orden de progresión del jugador); sede del pueblo del arrecife en La Meseta, campamento pirata en Los Dientes — ambas decisiones `[LD-doc]` nuevas de esta sección. |
| 05 | `05-navegantes-piratas-y-combate.md` | Por qué hay personajes humanos visibles (pivote de producción, packs CC0 con esqueleto Mixamo), navegantes del arrecife (cultura, trueque, reputación), piratas (tipos, IA, asaltos, botín), combate (reutiliza Salud/heridas de 01, sin barra nueva), defensa de base, fauna peligrosa. | El trueque nunca es un menú de tienda: es un objeto en la mano igual que cualquier otra interacción; los aldeanos son invulnerables por diseño, nunca «matables por accidente». |
| 06 | `06-interfaces.md` | Principios de UI (diegético, umbral 55 %, máximo tres verbos, papel y tinta), guía visual (tipografía, paleta, iconografía), las 16 pantallas del juego con wireframe/textos ES-EN/sonido, accesibilidad completa. | Nunca hay minimapo ni HUD de sistema; el diario/bitácora/museo son un único sistema (Museo + «Rumbos» del mapa), no tres pantallas; el trueque de F3 es una extensión del prompt de contexto, no una pantalla de tienda. |
| 07 | `07-logros-museo-y-textos.md` | Guía de estilo anti-IA (obligatoria para todo texto de cara al jugador de las 7 secciones), 54 logros de Steam, el museo y sus 7 colecciones, el diario del jugador, la localización ES→EN completa (proceso, glosario de 66 términos, formato). | La guía anti-IA de §1 es la norma que rige el resto de la biblia — aplicada ya en esta revisión a las 7 secciones (ver abajo); «artefactos» y «tesoros» son el mismo catálogo, no dos. |
| 08 | `08-cooperativo-y-red.md` | Cooperativo de 2 a 4 jugadores con servidor de escucha por Steam: modelo de autoridad, replicación sistema por sistema, presupuesto de ancho de banda, sesiones y guardado, reglas de diseño del coop, interfaces nuevas con textos ES/EN, matriz de pruebas, riesgos y plan de migración con coste en días de agente. | Servidor autoritativo sin excepciones: el cliente solo predice su movimiento y el efecto audiovisual de las acciones instantáneas; el mundo es del anfitrión y el cuerpo es de cada jugador; el mapa dibujado se comparte; sin migración de anfitrión en acceso anticipado. |

## Cooperativo — decisión nueva del 2026-09-27 (tarde)

Encargo del director después de cerrar las 7 primeras secciones: **el juego tendrá
cooperativo de 2 a 4 jugadores en el acceso anticipado**, con servidor de escucha a
través de Steam (Online Subsystem Steam + Steam Sockets). Se decide antes de escribir la
mayoría de los sistemas que faltan para que cada uno nazca con la autoridad en el
servidor, en vez de reconvertirlos todos al final.

Esto **deroga** la regla «sin multijugador» de `gdd_v2.md` §7.2, que era una regla de
producción del GDD v3 heredada sin revisar. Es la única derogación de esta revisión, y
está anotada en el propio GDD (§6.2 y §7.2). El estado de partida es duro y se dice tal
cual en la sección 08: hoy no hay **ni una línea** de replicación en `Source/`, y el
coste estimado del cooperativo es de **69 días de agente** repartidos de H0 a H5
(08 §9.3), con los cimientos —y solo los cimientos— en H0.

## Contradicciones encontradas y resueltas

- **El objeto `pico` (02 vs 03).** 02 §2.2 y 03 §2.1 definían la misma plantilla nueva
  con requisitos y durabilidad distintos (Cabeza por Punta≥3 en 03 frente a
  Contundente/Rígido≥3 en 02; `baseMaxDurability` 40 frente a 55; «pickaxe» frente a
  «pick» en inglés). `[Decisión]` manda 02 §2.2 porque ata la Cabeza a las propiedades
  reales de `canto_rodado`/`basalto` (Contundente) que ya fija 03 §3.3, en vez de a
  Punta, una propiedad que esos materiales de piedra no tienen. Corregido en 03 §2.1,
  §3.3 y §4.1.
- **Punta de la obsidiana (03 vs 05).** 03 §3.3 fijaba la obsidiana en «Filo 5 / Punta
  2-3», pero 05 §3.1 usa Punta 5 para la lanza de obsidiana de Nivel 3 (con la fórmula
  de daño ya aplicada correctamente: 5×3×0.7≈10, 5×3×1.6=24). `[Decisión]` se amplía el
  rango de 03 a «Punta 3-5» — una lasca de obsidiana bien tallada perfora igual de bien
  que corta — en vez de rebajar el arma de 05 y romper la paridad de daño con el resto
  de armas de Nivel 3.
- **Fases del trueque y de los humanos visibles (05 vs 06).** Revisadas ambas a fondo:
  no hay contradicción real — 05 §0 y 06 §2.11 coinciden en que `Villages` es un módulo
  de fase 3, y GDD §6.2 ya distingue piezas de muralla (fase 2) de la IA de asalto/
  patrulla de piratas (fase 3). Se aclaró igualmente la tabla de GDD §2.3 («Mundo
  vivo»), que listaba «Pueblo con horario» y «Piratas que patrullan» sin fase explícita
  y podía leerse como si estuvieran activos desde el acceso anticipado: ahora llevan
  `[F3]` explícito.
- **Coco de la palmera (02 vs código real).** 02 §1.2 decía que talar una palmera suelta
  «2–4× `coco_verde`/`coco_maduro`»; `HarvestModel.cpp` (`Palm.FellDrops`) solo suelta
  `coco_maduro` ×1-3. Corregido en 02 §1.2 y enlazado con la escalada nueva (§13.1): el
  `coco_verde` ya no cae solo, se coge trepando vivo el tronco — resuelve la
  discrepancia sin perder ninguno de los dos objetos.
- **Estados del cuerpo citados desde otras secciones** (picaduras, quemaduras,
  escorbuto, ahogo): revisados uno a uno contra 01 §6 — todas las cifras que 02/03/05
  citan (profundidad de corte de la raya 0,35; duración de medusa 6 h; fórmula de daño
  cortante/contundente) coinciden con el modelo original. No se encontró ninguna
  descoincidencia real más allá de las dos ya listadas arriba.

## Escalada — mecánica nueva incorporada el 2026-09-27

Encargo del director tras el primer borrador de la biblia: trepar palmeras (a pulso o
con `pie_de_palmera`, da acceso a `coco_verde` en la copa) y escalar roca por salientes
(más de 60° de pendiente, tope de 3 m sin herramienta, clavijas con el pico para seguir
más allá en H2). Documentada entera en `02-mecanicas-del-mundo.md` §13, con sus textos
de feedback ES/EN y su TODO propio. No es un sistema aparte: reutiliza Energía y el
sistema de caída/esguince ya descritos en `01-nucleo-y-estados.md` §6.3/§6.13.

## Guía anti-IA — aplicada, no solo escrita

`07-logros-museo-y-textos.md` §1 fija la guía de estilo obligatoria para todo texto de
cara al jugador (logros, museo, diario, avisos interiores, subtítulos) en las 7
secciones. Al revisar las 7 secciones completas contra esa guía no se encontró ningún
texto jugador-facing que la incumpliera: los avisos interiores de 01 §6, los textos de
feedback de 02, las descripciones de objetos de 03 y los textos de UI de 06 ya evitan
tríadas retóricas, adjetivos vacíos, metáforas de inmersión y exclamaciones editoriales
sin necesidad de reescritura. La única excepción con exclamación («¡Aire! ¡Necesito
aire!», 01 §6.15) es exactamente el caso que la guía permite: peligro real dentro de la
ficción (ahogo).

## Ver también

- `00-TODO.md` — lista maestra de todo lo que falta para el juego completo, agrupada
  por hitos de ejecución (H0–H5, F2, F3), con verificación contra el código real del
  árbol principal. **207 casillas** al cerrar esta revisión (9 hechas, 198 pendientes),
  de las cuales 43 son de red y cooperativo.
- `docs/diseno/gdd_v2.md` — documento rector (alcance, fases, pilares, producción).
- `docs/design/biblia-de-contenido.md` — catálogo de objetos, propiedades y verbos
  (documento anterior, complementario, sin cambios de esta revisión).
