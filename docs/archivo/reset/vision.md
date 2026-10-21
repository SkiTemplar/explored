> Obsoleto: sustituido por docs/v3/.

# Explored: documento de visión del reset (v2)

Estado: borrador para confirmación del autor · 2026-10-18 · rama `reset/v2` · UE 5.6, C++.

Este documento sustituye como visión rectora a `docs/diseno/gdd_v2.md` (sandbox de
transformación del terreno) y deja `docs/v1-alcance.md` como histórico. Lo que no se
menciona aquí no existe en el reset hasta que se decida lo contrario. Las decisiones del
autor están marcadas **[A]**; las propuestas de este documento que el autor debe confirmar
están marcadas **[P]** y se resumen en la sección 11.

> Dedicado a Almudena, mi Limón. (Se conserva la dedicatoria de la v1 en créditos y en la
> última página del cuaderno.)

---

## 1. Pitch y pilares

**Pitch.** Knight, piloto de correo aéreo, se estrella con el hidroavión Albatros en un
archipiélago sin nombre. Lemon, su esposa, no puede hablarle: solo le quedan doce cartas
selladas que ella le escondió en el equipaje, los recuerdos que se encienden en ciertos
lugares y objetos, y un cuaderno donde Knight le escribe lo que no puede decirle en voz
alta. Es un juego narrativo de supervivencia tranquila, de 3 a 10 horas, que se cuenta
sin HUD y con un único estilo low poly cartoon de colores planos.

### 1.1 Pilares

| # | Pilar | Qué significa en decisiones concretas |
|---|---|---|
| 1 | **El cuaderno es el mundo** | Toda la información y toda la interacción de «menú» pasa por un objeto físico que Knight sostiene. Nada flota sobre la pantalla. |
| 2 | **Volver es contar** | Cada sistema empuja hacia Lemon. La supervivencia da cosas que escribirle; el final depende de lo que el cuaderno contiene al llegar. |
| 3 | **Pocas islas, muy habladas** | Cinco zonas pequeñas y densas, con libertad de orden dentro de cada una, en vez de un archipiélago grande y vacío. |
| 4 | **Un solo estilo** | Una paleta, un material maestro, un validador. Si un asset no pasa el validador, no entra. El estilo es un pilar de producción, no un acabado. |

Referencia de tono: Firewatch (voz íntima, paisaje que consuela, soledad con una persona al
otro lado). Diferencia buscada: aquí la persona del otro lado no responde, y el jugador
tiene más verbos (cocinar, pescar, navegar, cuidar de un animal, dibujar el mapa).

### 1.2 Contradicciones con el GDD v2 que este reset resuelve explícitamente

| GDD v2 / v1 | Reset | Efecto |
|---|---|---|
| Pilar «transformar la isla» (minería, terraformado, muros) | Cortado | Sin minería, sin terreno modificable, sin muralla. |
| «Sin diálogos ni cinemáticas», historia mínima y ambiental | Historia explícita y dirigida | Knight habla y escribe; hay escenas cortas de recuerdo. |
| Cooperativo 2-4 jugadores | Cortado | Un jugador. |
| Pueblo vivo, comercio y piratas | Cortado | Sin humanos vivos salvo Knight y, al final, Lemon. |
| Libertad total sin objetivo | Libertad dentro de cada acto | Hay final obligatorio; el orden interno de cada isla es libre. |
| Mapa con tecla M, objetivo arriba a la derecha (v1) | Cuaderno sin HUD | Se retira el HUD completo. |
| Siete islas procedurales horneadas | Cuatro islas de kit más la revisita | El generador se extrae a un proyecto aparte (sección 11.3). |

---

## 2. Historia

### 2.1 Premisa y tema

Knight vuela correo entre islas para pagar la hipoteca del limonar de la casa. Lemon es
astrónoma aficionada y le enseñó a orientarse por las estrellas («si falla todo, sabes
leer el cielo»). Antes de su último vuelo discutieron: ella le pidió que dejara los vuelos
largos; él prometió que era el último. Lemon le escondió en el equipaje doce cartas
numeradas con una indicación en el sobre sobre cuándo abrirlas.

Tema: *volver no es llegar, es poder contar lo que pasó*. Knight ha ocultado miedos a
Lemon durante años por protegerla. El juego le obliga a decidir, carta a carta, si seguirá
haciéndolo.

### 2.2 Estructura en actos

| Acto | Zona | Título provisional | Qué ocurre | Cartas (fijas) | Pieza del Limón |
|---|---|---|---|---|---|
| Prólogo | Aire y costa | «El último vuelo» | Vuelo corto jugable, fallo del motor, caída. Se despierta en la playa con el cuaderno. | 0 | — |
| I | Isla 1, Naufragio | «Lo que flota» | Aprender a sobrevivir. Encuentra el equipaje y la carta 1. Recupera fuselaje y flotadores. Ve humo y otra isla. Construye la balsa. | 1, 2 | Casco |
| II | Isla 2, Marae | «Los que navegaban» | Ruinas de un pueblo antiguo de navegantes. Aprende su método de orientación por estrellas y olas. Primera noche en el mar. | 3, 4 | Brújula estelar |
| III | Isla 3, Faro | «Los que no volvieron» | Campamento de la expedición científica de 1974, con diarios. Una radio quemada: Knight intenta llamar y solo hay estática. Encuentra un limonero plantado por la expedición. Conoce al perro isleño si no lo conoció antes. | 5, 6 | Vela (tela de la tienda) |
| IV | Isla 4, Laguna | «La tormenta» | Ciclón. Se pierde algo (ver 2.5). Recupera la cola del Albatros en el manglar y la convierte en timón. Peor momento de Knight. | 7, 8 | Timón y mástil |
| V | Isla 1 revisitada | «El Limón» | Vuelve a la grada de la playa con las piezas, construye el barco, espera la noche y zarpa guiándose por el cielo. La carta 12 se abre en el mar. | 9-12 (opcionales 9-11) | Barco completo |
| Epílogo | Puerto de casa | «Lo que se cuenta» | Llegada al amanecer, camino al limonar, reencuentro con Lemon. Escena del cuaderno. | — | — |

Notas:

- El orden de las islas 2, 3 y 4 es **fijo en el hilo narrativo pero con una elección de
  rumbo inicial** [P]: desde la isla 1 se ven dos humos; el jugador puede ir primero a
  Marae o al Faro. El Acto IV siempre es el tercero. Cuesta una variante de dos
  escenas de transición y ninguna isla extra.
- Si se cambia el orden II/III, la carta 3-4 y 5-6 se intercambian por zona, no por orden
  (cada carta lleva en el sobre una condición de lugar, no de fecha).
- La isla 1 se revisita en el Acto V con cambios: clima distinto, hierba crecida, marcas
  del jugador en el mapa, y el perro si lo trajo.

### 2.3 Lemon: presencia sin contacto

Reglas de diseño (invariantes):

1. **Lemon no se ve nunca [A]**: ni en los recuerdos ni en el epílogo, ni en silueta. Los
   recuerdos la cuentan por su ausencia: su taza, su sombrero en la silla, su letra, la
   puerta entreabierta del limonar, el sonido de la casa.
2. No hay radio operativa. La radio del Faro (Acto III) es un momento deliberado: Knight
   intenta llamar, solo hay estática, y el jugador debe cerrar la tapa.
3. Todo lo que Lemon aporta tiene forma física: sobre, papel, frase en una pared, objeto.

### 2.4 Dosificación de cartas y recuerdos

**Cartas de Lemon (12).** Sobres cerrados en la mochila que se ven en la página de cartas.
Cada sobre indica en su exterior, con letra de Lemon, cuándo abrirlo («Abre esta cuando
tengas hambre de verdad», «Abre esta cuando hayas hecho tu primer fuego»). El juego la
habilita cuando se cumple la condición; Knight puede abrirla antes pero se anota «la abrí
antes de tiempo» y se pierde una frase final de la carta (que se recupera releyéndola
cuando llega la condición).

| Carta | Condición del sobre | Tono | Contenido narrativo |
|---|---|---|---|
| 1 | Primer anochecer en tierra | Ternura | Cómo se conocieron (el limón en el mercado). |
| 2 | Primer fuego encendido | Humor | Lista de cosas que Knight olvida siempre. |
| 3 | Primera vez que se ve el mar abierto desde la balsa | Miedo en calma | Lemon le enseña la Osa Mayor; instrucciones para encontrar el norte. |
| 4 | Al reconocer un patrón de las ruinas | Admiración | Su infancia mirando el cielo con su padre. |
| 5 | Al leer el primer diario de la expedición | Duda | La discusión previa vista desde ella. |
| 6 | Al plantar o regar el limonero | Esperanza | La casa, el limonar, la deuda. |
| 7 | En plena tormenta (Acto IV) | Urgencia | «No te pido que seas valiente, te pido que vuelvas.» |
| 8 | Tras el punto bajo (desmayo o pérdida) | Dolor | Ella también tuvo miedo; no se lo contó. |
| 9 | Opcional: una comida compartida con el perro | Humor | La anécdota del gato. |
| 10 | Opcional: cuando se escribe una carta honesta | Cercanía | Le pide que le cuente cosas feas. |
| 11 | Opcional: cuarto desmayo o más | Ternura dura | Una promesa sin condiciones. |
| 12 | Primera noche de mar con el Limón | Cielo | El cometa que quiere enseñarle; instrucciones finales de navegación. |

Las cartas 1-8 y 12 son obligatorias (9 en total). 9-11 solo se abren si se cumple su
condición, y alimentan las variantes del final. Todas se leen en el cuaderno con la letra
de Lemon; locución de actriz como **mejora posterior**, no requisito de hito.

**Recuerdos (32).** Escenas de 15-40 s, sin diálogo largo, a color paleta alternativa
(más cálida), con ambiente sonoro y una frase escrita en el cuaderno al terminar.
Se disparan al mirar o coger un objeto o al entrar en un lugar durante 3 s (dispara una vez).

| Tipo | Cantidad | Cómo se encuentran | Ejemplo |
|---|---|---|---|
| Principales | 10 | En el camino crítico, inevitables | Tostadora del equipaje; cuaderno de vuelo; limonero de la expedición. |
| Opcionales | 22 | Exploración, objetos y vistas concretas | Una concha igual a la de su boda; un amanecer sobre el arrecife. |

Dosificación objetivo: un evento de Lemon (carta o recuerdo) cada **6-8 min** de partida
a ritmo speedrun (≈ 25 eventos en 3 h) y no más de uno cada **4 min** en partida tranquila.
Cooldown mínimo de 90 s entre eventos para que respiren. Un recuerdo opcional no
bloquea ninguna pieza de la historia.

### 2.5 Decisiones del jugador que cambian el tono o el final

El final siempre es el reencuentro **[A]**. Lo que varía es lo que Knight lleva en las
manos y cómo se dice. Hay cuatro palancas, todas diegéticas.

| Palanca | Dónde se decide | Valores | Efecto |
|---|---|---|---|
| **Cartas a Lemon** | 9 noches de escritura (una al final de cada tramo; ver sección 7) | Cada carta se compone eligiendo 3 de 5 frases por tarjeta. Cada frase cuenta como Honesta (H) o Protectora (P) | Puntuación H/P acumulada; determina el tono final |
| **Desmayos no ocultados** | Al escribir tras un desmayo, el jugador elige contarlo o callarlo | Contado / Callado | +1 H o +1 P |
| **El perro** | Acto III en adelante | Cuidarlo y llevarlo / dejarlo | Con perro: epílogo con perro y cambio en una escena del mar. Sin perro: epílogo más íntimo y silencioso. No hay muerte del animal. |
| **La pérdida del Acto IV** | Durante la tormenta | Salvar el limón (la planta del limonero) o salvar el instrumento de navegación (la brújula estelar) | Salvar la planta: la navegación final es más difícil (sin brújula, solo estrellas). Salvar la brújula: más fácil, pero el limón del epílogo es el comprado en el mercado. Ambas rutas son completables. |

**Final único [A].** Knight llega al limonar al amanecer y la puerta de casa se abre; antes de
que se vea a nadie, fundido a blanco. La última página del cuaderno aparece escrita con la
letra de Lemon: responde a lo que Knight le escribió durante el viaje. Las palancas de arriba
no cambian el desenlace; solo cambian el contenido de esa página (qué cartas cita, si
menciona los desmayos que Knight contó o calló, el perro y el limón). El contador H/P es
interno y nadie lo ve.

---

## 3. Estructura del mundo

Cuatro zonas nuevas más una revisita. Cada zona es una isla pequeña (≈ 0,6-1,0 km² jugable,
3-6 min de punta a punta a pie) con una identidad visual por paleta de bioma.

| Zona | Rol narrativo | Rasgo jugable único | Fauna principal | Verbo nuevo que enseña |
|---|---|---|---|---|
| 1 Naufragio | Acto I y V | Playa, bosque de palmas, pecio | gallina silvestre, cangrejo de los cocos, fragata | moverse, recoger, fuego, cuaderno |
| 2 Marae | Acto II | Ruinas con patrones astronómicos | tortuga marina, fragata | leer el cielo y las olas |
| 3 Faro | Acto III | Campamento Halden, faro, limonero | perro isleño, jabalí | cuidar, cocinar y regar |
| 4 Laguna | Acto IV | Manglar y laguna, ciclón | tiburón de arrecife, cangrejo | refugiarse, remar con mal tiempo |
| Mar | Travesías | Cruces de 4-8 min | delfín, fragata, peces | rumbo con brújula; estrellas |

### 3.1 Ritmo de 3 h (speedrun) y de 10 h (con calma)

| Acto | Speedrun | Calma | Qué se añade en calma |
|---|---|---|---|
| Prólogo | 0:05 | 0:08 | Vuelo libre unos minutos |
| I Naufragio | 0:35 | 1:50 | Pesca, cocina, mapa completo, 5 recuerdos opcionales |
| II Marae | 0:30 | 1:50 | Todos los patrones, petroglifos, tortugas |
| III Faro | 0:35 | 2:00 | Diarios completos, cuidar el perro, limonero |
| IV Laguna | 0:30 | 1:30 | Supervivencia con mal tiempo, pecio hundido |
| V El Limón | 0:35 | 1:25 | Construcción cuidada, travesías, mapa de estrellas |
| Epílogo | 0:10 | 0:15 | |
| **Total** | **2:40-3:00** | **9:00-10:00** | |

Regla anti-relleno: el contenido de calma es siempre opcional y siempre produce algo que
Knight puede escribir (recuerdo, anotación de fauna, carta opcional). Si no produce una
entrada del cuaderno, no se hace. No hay fetch de repetición ni contadores de recolecta.

El speedrun se apoya en: ruta crítica corta señalizada por el paisaje (humo, luz del faro,
sendero de huellas), construcción con cada pieza ya localizada, supervivencia que no
bloquea mientras se mantenga una comida y una bebida cada ≈ 15 min.

---

## 4. Bucle de juego

### 4.1 Minuto a minuto

1. **Mirar.** Un punto de interés se ve desde lejos (humo, reflejo, ave posada, cresta).
2. **Ir.** Atravesar terreno, con la supervivencia como textura: sed, calor, cuesta.
3. **Interactuar.** Recoger, abrir, cavar con la mano, observar. Las interacciones son
   mantener pulsado (0,4-1,5 s) con animación visible.
4. **Anotar.** Knight escribe solo una línea breve en el cuaderno o el jugador abre el
   cuaderno para dibujar el lugar (sección 7).
5. **Disparo.** Un recuerdo o una carta se activa de vez en cuando (sección 2.4).

### 4.2 Por sesión (≈ 60-90 min)

| Fase | Duración | Contenido |
|---|---|---|
| Retomar | 2-3 min | Al cargar, Knight lee en voz baja la última entrada del cuaderno. |
| Objetivo del día | 15-25 min | Salir de la base, ir al lugar, resolver la zona. |
| Aprovisionar | 5-10 min | Comida, agua, fuego antes del anochecer. |
| Noche | 5-8 min | Fuego, descanso, **escritura de carta** (cada 2-3 sesiones), carta de Lemon si toca. |
| Cierre | 1 min | Knight cierra el cuaderno; guardado automático de la noche. |

El ciclo de día/noche dura **40 min** (valor v1 conservado) [P: contrastar con la
duración elegida para que cada noche coincida con el cierre de sesión]. La noche es el
momento emocional del juego; el día es el momento del mundo.

### 4.3 Enseñanza sin HUD

La enseñanza se hace con el mundo y con Knight, no con tutoriales de texto:

| Concepto | Cómo se enseña |
|---|---|
| Cuaderno | Primer plano obligado en la playa: Knight lo encuentra, lo abre, escribe su nombre. Se vuelve a pedir una vez por cada página nueva. |
| Interacción | El objeto interactuable brilla con un pulso de dos tonos y Knight gira la cabeza hacia él. |
| Sed | La primera sed aparece a los 5 min; hay un coco a la vista. |
| Fuego | Carta 2 sugiere hacerlo; el cuaderno dibuja el esquema. |
| Navegar | Carta 3 y las ruinas de Marae: paso a paso en dibujos. |
| Controles | Pantalla de ajustes y una hoja de «controles» pegada al interior de la tapa del cuaderno. |

Accesibilidad: subtítulos y modo de contraste del cuaderno como **ajustes** (no HUD): están
en el menú de pausa, que sí es una pantalla.

---

## 5. Mecánicas núcleo (lista cerrada: 7)

Escala de coste: S ≈ días (1 persona-semana o menos), M ≈ 2-4 semanas, L ≈ 1-2 meses.
Las estimaciones son para ejecución con el pipeline actual (modelos puros + Automation Spec
+ scripts de Blender).

| # | Mecánica | Por qué es original o vistosa | Coste |
|---|---|---|---|
| 1 | **Cuaderno físico** | Un objeto 3D único con páginas que se pasan; el mapa se dibuja con tinta y se corrige con el tiempo; los sobres se pegan; todo está iluminado por la fogata o la luna. Es el menú, el HUD y el final. | L |
| 2 | **Cuerpo legible** | El estado del cuerpo se lee en animación, audio, postproceso y frases; no hay barras. Dar al jugador una impresión clara del estado sin números es el reto. | M |
| 3 | **Recuerdos de Lemon por objeto** | Un objeto concreto cambia la paleta del mundo a una cálida y muestra una escena silueteada de 15-40 s. Pequeño, repetible, sin cinemática. | M |
| 4 | **Cartografía a mano** | El mapa solo se rellena por avistamiento y por el dibujo del jugador (marcar tres referencias desde un punto alto y cerrar el trazo). Mapa útil porque es tuyo. | M |
| 5 | **Fauna con relación** | Animales con rutina y reacción al jugador; uno es compañía real (perro). No hay combate: huir, esquivar, alimentar. | L |
| 6 | **Navegación a vela y por estrellas** | Travesías cortas con balsa (viento, corriente) y la travesía final por constelaciones reales dibujadas en el cuaderno, que el jugador compara con el cielo. | L |
| 7 | **Construcción por piezas del Limón** | Un único objeto grande armado por encaje de 4 piezas importantes en la grada, con animación de martillo, sin construcción libre. Aprovecha el kit del Limón de la v1. | M |

Fuera de la lista de 7, soportes necesarios y baratos: fuego y cocina (S, parte de 2),
pesca simple (S, parte de 5), recogida (S), guardado por noche (S), pausa y ajustes (S).

### 5.1 Detalle de las mecánicas

**1. Cuaderno.** Ver sección 7.

**2. Cuerpo legible: parámetros de primera pasada.** Cuatro variables, 0-100, con cuatro
estados. Valores permisivos: la supervivencia es textura, no castigo (no hay game over).

| Variable | Baja de 100 a 0 sin atención | Umbrales de estado (>60 bien / 35-60 leve / 15-35 marcado / <15 grave) |
|---|---|---|
| Sed | 30 min de juego | Por las razones de abajo |
| Hambre | 80 min de juego (2 días en reposo) | |
| Energía | 18 h de juego en vigilia (≈ 27 min por día de 40) | |
| Temperatura | Depende del clima: noche sin abrigo o fuego -20/h | |

Razones: la sed es la señal más rápida porque lo que motiva el mundo es ir a por agua;
el hambre es lenta porque la cocina es un lujo agradable, no un bloqueo.

| Estado | Lectura sin HUD |
|---|---|
| Sed leve | Se pasa la lengua por los labios; voz algo ronca. |
| Sed marcada | Jadeo; el sonido pierde agudos; bordes del encuadre se resecan (viñeta amarillenta). |
| Sed grave | Camina más lento; temblor de mano al coger; Knight dice «agua» en voz baja. |
| Hambre marcada | Sonido de estómago; se lleva la mano al vientre; el mundo pierde un 10 % de saturación. |
| Frío | Aliento visible, brazos cruzados, castañeteo; en la fogata el rostro se relaja. |
| Calor | Camisa abierta; sudor visible; el aire ondula. |
| Cansancio | Bostezo; el encuadre se inclina medio grado; parpadeo lento de arriba abajo. |
| Desmayo (cualquier variable < 5) | Fundido; amanece en el punto de sombra más cercano; se ha perdido medio día y se han soltado 2 objetos no clave; se escribe una entrada. |

Reglas: sin muerte; sin pérdida de objetos clave (cartas, brújula, piezas del Limón);
máximo un desmayo cada 20 min de juego (invulnerabilidad de 20 min tras despertar).
Cada desmayo añade +1 a la puntuación H o P según el jugador lo cuente (sección 2.5).

**3. Recuerdos.** Disparo con 3 s de atención sostenida a un objeto o lugar. Visual: la
paleta se sustituye por la paleta cálida del recuerdo; Lemon no aparece, se la intuye
por objetos y sonido de la escena; duración fija; no se pueden saltar la primera vez y
se pueden saltar después. Hay un indicador diegético de que una zona tiene recuerdo cerca:
Knight se detiene, toca el objeto y las gaviotas se callan 2 s.

**4. Cartografía.** Dos fuentes: **avistamiento** (la línea de costa y puntos de interés
entran al mapa cuando se ven desde un punto alto) y **dibujo** (el jugador marca tres
puntos de referencia desde un lugar fijo y el mapa traza la zona entre ellos). Sin
GPS, sin punto del jugador: el jugador se localiza comparando el mapa con lo que ve.
Para evitar frustración: hay un punto de partida «Estoy aquí» que se actualiza solo al
tocar una pluma colocada en el suelo (hito: boyas y mojones que el jugador planta).

**5. Fauna.** Ver sección 8.

**6. Navegación.** Dos fases:

- *Travesías cortas (Actos II-IV):* balsa, 4-8 min, un solo mando de vela (orientación) y
  uno de remo; el viento se ve en las velas y los pájaros. No hay carta de navegación
  numérica: se sigue una referencia visible (humo, faro).
- *Travesía final (Acto V):* 15 min, de noche, sin tierra a la vista. El cuaderno tiene
  dibujadas tres constelaciones de Lemon; el jugador gira el cuaderno hasta que el dibujo
  casa con el cielo (ángulo ±8°) y entonces fija rumbo. Si se desvía > 20° durante > 90 s,
  Knight lo dice y el cielo marca la corrección (una estrella parpadea). Nunca hay naufragio
  ni fallo: el coste de equivocarse es tiempo, y la carta 12 se abre cuando se cumple el
  tercer alineamiento.

**7. Limón.** Grada fija en la playa de la isla 1. Cada pieza llega con un recuerdo
asociado. Se instala con un gesto de 4 s y un golpe de martillo. Sin inventario de
materiales intermedios: las piezas ya vienen casi listas.

---

## 6. El mundo hecho de un mismo material

Antes del cuaderno, un resumen de reglas de contenido (detalle en sección 9):

- Cada isla tiene 12-18 tipos de objetos únicos y reutilizados.
- Cada objeto del mundo tiene ≤ 800 triángulos; los árboles ≤ 400; Knight ≤ 3.000.
- El mundo se construye con un kit de 90-120 mallas, no con terreno procedural.

---

## 7. Cuaderno sin HUD

### 7.1 Apertura y uso

- **Abrir:** mantener `Tab` (mando: botón superior) 0,3 s. Knight saca el cuaderno de la
  chaqueta con la mano izquierda y lo sostiene frente al pecho. Cámara en primer plano
  parcial, el mundo se desenfoca (profundidad de campo) y baja a 0,8 de saturación.
- **El mundo no se pausa.** El tiempo continúa a velocidad 1. Abrirlo en medio de un peligro
  es una decisión, no una pausa gratis. Para jugadores que lo prefieran, ajuste
  «tiempo lento con el cuaderno» (×0,2) en menú.
- **Pasar página:** `A`/`D` o gatillos; las páginas se animan con una curva de 0,35 s.
- **Cerrar:** soltar o pulsar de nuevo; Knight lo guarda.
- **Escribir:** donde el jugador escribe (cartas a Lemon, marcas en el mapa) se hace con
  una pluma que Knight sostiene en la derecha; el trazo aparece con animación de tinta.

### 7.2 Páginas

| # | Página | Qué es | Quién la escribe | Cómo se ve |
|---|---|---|---|---|
| 1 | **Hoy** | Diario del día: fecha de la isla, clima en un dibujo, 3-5 líneas | Se escribe sola cuando pasa algo (llegar, comer, recuerdo) | Letra de Knight con tachones. Sobrio. |
| 2 | **Mochila** | Inventario | Se rellena con dibujos de lo que se lleva | Cuadrícula dibujada de 12 casillas más manos |
| 3 | **Mapa** | Mapa de la isla actual y del mar | Avistamiento (auto) y marcas del jugador | Tinta sobre papel; color de acuarela plano por bioma |
| 4 | **Naturaleza** | Fauna y flora observada | Auto al observar 3 s o interactuar | Un dibujo y dos líneas por especie |
| 5 | **Planos** | Esquema del fuego, la balsa, el Limón | Se desbloquea con cada pieza | Dibujo de montaje con cotas |
| 6 | **De Lemon** | Sobres | Se rellenan solos al recibir una carta | Sobre cerrado con la letra de Lemon; abierto, la carta |
| 7 | **Para Lemon** | Carta a escribir | El jugador compone la carta eligiendo frases | Hoja con tarjetas de frases |

### 7.3 Qué se escribe solo y qué escribe el jugador

| Lo escribe solo | Lo escribe el jugador |
|---|---|
| Entradas de «Hoy» por eventos | Cartas a Lemon (elige 3 de 5 frases por tarjeta) |
| Fauna al observarla | Marcas y nombres en el mapa (hasta 20 marcas por isla, 4 símbolos) |
| Planos al conseguir una pieza | Cuántas noches escribir (opcional; pero hay 9 cartas que afectan al final) |
| Frases de recuerdo | Nombre del perro (lista de 8 nombres + libre) |
| Resúmenes de sed/hambre/cansancio en «Hoy» | Decisión de contar o callar un desmayo |

### 7.4 Inventario

- Capacidad: **12 casillas** (stack de 5 para comida y fibras; 1 para herramientas) más
  **2 manos**: una con objeto grande o antorcha y otra con herramienta.
- Lo que Knight lleva se ve en él: cantimplora al cinturón, machete, mochila, antorcha en
  la mano. Si está sin mochila porque la dejó en el campamento, no puede abrir la
  página 2 (la tapa está vacía).
- Selección rápida: rueda de objeto en mano (sin menú): un anillo corto de los 3 objetos
  de cinturón (machete, antorcha, cantimplora) que se ve **en el cuerpo** de Knight.
- Peso: no existe. Límite por casillas, sin gestión de peso.

### 7.5 Mapa, cartas y lo que el cuaderno no hace

- El mapa no tiene icono del jugador salvo el mojón (sección 5.1 mecánica 4).
- Las cartas de Lemon se leen en una página con el sobre a la izquierda y el texto a
  la derecha. El sonido de papel y, en la noche, la luz de la fogata.
- El cuaderno **no tiene** lista de misiones ni brújula de objetivos. El único «objetivo»
  son las frases de Knight en la página «Hoy» («Creo que el humo viene del este»).

---

## 8. Animales (9 especies)

| Especie | Rol | Comportamiento | Interacción | Zona |
|---|---|---|---|---|
| **Perro isleño** (único) | Compañía y narrativa | Sigue al jugador a 3-6 m; duerme junto al fuego; se asusta de la tormenta | Acariciar, dar de comer, nombrar; encuentra objetos enterrados y marca recuerdos | 3, luego todo |
| **Gallina silvestre** | Recurso | Pasta en grupo; huye a 6 m | Huevos y carne; puede domesticarse con granos | 1 |
| **Cangrejo de los cocos** | Recurso y peligro leve | Nocturno; roba objetos pequeños | Se esquiva; se captura con trampa; carne rica | 1, 4 |
| **Jabalí** | Peligro y recurso | Embiste si se le acerca mucho (< 4 m) | Se evita: desmayo no, solo derribo con pérdida de 1 objeto | 3 |
| **Tortuga marina** | Narrativa | Aparece en la orilla de noche; indica dirección del mar abierto | Observarla da un recuerdo opcional | 2 |
| **Fragata** | Narrativa y señal | Vuela alto sobre la costa; indica tierra cercana | Seguirla revela la siguiente isla en el mapa | todas |
| **Tiburón de arrecife** | Peligro | Patrulla el borde de la laguna; se aleja con luz | Hay que remar lejos de la boca; sin muerte (vuelca la balsa: pierdes 1 objeto flotante) | 4 |
| **Delfines** | Compañía en el mar | Acompañan la balsa y el Limón; salto en la travesía final | Solo mirar; marcan el rumbo correcto en el tramo medio de la noche | Mar |
| **Peces** (grupo) | Recurso | Bancos que se ven en la luz del día | Pesca con lanza o caña, minijuego de 3 s | 1, 2, 4 |

Reglas comunes: sin combate; animales amenazantes quitan tiempo u objetos, no vida;
cada especie tiene una sola animación de reacción clara; máximo 12 animales activos
simultáneos por zona, LOD de comportamiento a 40 m [P]. El perro no puede morir ni
perderse; si el jugador lo deja, lo ve en la orilla desde el barco.

---

## 9. Dirección de arte

Una sola fuente de estilo: `Content/Style/` con la paleta maestra, el material maestro y
el validador. Todo asset, propio, procedural o de terceros, pasa por ahí.

### 9.1 Reglas

| Elemento | Regla |
|---|---|
| **Paleta** | Una paleta maestra de **48 colores** (6 familias de 8 tonos: tierra, arena, verde, agua, cielo/lila, acentos cálidos). Las paletas de bioma son subconjuntos de 20-24 colores. La paleta de recuerdo es un subconjunto cálido propio. |
| **Tonos por objeto** | Máximo 3 colores de paleta por prop (base, sombra, acento); 4 para héroes (cuaderno, barco, Knight). Vegetación: 2 verdes + 1 tronco. Terreno: ≤ 5 por bioma. |
| **Texturas** | Prohibidas texturas de detalle, fotográficas, pintadas a mano o de ruido. Se permite **una** textura única: la hoja de paleta (64×16 px, filtro punto) a la que las mallas acceden por UV o por color de vértice. |
| **Geometría** | Facetas visibles (normales planas / suavizado por grupos solo en esferas pequeñas). Props 50-800 tris, árboles ≤ 400, personajes ≤ 3.000. |
| **Sombreado** | Material maestro `M_Toon` con 2 bandas (luz / sombra) sobre luz solar única; sombra proyectada en tono de paleta, no negra; ambiente tintado por la paleta del momento del día. Sin especular ni normal maps. Sin GI dinámica (Lumen apagado salvo decisión técnica en contra) [P]. |
| **Contorno** | **No** en el mundo. Solo hay trazo de tinta en el cuaderno (2D). Interactuables: pulso de luz de dos tonos al mirarlos, nunca contorno. |
| **Postproceso** | Viñeta suave, bloom bajo, LUT por momento del día construido desde la paleta, desenfoque de campo solo cuando el cuaderno está abierto. Sin grano, sin aberración, sin SSAO, sin SSR. |
| **Cielo** | Degradado vertical de 4 paradas por momento del día (amanecer, día, atardecer, noche, tormenta); sol y luna como discos planos; nubes de 5 formas low poly en 2 tonos; estrellas como puntos de 3 tamaños con constelaciones **astronómicamente correctas** (son mecánica). |
| **Agua** | Superficie facetada low poly (triángulos ≈ 1,5 m) con color plano en 3 profundidades (orilla, medio, profundo) por atributo de vértice; oleaje por desplazamiento de vértices; sin reflejos, sin refracción, sin espuma por textura. |
| **Orilla** | Banda de arena mojada de un tono más oscuro (≈ 0,5 m); línea de espuma como tira de geometría blanca de dos escalones que avanza y retrocede en 3 pasos. |
| **Luz** | Una luz direccional, un cielo, luces puntuales solo para fuego y linternas. Sombras con resolución fija. |
| **Cuaderno** | Es la excepción de lenguaje: papel y tinta, pero con la misma paleta; trazos vectoriales y sin fotografía. |

### 9.2 Validación

- `Tools/Style/check_style.py` (a crear): rechaza mallas con más de N materiales, colores
  fuera de la paleta (ΔE > 2), texturas distintas de la hoja de paleta, o triángulos fuera de
  presupuesto. Se ejecuta en CI y en el export de Blender.
- Todo recurso de terceros (Quaternius, Poly Pizza) se **remapea** a la paleta con un
  script que asigna el color de paleta más cercano; si no pasa, se descarta.
- Lámina de comparación (misma escena, antes/después del cambio de paleta) para revisión
  visual de Rodrigo en cada hito de arte.

### 9.3 Prohibido

- Texturas fotográficas, pintadas o de detalle, normal maps, mapas de rugosidad.
- Materiales de color plano **sin paleta** (cualquier RGB suelto).
- Contornos en el mundo, cel-shading con 3 o más bandas, degradados sobre mallas.
- Mezclar mallas lowpoly de packs distintos sin remapeo.
- Reflejos de pantalla, espejos, SSAO, DoF cinematográfico fuera del cuaderno.
- Efectos de partículas con sprites fotográficos (humo, fuego): se usan formas low poly
  animadas en dos tonos.
- Cualquier elemento de interfaz sobre la pantalla durante el juego.
- Fuentes distintas de la letra de Knight y la de Lemon en el cuaderno.

---

## 10. Fuera de alcance

| Cortado | Motivo |
|---|---|
| Multijugador y cooperativo | Rompe la intimidad del tono. |
| Combate y armas | Los peligros son espaciales, no de daño. |
| Minería, terraformado, muralla, construcción libre | Pilar eliminado. |
| Granja, cría, estaciones | Complejidad sin retorno narrativo. |
| Humanos vivos, aldeas, comercio, piratas | Soledad con una sola voz ausente. |
| Más de 4 islas nuevas + revisita | Alcance. |
| Mapa generado de forma procedural | El mundo es un kit curado. |
| HUD de cualquier tipo, minimapa, marcador de objetivos | Pilar 1. |
| Locución completa (voces) | Mejora posterior a v2.0 (cartas de Lemon y algunas frases de Knight). |
| Modding, DLC, tienda, logros en el primer lanzamiento | Alcance. |
| Soporte de Mac/Linux/consolas | Solo Windows. |
| Cambio de personaje, flashback jugable | Un solo protagonista. |
| Física compleja de agua y de barco hidrostático | La navegación es dirigida y tolerante. |

---

## 11. Riesgos, orden de producción y decisiones abiertas

### 11.1 Riesgos

| # | Riesgo | Prob. | Impacto | Mitigación |
|---|---|---|---|---|
| R1 | El estilo vuelve a salir incoherente | Alta | Alto | Hito H0: paleta, material y validador **antes** de cualquier isla. Nada entra sin pasar `check_style.py`. |
| R2 | El cuaderno es ilegible o pesado de usar | Media | Alto | Prototipo H1 con las 4 páginas esenciales; test con 5 jugadores sin explicación previa. |
| R3 | El mundo horneado de la v1 (siete islas, texturas) no sirve | Alta | Medio | Asumir rehacer: 4 islas nuevas de kit. Reaprovechar solo lo que pase el validador. |
| R4 | Supervivencia aburre o frustra | Media | Medio | Valores permisivos; sin game over; cada recurso conecta con un texto del cuaderno. |
| R5 | Escritura: 12 cartas + 32 recuerdos + ~100 frases | Alta | Alto | Redacción asistida y revisión del autor por actos; las cartas opcionales (9-11) y 12 de los 22 recuerdos son recortables. |
| R6 | 10 h sin relleno | Media | Alto | Regla anti-relleno (sección 3.1): todo opcional produce una entrada de cuaderno. |
| R7 | Navegación por estrellas poco clara | Media | Medio | Ángulos tolerantes, ayuda de estrella parpadeante, travesía de prueba en Marae. |
| R8 | Fauna con IA cuesta más de lo previsto | Media | Medio | Máquinas de estado simples; el perro es lo único complejo. |
| R9 | Probar sin HUD es difícil | Media | Bajo | Tomas automáticas (`Tools/shots.ps1`) y registro de estado del cuerpo en logs de test. |

### 11.2 Orden por hitos

Cada hito tiene criterio de salida medible; ninguno empieza sin que el anterior cumpla el suyo.

| Hito | Contenido | Criterio de salida |
|---|---|---|
| **H0 Estilo** | Paleta de 48, `M_Toon`, hoja de paleta, `check_style.py`, playa + agua + cielo, Knight, 10 props, lámina de revisión | 0 errores del validador; Rodrigo aprueba una lámina |
| **H1 Vertical slice** | **Isla 1 completa con el Acto I** (30-40 min): prólogo corto, cuaderno (Hoy, Mochila, Mapa, De Lemon), cuerpo legible (sed, hambre, cansancio), fuego, 2 cartas, 4 recuerdos, 2 especies (gallina, cangrejo), balsa | 60 fps en el equipo de referencia; 5 jugadores completan la zona sin explicación y sin HUD; mismo estilo en todos los assets |
| **H2 Núcleo** | Cuaderno completo, escritura de cartas, desmayo, guardado por noche, perro, cartografía | Todas las mecánicas 1-5 jugables en la isla 1; tests de modelos puros en verde |
| **H3 Marae + travesías** | Isla 2, balsa, navegación corta, brújula estelar, cartas 3-4 | Una travesía de 5 min sin atascos; Actos I y II de principio a fin |
| **H4 Faro y Laguna** | Islas 3 y 4, ciclón, pérdida elegible, perro completo, radio muda | Juego completo hasta el Acto IV; 3 caminos de decisión verificables |
| **H5 Limón y final** | Isla 1 revisitada, construcción, travesía final, epílogo con la página de Lemon | Partida entera en ≈ 3 h de speedrun grabado |
| **H6 Pulido** | Audio, música, locución opcional, accesibilidad, rendimiento | Prueba en otro PC y con mando; 10 h de calma medidas con 2 jugadores |

El vertical slice es **la isla 1 con su acto**, porque valida estilo, cuaderno, cuerpo
legible y tono con un coste bajo (y es la primera isla que se construye con el kit nuevo).

### 11.3 Decisiones del autor (2026-10-18)

1. Final único; **Lemon no se ve nunca**. La variación está en la última página del cuaderno.
2. Pérdida elegible en el Acto IV (limonero o brújula estelar); el perro no muere ni se pierde.
3. Islas nuevas de kit hechas a mano. El generador procedural de archipiélagos de la v1
   (WorldGen, horneado) **se extrae a un proyecto aparte** y sale de Explored.
4. El cuaderno no pausa el mundo (ajuste opcional de tiempo lento ×0,2).

Siguen abiertas: el orden I → (II o III) → IV, el día de 40 min y Lumen apagado.
