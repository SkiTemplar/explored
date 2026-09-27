# EXPLORED — Biblia de diseño · 04. El archipiélago, las islas y la exploración

Versión 1 · 2026-09-27 · Complementa `docs/diseno/gdd_v2.md` (documento rector) y
`docs/design/biblia-de-contenido.md`. Fuentes de código: `Source/Explored/WorldGen/`
(`ArchipelagoLayout`, `PointsOfInterest`, `FormationPlacementModel`, `TerrainDensity`),
`Source/Explored/Ocean/OceanCurrents`, `Source/Explored/Cartography/`
(`CartographyModel`, `MapStroke`, `CoastlineTrace`), `Source/Explored/Ruins/`
(`RuinsModel`, `MuseumModel`), `Source/Explored/Fauna/` (`FaunaSpawning`, `FaunaTypes`),
`Source/Explored/Boats/BoatModel`, y `Content/Data/{ruins,artifacts,story_es,boats}.json`.
Decisiones del director de 2026-09-27 (terreno volumétrico excavable, subterráneos
increíbles, ruinas y tesoros, barcos por piezas, cuatro islas de acceso anticipado,
pueblos navegantes nunca enemigos, piratas, sin tienda, referencia El Nido) se dan por
vigentes en todo el documento y no se repiten isla por isla.

`docs/diseno/exploracion.md`, citado como fuente por el GDD, no existe en este
worktree (vivía en una rama de agente no fusionada). Este documento lo sustituye como
fuente de contenido por isla; donde el GDD o la biblia de contenido ya fijaban un dato
(recursos por isla §4, mareas §6.3, capturas legendarias §4.6, wayfinding §9.2), se
respeta tal cual y se enlaza en vez de repetirlo con otras palabras.

Todas las cifras de generación procedural (radios, canales, corrientes) citan las
constantes reales de `ArchipelagoLayout.h`/`.cpp` y `OceanCurrents.h`. Donde el código
genera un **rango** por semilla, la tabla da la cifra de referencia para la **semilla
oficial** (`FArchipelagoLayout::OfficialSeed = 20260926`) y el rango completo en nota;
donde no había ninguna cifra publicada (radio y altura por isla, ubicación del pueblo y
del campamento pirata), se decide aquí con una línea de justificación, marcado **[LD-doc]**.

---

## 1. El archipiélago

### 1.1 Geometría general

- **7 islas** en cadena volcánica (`EIslandArchetype`: Landing, Emerald, Smoke, Teeth,
  Mangrove, WhiteSands, Mesa — `Count = 7`) + **1 isla oculta**, fuera de la cadena, sin
  archetype propio (`FRuinsLayout::HiddenIslandIndex`), inalcanzable en ningún mapa
  hasta reunir caminos de estrellas suficientes (§4).
- El mundo jugable es un cuadrado de **6000 × 6000 m** (`WorldHalfExtent = 3000 m`). La
  cadena se traza como un arco de Bézier de cuerda **5000–5600 m** (`HalfChord`
  2500–2800 m ×2) con flecha lateral variable; las islas se reparten a lo largo en
  escalón alterno, nunca en línea recta.
- **Canal mínimo entre dos costas nominales:** 140 m (`MinChannel`) — nunca hay dos
  islas más juntas que eso. **Canal máximo entre islas consecutivas de la cadena:**
  520 m (`MaxChainChannel`).
- Cada canal de la cadena sale, por semilla, **corto (35 % de probabilidad, 160–240 m,
  cruzable a nado con precaución o en balsa) o largo (65 %, 280–480 m, exige balsa o
  canoa)** (`ArchipelagoLayout.cpp`, tirada `Chance(0.35f)` por canal).
- **[LD-doc] Orden de la cadena, de la isla más joven (volcán activo) a la más vieja
  (atolón puro):** Humo → Los Dientes → Amaraje (Landing) → Esmeralda → Manglar de las
  Voces → La Meseta → Arenas Blancas. Sigue el ciclo real de subsidencia de una cadena
  volcánica (Darwin): roca desnuda recién emergida → isla estabilizada con selva
  incipiente → selva madura → llano en subsidencia → caliza kárstica de un arrecife
  fósil levantado → atolón puro cuando el volcán original ya está bajo el agua. Esta
  cadena geográfica (para canales y corrientes, §1.2) es **independiente** del orden de
  progresión tecnológica del jugador (§1.3): el jugador no viaja por la cadena en
  orden, cruza en diagonal.

### 1.2 Distancias y corrientes por canal de la cadena

Cifras de referencia para la semilla oficial (20260926); el algoritmo genera un valor
distinto pero dentro de los mismos rangos para cualquier otra semilla.

| Canal (cadena geográfica) | Distancia de referencia | Tipo | Corriente máx. en el centro |
|---|---|---|---|
| Humo ↔ Los Dientes | 210 m | Corto | 1,8 m/s en marea viva y viento fuerte |
| Los Dientes ↔ Amaraje (Landing) | 340 m | Largo | 1,8 m/s |
| Amaraje ↔ Esmeralda | 195 m | Corto | 1,8 m/s |
| Esmeralda ↔ Manglar de las Voces | 460 m | Largo | 1,8 m/s |
| Manglar ↔ La Meseta | 305 m | Largo | 1,8 m/s |
| La Meseta ↔ Arenas Blancas | 225 m | Corto | 1,8 m/s |

- La corriente de cada estrecho (`FOceanCurrents::CurrentAt`) empuja **a lo largo del
  canal**, nula junto a las dos costas y máxima en el centro (caída cuadrática con la
  distancia al eje); su magnitud es `180 cm/s × fuerza de marea (0,4–1,0) × (0,6 + 0,4 ×
  viento) × sentido de la marea`. En marea muerta y sin viento la corriente de un canal
  corto apenas se nota (≈ 43 cm/s); en marea viva con temporal se acerca al máximo.
- Cruzar a nado un canal corto en plena corriente de marea viva es **posible pero te
  desvía**; cruzarlo a la vuelta de marea (cambio, corriente nula) es la ventana segura
  — el propio juego no lo señala, se aprende a ojo como el resto de la navegación (§3).

### 1.3 Qué isla desbloquea qué, y con qué barco se llega

Orden de progresión del jugador (GDD §4, aquí con el barco mínimo recomendado y la
fase). El barco «mínimo» es el primero con el que el cruce es razonable, no el único
posible — con más maña y más riesgo se puede intentar antes.

| Isla | Fase | Se llega desde | Barco mínimo | Por qué ese barco |
|---|---|---|---|---|
| **Isla del Amaraje** (Landing) | [AA] | — (el hidroavión amara aquí) | A pie | Punto de partida |
| **Esmeralda** | [AA] | Landing | Balsa | Canal corto de la cadena (195 m), a favor de la corriente de marea saliente |
| **Isla del Humo** | [AA] | Esmeralda | Canoa | Cruce en diagonal fuera de la cadena directa (no son canal-vecinas): mar algo más abierto, la balsa no gobierna bien |
| **Los Dientes** | [AA] | Humo | Canoa o balsa | Canal corto de cadena (210 m) entre ambas, pero con oleaje de mar abierto: la canoa lo hace seguro, la balsa lo hace posible |
| **Manglar de las Voces** | [F2] | Los Dientes o Esmeralda | Canoa con balancín y vela | Cruce largo y expuesto (≥ 340 m en diagonal); necesita vela para no depender solo del remo |
| **Arenas Blancas** | [F2] | Manglar | Canoa con balancín y vela | Mar abierto entre ambas, fuera de la cadena directa |
| **La Meseta** | [F2] | Arenas Blancas | Canoa con balancín y vela | Canal corto de cadena (225 m) pero con corriente de marea fuerte junto al atolón |
| **Isla oculta** | [F3] | La Meseta (o cualquier isla con los 3 caminos de estrellas) | Barco «Limón» | Objetivo final: de noche, solo con el cielo, sin mapa (§4) |

Herramientas y desbloqueos técnicos por isla: sin cambios respecto al GDD §4 (tabla
«Progresión de islas y tecnología»), que sigue siendo la fuente para qué nivel de
herramienta habilita cada isla. Los estratos de minería (obsidiana solo en el Humo,
caliza solo en la Meseta, cobre y hierro de meteorito donde se indica en §2) tampoco se
repiten aquí.

---

## 2. Cada isla

Cada ficha cubre bioma y recursos exclusivos, fauna, peligros, POI (`EPoiType`) con
nombre ES/EN y qué esconden, subterráneos, ruinas y tesoros. Radio y altura máxima son
cifras de diseño **[LD-doc]** representativas del rango que genera la semilla para ese
arquetipo (`FIslandDesc::Radius`/`MaxHeight`); el generador varía el valor exacto por
semilla dentro de un ±15 % de estas cifras.

### 2.1 Isla del Amaraje / Landing Island — [AA]

**Radio 420 m · altura máx. 45 m.** Playa y palmeral sobre una laguna de agua salobre
donde amara el hidroavión Albatros; la isla más llana y segura del archipiélago, pensada
para los primeros días.

- **Bioma y recursos exclusivos:** coco (verde y maduro), palma, piedra básica (canto
  rodado, pedernal), el limón silvestre que funda el limonero de la base (biblia §7.1),
  los restos del Albatros repartidos entre la laguna y la playa.
- **Fauna:** cangrejo de los cocoteros, tortugas marinas que desovan en la playa sur en
  estación seca (biblia §6.1), gaviotas, primeros bancos de peces de arrecife junto a
  la boca de la laguna.
- **Peligros:** insolación en la playa abierta sin sombrero; cortes con la chapa
  oxidada del fuselaje; la pleamar cierra el paso a la Cueva del Sextante y puede dejar
  a alguien despistado dentro.
- **POI:**
  | Id | Nombre ES | Nombre EN | Qué esconde |
  |---|---|---|---|
  | `WreckFuselage` | Fuselaje hundido | Sunken fuselage | Pieza de casco para el barco «Limón»; interior explorable buceando |
  | `WreckWing` | Ala varada | Beached wing | Mochila del equipaje, pieza para el barco |
  | `SextantCave` | Cueva del Sextante | Sextant Cave | Entrada semihundida junto a la laguna, solo con marea baja; primer sextante roto (§4) |
  | `TurtleBeach` | Playa de las tortugas | Turtle beach | Huevos (no se tocan si se quiere respetar el desove), rastro de wayfinding «aves al atardecer» |
  | `Bottle` | Botella varada | Washed-up bottle | Mensaje corto, primer gancho de historia ambiental (§5) |
- **Subterráneo:** una cueva pequeña excavable a mano en tierra/arena (porción vertical,
  GDD §6.1) — el primer sitio donde el jugador prueba el pico contra el terreno
  volumétrico. Sin carving grande: eso empieza en el Humo.
- **Ruinas — Marae del palmeral** (`ruin_landing`): dos petroglifos, *Tortuga con siete
  puntos (las siete islas)* / *Turtle with seven dots (the seven islands)* y *Canoa de
  doble casco bajo una estrella* / *Double-hulled canoe beneath a star*. Enseña
  **Camino de estrellas hacia Esmeralda**.
- **Tesoros:** Anzuelo de hueso tallado (común, marae).

### 2.2 Esmeralda / Emerald — [AA]

**Radio 480 m · altura máx. 180 m.** Selva densa que sube hasta una cresta con
cascada; la isla con más dosel cerrado del archipiélago (`ForestFloorAmount` 0,85, la
más alta de las siete).

- **Bioma y recursos exclusivos:** madera dura (guayabo, ébano isleño), fibra, la
  primera veta de cobre superficial en basalto (biblia §3.4).
- **Fauna:** cerdo salvaje (nueva fauna terrestre, GDD §3.7), aves que aquí sí se posan
  y anidan, abejas silvestres (origen de la primera colmena capturada).
- **Peligros:** caída desde la cascada; el jabalí carga si se le acorrala contra la
  maleza; bajo el dosel cerrado no se ve el cielo, así que de noche aquí no sirve el
  camino de estrellas — hay que salir a un claro o a la costa.
- **POI:**
  | Id | Nombre ES | Nombre EN | Qué esconde |
  |---|---|---|---|
  | `WreckEngine` | Motor del Albatros | Albatros engine | Cable y piezas coladas, pieza para el barco |
  | `Waterfall` | La Cascada Verde | The Green Falls | Cueva tras la cortina de agua (sin necesidad de pico) |
  | `Viewpoint` | Mirador de la Cresta | Ridge lookout | Boceto tenue de toda la costa de Esmeralda en el mapa |
- **Subterráneo:** cueva pequeña tras la cascada, accesible sin herramienta de minado
  (solo agua y un hueco natural).
- **Ruinas — Marae de la cascada** (`ruin_emerald`): petroglifos *Cascada* / *Waterfall*
  y *Pareja de aves* / *Pair of birds*. Enseña **Lectura del oleaje** (técnica global,
  válida en todo el archipiélago).
- **Tesoros:** Colgante de concha (común, marae).

### 2.3 Isla del Humo / Smoke Island — [AA]

**Radio 380 m · altura máx. 340 m.** Un volcán con fumarolas activas pero sin erupción
jugable; la isla más joven y más alta de la cadena, y la única con obsidiana.

- **Bioma y recursos exclusivos:** obsidiana (veta cerca del cráter), azufre
  (fumarolas), vetas de cobre en el basalto, aguas termales.
- **Fauna:** sin especie terrestre propia (ninguna se adapta bien a la ceniza activa);
  aves marinas de paso y, en el mar próximo, los bancos y depredadores normales de
  talud.
- **Peligros:** gases de fumarola en bolsas cerradas de los tubos de lava (indicador de
  aire, §3.4 del GDD); calor cerca de la roca fundida vieja; las herramientas de
  obsidiana se rompen si golpean roca dura; temblores ocasionales (evento sonoro y de
  cámara, sin daño estructural en el acceso anticipado).
- **POI:**
  | Id | Nombre ES | Nombre EN | Qué esconde |
  |---|---|---|---|
  | `StarCompass` | Brújula estelar del Humo | Star compass of Smoke Island | Marae en la cumbre; enseña wayfinding (ver ruinas) |
  | `HotSpring` | Aguas termales | Hot springs | Cura pasiva de dolores musculares, punto de ánimo |
  | `Petroglyph` | Petroglifo de la boca del tubo | Lava-tube mouth petroglyph | Motivo *Volcán con humo* junto a la entrada del carving principal |
- **Subterráneo:** **tubos de lava** (carving grande, `FCaveDesc`) — el primer «lugar
  increíble» subterráneo del acceso anticipado; más adentro, cavernas bioluminiscentes.
- **Ruinas:**
  - **Marae del tubo de lava** (`ruin_smoke`): petroglifos *Volcán con humo* / *Smoking
    volcano* y *Camino de estrellas* / *Star path*. Enseña **Aves al atardecer**
    (técnica global).
  - **Brújula estelar del Humo** (`ruin_compass`): petroglifos *Estrella de ocho brazos*
    / *Eight-armed star* y *Figura mirando al cielo* / *Figure gazing at the sky*.
    Enseña **Camino de estrellas hacia Los Dientes**.
- **Tesoros:** Figura del navegante (rara, marae).

### 2.4 Los Dientes / Teeth — [AA]

**Radio 260 m** (isla principal) **+ islotes satélite · altura máx. 90 m.** Islotes
rocosos y acantilados batidos por el oleaje de mar abierto; la única isla con arco de
mar (`GenerateSeaArch`, un único arco, en un cabo de Los Dientes).

- **Bioma y recursos exclusivos:** plumas, huevos, guano, hierro de meteorito (raro,
  solo en los cráteres de impacto de esta isla).
- **Fauna:** colonias de fragatas y gaviotas (única isla marcada `bNearTeeth`: probabilidad
  de aves 0,5–0,6 frente al 0,08–0,35 del resto), tiburón de arrecife en el talud
  cercano.
- **Peligros:** caída de acantilado; oleaje fuerte entre islotes; las fragatas atacan si
  te acercas demasiado a un nido; el suelo de los cráteres de impacto es inestable.
- **POI:**
  | Id | Nombre ES | Nombre EN | Qué esconde |
  |---|---|---|---|
  | `WreckTail` | Cola hundida en el canal | Sunken tail section | Pieza para el barco, en el fondo del canal hacia Landing |
  | `Lighthouse` | Faro de Los Dientes | The Teeth lighthouse | Objetivo emergente de reparación (GDD §4); una vez reparado, referencia nocturna fija |
  | `Viewpoint` | Mirador del Arco | Sea Arch lookout | Boceto del arco de mar y de los islotes vecinos |
- **Subterráneo:** grutas marinas, solo accesibles con la bajamar.
- **Ruinas — Marae de los acantilados** (`ruin_teeth`): petroglifos *Nube fija sobre el
  horizonte* / *Still cloud on the horizon* y *Fragata en vuelo* / *Frigatebird in
  flight*. Enseña **Nubes fijas** (técnica global).
- **Tesoros:** Colgante de carey con tortuga (raro, pecio cercano).
- **[F3] Escondite pirata:** un islote secundario de Los Dientes, apartado de la ruta
  del faro. **[LD-doc]** justificación: los islotes dan cobertura natural y vista sobre
  los canales de paso de todo el archipiélago — la base perfecta para patrullar sin ser
  vistos.

### 2.5 Manglar de las Voces / Mangrove of Voices — [F2]

**Radio 520 m · altura máx. 20 m.** Llano de canales y raíces; la isla con el terreno
más bajo del archipiélago (nivel freático casi en superficie).

- **Bioma y recursos exclusivos:** arcilla roja y caolín, junco de manglar, el
  campamento científico Halden de 1974 (§6).
- **Fauna:** raya en aguas someras, cangrejos, aves zancudas, mosquitos densos en la
  estación de lluvias (biblia §5.4).
- **Peligros:** fango que atrapa (arenas movedizas en los canales secos de la estación
  seca); mosquitos con riesgo de fiebre; crecida del monzón que inunda los canales y
  puede llevarse una construcción mal anclada; agua turbia que oculta a la raya hasta
  pisarla.
- **POI:**
  | Id | Nombre ES | Nombre EN | Qué esconde |
  |---|---|---|---|
  | `HaldenCamp` | Campamento Halden | Halden Camp | Diarios y objetos de la expedición de 1974 (§6) |
  | `RadioStation` | Estación de radio abandonada | Abandoned radio station | Radio rota (ya en el inventario inicial) y bitácora de la expedición |
  | `TidePool` | Poza de marea de las raíces | Root-tangle tide pool | Pulpo, cangrejos, marisqueo fácil en bajamar |
- **Subterráneo:** solo bolsas de arcilla poco profundas; sin cuevas grandes — el nivel
  freático alto hace que cualquier excavación honda se inunde sola.
- **Ruinas — Marae de las raíces** (`ruin_mangrove`): petroglifos *Raíces de manglar* /
  *Mangrove roots* y *Cangrejo de los cocoteros* / *Coconut crab*. Enseña **Color del
  agua** (técnica global).
- **Tesoros:** Collar de conchas (común, cueva ritual cercana).

### 2.6 Arenas Blancas / White Sands — [F2]

**Radio 600 m (incluida la laguna) · altura máx. 12 m.** Atolón bajo con laguna
turquesa y arrecife exterior; la isla más vieja de la cadena.

- **Bioma y recursos exclusivos:** pesca abundante, conchas raras, velas de lona
  (recuperadas del pecio), el mejor sitio natural para un astillero.
- **Fauna:** bancos de peces de laguna, tortuga marina, tiburón de arrecife en el
  talud exterior, y **El Viejo** — el mero legendario de la cueva submarina (biblia
  §4.6).
- **Peligros:** corrientes fuertes de la laguna en el cambio de marea; cortes de coral;
  El Viejo rompe sedales que no sean de nailon.
- **POI:**
  | Id | Nombre ES | Nombre EN | Qué esconde |
  |---|---|---|---|
  | `Shipwreck` | Pecio del velero | The sailboat wreck | Vela de lona, cabos, ancla pequeña, sextante y catalejo rotos |
  | `TidePool` | Pozas del arrecife exterior | Outer-reef tide pools | Marisco y erizos |
  | `Viewpoint` | Duna de la Laguna | Lagoon dune lookout | Boceto de toda la laguna interior |
- **Subterráneo:** cueva submarina de El Viejo — se bucea, no se cava.
- **Ruinas — Marae de las canoas** (`ruin_whitesands`): petroglifos *Tres canoas* /
  *Three canoes* y *Ola que se retira* / *Receding wave*. Enseña **Camino de estrellas
  hacia La Meseta**.
- **Tesoros:** Pectoral de nácar (raro, ruina sumergida).
- **[F3] Campamento estacional del pueblo del arrecife:** los navegantes vienen aquí a
  pescar y curar velas en la estación seca; su sede permanente está en La Meseta (§2.7).

### 2.7 La Meseta / The Mesa — [F2, con cenotes y ríos subterráneos como parche de
contenido dentro del propio acceso anticipado]

**Radio 440 m · altura máx. 210 m.** Macizo kárstico de caliza — la única isla del
archipiélago con este estrato — con cresta irregular, farallones sueltos
(`GenerateLimestoneSlabs`) y cenotes. Referencia visual directa: la caliza de El Nido.

- **Bioma y recursos exclusivos:** caliza (única fuente del juego), fibras, cultivos,
  cenotes de agua dulce turquesa.
- **Fauna:** cabra salvaje en los farallones, aves rapaces.
- **Peligros:** sumideros ocultos por la vegetación (un paso en falso cae a un cenote
  sin aviso); caída desde los farallones sueltos; las cabras embisten si se las
  acorrala; una crecida del monzón llena los cenotes y los ríos subterráneos muy
  deprisa.
- **POI:**
  | Id | Nombre ES | Nombre EN | Qué esconde |
  |---|---|---|---|
  | `TideObservatory` | Observatorio de mareas abandonado | Abandoned tide observatory | La tabla de mareas del archipiélago (biblia §6.3) |
  | `Viewpoint` | Mirador de los Farallones | Sea-stack lookout | Boceto de la cresta entera |
  | `Petroglyph` | Petroglifo del cenote | Cenote petroglyph | Motivo *Marae con estatua*, junto a la boca de un cenote |
- **Subterráneo:** **cenotes de agua turquesa** y **ríos subterráneos navegables en
  balsa** (ambos, parche de contenido dentro del propio acceso anticipado); más
  profundo, **ruinas enterradas del pueblo navegante** — un templo con petroglifos y
  tesoro de museo bajo tierra.
- **Ruinas — Marae de la meseta** (`ruin_mesa`): petroglifos *Marae con estatua* /
  *Marae with statue* y *Luna nueva* / *New moon*. Enseña **Camino de estrellas hacia
  la isla oculta** — el tercero de los mínimos tres necesarios (§4).
- **Tesoros:** Tapa de las estrellas (único, marae) — la pista final antes de la isla
  oculta.
- **[F3] Sede del pueblo del arrecife (los navegantes):** en La Meseta, junto a los
  cenotes de agua dulce. **[LD-doc]** justificación: agua dulce fiable todo el año y
  posición defendible en altura la hacen la mejor base para un pueblo permanente; el
  observatorio de mareas ya abandonado en el mismo terreno explica por qué Halden
  documentó su existencia sin llegar a contactar con ellos (§6).

### 2.8 La isla oculta — [F3]

No tiene `EIslandArchetype` propio: vive fuera de la cadena (`FRuinsLayout::HiddenIslandIndex`)
y no aparece en ningún mapa que el jugador pueda dibujar por sí mismo hasta el final.
Solo es alcanzable de noche, con el barco «Limón», guiándose exclusivamente por el
cielo, con al menos **3 caminos de estrellas** conocidos (`RequiredStarPaths = 3` en
`ruins.json`) — el archipiélago ofrece 4 sitios que lo enseñan (Landing, Brújula
estelar del Humo, Arenas Blancas, La Meseta), así que el jugador no necesita
encontrarlos todos. Contenido y objetivo final: fuera del alcance de este documento
(vive en el GDD §0 y §9.3 de la biblia de contenido; no repetido aquí).

---

## 3. La cartografía

El mapa lo dibuja el jugador, nunca el juego (GDD §3.2). Todo lo que sigue describe
mecanismos ya implementados (`CartographyModel`, `MapStroke`, `CoastlineTrace`) y cómo
se leen en pantalla.

### 3.1 Instrumentos y qué marca cada uno

| Instrumento | Origen | Qué añade al mapa | Precisión |
|---|---|---|---|
| **A mano** (caminar/nadar cerca de la costa) | Siempre disponible | Trazo de costa (`FMapStroke`), punto a punto | Con la deriva del jugador (`State.Drift`): la posición donde el mapa dibuja no es exacta, como iría a estima |
| **Marca a mano** (`AddMark`) | Siempre disponible | Un sello puntual (agua, cueva, peligro, recurso, ruina — `story_es.json::map_marks`) donde el jugador *cree* estar | Con la misma deriva del trazo de costa |
| **Catalejo** (recuperado del pecio de Arenas Blancas) | Botín del pecio | Marca a distancia de algo visto sin visitarlo (`AddSpyglassMark`) | Error de rumbo y de distancia que **crece con el alcance**: útil para apuntar un objetivo, no para clavarlo |
| **Sextante** (reparado con piezas del campamento Halden o del pecio) | Botín + reparación | Marca exacta (`AddSextantMark`), sin deriva | Precisión total: la única forma de fijar un punto con certeza en el mapa |
| **Mirador** (`Viewpoint`) | Puntos de interés | Boceto tenue en anillo (`FMapSketch`) de la costa visible desde ahí, **sin confirmar** hasta que se recorre a pie | Aproximado por diseño: es una pista, no un hecho |

### 3.2 Niveles de detalle y tinta

- **Cobertura por isla** (`FMapIslandCoverage::Fraction`): fracción de la costa
  recorrida sobre el total de puntos de muestreo de esa isla. El mapa no rellena solo
  ni interpola: cada tramo no recorrido queda literalmente en blanco.
  - **< 25 % recorrido:** trazos sueltos sin conectar, la forma de la isla es una
    intuición.
  - **25–75 %:** costa reconocible con huecos evidentes; suficiente para navegar de
    día con margen de error.
  - **> 90 %:** isla prácticamente completa; solo faltan calas o tramos peligrosos
    (acantilados, arrecife) que el jugador ha evitado a propósito.
- **Tinta:** cada trazo tiene `Ink` (intensidad, 1 recién dibujado) y `Blur` (cuánto se
  ha corrido); la lluvia y el paso del tiempo bajan `Ink` y suben `Blur`, y con `Blur`
  alto crece también `ToleranceMeters` — la línea se simplifica y pierde detalle, no
  solo color. Repasar un tramo mojado lo refresca.
- **La mesa de cartografía** de la base (biblia §7.4) es donde se copia el mapa de
  campo al mapa grande: el original de campo puede seguir corriéndose sin que la copia
  de la pared se vea afectada.

### 3.3 Marcadores

Los seis tipos de sello de `story_es.json` (`map_marks`): agua dulce / fresh water,
cueva / cave, peligro / danger, recurso / resource, ruina / ruin, resto del Albatros /
Albatros wreckage. El jugador los coloca a mano sobre cualquier trazo o marca; no hay
un séptimo tipo genérico — si no encaja en ninguno, no se marca (coherente con la regla
anti-agobio de la biblia §8.3).

### 3.4 Niebla de guerra

No existe una capa de «niebla» que se descorre sola: lo no explorado es papel en
blanco. La única información gratuita es el propio horizonte visible desde donde el
jugador esté (regla anti-aburrimiento §1 de la biblia: humo de otra isla, un pico, una
bandada) — verlo desde lejos no lo dibuja en el mapa; solo indica que hay algo hacia
donde ir. Las anotaciones de wayfinding (§4) son la única excepción: una vez aprendida
la técnica en una ruina, el propio conocimiento se dibuja solo, como una idea que el
personaje ya tiene, no como algo que el juego revele por descubrimiento pasivo.

---

## 4. La navegación

- **Viento:** alisio constante en la estación seca (biblia §6.1); en primeras lluvias y
  monzón, más variable y con chubascos; en la temporada de ciclones, calma bochornosa
  entre tormentas. La canoa con balancín y el barco «Limón» dependen de él para
  navegar sin remar; la balsa y la canoa simple no.
- **Corrientes:** de marea, descritas en §1.2 (`FOceanCurrents`) — empujan a lo largo
  de cada estrecho, calmadas junto a las costas y máximas en el centro del canal, con
  fuerza según la marea viva/muerta (`SpringNeapFactor`, 1,0 en luna nueva o llena,
  0,4 en cuartos) y el viento.
- **Estrellas:** el camino de estrellas (§9.2 de la biblia) es la técnica principal de
  navegación nocturna y la única forma de llegar a la isla oculta; se aprende en las
  ruinas marcadas en §2, nunca por el HUD.
- **Mareas:** dos pleamares al día (`FOceanTide::CyclesPerDay = 2`), remitido a biblia
  §6.3 para el detalle de qué abre y qué cierra cada una — no se repite aquí.
- **Tormentas:** temporales y ciclones, remitido a biblia §6.2 (tabla completa de
  señales previas, efectos y después) — no se repite aquí. Aplicado a la navegación: un
  barómetro bajando (recuperado del Albatros) es el único aviso fiable de que hay que
  buscar puerto antes de que llegue.

---

## 5. La exploración recompensada

Regla de ritmo (biblia §8.2.1: «siempre hay un horizonte visible») traducida a cifras
de diseño **[LD-doc]**:

| Escala | Qué encuentra el jugador | Densidad de referencia |
|---|---|---|
| **Costa, caminando o nadando cerca de la orilla** | Hallazgo menor: concha, huella, fruta caída, brillo en el agua, botella | Cada 60–90 m recorridos |
| **Interior o sendero de isla** | Hallazgo mayor: un recurso singular (veta, árbol raro), una vista, la entrada de una cueva | Cada 250–400 m recorridos |
| **Isla completa** (radio 260–600 m según §2) | Total de POI mayores marcados en el mundo (`EPoiType`, sin contar ambientación) | 8–14 por isla |
| **Cada 2–5 minutos de juego activo** (caminando, remando o buceando, no parado fabricando) | Al menos un hallazgo (menor o mayor) entra en esa ventana | Consecuencia directa de las dos densidades anteriores a paso normal de un náufrago cargado (~90 m/min) |

Esto no es un temporizador ni una tirada oculta: es la consecuencia de cómo se reparten
los POI y el scatter de recursos en el mundo (`PointsOfInterest`, `FormationPlacementModel`,
`VegetationScatter`), verificable jugando una isla de principio a fin. Ninguna isla
tiene tramos de más de 400 m sin nada que ver o coger, salvo que el propio jugador haya
elegido un atajo por mar abierto.

---

## 6. Los tesoros

- **Mapas del tesoro:** no son un objeto de inventario aparte — la propia hoja de mapa
  del jugador, con las marcas de catalejo o sextante colocadas sobre una pista
  encontrada en una ruina o un diario (§7), **es** el mapa del tesoro. No hay un ítem
  «mapa del tesoro X» separado del mapa general.
- **Pistas:** un petroglifo, un diario del campamento Halden o un elemento de altar
  (`altar` en `ruins.json::elements`) da una descripción en prosa corta, nunca
  coordenadas exactas («al pie del farallón partido, donde la costa gira al este»); el
  jugador la traduce a una marca en su propio mapa con el instrumento que tenga.
- **Excavación:** una vez localizado el punto aproximado, el tesoro está enterrado bajo
  una capa fina de terreno editable (§3.4 del GDD) o dentro de una ruina sumergida —
  nunca requiere más de una `FCaveDesc` pequeña o un par de golpes de pala; el reto está
  en encontrar el sitio, no en cavar un pozo.
- **Catálogo:** 15 artefactos en 3 rarezas (`artifacts.json`) repartidos entre 4
  procedencias — marae, cueva ritual, pecio, ruina sumergida — ya asignados isla por
  isla en §2. El catálogo completo, sus tamaños y sus muebles de exposición (estantería,
  vitrina, panel) viven en `artifacts.json` y no se repiten aquí; §2 solo señala qué
  tesoro **arranca** el jugador en cada isla, no la lista completa (algunos artefactos,
  como los de ruina sumergida, aparecen solo con marea viva extrema y no están atados a
  una isla única).

---

## 7. La historia ambiental

Tres capas, sin diálogos ni texto largo (regla dura del GDD §7.2), coherentes con
`story_es.json`:

### 7.1 Diarios (campamento Halden, 1974)

No existe hoy un fichero de datos para estas entradas (`Content/Data/` no tiene
`halden_diaries.json` ni equivalente); §8 lo deja como tarea. Contenido decidido aquí,
tono seco de expedición científica, nunca grandilocuente:

| Id | ES | EN |
|---|---|---|
| `halden_01` | «Día 14. La radio se moja más de lo que transmite. Marchena dice que es la antena. Yo digo que es la isla.» | "Day 14. The radio gets wetter than it transmits. Marchena blames the antenna. I blame the island." |
| `halden_02` | «Encontramos tallas en la roca, bajo el tubo de lava. Nadie las esperaba. Fotografiamos, no tocamos nada.» | "Found carvings in the rock, under the lava tube. Nobody expected them. We photograph, we touch nothing." |
| `halden_03` | «El barómetro no ha dejado de bajar en dos días. Guardamos los cuadernos en la lata de galletas, por si acaso.» | "The barometer hasn't stopped falling in two days. Notebooks go in the biscuit tin, just in case." |
| `halden_04` | «Se acabó el café el martes. Se acabó la paciencia el jueves. Quedan tres semanas de expedición.» | "Coffee ran out Tuesday. Patience ran out Thursday. Three weeks of expedition left." |
| `halden_05` (última, sin fecha) | «Si alguien lee esto: la radio nunca funcionó del todo bien. No busquéis un mensaje de socorro, no lo hubo. Solo nos fuimos con la siguiente marea baja.» | "If anyone reads this: the radio never really worked. Don't look for a distress call, there wasn't one. We just left on the next low tide." |

El campamento no cuenta una tragedia (evita el cliché de «expedición desaparecida»): la
2026-09-27 el director fija que Halden **se fue**, no se perdió — coherente con «ni
diálogos ni cinemáticas» y con no competir en tono con el pueblo del arrecife.

### 7.2 Petroglifos

Las 30 entradas de `story_es.json::petroglyph_themes` son el catálogo completo; §2
asigna 2 por ruina (`petroglyphsPerSite = 2`) a las 8 ruinas existentes — 16 de los 30
usados de forma explícita. Los 14 restantes quedan disponibles para tesoros sueltos,
elementos de cueva ritual sin ruina asociada y contenido de fase 2/3 (más marae al
completar Manglar, Arenas Blancas y la Meseta con contenido nuevo).

### 7.3 El campamento Halden como objeto, no como cadáver

Sin restos humanos ni esqueletos (coherente con «sin personajes humanos animados» del
GDD §7.2): el campamento se lee por lo que dejaron — la radio rota, la lata de
galletas, los cuadernos, la caja de herramientas — nunca por un cuerpo o una escena de
muerte. Es el mismo principio que ya rige los pecios y las ruinas.

---

## TODO de implementación

- [ ] [AA] `Content/Data/`: crear `halden_diaries.json` (id, texto ES, texto EN, isla,
      POI asociado) con las 5 entradas de §7.1; añadir su parseo a `RuinsSubsystem` o a
      un nuevo `HaldenLoreSubsystem` ligero, y su check a `Tools/DataCheck`.
- [ ] [AA] `Source/Explored/Fauna/FaunaTypes.h`: añadir `EFaunaSpecies::WildBoar` y
      `EFaunaSpecies::WildGoat` (terrestres, GDD §3.7) y sus reglas de aparición en
      `FaunaSpawning.cpp` (Esmeralda para el jabalí, La Meseta para la cabra).
- [ ] [AA] `Source/Explored/WorldGen/TerrainDensity.cpp` / `WorldGenCommandlet.cpp`:
      verificar que el carving de tubo de lava del Humo (`FCaveDesc`) tiene una
      variante «boca visible desde el marae de la cumbre» para que `StarCompass`
      (`ruin_compass`) y `ruin_smoke` queden junto a entradas de cueva reales, no solo
      conceptualmente cerca.
- [ ] [AA] `Content/Data/ruins.json`: asignar explícitamente `Teaches` y
      `StarPathTarget` a los 8 `sites` según §2 (Landing→Esmeralda, Brújula del
      Humo→Los Dientes, Arenas Blancas→La Meseta, La Meseta→isla oculta; los otros 4
      sitios con las técnicas globales de §2) si el fichero de datos real aún no trae
      esa asignación cableada en el lado de gameplay (`RuinsLayout`).
- [ ] [AA] `Content/Data/artifacts.json` o un nuevo `map_clues.json`: dar forma de dato
      a las «pistas en prosa» de §6 (una por artefacto raro/único, 6 pistas) en vez de
      dejarlas solo en este documento.
- [ ] [AA] Landing: confirmar en `PointsOfInterest.cpp` que `SextantCave` solo es
      accesible con `FOceanTide::Level < 0` (bajamar), coherente con §2.1.
- [ ] [F2] `Source/Explored/WorldGen/`: extender `FormationPlacementModel` o
      `TerrainDensity` con el carving de **cenote** y **río subterráneo navegable**
      para La Meseta (hoy solo hay tubo de lava y cueva/arco de superficie
      documentados en el código explorado).
- [ ] [F2] Nuevo módulo `Villages` (GDD §3.9): ubicar la sede en La Meseta y el
      campamento estacional en Arenas Blancas según §2.6–2.7; sección de guardado
      `reputation`.
- [ ] [F2] `Content/Data/`: revisar si Manglar (arcilla, junco), Arenas Blancas
      (conchas raras, velas de lona) y La Meseta (caliza, cultivos) ya tienen sus
      recursos exclusivos dados de alta en `items.json`/`templates.json`; si no,
      añadirlos antes de abrir esas islas.
- [ ] [F3] Nuevo módulo `Raiders` (GDD §3.8): escondite pirata en el islote secundario
      de Los Dientes según §2.4; rutas de patrulla que pasen cerca de Arenas Blancas y
      La Meseta (amenaza real al pueblo, coherente con GDD §3.9).
- [ ] [F3] `Source/Explored/Ruins/RuinsModel`: confirmar que `HiddenIslandIndex` exige
      `RequiredStarPaths = 3` de los 4 caminos disponibles (Landing, Brújula del Humo,
      Arenas Blancas, La Meseta) y no los 4 completos.
- [ ] [F3] Arte: el pueblo del arrecife y el campamento pirata necesitan asset propio
      (marae «vivo» con estructuras ligeras, GDD §7.1); no reutilizar directamente las
      piezas de ruina, que deben leerse como abandonadas.
