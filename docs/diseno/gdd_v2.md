# EXPLORED — Documento de diseño del juego (GDD v2)

Versión 2.1 · 2026-09-27 · Unreal Engine 5.6 · Windows (objetivo: Steam)

> Revisión 2.1 (tarde del 2026-09-27): el acceso anticipado incluye **cooperativo de 2 a
> 4 jugadores** con servidor de escucha por Steam (§6.2), y la regla «sin multijugador»
> de §7.2 queda derogada. Diseño completo en
> `docs/diseno/biblia/08-cooperativo-y-red.md`.

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
| Arena viva | La arena se cava y se apila, se derrumba a su ángulo de reposo (más empinada si está mojada) y las olas borran hoyos y montones en la orilla; las estructuras la sujetan | `WorldGen` (§3.13, modelo puro hecho) |
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
- **Reglas y números del modelo [F2]** (modelo puro `FTramwayModel`, spec
  `Explored.Tramway`; integración en `docs/tecnico/railes-vagones.md`). El alcance
  (raíles y vagones en [F2]) lo decidió el director el 2026-09-27 (biblia 02 §9); el
  detalle del modelo (tramos sobre rejilla, pendiente máxima, inercia del vagón con su
  carga) está pendiente de validar. Los números marcados con *(biblia)* vienen de la biblia 02 §9;
  el resto es **propuesta pendiente de revisar**:
  - **Vía sobre rejilla.** Nodos cada **2 m** en horizontal *(biblia)* y cada
    **12,5 cm** en vertical. Un tramo une dos nodos vecinos en una de las 4 direcciones
    y sube o baja de 0 a 5 escalones: **pendiente máxima 17,4°** (5 × 12,5 cm en 2 m).
    La pieza sale sola de la forma del nodo: dos tramos alineados son vía recta
    (`rail_recto`); dos perpendiculares, una curva de 90° (`rail_curvo`, radio 1 m); tres
    o cuatro, un cambio de agujas (`cambio_agujas`).
  - **Cambio de agujas.** La palanca apunta a una salida. El vagón nunca da media
    vuelta: si la palanca apunta por donde viene, sigue recto, y si no hay recta, toma la
    salida de menor índice (+X, +Y, −X, −Y).
  - **Vagón.** 60 kg vacío y **200 kg de carga** como máximo *(biblia)*. La carga no se
    reparte: pasarse de 200 kg se rechaza. Toda la dinámica se hace sobre la masa total
    (60 + carga). Rodadura μ = 0,02 y freno de zapata μ = 0,3.
  - **Empujar a mano.** Hasta **1,2 m/s** *(biblia)* con una fuerza sostenida de
    280 N. Con carga, el vagón tiene inercia: lleno tarda **1,1 s** en llegar a 1 m/s y
    vacío, 0,23 s. Suelto a 1,2 m/s, rueda 3,7 m hasta pararse, esté lleno o vacío; con
    el freno echado se para en 25 cm. **Lleno solo se sube a mano hasta 5°** *(biblia)*:
    sí sube una rampa de 1 escalón (3,6°) pero no una de 2 (7,1°), y ahí se queda quieto
    (no repta ni rueda hacia atrás). Vacío se sube a mano cualquier pendiente de vía.
  - **Torno de cuerda.** Tira a **2 m/s** *(biblia)* con hasta 900 N. Así sube el vagón
    lleno por la pendiente máxima, que necesita 811 N. La cuerda mide **60 m** medidos
    por la vía. El torno tira hacia sí por el camino más corto, y a 0,5 m el trinquete
    sujeta el vagón, aunque esté en cuesta. Si no hay torno al alcance, no pasa nada y
    la interfaz lo dice. Por encima de 5°, el torno es obligatorio *(biblia)*; que
    el vagón vacío sí se pueda empujar más arriba es interpretación de este modelo
    (propuesta).
  - **Curvas y topes: donde el error es parte de la diversión.** Un vagón vuelca en
    curva a partir de **2,56 m/s vacío** y **2,05 m/s lleno** (el centro de masas sube
    de 0,45 a 0,7 m con la carga). Empujado o con el torno nunca vuelca, pero dejado
    rodar cuesta abajo sí: lleno por 3 tramos a 17° llega a unos 5,9 m/s y vuelca en la
    primera curva. Al final de la vía hay un tope. Por debajo de 2,5 m/s el vagón se para
    en él; por encima, vuelca.
  - **Terreno editado bajo la vía** *(biblia)*. Picar, cavar o echar tierra a menos
    del radio del pincel de un tramo lo marca **dañado**. Un vagón que entra en un tramo
    dañado (o que está encima de uno que desaparece) descarrila y se queda quieto hasta
    que se repara el tramo y se vuelve a poner en la vía.
  - **Guardado.** La vía, los daños, las palancas y los tornos van en una capa opaca
    `"tramway"` de la sección `world`, y cada vagón con su tramo, posición, velocidad,
    carga y estado. La simulación usa pasos fijos de 1/120 s: el resultado es el mismo
    bit a bit con cualquier tasa de fotogramas (lo comprueba el spec).
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
  Pendiente de siempre: malla del «Limón». El astillero de balsas está en §3.17.
- **Dependencias:** `Boats`, `Ruins`, `Villages`.
- **Construcción:** los barcos se arman pieza a pieza y la física decide si
  navegan (§3.14).

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
vendrán la arena viva (§3.13), el astillero de balsas (§3.17) y otras interacciones
naturales: el incendio (§3.15), la lluvia en recipientes (§3.16) y los cocos que caen
al sacudir (§3.18).

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

### 3.13 Mundo interactivo: arena viva **[director, 2026-09-27]**

Segunda mecánica del principio «el mundo entero es interactivo y se comporta de forma
natural». La playa deja de ser un decorado: se cava, se apila y reacciona. **Manda la
biblia:** las reglas son las de `biblia/02-mecanicas-del-mundo.md` §5 y los presupuestos,
los de `biblia/08-cooperativo-y-red.md` §2.6 (la capa de alturas propia y la rampa del
oleaje están anotadas allí como decisiones del director). Esta sección solo añade el
tamaño de la rejilla, la capa de arena y las pasadas por revisión.

- **Objetivo:** que cavar en la playa se sienta como en una playa de verdad (el hoyo
  se desmorona, el montón se escurre, la marea lo borra con el paso de los días) sin
  pagar el coste de la edición volumétrica de §3.4.
- **Reglas:**
  - **Solo la capa de superficie.** La arena es un campo de alturas de deltas sobre el
    suelo de la isla: una columna cada 0,25 m, chunks de 8 m (la misma rejilla que la
    edición volumétrica). Bajo la arena hay roca: como mucho se cava **1,5 m**, y un
    montón no pasa de **2 m** sobre el suelo original. Túneles y cuevas siguen siendo
    cosa del pico (§3.4).
  - **Pala.** Cada pasada es un cono de 0,6 m de radio y 15 cm en el centro (unos
    0,06 m³). Lo cavado va al cubo como arena, que tiene masa exacta. Al apilar se echa
    lo que se lleva y nada más.
  - **La arena no desaparece.** La avalancha es un traspaso entre columnas vecinas. El
    oleaje cambia arena con el **banco del mar** (la arena en suspensión de la resaca):
    lo que alisa de un montón va al banco y lo que rellena un hoyo sale de él. La suma
    «arena de la playa + banco del mar» solo cambia con lo que la pala saca o echa.
  - **Ángulo de reposo (biblia 02 §5.1).** **Una revisión por segundo** por chunk
    activo. Si el desnivel entre dos columnas pasa del reposo, la arena resbala: **34°**
    seca (168 mm por celda) y **45°** húmeda (249 mm). Está húmeda toda la arena a la
    altura de la pleamar del día o por debajo, y toda la arena mientras llueve. Al
    secarse (baja la pleamar con la luna, deja de llover), lo que estaba a 45° se vuelve
    a derrumbar hasta 34°. Cada revisión hace hasta 4 pasadas de ¼ del exceso. Una
    palada se asienta en 1–2 s, un montón de 1 m³ en unos pocos segundos y uno de
    2,5 m³ en unos 25 s, porque lo frena el tope de red: se ve escurrir.
  - **Lo natural no se derrumba solo.** El umbral nunca es menor que la pendiente del
    suelo original: una duna generada a 50° se queda como está. Solo se mueve la arena
    que ha tocado el jugador (o lo que esta arrastra).
  - **Relleno por oleaje (biblia 02 §5.2).** No es continuo: se resuelve **una vez por
    medio ciclo de marea** (~6 h de juego), en toda la isla. Cada columna editada cuya
    altura original está bajo la pleamar vuelve **hacia su altura original** (los hoyos
    se rellenan y los montones se alisan):
    - **20 %** por medio ciclo en la línea de pleamar, creciendo en línea recta hacia el
      agua hasta el **60 %** en la línea de bajamar y por debajo;
    - en marea viva, **+15 puntos** en todas partes (**35 %** en la pleamar, 75 % en la
      bajamar);
    - un resto de **2 cm** o menos lo remata la última onda.
    Con estos números, un hoyo bajo la bajamar se cierra del todo en **2–3 ciclos**: uno
    de 30 cm en 1,5 ciclos y el más hondo posible (1,5 m) en 2,5. Entre medio ciclo y
    medio ciclo, un hoyo en la orilla se queda como está: la marea lo borra con los días,
    no con los segundos.
  - **Anclaje (biblia 02 §5.3).** El tablón de contención, los pilotes, los muelles y
    los sacos sujetan toda la arena a **1 m** o menos de su huella (distancia real, con
    las esquinas redondas). Esa arena no desliza y el oleaje no la rellena mientras la
    pieza siga en pie; la de fuera sí puede caer contra ella. La pala no cava bajo la
    huella. Al quitar la pieza, la arena que sujetaba se suelta y se derrumba a 34°.
    Dos piezas que se solapan sujetan hasta que se quitan las dos.
- **Números:** `FSandModel` en `Source/Explored/WorldGen/SandModel.h`.

  | Qué | Valor | Fuente |
  |---|---|---|
  | Rejilla / chunk | 0,25 m / 8 m (32 columnas) | este GDD |
  | Capa de arena / montón máximo | 1,5 m / 2 m | este GDD |
  | Reposo seco / húmedo | 34° / 45° | biblia 02 §5.1 |
  | Revisión de pendiente | 1 por segundo, 4 pasadas de ¼ del exceso | biblia 02 §5.1 / este GDD |
  | Relleno por medio ciclo, pleamar → bajamar | 20 % → 60 % | biblia 02 §5.2 |
  | Marea viva | +15 puntos (35 % en la pleamar) | biblia 02 §5.2 |
  | Remate del oleaje | ≤ 2 cm | biblia 02 §5.2 |
  | Arena sujeta por una estructura | ≤ 1 m de la huella | biblia 02 §5.3 |
  | Radio activo alrededor de cada jugador | 80 m hasta el borde del chunk | biblia 08 §2.6 |
  | Tope de columnas cambiadas | 64 por chunk y revisión | biblia 08 §2.6 |
  | Revisiones acumuladas al acercarse | 4 como máximo, de golpe; el resto se descarta | biblia 08 §2.6 |
  | Paquete de red | versión 2, capa 1, ≤ 512 B | biblia 08 §2.2 |

- **Progresión:** con la pala tosca desde el primer día (hoyos para cocinar bajo
  tierra, zanjas de drenaje, rampas de arena para botar balsas). El tablón de
  contención (tier `bambu`) y los sacos de arena, más adelante, sirven para muros,
  diques y muelles que la marea no borra.
- **Interfaz:** ninguna. La arena se ve escurrir y oscurecerse al mojarse.
- **Coste:** solo lo simula el servidor, y solo en los chunks a menos de 80 m de algún
  jugador. Dentro de ellos solo se revisan las columnas **sucias** (tocadas, o vecinas
  de algo que se ha movido). Fuera, la arena se congela con su estado y cuenta las
  revisiones que se salta; al volver alguien, las recupera de golpe (4 como máximo) y
  después sigue a su ritmo, sin recordar el resto del tiempo perdido. Un montón asentado cuesta cero. El medio ciclo de marea toca solo las
  columnas editadas, una vez cada 10 minutos reales.
- **Riesgos técnicos:**
  - Casar la malla de la arena con el terreno volumétrico: ver
    `docs/tecnico/arena-viva.md`.
  - Si la pleamar del día cambia mientras nadie está cerca, la arena de esa playa no
    se entera hasta que llegue alguien. Es invisible para el jugador, porque nadie lo ve
    pasar. El relleno por oleaje sí llega a toda la isla.
- **Dependencias:** `WorldGen` (`FSandModel`, `FTerrainDensity` como suelo base),
  `Ocean` (`FOceanTide`: pleamar, bajamar y marea viva), `Weather` (lluvia),
  `Building` (anclajes), `Save` (capa `sand`).

### 3.14 Construcción naval: barcos que hay que pensar **[alcance aprobado 2026-09-27 (biblia 02 §8); detalle pendiente de validar]**

Modelo puro `FHullAssemblyModel` (`Source/Explored/Boats/HullAssemblyModel.h`), spec
`Explored.HullAssembly`. Integración en `docs/tecnico/casco-por-piezas.md`.

- **Objetivo:** no existe «construir barco». El jugador arma un casco con piezas y el
  agua le dice si ha acertado. Una balsa mal equilibrada vuelca, y ese error es parte
  de la diversión: se aprende mirando cómo escora, no leyendo una barra.
- **Piezas.** Cada pieza es una caja con masa, volumen y posición en el marco del casco
  (X hacia proa, Y hacia estribor, Z hacia arriba). Tamaño por defecto, que se puede
  cambiar:

  | Pieza | Tamaño (cm) | Densidad efectiva | Masa | Qué aporta |
  |---|---|---|---|---|
  | Tronco | 300 × 22 × 22 (≈ Ø 25 cm) | 500 kg/m³ | 72,6 kg | Flotación pesada y estable |
  | Tablón | 200 × 25 × 4 | 550 kg/m³ | 11 kg | Cubierta, largueros |
  | Bambú (haz de cañas gruesas) | 300 × 10 × 10 | 300 kg/m³ (hueco) | 9 kg | Mucha flotación por kilo |
  | Flotador sellado (calabaza, barril) | 60 × 40 × 40 | 80 kg/m³ | 7,7 kg | Balancín, reserva de flotación |
  | Mástil | 10 × 10 × 400 | 550 kg/m³ | 22 kg | Habilita la vela; sube el centro de masas |
  | Vela | 2 × 2 m | 1,2 kg/m² | 4,8 kg | Empuje con viento (máx. 12 m² por mástil) |
  | Remos (par) | — | — | 6 kg | 70 N sostenidos por tripulante |
  | Pala (canalete) | — | — | 2,5 kg | 35 N sostenidos por tripulante |

  Carga y pasajeros son masas puntuales. Un pasajero pesa 75 kg y, de pie, tiene su
  centro de masas a 90 cm sobre la cubierta; sentado, a unos 50 cm.
- **Hidrostática (agua de mar, 1025 kg/m³).** El calado es el que desplaza el peso
  total (Arquímedes). El centro de carena es el centroide de lo sumergido. La **altura
  metacéntrica** GM = KB + BM − KG sale de la pendiente del brazo adrizante en 0°. El
  cálculo es exacto para cajas: recorta la sección de cada pieza con el plano del agua.
  El spec comprueba GM contra la fórmula de la barcaza (±1 %).
- **Veredicto, del mejor al peor:**

  | Veredicto | Cuándo |
  |---|---|
  | Flota nivelada | Escora y asiento ≤ 2° |
  | Escora | Flota, pero con escora o asiento > 2° |
  | Anegada | Francobordo < 2 cm en algún canto de la cubierta: cualquier ola entra; va a la mitad de velocidad |
  | Vuelca | No hay equilibrio estable por debajo de 55° de escora (biblia 02 §8.2), o GM < 0 sin un ángulo de apoyo antes de 55° |
  | Se hunde | Masa total > flotación máxima (todo el volumen sumergido) |

- **Números de referencia** (calculados con el modelo):

  | Montaje | Masa total | Calado | Francobordo | GM | Resultado |
  |---|---|---|---|---|---|
  | 6 troncos + 1 pasajero de pie + remos | 519 kg | 12,8 cm | 9,2 cm | 94 cm | Flota nivelada; 1,0 m/s remando |
  | La misma + un segundo pasajero a 40 cm del eje | 592 kg | 14,6 cm | 2,7 cm | 70 cm | Escora 4,1° |
  | La misma + 60 kg de carga en la borda (66 cm) | 571 kg | 14,1 cm | 2,4 cm | 83 cm | Escora 4,8° |
  | 6 troncos + 1 pasajero + 300 kg de carga | 811 kg | 20,0 cm | 2,0 cm | 51 cm | Casi anegada; con 350 kg ya anegada; con 400 kg se hunde (flotación máxima: 893 kg) |
  | 2 troncos + pasajero de pie | 223 kg | 16,5 cm | — | −27 cm | **Vuelca** (KG 45 cm > KM 18 cm) |
  | 3 troncos + pasajero de pie | 295 kg | 14,6 cm | — | −4,5 cm | **Vuelca** por poco |
  | 4 troncos + pasajero de pie | 368 kg | 13,6 cm | 8,4 cm | 23 cm | Flota; el mínimo seguro |
  | 2 troncos + balancín (travesaño de bambú y 2 flotadores a 1,5 m) | 238 kg | 13,8 cm | 18,2 cm | 322 cm | Flota muy estable |
  | 4 haces de bambú + pasajero de pie | 114 kg | 9,2 cm | — | −49 cm | **Vuelca**; con 8 haces flota y va a 1,5 m/s a pala |
  | 3 troncos de 6 m + pasajero + remos | 517 kg | 12,7 cm | 9,3 cm | 9 cm | Flota; 1,9 m/s remando (el doble que la balsa de 6 × 3 m) |

  La lección que el juego enseña sin texto: **más ancho es más estable, más alto es
  menos estable, y un balancín compra estabilidad casi gratis**. Sentarse (bajar el
  centro de masas 40 cm) salva una balsa estrecha que de pie vuelca.
- **Propulsión y forma.**
  - Cada tripulante coge unos remos si quedan libres y, si no, una pala.
  - La vela necesita un mástil y un tripulante que lleve la escota. Empuja
    ½ · ρ_aire · 0,8 · A · V², así que con 4 m² y el alisio de 6,5 m/s da 83 N,
    algo más que los remos.
  - Con viento flojo se rema. El modelo elige lo que más empuja.
  - Resistencia de forma: Cd = 0,3 + 1,2 / (eslora / manga efectiva). La manga
    efectiva suma solo lo que está bajo el agua, así que un balancín no frena como un
    casco ancho.
  - Velocidad de casco: 0,4 · √(g · eslora), unos 2,2 m/s para 3 m. Pasar de ella
    rinde una cuarta parte del empuje de más.
  - Escorada, la embarcación pierde velocidad (× cos escora).
  - Giro con radio de 1,5 esloras. Mantener el rumbo depende de lo esbelto que sea el
    casco (0 si es cuadrado, 1 a partir de 8:1). Un casco chato gira y deriva; uno
    esbelto corre y va recto.
- **La carga cuenta.** La misma balsa con 150 kg más cala 3,7 cm más, pierde GM y va más
  lenta. Una carga descentrada escora hacia su lado (tan φ ≈ w · e / (Δ · GM), lo
  comprueba el spec), y hacia proa la asienta de proa.
- **Al agua.** `ToBoatDefinition` traduce lo armado a la ficha con la que navega
  `FBoatModel`: eslora, manga, altura de cubierta, masa, coeficiente de flotación, GM,
  vuelco en el ángulo de estabilidad nula (tope 55°), vela y su altura sobre la
  flotación, empuje y velocidad de remo, y carga máxima (95 % de la flotación con un
  tripulante, biblia 02 §8.2). Las olas, el viento aparente, la escora dinámica por la
  vela y el vuelco en marcha los sigue resolviendo `FBoatModel`, que ya tiene specs.
- **Progresión:** balsa de troncos ancha (pesada, lenta, segura) → balsa de bambú
  (ligera y rápida, pero hay que ensancharla o sentarse) → casco estrecho con balancín
  (rápido y estable) → vela en mástil. Los planos canónicos de `boats.json` pasan a
  ser montajes de ejemplo sobre estas mismas piezas.
- **Interfaz:** sin números en pantalla. En el astillero, la pieza fantasma muestra el
  casco inclinándose hacia donde quedaría escorado y hundiéndose hasta su calado. Al
  botarlo, la física hace el resto.
- **Riesgos técnicos:** escora y asiento se resuelven desacoplados (estabilidad
  estática clásica). Una carga en diagonal da una escora y un asiento correctos por
  separado, pero no su combinación exacta. Es suficiente para el astillero, y la
  dinámica en el mar la lleva `FBoatModel`.
- **Pendiente de decisión:** el objeto `tronco_pequeno` pesa 8 kg en la biblia 03, y con
  8 troncos así una balsa no aguanta a una persona. La pieza «tronco» del casco es un
  tronco de balsa de verdad (72,6 kg: se lleva a hombros entre dos o se hace rodar).
  Hay que decidir si es un objeto nuevo (`tronco_balsa`) o si se revisa el peso.
- **Dependencias:** `Boats` (`FBoatModel`), `Building` (astillero), `Save` (montaje
  por piezas en la sección de barcos, pendiente).

### 3.15 Mundo interactivo: incendio de vegetación **[números de la biblia 02 §6; duraciones de quema pendientes de validar]**

Cuarta mecánica del principio del mundo interactivo. Los números de
contagio, rebrote y ceniza son los de la biblia 02 §6, que manda en el detalle. Las
duraciones de quema son una propuesta y no las ha validado nadie.

- **Objetivo:** que un fuego mal vigilado en la seca pueda quemar una ladera de hierba, y
  que el jugador lo pueda frenar con agua, arena o un cortafuegos.
- **Rejilla:** celdas de **2 m** (la misma medida que la rejilla de construcción) en
  chunks de **16 × 16 celdas (32 m)**. Arden la hierba y el matorral; los árboles, la
  arena, la roca, el agua y el suelo desnudo no arden. Solo se guardan las celdas que
  se apartan del mundo base: ardiendo, quemadas o mojadas.
- **Reglas:**
  - **Contagio.** Cada segundo, una celda que arde tira por cada una de sus **8
    vecinas** no mojadas con combustible:

    | Condición | Probabilidad por vecina y segundo |
    |---|---|
    | Seca (también con ola de calor o llovizna) | **45 %** |
    | Primeras lluvias, monzón o ciclones | **13,5 %** (−70 %) |
    | Niebla matinal | **9 %** |
    | Chubasco, tormenta o ciclón activo | 0: el incendio se apaga entero |

  - **Viento.** Con viento de fuerza ≥ 0,1, a la probabilidad se suman **25 puntos**
    multiplicados por el coseno entre la vecina y la dirección del viento: +25 a favor,
    0 de través y −25 en contra, sin bajar de 0. En la estación húmeda, un fuego con
    viento no avanza nunca hacia barlovento (13,5 − 25 < 0).
    - *Interpretación:* la biblia dice «+25 %», que aquí se lee como puntos
      porcentuales. Aplicado como factor (×1,25), el viento apenas se notaría en la
      estación húmeda.
  - **Duración de la quema [propuesta]:** la hierba arde **20 s** y el matorral **60
    s**. Después, la celda queda quemada. En seca y sin viento, una mancha de hierba
    de 40 × 40 m que prende en el centro se apaga en menos de un minuto (35 s con la
    semilla del spec).
  - **Apagar:**
    - Un chubasco o más apaga todo el incendio, incluidas las celdas lejanas.
    - Echar agua o arena en un disco de hasta 8 celdas de radio apaga lo que arde en
      él, que queda quemado.
    - Las celdas con combustible del disco quedan **mojadas**: no prenden mientras
      dura la humedad, que la fija quien la causa (cubo, lluvia local).
    - Una línea mojada de una sola celda de ancho ya funciona como cortafuegos.
  - **Rebrote:** la hierba quemada vuelve a los **12 días** y el matorral a los **25
    días**. Hasta entonces no tiene combustible y no se puede volver a quemar.
  - **Ceniza:** cada celda quemada da **1 `ceniza_madera`** durante los **3 primeros
    días**. Se recoge una sola vez.
- **Contradicción abierta:** la biblia 02 §6 dice que los 25 días del matorral son «el
  mismo número que el rebrote de tala de arbustos (1.2)», pero la tala implementada
  (§3.12, `FFellingModel`) rebrota el arbusto en 3 + 2 días. Queda pendiente de que
  decida el director. Mientras tanto, el incendio usa los 25 días de la biblia.
- **Red y coste (misma política que la arena viva, biblia 08 §2.6):**
  - Solo simula el servidor.
  - Solo se revisan los chunks con fuego que estén a **menos de 80 m** de algún
    jugador, medidos hasta el borde del chunk. Fuera de ese radio, el fuego se congela.
  - Como mucho se acumulan **4 pasos** y el resto se descarta.
  - Como mucho prenden **64 celdas por chunk y segundo**; el resto espera.
  - El coste de un paso es proporcional al número de celdas que arden cerca de un
    jugador, nunca al tamaño del mundo.
- **Determinismo:** cada tirada es un hash de (semilla, celda destino, segundo,
  vecina de origen), y las celdas que prenden no contagian hasta el segundo siguiente.
  El resultado no depende del orden de visita ni de cómo se trocee el avance, y una
  partida guardada a mitad de incendio sigue exactamente igual.
- **Interfaz:** humo que avisa desde lejos, crepitar, suelo ennegrecido y ceniza gris
  que se puede recoger. Sin barras ni avisos de texto.
- **Dependencias:** `WorldGen` (`FWildfireModel`), `Weather` (`ESeason`,
  `EWeatherState`, viento), `Cooking` (una hoguera o una antorcha sin vigilar es la
  chispa), `Save` (sección `wildfire`). Ver `docs/tecnico/incendio-integracion.md`.

### 3.16 Mundo interactivo: la lluvia llena los recipientes **[biblia 02 §5.4; números pendientes de validar]**

La biblia 02 §5.4 ya lo pide: «cualquier recipiente abierto se llena con la lluvia
activa», y la biblia 01 §6.2 cuenta la «lluvia recogida» como agua para beber sin riesgo.
Esta sección fija cuánto se llena, cuándo se vacía y qué pasa si se mezcla.

- **Objetivo:** que dejar un cuenco a la intemperie antes de un chubasco sea una
  decisión natural de supervivencia, sin menús ni temporizadores.
- **Reglas:**
  - **Cuánto entra.** Recoge lo que cae sobre la boca: 1 mm de lluvia sobre 1 m² son
    1 L. La intensidad sale de `FWeatherSample::Rain`:

    | Tiempo (`Rain`) | mm/h |
    |---|---|
    | Llovizna (0,3) | 2,5 |
    | Galerna (0,35) | 3,3 |
    | Chubasco (0,75) | 10 |
    | Tormenta (0,95) | 30 |
    | Ciclón (1,0) | 50 |

    Entre los puntos se interpola en línea recta. Por debajo de 0,02 no llueve.
  - **A cubierto no recoge.** Un recipiente bajo techo no se llena, y a la sombra se
    evapora al 30 %.
  - **Evaporación.** Sin lluvia, una lámina de agua pierde 0,25 mm/h con cielo
    despejado (unos 5,5 mm al día) y un 60 % menos con el cielo cubierto.
  - **Rebose.** Lo que no cabe se sale. La capacidad es la del inventario
    (`Recipiente` × 0,25 L), así que un cuenco lleno en el suelo pesa lo mismo al
    cogerlo.
  - **Mezcla.** Si dentro había agua de mar o sin tratar, la lluvia se mezcla, y al
    rebosar sale mezcla. Una vasija llena de agua de mar que se deja en una tormenta
    deja de ser salobre a las 4,4 h y queda con agua de lluvia limpia a las 12,3 h.
    - **Salobre** con un 3 % o más de agua de mar (~1 g/L de sal). Cuenta como
      `agua_mar`.
    - **Sin tratar** con cualquier rastro de agua ajena por debajo de eso. Cuenta como
      `agua_sin_tratar` (se hierve).
    - **Lluvia** solo si no queda nada ajeno. Se bebe sin riesgo.
    - El agua de mar manda sobre la sin tratar al mezclarse.
  - **Qué recipientes recogen.** Solo los abiertos del catálogo, con su boca:

    | Objeto | Boca (m²) | Capacidad (L) | Horas de chubasco para llenarse |
    |---|---|---|---|
    | `concha_grande` | 0,050 | 0,5 | 1 |
    | `cascara_coco`, `recipiente_coco` | 0,018 | 0,5 | 2,8 |
    | `vasija_barro` | 0,020 | 0,75 | 3,75 |
    | `bambu_grueso` | 0,003 | 0,5 | 16,7 |
    | `concha_pequena` | 0,004 | 0,25 | 6,3 |
    | `bambu_fino`, `caracola` | 0,001 | 0,25 | 25 |
    | `cantimplora` (boca estrecha) | 0,0007 | 1,0 | 143 |

    El coco verde, la cesta, las mochilas y la bolsa estanca no recogen.
- **Progresión:** el primer día se bebe coco. Con la primera cáscara raspada ya se puede
  poner a recoger lluvia, y en `primeras_lluvias` y `monzon` unas cuantas conchas y
  vasijas dan agua limpia sin hervir. La cantimplora se llena mejor en el río o
  vertiendo desde un cuenco.
- **Interfaz:** el nivel del agua se ve dentro del recipiente, con salpicaduras
  mientras llueve. Al mirarlo, la etiqueta dice «agua de lluvia», «agua sin tratar» o
  «agua salobre».
- **Pendiente de decidir:**
  - En `items.json` no hay un objeto `agua_lluvia`. La lluvia limpia se bebe
    directamente del recipiente; si hace falta como ingrediente, habría que crearlo.
  - Un colector de hojas o de lona que amplíe la boca de una vasija (más m² para el
    mismo recipiente) encaja en el modelo, pero no existe como pieza de construcción.
  - Los números de esta sección no los ha validado el director.
- **Riesgos técnicos:** ver `docs/tecnico/lluvia-recipientes.md`. Ponerse al día al
  cargar recorre como mucho 60 días de juego.
- **Dependencias:** `Weather` (`FRainCatchModel`, `FWeatherModel`), `Carry`
  (`LiquidCapacityFromRecipiente`), `Save` (capa de recipientes del mundo).

### 3.17 Mundo interactivo: astillero de balsas **[mecánicas pedidas por el director 2026-09-27; números pendientes de validar]**

Tercera mecánica del principio «el mundo entero es interactivo». Modelo puro
`FRaftYardModel` (`Source/Explored/Boats/RaftYardModel.h`), spec `Explored.RaftYard`.
Usa el casco por piezas de §3.14 (`FHullAssemblyModel`) para la forma y la flotación, y
`FBoatModel` para navegar, sin duplicar ninguno de los dos. Integración en
`docs/tecnico/astillero-balsas.md`.

- **Objetivo:** que botar una balsa sea un pequeño problema físico, no un botón. Dónde
  se construye importa, cómo se lleva al agua importa y cómo se ata importa.
- **Dónde se construye:**
  - **En tierra** la balsa es estable: no le afectan las olas ni la marea. Para botarla
    hay que empujarla hasta que el agua la levante.
  - **En el agua** flota desde la primera pieza: deriva con la corriente y el viento y
    cabecea con el oleaje (`FBoatModel`), salvo que se **amarre** a un poste o a un
    muelle. Amarrada sigue cabeceando, pero el cabo no la deja alejarse más que su
    largo; se puede soltar en cualquier momento. El amarre se guarda con el barco.
- **Uniones.** Cada pareja de piezas que se tocan (hueco ≤ 5 cm) se une con uno de
  estos tres tipos:

  | Unión | Objeto | Aguante al roce | Aguante a los golpes | Reparación |
  |---|---|---|---|---|
  | Cordel de fibra | 1 `cordel` | 0,6 | 0,7 | +50 % por 1 `cordel` |
  | Cuerda | 1 `cuerda` | 1,0 | 1,2 | +50 % por 1 `cuerda` |
  | Clavos | 2 `clavo` | 2,0 | 0,7 | +100 % por 2 `clavo` |

  La cuerda cede y vuelve, así que aguanta mejor los golpes. Los clavos resisten el
  roce, pero un golpe seco raja la madera a su alrededor.
  - **Una unión rota suelta la pieza** si ya no queda otra que la sujete. Se queda el
    grupo de más masa y lo demás sale flotando (o cae, si está en tierra). Si hay otras
    uniones que sujetan, la rota no suelta nada y se puede reparar allí mismo.
  - Una pieza que nadie ató se va flotando al botar la balsa.
  - No se pierde nada: casco más piezas sueltas suman siempre las mismas piezas y la
    misma masa (lo comprueba el spec).
- **Botadura desde tierra.** La balsa se mueve a lo largo de un camino hacia el agua.
  Cada tramo del camino tiene un suelo y una pendiente.
  - **Rozamiento de Coulomb:** para arrancarla hace falta vencer μ · peso sobre el
    suelo. La pendiente ayuda cuesta abajo, y el agua sostiene una parte del peso
    según la profundidad frente al calado. Cuando el agua cubre el calado, flota y
    pasa a `FBoatModel` con la arrancada que llevaba.
  - **Rodillos.** Son troncos atravesados en el camino. Con al menos uno bajo cada
    mitad del casco, la balsa rueda: resistencia de rodadura de 0,05 y ningún desgaste.
    Los rodillos de debajo avanzan la mitad que la balsa, así que se quedan atrás y hay
    que recogerlos y volver a ponerlos delante. Es el trabajo de la botadura, y en
    cooperativo lo hace uno mientras los demás empujan.
  - **Rampa de tablones:** poco roce (0,30). A partir de unos 17° la balsa baja sola.
  - **Arrastrarla sin rodillos gasta** las uniones de las piezas que tocan el suelo.
    El desgaste es de Archard: carga × distancia, repartido entre esas uniones.
  - Una persona empuja 300 N sostenidos.

  | Suelo | μ | Desgaste (salud / kN·m) | 6 troncos (451 kg): fuerza para arrancar | Personas |
  |---|---|---|---|---|
  | Arena seca | 0,55 | 0,12 | 2,43 kN | 9 |
  | Arena mojada | 0,45 | 0,08 | 1,99 kN | 7 |
  | Hierba | 0,40 | 0,05 | 1,77 kN | 6 |
  | Roca | 0,50 | 0,40 | 2,21 kN | 8 |
  | Rampa de tablones (llana) | 0,30 | 0,01 | 1,33 kN | 5 |
  | Rampa de tablones a 8° | 0,30 | 0,01 | 0,70 kN | 3 |
  | Rodillos | 0,05 | 0 | 0,22 kN | **1** |

  Arrastrar 10 m la balsa de 6 troncos (17 uniones con el fondo):

  | Uniones | Por arena | Por roca |
  |---|---|---|
  | Cordel de fibra | −52 % | se rompen a los 5,8 m |
  | Cuerda | −31 % | se rompen a los 9,6 m |
  | Clavos | −16 % | −52 % |

- **En el agua.**
  - **Golpes:** cuando `FBoatModel` encalla o choca por encima de su velocidad segura
    (0,8 m/s), el astillero reparte el golpe entre las uniones cercanas al punto de
    impacto. Resta 0,25 por cada m/s de más en la unión más cercana, y el efecto baja
    linealmente hasta cero a media eslora. Un golpe de proa a 3 m/s quita un 46 % a la
    cuerda más cercana y un 79 % a los clavos o al cordel.
  - **Roce:** varada y arrastrándose sobre un bajío, `FBoatModel` acumula carga ×
    distancia. Ese trabajo gasta las uniones del fondo con el mismo desgaste que en
    tierra, según el suelo que haya debajo.
  - El daño de `FBoatModel` es 1 − la salud media de las uniones. Al soltarse una
    pieza, el barco toma la ficha nueva sin perder la posición ni el rumbo.
- **Progresión:** al principio, balsa atada con cordel y botada desde la arena mojada o
  sobre rodillos. Con cuerda, balsas que aguantan los arrecifes. Con clavos y rampa
  de tablones, un astillero de verdad en la playa (Arenas Blancas, §4).
- **Interfaz:** sin barras.
  - Las ataduras gastadas se ven deshilachadas y crujen al golpear.
  - La balsa en tierra no se mueve hasta que empujan bastantes, y los rodillos giran y
    se quedan atrás.
  - Al romperse una unión, la pieza se separa con un chapoteo.
- **Riesgos técnicos:**
  - El camino de botadura es una línea recta de tramos, no la física de Chaos. Para
    varar y botar por el terreno real, el motor genera el camino con una traza hacia
    el agua.
  - Las uniones no modelan el esfuerzo interno de la estructura (una balsa con el
    mástil atado a un solo tablón no se tuerce): solo hay roce, golpe y daño directo.
- **Pendiente de decisión: contradicciones con la biblia 02 §8.** El encargo del
  director del 2026-09-27 pide que una unión rota suelte la pieza y que arrastrar sin
  rodillos dañe las uniones. La biblia, que manda en el detalle, dice otra cosa en varios
  puntos. Hasta que el director elija, el modelo sigue el encargo:

  | Punto | Biblia 02 §8 | Este modelo | Opciones |
  |---|---|---|---|
  | Unión rota (§8.3) | Abre una vía de agua de 0,5 L/s por brecha | Suelta la pieza | Las dos a la vez (unión casco–casco abre una vía; pieza de cubierta o balancín, se suelta), o una de ellas |
  | Salud de la unión (§7, §8.3) | `integrity` 1–100 de `FBuildingModel`, sin sistema aparte | `FRaftJoint::Health01` propio | Guardar la salud en la `integrity` de la pieza de construcción (×100) y que este modelo solo calcule el daño |
  | Botadura (§8.4) | Canal de esfuerzo de 8 s por tonelada | Rozamiento de Coulomb: 9 personas en arena seca, 1 sobre rodillos | Mantener los rodillos obligatorios en arena (el cooperativo es de 2 a 4) o escalar el empuje para cuadrar con 8 s/t |
  | Anegarse y hundirse (§8.2) | Por encima del 95 % de flotabilidad embarca agua (`SwampWaterKg` sube 2 kg/s); por encima del 115 %, se hunde | Francobordo < 2 cm y > 100 % (§3.14) | Adoptar los umbrales de la biblia en `FHullAssemblyModel` |
  | Balancín (§8.2) | Reduce un 60 % el momento de escora en el lado del flotador | Sin regla fija: el flotador sube la GM por hidrostática (322 cm en el ejemplo de §3.14) | Mantener la hidrostática o aplicar el 60 % de la biblia |
  | Piezas (§8.1) | Quilla, cuaderna, tablón, cubierta, mástil, vela, balancín, timón, banco de remo, noray | Tronco, tablón, bambú, flotador, mástil, vela, remos, pala (§3.14) | Añadir las piezas que faltan o revisar la lista de la biblia |
  | Peso del tronco (biblia 03) | `tronco_pequeno`: 8 kg; balsa: 8 troncos + 6 `liana` | Tronco de balsa: 72,6 kg | Nuevo objeto `tronco_balsa` o revisar el peso |
  | Unión con clavos | `clavo` no existe en `items.json` | Tipo `Nails` | Crear el objeto o quitar ese tipo |

- **Red (biblia 08; «todo sistema nuevo nace con la autoridad en el servidor»).**
  Simula el servidor. Lo que se replica y cuánto cuesta está en
  `docs/tecnico/astillero-balsas.md` §Red.
- **Dependencias:** `Boats` (`FHullAssemblyModel`, `FBoatModel`), `Building`
  (astillero y postes de amarre), `WorldGen` (troncos de la tala para los rodillos),
  `Save` (uniones y rodillos en la sección de barcos).

### 3.18 Mundo interactivo: los cocos caen al sacudir **[biblia 02 §1.2 y §13.1; números pendientes de validar]**

Tercera de las «otras interacciones naturales» del principio del director (§3.12). La
biblia ya fija que la tala suelta el coco maduro y que trepando se coge el verde
(biblia 02 §1.2 y §13.1). Esta sección añade lo que faltaba: que la palmera tenga
cocos de verdad en la copa, que maduren y caigan solos, y que sacudirla los suelte.

- **Objetivo:** que el primer alimento del día 1 (biblia 01, «un coco al alcance»)
  salga de mirar la palmera y actuar, sin menús: debajo hay cocos caídos, arriba se ven
  verdes y maduros, y sacudir el tronco trae los maduros.
- **Reglas:**
  - **La copa tiene huecos.** Cada palmera adulta tiene 6. En cada hueco cuaja un coco
    verde a los 2–4 días de quedarse vacío. Pasa 5 días verde, cuelga maduro 3–8 días
    y cae solo. En el suelo aguanta 6 días y se pudre; entonces el hueco vuelve a
    cuajar. De media, una palmera sin tocar tiene ~2,2 verdes, ~2,5 maduros y ~2,6
    cocos en el suelo, y suelta ~0,44 cocos al día.
  - **Sacudir.** Verbo del tronco sin herramienta en la mano (el golpe con herramienta
    es talar y E mantenido es trepar, biblia 02 §13.1). Cada maduro cae con
    probabilidad `fuerza × (0,35 + 0,65 × flojera)`. La flojera va de 0 al madurar a 1
    justo antes de caer solo, así que sacudir sirve sobre todo para los que ya iban a
    caer. **El verde no cae nunca**, ni sacudiendo ni con viento: para él hay que trepar.
  - **Fuerza a mano.** 1 en palmeras de hasta 4,5 m; después, `4,5 / altura`, con un
    mínimo de 0,15. Una palmera adulta de 9 m se sacude con fuerza 0,5: suelta ~1/3 de
    sus maduros por sacudida.
  - **Dónde caen.** En un anillo de 0,5 m a 0,6 × copa (1,8 m en la palmera) alrededor
    del tronco, repartidos por área. Pueden caer en la celda de vegetación vecina, pero
    siguen siendo de su palmera.
  - **En la cabeza.** Un coco que cae a menos de 35 cm de quien sacude le da. Pegado al
    tronco pasa en ~3 % de los cocos. El daño lo fija la biblia 01 (propuesta: 5 de
    salud, sin esguince).
  - **Rachas.** Con viento por encima de 0,6, cada hora de juego sacude la copa con
    fuerza `0,5 × (viento − 0,6) / 0,4` (0,5 en un ciclón). Solo en palmeras cercanas al
    jugador: es un efecto que se ve.
  - **Trepar.** Arriba se coge un coco de la copa, verde o maduro, y el hueco queda vacío.
  - **Talar.** Lo que queda en la copa cae con ella, repartido por la copa caída. Cada
    maduro se abre con probabilidad 0,3 y queda en `cascara_coco`; los verdes aguantan.
    La copa se vacía, así que **sacudir y luego talar no da cocos de más**. Estos cocos
    sustituyen a los `coco_*` de la tabla de la palmera en §3.12.
  - **Rebrote.** Cuando el tocón vuelve a ser adulto (§3.12), la copa empieza vacía y
    cuaja su primer coco a los 2–4 días.

  | Número | Valor | Dónde |
  |---|---|---|
  | Huecos por palmera | 6 | `FCoconutPalmProfile::Slots` |
  | Cuajar tras vaciarse | 2–4 días | `RefillMinDays`/`RefillMaxDays` |
  | Verde | 5 días | `GreenDays` |
  | Maduro colgando | 3–8 días | `HangMinDays`/`HangMaxDays` |
  | En el suelo hasta pudrirse | 6 días | `GroundLifeDays` |
  | Flojera mínima de un maduro | 0,35 | `BaseLooseness` |
  | Fuerza a mano | 1 hasta 4,5 m, luego 4,5 / altura (mín. 0,15) | `HandShakeStrength` |
  | Racha | desde viento 0,6; máx. 0,5 | `GustStrength` |
  | Anillo de caída | 0,5 m – 0,6 × copa | `MinFallRadiusMeters`, `FallRadiusCrownFraction` |
  | Radio de la cabeza | 0,35 m | `HeadHitRadiusMeters` |
  | Maduros que se abren al talar | 30 % | `CrackChanceOnFell` |

- **Determinismo.** Todo el ciclo sale de un hash de (semilla, hueco, generación) y va en
  minutos enteros: avanzar 30 días de golpe o minuto a minuto da lo mismo (hay spec). Una
  palmera que nadie ha tocado no guarda nada.
- **Interfaz:** sin barra ni contador. Los cocos se ven en la copa (verdes o pardos); la
  copa se agita al sacudir y suena el golpe sordo de cada coco al caer.
- **Dependencias:** `WorldGen` (`FCoconutPalmModel`, `FFellingModel`), `Weather`
  (viento), `Survival` (daño en la cabeza), `Save` (sección `coconuts`). Integración en
  `docs/tecnico/cocos-palmeras.md`.

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
- **Cooperativo de 2 a 4 jugadores [director, 2026-09-27]**, con **servidor de escucha**
  (uno de los jugadores hospeda) a través de Steam (Online Subsystem Steam + Steam
  Sockets): servidor autoritativo, invitación y entrada en caliente desde la
  superposición de Steam, guardado del mundo en la partida del anfitrión y personaje e
  inventario propios de cada invitado. Diseño completo, con números y plan de migración,
  en `docs/diseno/biblia/08-cooperativo-y-red.md`. Esta decisión **deroga** la regla «sin
  multijugador» de §7.2. Estado de partida, sin maquillar: hoy no hay ninguna replicación
  en `Source/`, y el coste estimado es de **69 días de agente** repartidos de H0 a H5
  (biblia 08 §9.3), con los cimientos en H0. Se decide ahora, y no más tarde, para que
  todo sistema nuevo nazca con la autoridad en el servidor en vez de reconvertirse dos
  veces.

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

- Sin diálogos y sin cinemáticas pregrabadas (GDD v3 §12: el pueblo del arrecife y los
  piratas se comunican por gesto y comportamiento, nunca por texto largo ni voz).
  «Sin personajes humanos animados en pantalla» ya quedó descartado en §3.6 y en la
  biblia 05 §0 (packs CC0 con esqueleto Mixamo, fase 3).
- **«Sin multijugador» queda derogado [director, 2026-09-27].** Era una regla de
  producción heredada del GDD v3 §12, no un pilar de diseño. El juego tendrá cooperativo
  de 2 a 4 jugadores con servidor de escucha por Steam en el acceso anticipado (§6.2);
  el diseño entero está en `docs/diseno/biblia/08-cooperativo-y-red.md`. Lo que sí se
  mantiene de esa regla es el espíritu: **nada de servidores dedicados, ni sesiones
  públicas, ni chat de texto propio** (Steam ya da voz y texto en su superposición, y el
  juego solo añade un ping diegético de posición, biblia 08 §6.4).
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
| El cooperativo (§6.2) se come el presupuesto de H0–H5 y retrasa el acceso anticipado | Alcance | 69 días de agente estimados y repartidos por hito (biblia 08 §9.3), con **solo los cimientos** en H0: Steam, `GameState`/`PlayerState`, el reparto del jugador único, movimiento e interacción autoritativa. Ningún sistema se replica antes de estar terminado en solitario, y todo sistema nuevo nace con la autoridad en el servidor para no pagarlo dos veces. |
| El terreno replicado desincroniza y el mundo deja de ser el mismo en dos máquinas | Técnico | El formato de deltas de `FTerrainEditModel` guarda muestras, no operaciones, así que la fusión es idempotente y conmutativa (biblia 08 §2.2); comprobación de 4 bytes por chunk cada 30 s y petición del chunk completo al menor desacuerdo; una fila de la matriz de pruebas es una sesión de 2 h con 4 jugadores vigilando esa comprobación. |
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
por código, sin ningún asset externo» (§7.1), la regla de «sin fauna terrestre, sin
animación por esqueleto» (§3.6, §3.7) y la regla de «sin multijugador» (§7.2, derogada
por la decisión del director del 2026-09-27; ver §6.2 y
`docs/diseno/biblia/08-cooperativo-y-red.md`).
