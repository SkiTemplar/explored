# EXPLORED — Documento de diseño del juego (GDD v2)

Versión 2 · 2026-09-27 · Unreal Engine 5.6 · Windows (objetivo: Steam)

Sustituye a `docs/superpowers/specs/2026-09-26-explored-design.md` (v3) como documento
rector. Complementa y no repite `docs/design/biblia-de-contenido.md` (catálogo de
objetos, propiedades, verbos), `docs/diseno/exploracion.md` (contenido por isla) y
`docs/roadmap.md` (estado de implementación). Decisiones del líder de diseño de
2026-09-27, confirmadas con Rodrigo, marcadas **[LD]**.

---

## 0. Resumen en una frase

Sandbox de supervivencia, exploración y **transformación** en un archipiélago del
Pacífico sin nombre: un hidroavión se estrella, sobrevives con lo que encuentras, y
cada isla se convierte en tu mundo propio — minada, cultivada, amurallada — mientras
dibujas tu mapa a mano y descubres las ruinas de un pueblo de navegantes antiguo, los
restos de una expedición científica de 1974, y un pueblo vivo con el que comerciar o
del que defenderte.

> Dedicado a Almudena, mi Limón.

Sin acto ni final obligatorio. Objetivo final opcional: construir el barco «Limón» y
zarpar de noche guiándote solo por las estrellas, o quedarte.

---

## 1. Visión, gancho y referencias

**Gancho:** *transformar las islas en tus mundos* — sobrevive, explora, construye,
excava y amuralla cada isla hasta que sea irreconociblemente tuya, sin perder nunca de
vista el horizonte de la siguiente.

### Referencias de Steam

| Referencia | Qué se toma |
|---|---|
| **Valheim** | Progresión por niveles de herramienta ligados a bioma/material (tosco → tallado → obsidiana → rescatado, ahora también por dureza de estrato bajo tierra); construcción libre por piezas con integridad estructural; sensación de mundo hecho a mano con generación procedural por debajo. |
| **Subnautica** | Exploración silenciosa guiada por curiosidad y horizonte visible, sin combate como eje; progresión por profundidad y equipo (aquí también válida bajo tierra: minas más profundas exigen mejor herramienta y luz); historia ambiental sin diálogos. |
| **Stardew Valley** | Pueblo con reputación en vez de una tienda con precios fijos; ritmo agrícola por estaciones; relación con NPCs que se gana con respeto y regalos/trueque, no con dinero. |

---

## 2. Pilares y bucles

### 2.1 Pilares (máximo 5)

1. **Sobrevivir con el cuerpo.** Hambre, sed, temperatura, heridas, escorbuto: el
   cuerpo es la interfaz principal (GDD v3 §8.3, sin cambios).
2. **Transformar la isla.** Minar, cultivar, construir y amurallar: cada acción dejar
   una huella persistente y visible en el terreno. **[LD]** Pilar nuevo que sustituye a
   «una base que crece» (lo incluye y lo amplía: ya no es solo la base, es la isla
   entera).
3. **Cartografiar a mano.** El mapa en blanco que dibuja el jugador, ahora con una capa
   subterránea propia (§3.2).
4. **Libertad total.** Sin camino fijo ni orden obligatorio entre islas, sistemas o
   superficie/subsuelo.
5. **Historia mínima y ambiental**, con un pueblo vivo tratado con respeto y un
   conflicto real (piratas) que le da textura sin traicionar el pilar 5 original
   («ni diálogos ni cinemáticas»).

Se retira como pilar independiente «hecho por código sin asset externo» (era el pilar 6
del GDD v3): pasa a ser una regla de producción (§7), no un pilar de diseño, porque ya
no es absoluta (§7.1).

### 2.2 Bucles de juego por escala de tiempo

| Escala | Bucle | Ejemplo |
|---|---|---|
| 30 segundos | Ver algo → acercarse → coger, cavar o usar | Una fruta, una veta a la vista, un pez |
| 5 minutos | Necesidad → plan → fabricar/cavar/cazar → resolver | «Necesito cobre» → pico → veta → fundir |
| 1 día | Amanecer → trabajo en superficie o en la mina → volver antes de la noche → fuego → dormir | Turno de mina con antorchas de sobra para volver |
| 1 semana | Proyecto grande | Abrir una galería con raíles, amurallar la base, criar el primer estanque |
| 1 estación | Adaptarse | Apuntalar antes del monzón (crecidas inundan minas bajas), aprovechar la seca para cavar |
| 1 partida | Explorar, coleccionar, transformar | Mapa completo, museo lleno, isla irreconocible, barco «Limón» |

### 2.3 Mundo vivo (ritmo del ecosistema) **[LD]**

Sección nueva: qué hace que el archipiélago se sienta habitado y no decorado.

| Capa | Qué aporta | Sistema |
|---|---|---|
| Fauna salvaje con rutina | Animales terrestres y marinos con horario propio (pastan, beben, duermen), huyen o cargan según especie | `Fauna` (§3.7) |
| Aves e insectos | Bandadas en vuelo, enjambres de abejas, luciérnagas, mariposas — ambiente con movimiento constante | `Fauna` (boids ya existentes) |
| Flora reactiva | Hierba que se aparta al pasar, palmeras que se doblan en el ciclón, quemado que rebrota | Ya implementado (`VegetationScatter`, `Weather`) |
| Mundo interactivo | Todo árbol se tala y cae según el golpe y la pendiente; el tocón rebrota salvo que se arranque; ramas sueltas bajo los árboles | `WorldGen` (§3.12, modelos puros hechos) |
| Día/noche, estaciones, marea | Cambian qué se puede hacer, no solo cómo se ve: pesca, mareas que abren pasos, mina que se inunda con la crecida | `Sky`, `Weather`, `Events`, `Ocean` (ya implementado) |
| Pueblo con horario **[F3]** | Los navegantes del arrecife trabajan, comercian y hacen ofrendas en su propio ciclo diario | `Villages` (§3.9, nuevo) |
| Piratas que patrullan **[F3]** | Rutas de patrulla y asaltos programados, no solo reactivos (las piezas de muralla que se defienden de ellos llegan antes, en fase 2, §3.8) | `Raiders` (§3.8, nuevo) |
| Rastros del pasado | Campamentos Halden, pecios, petroglifos: cuentan historia por lo que dejan, nunca por texto largo | Ya implementado (`Ruins`, `Narrative`) |

---

## 3. Mecánicas

Cada mecánica: objetivo, reglas, progresión, interfaz, riesgos técnicos, dependencias.
Las que ya están descritas en detalle en la biblia de contenido o en el GDD v3 se
resumen y enlazan; el detalle nuevo (minería, minas, ganadería, murallas, pueblos) se
desarrolla entero aquí.

### 3.1 Supervivencia (el cuerpo)

Sin cambios de diseño; ver GDD v3 §8.3 y `docs/tecnico/cuerpo.md`, `docs/tecnico/estadisticas.md`.

- **Objetivo:** el cuerpo (sed, hambre, energía, sueño, temperatura, vitamina C, ánimo)
  es la interfaz principal; sin HUD de números.
- **Reglas:** necesidades bajan con el tiempo/esfuerzo, heridas y estados (corte,
  esguince, quemadura, intoxicación, infección, fiebre) se leen por sensación.
- **Progresión:** de sobrevivir el día 1 a que la supervivencia deje de ser
  protagonista hacia la semana 2–3 (biblia §5.1).
- **Interfaz:** sensaciones de personaje + reloj de muñeca opcional.
- **Riesgos técnicos:** ninguno nuevo; `FSurvivalModel`/`FBodyModel` ya tienen specs en
  host verdes (roadmap).
- **Dependencias:** `Survival`, `Cooking`, `Farming` (limonero contra el escorbuto).

### 3.2 Exploración y cartografía a mano

Sin cambios de fondo respecto al GDD v3 §5; **ampliación [LD]:** capa subterránea del
mapa.

- **Objetivo:** el mapa lo dibuja el jugador, nunca el juego.
- **Reglas:** trazado de costa al caminar/nadar cerca de la orilla, bocetos tenues
  desde miradores, marcas y sellos manuales, instrumentos de precisión (brújula,
  catalejo, sextante). **Nuevo:** al entrar en una galería minera, el mapa abre una
  **hoja subterránea** por sistema de túneles (no por isla completa): se dibuja igual
  que la costa, a mano, según por dónde ha cavado o caminado el jugador; no hay
  minimapa de mina.
- **Progresión:** de la costa de Landing a las siete islas y sus minas, hasta reunir
  los caminos de estrellas.
- **Interfaz:** el mapa como objeto físico en las manos (superficie) + la hoja
  subterránea, que se hojea igual, dentro del mismo objeto de mapa.
- **Riesgos técnicos:** una hoja subterránea por sistema de galerías multiplica el
  número de `UDataAsset` de trazos; mitigar generando la hoja bajo demanda (solo
  cuando el jugador entra la primera vez) y descartando trazos de galerías rellenadas
  (colapsos reparados) igual que hoy se difumina un trazo mojado.
- **Dependencias:** `Cartography`, `Mining` (nuevo, §3.4), `Save` (una sección más,
  patrón idéntico a `world`).

### 3.3 Construcción libre

- **Objetivo:** casa y base 100 % modulares, sin plantillas cerradas; cualquier isla
  puede amurallarse o urbanizarse.
- **Reglas:** piezas por encaje (suelo, pared, puerta, techo, escalera, pilote) en
  materiales que mejoran (hoja/bambú/madera/piedra), integridad estructural con apoyo,
  degradación por clima, reparación. Sin cambios respecto al GDD v3 §8.6; el kit se
  **reutiliza** para las piezas de raíl (§3.5) y de muralla (§3.8), que son piezas de
  construcción con reglas propias, no un sistema aparte.
- **Progresión:** refugio inclinado → cabaña sobre pilotes → casa modular → base
  amurallada con producción propia.
- **Interfaz:** vista previa fantasma de la pieza, resaltado de apoyo insuficiente.
- **Riesgos técnicos:** ninguno nuevo; `FBuildingModel` tiene specs en host verdes.
  Extender el mismo modelo a piezas de raíl y muralla es añadir plantillas de pieza,
  no un sistema nuevo.
- **Dependencias:** `Building`, `Save` (sección `building` ya existe).

### 3.4 Terraforming y minería **[LD, mecánica central]**

- **Objetivo:** cavar el terreno volumétrico con herramientas propias, literalmente —
  no un minijuego abstracto ni un recurso que se genera al golpear una roca
  decorativa. El terreno editado se queda editado.
- **Reglas:**
  - El terreno ya es un campo de densidad continuo (`FTerrainDensity`, sólido si
    densidad < 0) poligonizado por chunk con Surface Nets (`FTerrainChunkBuilder`,
    `FSurfaceNets`). Cavar = restar densidad (volver el valor positivo) en una región
    alrededor del punto de impacto de la herramienta; apilar = sumarla.
  - **Estratos por profundidad y por isla** (mapean a color/dureza de
    `FTerrainDensity::SurfaceLayers`, que ya distingue arena/hojarasca/roca/carácter
    volcánico):

    | Estrato | Dónde | Dureza | Herramienta mínima |
    |---|---|---|---|
    | Tierra y arena | Superficial, toda isla | 1 | Pala tosca |
    | Arcilla | Bolsas costeras y de manglar | 1 | Pala tosca |
    | Caliza | La Meseta (macizo kárstico) | 2 | Pico de piedra |
    | Basalto | Subsuelo general, más denso cerca del Humo | 3 | Pico tallado |
    | Obsidiana (veta) | Isla del Humo, cerca del cráter | 4 | Pico de obsidiana (frágil contra roca dura: biblia §2.4) |
    | Veta de cobre | Basalto de cualquier isla, poco profunda | 2 | Pico de piedra |
    | Hierro de meteorito | Rara, cráteres de impacto en Los Dientes | 3 | Pico tallado |
    | Azufre | Fumarolas del Humo | 1 | A mano o pala |
    | Cristal (caverna de cristal) | Profundidad alta, cualquier isla | 4 | Pico de obsidiana o rescatado |

  - **Peligros:**
    - **Derrumbe:** un hueco sin apoyo por encima de una luz de N metros (a definir en
      pruebas de PIE) colapsa; una viga de apoyo (pieza del kit de construcción, §3.3)
      colocada dentro de la galería lo evita.
    - **Oscuridad:** sin luz propia; antorcha o lámpara de aceite obligatorias más allá
      de la luz del día que entra por la boca.
    - **Aire viciado:** en bolsas cerradas y profundas, un indicador de aire que baja
      obliga a airear (romper hacia la superficie o una chimenea) o a retirarse; no es
      un temporizador de muerte súbita, es presión, como el resto de necesidades.
    - **Inundación:** una galería que conecta con el mar o con el nivel freático se
      inunda si no se sella; una crecida (biblia §6.2) puede inundar minas bajas en la
      estación de monzón.
  - **Lugares increíbles generados bajo tierra** (carvings deterministas por semilla,
    no manuales): cenotes de agua turquesa en La Meseta, tubos de lava en el Humo,
    cavernas de cristal, ríos subterráneos navegables en balsa, ruinas antiguas
    enterradas del pueblo navegante (templos con petroglifos y tesoros de museo),
    grutas marinas que solo se abren con la bajamar, cavernas bioluminiscentes. Todas
    parten del mismo `FCaveDesc` (cápsula deformada) que ya usa el terreno para cuevas
    y arcos de superficie; lo nuevo es su tamaño, profundidad y contenido, no el
    mecanismo.
- **Herramientas y números [aprobado por Rodrigo 2026-09-27]** (modelo puro
  `FTerrainEditModel`, spec `Explored.TerrainEdit`; integración en
  `docs/tecnico/terreno-editable.md`):
  - **Pico: ahuecar de forma progresiva.** Cada golpe (0,9 s) resta densidad con un
    pincel esférico **irregular** (radio 0,5 m ± 20 %, forma distinta en cada golpe) cuyo
    centro entra 15 cm en la pared en la dirección del golpe. Lo que arranca se divide
    por la dureza. Si la herramienta no llega al material, el pico **rebota** y no hay
    cambio. Así se cavan minas, túneles y escaleras.

    | Material (modelo) | Estratos del GDD | Dureza | Herramienta mínima | Golpes/m³ con la mínima | s/m³ |
    |---|---|---|---|---|---|
    | Arena | Arena | 0,75 | Pala tosca (1) | 6 | 5,4 |
    | Tierra | Tierra, arcilla, azufre | 1 | Pala tosca (1) | 6 | 5,4 |
    | Caliza | Caliza, veta de cobre | 2 | Pico de piedra (2) | 12 | 10,8 |
    | Basalto | Basalto, hierro de meteorito | 3 | Pico tallado (3) | 18 | 16,2 |
    | Obsidiana | Obsidiana, cristal | 4 | Pico de obsidiana/rescatado (4) | 24 | 21,6 |

    Regla: 6 × dureza golpes por m³ con la herramienta mínima, y ÷ 1,5 por cada nivel de
    herramienta por encima. **Nunca bajan de 6 golpes/m³**, porque en blando el límite es
    el tamaño del hueco (≈ 0,17 m³ por golpe), no la dureza: para mover tierra se usa
    la pala. Ejemplos: una galería de 1 × 2 × 5 m en basalto con pico tallado lleva
    180 golpes (≈ 2 min 42 s); en caliza con pico tallado, 80 golpes (≈ 1 min 12 s).
  - **Pala: caminos que parecen caminos.** Cada pasada (1,2 s) lleva el terreno hacia
    un plano objetivo, el de los pies del jugador, que puede inclinarse para hacer
    rampas. Dentro de 1 m de radio alcanza el plano. Entre 1 y 1,75 m hace una
    transición suave (smoothstep) y más allá no cambia nada. Solo actúa a ±1 m del plano
    (una pala no arrasa un cerro) y mueve como mucho 25 cm por pasada en tierra (33 cm en
    arena). Corta lo que sobresale y **rellena lo que falta solo con la tierra que se
    lleva** más la que corta en esa misma pasada. Cada pasada **compacta** (+34 %): con
    2 pasadas la franja es **camino** (capa de superficie propia) y con 3 queda
    totalmente compactada. Un camino compactado es un 50 % más duro de cavar. Picar o
    echar tierra encima deshace el camino de esa columna. Solo funciona en arena, tierra
    y arcilla; en caliza o roca, la pala rebota.
  - **Transportar y echar tierra.** Todo lo que se arranca sale en m³ exactos
    (`VolumeRemoved`) y se puede volver a colocar: echar tierra rellena una esfera de
    0,5 m hasta agotar lo que se lleva. **El volumen se conserva:** cavar y volver a
    echar la misma tierra deja el mismo sólido, y nunca se coloca más de lo que se lleva
    (lo comprueba el spec).
  - **Escaleras picadas.** El jugador marca el arranque y la dirección, y la escalera
    se ajusta a una **rejilla de 30 cm**: origen en múltiplos de 30 cm, 8 rumbos como el
    kit de construcción, contrahuella de 15, 30 (por defecto) o 45 cm (sube por una
    ladera o baja a una mina), huella de 30 a 90 cm, ancho de 0,6 a 3 m (1 m por
    defecto), altura libre de 1,8 a 3 m (2,2 m por defecto; en ladera empinada o bajo
    tierra la escalera es un túnel) y hasta 64 peldaños. Tallarla cuesta golpes: cada
    golpe sobre la escalera marcada arranca como mucho el volumen de un golpe de pico en
    ese material, y la escalera se va definiendo poco a poco hasta quedar completa.
  - **Persistencia.** Todo queda en la capa `"terrain"` de la sección `world` del
    guardado (deltas por chunk de 8 m en milímetros enteros). Un agujero sigue cavado
    al recargar la partida (criterio de salida de §6.1).
- **Progresión:** pala tosca (tierra/arcilla) → pico de piedra (caliza, cobre) → pico
  tallado (basalto, hierro) → pico de obsidiana/rescatado (obsidiana, cristal, minas
  profundas con más riesgo de derrumbe y aire viciado).
- **Interfaz:** sin HUD de minería; el indicador de aire y la necesidad de luz se leen
  igual que el resto del cuerpo (§3.1): respiración, visión, temblor.
- **Riesgos técnicos:** ver §7.3 (pipeline de edición en tiempo de ejecución) — es el
  riesgo técnico más alto de todo el documento.
- **Dependencias:** `WorldGen` (nuevo submódulo `Mining` sobre `TerrainDensity`),
  `Building` (apuntalamiento), `Save` (capa de ediciones persistente), `Cartography`
  (hoja subterránea).

### 3.5 Minas y vagones

- **Objetivo:** sacar de la galería el mineral que hoy solo se puede cargar a cuestas,
  viaje a viaje, y moverlo hasta la base por vía.
- **Reglas:** vías por tramos con encaje (pieza del kit de construcción, como raíles),
  cambios de agujas, vagón que se empuja a mano o se tira con un torno de cuerda en el
  extremo de la base (sin motores: coherente con el nivel tecnológico del juego —
  nunca hay combustión interna). Ascensor de pozo (torno vertical) opcional para minas
  profundas.
- **Progresión:** galería corta con acarreo a mano → vía simple con vagón empujado →
  red con cambios de agujas y torno → ascensor de pozo en la mina más profunda de cada
  isla.
- **Interfaz:** colocación de vía igual que una pieza de construcción (vista previa
  fantasma, encaje); el vagón se monta o se empuja con la misma interacción contextual
  del resto del juego (máximo tres verbos).
- **Riesgos técnicos:** un grafo de vía + física de vagón por tramos es trabajo nuevo,
  no una extensión trivial de `Building`; el vagón necesita seguir una spline con
  colisión contra el propio terreno editado (§3.4), que cambia en tiempo de
  ejecución. Sin prototipo de PIE, el coste real de esta mecánica es una incógnita.
- **Dependencias:** `Building` (piezas de vía), `Mining` (galerías), un nuevo módulo
  ligero `Tramway` (grafo de vía + vagón).
- **Decisión de alcance:** **queda fuera del acceso anticipado** (§6). La minería
  manual y las cuevas sí entran porque reutilizan sistemas que ya existen
  (`TerrainDensity`, `Building`, `Save`); los raíles y vagones son un sistema nuevo de
  principio a fin con un riesgo técnico no verificado (grafo de vía + física de
  vagón sobre terreno editable). Entran en la fase 2 una vez que la minería manual
  esté jugada y equilibrada y se sepa cuánto pesa de verdad transportar mineral a
  mano.

### 3.6 Granja: cultivos y animales

**Cultivos:** sin cambios respecto al GDD v3 §8.7 y biblia §7 (limonero, huerto por
etapas estáticas, riego, compost, espantapájaros).

**Animales domésticos [LD, con una decisión de arte que se descarta explícitamente]:**

- **Objetivo:** criar animales para comida, materiales y compañía de granja, más allá
  de la colmena y el estanque de peces ya existentes.
- **Decisión que se descarta:** el GDD v3 §10 y §12 prohibían **toda** fauna terrestre
  y **toda** animación por esqueleto («ninguna fauna camina sobre patas ni se posa»).
  El líder de diseño pide explícitamente granjas de animales domésticos y fauna
  salvaje terrestre usando los packs CC0 de Quaternius, que traen animales **con
  rig**. Esta decisión sustituye a la regla anterior: se mantiene «sin esqueleto» solo
  para la fauna **marina** y las bandadas ya construidas por shader (§3.7), que
  siguen su propio pipeline y no se tocan. La razón por la que se descartó
  originalmente (riesgo de producción de modelos/esqueletos/animación propios, ver
  memoria de sesión 2026-09-26) queda resuelta al usar packs CC0 ya riggeados y
  probados en producción en lugar de producción propia: el riesgo técnico que
  motivó la prohibición ya no aplica.
- **Reglas:** gallinero, pocilga, corral — estructuras del kit de construcción con una
  especie doméstica asociada (gallina, cerdo, cabra); alimentar, recoger huevos/leche,
  reproducción simple por pares en el corral. Cangrejera y estanque de peces (ya
  existentes) siguen siendo la vía sin animales con rig para quien juegue en un
  hardware limitado (ver riesgo de rendimiento abajo).
- **Progresión:** domesticar la primera pareja (capturada o comprada al pueblo del
  arrecife, §3.9) → corral básico → corral con reproducción → granja mixta con varias
  especies.
- **Interfaz:** indicador de hambre/salud del animal en el propio animal (postura,
  sonido), no en un HUD.
- **Riesgos técnicos:** IA de fauna terrestre con rig es coste de CPU/GPU nuevo
  (animación esquelética + navegación en el terreno editable de §3.4); mitigar con
  LOD de IA agresivo (el patrón ya existe para fauna marina: «IA optimizada por nivel
  de detalle», roadmap) y con un tope bajo de animales domésticos vivos a la vez por
  base.
- **Dependencias:** `Fauna` (extensión a terrestre doméstica), `Building` (corrales),
  `Save` (nueva sección `livestock`).

### 3.7 Animales salvajes e IA

- **Objetivo:** un archipiélago habitado, no solo navegable.
- **Reglas:** fauna marina (ya implementada: boids, cerebro marino, aparición, LOD,
  animación por shader — sin cambios) **más** fauna salvaje terrestre nueva **[LD]**:
  cerdos y cabras salvajes, aves que ahora sí pueden posarse y anidar en tierra (se
  descarta la restricción «siempre en vuelo» del GDD v3 §10 para las especies
  terrestres nuevas; las bandadas marinas de fondo, que son ambiente lejano, se
  quedan como están). Rutina diaria (pastar, beber, dormir), huida o carga según
  especie, percepción (vista, oído, olfato con el viento).
- **Progresión:** primero fauna pacífica y observable (cerdo salvaje, cabra), después
  fauna que exige cautela cerca de las minas más profundas y de los territorios de
  los piratas.
- **Interfaz:** ninguna; se lee por comportamiento.
- **Riesgos técnicos:** máquinas de estados en C++ ya existen para fauna marina; la
  fauna terrestre añade navegación (NavMesh o campo de navegación propio sobre el
  terreno editable, que cambia con la minería) — coste no trivial si el terreno se
  reedita con frecuencia (invalidar y reconstruir navegación por chunk).
- **Dependencias:** `Fauna`, `WorldGen`/`Mining` (el terreno que pisa la fauna puede
  cambiar bajo sus pies).

### 3.8 Murallas y defensa (piratas) **[LD]**

- **Objetivo:** dar sentido a fortificar una base: los piratas son el conflicto real
  del juego, nunca el pueblo del arrecife.
- **Reglas:** murallas y torres de defensa como piezas del kit de construcción (§3.3);
  trampas simples (estacas, foso); los piratas patrullan en rutas fijas por el
  archipiélago y **asaltan** bases con recursos visibles o con reputación baja frente
  a ellos (mecánica de saqueo: se llevan lo que esté fuera de un contenedor cerrado si
  ganan el asalto). Defender con éxito da recursos rescatados de su campamento.
  Atacar un campamento pirata es opcional y da acceso a sus propios recursos
  (metal trabajado, pólvora si se decide más adelante).
- **Progresión:** cerca de estacas básica → empalizada → muralla de piedra con torres
  → base fortificada capaz de resistir un asalto de categoría alta.
- **Interfaz:** aviso de asalto por sonido/humo en el horizonte (nunca un marcador de
  misión, coherente con la regla anti-agobio de la biblia §8.3).
- **Riesgos técnicos:** IA de asalto (pathing hacia la base, elección de objetivo,
  daño a piezas de construcción) es sistema nuevo; reutiliza la percepción y las
  máquinas de estados de `Fauna` como base de código, pero el comportamiento de grupo
  y saqueo es trabajo propio.
- **Dependencias:** `Building` (integridad ya soporta daño), nuevo módulo `Raiders`,
  `Villages` (la reputación con el pueblo también puede subir o bajar la frecuencia de
  asaltos si el jugador colabora con los piratas o los combate cerca de un marae).

### 3.9 Pueblos: los navegantes del arrecife (comercio y reputación) **[LD, confirmado]**

- **Objetivo:** un pueblo ficticio en las islas avanzadas, con la misma dignidad que
  una cultura real — **nunca** un enemigo que exterminar ni un tópico colonial. El
  jugador es un náufrago que llega a su territorio, no un colonizador.
- **Reglas:**
  - Ubicación: islas avanzadas (Arenas Blancas y La Meseta son las candidatas
    naturales por su cercanía a los marae ya descritos en `exploracion.md`).
  - **Comercio:** trueque, nunca moneda (coherente con «sin tienda», §5). El pueblo
    valora lo que el jugador trae según su propia necesidad, no una tabla de precios
    fija.
  - **Aprendizaje de navegación:** el pueblo puede enseñar directamente una técnica de
    wayfinding (§3.10) si la reputación es alta, como alternativa a encontrarla en una
    ruina.
  - **Reputación:** sube por respetar sus marae y su tierra: mantener las minas y la tala lejos de sus ruinas
    activas, devolver a su sitio los objetos rituales que se encuentren y resolver
    todo intercambio con ellos por trueque. Baja si el jugador caza en su territorio,
    saquea sus ruinas o roba en vez de trocar.
    Reputación alta abre mejores trueques y enseñanza de wayfinding; reputación baja
    cierra el trueque y puede acercar más patrullas piratas a su territorio (un
    pueblo desprotegido por el jugador es más vulnerable).
  - **Nunca es un enemigo:** no hay forma de «ganar» contra el pueblo ni misión que
    pida atacarlo; el único conflicto armado del juego es con los piratas (§3.8).
- **Progresión:** primer contacto (reputación neutra, trueque básico) → reputación
  alta (trueque preferente, técnicas de navegación, quizá la primera pareja de
  animales domésticos) → aliado de facto en la defensa contra piratas si su
  territorio se ve amenazado.
- **Interfaz:** el trueque se resuelve en un objeto en cada mano igual que el resto de
  interacciones del juego (máximo tres verbos), no un menú de tienda.
- **Riesgos técnicos:** IA de NPC con horario, diálogo no verbal (gestos, no texto
  largo — coherente con el pilar 5) y sistema de reputación persistente son trabajo
  nuevo de principio a fin; ningún sistema actual lo cubre.
- **Riesgo de sensibilidad cultural:** ver §8.
- **Dependencias:** nuevo módulo `Villages`, `Save` (sección `reputation`), `Ruins`
  (los marae ya existentes ganan un dueño activo).

### 3.10 Navegación y barcos

Sin cambios respecto al GDD v3 §6 y §8.10. Balsa → canoa → canoa con balancín y vela →
barco «Limón»; wayfinding por marae (camino de estrellas, lectura del oleaje, aves al
atardecer, nubes fijas, color del agua), ahora también enseñable por el pueblo del
arrecife con reputación alta (§3.9).

- **Objetivo:** moverse entre islas y, al final, navegar de noche solo por las
  estrellas.
- **Reglas / progresión / interfaz:** ver GDD v3 §6, §8.10 (sin cambios).
- **Riesgos técnicos:** ninguno nuevo; `FBoatModel` tiene specs en host verdes.
  Pendiente de siempre: malla del «Limón» y astillero (roadmap).
- **Dependencias:** `Boats`, `Ruins`, `Villages`.

### 3.11 Museo y tesoros

Sin cambios respecto al GDD v3 §7 y biblia §10. **Ampliación:** las ruinas enterradas
bajo tierra (§3.4) son ahora una fuente más de tesoros de museo, con el mismo
catálogo y la misma vitrina.

- **Objetivo:** la base como pequeño museo que crece con la partida.
- **Reglas:** artefactos de marae, cuevas rituales, pecios y ahora también templos
  enterrados (§3.4); exposición en estantería o vitrina; catálogo con silueta y
  procedencia en el mapa.
- **Progresión:** primer tesoro expuesto → colección por isla → colección completa.
- **Interfaz:** vitrinas físicas en la base, catálogo en el mapa.
- **Riesgos técnicos:** ninguno nuevo; `FRuinsModel`/`FMuseumModel` ya con specs en
  host verdes.
- **Dependencias:** `Ruins`, `Mining` (nueva fuente), `Building` (vitrinas).

### 3.12 Mundo interactivo: tala universal **[director, 2026-09-27]**

Principio aprobado por el director: **el mundo entero es interactivo y se comporta de
forma natural.** Esta sección cubre la primera mecánica de ese principio. Después
vendrán la arena viva, el astillero de balsas y otras interacciones naturales.

- **Objetivo:** que cualquier árbol, palmera o arbusto se pueda talar o modificar, y que
  el bosque se regenere sin necesitar reglas especiales.
- **Reglas:**
  - **Golpes según la herramienta.** Se puede talar a mano, con algo contundente o con
    filo. La pala no tala, pero arranca tocones y desbroza arbustos. Mezclar
    herramientas a mitad de tala suma fracciones del trabajo: cada golpe aporta
    1/N del total, así que 2 de 4 con hacha más 4 de 8 a mano tumban la palmera.
  - **Qué suelta y dónde cae.** Los troncos quedan repartidos a lo largo del tronco
    caído (en su 80 % inferior). Las ramas y las hojas caen en la copa, y los frutos,
    en el radio de la copa. Mientras se golpea se siguen soltando los `PerHitDrops` de
    la recolección.
  - **Dirección de caída.** El árbol cae hacia donde empujan los golpes, es decir,
    hacia el lado contrario al jugador, pero la pendiente tira cuesta abajo. Las dos
    fuerzas pesan igual con unos 27° de pendiente (peso 2 × tangente). Con más
    pendiente cae cuesta abajo aunque se golpee desde abajo. Si los golpes se anulan
    en terreno llano, cae hacia una dirección fija de cada ejemplar.
  - **Tocón.**
    - Al talar queda un tocón. A los N días echa un brote, que crece desde el 15 %
      hasta adulto y entonces se puede volver a talar.
    - Si se arranca con pala (tocón o brote), no vuelve nunca. Es la única forma de
      deforestar para siempre y sustituye a la regla anterior de que los árboles
      grandes no rebrotaban.
    - El arbusto no cae: se desbroza en el sitio.
  - **Ramas sueltas.** Bajo cada ejemplar en pie se encuentran ramas en el suelo (hojas
    secas bajo las palmeras). Se recogen a mano y reaparecen a su ritmo hasta llenar la
    capacidad. Con la celda llena no se acumula nada, así que no aparece una ráfaga de
    ramas al volver. Un tocón no da ramas: un bosque talado se queda sin leña fácil.

  | Especie | Mano / contundente / filo | Altura (m) | Copa (m) | Rebrote + adulto (días) | Pala para arrancar | Ramas del suelo (máx., por día) |
  |---|---|---|---|---|---|---|
  | Palmera (`Palm`) | 8 / 7 / 4 | 9 | 3 | 12 + 30 | 4 | 2, 0,5 (hoja de palma) |
  | Gigante (`JungleGiant`) | 14 / 12 / 6 | 22 | 6 | 20 + 60 | 8 | 4, 1,5 |
  | Copa ancha (`JungleWide`) | 11 / 10 / 5 | 14 | 7 | 15 + 45 | 6 | 4, 1,2 |
  | Manglar (`Mangrove`) | 9 / 8 / 4 | 7 | 4 | 20 + 30 | 5 | 2, 0,6 |
  | Sotobosque (`Understory`) | 5 / 4 / 3 | 5 | 2 | 10 + 12 | 3 | 2, 0,8 |
  | Arbusto (`Shrub`) | 1 / 1 / 1 (y pala 1) | — | 1 | 3 + 2 | 1 | 1, 0,3 |

  Los golpes a mano y con filo son los mismos que en `FHarvestModel`, y hay un spec
  que lo comprueba. Rendimiento al caer:
  - **Gigante:** 2–3 troncos, 1–2 de madera dura, 1–2 de corteza, 0–1 de resina y 2–4
    ramas secas.
  - **Palmera:** 1 tronco, 2–4 hojas, 0–2 de fibra, 1–3 cocos maduros, 0–2 verdes y 0–1
    cáscaras.
  - **Resto de especies:** ver `FFellingModel::DefaultProfiles`.
- **Progresión:** al principio se tala a mano y se recogen ramas del suelo. Con el hacha
  se tala en la mitad de golpes, y con la pala se despeja terreno para siempre (huerto,
  base, astillero).
- **Interfaz:** sin barra de progreso. El árbol tiembla más con cada golpe y cruje en el
  penúltimo. El brote se ve crecer.
- **Riesgos técnicos:**
  - Guardar la hora de tala de cada tocón exige una sección nueva, porque los deltas
    actuales no tienen tiempo.
  - La caída es un actor temporal, no física.
  - Ver `docs/tecnico/tala-integracion.md`.
- **Dependencias:** `WorldGen` (`FFellingModel`, `FGroundBranchModel`, `FHarvestModel`),
  `Save` (sección `vegetationClock`), `Sky` (reloj de juego), `Carry` (clase de
  herramienta).

---

## 4. Progresión de islas y tecnología

| Isla | Recursos que aporta | Herramientas que habilita | Desbloqueos |
|---|---|---|---|
| **Isla del Amaraje** (Landing) | Coco, palma, piedra básica, restos del Albatros | Nivel 0 (a mano) → Nivel 1 (tosco) | Refugio, huerto, primer trazo de mapa |
| **Esmeralda** | Madera dura, fibra, primera ruina | Nivel 2 (tallado); pala tosca (tierra/arcilla) | Cabaña, primera técnica de wayfinding, minería superficial |
| **Isla del Humo** | Obsidiana, azufre, vetas de cobre, aguas termales | Nivel 3 (obsidiana); pico de piedra y tallado | Minería en basalto, primeras herramientas de filo superior |
| **Los Dientes** | Plumas, huevos, guano, hierro de meteorito (raro) | — | Faro a reparar, vetas de hierro |
| **Manglar de las Voces** | Arcilla, peligro, estación Halden | — | Cerámica, campamento científico |
| **Arenas Blancas** | Pesca, conchas raras, velas de lona | — | Astillero, posible sede del pueblo del arrecife |
| **La Meseta** | Fibras, cultivos, caliza, cenotes | Pico de piedra (caliza) | Farallones, observatorio de mareas, posible sede del pueblo del arrecife |
| **Nivel 4 (rescatado/fundido)** | Aluminio del fuselaje, chapa | Requiere banco de chatarra (cualquier isla con restos del Albatros) | Herramientas de mayor durabilidad, piezas del barco «Limón» |
| **Isla oculta** | — | — | Objetivo final: 3 caminos de estrellas + barco «Limón» |

Los estratos de minería (§3.4) no están atados a una sola isla salvo la obsidiana
(Humo) y la caliza (Meseta): tierra, arcilla y cobre aparecen en varias.

---

## 5. Economía sin tienda

- **Regla dura:** no hay ninguna tienda con precios fijos en todo el juego (decisión
  del líder de diseño, sin excepción). Todo se fabrica, se intercambia o se
  encuentra.
- **Recetas:** el sistema de propiedades y plantillas de la biblia de contenido
  (§2–3) es la única fuente de objetos fabricables; no hay una moneda intermedia
  entre materiales y objeto final.
- **Trueque con el pueblo del arrecife (§3.9):** valor **relativo**, no fijo. Cada
  trueque se resuelve comparando la utilidad del objeto ofrecido para el pueblo (p.
  ej. metal trabajado o medicina valen más que fruta común, que ellos ya tienen) y la
  reputación actual, nunca una tabla de precios en una moneda. Reputación alta mejora
  el tipo de cambio, no introduce dinero.
- **Valor de los tesoros:** los tesoros del museo (§3.11) no tienen valor de cambio;
  su valor es narrativo y de colección (catálogo del mapa). No se pueden trocar con el
  pueblo — son parte del propio pasado del jugador, no mercancía. Esto evita que el
  museo compita con el trueque como «segunda economía».
- **Piratas:** no comercian; su relación con el jugador es puramente de conflicto
  (§3.8), lo que mantiene el trueque como el único canal de intercambio social del
  juego, y así de simple.

---

## 6. Alcance por fases

Prioridad constante: **porción vertical primero** — la isla de inicio (Landing) 100 %
jugable y pulida antes de ampliar al resto del archipiélago (GDD v3 §19, sin cambios).

### 6.1 Porción vertical (Landing)

Debe entrar **completo** antes de tocar cualquier otra isla:

- Supervivencia básica completa (§3.1).
- Manos, fabricación tosca y tallada, refugio → cabaña.
- Huerto y limonero.
- Cartografía de Landing (costa + un boceto de mirador).
- Una ruina explorable con una técnica de wayfinding.
- Un tesoro expuesto en el museo.
- **Nuevo respecto al GDD v3:** minería manual básica (pala en tierra/arena) y al
  menos una cueva pequeña excavable a mano, para probar el pipeline de edición de
  terreno (§7.3) en el entorno más simple posible antes de llevarlo a las otras islas.

Criterio de salida: un jugador puede aterrizar, sobrevivir, construir, cavar un
agujero que se queda cavado al recargar la partida, cartografiar y encontrar un
tesoro, todo en Landing, sin salir de la isla.

### 6.2 Acceso anticipado

**Entra:**

- Supervivencia completa, exploración, construcción libre, terraforming y minería
  manual (§3.4, sin raíles), huerto y granja de cultivos, 3–4 islas (Landing,
  Esmeralda, Humo, Los Dientes — cubren tierra, caliza no incluida aún, basalto,
  obsidiana y vetas de cobre/hierro, más el faro como objetivo emergente).
- Cuevas y lugares subterráneos generados en esas islas (tubos de lava en el Humo,
  grutas marinas en Los Dientes); cenotes de La Meseta y ríos subterráneos quedan
  para cuando esa isla entre en un parche de contenido dentro del propio acceso
  anticipado.
- Fauna marina completa (ya implementada) y una primera pasada de fauna salvaje
  terrestre (§3.7) en las islas disponibles.

**No entra** (fases posteriores, dentro del propio ciclo de acceso anticipado antes de
la versión 1.0):

- **Fase 2:** minas con raíles y vagones (§3.5, justificado su aplazamiento en esa
  misma sección), animales domésticos (§3.6), murallas y defensas (§3.8), resto de
  islas (Manglar, Arenas Blancas, Meseta completa).
- **Fase 3:** pueblo del arrecife con comercio y reputación (§3.9), conflicto con
  piratas (§3.8 completo, con patrullas y asaltos), isla oculta y final.

Criterio de salida del acceso anticipado hacia 1.0: las tres fases completas, más
equilibrado, rendimiento objetivo (GDD v3 §17.4) y empaquetado Win64.

---

## 7. Producción

### 7.1 Assets: qué es CC0 y qué es propio **[LD, pivote respecto al GDD v3 §12]**

El GDD v3 exigía «todo el contenido visual hecho por código, ningún asset externo».
**Se descarta esa regla absoluta.** Se sustituye por:

| Origen | Qué cubre | Notas |
|---|---|---|
| **Kenney** (CC0) | Kits de entorno, mobiliario genérico, iconos de UI | Base rápida para props no protagonistas |
| **KayKit** (CC0) | Props y kits de construcción estilizados, herramientas | Encaja con el low-poly pulido buscado |
| **Quaternius** (CC0) | Animales (domésticos y salvajes, con rig), naturaleza | Habilita §3.6 y §3.7 sin producción propia de rig |
| **Blender propio** (script versionado en `Tools/Blender/`) | Lo único del juego: marae, petroglifos, campamento Halden, el barco «Limón», objetos narrativos | Se mantiene el pipeline actual para todo lo que da identidad propia |
| **Terreno, agua, rocas escaneadas** | Sin cambios | Terreno volumétrico (§7.3), océano y el kit de rocas ya fotobasheado desde Poly Haven CC0 (`docs/art/texturas.md`) siguen como están |

Precedente ya existente en el repo que respalda el pivote: `VolcanicRock` y
`Limestone` ya son fotobasheados desde fotografías CC0 de Poly Haven
(`docs/art/texturas.md`); el proyecto ya rompió la regla «todo por código» antes de
esta decisión, solo que sin decirlo en el GDD. Este documento lo hace explícito.

Regla de estilo que **no** cambia: low-poly estilizado y pulido (GDD v3 §13); los
packs CC0 se seleccionan y, si hace falta, se retocan en materiales/color para no
romper la paleta por isla.

### 7.2 Reglas de producción que se mantienen

- Sin personajes humanos animados en pantalla, sin diálogos, sin multijugador, sin
  cinemáticas pregrabadas (GDD v3 §12, sin cambios: el pueblo del arrecife y los
  piratas se comunican por gesto y comportamiento, nunca por texto largo ni voz).
- Pipeline reproducible por script (`Tools/build_content.ps1`) para todo lo que sigue
  siendo generado (texturas, audio, música, terreno).

### 7.3 Pipeline técnico de terraforming en tiempo de ejecución

El terreno actual (`Source/Explored/WorldGen/TerrainDensity.h`,
`SurfaceNets.h`, `TerrainChunkBuilder.h`) es un **campo de densidad calculado como
función pura** a partir de ruido y del layout del archipiélago — inmutable, sin
estado, seguro entre hilos. No hay ninguna rejilla de vóxel almacenada hoy: cada
consulta de densidad se recalcula. Minar exige romper esa pureza sin perder sus
garantías de rendimiento:

1. **Capa de ediciones.** `FTerrainEditModel` (hecho, con spec en el host): deltas
   de densidad dispersos por chunk de 8 m sobre una rejilla de 0,25 m, aplicados
   **encima** de la densidad procedural pura (se guardan muestras editadas, no
   operaciones, así que el coste de consulta no crece con el número de golpes).
   `FTerrainDensity` no cambia: el remallado lee base + delta con
   `FTerrainEditModel::BuildChunkGrid`, y el terreno no tocado no paga nada.
2. **Persistencia en el guardado.** Mismo patrón que `FSaveScatterDeltas`
   (`docs/tecnico/guardado.md`): las ediciones se guardan como deltas por celda de
   chunk, no como un vóxel completo por chunk — coherente con el criterio ya usado
   para la recolección de scatter (`"harvested"`). Una nueva capa `"terrain"` en
   `FSaveWorldDeltas`.
3. **Remallado solo de los chunks tocados.** `FTerrainChunkBuilder::Build` ya trabaja
   por chunk; minar solo debe invalidar y reconstruir la malla de los chunks cuya caja
   se solape con el radio de la edición (más el margen de una muestra que ya exige
   `FSurfaceNets` para no dejar grietas entre chunks vecinos). No se toca el resto del
   mundo.
4. **Cuevas grandes como carvings deterministas.** Los «lugares increíbles» (§3.4) no
   son ediciones del jugador: son `FCaveDesc` generados por semilla igual que las
   cuevas y arcos de superficie ya existentes, con radio y forma mayores. No
   necesitan capa de ediciones propia; son parte de la densidad procedural, como
   siempre.

### 7.4 Riesgos de rendimiento (terraforming)

- **Coste por consulta de densidad:** cada resta/suma de densidad revisada en cada
  llamada a `Density()` puede convertir una función barata en una búsqueda cara si el
  jugador cava mucho en un chunk. Mitigar con una rejilla dispersa por chunk (no una
  lista lineal) y un límite razonable de ediciones acumuladas por chunk antes de
  «congelarlas» en una rejilla de vóxel local explícita (coste fijo, ya no
  proporcional al número de ediciones).
- **Remallado en tiempo real:** cavar debe sentirse inmediato pero remallar un chunk
  entero por cada golpe de pico es caro; agrupar ediciones en una ventana corta
  (varios golpes) antes de reconstruir la malla, con una malla temporal de baja
  fidelidad mientras tanto (ya existe precedente de LOD en el propio terreno).
- **Guardado:** una partida larga con minería extensa puede acumular muchos deltas de
  terreno; el mismo mecanismo de rangos/mapa de bits que ya usa `FSaveScatterDeltas`
  (`"r:0-39,57"` o base64) debería bastar, pero no está probado a la escala de una
  mina completa — verificar con una prueba de estrés antes de M3.
- **Navegación de fauna sobre terreno editable** (§3.6, §3.7): cada remallado que
  afecte a una zona con fauna cercana debe invalidar y reconstruir su campo de
  navegación local, no el del archipiélago entero.

Ninguno de estos cuatro riesgos tiene hoy un prototipo verificado en PIE: es el mayor
riesgo técnico de todo el documento (ver §8).

---

## 8. Riesgos de diseño y mitigación

| Riesgo | Tipo | Mitigación |
|---|---|---|
| Minería + raíles + granja de animales + pueblo + piratas es mucho para un equipo que ya viene arrastrando 17 sistemas sin compilar en local (roadmap) | Alcance | Recorte explícito por fases (§6): raíles y vagones fuera del acceso anticipado con justificación técnica propia; pueblo y piratas en fase 3. La porción vertical de Landing prueba el pipeline de minería antes de escalarlo. |
| Pipeline de edición de terreno en tiempo real sin prototipo (§7.4) | Rendimiento | Prototipo aislado en Landing (§6.1) antes de tocar el resto de islas; prueba de estrés de guardado antes de M3; presupuesto de ms por sistema ya es práctica establecida (Unreal Insights, GDD v3 §18). |
| Fauna terrestre con rig (§3.6, §3.7) añade coste de animación y de navegación sobre terreno que cambia | Rendimiento | LOD de IA agresivo (patrón ya usado en fauna marina), tope de animales vivos por base, invalidación de navegación solo en el chunk afectado. |
| El pueblo del arrecife cae en el tópico de «isleños místicos» o «recurso narrativo del náufrago blanco» | Sensibilidad cultural | Es un pueblo **ficticio**, nunca atado a una cultura real concreta; se trata con el mismo respeto que las ruinas del pueblo navegante ya reciben en el GDD v3 (§6, sin lore largo ni caricatura); el jugador nunca «gana» contra ellos ni hay misión de conquista; su reputación se gana respetando su tierra, no completando una lista de favores coloniales. Revisión de sensibilidad como parte del criterio de salida de la fase 3 (§6.2), no como nota a posteriori. |
| Descartar «sin fauna terrestre / sin esqueleto» (§3.6) puede reabrir el riesgo de producción que motivó la regla original | Originalidad/producción | El riesgo original era el coste de producir modelos y rig propios; usar packs CC0 ya terminados y probados (Quaternius) mantiene esa misma cautela con otra vía de producción. Se documenta como pivote razonado (§3.6). |
| El juego se parece a Valheim/Raft/Subnautica sin una identidad propia clara | Originalidad | Los rasgos originales se mantienen intactos y se refuerzan como columna vertebral: cartografía a mano (única entre las referencias), navegación polinesia aprendida en ruinas, museo de tesoros, barco «Limón» y dedicatoria a Almudena. Minería y pueblo se diseñan como extensión del pilar «transformar la isla», no como sistemas calcados de otro juego. |
| Trueque sin tabla de precios fija puede sentirse arbitrario o injusto para el jugador | Diseño/economía | El valor depende de la necesidad real del pueblo (visible: piden lo que les falta) y de la reputación, ambas legibles sin números — coherente con la regla anti-agobio de la biblia (§8.3, sin iconos ni barras). Ajuste fino en playtesting de la fase 3. |

---

**Fuentes reutilizadas:** `docs/superpowers/specs/2026-09-26-explored-design.md` (GDD
v3, base de este documento), `docs/design/biblia-de-contenido.md` (catálogo y
sistema de combinación, sin cambios), `docs/diseno/exploracion.md` (rama
`worktree-agent-acb73aafafe15225d`, contenido por isla), `docs/roadmap.md` (estado de
implementación), `docs/tecnico/guardado.md` (patrón de deltas reutilizado para
terreno), `docs/art/texturas.md` (precedente de CC0 ya en uso),
`Source/Explored/WorldGen/{TerrainDensity,SurfaceNets,TerrainChunkBuilder}.h`
(arquitectura real del terreno volumétrico).

**Descartado explícitamente:** la regla del GDD v3 de «todo el contenido visual hecho
por código, sin ningún asset externo» (§7.1) y la regla de «sin fauna terrestre, sin
animación por esqueleto» (§3.6, §3.7).
