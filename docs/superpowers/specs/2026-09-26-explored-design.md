# EXPLORED — Documento de diseño del juego (GDD)

Versión 3 · 2026-09-26 · Unreal Engine 5.6 · Windows (objetivo: Steam)

---

## 0. Resumen en una frase

Sandbox de supervivencia y exploración **no lineal** en un archipiélago del Pacífico
sin nombre: un hidroavión se estrella, sobrevives con lo que encuentras, tu base crece
isla a isla y dibujas tu propio mapa a mano mientras descubres las ruinas de un pueblo
de navegantes antiguo y los restos de una expedición científica de 1974. Sin diálogos
ni personajes animados: la historia se lee en los lugares, nunca se cuenta. Un limonero
junto a la base y, al final, un barco amarillo llamado «Limón» son la única firma
personal del autor.

> Dedicado a Almudena, mi Limón.

Sin acto ni final obligatorio. Objetivo final opcional: construir el barco «Limón» y
zarpar de noche guiándote solo por las estrellas, o quedarte.

---

## 1. Pilares de diseño

1. **Sobrevivir con el cuerpo.** Hambre, sed, insolación y quemaduras, frío nocturno,
   heridas e infección, intoxicación alimentaria, escorbuto si pasan semanas sin fruta
   fresca.
2. **Una base que crece.** Refugio → cabaña → taller, con huerto, secadero, muelle y
   astillero.
3. **Libertad total.** Sin camino fijo ni orden obligatorio entre islas o sistemas.
4. **Cartografiar a mano.** El mapa está en blanco al empezar; lo dibuja el jugador,
   isla a isla (§5).
5. **Historia mínima y ambiental.** Ni diálogos ni cinemáticas: los lugares y los
   objetos cuentan lo que pasó, como mucho en una frase.
6. **Hecho por código.** Mallas, texturas, sonido y música salen de scripts
   versionados en el repositorio. Ningún asset externo.

---

## 2. Prólogo

Un hidroavión chárter vuela sobre el Pacífico cuando una tormenta lo desvía hacia un
archipiélago que no aparece en ningún mapa. Secuencia jugable breve, sin diálogo ni
personajes con nombre: turbulencias, amaraje de emergencia, escape buceando del avión
hundido y primera noche en la playa.

---

## 3. Historia mínima ambiental

Sin tramas ni finales narrativos. El mundo cuenta lo que pasó por lo que deja atrás:

- El propio **Albatros** hundido (fuselaje, ala, cola, motor), recuperable pieza a
  pieza.
- Un **campamento científico abandonado de 1974** (Halden queda como nombre en cajas,
  herramientas y un observatorio de mareas oxidado: nada que leer en párrafos, solo
  ver y usar).
- Las **ruinas de un pueblo de navegantes** mucho más antiguo (§6).

Cuando un lugar necesita una frase, es una sola línea grabada o escrita, nunca un
párrafo ni una conversación.

---

## 4. El archipiélago

### 4.1 Generación

- El archipiélago se genera **en el editor** a partir de una semilla (proceso
  determinista y reproducible) y se hornea como mallas Nanite en World Partition:
  Lumen con campos de distancia, LOD y streaming nativos. La semilla oficial es la del
  juego publicado; la elección de semilla en «Nueva partida» queda como mejora
  posterior y no bloquea el lanzamiento.
- Mapa de unos **6 × 6 km**.
- La **estructura** está garantizada (7 islas con su identidad, PdI obligatorios,
  rutas navegables) y el **detalle** es procedural: forma de las costas, relieve,
  ríos, vegetación, posición exacta de recursos y secretos.
- Terreno: ruido fractal + máscara por isla + erosión hidráulica simplificada + ríos
  desde las cumbres + arrecifes en la plataforma costera. Terreno volumétrico (campo
  de distancia con signo poligonizado con Surface Nets): malla suave por chunks que
  admite cuevas, voladizos, arcos marinos y túneles, con LOD y colisión.
- Fondo marino modelado: plataforma somera turquesa, arrecife, talud y aguas profundas
  (azul oscuro, tiburones).

### 4.2 Las siete islas

| # | Isla | Bioma dominante | Rasgos únicos | Qué aporta |
|---|---|---|---|---|
| 1 | **Isla del Amaraje** | Playa y palmeral | Laguna con el Albatros hundido, cala protegida, arroyo | Inicio seguro, recursos básicos |
| 2 | **Esmeralda** | Selva densa | Cascada de tres saltos con cueva detrás, árboles gigantes, puentes de lianas | Madera dura, fruta, ruinas rituales |
| 3 | **Isla del Humo** | Volcánica | Cráter activo, fumarolas, aguas termales, campos de obsidiana, tubo de lava | Obsidiana, azufre, marae en la cima |
| 4 | **Los Dientes** | Islotes rocosos | Acantilados, nidos vacíos de aves marinas con bandadas en vuelo lejano, faro en ruinas, cuevas marinas | Huevos, plumas, guano, faro a reparar |
| 5 | **Manglar de las Voces** | Manglar y estuario | Niebla perpetua, raíces aéreas, bioluminiscencia, barro que atrapa, corrientes traicioneras | Estación Halden, arcilla, peligro |
| 6 | **Arenas Blancas** | Atolón con laguna | Arrecife de coral, pecio de un velero, playa de desove de tortugas, canoas dobles fosilizadas | Pesca, buceo, conchas raras, velas de lona |
| 7 | **La Meseta** | Pradera alta y bosque nuboso | Mesetas con viento constante, observatorio de mareas, marae con petroglifos | Fibras, cultivos, piezas del barco |
| — | **La isla oculta** | Arrecife fósil | Solo se llega siguiendo los caminos de estrellas reunidos (§6) | Objetivo final opcional |

Además: bancos de arena que aparecen con la bajamar, islotes con una sola palmera
(guiño), una cueva submarina con aire atrapado, ruinas sumergidas que solo la marea
viva extrema descubre, y la cola del Albatros en el fondo del canal.

### 4.3 Puntos de interés (más de 60)

- 7 **miradores**: revelan un contorno aproximado del terreno visible en el mapa.
- 5 **campamentos Halden**, la estación de radio y el observatorio de mareas: ruinas
  con recursos, sin lectura larga.
- 4 **restos del Albatros**: fuselaje, ala, cola y motor, cada uno con piezas y
  recursos para el barco «Limón».
- **Pecio** de un velero de los años 60 (lona, cuerda, metal).
- **Ruinas del pueblo navegante** (§6): marae, estatuas, cuevas rituales, canoas
  dobles fosilizadas.
- 12 **cuevas** (marinas, volcánicas, de cascada) con recursos raros o petroglifos.
- **Aguas termales** (recuperan temperatura y ánimo), **pozas de marea** con vida
  propia.
- 8 **botellas** en playas al azar: pequeños objetos de colección para el museo.
- **Nidos**, madrigueras y colmenas como lugares vivos que cambian con el día.

---

## 5. Cartografía a mano

Sistema central del juego: sin minimapa ni marcadores automáticos. El mapa es lo que
el jugador dibuja.

### 5.1 El mapa como objeto

- Objeto físico de papel que existe en el mundo: se moja con la lluvia y el agua de
  mar (la tinta corre si no se guarda seco) y se puede copiar en limpio en la mesa de
  cartografía de la base.
- Empieza en blanco.

### 5.2 Trazado de costa

- Al caminar o nadar cerca de la orilla, el juego registra la ruta del jugador y la
  dibuja como línea de costa a mano alzada, con el temblor propio de estar caminando
  (nunca una línea de precisión GPS).
- La precisión del trazo empeora al correr y mejora usando la brújula (rectifica el
  rumbo general del dibujo).

### 5.3 Miradores y bocetos sin confirmar

- Desde un mirador se ve el contorno aproximado de la isla y de las cercanas; el juego
  añade ese contorno al mapa como un boceto tenue, sin confirmar.
- El boceto pasa a trazo firme solo cuando el jugador recorre esa costa de verdad.
  Hasta entonces es una silueta orientativa que puede estar equivocada en el detalle.

### 5.4 Marcas y sellos

- El jugador coloca sus propias marcas manuales en el mapa: agua dulce, cueva,
  peligro, recurso, y un texto corto propio.
- Con el catalejo se puede marcar a distancia sin haber llegado al sitio.

### 5.5 Instrumentos de precisión

| Instrumento | Efecto |
|---|---|
| Brújula del Albatros | Rumbo fijo en el trazo de costa; corrige la deriva |
| Catalejo | Marca lugares vistos a distancia sin haber llegado |
| Sextante (hallazgo de ruina o pecio) | Coordenadas aproximadas: marca un punto exacto en mar abierto |

### 5.6 Datos y persistencia

- El mapa es un `UDataAsset` de trazos vectoriales (polilíneas) por isla, más una
  lista de marcas (id de sello, posición, texto corto) y el catálogo de tesoros (§7).
- Se guarda con la partida; no se regenera desde el mundo. Un trazo mojado y no
  copiado a tiempo se difumina: se pierde el detalle dibujado, no el hecho de haber
  estado allí.
- UI: el mapa se abre como un objeto en las manos, no como un menú de pantalla
  completa, coherente con el inventario diegético.

---

## 6. Ruinas y navegación ancestral

Antes de Halden, antes de que el jugador llegara: un pueblo de navegantes ficticio,
tratado con el mismo respeto que una cultura real, dejó su huella en las siete islas.

### 6.1 Qué se encuentra

- **Marae:** plataformas de piedra a ras de suelo, a veces con un muro bajo o un
  patio.
- **Estatuas** talladas en basalto, erosionadas, mirando siempre al mar o a una
  estrella concreta.
- **Petroglifos** (30): motivos tallados junto a cuevas y cumbres.
- **Canoas dobles fosilizadas**, varadas o semienterradas en la arena.
- **Cuevas rituales** con pinturas y ofrendas, a veces bajo el nivel del mar.
- **Ruinas sumergidas**, solo visibles con la marea viva extrema.

### 6.2 Wayfinding: lo que enseñan

Cada ruina completa enseña una técnica de navegación tradicional, que se añade al
mapa como una anotación propia:

| Técnica | Qué revela |
|---|---|
| Camino de estrellas | Rumbo nocturno hacia otra isla concreta |
| Lectura del oleaje | Distancia y dirección a tierra por el patrón del mar de fondo |
| Aves al atardecer | Las bandadas vuelan hacia tierra al anochecer: delatan la isla más cercana |
| Nubes fijas | Una nube estacionaria sobre el horizonte delata una isla lejana |
| Color del agua | El cambio de tono marca bajíos y arrecifes antes de verlos |

### 6.3 El objetivo final

Reunir suficientes caminos de estrellas permite, de noche y solo guiándose por el
cielo, llevar el barco «Limón» hasta la isla oculta.

---

## 7. Tesoros y museo

- **Artefactos:** anzuelos de hueso tallado, adornos de concha, figuras de piedra,
  cartas de navegación de varillas y conchas, tapa (tela vegetal pintada), remos
  ceremoniales. Se encuentran en marae, cuevas rituales y pecios.
- **Exposición:** cada artefacto se coloca en una estantería o vitrina de la base; la
  base se convierte así en un pequeño museo que crece con la partida.
- **Catálogo:** el mapa lleva un apartado de colección con silueta y procedencia de
  cada tesoro, completo o no.

---

## 8. Sistemas del jugador

> El catálogo completo de objetos, el sistema de combinación por propiedades, la
> pesca, las estaciones y los temporales están en
> `docs/design/biblia-de-contenido.md`, que prevalece sobre este documento en caso de
> discrepancia.

### 8.1 Movimiento

Andar, correr, agacharse, saltar, **trepar** salientes y paredes marcadas con grietas
y lianas, **nadar** en superficie y **bucear** (pulmones limitados, mejorables),
deslizarse por pendientes, **tirolinas de liana** entre árboles altos (construibles) y
remar. Caídas con daño, cansancio si nadas con peso y corriente fuerte en los canales.

### 8.2 Inventario diegético

| Contenedor | Capacidad | Notas |
|---|---|---|
| **Mano izquierda / derecha** | 1 objeto cada una | Los objetos grandes (tronco, balsa, pesca grande) ocupan las dos |
| **Bolsillos** | 4 objetos pequeños | Piedras, semillas, conchas, cerillas |
| **Cinturón** | 3 enganches | Herramientas y cantimplora. Se mejora con cuero |
| **Mochila** | Volumen + peso | Se encuentra en el ala del Albatros. Al abrirla se ve su interior en 3D y se sacan las cosas con la mano. Mejorable: mochila de fibra → de cuero con armazón de bambú |
| **Cestas, estantes, arcones** | Mundo | Almacenamiento físico en la base: lo guardado se ve colocado |
| **Angarillas** | Arrastre | Para transportar troncos y piedras en cantidad |

El peso total afecta a la energía, al nado y al ruido que haces al pescar.

### 8.3 Supervivencia

| Necesidad | Se baja por | Se recupera con | Si llega a cero |
|---|---|---|---|
| **Sed** | Tiempo, calor, esfuerzo | Coco, lluvia, agua hervida, destilador | Visión borrosa, daño |
| **Hambre** | Tiempo, esfuerzo | Comida, mejor si está cocinada | Menos energía máxima, daño |
| **Energía** | Correr, nadar, trepar | Descanso, comida | No puedes esprintar ni trepar |
| **Sueño** | Horas despierto | Dormir en refugio o hamaca | Torpeza, alucinaciones leves |
| **Temperatura** | Noche, lluvia, viento, estar mojado | Fuego, refugio, ropa, aguas termales | Hipotermia |
| **Vitamina C** | Días sin fruta fresca | Limón del limonero, cítricos silvestres | Escorbuto: encías, visión, sangrado leve |
| **Ánimo** | Soledad, hambre, heridas, tormentas | Fuego, descubrimientos, música, un mapa que avanza | Menos eficiencia; nunca mata |

**Nutrición:** la comida aporta proteína, energía y vitaminas. Comer solo cocos
durante días tiene consecuencias leves: la dieta variada importa.

**Heridas y estados:** cortes (sangrado, se vendan con tela o hojas medicinales),
esguince por caídas, quemadura solar, intoxicación (agua sin hervir, setas, yuca
cruda), picaduras (medusa, raya, con antídotos de plantas), infección si una herida no
se cura, y fiebre. Se diagnostican por sensaciones y efectos en pantalla, no con
iconos.

**HUD:** mínimo. El estado se comunica con el cuerpo: respiración, latido, temblor,
estómago que suena, bordes de pantalla y manos temblorosas. En el reloj del avión
(objeto de muñeca) se consultan la hora y los indicadores si se quiere.

### 8.4 Recursos

- **Madera:** ramas, palma, bambú, madera blanda, madera dura, madera flotante.
- **Piedra:** canto rodado, pedernal, basalto, obsidiana, arcilla, arena, sal (por
  evaporación), azufre.
- **Vegetal:** fibra de coco, lianas, hojas de palma, hojas grandes, resina, corteza,
  algodón silvestre, plantas medicinales (5 especies), tintes.
- **Marino:** espinas y huesos de pescado, piel de tiburón curtida, aceite de
  pescado y de coco, conchas, caparazón.
- **Rescatado:** aluminio del fuselaje, cable, tela de los asientos, paracaídas del
  equipaje, vidrio, piezas electrónicas, botiquín, bengalas, cerillas (limitadas).

### 8.5 Fabricación

**Con las manos:** un objeto en cada mano y la combinación aparece si existe (piedra +
palo → martillo tosco; pedernal + piedra → lasca; lasca + palo + fibra → hacha). Las
recetas se aprenden al descubrirlas y quedan anotadas con un boceto en el mapa.

**Estaciones:**

| Estación | Permite |
|---|---|
| **Piedra de trabajo** | Herramientas de piedra, hueso y obsidiana |
| **Fogata → hoguera → horno de arcilla** | Cocinar, hervir, cerámica, carbón |
| **Secadero y ahumadero** | Conservar pescado, curar pieles |
| **Telar de fibra** | Cuerda, redes, velas, ropa, mochila de fibra |
| **Banco de chatarra** | Herramientas de aluminio, reparar piezas del Albatros |
| **Astillero** | Balsa, canoa, canoa con balancín y vela, barco «Limón» |

**Árbol de herramientas (4 niveles):** Tosco (piedra y palo) → Tallado (pedernal,
hueso) → Obsidiana (cortes superiores, frágil) → Rescatado (aluminio del avión,
duradero). Arpón, cuchillo, caña, red, pala, azada, antorcha, cantimplora de bambú,
olla de coco → vasija de barro, catalejo (lente del avión + bambú), flauta de bambú,
silbato (espanta fauna marina curiosa cerca de la orilla), mesa de cartografía.

### 8.6 Construcción

- **Por piezas y encaje:** suelo, pared, pared con ventana, puerta, techo, escalera,
  pilote, en materiales que mejoran: hoja de palma → bambú → madera → piedra.
- **Estructuras especiales:** refugio inclinado (nivel 0), cabaña sobre pilotes, casa
  en el árbol, embarcadero, torre de vigía, puente colgante, tirolina.
- **Útiles:** cama de hojas → hamaca, recolector y depósito de agua de lluvia,
  destilador solar, nasa para peces y trampa de cangrejos, huerto, colmena, mesa de
  cartografía, estanterías y vitrinas para el museo (§7).
- **Integridad:** el viento y los ciclones dañan lo mal apoyado. Las estructuras se
  degradan y se reparan.
- Varias bases posibles; las fogatas encendidas son puntos de reaparición.

### 8.7 Huerto y limonero

- **El limonero:** primer árbol que se planta junto a la base, con un limón
  encontrado en el equipaje del Albatros. Crece en etapas estáticas ligadas a los días
  (esqueje → brote → árbol joven → árbol adulto con fruta). Sus limones previenen y
  curan el escorbuto. Es el único cultivo que nunca se arranca.
- **Huerto:** plátano, taro, batata, piña, maracuyá, especias y plantas medicinales,
  cada una con sus propias etapas estáticas y su estación. Riego, compost,
  espantapájaros.

### 8.8 Comida y cocina

- **Recolección:** coco, plátano, mango, papaya, carambola, guayaba, fruta del pan,
  taro, yuca (tóxica si no se cocina), batata, setas (2 comestibles, 2 tóxicas, 1
  alucinógena), miel, huevos de gaviota, algas.
- **Mar:** cangrejos, lapas, erizos, pulpo, 11 especies de peces, langosta de
  arrecife. Cangrejos, lapas, erizos y pulpo se obtienen de nasas y trampas, o
  directamente de la poza de marea, sin un animal visible que perseguir.
- **Técnicas:** asar en espeto, hervir, guisar en vasija, ahumar, salar, secar. Cada
  receta combina ingredientes en la vasija y da efectos distintos (calor, energía,
  ánimo). La comida se estropea: crudo < cocinado < ahumado o salado.

### 8.9 Pesca y marisqueo

- **Rastreo:** sombras de bancos de peces bajo el agua y rastros de tortuga hacia el
  mar tras el desove.
- **Captura:** arpón (empuje y arrojadizo), arco con flechas de arpón, y trampas de
  nasa (cangrejo, huevos: se revisan sin que aparezca un animal al que perseguir).
- **Pesca:** caña con cebo (minijuego de tensión), arpón en aguas someras, red y
  nasas, y pesca submarina buceando.

### 8.10 Embarcaciones

Balsa (lenta, solo aguas someras) → canoa (rápida) → canoa con balancín y vela (mar
abierto) → barco «Limón» (amarillo, construido con las piezas recuperadas del
Albatros y madera del astillero, objetivo final opcional junto con los caminos de
estrellas de §6). El viento importa al navegar a vela.

### 8.11 Cámara desechable

En el equipaje del Albatros aparece una cámara desechable con fotos limitadas (luego
se encuentran más carretes). Fotografiar una especie o un tesoro lo registra en el
catálogo del mapa. Además hay un **modo foto** libre desde la pausa.

### 8.12 Música diegética

La flauta de bambú se toca con el teclado (5 notas pentatónicas). Tocar junto al
fuego sube el ánimo.

---

## 9. Mundo vivo

### 9.1 Tiempo

- Día de 40 min reales (configurable). Amaneceres y atardeceres largos y cuidados.
- **Fases lunares** en un ciclo de 12 días de juego: iluminación nocturna, mareas y
  bioluminiscencia (máxima con luna nueva).
- **Mareas** reales: dos pleamares al día, amplitud según la Luna. La bajamar abre
  pasos, pozas y cuevas; la pleamar las cierra y puede dejar atrapado.
- **Marea viva extrema:** evento raro que descubre las ruinas sumergidas (§6).

### 9.2 Clima

El año tiene 4 estaciones (seca, primeras lluvias, monzón y ciclones) de 8 días cada
una; ver la biblia de contenido, §6.

Soleado, nublado, niebla matinal, lluvia ligera, chubasco tropical, tormenta eléctrica
(rayos que pueden prender fuego a la vegetación seca), ola de calor, viento fuerte y
**ciclón**: se anuncia 1–2 días antes (barómetro del Albatros, cielo, mar de fondo),
dura una noche, daña construcciones y cambia la costa, arrastrando recursos nuevos a
la playa. Arcoíris tras la lluvia.

### 9.3 Eventos

| Evento | Frecuencia | Qué pasa |
|---|---|---|
| Desove de tortugas | Luna llena | Tortugas en Arenas Blancas; crías hacia el mar al amanecer |
| Lluvia de estrellas | Rara | Noche espectacular; ánimo máximo |
| Bioluminiscencia | Luna nueva | El mar brilla |
| Paso de ballenas | Cada ~10 días | Visibles desde los miradores y los acantilados |
| Barco en el horizonte | Ocasional | Si hay una hoguera de señal encendida, deja un paquete a la deriva |
| Erupción menor | Azar | Temblor, ceniza, obsidiana nueva, peligro en el Humo |
| Marea viva extrema | Rara | El mar se retira más de lo normal; ruinas sumergidas visibles |

---

## 10. Fauna (9 especies marinas y de bandada)

Sin animales de compañía ni fauna terrestre. Solo vida marina y aves siempre en
vuelo, nunca posadas ni en tierra.

| Especie | Hábitat | Comportamiento | Uso |
|---|---|---|---|
| Peces de arrecife (×8) | Arrecife | Bancos que se dispersan y se rehacen | Pesca |
| Peces de mar abierto (×3) | Talud, aguas profundas | Grandes bancos migratorios | Pesca |
| Raya | Fondos arenosos | Enterrada; se aleja ondulando si te acercas | Peligro |
| Medusa | Aguas abiertas | Deriva con la corriente | Peligro |
| Tiburón de arrecife | Talud | Curioso, rara vez ataca | Tensión |
| Tiburón tigre | Aguas profundas | Ataca en mar abierto | Límite natural del mundo |
| Delfines | Mar abierto | Acompañan la canoa, saltan | Maravilla |
| Tortuga marina | Arenas Blancas y mar abierto | Nada en la laguna; desova con luna llena | Evento (no se caza) |
| Ballena jorobada | Mar abierto, lejana | Se ve desde miradores y acantilados, nunca de cerca | Maravilla |
| Gaviota / charrán y fragata (bandadas) | Costa, Los Dientes | Siempre en vuelo, a distancia; roban pescado dejado al aire | Ambiente |

**Sin criatura visible:** cangrejo, cangrejo de los cocoteros, lapas, erizos y pulpo
se obtienen de nasas, trampas y pozas de marea, como un objeto más que se recoge, sin
un animal al que perseguir ni animar. Huevos y guano salen de nidos vacíos en Los
Dientes.

Además: luciérnagas, mariposas, abejas (colmenas), libélulas y peces voladores como
fauna ambiental con sistemas de partículas o bandadas (boids), y bioluminiscencia
marina.

**Técnica y regla de animación — sin esqueletos:** cada especie es un conjunto de
piezas low-poly sin rig de huesos, animadas proceduralmente en el shader de vértices
(columna ondulante en peces, rayas y tortugas, aleteo en aves, pulsación en medusas) o
por piezas rígidas movidas por código (aletas, pectorales de la raya). Ninguna fauna
camina sobre patas ni se posa. IA en C++ con máquinas de estados, percepción (vista,
oído, olfato con la corriente), bancos y bandadas (boids), y ciclo diario de
actividad.

---

## 11. Modos y opciones de juego

| Modo | Descripción |
|---|---|
| **Explorador** | Las necesidades no matan; fauna pacífica. Para disfrutar del mundo |
| **Superviviente** | La experiencia prevista |
| **Náufrago** | Necesidades más duras, sin reaparición en fogatas (permadeath) |
| **Personalizado** | Duración del día, velocidad de necesidades, frecuencia de ciclones |

---

## 12. Reglas de producción (obligatorias)

- **Sin personajes animados ni humanos en pantalla.** El jugador nunca ve a nadie.
- **Sin fauna terrestre.** Solo la fauna de §10 (marina y bandadas siempre en vuelo).
- **Sin diálogos.** Ningún sistema de conversación ni subtítulos de voz de personaje.
- **Sin multijugador.**
- **Sin cinemáticas.** Toda secuencia (prólogo, hallazgo de una ruina) es jugable o se
  resuelve con cámara diegética, nunca con un vídeo pregrabado.
- **Todo el contenido visual es estático o animado por shader/código:** nada de
  animación por huesos (rig de esqueleto) en ningún asset del juego.

---

## 13. Dirección de arte

- **Estilizado suave («low-poly pulido»):** geometría de densidad media con
  **sombreado suave** (normales interpoladas), siluetas limpias y formas redondeadas;
  no facetado extremo. Referencias de tono: la calidez de *Firewatch*, la limpieza de
  *Sable* y la luz de *A Short Hike*, en 3D con iluminación moderna.
- **Materiales estilizados:** color base por vértice + gradientes suaves + texturas de
  ruido procedurales (triplanares en el terreno) que dan variación sin
  fotorrealismo; rugosidad por material, borde de luz (rim) sutil en la vegetación,
  subsuperficie en hojas.
- **Paletas por isla:** turquesa y arena en el Amaraje, verdes saturados en Esmeralda,
  negro y rojo en el Humo, gris y blanco en los Dientes, verde grisáceo y niebla en el
  Manglar, blanco y cian en Arenas Blancas, dorado y verde en la Meseta.
- **Ruinas:** basalto oscuro erosionado, musgo y líquenes, sin ornamento excesivo:
  respeto por delante de espectáculo.
- **Densidad:** la selva debe sentirse frondosa: capas de sotobosque, helechos,
  lianas, árboles medianos y dosel, con variación de escala, tono y rotación por
  instancia.
- **Criterio de calidad:** cada captura de referencia (§18) debe apetecer explorarla.
  Si no apetece, no pasa el hito.
- **Luz como protagonista:** Lumen, niebla volumétrica, rayos de sol entre hojas,
  atardeceres saturados, noches azules con estrellas y Vía Láctea.
- **Agua:** océano con oleaje por vértice, espuma en la orilla, caústicas,
  transparencia en someros, bioluminiscencia.
- **Vegetación viva:** viento en hojas por material, palmeras que se doblan en el
  ciclón, hierba que se aparta al pasar.
- **Mapa y UI dibujados a mano** con una tipografía manuscrita y trazos generados por
  código.

---

## 14. Audio y música

### 14.1 Efectos (síntesis procedural con Python)

Olas según la marea y el viento, viento por altura, lluvia sobre hojas, tela y techo,
truenos, fuego, pasos por superficie (arena, roca, hierba, madera, agua, barro),
golpes de herramienta por material, fauna marina y bandadas lejanas (cantos, aleteos
a distancia), bajo el agua, respiración y latido, radio y estática ambiental, UI.

### 14.2 Paisaje sonoro

Ambiente en capas por isla, hora, clima y altura: coro de insectos por la noche, aves
lejanas al amanecer y silencio antes del ciclón.

### 14.3 Música adaptativa

Banda sonora compuesta proceduralmente (marimba, kalimba, pads cálidos, flauta de
bambú, percusión suave, cuerdas sintéticas):

| Capa | Cuándo |
|---|---|
| Tema principal | Menú, momentos clave |
| Exploración diurna | Por isla, con variaciones instrumentales |
| Noche | Minimalista, con insectos |
| Descubrimiento | Motivo corto al encontrar un PdI o completar un tramo del mapa |
| Tensión | Depredador cerca, ciclón, hipotermia |
| Mar abierto | Navegación a vela |
| Final | Tema principal orquestado para la partida del barco «Limón» |

Transiciones por fundido y compás; la música nunca tapa el ambiente.

---

## 15. Interfaz y frontend

- **Menú principal** con la isla en tiempo real y la cámara sobrevolando el
  archipiélago: Continuar, Nueva partida (modo, semilla), Cargar, Ajustes, Logros,
  Créditos, Salir. La pantalla de inicio y los créditos llevan la dedicatoria «Para
  Almudena, mi Limón».
- **Pantalla de carga** que muestra la generación del archipiélago en forma de mapa.
- **Pausa:** Reanudar, Mapa, Museo, Modo foto, Ajustes, Guardar, Salir al menú.
- **Ajustes:**
  - *Gráficos:* resolución, modo de ventana, escala de render, TSR/DLSS, calidad
    general y por categoría, distancia de vegetación, límite de FPS, VSync, brillo y
    gamma.
  - *Audio:* maestro, música, efectos, ambiente, dispositivo de salida.
  - *Controles:* remapeo completo de teclado y ratón, soporte de mando (Xbox/PS),
    sensibilidad, invertir Y, mantener o alternar.
  - *Juego:* FOV, balanceo de cámara, duración del día, tamaño del texto, idioma
    (ES/EN).
  - *Accesibilidad:* modo daltónico, reducir movimiento, desactivar destellos, ayuda
    para apuntar, avisos visuales de sonido.
- **Guardado:** automático al dormir y en fogatas, 3 ranuras manuales y una copia de
  seguridad.

---

## 16. Logros (30)

Ejemplos: *Primer fuego*, *Tierra firme* (llegar a la 2.ª isla), *Cartógrafo* (mapa
completo de una isla), *Coleccionista* (museo con 10 tesoros expuestos), *Rey del
cocotero*, *Bajo el volcán*, *Luz en el agua*, *Ojo de ciclón* (sobrevivir a uno sin
daños en la base), *Wayfinder* (aprender las 5 técnicas de navegación), *El limonero*
(cosechar el primer limón), *Limón zarpa* (construir el barco y llegar a la isla
oculta), *Náufrago de verdad* (terminar en modo Náufrago), *Sin mapa* (llegar a la
isla oculta sin haber dibujado ninguna costa).

---

## 17. Arquitectura técnica

### 17.1 Principios

- **C++ como núcleo** (módulo `Explored`). Los Blueprints solo son datos generados
  por scripts de Python del editor; la lógica no vive en Blueprint.
- **Datos en tablas** (`UDataAsset` / `DataTable` generados desde JSON versionado):
  objetos, recetas, especies, recetas de cocina, eventos y sellos de mapa. Añadir
  contenido no requiere tocar código.
- **Subsistemas** con una responsabilidad cada uno, comunicados por delegados y
  `GameplayTags`.

### 17.2 Módulos

| Módulo | Responsabilidad |
|---|---|
| `WorldGen` | Semilla → islas, relieve, biomas, ríos, arrecifes, PdI; chunks de malla de terreno suave |
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
| `Farming` | Huerto, limonero, etapas de crecimiento por días |
| `Fauna` | Especies marinas y de bandada, IA, percepción, animación procedural |
| `Boats` | Balsa, canoas, vela, remo, barco «Limón», físicas de flotación |
| `Cartography` | Trazado de costa, bocetos de mirador, marcas y sellos, mapa persistente |
| `Ruins` | Marae, wayfinding, tesoros, catálogo del museo |
| `Narrative` | Puntos de interés ambientales, frases de una línea, eventos de mundo |
| `Audio` | Ambiente, música adaptativa, mezcla |
| `Save` | Semilla + deltas del mundo + jugador + progreso |
| `Settings` | Ajustes y persistencia |
| `UI` | Menús, pausa, mapa, museo, HUD mínimo (C++ y Slate/UMG por código) |

### 17.3 Pipeline de contenido (todo por código)

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

### 17.4 Rendimiento

- 60 FPS a 1080p en calidad Alta sobre una RTX 4060 Laptop; 30 FPS estables en Baja
  con gráficos integrados.
- Streaming por chunks (World Partition para lo fijo, chunks propios para el
  terreno), HISM y culling por distancia en la vegetación, IA de fauna con LOD de
  actualización.

---

## 18. Calidad y pruebas

- **Automation Tests (C++):** determinismo por semilla, garantías de PdI y
  navegabilidad, manos, mochila y peso, recetas, necesidades, integridad de
  construcciones, mareas, guardado y carga.
- **Pruebas funcionales en mapa:** escenarios automatizados (encender fuego, cruzar a
  nado, sobrevivir a una noche).
- **Verificación visual:** capturas automáticas desde cámaras fijas por isla y hora,
  revisadas en cada hito.
- **Perfilado:** Unreal Insights en cada hito con presupuesto de ms por sistema.

---

## 19. Hitos

Prioridad: **porción vertical** primero — la isla de inicio completa y pulida antes
de ampliar al resto del archipiélago.

| Hito | Contenido | Resultado jugable |
|---|---|---|
| **M0 Cimientos** | Proyecto C++, build por CLI, pipelines de Blender, audio e importación, tests | Proyecto que compila y un asset generado dentro |
| **M1 Porción vertical** | Isla del Amaraje completa: WorldGen de una isla, supervivencia básica, manos, refugio → cabaña, huerto y limonero, cartografía de esa isla, una ruina explorable, un tesoro expuesto | La isla de inicio 100 % jugable de principio a fin |
| **M2 Archipiélago** | WorldGen de las 7 islas, océano, cielo, día/noche, Luna, mareas, vegetación | Pasear y cartografiar todas las islas |
| **M3 Sobrevivir** | Necesidades completas, fuego, cocina, fabricación, construcción, clima | Aguantar 10 días en cualquier isla |
| **M4 Mar** | Balsa, canoas, vela, buceo, arrecife | Navegar entre islas |
| **M5 Vida marina** | Fauna marina y de bandada completa, pesca, trampas | Aguas habitadas |
| **M6 Ruinas y museo** | Marae, wayfinding completo, tesoros, catálogo, barco «Limón» | Objetivo final alcanzable |
| **M7 Sonido** | Efectos, ambiente, música adaptativa | Juego sonorizado |
| **M8 Frontend** | Menús, ajustes, guardado, logros, localización, accesibilidad | Producto completo |
| **M9 Pulido** | Equilibrado, rendimiento, bugs, empaquetado Win64 | Build listo para Steam |

---

## 20. Fuera de alcance

- Multijugador.
- Diálogos y doblaje de personajes: no hay personajes con voz ni subtítulos de
  conversación.
- Humanos y fauna terrestre animados en pantalla: ver §10 y §12.
- Cinemáticas pregrabadas: toda secuencia es jugable o diegética.
- Alta en Steamworks: depende del titular (cuenta y tasa de 100 USD). El proyecto
  entrega el build empaquetado, la página de tienda redactada y las capturas.
