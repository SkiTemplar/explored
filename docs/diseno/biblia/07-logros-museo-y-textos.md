# EXPLORED — Biblia de diseño, sección 7: Logros, museo y textos

Versión 1 · 2026-09-27 · Complementa `docs/diseno/gdd_v2.md` (documento rector) y
`docs/design/biblia-de-contenido.md` (catálogo de objetos y sistema de combinación).
Esta sección define **toda la escritura del juego que no es diálogo** — porque el
juego no tiene diálogo (GDD §7.2): logros de Steam, el museo de la base, el diario
del jugador y el pipeline de localización ES↔EN que los sostiene a todos.

**Leyenda de fases** (misma convención que el GDD, §6): **[AA]** entra en acceso
anticipado, **[F2]** llega en la fase 2 del propio acceso anticipado, **[F3]** llega
en la fase 3 (pueblo, piratas, final). Un elemento sin fase marcada hereda la fase de
la sección que lo contiene.

---

## 1. Guía de estilo de escritura anti-IA

Explored no tiene diálogos ni cinemáticas (GDD §7.2, biblia §8.3): cada palabra que
ve el jugador — un logro, una etiqueta de vitrina, una línea de diario — es la única
oportunidad de sonar como un juego escrito por una persona y no generado. Esta guía
es de cumplimiento obligatorio para **todo** texto nuevo del juego, no solo para los
de esta sección.

### 1.1 Voz y tono

**Quién habla.** No hay un narrador: hablan (a) el propio náufrago, en primera
persona, cuando el texto es un pensamiento o una entrada de diario, y (b) el juego
mismo, en segunda persona informal («tú»), cuando el texto se dirige al jugador
(nombres y descripciones de logro, etiquetas de museo). Nunca una tercera voz
publicitaria que hable *sobre* el juego.

**Registro.** Directo, concreto, con los pies en la arena. El náufrago describe lo
que ve y lo que hace, no lo que siente el marketing sobre ello. Frases cortas,
sujeto-verbo-objeto, sin subordinadas en cascada. Presente para reglas y logros
(«construye», «recupera»), pasado para el diario («encendí», «encontré»).

**Lo que no hace esta voz:** no vende el juego, no explica por qué algo es
divertido, no usa signos de exclamación salvo un peligro real dentro de la ficción
(nunca para entusiasmo editorial), no rompe la cuarta pared salvo en el diario
(que es del personaje, no del estudio).

**En inglés (British English, `docs/tecnico/localizacion.md`):** mismo registro,
mismas frases cortas, ortografía y vocabulario británicos (*colour*, *metre*,
*fibre*, *favour*), nunca calco literal del español — se traduce el sentido y el
ritmo de la frase, no la sintaxis palabra por palabra. Un ejemplo de calco a evitar:
«Light your first fire» (bien, corto y directo) frente a «You have managed to light
your very first fire on the archipelago!» (calco hinchado de un «¡enhorabuena!»
implícito que el español nunca tuvo).

### 1.2 Lista negra — palabras y construcciones

Prohibido en cualquier texto que vea el jugador (logros, museo, diario, UI). Un
texto con cualquiera de estos rasgos se reescribe antes de entrar al juego.

**Español:**

| Categoría | Ejemplos prohibidos |
|---|---|
| Tríadas retóricas | «con determinación, coraje y astucia»; «explora, construye y sobrevive»; cualquier lista de tres adjetivos o verbos que no aporte información nueva en el tercer elemento |
| «No es X, es Y» | «Esto no es solo un refugio, es tu nuevo hogar»; cualquier variante de la falsa disyuntiva |
| Adjetivos vacíos | épico, increíble, asombroso, único en su especie (salvo que sea literalmente irrepetible), majestuoso, impresionante, definitivo, perfecto |
| Metáforas de viaje/inmersión | «sumérgete en», «embárcate en la aventura», «desata tu creatividad», «un viaje de descubrimiento», «adéntrate en» |
| Grandilocuencia | «un mundo que te espera», «los límites de lo posible», «una experiencia que nunca olvidarás», «cambia las reglas del juego» |
| Muletillas de resumen | «en definitiva», «en resumen», «cabe destacar que», «no cabe duda de que», «sin lugar a dudas» |
| Preguntas retóricas de gancho | «¿Alguna vez soñaste con…?», «¿Estás listo para…?» |
| Abuso de la raya como muletilla | más de una raya explicativa por frase, o rayas que sustituyen a una coma normal solo por «sonar mejor» |
| Exclamaciones editoriales | «¡Un descubrimiento fascinante!», cualquier signo de exclamación que no marque peligro real dentro de la ficción |

**Inglés:**

| Categoría | Ejemplos prohibidos |
|---|---|
| Hype verbs | *unleash*, *elevate*, *unlock your potential*, *embark on a journey*, *dive into*, *discover a world of* |
| Vacío corporativo | *seamless*, *game-changing*, *a testament to*, *boundless*, *immersive* usado como relleno |
| Falsa disyuntiva | «It's not just X, it's Y» |
| Muletillas | *it's worth noting that*, *in today's world*, *at the end of the day*, *needless to say* |
| Inclusividad de catálogo vacía | «whether you're a builder, an explorer, or a survivor» — la triple enumeración de tipos de jugador |
| Preguntas retóricas de gancho | «Have you ever wondered…?», «Ready to…?» |

### 1.3 Longitud máxima por tipo de texto

Coherente con la regla de una línea del GDD (§7.2) y con el aviso de longitud de
`Tools/Localization` (el inglés no puede superar 1,3× el español desde 12
caracteres, `localizacion.md`).

| Tipo de texto | Máximo ES | Máximo EN | Notas |
|---|---|---|---|
| Nombre de logro | 4 palabras | 4 palabras | Sin subtítulo ni puntuación final |
| Descripción de logro | 90 caracteres, una frase | 110 caracteres | Una condición, un verbo, sin «y además» |
| Etiqueta de vitrina (nombre de pieza) | 6 palabras | 6 palabras | Igual que `nameEs`/`nameEn` de `artifacts.json` |
| Ficha de vitrina (descripción) | 2 frases, ≤160 caracteres | ≤200 caracteres | Procedencia + un detalle sensorial, nunca lore largo (biblia §9) |
| Entrada de diario | 2–4 frases, ≤280 caracteres | ≤340 caracteres | Un hecho + una reacción breve, nunca dos hechos distintos |
| Idea del personaje (GDD, ya implementado) | ≤8 palabras | ≤8 palabras | Sin cambios; referencia de calibre para todo lo demás |

### 1.4 Reglas de humor

1. El humor sale de la **situación**, nunca del chiste verbal (evita juegos de
   palabras: no sobreviven a la traducción y rompen la regla anti-calco de §1.1).
2. Nunca se ríe del jugador ni le llama torpe en segunda persona; si un logro es
   autoparódico («Manazas», §2), lo dice el propio náufrago sobre sí mismo, no el
   juego sobre el jugador.
3. Máximo un chiste por texto. Un logro gracioso no lleva además una descripción
   ingeniosa: el nombre hace el chiste o lo hace la descripción, nunca los dos.
4. Sin memes, sin anacronismos, sin referencias a internet o a otros juegos: rompen
   la ficción de un náufrago en los años 70 (GDD, campamento Halden de 1974).
5. Sin emoji, nunca (regla dura de todo el proyecto, no solo del humor).
6. El humor es opcional y raro: de 54 logros (§2), solo 3 son explícitamente
   graciosos. Si hiciera falta más, es señal de que el resto de la escritura se ha
   vuelto sosa, no de que falte gracia.

### 1.5 Checklist anti-IA (aplicable a cualquier texto del juego)

- [ ] ¿Tiene una tríada retórica (tres adjetivos, tres verbos, tres ejemplos)? Si sí, quitar el tercero o reescribir.
- [ ] ¿Usa la construcción «no es X, es Y»? Si sí, decir solo Y.
- [ ] ¿Tiene un adjetivo de la lista negra (§1.2)? Si sí, sustituir por un dato concreto o quitarlo.
- [ ] ¿Usa una metáfora de viaje, inmersión o límites? Si sí, describir la acción real en su lugar.
- [ ] ¿Cabe en el máximo de longitud de su tipo (§1.3)? Si no, cortar, no abreviar con «…».
- [ ] ¿Tiene más de un signo de exclamación, o alguno fuera de un peligro real de la ficción? Si sí, quitarlo.
- [ ] ¿La frase suena igual si se lee en voz alta como diría un náufrago cansado, no como un tráiler? Si no, reescribir.
- [ ] ¿El inglés es una traducción del sentido, o un calco palabra por palabra del español? Si es calco, reescribir desde cero en inglés.
- [ ] ¿Pasaría `uv run l10n --strict` sin avisos de longitud ni marcadores desajustados (`Tools/Localization/README.md`)?
- [ ] ¿Se podría borrar la mitad de las palabras sin perder información? Si sí, borrarlas.

### 1.6 Diez ejemplos malos reescritos bien

1. ❌ «¡Sumérgete en un mundo de exploración, supervivencia y construcción sin
   límites!» → ✅ ES: «Un archipiélago sin nombre. Lo que sobrevive de tu equipaje,
   lo que encuentres, y una isla por transformar.» / EN: «An unnamed archipelago.
   What survives from your kit, what you find, and one island to remake.»
2. ❌ «Enciende tu primerísima hoguera y da un paso más en tu increíble viaje de
   supervivencia» → ✅ ES: «Enciende tu primera hoguera en el archipiélago.» / EN:
   «Light your first fire on the archipelago.» *(logro real, `primer_fuego`)*
3. ❌ «Este cuchillo no es solo una herramienta, es la extensión de tu voluntad de
   sobrevivir» → ✅ ES: «Hoja de obsidiana con mango de guayabo, atada con
   tendón.» / EN: «Obsidian blade, guava handle, bound with sinew.»
4. ❌ «¿Alguna vez soñaste con navegar de noche guiándote solo por las estrellas?
   ¡Ahora puedes hacerlo realidad!» → ✅ ES: «Reúne los caminos de estrellas y
   navega de noche sin más guía que el cielo.» / EN: «Gather the star paths and
   sail by night with nothing but the sky.»
5. ❌ «El pueblo del arrecife te dará la bienvenida con los brazos abiertos a su
   fascinante y ancestral cultura» → ✅ ES: «El pueblo del arrecife comercia por
   trueque. Lo que necesitan hoy vale más que lo que sobra.» / EN: «The reef
   village trades by barter. What they need today is worth more than what's spare.»
6. ❌ «¡Prepárate para la batalla definitiva contra los temibles piratas que
   asolarán tu base!» → ✅ ES: «Humo en el horizonte antes del amanecer: los
   piratas vienen a por lo que dejaste fuera.» / EN: «Smoke on the horizon before
   dawn: the raiders are coming for what you left outside.»
7. ❌ «Esta vitrina alberga un tesoro milenario, testimonio imperecedero de una
   civilización perdida en el tiempo» → ✅ ES: «Anzuelo de nácar. Salió del pecio,
   entero, después de todos estos años.» / EN: «Mother-of-pearl hook. It came out
   of the wreck whole, after all this time.»
8. ❌ «Domina el arte culinario de la isla preparando veinte deliciosas y variadas
   recetas» → ✅ ES: «Prueba veinte comidas distintas.» / EN: «Taste twenty
   different foods.» *(logro real, `cocina_de_isla`)*
9. ❌ «Día 7: Hoy ha sido un día increíble lleno de descubrimientos asombrosos
   mientras exploraba esta fascinante isla» → ✅ ES: «Día 7. Encontré la primera
   piedra tallada bajo la maleza. No la hizo el mar.» / EN: «Day 7. Found the
   first carved stone under the undergrowth. The sea didn't shape that.»
10. ❌ «No es simplemente una muralla, es la barrera definitiva entre tú y el
    caos que acecha fuera» → ✅ ES: «Empalizada de estacas. Aguanta un asalto, no
    dos.» / EN: «Stake palisade. Holds off one raid, not two.»

---

## 2. Logros de Steam

54 logros (dentro del rango pedido de 40–60). **Reconciliación con
`Content/Data/achievements.json`:** se conservan los 30 ids existentes sin cambiar
nombre, descripción ni condición — solo se les asigna fase (ninguno estaba
marcado). Dos pasan a **[F2]** (`las_siete_islas`, `el_mapa_entero`: exigen las
siete islas, que según GDD §6.2 no están completas hasta la fase 2) y tres a
**[F3]** (`limon_zarpa`, `naufrago_de_verdad`, `sin_mapa`: exigen la isla oculta,
que GDD §6.2 sitúa en fase 3). El resto queda en **[AA]**. Se añaden 24 logros
nuevos para cubrir minería, minas y vagones, granja, murallas, pueblo, piratas y
humor — mecánicas que `achievements.json` todavía no cubre porque son nuevas en
este GDD (v2).

**Convención de rareza** (estimación de diseño para balancear el peso del icono en
Steam, no un dato medido — se revisa en playtesting): **Común** (se espera que lo
consiga más del 40 % de quienes empiezan a jugar), **Infrecuente** (15–40 %),
**Raro** (4–15 %), **Muy raro** (menos del 4 %).

Formato de condición: mismo esquema que `achievements.json` (`units` del propio
fichero) — `{stat, op, value}` compara un contador/máximo/tamaño de conjunto;
`{stat, contains}` pide un id en un conjunto; `{flag}` pide una marca; `{all}` /
`{any}` / `{not}` combinan condiciones.

### 2.1 Extensión del catálogo de estadísticas

Estadísticas nuevas que hace falta añadir a `achievements.json` (`stats`) para
soportar los logros de §2.3 en adelante. Mismo formato que las 22 ya existentes en
el fichero.

| id | kind | scope | reportedBy | Descripción | Fase |
|---|---|---|---|---|---|
| `terrain_edits_made` | counter | run | Mining | Ediciones de terreno hechas (golpes de pala o pico que restan o suman densidad). | [AA] |
| `strata_mined` | set | run | Mining | Estratos de los que se ha extraído al menos una unidad: `tierra`, `arcilla`, `caliza`, `basalto`, `obsidiana`, `cobre`, `hierro_meteorito`, `azufre`, `cristal`. | [AA] |
| `max_mining_depth_m` | max | run | Mining | Profundidad máxima cavada bajo la superficie, en metros. | [AA] |
| `air_pocket_survived` | flag | run | Mining | Se marca la primera vez que el indicador de aire llega al mínimo en una bolsa cerrada y el jugador sale con vida. | [AA] |
| `cave_collapse_avoided` | flag | run | Mining / Building | Se marca al colocar una viga de apoyo en una galería a punto de colapsar. | [AA] |
| `artifact_ids_found` | set | run | Ruins | Tesoros encontrados por id, estén expuestos o no. `valuesFrom: artifacts`. | [AA] |
| `underground_treasure_found` | flag | run | Ruins / Mining | Se marca al encontrar el primer tesoro en un templo enterrado bajo tierra. | [F2] |
| `tools_broken_on_wrong_material` | counter | profile | Crafting | Herramientas rotas por golpear un material más duro del que soportaban. | [AA] |
| `crab_stole_item` | flag | profile | Fauna | Se marca la primera vez que un cangrejo se lleva un objeto dejado en la arena. | [AA] |
| `rail_track_and_cart_used` | flag | run | Tramway | Se marca al mover un vagón cargado por una vía tendida por el jugador. | [F2] |
| `livestock_species_raised` | set | run | Fauna (granja) | Especies domésticas con al menos una cría nacida en un corral: `gallina`, `cerdo`, `cabra`. | [F2] |
| `eggs_collected` | counter | profile | Fauna (granja) | Huevos recogidos del gallinero. | [F2] |
| `barter_trades_completed` | counter | profile | Villages | Trueques completados con el pueblo del arrecife. | [F3] |
| `reputation_village_tier` | max | run | Villages | Nivel de reputación alcanzado (0 neutral, 1 básica, 2 alta, 3 aliado de facto). | [F3] |
| `wayfinding_taught_by_village` | flag | run | Villages | Se marca al aprender una técnica de wayfinding directamente del pueblo. | [F3] |
| `village_defended_from_raid` | flag | run | Villages / Raiders | Se marca cuando un asalto sobre el pueblo se repele con ayuda del jugador. | [F3] |
| `raids_defended` | counter | profile | Raiders | Asaltos piratas a la propia base repelidos sin pérdidas. | [F3] |
| `raider_camps_defeated` | counter | profile | Raiders | Campamentos pirata derrotados. | [F3] |

Además, `building_pieces_built` (ya existente) necesita que `building_pieces.json`
incorpore las piezas nuevas de muralla (§2.3, «Construcción»): `empalizada`,
`muralla_piedra`, `torre_defensa` — hoy no existen en el fichero (verificado: no
hay ningún id de muralla en `building_pieces.json`, solo mobiliario, estructura y
producción).

### 2.2 Logros existentes (30, ids sin cambios)

Reproducidos aquí solo con su fase, para no duplicar texto que ya vive en
`Content/Data/achievements.json` y que ese fichero sigue gobernando:

**[AA]** `primer_fuego`, `diez_amaneceres`, `un_ano_de_islas`, `rey_del_cocotero`,
`tierra_firme`, `cartografo`, `bajo_el_volcan`, `restos_del_albatros`,
`primer_techo`, `cimientos_de_piedra`, `ojo_de_ciclon`, `el_limonero`,
`huerto_en_flor`, `cocina_de_isla`, `primera_captura`, `una_historia_que_contar`,
`pulmones_de_perla`, `mar_abierto`, `luz_en_el_agua`, `madrugada_de_tortugas`,
`canto_de_ballenas`, `deseos_a_punados`, `melodia_junto_al_fuego`, `coleccionista`,
`wayfinder`.

**[F2]** `las_siete_islas`, `el_mapa_entero`.

**[F3]** `limon_zarpa`, `naufrago_de_verdad`, `sin_mapa`.

`una_historia_que_contar` (una captura legendaria) se queda en [AA] aunque cuatro
de las cinco legendarias viven en islas de fase 2/3 (biblia §4.6): «El Rey de
Plata», en mar abierto, es alcanzable desde el acceso anticipado.

### 2.3 Logros nuevos (24), por categoría

Formato por logro: `id` — nombre ES / nombre EN — fase · rareza · oculto: sí/no.
Luego la descripción ES y EN, y la condición exacta.

#### Minería [AA]

**`primera_palada`** — Primera palada / First Shovelful — [AA] · Común · oculto: no
ES: «Cava tu primer agujero en la tierra o la arena.» EN: «Dig your first hole in
earth or sand.» Condición: `{stat: "terrain_edits_made", op: ">=", value: 1}`.

**`buscador_de_vetas`** — Buscador de vetas / Seam Seeker — [AA] · Infrecuente ·
oculto: no. ES: «Extrae mineral de tres estratos distintos.» EN: «Mine ore from
three different strata.» Condición: `{stat: "strata_mined", op: ">=", value: 3}`.

**`filo_de_obsidiana`** — Filo de obsidiana / Obsidian Edge — [AA] · Raro ·
oculto: no. ES: «Extrae tu primera veta de obsidiana en la Isla del Humo.» EN:
«Mine your first obsidian seam on Smoke Island.» Condición:
`{stat: "strata_mined", contains: "obsidiana"}`.

**`topo_de_isla`** — Topo de isla / Island Mole — [AA] · Raro · oculto: no. ES:
«Cava una galería de veinte metros bajo la superficie.» EN: «Dig a tunnel twenty
metres below the surface.» Condición:
`{stat: "max_mining_depth_m", op: ">=", value: 20}`.

**`el_aire_que_falta`** — El aire que falta / The Air That Ran Out — [AA] · Raro ·
**oculto: sí**. ES: «Sal con vida de una bolsa de aire viciado justo a tiempo.»
EN: «Get out of a foul-air pocket alive, just in time.» Condición:
`{flag: "air_pocket_survived"}`.

**`viga_a_tiempo`** — Viga a tiempo / Beam in Time — [AA] · Infrecuente ·
**oculto: sí**. ES: «Coloca un apoyo en una galería a punto de derrumbarse.» EN:
«Place a support beam in a tunnel about to cave in.» Condición:
`{flag: "cave_collapse_avoided"}`.

#### Minas y vagones [F2]

**`primer_tren_de_isla`** — El primer tren de la isla / The Island's First Train
— [F2] · Infrecuente · oculto: no. ES: «Tiende una vía y mueve por ella un vagón
cargado.» EN: «Lay track and move a loaded cart along it.» Condición:
`{flag: "rail_track_and_cart_used"}`.

#### Construcción — murallas [F2]

**`primera_empalizada`** — Primera empalizada / First Palisade — [F2] · Común ·
oculto: no. ES: «Construye tu primera empalizada.» EN: «Build your first
palisade.» Condición:
`{stat: "building_pieces_built", contains: "empalizada"}`.

**`muralla_de_piedra`** — Muralla de piedra / Stone Wall — [F2] · Infrecuente ·
oculto: no. ES: «Completa una muralla de piedra con al menos una torre.» EN:
«Complete a stone wall with at least one tower.» Condición:
`{all: [{stat: "building_pieces_built", contains: "muralla_piedra"}, {stat: "building_pieces_built", contains: "torre_defensa"}]}`.

#### Granja [F2]

**`primera_pareja`** — La primera pareja / The First Pair — [F2] · Común ·
oculto: no. ES: «Consigue tu primera pareja de animales domésticos.» EN: «Get
your first pair of domestic animals.» Condición:
`{stat: "livestock_species_raised", op: ">=", value: 1}`.

**`corral_completo`** — Corral completo / Full Pen — [F2] · Infrecuente ·
oculto: no. ES: «Cría tres especies domésticas distintas a la vez.» EN: «Raise
three different domestic species at once.» Condición:
`{stat: "livestock_species_raised", op: ">=", value: 3}`.

**`huevos_por_docenas`** — Huevos por docenas / Eggs by the Dozen — [F2] ·
Infrecuente · oculto: no. ES: «Recoge cien huevos.» EN: «Collect a hundred
eggs.» Condición: `{stat: "eggs_collected", op: ">=", value: 100}`.

#### Piratas [F3]

**`asalto_repelido`** — Asalto repelido / Raid Repelled — [F3] · Raro ·
oculto: no. ES: «Repele tu primer asalto pirata sin perder nada.» EN: «Repel
your first raid without losing anything.» Condición:
`{stat: "raids_defended", op: ">=", value: 1}`.

**`campamento_tomado`** — Campamento tomado / Camp Taken — [F3] · Raro ·
oculto: no. ES: «Derrota un campamento pirata.» EN: «Defeat a raider camp.»
Condición: `{stat: "raider_camps_defeated", op: ">=", value: 1}`.

#### Navegantes del arrecife [F3]

**`primer_trueque`** — Primer trueque / First Trade — [F3] · Común · oculto: no.
ES: «Completa tu primer trueque con el pueblo del arrecife.» EN: «Complete your
first trade with the reef village.» Condición:
`{stat: "barter_trades_completed", op: ">=", value: 1}`.

**`aliado_de_facto`** — Aliado de facto / Ally in All but Name — [F3] · Raro ·
oculto: no. ES: «Alcanza la reputación más alta con el pueblo del arrecife.» EN:
«Reach the highest standing with the reef village.» Condición:
`{stat: "reputation_village_tier", op: ">=", value: 3}`.

**`otra_forma_de_aprender`** — Otra forma de aprender / Another Way to Learn —
[F3] · Infrecuente · **oculto: sí**. ES: «Aprende una técnica de wayfinding
directamente del pueblo del arrecife.» EN: «Learn a wayfinding technique
straight from the reef village.» Condición: `{flag: "wayfinding_taught_by_village"}`.

**`sin_disparar_una_flecha`** — Sin disparar una flecha / Without Loosing an
Arrow — [F3] · Muy raro · **oculto: sí**. ES: «Ayuda a repeler un asalto pirata
sobre el pueblo del arrecife.» EN: «Help repel a raid on the reef village.»
Condición: `{flag: "village_defended_from_raid"}`.

#### Barcos [AA]

**`primera_canoa`** — Primera canoa / First Canoe — [AA] · Común · oculto: no.
ES: «Termina tu primera canoa en el astillero.» EN: «Finish your first canoe at
the shipyard.» Condición: `{stat: "boats_built", contains: "canoa"}`.

#### Tesoros

**`bajo_el_templo`** — Bajo el templo / Under the Temple — [F2] · Raro ·
**oculto: sí**. ES: «Encuentra un tesoro en un templo enterrado bajo tierra.» EN:
«Find a treasure inside a buried temple.» Condición:
`{flag: "underground_treasure_found"}`.

**`juego_de_anzuelos`** — Juego de anzuelos / Set of Hooks — [AA] · Raro ·
oculto: no. ES: «Reúne los tres anzuelos del pueblo navegante.» EN: «Collect all
three of the voyaging people's fish hooks.» Condición:
`{all: [{stat: "artifact_ids_found", contains: "anzuelo_hueso"}, {stat: "artifact_ids_found", contains: "anzuelo_nacar"}, {stat: "artifact_ids_found", contains: "anzuelo_ceremonial"}]}`.

#### Absurdos y graciosos [AA]

**`manazas`** — Manazas / Butterfingers — [AA] · Infrecuente · oculto: no. ES:
«Rompe veinte herramientas golpeando algo demasiado duro para ellas.» EN: «Break
twenty tools on something too hard for them.» Condición:
`{stat: "tools_broken_on_wrong_material", op: ">=", value: 20}`.

**`banquete_de_mil_cocos`** — Banquete de mil cocos / Feast of a Thousand
Coconuts — [AA] · Muy raro · **oculto: sí**. ES: «Abre quinientos cocos en
total.» EN: «Open five hundred coconuts in total.» Condición:
`{stat: "coconuts_opened", op: ">=", value: 500}`.

**`el_cangrejo_se_lo_llevo`** — El cangrejo se lo llevó / The Crab Took It —
[AA] · Infrecuente · **oculto: sí**. ES: «Deja que un cangrejo se lleve algo que
habías dejado en la arena.» EN: «Let a crab make off with something you left on
the sand.» Condición: `{flag: "crab_stole_item"}`.

---

## 3. El museo y las colecciones

### 3.1 Por qué existe

Cada isla explorada deja al menos una pieza que exponer: un pez para la pecera,
una concha de la orilla, un tesoro de una ruina. Guardar y colocar esas piezas en
la base es la meta de largo plazo del juego junto al barco «Limón» (GDD §3.11,
biblia §7.4, §10). A diferencia del barco, que se construye una vez y zarpa, el
museo no tiene un final fijo: sigue aceptando piezas nuevas después de que el
«Limón» haya zarpado, mientras queden colecciones sin completar (§3.2).

### 3.2 Las siete colecciones

| Colección | Fuente | Piezas | Fase |
|---|---|---|---|
| Peces | `Content/Data/fish.json` | 17 (12 especies + 5 legendarias) | [AA] |
| Conchas | `Content/Data/items.json` (tag `concha`) + nuevas | 8 | [AA] |
| Herbario (plantas silvestres) | Nueva | 10 | [AA] |
| Insectos | Nueva | 10 | [AA] |
| Artefactos y tesoros de ruinas | `Content/Data/artifacts.json` | 15 | [AA]/[F2] según procedencia |
| Minerales y cristales | `Content/Data/items.json` (tag `mineral`) + nuevas | 10 | [AA] |
| Fósiles | Nueva | 8 | [F2] |

**Artefactos vs. tesoros — decisión de reutilización de datos:** en vez de crear un
segundo catálogo, «Tesoros» es el subconjunto de `artifacts.json` con
`rarity: "raro"` o `"unico"` (10 de las 15 piezas ya existentes); «Artefactos» es
la colección completa (las 15). Evita que el museo tenga dos sistemas de datos
para lo mismo — el director pidió las dos palabras, no dos catálogos distintos.

### 3.3 Cómo se expone cada colección

| Colección | Mueble | Por qué esa forma |
|---|---|---|
| Peces | Pecera de exhibición (`pecera_museo`, nueva pieza, categoría `museo`) | Ejemplares vivos nadando, nunca disecados — coherente con «sin microgestión ni crueldad gratuita» del tono del juego. Las 5 capturas legendarias son la única excepción: se exponen como trofeo montado (ya establecido en biblia §4.6, «Trofeo»), porque cada una es un animal único y su captura ya es la historia. |
| Conchas | Bandeja de arena (`bandeja_conchas`, nueva pieza) | Se disponen sobre arena como en una playa en miniatura, no en vitrina cerrada — son inertes, no hace falta protegerlas del polvo. |
| Herbario | Marco de prensado (`marco_herbario`, nueva pieza) | Flor u hoja prensada entre dos cristales, colgado en la pared como un cuadro pequeño. |
| Insectos | Cuaderno de bocetos abierto en un atril (`atril_cuaderno`, nueva pieza) | Nunca se cazan ni se clavan con alfileres: se observan y se dibujan, reutilizando el lenguaje visual del boceto tenue de las Ideas (biblia §1.4). |
| Artefactos y tesoros | Estantería (`estanteria_museo`), vitrina (`vitrina_museo`) y panel de pared (`panel_museo`) — ya existen en `building_pieces.json` | Sin cambios; los tesoros (rareza raro/único) se colocan preferentemente en vitrina, con una peana algo más alta que el resto. |
| Minerales y cristales | Vitrina con luz interior (`vitrina_minerales`, nueva pieza) | Una luz cálida bajo la pieza — el único mueble del museo con luz propia, porque el brillo es la razón de ser de la colección. |
| Fósiles | Panel de piedra (`panel_fosiles`, nueva pieza, variante de `panel_museo` en piedra en vez de madera) | Empotrados como si la propia roca fuera el marco, coherente con cómo se encuentran (incrustados en caliza). |

### 3.4 Qué da completar una colección

Ninguna recompensa es dinero ni descuento — no hay tienda (GDD §5). Todas son
cosméticas o desbloquean una receta; ninguna cambia el equilibrio de
supervivencia.

| Colección completa | Recompensa |
|---|---|
| Peces | Pecera central de gran tamaño para la base, puramente decorativa. |
| Conchas | Móvil de conchas (decoración con sonido ambiental propio al viento). |
| Herbario | Una semilla ornamental rara (flor decorativa, no alimenticia) para el huerto. |
| Insectos | Farol de luciérnagas: fuente de luz cosmética que no gasta combustible. |
| Artefactos (las 15) | Estandarte ceremonial para la base — un gesto de respeto hacia el pueblo navegante, coherente con la regla de nunca tratarlo como recurso a saquear (GDD §8). |
| Tesoros (subconjunto raro/único) | Placa grabada con una inscripción elegida por el jugador — condición de `museo_completo`, ver nota abajo. |
| Minerales y cristales | Pigmento fosforescente para pintar paredes de la base (variante del pigmento ya descrito en biblia §3.2). |
| Fósiles | Aldaba tallada con forma de amonita para la puerta principal. |

**Nota de reconciliación con logros (§2):** completar la colección de tesoros
(subconjunto raro/único de `artifacts.json`) es la condición natural de un logro
oculto de cierre de partida. No se añade a la lista de 54 de §2 para no
sobrepasar el rango pedido (40–60); queda documentado aquí como **logro
candidato para una actualización posterior de contenido**, con id propuesto
`museo_completo` y condición
`{stat: "artifact_ids_found", op: ">=", value: 10}` sobre los ids de rareza
raro/único.

### 3.5 Ficha de ejemplo por colección

**Peces — Pez loro** (`pez_loro`, ya en `fish.json`)
ES: «Pez loro. Vive en el arrecife, muerde el coral para comer las algas que
crecen encima.» EN: «Parrotfish. Lives on the reef, bites the coral to eat the
algae growing on it.» Procedencia: arrecife, cualquier isla con costa de
arrecife. Cómo se consigue: pescado vivo con nasa o red, nunca con arpón (el
arpón mata, y esta colección expone ejemplares vivos, §3.3).

**Conchas — Cauri anillado** (`cauri_anillado`, nueva)
ES: «Cauri anillado. La marea lo deja varado entre las rocas después de una
noche de luna llena.» EN: «Ringed cowrie. The tide leaves it stranded among the
rocks after a full-moon night.» Procedencia: intermareal, cualquier costa
rocosa. Cómo se consigue: recogido a mano en bajamar.

**Herbario — Orquídea de acantilado** (`orquidea_acantilado`, nueva)
ES: «Orquídea de acantilado. Crece donde nadie más se sostiene, con las raíces
metidas en la propia roca.» EN: «Cliff orchid. Grows where nothing else can
hold on, roots wedged straight into the rock.» Procedencia: acantilados de La
Meseta. Cómo se consigue: se corta con cuchillo y se prensa en el herbario.

**Insectos — Luciérnaga de caverna** (`luciernaga_caverna`, nueva)
ES: «Luciérnaga de caverna. Parpadea despacio en la oscuridad de las cuevas más
profundas — no hace falta antorcha para verla venir.» EN: «Cave firefly.
Flickers slowly in the dark of the deepest caves — you don't need a torch to
see it coming.» Procedencia: cuevas profundas, cualquier isla con galería
minera. Cómo se consigue: se observa y se dibuja en el cuaderno, sin capturarla.

**Artefactos y tesoros — Carta de varillas y conchas** (`carta_varillas`, ya en
`artifacts.json`)
ES: «Carta de varillas y conchas.» EN: «Stick and shell chart.» Procedencia:
marae. Rareza: raro. Cómo se consigue: hallazgo en la exploración de un marae ya
descrito (biblia §9.1).

**Minerales y cristales — Obsidiana arcoíris** (`obsidiana_arcoiris`, nueva)
ES: «Obsidiana arcoíris. Solo se ve el color si la giras contra el sol — el
resto del tiempo es negra como cualquier otra.» EN: «Rainbow obsidian. The
colour only shows if you turn it against the sun — the rest of the time it's
black like any other.» Procedencia: Isla del Humo, cerca del cráter. Rareza:
raro (variante infrecuente de la obsidiana común). Cómo se consigue: minada con
pico de obsidiana o rescatado.

**Fósiles — Diente de tiburón fósil** (`diente_tiburon_fosil`, nueva)
ES: «Diente de tiburón fósil. Mucho más viejo que la isla en la que apareció —
la caliza lo trajo desde el fondo de otro mar.» EN: «Fossil shark tooth. Much
older than the island it turned up on — the limestone carried it up from the
floor of another sea.» Procedencia: caliza de La Meseta, o charcas secas
durante una ola de calor (biblia §6.2). Cómo se consigue: minado en caliza, o
recogido en una charca seca durante el temporal.

### 3.6 Lista completa de piezas por colección

**Peces (17 — de `fish.json`, nombres en inglés definidos en esta sección):**

| id | ES | EN |
|---|---|---|
| `pez_loro` | Pez loro | Parrotfish |
| `pez_cirujano` | Pez cirujano | Surgeonfish |
| `pargo` | Pargo | Snapper |
| `mero` | Mero | Grouper |
| `salmonete` | Salmonete | Goatfish |
| `pez_ballesta` | Pez ballesta | Triggerfish |
| `jurel` | Jurel | Trevally |
| `barracuda` | Barracuda | Barracuda |
| `bonito` | Bonito | Bonito |
| `atun` | Atún | Tuna |
| `dorado` | Dorado | Mahi-mahi |
| `langosta` | Langosta | Spiny lobster |
| `el_viejo` | El Viejo | The Old One |
| `sombra` | Sombra | Shadow |
| `manta_negra` | La Manta Negra | The Black Ray |
| `el_errante` | El Errante | The Wanderer |
| `rey_de_plata` | El Rey de Plata | The Silver King |

**Artefactos y tesoros (15 — de `artifacts.json`, sin cambios, reproducidos aquí
por ser la colección de referencia):** `anzuelo_hueso`, `anzuelo_nacar`,
`anzuelo_ceremonial`, `colgante_concha`, `collar_conchas`, `pectoral_nacar`,
`colgante_carey`, `figura_navegante`, `figura_gemelos`, `figura_mira_cielo`,
`carta_varillas`, `carta_oleaje`, `tapa_pintada`, `tapa_estrellas`,
`remo_ceremonial`, `remo_canoa_doble` — quince ids, nombres ES/EN ya definidos en
el propio fichero (§ leída de origen).

**Minerales y cristales (10, nuevas):**

| id | ES | EN |
|---|---|---|
| `basalto_pulido` | Basalto pulido | Polished basalt |
| `obsidiana_negra` | Obsidiana negra | Black obsidian |
| `obsidiana_arcoiris` | Obsidiana arcoíris | Rainbow obsidian |
| `cobre_nativo` | Cobre nativo | Native copper |
| `hierro_meteorito` | Hierro de meteorito | Meteoric iron |
| `azufre_cristalizado` | Azufre cristalizado | Crystallised sulphur |
| `cuarzo_transparente` | Cuarzo transparente | Clear quartz |
| `amatista_caverna` | Amatista de caverna | Cave amethyst |
| `opalo_fumarola` | Ópalo de fumarola | Fumarole opal |
| `caliza_veteada` | Caliza veteada | Streaked limestone |

**Conchas (8, nuevas):**

| id | ES | EN |
|---|---|---|
| `cauri_anillado` | Cauri anillado | Ringed cowrie |
| `oreja_de_mar` | Oreja de mar nacarada | Pearly abalone |
| `caracola_trompeta` | Caracola trompeta | Trumpet conch |
| `almeja_gigante` | Concha de almeja gigante | Giant clam shell |
| `dolar_de_arena` | Dólar de arena | Sand dollar |
| `concha_abanico` | Concha de abanico rosado | Pink fan shell |
| `turbante_verde` | Turbante verde | Green turban shell |
| `casco_real` | Concha de casco real | Royal helmet shell |

**Herbario (10, nuevas):** `orquidea_acantilado`, `hibisco_silvestre`,
`helecho_de_cueva`, `musgo_luminoso`, `loto_de_cenote`, `campanas_manglar`,
`frangipani`, `alga_roja_seca`, `liquen_de_lava`, `flor_fantasma_nocturna`.

**Insectos (10, nuevas):** `mariposa_azul_manglar`, `mariposa_cristal`,
`libelula_roja_cenote`, `luciernaga_caverna`, `escarabajo_rinoceronte`,
`mantis_hoja_esmeralda`, `polilla_luna_meseta`, `abeja_isleña`,
`hormiga_cortadora`, `saltamontes_hierba_seca`.

**Fósiles (8, nuevas):** `diente_tiburon_fosil`, `amonite_caliza`,
`coral_fosil_ramificado`, `huella_ave_ceniza`, `vertebra_pez_fosil`,
`madera_petrificada`, `huevo_fosil`, `concha_gigante_fosilizada`.

---

## 4. El diario del jugador

### 4.1 Qué es

Un cuaderno físico, gemelo del mapa (biblia §7.4, GDD §3.2), que se rellena solo:
cada hito de la partida añade una entrada corta escrita en primera persona por el
náufrago, con el día de juego como fecha. No sustituye a las Ideas del personaje
(biblia §2.5, pensamientos en el momento) ni al mapa (líneas y sellos): el diario
es la única voz que mira hacia atrás. Fichero propuesto:
`Content/Data/journal_entries.json` (nuevo), con el mismo patrón bilingüe que el
resto de `Content/Data` (`textEs`/`textEn`), un `id`, un `trigger` (el stat o flag
de `achievements.json` que dispara la entrada) y un marcador `{Day}` para el
número de día real de la partida.

### 4.2 Quince ejemplos

1. **Choque del Albatros** — ES: «Día 1. El avión ya no vuela. Yo sí puedo
   caminar, así que empiezo por ahí.» EN: «Day 1. The plane doesn't fly anymore.
   I can still walk, so that's where I start.»
2. **Primera noche superada** — ES: «Día 1, de noche. Primera noche entera.
   El fuego aguantó más que yo despierto.» EN: «Day 1, night. First whole night
   through. The fire outlasted me staying awake.»
3. **Primer fuego** — ES: «Día 1. Chispa, yesca, humo, y por fin una llama que
   no se apaga con el viento.» EN: «Day 1. Spark, tinder, smoke, and finally a
   flame the wind doesn't kill.»
4. **Primer refugio** — ES: «Día 3. Cuatro palos y una hoja de palma encima.
   No es una casa. Es un techo, que ya es bastante.» EN: «Day 3. Four sticks and
   a palm leaf on top. Not a house. A roof, which is already a lot.»
5. **Primer trazo de costa** — ES: «Día 4. Empecé el mapa hoy. Una línea
   torcida donde la playa se acaba y empieza la roca.» EN: «Day 4. Started the
   map today. One crooked line where the beach ends and the rock begins.»
6. **Primera ruina** — ES: «Día 9. Piedras que no puso el mar. Alguien vivió
   aquí antes que yo, y dejó su huella tallada.» EN: «Day 9. Stones the sea
   didn't place. Someone lived here before me, and carved their mark into it.»
7. **Primera técnica de wayfinding** — ES: «Día 11. Aprendí a leer el oleaje
   hoy. Ahora el mar me dice hacia dónde está tierra, antes de que la vea.» EN:
   «Day 11. Learned to read the swell today. Now the sea tells me where land is
   before I see it.»
8. **Primera tormenta sobrevivida** — ES: «Día 14. El techo aguantó. Yo también.
   Mañana reviso qué se llevó el viento.» EN: «Day 14. The roof held. So did I.
   Tomorrow I check what the wind took.»
9. **Primera palada de mina** — ES: «Día 16. Empecé a cavar en serio hoy. Debajo
   de la tierra hay tanta isla como encima.» EN: «Day 16. Started digging in
   earnest today. There's as much island underground as above it.»
10. **Primera cueva descubierta** — ES: «Día 18. El pico rompió la roca y detrás
    no había más roca. Una cámara entera, y nadie la había visto.» EN: «Day 18.
    The pick broke through the rock and there was no more rock behind it. A
    whole chamber, and no one had ever seen it.»
11. **Primer limón** — ES: «Día 22. El limonero dio su primer fruto. Lo planté
    el día 1. Ha tardado, pero ha llegado.» EN: «Day 22. The lemon tree gave its
    first fruit. I planted it on day one. It took a while, but it got here.»
12. **Primera captura legendaria** — ES: «Día 30. Rompió el sedal dos veces
    antes de rendirse. Le llaman El Viejo por algo.» EN: «Day 30. It snapped the
    line twice before it gave up. They call it The Old One for a reason.»
13. **Primera empalizada [F2]** — ES: «Día 38. Clavé la última estaca antes de
    que oscureciera. No sé si aguantará un asalto, pero ya no duermo con la
    puerta abierta.» EN: «Day 38. Drove the last stake before dark. I don't know
    if it'll hold off a raid, but I stop sleeping with the door open.»
14. **Primer trueque con el pueblo [F3]** — ES: «Día 52. Les llevé cuerda y
    aluminio trabajado. Me dieron semillas que no había visto nunca. Ninguno de
    los dos contó monedas.» EN: «Day 52. Brought them rope and worked aluminium.
    They gave me seeds I'd never seen before. Neither of us counted coins.»
15. **Zarpa el «Limón» [F3]** — ES: «Día 70. Última entrada desde tierra. A
    partir de esta noche, guío el barco solo con lo que aprendí del cielo.» EN:
    «Day 70. Last entry from land. From tonight on, I steer by nothing but what
    the sky taught me.»

---

## 5. La localización

### 5.1 Proceso ES → EN

El español es el idioma fuente (GDD, `localizacion.md`); nunca se escribe primero
en inglés. Flujo para un texto nuevo de esta sección (logro, ficha de museo,
entrada de diario):

1. **Escribir el español** siguiendo §1 de este documento (voz, lista negra,
   longitud, checklist).
2. **Pasarlo por el checklist anti-IA (§1.5)** antes de traducir — traducir un
   texto ya inflado solo duplica el problema en el segundo idioma.
3. **Traducir el sentido, no la frase**, en inglés británico (`localizacion.md`):
   mismo registro, misma longitud relativa, los marcadores (`{0}`, `{Count}`)
   en el mismo lugar aunque cambie el orden de las palabras.
4. **Añadir la entrada** en el sitio que le toque: C++ y `.ini` van en
   `Tools/Localization/translations/en.json`; los datos de esta sección
   (`achievements.json`, `story_es.json`, el futuro `journal_entries.json`) llevan
   el inglés en su propio campo `…En` / `…En`, al lado del español, en el mismo
   JSON — nunca en un fichero de traducción aparte (regla ya establecida en
   `localizacion.md`: los datos no pasan por el pipeline de Unreal).
5. **Verificar**: `cd Tools/Localization && uv run l10n --strict` — falla si
   falta el inglés, si los marcadores no coinciden, si la dedicatoria difiere, o
   si el inglés supera 1,3× la longitud del español.
6. **Exportar**: `uv run l10n export` regenera `Content/Localization/Game`, el
   catálogo (`catalogo.json`) y el informe. Solo entonces el texto está listo
   para compilarse en el editor (`localizacion.md`, «Compilar en el editor»).

### 5.2 Glosario ES/EN (66 términos)

Términos propios del juego, para que ningún texto nuevo traduzca dos veces la
misma palabra de dos formas distintas. Decisiones de nombre propio marcadas
**(decisión)** cuando había más de una opción razonable.

| Español | Inglés | Nota |
|---|---|---|
| Explored | Explored | Título, no se traduce |
| Archipiélago | Archipelago | — |
| Isla del Amaraje / Landing | Landing (Island) | El nombre de juego ya es «Landing» en los dos idiomas |
| Esmeralda | Emerald | — |
| Isla del Humo | Smoke Island | — |
| Los Dientes | The Teeth | — |
| Manglar de las Voces | Mangrove of Voices | — |
| Arenas Blancas | White Sands | — |
| La Meseta | The Mesa | **(decisión)** «mesa» ya es término geográfico inglés (préstamo del español); evita confundir con «plateau», más genérico |
| Isla oculta | Hidden Island | — |
| Albatros | Albatross | El avión conserva su nombre propio, con la grafía inglesa correcta del ave |
| Barco «Limón» | The boat «Limón» | El nombre propio no se traduce nunca (dedicatoria) |
| Náufrago | Castaway | También nombre del modo de juego |
| Hidroavión | Seaplane | — |
| Pueblo del arrecife | Reef village | **(decisión)** descriptivo, no nombre propio de cultura — evita el riesgo de sensibilidad cultural del GDD §8 |
| Piratas | Raiders | **(decisión)** el módulo de código ya se llama `Raiders`; se usa también en el texto que ve el jugador para mantener una sola palabra en los dos idiomas |
| Wayfinding | Wayfinding | El propio español ya usa el préstamo inglés (biblia §9.2) |
| Marae | Marae | Término real, no se traduce (biblia §9.1) |
| Petroglifo | Petroglyph | — |
| Camino de estrellas | Star path | — |
| Lectura del oleaje | Swell reading | — |
| Aves al atardecer | Birds at dusk | — |
| Nubes fijas | Fixed clouds | — |
| Color del agua | Water colour | — |
| Trueque | Barter | — |
| Reputación | Standing / reputation | «Standing» en UI corta (cabe mejor), «reputation» en texto largo |
| Museo | Museum | — |
| Vitrina | Display case | — |
| Estantería | Shelf | — |
| Herbario | Herbarium | — |
| Cuaderno de bocetos | Sketchbook | — |
| Colección | Collection | — |
| Mina | Mine | — |
| Galería (minera) | Tunnel | **(decisión)** «gallery» en inglés suena a arte; «tunnel» es el término minero correcto |
| Veta | Ore seam | — |
| Estrato | Stratum (pl. strata) | — |
| Derrumbe | Cave-in | — |
| Aire viciado | Foul air | — |
| Inundación | Flooding | — |
| Cenote | Cenote | Préstamo geológico ya internacional, se mantiene en los dos idiomas |
| Tubo de lava | Lava tube | — |
| Caverna de cristal | Crystal cavern | — |
| Río subterráneo | Underground river | — |
| Bioluminiscencia | Bioluminescence | — |
| Marea viva | Spring tide | — |
| Marea muerta | Neap tide | — |
| Bajamar | Low tide | — |
| Pleamar | High tide | — |
| Monzón | Monsoon | — |
| Ciclón | Cyclone | — |
| Escorbuto | Scurvy | — |
| Limonero | Lemon tree | — |
| Huerto | Garden | — |
| Granja | Farm | — |
| Corral | Pen | — |
| Gallinero | Coop | — |
| Pocilga | Sty | — |
| Colmena | Beehive | — |
| Estanque de peces | Fish pond | — |
| Vagón (de mina) | Mine cart | — |
| Vía (de mina) | Track | — |
| Torno | Winch | — |
| Apuntalamiento | Shoring | — |
| Muralla | Wall | — |
| Empalizada | Palisade | — |
| Torre de vigía | Watchtower | — |
| Integridad estructural | Structural integrity | — |
| Modo Explorador | Explorer mode | — |
| Dedicatoria | Dedication | «Para Almudena, mi Limón», idéntica en los dos idiomas (`localizacion.md`) |

### 5.3 Reglas de formato

- **Números.** Siempre con `FText::AsNumber` (nunca concatenación de `FString`),
  para que cada cultura use su propio separador — el español de España agrupa
  millares con punto y decimales con coma; el inglés británico al revés. Nunca se
  escribe un número a mano dentro de un `NSLOCTEXT`.
- **Unidades.** Siempre métricas, en los dos idiomas — el archipiélago no usa
  millas ni pies aunque el inglés sea británico; el juego no es real y no debe
  nada al sistema imperial. Abreviaturas: `m`, `km`, `kg`, `°C`, sin plural
  («20 m», nunca «20 ms»).
- **Plurales.** El español del juego es casi todo regular (`+s`/`+es`); la
  única irregularidad frecuente es *pez → peces*, ya presente en logros
  existentes (`fish_caught`). El inglés tiene una trampa real: *fish* es
  invariable en plural («10 fish», nunca «10 fishes»); cualquier texto nuevo
  sobre capturas debe usarlo bien. Para todo texto cuyo número pueda ser 0, 1 o
  muchos, se usa el modificador de plural de Unreal en el propio `FText` en vez
  de dos claves separadas:
  `{Count} {Count}|plural(one=pez,other=peces)` (ES) /
  `{Count} {Count}|plural(one=fish,other=fish)` (EN) — mismo marcador `{Count}`
  en los dos idiomas, verificado por el chequeo de marcadores de
  `Tools/Localization`.
- **Comillas.** Español: comillas angulares («»), ya en uso para el barco
  «Limón» y en la dedicatoria. Inglés: comillas dobles rectas, salvo en el
  nombre propio del barco, que se mantiene con «» en los dos idiomas por ser un
  nombre, no una cita.

### 5.4 Cómo añadir un idioma nuevo

El esquema actual es deliberadamente binario: cada campo de dato es un par
(`nameEs`/`nameEn`, `label`/`labelEn`…) y `ExploredLocalization::Pick` elige
entre dos culturas. **Decisión:** para 1.0 el juego se queda en ES + EN; un
tercer idioma es contenido post-lanzamiento y se añade así, sin rediseñar el
esquema (migrar a un mapa `{cultura: texto}` sería más flexible pero es un
cambio grande para una necesidad que hoy no existe):

1. Elegir el código de cultura de Unreal (p. ej. `fr` para francés) y añadirlo a
   `CulturesToStage` en `Config/DefaultGame.ini`, junto a `es` y `en`, con el
   preset de datos ICU que lo cubra (el actual `EFIGS` ya incluye francés,
   italiano y alemán; otros alfabetos necesitan revisar el preset).
2. C++ y `.ini`: crear `Tools/Localization/translations/fr.json` con la misma
   forma que `en.json` (`espacio → clave → {es, fr}`), traduciendo cada clave
   existente.
3. Datos: añadir un campo `nameFr` (o el sufijo que toque) junto a cada
   `nameEs`/`nameEn` existente en los ficheros de `Content/Data` — mismo patrón
   que ya sigue el proyecto, sin introducir un formato nuevo.
4. Extender `ExploredLocalization::Pick` para aceptar una cultura arbitraria en
   vez de una disyuntiva es/en, y los cargadores de datos para leer el campo que
   corresponda a la cultura activa.
5. Extender `Tools/Localization` (`src/l10n/`) para validar el tercer idioma con
   las mismas reglas que el inglés (marcadores, longitud, dedicatoria).
6. `uv run l10n export`, compilar con `GatherText` (`localizacion.md`,
   «Compilar en el editor») y probar con
   `UnrealEditor.exe Explored.uproject -game -culture=fr`.
7. Traducir esta sección (§1, guía de estilo) al nuevo idioma antes de traducir
   ningún texto de juego con él: sin una guía de voz propia, el tercer idioma
   hereda los mismos vicios de IA que esta sección existe para evitar.

---

## TODO de implementación

- [ ] Añadir a `Content/Data/achievements.json` los 18 stats nuevos de §2.1 (`terrain_edits_made` … `raider_camps_defeated`), con `values`/`valuesFrom` donde aplique.
- [ ] Añadir a `Content/Data/achievements.json` los 24 logros nuevos de §2.3, con `condition`, `hidden`, `icon` e id de fase como campo de datos (hoy `achievements.json` no tiene campo de fase: añadirlo, p. ej. `"phase": "AA" | "F2" | "F3"`).
- [ ] Añadir el campo `phase` a los 30 logros ya existentes en `achievements.json`, con los valores de §2.2.
- [ ] Añadir un campo de rareza (`rarity: "comun" | "infrecuente" | "raro" | "muy_raro"`) a los 54 logros de `achievements.json`, con los valores asignados en §2.
- [ ] Añadir a `Content/Data/building_pieces.json` las piezas de muralla: `empalizada`, `muralla_piedra`, `torre_defensa` (categoría nueva `defensa`, sin equivalente hoy en el fichero).
- [ ] Añadir a `Content/Data/building_pieces.json` las piezas de museo nuevas: `pecera_museo`, `bandeja_conchas`, `marco_herbario`, `atril_cuaderno`, `vitrina_minerales`, `panel_fosiles` (categoría `museo`, mismo patrón que `estanteria_museo`/`vitrina_museo`/`panel_museo`).
- [ ] Crear `Content/Data/shells.json`, `Content/Data/herbarium.json`, `Content/Data/insects.json` y `Content/Data/fossils.json` con las piezas listadas en §3.6, siguiendo el patrón bilingüe (`nameEs`/`nameEn`) de `artifacts.json`.
- [ ] Añadir el subconjunto «tesoros» a `artifacts.json` como campo derivado o consulta (`rarity` en `["raro", "unico"]`), documentado en §3.2, sin duplicar el catálogo.
- [ ] Crear `Content/Data/journal_entries.json` con el esquema de §4.1 (`id`, `trigger`, `textEs`, `textEn`, marcador `{Day}`) y las 15 entradas de §4.2 como contenido inicial.
- [ ] Añadir el logro candidato `museo_completo` (§3.4) a una futura revisión de `achievements.json` cuando el total de logros lo permita sin salir del rango 40–60, o en una actualización de contenido posterior al lanzamiento.
- [ ] Añadir las 66 entradas del glosario de §5.2 a `docs/tecnico/localizacion.md` o a un fichero de glosario propio referenciado desde ahí, para que el equipo de traducción futuro (o Codex/Antigravity delegados en localización) no reinvente ninguna de estas decisiones de nombre propio.
- [ ] Actualizar `Tools/Localization/src/l10n/` para comprobar los modificadores de plural ICU (`{Count}|plural(...)`) descritos en §5.3, hoy no verificados explícitamente por el chequeo de marcadores.
- [ ] Ejecutar `cd Tools/Localization && uv run l10n --strict` sobre cada fichero de datos nuevo en cuanto exista, antes de darlo por escrito definitivo.
- [ ] Pasar cada logro, ficha de museo y entrada de diario de este documento por el checklist de §1.5 una segunda vez en revisión de contenido, no solo en la redacción inicial.
