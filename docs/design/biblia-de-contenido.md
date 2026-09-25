# EXPLORED — Biblia de contenido

Versión 1 · 2026-09-26 · Complementa el GDD
(`docs/superpowers/specs/2026-09-26-explored-design.md`).

Este documento define **qué hay en el mundo y cómo se combina**: objetos, materiales,
combinaciones, caza, supervivencia, estaciones y temporales, y las reglas que evitan
que el juego sea aburrido o abrumador.

---

## 1. Filosofía: mucho contenido, poca carga mental

El objetivo son **más de 400 objetos distintos** sin que el jugador tenga que memorizar
una wiki. Se consigue con cuatro ideas:

1. **Los objetos tienen propiedades, no recetas.** Un palo es *largo* y *rígido*; una
   lasca de obsidiana tiene *filo* y *punta*; una liana *ata*. Cualquier combinación que
   tenga sentido físico funciona. El jugador razona como un náufrago real («necesito
   algo largo con punta»), no como alguien que consulta un libro de recetas.
2. **El resultado hereda de sus piezas.** Una lanza de bambú con punta de obsidiana es
   ligera, afilada y frágil; una de madera dura con punta de hueso es pesada y duradera.
   Pocas plantillas generan cientos de variantes con nombre, aspecto y estadísticas
   propias.
3. **El mundo enseña.** Cada isla introduce sus materiales, cada estación sus
   problemas, cada necesidad su solución. Nunca se presentan más de tres conceptos
   nuevos a la vez.
4. **El personaje tiene ideas.** Al sostener objetos que se podrían combinar, el
   personaje piensa en voz baja («esto podría servir de mango…») y en el diario aparece
   un boceto tenue. Es una pista, no una solución.

---

## 2. Sistema de combinación natural

### 2.1 Propiedades de los objetos

Cada objeto declara un conjunto de propiedades con un valor de 0 a 5.

| Propiedad | Significado | Ejemplos altos |
|---|---|---|
| **Filo** | Corta | Obsidiana, lasca de pedernal, aluminio afilado, concha rota |
| **Punta** | Perfora | Espina, hueso aguzado, obsidiana, clavo |
| **Largo** | Alcance, palanca | Palo, bambú, rama, tubo del avión |
| **Rígido** | No se dobla | Madera dura, hueso, aluminio |
| **Flexible** | Se curva y vuelve | Bambú fino, rama verde, tendón |
| **Contundente** | Golpea | Piedra, coco, madera dura, llave inglesa |
| **Ata** | Une piezas | Liana, fibra, cuerda, tendón, cable, cordón de zapato |
| **Adhesivo** | Pega y sella | Resina, brea, cera, savia |
| **Inflamable** | Prende con chispa | Yesca, fibra de coco seca, hojas secas, papel |
| **Combustible** | Mantiene el fuego | Madera, carbón, grasa, aceite |
| **Calor** | Transmite fuego | Brasa, antorcha, piedra caliente |
| **Recipiente** | Contiene (volumen) | Coco vacío, caña de bambú, vasija, lata, casco |
| **Estanco** | No pierde líquido | Vasija cocida, bambú con nudo, cantimplora |
| **Abrasivo** | Lija y afila | Arenisca, coral, arena mojada, piel de raya |
| **Moldeable** | Toma forma | Arcilla, barro, cera caliente, aluminio fundido |
| **Fibroso** | Se trenza o se deshace en hebras | Hoja de palma, corteza, fibra de coco, tela |
| **Aislante** | Protege del frío o el calor | Hojas grandes, piel, tela, ceniza |
| **Impermeable** | Repele el agua | Hoja de plátano, piel curtida, lona, resina |
| **Flota** | Flotabilidad | Madera seca, bambú, coco, espuma del asiento |
| **Brillante** | Refleja o señaliza | Aluminio pulido, espejo, vidrio, nácar |
| **Sonoro** | Suena al golpearlo o soplarlo | Bambú hueco, caracola, lata |
| **Nutritivo** | Alimento (proteína / energía / vitaminas) | Todo lo comestible |
| **Medicinal** | Cura (tipo) | Aloe, hojas de noni, miel, sal, alcohol del botiquín |
| **Tóxico** | Veneno (tipo) | Yuca cruda, setas, rana dardo, látex de manzanillo |
| **Pesado** | Peso por unidad | Todo |

### 2.2 Verbos: cómo se combina

La combinación es **un objeto en cada mano + un verbo**, o **un objeto en la mano + un
objetivo en el mundo**. El juego solo muestra los verbos posibles (máximo tres).

| Verbo | Qué requiere | Ejemplo | Resultado |
|---|---|---|---|
| **Golpear** | Contundente sobre duro | Piedra contra pedernal | Lascas (filo aleatorio) |
| **Tallar** | Filo sobre madera/hueso | Lasca sobre palo | Estaca, mango, cuchara, flauta, anzuelo |
| **Atar** | Ata + dos piezas | Liana: palo + lasca | Herramienta compuesta |
| **Pegar** | Adhesivo + dos piezas | Resina: punta + asta | Unión más duradera que atar |
| **Afilar** | Abrasivo sobre filo | Arenisca sobre hacha | Recupera filo y durabilidad |
| **Trenzar** | Fibroso ×3 | Fibras de coco | Cordel → cuerda → red, cesta, estera |
| **Machacar** | Contundente sobre blando | Piedra sobre hojas | Pasta medicinal, fibra, pigmento, cebo |
| **Raspar** | Filo sobre piel/corteza | Cuchillo sobre piel | Cuero en bruto, fibra de corteza |
| **Frotar** | Rígido sobre madera seca | Arco de fuego | Brasa (minijuego de ritmo) |
| **Llenar / verter** | Recipiente + fuente | Vasija en el río | Agua (sin tratar) |
| **Calentar** | Objeto sobre fuego | Vasija con agua | Agua hervida, cocción, secado |
| **Moldear** | Moldeable + forma | Arcilla en la mano | Vasija, molde, ladrillo, pipa (sin cocer) |
| **Remojar** | Objeto en agua | Fibra de coco en agua de mar | Fibra más fácil de trenzar |
| **Secar** | Objeto al sol/fuego | Carne en el secadero | Conserva, leña seca, piel |
| **Rellenar** | Recipiente + ingrediente | Bambú + pescado + hojas | Cocción al vapor en caña |
| **Fermentar** | Recipiente sellado + tiempo | Savia de palma | Tuba (vino de palma) → vinagre |
| **Fundir** | Horno + metal | Aluminio del fuselaje | Lingote, piezas coladas en molde |

### 2.3 Plantillas de resultado

Una **plantilla** define qué propiedades hacen falta y cómo se calculan las del
resultado. Ejemplos:

| Plantilla | Requisitos | Estadísticas heredadas |
|---|---|---|
| **Hacha** | Cabeza (Filo ≥ 2, Contundente ≥ 2) + Mango (Largo ≥ 2, Rígido ≥ 3) + Unión (Ata o Adhesivo) | Corte = Filo cabeza; Durabilidad = min(cabeza, unión); Peso = suma |
| **Lanza** | Asta (Largo ≥ 4, Rígido ≥ 2) + Punta (Punta ≥ 3) + Unión | Daño = Punta; Alcance = Largo; Lanzable si Peso ≤ 3 |
| **Cuchillo** | Hoja (Filo ≥ 3) + Mango opcional | Sin mango: te cortas al usarlo |
| **Arco** | Pala (Flexible ≥ 4, Largo ≥ 4) + Cuerda (Ata ≥ 3, tendón o cordel) | Potencia = Flexible × calidad de cuerda |
| **Flecha** | Astil (Largo 2–3, Rígido) + Punta + Emplumado (plumas, opcional) | Sin plumas: precisión −50 % |
| **Antorcha** | Largo + Combustible en la punta (grasa, resina, tela con aceite) | Duración = Combustible |
| **Recipiente** | Forma hueca (Recipiente ≥ 1) | Estanco o no según el material |
| **Anzuelo** | Punta pequeña curvada (hueso, espina, alambre, concha) | Tamaño de pez máximo |
| **Martillo** | Contundente ≥ 3 + Mango | Clavar estacas, romper cocos y rocas |
| **Pala** | Superficie plana (concha grande, tabla, chapa) + Mango | Cavar, sacar arcilla, cubrir fosas |

Hay unas **60 plantillas** en total (sección 3). Con 3–10 materiales válidos por pieza
generan más de **400 variantes** con nombre propio
(«Hacha de basalto con mango de guayabo, atada con tendón»).

### 2.4 Calidad, desgaste y reparación

- **Calidad** de 1 a 5 estrellas, calculada por los materiales y por lo bien hecho
  (el tallado y el afilado tienen un pequeño minijuego opcional que mejora la calidad).
- **Desgaste visible** en el modelo: grietas, ataduras que se aflojan, filo mellado. No
  hay barras de durabilidad.
- **Reparar** es la misma acción que crear: volver a atar, afilar o reemplazar la
  pieza rota. La cabeza de un hacha rota se recupera.

### 2.5 Descubrimiento sin wiki

| Mecanismo | Cómo funciona |
|---|---|
| **Ideas** | Sostener combinaciones plausibles provoca una idea del personaje y un boceto tenue en el diario |
| **Observación** | Ver algo en el mundo sugiere su uso: un mono rompiendo un coco con una piedra, una nota Halden con un dibujo de una nasa |
| **Hallazgos** | Objetos fabricados por Inés o Halden encontrados en el mundo: al examinarlos se aprende su receta |
| **Experimentación** | Una combinación nueva que funciona se anota con un dibujo; las que fallan no penalizan |
| **Necesidad** | Cuando una necesidad aprieta, la voz interna sugiere soluciones del nivel actual («si pudiera hervir el agua…») |

---

## 3. Catálogo de objetos

Cifras aproximadas por categoría. Los nombres son definitivos; las estadísticas salen
de la tabla de propiedades en `Data/items.json`.

### 3.1 Materiales naturales (≈ 80)

**Madera y plantas (32):** rama seca, rama verde, palo recto, madera flotante, tronco
pequeño, tronco grande, tabla partida, caña de bambú (fina, media, gruesa), madera de
palma, madera dura (guayabo, ébano isleño), madera blanda (balsa), corteza, hoja de
palma, hoja de plátano, hoja grande de taro, hojas secas, liana, raíz aérea, fibra de
coco, cáscara de coco, algodón silvestre, pandano (hojas para trenzar), resina, savia de
palma, látex de manzanillo (tóxico), musgo, yesca de hongo, pita (fibra de agave), junco
del manglar, semillas (varias), plántulas.

**Piedra y tierra (22):** canto rodado, piedra plana, pedernal, basalto, obsidiana,
piedra pómez, arenisca, granito, cuarzo, arcilla roja, arcilla blanca (caolín), barro,
arena, sal marina, azufre, ceniza volcánica, carbón mineral (vena en el volcán), ocre
rojo, ocre amarillo, malaquita (pigmento verde), coral muerto, piedra imán (brújula).

**Mar (14):** concha pequeña, concha grande (almeja gigante), caracola, nácar, esponja,
alga comestible, alga de fibra, erizo (púas), coral, piedra de coral, madera de naufragio,
red vieja arrastrada, botella con mensaje, perla (rara).

**Animal (12):** hueso pequeño, hueso largo, cráneo, colmillo, tendón, piel en bruto,
grasa, plumas (pequeñas, largas, de colores), caparazón de tortuga (solo si la tortuga
muere por causas naturales), escama, huevo, cera de abeja.

### 3.2 Materiales procesados (≈ 45)

Lasca de pedernal, lasca de obsidiana, hoja de obsidiana pulida, cordel, cuerda, cuerda
gruesa, cabo trenzado, tela de fibra, estera, hilo, tendón seco, cuero curtido, cuero
engrasado, pergamino de piel, pegamento de resina, brea (resina + carbón), cera
derretida, aceite de coco, grasa fundida (sebo), jabón (ceniza + grasa), lejía, carbón
vegetal, ceniza de madera, cal viva (coral quemado), mortero de cal, ladrillo de barro
sin cocer, ladrillo cocido, teja, cerámica, pigmentos (5 colores), tinta de pulpo, yesca
preparada, mecha, sal refinada, azúcar de palma, vinagre, tuba, alcohol destilado,
lingote de aluminio, alambre, chapa, remaches, vidrio fundido (en bruto), lente pulida.

### 3.3 Rescatados del Albatros y del mundo (≈ 40)

**Del avión:** mochila, brújula, reloj de muñeca, barómetro, radio (rota), batería,
antena, bengalas (3), pistola de bengalas, botiquín (vendas, antiséptico, analgésicos,
pinzas), mechero (poco gas), cerillas (12), navaja multiusos (rota: falta una pieza),
linterna (sin pilas), cinta americana, cable eléctrico, tubo de aluminio, chapa del
fuselaje, remaches, cristal de la ventanilla, espejo del retrovisor, espuma del asiento,
tela del asiento, cinturón de seguridad, chaleco salvavidas, manual del avión (papel),
cámara desechable, termo, cantimplora, gafas de sol, kit de costura.

**Del mundo:** caja de un contenedor naufragado (contenido aleatorio: cuerda de nailon,
latas de conserva, lona, herramientas oxidadas, un balón de voleibol), material de los
campamentos Halden (pala, machete oxidado, caja de herramientas, lámpara de queroseno,
ollas de hierro, frascos de vidrio, tablas de mareas), botín del pecio (vela de lona,
cabos, ancla pequeña, sextante roto, catalejo).

### 3.4 Herramientas (≈ 45 plantillas y variantes)

**Nivel 0 — A mano:** piedra de golpear, palo de cavar, lasca, piedra de moler.

**Nivel 1 — Tosco:** hacha de piedra, cuchillo de lasca, martillo de piedra, pala de
concha, azada de hueso, raspador, punzón, arco de fuego, taladro de arco, cuchara
tallada, cesta de hoja, cordel.

**Nivel 2 — Tallado:** hacha de pedernal con mango, cuchillo de hueso, machete de madera
dura con filo de obsidiana, sierra de dientes de tiburón, cincel, azuela (para vaciar
canoas), escoplo, aguja de hueso, telar de cintura, piedra de afilar montada, cedazo,
mortero, garfio.

**Nivel 3 — Obsidiana:** hacha, cuchillo, navaja de afeitar (curar heridas limpiamente),
puntas de flecha finas. Cortan lo mejor del juego, pero se rompen si golpeas piedra.

**Nivel 4 — Rescatado y fundido:** hacha de aluminio, cuchillo de chapa, machete del
campamento Halden restaurado, pala de chapa, anzuelos de alambre, clavos, bisagras,
herramientas coladas en molde de arcilla (el horno de fundición llega en el Acto III).

**Utilidades:** antorcha, lámpara de aceite de coco, farol de queroseno, catalejo, gafas
de buceo (madera tallada + cristal + resina), tubo de respiración de bambú, silbato,
flauta de bambú, tambor de tronco, reloj de sol, destilador solar, colador, embudo de
hoja, fuelle de piel, camilla (angarillas), escalera de cuerda, garfio de escalada.

### 3.5 Caza, pesca y combate (≈ 30)

Lanza (de empuje y arrojadiza), arpón de pesca con cabo, arco corto y largo, flechas
(punta roma para aves, hueso, pedernal, obsidiana, pesca con arpón, incendiaria con
brea), honda, boleadoras (piedras + cuerda), cerbatana de bambú, dardos (normales y
envenenados con rana dardo o látex), garrote, cuchillo de desollar, red de mano, red de
pesca, caña de pescar con carrete de madera, sedal de fibra o de nailon, anzuelos (6
tipos), cebos (lombriz, vísceras, fruta fermentada, cangrejo), nasa, corral de piedras
para peces (en la zona intermareal, funciona con la marea), trampa de lazo, lastre
(tronco que cae), fosa con estacas, jaula, liga de resina para aves, reclamo de ave,
señuelo de pesca tallado.

### 3.6 Comida (≈ 95)

**Crudo (≈ 50):** coco verde (agua), coco maduro (carne), plátano, plátano macho,
mango, papaya, carambola, guayaba, fruta del pan, maracuyá, piña silvestre, lima
silvestre, higo, noni, anacardo (tóxico sin tostar), taro, yuca, batata, ñame, palmito,
brotes de bambú, algas, setas (5 especies: 2 comestibles, 2 tóxicas y 1 alucinógena),
miel, huevos (gallina, gaviota, tortuga no), larvas de palmera, cangrejo, cangrejo de los
cocoteros, langosta, lapas, almejas, mejillones, erizo, pulpo, calamar, 8 peces de
arrecife, 4 peces de río o estuario, 3 de mar abierto, carne de jabalí (4 cortes), carne
de ave, iguana, caracol terrestre.

**Preparado (≈ 45):** pescado asado en espeto, pescado al vapor en caña de bambú,
pescado en hoja de plátano, pescado ahumado, pescado salado, ceviche (lima + pescado
crudo), sopa de pescado, caldo de huesos, estofado de jabalí, jabalí ahumado, cecina,
pinchos, huevos a la brasa, fruta del pan asada, taro hervido, yuca cocida (desintoxica),
batata asada, puré de plátano macho, pan de ñame en piedra caliente, leche de coco,
crema de coco, dulce de coco con miel, mermelada de mango, fruta seca, cangrejo en su
caparazón, langosta a la brasa, pulpo a la piedra, almejas al vapor, erizo con lima,
larvas tostadas, algas crujientes, anacardos tostados, té de hojas (3 tipos: calor,
sueño, digestión), tuba, vinagre, conservas en vasija sellada (encurtidos), sal de hierbas.

La **vasija de cocina** funciona con propiedades: el resultado depende de la base
(agua, leche de coco, caldo), del ingrediente principal y de los complementos. Los
platos combinados dan bonificaciones (calor duradero, energía, ánimo) y aparecen con
nombre si coinciden con una receta conocida; si no, son un «guiso improvisado» de
calidad media.

### 3.7 Medicina (≈ 20)

Venda de tela, venda de hojas, sutura con aguja de hueso y tendón (cortes profundos),
cataplasma de noni, gel de aloe (quemaduras), pasta de cúrcuma silvestre (infección),
miel (heridas), agua salada (limpiar), antiséptico del botiquín, alcohol destilado,
analgésicos (6), antídoto de corteza (serpiente), vinagre (medusa), carbón activado
(intoxicación), té de corteza de sauce isleño (fiebre), férula de bambú (esguince y
fractura), repelente de ceniza y aceite (mosquitos), crema de óxido de cinc casera
(protector solar con arcilla blanca y aceite), jabón (previene infecciones).

### 3.8 Ropa y equipo (≈ 25)

Sombrero de hoja de palma (sol), gorro de hoja de plátano (lluvia), poncho de hojas,
capa de lona, manta de fibra, manta de piel, sandalias de corteza y cuero (arrecife y
cortes), botas de piel, guantes de fibra (espinas y trepar), rodilleras de cuero (ir
agachado), cinturón de cuero (+2 enganches), bandolera, carcaj, zurrón, mochila de fibra
(+volumen), mochila con armazón de bambú (+peso), chaleco de pesca (bolsillos), pintura
de barro (camuflaje y olor), collar de dientes (trofeo), gafas de buceo, aletas de
madera, chaleco salvavidas, faja de lona (espalda: más peso transportable).

### 3.9 Contenedores (≈ 15)

Coco vacío, caña de bambú con nudo, calabaza seca, cesta de hoja, cesta de mimbre,
bolsa de red, odre de piel, vasija sin cocer, vasija cocida, tinaja grande, olla de
hierro (Halden), lata, termo, arcón de madera, estantería.

### 3.10 Construcción (≈ 70 piezas)

- **Estructura (×3 materiales: hoja/bambú/madera):** pilar, viga, suelo, pared, pared
  con ventana, pared con puerta, puerta, contraventana, techo inclinado, techo a dos
  aguas, remate, escalera, rampa, barandilla, plataforma elevada, pilote (sobre agua).
- **De piedra (tardío):** cimiento de piedra, muro de piedra seca, horno, chimenea.
- **Mobiliario:** cama de hojas, catre de bambú, hamaca, banco, mesa, estantería,
  colgadores de pared (herramientas expuestas), arcón, alfombra de estera, cortina,
  lámpara colgante, trofeos de caza, tablero de corcho para el diario (mapa grande en la
  base).
- **Producción:** fogata, hoguera de señal, horno de barro, horno de fundición,
  ahumadero, secadero, salina, destilador solar, recolector de lluvia, cisterna, filtro
  de arena y carbón, telar, piedra de trabajo, banco de chatarra, astillero, mesa de
  despiece, pila de compost.
- **Granja:** bancal, espaldera (maracuyá), vivero, gallinero, pocilga (crías de
  jabalí), colmena, estanque de peces, espantapájaros (contra monos y aves).
- **Exterior:** muelle, puente colgante, tirolina, escalera de cuerda, torre de vigía,
  cerca de estacas, empalizada, sendero de piedras, hito de piedras, bandera, farol de
  camino.

---

## 4. Caza

La caza es un **bucle completo**: leer el terreno, acercarse, abatir, despiezar y
conservar. Cada fase tiene decisiones.

### 4.1 Leer el terreno

- **Sendas de animales** generadas entre agua, comida y refugio. Se reconocen por la
  vegetación pisada.
- **Rastros:** huellas (su nitidez indica cuánto hace: la lluvia las borra), excrementos
  (frescos o secos), pelos en ramas, marcas de colmillos en troncos, hozaduras de jabalí,
  plumas, restos de fruta.
- **Horarios:** cada especie tiene horas de actividad. Los abrevaderos al amanecer y al
  atardecer son los mejores sitios de espera.
- **Canela** sigue rastros frescos si se le da a oler una huella.

### 4.2 Acercarse

| Factor | Efecto | Contramedida |
|---|---|---|
| **Viento** | Lleva tu olor | Moverse contra el viento (se ve en la hierba, el humo y un puñado de arena) |
| **Olor** | Humo, sangre y sudor aumentan el rastro | Bañarse, pintura de barro |
| **Ruido** | Depende de la superficie, el peso y la velocidad | Agacharse, ir descalzo, soltar peso |
| **Vista** | Silueta contra el cielo, movimiento | Quedarse quieto, vegetación, camuflaje, horas de poca luz |
| **Canela** | Puede espantar a la presa | Orden de «quieta» con el silbato |

### 4.3 Abatir

- **Lanza:** empuje (seguro, requiere acercarse) o lanzamiento (arco de trayectoria).
- **Arco:** tensión progresiva, caída de la flecha, las flechas se recuperan.
- **Honda y boleadoras:** para aves y presas pequeñas.
- **Cerbatana:** silenciosa; los dardos envenenados duermen o matan a presas medianas.
- **Trampas:** lazo, lastre, fosa, jaula, liga de resina. Se revisan cada día y los
  cebos importan. Hay que resetearlas.
- **Presas heridas** huyen dejando rastro de sangre, que la lluvia borra.
- **Peligro:** el jabalí acorralado embiste; el cocodrilo arrastra al agua (se escapa
  golpeándole los ojos con una secuencia rápida).

### 4.4 Despiezar y conservar

- Hace falta un cuchillo. La calidad del filo cambia la velocidad y el rendimiento.
- La carcasa se divide en piezas físicas: cortes de carne, piel, grasa, tendones,
  huesos, colmillos o plumas y vísceras (cebo).
- Una carcasa sin aprovechar se pudre en un día y atrae cangrejos, aves y, en el agua,
  tiburones.
- **Conservar:** ahumar, salar, secar, confitar en grasa o guardar en vasija con
  vinagre.

### 4.5 Ecosistema

- Cada zona tiene **poblaciones**. Sobrecazar una zona vacía el área durante días,
  lo que empuja a explorar más lejos, y la población se recupera sola.
- **Cadena alimentaria simulada:** las aves comen pescado que se deja al aire, los
  cangrejos limpian carroña, los tiburones siguen la sangre en el agua y los monos roban
  fruta de los secaderos.

### 4.6 Presas legendarias

Cinco animales únicos con nombre, comportamiento propio y recompensa exclusiva.

| Legendario | Dónde | Reto | Recompensa |
|---|---|---|---|
| **Colmillo Roto** — jabalí macho gigante | Esmeralda | Muy listo; rompe trampas simples y huye de la luz | Colmillos (hacha de hueso legendaria), piel de alfombra |
| **El Viejo** — mero gigante | Cueva submarina de Arenas Blancas | Rompe sedales; hace falta sedal de nailon y un anzuelo de alambre | Trofeo, festín (ánimo máximo varios días) |
| **Reina del manglar** — cocodrilo | Manglar de las Voces | Emboscada; se caza con trampa de pozo y cebo | Placas de piel (armadura ligera) |
| **Sombra** — tiburón tigre | Canal profundo | Ataca embarcaciones pequeñas | Dientes (sierra de tiburón legendaria) |
| **El Ladrón** — mono alfa | Tropa de Esmeralda | Te roba un objeto valioso y hay que seguirlo por los árboles | Recuperar el objeto + una mascota opcional |

---

## 5. Supervivencia

### 5.1 Curva de dificultad

| Momento | Qué aprieta | Qué aprende el jugador |
|---|---|---|
| **Día 1** | Sed, sol, la primera noche | Coco, sombra, refugio inclinado, fuego con el mechero |
| **Días 2–4** | Hambre, agua limpia | Pesca en la orilla, fruta, hervir agua, arco de fuego |
| **Días 5–10** | Herramientas, lluvias | Hachas, cabaña, conservar comida, trampas |
| **Semanas 2–3** | Otras islas, peligros | Balsa, caza mayor, medicina, cerámica |
| **Temporada de ciclones** | Resistir | Construcción sólida, reservas, prepararse |
| **Mes 2+** | La vida ya es estable | El reto pasa a ser explorar, los legendarios y la historia |

La supervivencia **nunca desaparece**, pero deja de ser la protagonista cuando el
jugador domina lo básico; entonces toman el relevo la exploración y la historia.

### 5.2 Agua

| Fuente | Riesgo | Cómo hacerla segura |
|---|---|---|
| Agua de coco | Ninguno (limitada) | — |
| Lluvia recogida | Ninguno si el recipiente está limpio | — |
| Río o arroyo | Parásitos (intoxicación leve) | Hervir o filtro de arena y carbón |
| Charca o manglar | Grave | Hervir + filtrar |
| Mar | Deshidrata | Destilador solar o hervir y condensar |
| Rocío | Ninguno (poco) | Paño al amanecer |
| Brote de bambú | Ninguno | — |

### 5.3 Fuego

- **Encender:** mechero (usos limitados) → cerillas → arco de fuego (minijuego) →
  pedernal y acero (con metal rescatado) → lente del avión al sol.
- **Material:** yesca (fibra de coco, hongo, algodón), astillas y leña. La madera mojada
  humea y apenas calienta.
- **Tipos:** fogata, hoguera, fuego protegido de la lluvia (bajo techo), brasas para
  cocinar, fuego de señal (hojas verdes = humo denso y blanco).
- **Propagación:** en la estación seca el fuego se extiende por la hierba. Un descuido
  quema una ladera, y la zona quemada rebrota con plantas nuevas semanas después.

### 5.4 Cuerpo

Necesidades, estados y heridas se describen en el GDD (§4.3). Aquí se añaden:

- **Mojado:** llueve, nadas o sudas → el frío baja la temperatura. Secarse junto al
  fuego o al sol.
- **Mosquitos** en el manglar y en la estación de lluvias: picaduras que bajan el ánimo y
  una pequeña probabilidad de fiebre. Se evitan con humo, repelente y mosquiteras de
  tela.
- **Insolación** si pasas horas al sol sin sombrero en la estación seca.
- **Moral y rutina:** dormir en tu cama, comer caliente, tener la base ordenada, tocar la
  flauta, escribir en el diario y acariciar a Canela suben el ánimo. El ánimo alto da
  pequeñas ventajas (trabajo más rápido, ideas más frecuentes).

---

## 6. Estaciones y temporales

### 6.1 El año del archipiélago

Un año de juego dura **32 días** (configurable entre 16 y 64), en cuatro estaciones. La
estación cambia la paleta, el sonido, los recursos, los animales y los problemas.

| Estación | Días | Clima | Qué hay | Qué aprieta |
|---|---|---|---|---|
| **Seca** | 1–8 | Cielo limpio, calor, viento alisio | Mango, anacardo, tortugas desovando, pesca fácil, mareas vivas, noches estrelladas | Ríos bajos, sed, insolación, incendios |
| **Primeras lluvias** | 9–16 | Chubascos por la tarde, tormentas eléctricas | Setas, ranas, charcas, flores, abejas en plena actividad, fruta de la pasión | Mosquitos, rayos, caminos embarrados |
| **Monzón** | 17–24 | Lluvia casi diaria, niebla, ríos crecidos | Fruta del pan, taro, peces remontando ríos, cascadas enormes, cuevas con agua | Frío por humedad, crecidas, deslizamientos, comida que se pudre |
| **Temporada de ciclones** | 25–32 | Calma bochornosa entre tormentas, mar de fondo | Restos arrastrados a la costa, contenedores naufragados, ballenas | Ciclones, mar peligroso, navegación arriesgada |

La partida empieza a mitad de la estación seca, la más amable.

### 6.2 Temporales

Cada temporal se **anuncia** para que el jugador se prepare, **golpea** con consecuencias
y **deja un después** que invita a explorar.

| Temporal | Señales previas | Durante | Después |
|---|---|---|---|
| **Tormenta eléctrica** | Nubes de torre por la tarde, calor húmedo, radio con estática | Rayos que golpean árboles altos (y torres de vigía), incendios | Árboles partidos (leña fácil), setas nuevas |
| **Crecida** | Varios días de lluvia, río marrón | El río desborda y arrastra construcciones y objetos de la ribera | Barro fértil (huerto), objetos varados río abajo |
| **Deslizamiento** | Lluvias intensas en ladera deforestada | Una ladera cae y bloquea caminos | Cueva o veta de mineral al descubierto |
| **Galerna** | Barómetro bajando rápido | Viento fuerte y olas; peligroso navegar | Madera flotante en las playas |
| **Niebla cerrada** | Amanecer frío tras lluvia | Visibilidad de 10 m; animales más cerca | Bioluminiscencia en el manglar más intensa |
| **Ola de calor** | Varios días sin viento | Sed doble, insolación, fuego fácil | Charcas secas: aparecen fósiles y objetos Halden |
| **Ciclón (categorías 1–3)** | 1–2 días antes: barómetro, cielo cobrizo, animales huyendo, aves hacia el interior, mar de fondo | Una noche entera: viento que derriba lo mal anclado, lluvia horizontal, mar sobre la playa | Costa remodelada, contenedores y un pecio nuevo, árboles caídos, animales desplazados |

**Prepararse para un ciclón** es una misión emergente: reforzar la base con cuerda y
piedra, subir las embarcaciones a tierra, guardar la comida en vasijas selladas, llenar
cisternas y refugiarse en una cueva o en la construcción más sólida. Sobrevivirlo sin
daños da un logro.

### 6.3 Mareas y Luna

- Dos pleamares y dos bajamares al día. Amplitud según la fase lunar (mareas vivas en
  luna llena y nueva).
- **La bajamar abre:** pasos a pie entre islotes, pozas de marea (pulpo, cangrejos,
  erizos), cuevas marinas, el corral de piedras para peces y restos del Albatros.
- **La pleamar cierra** esos pasos: un jugador descuidado queda atrapado en un islote
  hasta la siguiente bajamar.
- La tabla de mareas del diario se completa observando y en el observatorio Halden.

---

## 7. Granja, animales y hogar

- **Huerto:** plantar semillas y esquejes encontrados (plátano, taro, batata, piña,
  maracuyá, especias, plantas medicinales). Cada cultivo tiene su estación. Riego,
  compost, espantapájaros y cercas contra jabalíes y monos.
- **Domesticar:** gallinas silvestres (atraídas con semillas) y crías de jabalí huérfanas.
  Dan huevos y compañía, necesitan cercado y comida.
- **Colmena:** capturar un enjambre en una caja de madera → miel y cera.
- **Estanque:** peces vivos capturados con nasa se crían para tener comida segura en el
  monzón.
- **El hogar crece:** la base se personaliza con trofeos, fotos colgadas, el mapa grande
  del diario en la pared, pigmentos para pintar paredes y un jardín.

---

## 8. Ritmo: por qué no aburre

### 8.1 Bucles de juego

| Escala | Bucle | Ejemplo |
|---|---|---|
| **30 segundos** | Ver algo → acercarse → cogerlo o usarlo | Una fruta, una huella, un brillo en el agua |
| **5 minutos** | Necesidad → plan → fabricar o cazar → resolver | «Tengo hambre» → lanza → pozas de marea → cangrejos |
| **1 día** | Amanecer → trabajo → volver antes de la noche → fuego → dormir | Planificar la salida según la marea y el clima |
| **1 semana** | Proyecto grande | La canoa, la cabaña sobre pilotes, cazar a Colmillo Roto |
| **1 estación** | Adaptarse | Prepararse para el monzón, aprovechar la seca para explorar |
| **Campaña** | Historia y finales | Seguir a Inés, descifrar Halden y la brújula estelar |

### 8.2 Reglas anti-aburrimiento

1. **Siempre hay un horizonte visible:** humo en otra isla, el faro, el pico, una
   bandada que se aleja, un destello en la costa.
2. **Las mejoras cambian verbos, no números:** la tirolina, la canoa, las gafas de buceo
   y el garfio abren formas nuevas de moverse, no solo +10 %.
3. **La estación renueva los problemas:** lo que funcionaba en la seca no funciona en el
   monzón.
4. **El mundo tiene su propia agenda:** eventos, legendarios, contenedores que llegan
   con los ciclones, monos que roban.
5. **Nada es relleno:** cada cueva y cada PdI tiene al menos una de estas cosas: recurso
   único, historia, vista o atajo.

### 8.3 Reglas anti-agobio

1. **Máximo tres verbos** en la acción contextual.
2. **Tres conceptos nuevos por sesión como máximo:** cada isla introduce sus materiales
   y cada estación sus problemas.
3. **Las ideas del personaje** sustituyen a la wiki.
4. **Legibilidad low-poly:** cada objeto se reconoce por su forma y su color a 20 m.
5. **Sin microgestión:** no hay ropa que se ensucie ni hambre de la mascota. Las
   estadísticas se leen en el propio objeto (grietas, color, olor).
6. **El diario ordena el conocimiento:** todo lo aprendido queda a un gesto.
7. **Modo Explorador** para quien solo quiere el mundo y la historia.

---

## 9. Contenido emergente (el mundo como sistema)

Interacciones que no están guionizadas y producen historias propias:

- El fuego en la estación seca se extiende, espanta animales y deja carbón y cenizas
  fértiles.
- Los monos roban lo que dejas en el suelo y lo esconden en la copa de un árbol: se
  forman «tesoros» de monos.
- Las mareas mueven objetos flotantes: una balsa mal amarrada aparece en otra playa.
- La lluvia llena cualquier recipiente abierto que dejes fuera.
- Las gaviotas siguen a tu canoa si llevas pescado.
- Los tiburones acuden a la sangre en el agua.
- Los rayos caen en lo más alto, incluida tu torre de vigía.
- La madera se pudre si está siempre húmeda; la de palma dura menos que la dura.
- Canela desentierra cosas donde los monos esconden.
- Los animales usan tus caminos y tus puentes.
- Un árbol talado cae según su inclinación y el viento, y puede servir de puente.

---

## 10. Implementación de datos

- `Data/properties.json` — definición de propiedades.
- `Data/items.json` — objetos base: malla, propiedades, peso, volumen, etiquetas.
- `Data/templates.json` — plantillas de resultado: piezas, requisitos y fórmulas.
- `Data/verbs.json` — verbos y condiciones.
- `Data/recipes_cooking.json` — recetas con nombre (el resto es guiso improvisado).
- `Data/species.json` — fauna: comportamiento, horarios, despiece, población.
- `Data/seasons.json`, `Data/weather.json`, `Data/events.json`.
- Importadas a `UDataAsset` por el pipeline. **Añadir un objeto nuevo es añadir una
  entrada de JSON y un script de malla.**
- Tests automáticos: toda plantilla es alcanzable con los materiales de al menos una
  isla; ninguna combinación produce objetos sin malla; recetas deterministas.
