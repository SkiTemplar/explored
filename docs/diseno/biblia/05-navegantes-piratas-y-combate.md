# EXPLORED — Biblia de contenido, sección 05: Navegantes, piratas y combate

Versión 1 · 2026-09-27 · Autor: diseño. Depende de `docs/diseno/gdd_v2.md` §3.8, §3.9,
§4, §6 y §8 (manda sobre este documento en caso de conflicto) y de
`docs/design/biblia-de-contenido.md` §2 (propiedades y verbos), §4.6 (capturas
legendarias) y §9 (ruinas y wayfinding). No repite lo que ya está cerrado allí; lo
extiende con números.

Todo lo numérico de este documento son valores de balance internos, coherentes con la
regla anti-agobio de la biblia (§8.3): el jugador nunca ve una cifra en pantalla, las
lee en el cuerpo (viñeteado, pulso, temblor — `docs/tecnico/cuerpo.md`), en el sonido y
en el propio objeto (grietas, sangre, humo). El apartado 3.0 explica cómo el combate se
apoya en el sistema de señales corporales y de heridas que **ya existe** en
`Content/Data/survival_needs.json` en vez de inventar una barra de vida nueva.

---

## 0. Personajes humanos: la decisión de producción

El GDD v3 prohibía personajes humanos animados en pantalla por falta de presupuesto de
animación. El GDD v2 mantiene la prohibición como regla de producción (§7.2) pero exige
que navegantes y piratas «se comuniquen por gesto y comportamiento» — lo que exige que
se les vea gesticular. Esta sección resuelve la contradicción, con el mismo patrón de
pivote que ya usó el GDD v2 para la fauna doméstica (§3.6): sustituir producción propia
de alto riesgo por packs ya terminados.

**Decisión:** sí hay personajes humanos visibles, con esqueleto estándar de Mixamo y
malla low-poly de un pack CC0/licencia permisiva tipo *KayKit Adventurers*
(compatible con el retargeting de UE5 porque usa la jerarquía de huesos de Mixamo). Lo
que **no** cambia respecto al GDD v3 es lo que hacía cara la animación de calidad: no
hay rig facial, no hay lipsync, no hay cinemáticas ni cámara en primer plano sobre un
rostro. La cara es una forma low-poly plana, sin ojos ni boca articulados — legible por
silueta y color a 20 m, igual que cualquier otro objeto del juego (biblia §8.3, regla
4), y evita además cualquier lectura de rasgo étnico marcado, que es precisamente el
riesgo de sensibilidad cultural que el GDD señala en su tabla de riesgos (§8).

- **Set de animación (reutilizado entre navegantes y piratas, 10 clips de Mixamo
  retargeteados):** ralentí, caminar, trabajar (bucle genérico: remo, tejido, martillo),
  saludar (mano alzada), ofrecer objeto (extender el brazo), asustarse/huir, ataque
  cuerpo a cuerpo, disparo de arco, reacción a impacto, caída/fuera de combate.
- **Diferenciación visual:** silueta y paleta, no rasgos faciales. Navegantes: paño
  natural, tonos tierra y arena, tocados de fibra trenzada. Piratas: cuero oscuro, tela
  remendada, siluetas más angulosas y con más metal a la vista (coherente con biblia
  §8.3, regla 4: se reconocen por forma y color, no por detalle).
- **Riesgo técnico:** ninguno nuevo respecto al que ya asume el GDD v2 para fauna con
  rig (§3.6, §3.7); incluye el IK Retargeter de UE5 (Mixamo → esqueleto propio) y LOD de
  animación agresivo con el mismo patrón ya usado en fauna.
- **Fase:** navegantes [F3] (§3.9 del GDD), piratas [F3] para IA de asalto completa,
  pero el modelo/esqueleto/animaciones base se producen antes, junto con las piezas de
  muralla [F2], para no bloquear el arte de fase 3 detrás de la IA de fase 3.

---

## 1. Los navegantes del arrecife

### 1.1 Cultura y aldeas

Pueblo **ficticio**, nunca atado a una cultura real: navegantes del Pacífico como
inspiración de vestuario, arquitectura sobre pilotes y wayfinding estelar (ya
establecido en biblia §9), sin nombre de etnia real ni imitación de un patrón cultural
concreto. En el propio juego se les llama siempre «el pueblo del arrecife» o «los
navegantes»; el archipiélago no tiene nombre y ellos tampoco.

**Decisión de ubicación** (el GDD deja «Arenas Blancas y La Meseta» como candidatas; se
fija aquí): una **aldea principal en Arenas Blancas** y un **puesto de trueque menor en
La Meseta**. Justificación: Arenas Blancas ya tiene el astillero y la pesca como eje
(coherente con un pueblo navegante); La Meseta ya tiene el observatorio de mareas, así
que su puesto se especializa en wayfinding en vez de trueque de materiales.

| Asentamiento | Isla | Población | Roles |
|---|---|---|---|
| **Aldea principal** | Arenas Blancas | 10 NPC | 1 guardián del marae (wayfinding, ofrendas), 2 trocadores (uno de metal/herramientas, otro de comida/medicina), 4 pescadores/tejedores en rutina ambiental, 3 mayores/niños decorativos (solo saludo) |
| **Puesto de trueque** | La Meseta | 4 NPC | 1 trocador, 1 guardián menor (enseña una técnica de wayfinding **distinta** a la de Arenas Blancas, nunca duplicada), 2 ambiente |

**Arquitectura:** piezas del kit de construcción ya existentes (biblia §3.10: pilote,
suelo, pared, techo de palma/bambú, plataforma elevada) sin piezas nuevas — la aldea es
una reutilización del mismo kit que usa el jugador, no un set de arte aparte. Una única
pieza nueva: el **tablón de peticiones** (§1.3), un prop tallado con motivos de
`story_es.json`.

### 1.2 Primer contacto

- **Detección:** al entrar por primera vez en un radio de **80 m** del centro de la
  aldea o el puesto, suena una caracola o un tambor lejano (aviso sonoro, nunca un
  marcador de misión — regla anti-agobio, biblia §8.3).
- **Gesto inicial:** el NPC más cercano saluda con la mano si el jugador no lleva un
  arma empuñada en ese instante. Si la lleva empuñada, el NPC retrocede un paso y no
  saluda; no hay penalización todavía, solo la reputación no sube ese primer contacto.
- **Reputación inicial:** se crea en el instante del primer contacto con valor **50**
  (tramo Neutral, §1.5) — no existe reputación con un pueblo que no se ha visitado.

### 1.3 Comunicación sin diálogos

Tres canales, ninguno con texto largo ni voz (GDD §7.2):

1. **Gestos:** saludo, ofrecer (extender el objeto sostenido hacia un NPC = intención de
   trueque), asentir/negar leve cuando se acepta o rechaza un intercambio.
2. **Objetos:** el trueque se resuelve exactamente como cualquier combinación del juego
   — un objeto en la mano + un NPC como «objetivo en el mundo» (biblia §2.2) — nunca un
   menú de tienda con lista de precios.
3. **Pictogramas:** el **tablón de peticiones**, un prop físico en el centro de cada
   asentamiento, tallado con un motivo de `story_es.json` (reutiliza el catálogo de
   `petroglyph_themes` como banco de iconos) que cambia cada **4 días de juego** y pide
   una categoría de objeto, nunca una cantidad exacta en texto. Ejemplos de icono →
   categoría pedida (para implementación; el jugador solo ve el tallado, esta tabla es
   la clave interna):

   | Icono (motivo reutilizado) | Categoría pedida | Etiqueta interna ES | Etiqueta interna EN |
   |---|---|---|---|
   | Cesta de pesca | Comida preparada o conservada | Comida | Food |
   | Tortuga con siete puntos | Cuero curtido o piel en bruto | Cuero | Leather |
   | Ola que se retira | Medicina (cualquiera del catálogo §7 de la biblia) | Medicina | Medicine |
   | Volcán con humo | Metal trabajado (lingote, chapa, alambre) | Metal | Metal |
   | Cesta de mimbre | Fruta rara del huerto (no la que ellos ya cultivan) | Fruta rara | Rare fruit |

### 1.4 Trueque

**Regla dura (GDD §5):** trueque, nunca moneda. Cada objeto tiene un **valor de trueque
fijo de 1 a 5** (tabla siguiente, análogo a la escala de propiedades 0–5 ya usada en
toda la biblia) que se multiplica por la **tasa de reputación** del tramo actual
(§1.5) para dar el valor real del intercambio; el pueblo nunca ofrece dinero ni un
precio en una moneda inexistente.

| Objeto que el jugador ofrece | Valor de trueque | Motivo |
|---|---|---|
| Fruta común, pescado sin conservar | 1 | Ya lo tienen en abundancia |
| Comida preparada o conservada (cualquiera de biblia §3.6) | 2 | Trabajo añadido, se valora |
| Cerámica, tela de fibra, cordelería | 2 | Útil pero no escaso |
| Cuero curtido, piel en bruto | 3 | Escaso en la aldea |
| Herramienta de Nivel 2–3 (tallado/obsidiana) | 3 | Filo mejor que el suyo |
| Medicina (cualquiera de biblia §7) | 4 | Muy valorada, no la fabrican igual |
| Metal trabajado (lingote, chapa, alambre, remaches) | 5 | El más escaso: no hay metal en la isla |
| Objeto ritual encontrado en una ruina activa suya | — | No se trueca: se **devuelve** (sube reputación directamente, §1.5) |

| Objeto que el pueblo ofrece | Valor de trueque | Requisito de reputación |
|---|---|---|
| Semillas y esquejes de una variedad exclusiva («batata violeta», no disponible en ninguna isla) | 2 | Cauta (20+) |
| Cerámica decorada, tinte, sal refinada | 2 | Cauta (20+) |
| Cabos, vela de lona reforzada, tablas de mareas detalladas (ayuda de cartografía) | 3 | Neutral (40+) |
| Perlas, nácar trabajado | 3 | Buena (70+) |
| Enseñanza de una técnica de wayfinding aún no aprendida | — (no se trueca, se enseña) | Buena (70+) |
| Primera pareja de animales domésticos (gallina o cabra, biblia §3.6 del GDD) | 5 | Alta (90+) |
| Aviso de un asalto pirata inminente cerca de su territorio | — (favor, no objeto) | Alta (90+) |

- **Tasa de reputación:** el valor de trueque del jugador se multiplica por 0.75
  (Cauta), 1.0 (Neutral), 1.25 (Buena) o 1.5 (Alta) — ver tabla de tramos en §1.5. Con
  Hostil (<20) el trueque está cerrado del todo.
- **Límite anti-grindeo:** solo el primer trueque de cada día de juego da reputación
  (§1.5); los siguientes ese mismo día dan objetos con normalidad pero 0 de reputación.
- **Ventana horaria:** el trueque solo está disponible de **8:00 a 18:00** hora de
  juego; fuera de esa franja los trocadores están en su rutina (pesca, tejido) o en la
  ofrenda del atardecer y no atienden — coherente con «pueblo con horario» (GDD §2.3).
  A las 18:00 en punto ya no atienden.
- `[Decisión]` **Qué se trueca con Cauta.** La tabla de tramos dice que con Cauta hay
  trueque «solo de objetos básicos», pero todas las ofertas del pueblo pedían Neutral:
  con Cauta el trueque estaba abierto y no había nada que recibir. Se resuelve así: con
  Cauta el pueblo **acepta** solo comida, fibra y cerámica (valor 1–2) y **ofrece** solo
  sus bienes básicos de valor 2 (batata violeta, cerámica decorada, sal refinada), que
  bajan a «Cauta (20+)» en la tabla de arriba y en `fases_futuras.json`. Así el trueque
  sigue siendo la vía para salir de Cauta, que es para lo que sirve el +3 diario.
- `[Decisión]` **Cuenta exacta.** El valor ofrecido (suma de valor × cantidad) se
  multiplica por la tasa y tiene que cubrir valor × cantidad de lo pedido; lo que sobra no
  se devuelve (no hay cambio sin moneda). Las tasas se calculan en cuartos (0,75 = 3/4)
  con enteros, para que servidor y clientes den el mismo resultado.
- `[Decisión]` **El trueque «gratis» del tablón** (§1.6) es un objeto de la oferta del
  pueblo, una unidad, sin entregar nada; no da reputación ni gasta el trueque que sí la
  da ese día. Caduca al acabar el día.

### 1.5 Reputación

Escala 0–100 por asentamiento (Arenas Blancas y La Meseta llevan la suya, **no**
comparten valor, aunque compartan el mismo pueblo — respetar a uno no compra al otro).

| Tramo | Rango | Trueque | Otros efectos |
|---|---|---|---|
| **Hostil** | 0–19 | Cerrado | Patrullas piratas +1 categoría de asalto cerca de su territorio (§2.3) |
| **Cauta** | 20–39 | Tasa ×0.75, solo objetos básicos (comida, fibra, cerámica) | — |
| **Neutral** | 40–69 | Tasa ×1.0, catálogo completo salvo lo exclusivo de Buena/Alta | Valor inicial al primer contacto: 50 |
| **Buena** | 70–89 | Tasa ×1.25 | Enseña wayfinding si el jugador no lo tiene ya |
| **Alta** | 90–100 | Tasa ×1.5 | Animales domésticos, aviso de asaltos, ayuda en defensa (§2.3) |

**Acciones que suben:**

| Acción | Reputación |
|---|---|
| Trueque exitoso (máx. 1/día de juego) | +3 |
| Devolver un objeto ritual a su marae de origen | +5 |
| Completar un encargo del tablón de peticiones | +4 |
| Pasar una jornada completa (ciclo día/noche) sin minar, talar ni cazar dentro de 150 m de la aldea o 50 m de un marae activo suyo (chequeo automático, una vez al día) | +2 |
| Ayudar a repeler un asalto pirata cerca de su territorio (evento raro, solo con Amenaza pirata alta y reputación ≥ 40, §2.3) | +10 |

**Acciones que bajan:**

| Acción | Reputación |
|---|---|
| Cazar fauna dentro de 150 m de la aldea | −15 |
| Minar o talar dentro de 50 m de un marae activo suyo | −10 |
| Saquear un contenedor o el tablón de ofrendas de la aldea | −20 |
| Golpear a un aldeano con un arma | −25 |

- **Aldeanos invulnerables:** un aldeano no se puede matar ni herir — el golpe se
  registra (baja reputación, el NPC huye y no vuelve a comerciar ese día) pero no hay
  barra de vida que vaciar. Es una decisión mecánica, no solo narrativa, que hace
  cumplir la orden del director de que el pueblo **nunca** es un enemigo: no existe la
  posibilidad de «matarlos por accidente» ni de declararles la guerra.
- **Enfriamiento tras una ofensa grave:** si la reputación cae por debajo de 20
  (Hostil), el trueque queda cerrado un mínimo de **15 días de juego** aunque la
  reputación numérica se recupere antes con acciones positivas — evita que un golpe
  aislado se perdone al día siguiente.
- **Sin decaimiento pasivo:** la reputación no baja sola con el tiempo; solo cambia por
  acción directa del jugador.
- `[Decisión]` **Detalles del enfriamiento y de las acciones** (fijados al escribir
  `Villages/ReputationModel`): el plazo empieza con cualquier acción que baje y deje la
  reputación por debajo de 20, también si ya estaba en 0 (pegar a un aldeano con la
  reputación en el suelo no sale gratis); una ofensa nueva durante el plazo lo alarga a
  15 días desde ese día y nunca lo acorta. Una acción sobre un pueblo sin visitar hace
  antes el primer contacto (quien caza junto a la aldea ya está allí). Devolver un objeto
  ritual cuenta **una vez por objeto en toda la partida**, se devuelva donde se devuelva,
  vale a cualquier hora y también durante el enfriamiento (no es trueque), y solo sirve
  con objetos de marae o de cueva ritual: lo del pecio no es suyo.

### 1.6 Enseñanzas y encargos

- **Wayfinding (biblia §9.2):** con reputación Buena (70+), el guardián del marae
  enseña directamente una de las cinco técnicas que el jugador **aún no tenga**, como
  vía alternativa a encontrarla en una ruina. Arenas Blancas y La Meseta nunca enseñan
  la misma técnica entre sí, para no duplicar contenido.
- **Encargos del tablón (§1.3):** rotan cada 4 días de juego; completar uno (entregar un
  objeto de la categoría pedida a cualquier trocador) da +4 de reputación y, además, un
  trueque «gratis» extra ese mismo día que no cuenta para el límite de uno al día.
- **Nunca hay una lista de misiones:** no hay registro de encargos en ningún menú; el
  tablón físico es la única fuente de verdad, coherente con «el mapa ordena el
  conocimiento» (biblia §8.3, regla 6) aplicado también aquí.

---

## 2. Los piratas

### 2.1 Tipos

Cuatro tipos, todos con esqueleto y set de animación de §0.

| Tipo | Nombre ES / EN | Vida | Arma | Rol en el grupo |
|---|---|---|---|---|
| **Saqueador** | Saqueador / Raider | 40 | Machete (Filo 3, daño 9) | Cuerpo a cuerpo genérico, prioriza recursos sueltos |
| **Arquero** | Arquero / Archer | 30 | Arco improvisado (Punta 3, daño 9 a <15 m) | Se mantiene a distancia, prioriza al jugador antes que a estructuras |
| **Incendiario** | Incendiario / Firestarter | 35 | Bolas de brea encendida (alcance 12 m) | Prioriza piezas de madera/bambú/palma de la base (§4.3) |
| **Capitán** | Capitán / Captain | 90 | Machete de aluminio (Filo 5, daño 15) + aturde | Solo en asaltos de categoría 2–3; si muere, el resto del grupo se retira |

Ningún pirata usa arma de fuego ni pólvora — decisión explícita, ver §2.6.

### 2.2 IA

- **Percepción:** vista (cono frontal, 25 m de día / 12 m de noche), oído (ruido de
  combate o de construcción a más del doble de distancia que la vista) y olfato al humo
  de una hoguera encendida — reutiliza literalmente las máquinas de estados y la
  percepción ya implementadas para `Fauna` (GDD §3.8, dependencias), no un sistema
  nuevo de percepción.
- **Patrulla:** rutas fijas por semilla entre los dos campamentos (§2.5) y las islas
  avanzadas; en patrulla no atacan si no se les provoca (atacar primero, entrar a su
  campamento, o robar de un cofre de su barco).
- **Grupo:** 2 a 6 piratas por asalto según categoría (§2.3); si el Capitán está
  presente y muere, el grupo entero se retira hacia el barco más cercano en 3 segundos
  (moral de grupo simple, sin IA de retirada individual).
- **Prioridad de objetivo:** jugador visible y a menos de 20 m > estructura con
  recursos visibles > estructura de madera si es Incendiario y no hay nada más que
  quemar.

### 2.3 Cuándo y cómo atacan

**Amenaza pirata:** contador propio, 0–100, empieza en 0. No es la reputación del
pueblo (esa es un contador aparte, §1.5).

| Acción del jugador | Amenaza |
|---|---|
| Saquear o incendiar un campamento pirata | +8 |
| Derrotar la tripulación de un barco pirata | +15 |
| Matar un pirata en un asalto que el jugador ha provocado él mismo | +4 |
| Repeler un asalto que los piratas iniciaron sin provocación | +0 (defenderse no sube la Amenaza) |
| (pasivo) 10 días de juego sin ninguna agresión del jugador | −5 |

**Condición para que exista un asalto programado:** el jugador debe tener una
**base marcada** (al menos una torre de vigía o una bandera construida, biblia §3.10).
Sin eso, no hay asaltos, solo patrullas visibles a distancia.

**Categoría de asalto según Amenaza:**

| Amenaza | Categoría | Frecuencia | Grupo | Aviso previo |
|---|---|---|---|---|
| 0–24 | Sin asalto | — | Solo patrullas visibles, no atacan | — |
| 25–49 | 1 (bajo) | Cada 6–9 días de juego | 1 Saqueador + 1 Arquero | Humo negro en el horizonte 1 día antes |
| 50–74 | 2 (medio) | Cada 4–6 días | Saqueador + Arquero + Incendiario (a veces 2 Saqueadores) | Humo negro 1 día antes |
| 75–100 | 3 (alto) | Cada 2–4 días | 5–6 piratas con Capitán | Humo negro 1 día antes + tambor más fuerte la madrugada del asalto |

Reputación **Hostil** (<20) con la aldea más cercana al territorio del jugador sube en
+1 la categoría efectiva de los asaltos que caigan sobre esa zona (máximo categoría 3),
tal y como fija el GDD: un pueblo sin protección del jugador es más vulnerable, y eso
se traduce en piratas más envalentonados cerca de su territorio, nunca en que el pueblo
mismo ataque.

**Condición de disparo del ciclo:** existe base marcada **y** (hay al menos 1 recurso
apilado fuera de un contenedor cerrado en un radio de 40 m de la base **o** la
reputación de la aldea más cercana es Hostil). Si ninguna de las dos se cumple, el
asalto de ese ciclo se cancela sin gastar el contador de días — se reintenta en el
siguiente.

`[Decisión]` Cómo lo lee el programador (`Raiders/PirateThreatModel`, una evaluación por
día de juego en el servidor):

- «Se reintenta en el siguiente» es **el día siguiente**: el asalto se aplaza un día sin
  volver a tirar el intervalo, y el humo sigue en el horizonte mientras tanto.
- El intervalo se tira al programar el asalto, con la categoría de ese momento. Si la
  Amenaza baja de 25 o desaparece la base marcada, el asalto programado se borra.
- La categoría del grupo se calcula el día del asalto. El +1 de aldea Hostil y el de
  jugadores (biblia 08 §5.6) **nunca crean un asalto** donde la Amenaza no lo pide: solo
  suben uno que ya existe, con tope en 3.
- El escalado por jugadores (`×(1 + 0,4·(N−1))`, redondeado) se aplica al grupo de la
  categoría; los que se añaden son Saqueadores, Arqueros e Incendiarios por turnos, nunca
  un segundo Capitán. «A veces 2 Saqueadores» en categoría 2 es un 50 % (propuesta).
- Calendario y grupos salen de la semilla de la partida y del número de asalto: misma
  partida, mismos asaltos, también tras guardar y cargar.

### 2.4 Qué quieren

Orden de prioridad de saqueo dentro de la base: 1) metal trabajado y herramientas de
Nivel 3–4, 2) comida conservada o en vasija sellada, 3) medicina, 4) cualquier otro
objeto apilado fuera de un contenedor cerrado. Si un Incendiario no encuentra nada que
saquear (todo bajo llave) y el asalto es de categoría 2–3, cambia de objetivo a quemar
piezas de madera/bambú/palma de la base (§4.3).

### 2.5 Campamentos y barcos

Dos campamentos fijos (generados por semilla, no aleatorios entre partidas), en islas
ya marcadas como peligrosas o poco exploradas en el GDD (§4):

| Campamento ES / EN | Isla | Contenido |
|---|---|---|
| **Cala Rota** / **Broken Cove** | Los Dientes | Tienda de mando, hoguera, cofre de botín, 1 canoa de asalto varada |
| **Fondeadero Podrido** / **Rotten Anchorage** | Manglar de las Voces | Tienda de mando, hoguera, cofre de botín, 1 balandra negra fondeada |

`[Decisión]` Los Dientes no tienen playa: con la semilla oficial no hay ni una celda
entre 0 y 3 m, se pasa del agua al acantilado. Cala Rota se planta en la **cornisa llana
más baja** con agua a menos de 90 m, y su piragua queda a flote al pie de la cornisa en
lugar de varada. El sitio exacto sale de la semilla del mundo y se separa al menos 60 m
de los puntos de interés (`Raiders/RaiderCampsModel`).

| Barco ES / EN | Tripulación | Uso |
|---|---|---|
| **Piragua de asalto** / **Raiding canoe** | 2–3 | Patrullas rápidas y asaltos de categoría 1 |
| **Balandra negra** / **Black sloop** | 5–6 (con Capitán) | Asaltos de categoría 2–3, ruta entre los dos campamentos y las islas avanzadas |

### 2.6 Botín

| Objeto | Fuente |
|---|---|
| Metal trabajado (lingote de aluminio, chapa, remaches, alambre, clavos) | Cofre de campamento y barco (biblia §3.3) |
| Cuerda gruesa, vela de lona, brea | Cofre de campamento y barco |
| Medicina (alcohol destilado, antiséptico) | Cofre de campamento |
| **Machete pirata** (misma plantilla que el machete de Nivel 2, biblia §3.4, con nombre y textura propios) | Saqueador derrotado |
| **Capa de vigía** / **Lookout's cloak** — trofeo cosmético, sin estadísticas de armadura | Capitán derrotado (único por partida y por Capitán abatido) |

**Decisiones explícitas de exclusión** (resuelven lo que el GDD dejaba abierto):

- **Pólvora: se descarta.** El GDD nunca introduce combustión interna ni armas de
  fuego; los piratas atacan con brea encendida, no con pólvora. Mantenerla fuera evita
  abrir una rama tecnológica que ningún otro sistema del juego usa.
- **Sin mapas de tesoro como botín:** rompería la regla dura de que el mapa lo dibuja
  siempre el jugador, nunca el juego (GDD §3.2, biblia §9).
- **Sin monedas:** coherente con «economía sin tienda» (GDD §5); los piratas no
  comercian, solo se combate contra ellos.

---

## 3. El combate

### 3.0 Marco común: por qué no hay una barra de vida nueva

El juego ya tiene **Salud** (`Content/Data/survival_needs.json`, id `salud`): escala
0–100, empieza en 100, muerte a 0 (reaparición en fogata salvo en modo Náufrago), se
cura con objetos medicinales. Ya tiene también un sistema de **heridas** (`wounds` en
el mismo fichero): un corte tiene una **profundidad** de 0 a 1, sangra
`bleedDamagePerHour × profundidad` si no se trata, se infecta a las 12 horas sin vendar
ni limpiar, cicatriza en 36 horas vendado (× (1 + profundidad)), y una venda solo cubre
hasta profundidad 0.8 — más hondo exige sutura. El combate **reutiliza ambos sistemas
tal cual**, no crea una barra de vida ni un sistema de heridas paralelo:

- **Golpe cortante o perforante** (Filo o Punta del arma/animal, escala 0–5 igual que
  cualquier objeto de la biblia §2.1): pérdida instantánea de Salud = propiedad × 3;
  abre un corte con profundidad = propiedad ÷ 5, que sangra con las mismas constantes
  que cualquier otro corte del juego.
- **Golpe contundente** (Contundente del arma/animal): pérdida instantánea de Salud =
  propiedad × 4, sin sangrado; aturdimiento de 1.5 s si la propiedad es ≥ 4.
- **Picaduras y aguijones de fauna peligrosa** (raya, medusa): no tienen fórmula nueva,
  usan las constantes que ya existen en `survival_needs.json` §`stings` (§5).
- **Retroalimentación:** todo lo anterior dispara las señales corporales que ya existen
  (`docs/tecnico/cuerpo.md`): `BleedingPulse`, `Vignette` con causa «poca salud» o
  «sangrado», `Heartbeat` y `HandTremor`. El combate no añade HUD.
- **Esfuerzo en combate:** atacar cuenta como esfuerzo físico igual que correr — sube
  `Breathing`/`Heartbeat` (ya implementado); no hay una barra de estamina nueva. Más de
  3 golpes rápidos seguidos exige una pausa de recuperación de 0.4 s antes de encadenar
  más (ver §3.1), que es lo que impide el mash infinito, no un recurso consumible.

### 3.1 Cuerpo a cuerpo

- **Golpe rápido:** 0.45 s de ejecución, daño × 0.7, encadenable hasta 3 veces antes de
  una pausa obligatoria de 0.4 s.
- **Golpe cargado:** 1.2 s de carga con telegraph visual (el arma se echa hacia atrás,
  legible y esquivable) + 0.3 s de recuperación, daño × 1.6.
- **Esquiva:** movimiento lateral/atrás con 0.3 s de invulnerabilidad al arranque y 1.2
  s de reutilización; sin coste de recurso.
- **Alcance:** 1.2 m con cuchillo/hacha/machete, 2.2 m con lanza.
- **Daño de referencia por nivel de herramienta** (biblia §3.4, valores de propiedad
  reales del material, no el mínimo de la plantilla):

  | Arma (ejemplo por nivel) | Propiedad ofensiva | Daño instantáneo (golpe rápido) | Daño instantáneo (golpe cargado) |
  |---|---|---|---|
  | Cuchillo de lasca (N1) | Filo 3 | 6 | 14 |
  | Hacha de pedernal con mango (N2) | Filo 3 | 6 | 14 |
  | Lanza de bambú con punta de obsidiana (N3) | Punta 5 | 10 | 24 |
  | Cuchillo de obsidiana (N3) | Filo 5 | 10 | 24 |
  | Machete de aluminio (N4) | Filo 5 | 10 | 24 |
  | Martillo de piedra (contundente, cualquier nivel) | Contundente 3 | 8 | 19 |

### 3.2 A distancia

- **Arco:** daño = Punta de la flecha × 3, con caída por distancia: 100 % a menos de 15
  m, 70 % de 15 a 30 m, 40 % de 30 a 45 m, 0 % más allá.
- **Lanza arrojadiza:** daño = Punta × 3 − 1 (penalización por estabilidad de vuelo),
  alcance de lanzamiento 12 m; solo si Peso ≤ 3 (regla ya fijada en la plantilla,
  biblia §2.3).
- **Flecha incendiaria con brea** (ya existe en biblia §3.5, pesca): además del daño
  perforante, aplica «ardiendo» — 4 de daño/segundo durante 3 s — a objetivos
  inflamables (piratas con ropa, piezas de madera/bambú/palma de estructuras).

### 3.3 Trampas

Dos usos: caza/pesca (ya cubierto en biblia §3.5 y §4, sin cambios) y combate/defensa
(nuevas, §4.2).

- **Trampa de cuerda (red):** inmoviliza 3 s, sin daño; pensada para ganar ventaja
  contra un pirata antes de que llegue a alcance.
- **Ballesta de resorte:** activada por un hilo de tensión (reutiliza la propiedad Ata),
  20 de daño perforante instantáneo, un solo uso, se rearma con una flecha nueva.
- Las trampas de defensa de base (foso, estacas ocultas) se listan en §4.2 porque
  dependen de piezas de construcción, no son un sistema aparte.

### 3.4 Armadura

Reduce solo el componente de **daño instantáneo**, nunca el sangrado — una venda o
sutura sigue haciendo falta aunque se lleve armadura.

| Pieza | Reducción cortante/perforante | Reducción contundente | Coste/penalización |
|---|---|---|---|
| Sin armadura | 0 % | 0 % | — |
| Coraza de cuero curtido (N2: cuero curtido ×3 + cordel) | −15 % | 0 % | Ninguna |
| Peto de placas (N4: chapa + remaches + cuero) | −30 % | −10 % | −10 % velocidad de esprint |
| Piel de tiburón curtida (trofeo único de «El Errante», biblia §4.6) | −20 % | 0 % | Ninguna; no se puede fabricar de nuevo |

### 3.5 Tabla resumen de números

| Concepto | Valor |
|---|---|
| Salud del jugador | 0–100 (ya existente, `survival_needs.json`) |
| Corte: daño instantáneo | Filo/Punta × 3 |
| Corte: profundidad de la herida | Filo/Punta ÷ 5 |
| Contundente: daño instantáneo | Contundente × 4 |
| Aturdimiento por contundente | 1.5 s si Contundente ≥ 4 |
| Golpe rápido / cargado | ×0.7 / ×1.6 del daño base |
| Caída de precisión del arco | 100 % <15 m, 70 % 15–30 m, 40 % 30–45 m, 0 % >45 m |

---

## 4. La defensa de la base

### 4.1 Piezas [F2]

Piezas nuevas del kit de construcción ya existente (biblia §3.10), no un sistema
aparte — mismas reglas de encaje, integridad y degradación por clima.

| Pieza ES / EN | Vida | Nota |
|---|---|---|
| Cerca de estacas / Stake fence | 60 | Barata, no detiene: solo añade 1.5 s de cruce |
| Empalizada de madera / Wooden palisade | 140 | — |
| Muralla de piedra / Stone wall | 380 | Inmune al fuego |
| Puerta de muralla / Wall gate | 180 | Trabable por dentro; objetivo prioritario del Capitán |
| Torre de defensa / Defense tower | 320 | Plataforma a 4 m; +30 % alcance de visión, sin caída de precisión de arco hasta 25 m |

### 4.2 Trampas defensivas [F2, se activan contra piratas en F3]

| Trampa ES / EN | Efecto | Reactivación |
|---|---|---|
| Foso con estacas / Staked pit | Reutiliza la minería (GDD §3.4): sin vida propia, es terreno cavado. Cruzarlo sin puente hace 18 de daño instantáneo + 1 s de aturdimiento. Los piratas lo evitan si hay un camino alternativo (misma lógica que la fauna evitando agua profunda) | Permanente (es terreno) |
| Estacas ocultas / Hidden stakes | Cubiertas de hojas, invisibles hasta activarse o examinarse de cerca; 22 de daño perforante + 2 s de aturdimiento, un solo uso | 2 estacas + fibra |

### 4.3 Daño a estructuras y reparación

| Atacante | Daño por golpe a una pieza | Nota |
|---|---|---|
| Saqueador | 6 | Objetivo: contenedores abiertos |
| Incendiario (brea) | 10 de impacto + «ardiendo» 4/s durante 5 s a madera/bambú/palma | Piedra inmune; se apaga con agua (cubo, lluvia activa, <2 m del mar) o arena |
| Capitán | 12 | Prioriza puertas |

**Reparación:** misma acción que construir (biblia §2.4, «reparar es la misma acción
que crear»): 1 unidad del material base restaura el 20 % de la vida máxima de la pieza.

---

## 5. La fauna peligrosa

Sigue el marco de §3.0 (mismo formulario cortante/contundente que el combate contra
piratas); raya y medusa **ya están implementadas** con sus propias constantes en
`survival_needs.json` y no se tocan.

| Especie | Fase | Vida | Ataque | Daño | Comportamiento |
|---|---|---|---|---|---|
| Cerdo salvaje (jabalí) | [AA] | 35 | Embestida (Contundente 4-equiv.) | 16 + aturdimiento 1 s si no se esquiva | Pacífico si no se le molesta; huye bajo el 30 % de su vida (GDD §3.7) |
| Cabra montés salvaje | [AA] | 20 | Embestida leve si se la acorrala (Contundente 1-equiv.) | 4, muy rara | Normalmente solo huye |
| Cangrejo de los cocoteros (grande) | [AA] | 15 | Pinza si se le molesta (Contundente 2-equiv.) | 8 | Huye tras el primer golpe recibido |
| Tiburón de arrecife (genérico) | [AA] | 55 | Mordisco (Filo 4-equiv.) | 12 + corte de profundidad 0.6 | 15 % de posibilidad de volcar una balsa o canoa sin balancín por embestida si el jugador va a bordo |
| Raya | [AA, ya implementado] | — | Aguijón solo si se la pisa o ataca | `RayDamagePerHour` 3 durante 12 h, profundidad 0.35 | Huye si no se la molesta; antídoto de corteza cura |
| Medusa | [AA, ya implementado] | — | Contacto pasivo a la deriva | `JellyfishDamagePerHour` 1 durante 6 h | Vinagre cura |
| **Sombra** — tiburón tigre legendario (biblia §4.6) | [AA] | 150 | Mordisco (fuera de escala, es un jefe) | 22 + corte de profundidad 0.9 (exige sutura) | Ataca embarcaciones pequeñas; único por partida |

---

## TODO de implementación

### Personajes humanos (§0)

- [ ] [F3] Importar un esqueleto humanoide compatible con Mixamo desde un pack CC0/licencia permisiva (tipo KayKit Adventurers) y verificar el retargeting con el IK Retargeter de UE5 — módulo `Villages`/`Raiders`.
- [ ] [F3] Retargetear 10 animaciones de Mixamo (ralentí, caminar, trabajar, saludar, ofrecer, huir, ataque cuerpo a cuerpo, disparo de arco, reacción a impacto, caída) sobre el esqueleto elegido.
- [ ] [F3] Modelar/ajustar la cara low-poly sin rig facial (sin ojos ni boca articulados) y validar legibilidad de silueta a 20 m.
- [ ] [F3] Crear dos paletas de material (navegantes: paño/tierra; piratas: cuero oscuro/metal) sobre la misma malla base.

### Navegantes (§1)

- [ ] [F3] Nuevo módulo `Villages`: spawn de la aldea (Arenas Blancas, 10 NPC) y el puesto de trueque (La Meseta, 4 NPC) con los roles de §1.1.
- [x] [F3] Sección `reputation` en `Save` (por asentamiento, 0–100, sin decaimiento pasivo).
- [ ] [F3] Prop nuevo «tablón de peticiones» (malla + rotación de icono cada 4 días de juego) reutilizando `story_es.json.petroglyph_themes`.
- [x] [F3] Lógica de trueque: valor 1–5 por objeto (tabla §1.4) × tasa de reputación (§1.5), ventana horaria 8:00–18:00.
- [ ] [F3] Enganchar «devolver objeto ritual» a `Ruins` (+5 reputación, sin trueque de por medio).
- [ ] [F3] Wayfinding enseñado por el guardián del marae con reputación ≥70 (biblia §9.2), sin duplicar entre Arenas Blancas y La Meseta.
- [ ] [F3] Aldeanos invulnerables al daño de arma (solo reacción de huida + penalización de reputación).
- [x] [F3] Enfriamiento de 15 días de juego cuando la reputación cae por debajo de 20.
- [ ] [F3] Añadir a `Content/Data/achievements.json`: «Primer trueque»/«First Trade», «Amigos del arrecife»/«Friends of the Reef».

### Piratas (§2)

- [ ] [F2] Piezas de muralla/torre/puerta nuevas en `Content/Data/building_pieces.json` (tabla §4.1), reutilizando el sistema de integridad ya existente en `Building`.
- [ ] [F2] Trampas de defensa (estacas ocultas) como pieza colocable con verbo «Rearmar».
- [ ] [F3] Nuevo módulo `Raiders`: percepción reutilizando `Fauna`, patrulla por semilla entre los dos campamentos, 4 tipos de pirata (§2.1) con sus daños y vidas.
- [x] [F3] Contador `Amenaza pirata` (0–100) en `Save`, con las reglas de subida/bajada de §2.3.
- [x] [F3] Programador de asaltos: categoría según Amenaza, condición de recursos visibles/reputación Hostil, aviso previo (humo + tambor).
- [x] [F3] Generar por semilla los dos campamentos fijos (Cala Rota en Los Dientes, Fondeadero Podrido en el Manglar) con cofre de botín y barco propio.
- [ ] [F3] Dos plantillas de barco pirata (Piragua de asalto, Balandra negra) sobre el mismo `FBoatModel` que el resto de embarcaciones.
- [ ] [F3] Botín: nueva skin «Machete pirata» (misma plantilla que el machete de Nivel 2) y accesorio cosmético único «Capa de vigía» al derrotar a un Capitán.
- [ ] [F3] Daño a estructuras por tipo de atacante (tabla §4.3), incluida la propagación «ardiendo» sobre piezas inflamables y su apagado con agua/arena.
- [ ] [F3] Añadir a `Content/Data/achievements.json`: «Asalto repelido»/«Raid Repelled», «Muralla de piedra»/«Stone Wall», «Cala Rota»/«Broken Cove».

### Combate y fauna (§3, §5)

- [x] [AA] Fórmulas de daño instantáneo (cortante/perforante ×3, contundente ×4) leyendo la propiedad Filo/Punta/Contundente real del objeto, no el mínimo de la plantilla.
- [x] [AA] Apertura de corte con profundidad = propiedad ÷ 5 enganchada al sistema `wounds` ya existente en `BodyModel`/`survival_needs.json` (sin crear una segunda barra de heridas).
- [x] [AA] Golpe rápido (×0.7, encadenable ×3 + pausa 0.4 s) y golpe cargado (×1.6, telegraph 1.2 s) como dos variantes de la misma acción contextual de ataque.
- [x] [AA] Esquiva con invulnerabilidad de 0.3 s y reutilización de 1.2 s.
- [x] [AA] Caída de precisión del arco por distancia (100/70/40/0 %).
- [x] [AA] Estadísticas de combate de cerdo salvaje, cabra montés y cangrejo de los cocoteros (tabla §5) en el módulo `Fauna` ya existente.
- [x] [AA] Tiburón de arrecife genérico como variante no legendaria del tiburón tigre «Sombra» ya descrito en biblia §4.6, con las estadísticas de la tabla §5.
- [ ] [F2] Tres piezas de armadura nuevas (coraza de cuero, peto de placas) en `Content/Data/items.json`/`templates.json`, más el trofeo único «Piel de tiburón curtida» ya previsto como recompensa de «El Errante» (biblia §4.6).
- [ ] [F2/F3] Verificar en `Tools/DataCheck` que ninguna combinación de daño nuevo rompe el invariante «ninguna plantilla produce un objeto sin malla» (biblia §12).
