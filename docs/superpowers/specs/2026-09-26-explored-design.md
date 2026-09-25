# EXPLORED — Documento de diseño del juego (GDD)

Versión 2 · 2026-09-26 · Unreal Engine 5.6 · Windows (objetivo: Steam)

---

## 0. Resumen en una frase

Un hidroavión chárter se estrella en un archipiélago del Pacífico que no aparece en
ningún mapa. Sobrevives con lo que puedes llevar en las manos y en la mochila,
reconstruyes tu vida isla a isla con la ayuda de una perra que también iba a bordo, y
sigues el rastro de la piloto desaparecida y de una expedición de 1974 que dejó algo sin
terminar. Al final decides cómo marcharte… o si quieres marcharte.

Duración estimada: 12–18 h la historia principal, más de 35 h completarlo todo.

---

## 1. Pilares de diseño

1. **Explorar siempre recompensa.** Cada colina, cueva o arrecife esconde algo: un
   recurso nuevo, una vista, una nota, un animal, un atajo. Nada de relleno.
2. **Supervivencia con las manos.** Sin cuadrícula de inventario. Llevas lo que te cabe
   en las manos, el cinturón y la mochila, y todo es un objeto físico que ves.
3. **Un mundo que respira.** Mareas, fases lunares, clima, migraciones, ciclo diario de
   los animales. El archipiélago cambia aunque no hagas nada.
4. **Historia que se encuentra, no que se cuenta.** Ni diálogos ni cinemáticas largas:
   notas, fotos, objetos, lugares, código morse. El jugador reconstruye lo ocurrido.
5. **Hecho por código.** Mallas, texturas, sonido y música salen de scripts versionados
   en el repositorio. Ningún asset externo.

---

## 2. Historia

### 2.1 Premisa

1974 — La **Expedición Halden**, cinco científicos de una fundación oceanográfica,
desembarca en un archipiélago sin nombre para estudiar dos anomalías: mareas que no
siguen las tablas y una bioluminiscencia que dibuja figuras en el agua las noches sin
luna. La expedición deja de emitir a los 41 días. Nunca se encuentra el archipiélago.

Hoy — El **Albatros**, un hidroavión chárter pilotado por **Inés Vidal**, cubre la ruta
entre dos atolones con dos pasajeros: tú y **Canela**, la perra de Inés. Una tormenta
tropical desvía el avión, los instrumentos fallan de forma extraña al acercarse a unas
islas que no deberían estar ahí y el Albatros amara de emergencia en una laguna.

Despiertas con el avión hundiéndose. Inés no está en la cabina y Canela tampoco.

### 2.2 Estructura en actos

| Acto | Nombre | Qué ocurre | Duración |
|---|---|---|---|
| Prólogo | *Amaraje* | Vuelo jugable en cabina, turbulencias, amaraje, escape buceando del avión hundido, primera noche en la playa | 20 min |
| I | *La playa* | Sobrevivir los primeros días: agua, fuego, refugio, comida. Encontrar la mochila en el ala flotante. Primera nota de Inés | 2–3 h |
| II | *Canela* | Construir la balsa. Llegar a la segunda isla, encontrar a Canela herida y ganarse su confianza. Descubrir el primer campamento Halden | 3–4 h |
| III | *El archipiélago* | Exploración abierta de las 7 islas. Recuperar piezas del Albatros para la baliza. Seguir el rastro de Inés. Descifrar el diario Halden | 5–8 h |
| IV | *La marea muerta* | Una marea extrema anunciada por la Luna revela lo que buscaba la expedición. Decisión final | 1–2 h |
| Epílogo | *Días después* | Según el final, créditos o modo libre con la partida intacta | — |

### 2.3 Los tres hilos narrativos

**Hilo A — Inés (emocional, principal).** Inés sobrevivió al amaraje y fue la primera en
explorar. Dejó **18 notas** en su libreta de vuelo, repartidas por el archipiélago, cada
vez más lejos. Cuentan cómo buscó ayuda, cómo perdió a Canela en una tormenta y cómo
encontró la radio de la estación Halden. La última nota, en el faro, explica que se
marchó en una canoa hacia el humo de un barco. Si terminas la baliza, su voz responde
por radio en el final: está viva y es quien avisa al rescate.

**Hilo B — La Expedición Halden (misterio, opcional).** **24 páginas del diario** de la
expedición, 5 campamentos abandonados, un observatorio de mareas y una estación de radio.
Los científicos descubrieron que las mareas del archipiélago siguen un ciclo propio,
ligado a una isla que solo emerge durante la «marea muerta» (una bajamar extrema cada
pocos meses). Sin fantasía: una explicación geológica y oceanográfica creíble contada a
trozos, con un giro humano (por qué uno de ellos decidió quedarse).

**Hilo C — Los Navegantes (antiguo, secreto).** Mucho antes de Halden, un pueblo de
navegantes ficticio pasó por aquí y dejó **30 petroglifos**, marcadores de piedra
alineados con estrellas y una **brújula estelar** tallada en la cima volcánica. Quien
descifra el sistema aprende a navegar sin instrumentos y accede al final verdadero.

### 2.4 Finales

| Final | Requisito | Resultado |
|---|---|---|
| **Rescate** | Montar y activar la baliza con las 4 piezas del Albatros | Helicóptero al amanecer. Eliges *Marcharte* (créditos) o *Quedarte* (modo libre) |
| **La Travesía** (verdadero) | Descifrar la brújula estelar + construir la canoa de balancín con vela | Navegas de noche por las estrellas hasta la isla emergida y desde allí, a casa. Secuencia final de navegación jugable |
| **El que se queda** | Rechazar el rescate con el diario Halden completo | Epílogo alternativo que cierra el hilo del científico que se quedó |
| **Epílogo secreto** | Diario al 100 % (notas, páginas, petroglifos, bestiario) | Escena extra poscréditos |

Los finales no se excluyen: tras cualquiera de ellos se puede continuar la misma partida.

---

## 3. El archipiélago

### 3.1 Generación

- Semilla elegida en «Nueva partida» (con una semilla «oficial» recomendada para la
  primera partida). Mapa de unos **6 × 6 km**.
- La **estructura** está garantizada (7 islas con su identidad, PdI obligatorios, rutas
  navegables) y el **detalle** es procedural: forma de las costas, relieve, ríos,
  vegetación, posición exacta de recursos y secretos.
- Terreno: ruido fractal + máscara por isla + erosión hidráulica simplificada + ríos desde
  las cumbres + arrecifes en la plataforma costera. Malla facetada low-poly por chunks,
  con LOD y colisión.
- Fondo marino modelado: plataforma somera turquesa, arrecife, talud y aguas profundas
  (azul oscuro, tiburones).

### 3.2 Las siete islas

| # | Isla | Bioma dominante | Rasgos únicos | Qué aporta |
|---|---|---|---|---|
| 1 | **Isla del Amaraje** | Playa y palmeral | Laguna con el Albatros hundido, cala protegida, arroyo | Inicio seguro, recursos básicos |
| 2 | **Esmeralda** | Selva densa | Cascada de tres saltos con cueva detrás, árboles gigantes, puentes de lianas | Madera dura, fruta, monos, Canela |
| 3 | **Isla del Humo** | Volcánica | Cráter activo, fumarolas, aguas termales, campos de obsidiana, tubo de lava | Obsidiana, azufre, brújula estelar en la cima |
| 4 | **Los Dientes** | Islotes rocosos | Acantilados, colonia de aves marinas, faro en ruinas, cuevas marinas | Huevos, plumas, guano, notas finales de Inés |
| 5 | **Manglar de las Voces** | Manglar y estuario | Niebla perpetua, raíces aéreas, bioluminiscencia, cocodrilo de estuario | Estación de radio Halden, arcilla, peligro |
| 6 | **Arenas Blancas** | Atolón con laguna | Arrecife de coral, pecio de un velero, playa de desove de tortugas | Pesca, buceo, conchas raras, velas de lona |
| 7 | **La Meseta** | Pradera alta y bosque nuboso | Mesetas con viento constante, observatorio de mareas, cuevas con petroglifos | Fibras, cultivos, piezas de la baliza |
| — | **La isla emergida** | Arrecife fósil | Solo aparece en la marea muerta | Final del Acto IV |

Además: bancos de arena que aparecen con la bajamar, islotes con una sola palmera
(guiño), una cueva submarina con aire atrapado y la cola del Albatros en el fondo del
canal.

### 3.3 Puntos de interés (más de 60)

- 7 **miradores**: al subir se revela en el mapa del diario la zona visible.
- 5 **campamentos Halden**, la estación de radio y el observatorio de mareas.
- 4 **restos del Albatros**: fuselaje, ala, cola y motor, cada uno con piezas y recursos.
- **Pecio** de un velero de los años 60 (lona, cuerda, metal, una botella con mensaje).
- 12 **cuevas** (marinas, volcánicas, de cascada) con recursos raros o petroglifos.
- **Aguas termales** (recuperan temperatura y ánimo), **pozas de marea** con vida propia.
- 8 **mensajes en botella**: humor, lore del mundo y pistas.
- **Nidos**, madrigueras, colmenas y colonias como lugares vivos que cambian con el día.

---

## 4. Sistemas del jugador

### 4.1 Movimiento

Andar, correr, agacharse, saltar, **trepar** salientes y paredes marcadas con grietas y
lianas, **nadar** en superficie y **bucear** (pulmones limitados, mejorables), deslizarse
por pendientes, **tirolinas de liana** entre árboles altos (construibles) y remar.
Caídas con daño, cansancio si nadas con peso y corriente fuerte en los canales.

### 4.2 Inventario diegético

| Contenedor | Capacidad | Notas |
|---|---|---|
| **Mano izquierda / derecha** | 1 objeto cada una | Los objetos grandes (tronco, balsa, animal cazado) ocupan las dos |
| **Bolsillos** | 4 objetos pequeños | Piedras, semillas, conchas, cerillas |
| **Cinturón** | 3 enganches | Herramientas y cantimplora. Se mejora con cuero |
| **Mochila** | Volumen + peso | Se encuentra en el ala del Albatros. Al abrirla se ve su interior en 3D y se sacan las cosas con la mano. Mejorable: mochila de fibra → de cuero con armazón de bambú |
| **Cestas, estantes, arcones** | Mundo | Almacenamiento físico en la base: lo guardado se ve colocado |
| **Angarillas** | Arrastre | Para transportar troncos y piedras en cantidad |

El peso total afecta a la energía, al nado y al ruido que haces al cazar.

### 4.3 Supervivencia

| Necesidad | Se baja por | Se recupera con | Si llega a cero |
|---|---|---|---|
| **Sed** | Tiempo, calor, esfuerzo | Coco, lluvia, agua hervida, destilador | Visión borrosa, daño |
| **Hambre** | Tiempo, esfuerzo | Comida, mejor si está cocinada | Menos energía máxima, daño |
| **Energía** | Correr, nadar, trepar | Descanso, comida | No puedes esprintar ni trepar |
| **Sueño** | Horas despierto | Dormir en refugio o hamaca | Torpeza, alucinaciones leves |
| **Temperatura** | Noche, lluvia, viento, estar mojado | Fuego, refugio, ropa, aguas termales | Hipotermia |
| **Ánimo** | Soledad, hambre, heridas, tormentas | Fuego, Canela, descubrimientos, música, vistas | Menos eficiencia; nunca mata |

**Nutrición:** la comida aporta proteína, energía y vitaminas. Comer solo cocos durante
días tiene consecuencias leves: la dieta variada importa.

**Heridas y estados:** cortes (sangrado, se vendan con tela o hojas medicinales),
esguince por caídas, quemadura solar, intoxicación (agua sin hervir, setas, yuca cruda),
picaduras (medusa, serpiente, con antídotos de plantas), infección si una herida no se
cura y fiebre. Se diagnostican por sensaciones y efectos en pantalla, no con iconos.

**HUD:** mínimo. El estado se comunica con el cuerpo: respiración, latido, temblor,
estómago que suena, bordes de pantalla, manos temblorosas y la voz interna del
personaje (texto breve). En el reloj del avión (objeto de muñeca) se consultan la hora y
los indicadores si se quiere.

### 4.4 Recursos

- **Madera:** ramas, palma, bambú, madera blanda, madera dura, madera flotante.
- **Piedra:** canto rodado, pedernal, basalto, obsidiana, arcilla, arena, sal (por
  evaporación), azufre.
- **Vegetal:** fibra de coco, lianas, hojas de palma, hojas grandes, resina, corteza,
  algodón silvestre, plantas medicinales (5 especies), tintes.
- **Animal:** hueso, cuero, tendón, plumas, conchas, caparazón, grasa, cera.
- **Rescatado:** aluminio del fuselaje, cable, tela de los asientos, paracaídas del
  equipaje, vidrio, piezas electrónicas, botiquín, bengalas, cerillas (limitadas).

### 4.5 Fabricación

**Con las manos:** un objeto en cada mano y la combinación aparece si existe
(piedra + palo → martillo tosco; pedernal + piedra → lasca; lasca + palo + fibra →
hacha). Las recetas se aprenden al descubrirlas y se anotan en el diario con un dibujo.

**Estaciones:**

| Estación | Permite |
|---|---|
| **Piedra de trabajo** | Herramientas de piedra, hueso y obsidiana |
| **Fogata → hoguera → horno de arcilla** | Cocinar, hervir, cerámica, carbón |
| **Secadero y ahumadero** | Conservar carne y pescado, curar pieles |
| **Telar de fibra** | Cuerda, redes, velas, ropa, mochila de fibra |
| **Banco de chatarra** | Herramientas de aluminio, reparar piezas del Albatros |
| **Astillero** | Balsa, canoa, canoa con balancín y vela |

**Árbol de herramientas (4 niveles):** Tosco (piedra y palo) → Tallado (pedernal, hueso)
→ Obsidiana (cortes superiores, frágil) → Rescatado (aluminio del avión, duradero). Hacha,
cuchillo, lanza, arco y flechas (de pesca y de caza), caña, red, garrote, pala, azada,
antorcha, cantimplora de bambú, olla de coco → vasija de barro, catalejo (lente del avión +
bambú), flauta de bambú, silbato para Canela.

### 4.6 Construcción

- **Por piezas y encaje:** suelos, pilares, paredes, techos, rampas, escaleras,
  barandillas y puertas, en tres materiales: hoja → bambú → madera.
- **Estructuras especiales:** refugio inclinado (nivel 0), cabaña sobre pilotes, casa en
  el árbol, embarcadero, torre de vigía, puente colgante, tirolina.
- **Útiles:** cama de hojas → hamaca, recolector de lluvia, destilador solar, trampas
  (lazo, jaula, nasa para peces), huerto (plátano, taro, batata, especias), gallinero
  para gallinas silvestres domesticadas, colmena, bandera o hito para marcar el mapa.
- **Integridad:** el viento y los ciclones dañan lo mal apoyado. Las estructuras se
  degradan y se reparan.
- Varias bases posibles; las fogatas encendidas son puntos de reaparición.

### 4.7 Comida y cocina

- **Recolección:** coco, plátano, mango, papaya, carambola, guayaba, fruta del pan, taro,
  yuca (tóxica si no se cocina), batata, setas (2 comestibles, 2 tóxicas, 1 alucinógena),
  miel, huevos, algas.
- **Mar:** cangrejos, lapas, erizos, pulpo, 8 especies de peces, langosta de arrecife.
- **Caza:** jabalí, aves, iguana, cangrejo de los cocoteros.
- **Técnicas:** asar en espeto, hervir, guisar en vasija, ahumar, salar, secar. Cada
  receta combina ingredientes en la vasija y da efectos distintos (calor, energía,
  ánimo). La comida se estropea: crudo < cocinado < ahumado o salado.

### 4.8 Caza y pesca

- **Rastreo:** huellas, excrementos, ramas rotas y ruidos. El viento lleva tu olor (se ve
  con hierba o humo) y te acercas agachado en su contra.
- **Caza:** lanza arrojadiza, arco y trampas. Los animales heridos huyen dejando rastro.
- **Pesca:** caña con cebo (minijuego de tensión), lanza en aguas someras, red y nasas, y
  pesca submarina buceando.

### 4.9 Navegación y mapa

- **Sin minimapa ni marcadores.** El **diario** incluye un mapa que se dibuja solo a
  medida que exploras, con trazo a mano. Los miradores revelan zonas grandes.
- Brújula del Albatros (se encuentra en el Acto I), catalejo, estrellas y sol para
  orientarse. Hitos de piedra y banderas para marcar rutas.
- **Embarcaciones:** balsa (lenta, solo aguas someras) → canoa (rápida) → canoa con
  balancín y vela (mar abierto, necesaria para el final verdadero). El viento importa al
  navegar a vela.

### 4.10 Canela

La perra de Inés. Aparece herida en Esmeralda en el Acto II. Para ganarte su confianza
hay que darle de comer y curarla. Después:

- Te sigue, se queda, vuelve (silbato). Duerme junto al fuego.
- **Olfato:** marca rastros de animales, notas de Inés cercanas y objetos enterrados.
- **Aviso:** gruñe ante serpientes, cocodrilos o jabalíes.
- Mejora el ánimo y el sueño. Se le puede acariciar y jugar a lanzarle palos.
- Nunca muere: si se asusta, huye a la base.

### 4.11 Diario

Objeto físico con pestañas dibujadas a mano:

- **Mapa**, **Notas de Inés**, **Diario Halden**, **Petroglifos**.
- **Bestiario** y **herbario**: cada especie se dibuja al observarla, cazarla o
  fotografiarla.
- **Recetas** de fabricación y de cocina, **Fotos** y **Mareas y Luna** (tabla de mareas
  que se completa en el observatorio).

### 4.12 Cámara desechable

En el equipaje del Albatros aparece una cámara desechable con **27 fotos** (luego se
encuentran más carretes). Fotografiar un animal lo registra en el bestiario y las fotos
quedan pegadas en el diario. Además hay un **modo foto** libre desde la pausa.

### 4.13 Radio y morse

La radio del Albatros, reparada, capta señales. En distintas horas y lugares se escuchan
**mensajes en morse** (sintetizados) que se descifran con la tabla del diario Halden:
pistas, coordenadas de secretos y, en el final, la respuesta de Inés.

### 4.14 Música diegética

La flauta de bambú se toca con el teclado (5 notas pentatónicas). Tocar junto al fuego
sube el ánimo, y ciertas melodías grabadas en los petroglifos abren un secreto en la
cueva de la cascada.

---

## 5. Mundo vivo

### 5.1 Tiempo

- Día de 40 min reales (configurable). Amaneceres y atardeceres largos y cuidados.
- **Fases lunares** en un ciclo de 12 días de juego: iluminación nocturna, mareas,
  actividad animal y bioluminiscencia (máxima con luna nueva).
- **Mareas** reales: dos pleamares al día, amplitud según la Luna. La bajamar abre
  pasos, pozas y cuevas; la pleamar las cierra y puede dejarte atrapado.

### 5.2 Clima

Soleado, nublado, niebla matinal, lluvia ligera, chubasco tropical, tormenta eléctrica
(rayos que pueden prender fuego a la vegetación seca), ola de calor, viento fuerte y
**ciclón**: se anuncia 1–2 días antes (barómetro del Albatros, cielo, animales inquietos,
mar de fondo), dura una noche, daña construcciones y cambia la costa, arrastrando
recursos nuevos a la playa. Arcoíris tras la lluvia.

### 5.3 Eventos

| Evento | Frecuencia | Qué pasa |
|---|---|---|
| Desove de tortugas | Luna llena | Tortugas en Arenas Blancas; crías hacia el mar al amanecer |
| Lluvia de estrellas | Rara | Noche espectacular; ánimo máximo |
| Bioluminiscencia | Luna nueva | El mar brilla; pistas Halden visibles en el agua |
| Paso de ballenas | Cada ~10 días | Visibles desde los miradores y los acantilados |
| Barco en el horizonte | 2–3 veces | Si hay una hoguera de señal encendida, deja un paquete a la deriva; si no, falsa esperanza |
| Avioneta de búsqueda | Acto III | Pasa de largo; hay que tener la señal lista |
| Erupción menor | Guion + azar | Temblor, ceniza, obsidiana nueva, peligro en el Humo |
| Eclipse | Una vez, Acto IV | Anuncia la marea muerta |
| Marea muerta | Acto IV | El mar se retira kilómetros; la isla emergida aparece |

---

## 6. Fauna (22 especies)

| Especie | Hábitat | Comportamiento | Uso |
|---|---|---|---|
| Cangrejo ermitaño | Playas | Cambia de concha y huye | Cebo, comida |
| Cangrejo de los cocoteros | Palmerales, noche | Trepa palmeras y roba | Comida, logro |
| Gaviota / charrán | Costa | Bandadas, roban pescado | Plumas, huevos |
| Fragata | Los Dientes | Planea en térmicas | Ambiente |
| Loro | Selva | Grupos ruidosos, imitan sonidos | Plumas, aviso |
| Tucán | Selva | Solitario, come fruta | Ambiente |
| Mono capuchino | Esmeralda | Tropa; **roba objetos** si los dejas en el suelo | Molestia, carisma |
| Murciélago frugívoro | Cuevas | Salen al atardecer | Guano |
| Jabalí | Selva, meseta | Pasta, embiste si lo acorralas | Carne, cuero, hueso |
| Gallina silvestre | Selva baja | Huye, se puede domesticar | Huevos |
| Iguana | Rocas soleadas | Toma el sol y huye al agua | Carne |
| Gecko | Nocturno | Paredes y refugios | Ambiente |
| Serpiente arborícola | Selva | Venenosa, pasiva salvo si la pisas | Peligro, antídoto |
| Cocodrilo de estuario | Manglar | Emboscada en el agua | Peligro principal |
| Tortuga marina | Arenas Blancas | Desova y nada | Evento (no se caza) |
| Peces de arrecife (×6) | Arrecife | Bancos | Pesca |
| Raya | Fondos arenosos | Enterrada, pica si la pisas | Peligro |
| Pulpo | Pozas y rocas | Se camufla | Comida |
| Medusa | Aguas abiertas | Deriva | Peligro |
| Tiburón de arrecife | Talud | Curioso, rara vez ataca | Tensión |
| Tiburón tigre | Aguas profundas | Ataca en mar abierto | Límite natural del mundo |
| Delfines / ballena jorobada | Mar abierto | Acompañan la canoa, saltan | Maravilla |

Además: luciérnagas, mariposas, abejas (colmenas), libélulas y peces voladores como
fauna ambiental con sistemas de partículas o bandadas (boids).

**Técnica:** cada especie es un conjunto de piezas low-poly animadas proceduralmente
(ciclos de patas, columna ondulante en peces y serpientes, aleteo, respiración). IA en
C++ con máquinas de estados, percepción (vista, oído, olfato con viento), manadas y
bandadas, y ciclo diario de actividad.

---

## 7. Modos y opciones de juego

| Modo | Descripción |
|---|---|
| **Explorador** | Las necesidades no matan; fauna pacífica. Para disfrutar del mundo y la historia |
| **Superviviente** | La experiencia prevista |
| **Náufrago** | Necesidades más duras, sin reaparición en fogatas (permadeath) |
| **Personalizado** | Duración del día, velocidad de necesidades, agresividad de la fauna, frecuencia de ciclones |
| **Nueva partida+** | Tras un final: nueva semilla conservando el diario de recetas y los conocimientos |

---

## 8. Dirección de arte

- **Low-poly facetado** con flat shading, color por vértice y paletas por isla: turquesa
  y arena en el Amaraje, verdes saturados en Esmeralda, negro y rojo en el Humo, gris y
  blanco en los Dientes, verde grisáceo y niebla en el Manglar, blanco y cian en Arenas
  Blancas, dorado y verde en la Meseta.
- **Luz como protagonista:** Lumen, niebla volumétrica, rayos de sol entre hojas,
  atardeceres saturados, noches azules con estrellas y Vía Láctea.
- **Agua:** océano con oleaje por vértice, espuma en la orilla, caústicas, transparencia
  en someros, bioluminiscencia.
- **Vegetación viva:** viento en hojas por material, palmeras que se doblan en el ciclón,
  hierba que se aparta al pasar.
- **Diario y UI dibujados a mano** con una tipografía manuscrita y trazos generados por
  código.

---

## 9. Audio y música

### 9.1 Efectos (síntesis procedural con Python)

Olas según la marea y el viento, viento por altura, lluvia sobre hojas, tela y techo,
truenos, fuego, pasos por superficie (arena, roca, hierba, madera, agua, barro), golpes
de herramienta por material, animales (cantos, gruñidos, aleteos), bajo el agua,
respiración y latido, Canela (ladridos, jadeo), radio, estática y morse, UI.

### 9.2 Paisaje sonoro

Ambiente en capas por isla, hora, clima y altura: coro de ranas e insectos por la noche,
aves al amanecer y silencio antes del ciclón.

### 9.3 Música adaptativa

Banda sonora compuesta proceduralmente (marimba, kalimba, pads cálidos, flauta de bambú,
percusión suave, cuerdas sintéticas):

| Capa | Cuándo |
|---|---|
| Tema principal | Menú, momentos clave |
| Exploración diurna | Por isla, con variaciones instrumentales |
| Noche | Minimalista, con insectos |
| Descubrimiento | Motivo corto al encontrar un PdI, una nota o un mirador |
| Tensión | Depredador cerca, ciclón, hipotermia |
| Mar abierto | Navegación a vela |
| Final | Tema principal orquestado para cada final |

Transiciones por fundido y compás; la música nunca tapa el ambiente.

---

## 10. Interfaz y frontend

- **Menú principal** con la isla en tiempo real y la cámara sobrevolando el archipiélago:
  Continuar, Nueva partida (modo, semilla), Cargar, Ajustes, Diario de logros, Créditos,
  Salir.
- **Pantalla de carga** que muestra la generación del archipiélago en forma de mapa.
- **Pausa:** Reanudar, Diario, Modo foto, Ajustes, Guardar, Salir al menú.
- **Ajustes:**
  - *Gráficos:* resolución, modo de ventana, escala de render, TSR/DLSS, calidad general
    y por categoría, distancia de vegetación, límite de FPS, VSync, brillo y gamma.
  - *Audio:* maestro, música, efectos, ambiente, voz interna, dispositivo de salida.
  - *Controles:* remapeo completo de teclado y ratón, soporte de mando (Xbox/PS),
    sensibilidad, invertir Y, mantener o alternar.
  - *Juego:* FOV, balanceo de cámara, duración del día, subtítulos, tamaño del texto,
    idioma (ES/EN).
  - *Accesibilidad:* modo daltónico, reducir movimiento, desactivar destellos, ayuda
    para apuntar, avisos visuales de sonido.
- **Guardado:** automático al dormir y en fogatas, 3 ranuras manuales y una copia de
  seguridad.

---

## 11. Logros (30)

Ejemplos: *Primer fuego*, *Tierra firme* (llegar a la 2.ª isla), *Buena chica* (Canela te
sigue), *Cartógrafo* (mapa completo), *Botánico* (herbario completo), *Ladrón de guante
blanco* (recuperar lo robado por un mono), *Rey del cocotero*, *Bajo el volcán*, *Luz en
el agua*, *Ojo de ciclón* (sobrevivir a uno sin daños en la base), *Navegante*,
*Morse* (descifrar todos los mensajes), *La Travesía*, *El que se queda*, *Náufrago de
verdad* (terminar en modo Náufrago), *Sin mapa* (final sin abrir el mapa del diario).

---

## 12. Arquitectura técnica

### 12.1 Principios

- **C++ como núcleo** (módulo `Explored`). Los Blueprints solo son datos generados por
  scripts de Python del editor; la lógica no vive en Blueprint.
- **Datos en tablas** (`UDataAsset` / `DataTable` generados desde JSON versionado):
  objetos, recetas, especies, recetas de cocina, eventos y textos. Añadir contenido no
  requiere tocar código.
- **Subsistemas** con una responsabilidad cada uno, comunicados por delegados y
  `GameplayTags`.

### 12.2 Módulos

| Módulo | Responsabilidad |
|---|---|
| `WorldGen` | Semilla → islas, relieve, biomas, ríos, arrecifes, PdI; chunks de malla facetada |
| `Scatter` | Vegetación, rocas y recursos en HISM, por reglas de bioma; recolección persistente |
| `Ocean` | Malla de océano, mareas, flotabilidad, corrientes |
| `Sky` | Sol, Luna y fases, estrellas, atmósfera |
| `Weather` | Estados del clima, transiciones, viento global, ciclón |
| `Events` | Calendario de eventos del mundo |
| `Interaction` | Foco, acciones contextuales, sostener y soltar |
| `Carry` | Manos, bolsillos, cinturón, mochila, contenedores, peso |
| `Crafting` | Recetas por combinación en las manos y en estaciones |
| `Building` | Piezas, encaje, integridad, daños, reparación |
| `Survival` | Necesidades, nutrición, estados y heridas, temperatura |
| `Cooking` | Fuego, vasijas, recetas, conservación |
| `Fauna` | Especies, IA, percepción, animación procedural, bandadas |
| `Companion` | Canela |
| `Boats` | Balsa, canoas, vela, remo, físicas de flotación |
| `Journal` | Mapa dibujado, colecciones, fotos, recetas |
| `Narrative` | Notas, páginas, petroglifos, morse, actos, finales |
| `Audio` | Ambiente, música adaptativa, mezcla |
| `Save` | Semilla + deltas del mundo + jugador + progreso |
| `Settings` | Ajustes y persistencia |
| `UI` | Menús, pausa, diario, HUD mínimo (C++ y Slate/UMG por código) |

### 12.3 Pipeline de contenido (todo por código)

```
Tools/Blender/*.py        --(blender -b -P)---------------> Art/Export/Meshes/*.fbx
Tools/Audio/*.py          --(uv run)----------------------> Art/Export/Audio/*.wav
Tools/Music/*.py          --(uv run)----------------------> Art/Export/Music/*.wav
Tools/Textures/*.py       --(uv run)----------------------> Art/Export/Textures/*.png
Data/*.json               --(importador)------------------> DataAssets
Tools/Unreal/import_*.py  --(UnrealEditor-Cmd -run=pythonscript)--> Content/
```

- Todo reproducible con un único comando (`Tools/build_content.ps1`).
- El MCP de Blender se usa para iterar y hacer capturas; la fuente de verdad son los
  scripts.

### 12.4 Rendimiento

- 60 FPS a 1080p en calidad Alta sobre una RTX 4060 Laptop; 30 FPS estables en Baja con
  gráficos integrados.
- Streaming por chunks (World Partition para lo fijo, chunks propios para el terreno),
  HISM y culling por distancia en la vegetación, IA de fauna con LOD de actualización.

---

## 13. Calidad y pruebas

- **Automation Tests (C++):** determinismo por semilla, garantías de PdI y
  navegabilidad, manos, mochila y peso, recetas, necesidades, integridad de
  construcciones, mareas, guardado y carga, finales.
- **Pruebas funcionales en mapa:** escenarios automatizados (encender fuego, cruzar a
  nado, sobrevivir a una noche).
- **Verificación visual:** capturas automáticas desde cámaras fijas por isla y hora,
  revisadas en cada hito.
- **Perfilado:** Unreal Insights en cada hito con presupuesto de ms por sistema.

---

## 14. Hitos

| Hito | Contenido | Resultado jugable |
|---|---|---|
| **M0 Cimientos** | Proyecto C++, build por CLI, pipelines de Blender, audio e importación, tests | Proyecto que compila y un asset generado dentro |
| **M1 Archipiélago** | WorldGen, océano, cielo, día/noche, Luna, mareas, vegetación | Pasear por las 7 islas |
| **M2 Manos** | Controlador, interacción, manos, mochila, recolección, herramientas básicas | Talar, recoger, cargar |
| **M3 Sobrevivir** | Necesidades, fuego, cocina, fabricación, construcción, clima | Aguantar 10 días |
| **M4 Vida** | Fauna completa, caza, pesca, Canela | Isla habitada |
| **M5 Mar** | Balsa, canoas, vela, buceo, arrecife | Navegar entre islas |
| **M6 Historia** | Notas, Halden, petroglifos, morse, eventos, prólogo, actos, finales | Historia completa |
| **M7 Sonido** | Efectos, ambiente, música adaptativa | Juego sonorizado |
| **M8 Frontend** | Menús, ajustes, guardado, diario, logros, localización, accesibilidad | Producto completo |
| **M9 Pulido** | Equilibrado, rendimiento, bugs, empaquetado Win64 | Build listo para Steam |

---

## 15. Fuera de alcance

- Multijugador.
- Doblaje con voces humanas: la narrativa es escrita, con morse y sonidos.
- Humanos animados en pantalla (Inés nunca aparece; se la conoce por lo que deja).
- Alta en Steamworks: depende del titular (cuenta y tasa de 100 USD). El proyecto
  entrega el build empaquetado, la página de tienda redactada y las capturas.
