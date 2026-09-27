# 06 — Interfaces

Biblia de diseño de «Explored» · sección UX/UI · 2026-09-27 · UE 5.6, HUD y menús en
Slate (`Source/Explored/UI/`). Manda `docs/diseno/gdd_v2.md`; complementa
`docs/design/biblia-de-contenido.md` (catálogo de objetos y verbos) y no repite su
contenido. Fase de cada elemento: **[AA]** acceso anticipado, **[F2]**, **[F3]**
(`gdd_v2.md` §6). Donde una pantalla ya existe en código se cita el fichero fuente
como referencia canónica; donde no existe, esta sección es la especificación a
implementar.

---

## 0. Principios rectores

1. **Diegético y mínimo, como en Sea of Thieves.** El HUD no es un panel de control:
   es la última red de seguridad cuando el cuerpo o el propio mundo ya no bastan para
   leer la situación. Nada aparece si no hace falta.
2. **Umbral del 55 %.** Las barras de necesidades solo se dibujan por debajo de
   `VisibleThreshold = 0.55f` (`Source/Explored/UI/ExploredHUD.cpp:248`), con opacidad
   creciente cuanto peor está la necesidad. Cualquier indicador nuevo que compita por
   atención de pantalla sigue el mismo criterio salvo que este documento diga lo
   contrario.
3. **Máximo tres verbos.** `UInteractionComponent::UpdateFocus` recorta a tres los
   verbos de contexto (`Source/Explored/Interaction/InteractionComponent.cpp:111-114`)
   y `UCraftingLibrary::MaxActions` hace lo mismo con las combinaciones. Ninguna
   pantalla nueva de esta sección ofrece más de tres opciones simultáneas por objeto
   enfocado.
4. **El mapa lo dibuja el jugador.** Nunca hay minimapa, brújula de pantalla completa
   ni marcador de misión. Los instrumentos de precisión (brújula, catalejo, sextante,
   reloj) son objetos que se llevan en la mano o en la muñeca, no capas de HUD
   permanentes.
5. **Papel y tinta como lenguaje visual único.** Cita literal del código
   (`Source/Explored/UI/Widgets/SExploredAchievementToast.h:10`): «con el mismo papel
   y tinta que el resto del frontend». Todas las pantallas —título, ajustes, mapa,
   museo, logros— comparten esa piel; no hay una paleta de «UI de sistema» distinta de
   la del mapa.
6. **Sin números salvo que ya excepcionen la regla.** El reloj de muñeca da la hora en
   palabras y niveles gruesos, nunca en cifras de necesidad (`SExploredWristWatch.h`).
   Las excepciones ya existentes en código (metros de sedal, minutos de partida en
   ajustes) se mantienen porque son medidas de un instrumento, no un contador de
   supervivencia.
7. **Textos ES/EN cortos y nativos.** Sin frases de manual, sin relleno. El inglés lo
   revisa oído nativo antes de doblar (fuera del alcance de este documento, pero se
   escribe ya con ese objetivo).

---

## 1. Guía visual de la UI

### 1.1 Tipografías (Google Fonts, licencia OFL)

| Uso | Fuente | Por qué |
|---|---|---|
| Titulares, sellos del mapa, nombres de logro | **Caveat** (Bold) | Cursiva manuscrita legible a tamaño grande; es la «letra de Almudena/del náufrago» en portadas y sellos, nunca en botones pequeños (ilegible por debajo de ~20 px). |
| Cuerpo de menús, HUD, botones, listas | **Nunito Sans** | Sans humanista, redondeada, muy legible a tamaño pequeño y en negrita; casa con el low-poly estilizado sin parecer sacada de una app de oficina. Única fuente con la que se garantiza accesibilidad (§3). |
| Fragmentos de sabor: notas de la expedición de 1974, entradas de diario del campamento Halden | **Special Elite** | Textura de máquina de escribir, coherente con «restos de una expedición científica de 1974» (`gdd_v2.md` §0). Uso decorativo y puntual **solo** en bloques de texto narrativo ya maquetados como cita; nunca en un control interactivo. |

Regla dura: **nunca** una tercera fuente sin pasar antes por esta tabla. Las tres
cubren cabecera, cuerpo y sabor; añadir una cuarta rompe la coherencia de «un único
cuaderno» que sostiene toda la UI.

### 1.2 Paleta

Base «papel y tinta» (todas las pantallas de menú y HUD):

| Token | Hex aprox. | Uso |
|---|---|---|
| `paper.base` | `#F1E7D0` | Fondo de paneles de menú (ajustes, pausa, museo, logros, créditos). |
| `paper.edge` | `#E4D3AC` | Bordes, pestañas inactivas, líneas de doblez. |
| `ink.strong` | `#2B2016` | Texto principal, iconografía de sello. |
| `ink.muted` | `#6B5A44` | Texto secundario, descripciones, marcas de agua. |
| `accent.brass` | `#B98A3D` | Foco de teclado/mando, selección activa, barra de progreso neutra (logros, museo). |
| `accent.reef` | `#3C9C8B` | Reputación con el pueblo del arrecife, trueque favorable **[F3]**. |
| `accent.danger` | `#8C2A20` | Avisos de asalto pirata, peligro de derrumbe/aire viciado, salud crítica. Rojo apagado, nunca neón: coherente con el resto de la paleta terrosa. |

Colores de urgencia — **reutilizados tal cual de código**, no se inventan de nuevo
(`Source/Explored/UI/ExploredHUD.cpp:239-243`):

| Necesidad | Color en código (`FLinearColor`) | Hex aprox. |
|---|---|---|
| Hambre | `(0.80, 0.55, 0.20)` | `#E7C47C` |
| Sed | `(0.30, 0.55, 0.90)` | `#95C4F3` |
| Energía | `(0.85, 0.80, 0.30)` | `#EDE795` |
| Sueño | `(0.55, 0.45, 0.85)` | `#C4B3ED` |
| Temperatura | `(0.90, 0.35, 0.20)` | `#F3A07C` |

**[LD]** Cualquier nueva barra de necesidad (p. ej. una futura barra de aire viciado en
minas, §3.4 del GDD) reutiliza este mismo sistema de columna de 5 colores en vez de
inventar uno nuevo: mantiene la paleta cerrada y reconocible.

### 1.3 Iconografía

No hay iconos de HUD de sistema (sin barra de vida con corazones, sin inventario en
rejilla con iconos). Lo único «icónico» del juego son **sellos de tinta**:

- Marcas del mapa (`Content/Data/story_es.json` → `map_marks`: agua dulce, cueva,
  peligro, recurso, ruina, resto del Albatros) y los logros: silueta de un solo trazo,
  estilo grabado/sello de caucho, un único tono de tinta (`ink.strong`) salvo el sello
  de peligro, que añade `accent.danger` como segundo tono.
- Nunca un icono vectorial plano de app. Si hace falta representar un objeto sin
  texto (ficha del museo sin identificar), es una **silueta rellena**, no un pictograma
  esquemático.
- Los bocetos de receta al margen del mapa (`SExploredDoodle`) ya siguen esta regla:
  trazos deterministas por id, no arte pre-dibujado.

### 1.4 Animación y lenguaje de movimiento

| Elemento | Animación | Duración | Nota |
|---|---|---|---|
| Apertura de menú de pantalla completa (Título, Pausa, Ajustes, Créditos, Logros, Museo) | Sube 40 px + rotación 1,5° con easing de salida, como pasar una página | 220 ms | Con «Reducir movimiento» (§3), sin desplazamiento ni rotación: solo fundido. |
| Mapa en las manos | Sube desde abajo con ladeo, ya implementado en 3D (`SExploredMapInHands.h:20-22`) | — | No es UI 2D: es la propia cámara/manos del personaje. Sin cambios. |
| Aviso de logro | Entra con un «sello» — escala 1.15→1.0 en 90 ms + sonido de estampado — y sale deslizando 3 s después | 90 ms entrada / 400 ms salida | Con «Reducir movimiento», solo fundido, sin el rebote de escala. |
| Notificaciones de recogida | Fundido en el último cuarto de vida (ya en código, `ExploredHUD.cpp:207-212`) | — | Sin cambios. |
| Fundido a negro (carga, transiciones) | `SExploredFade` ya implementado | Variable | Sin cambios. |
| Vista previa fantasma de construcción | Pulso de opacidad 0.6↔0.85 en la pieza válida | 900 ms loop | Con «Reducir movimiento», opacidad fija en 0.75 sin pulso. |

### 1.5 Layout maestro del HUD

Todas las zonas en juego, superpuestas, para que ninguna pantalla nueva choque con una
ya existente:

```
┌──────────────────────────────────────────────────────────────┐
│ [Hambre]                                                       │  ← barras de necesidad
│ ▓▓▓▓░░░░  Margen 24 px, solo si <55 % (ExploredHUD.cpp:249-267)│
│ [Sed]                                                           │
│ ▓▓░░░░░░                                                        │
│                                                                  │
│                                                                  │
│                        (zona de notificaciones · Y = 0.42×alto) │
│                                                                  │
│                              ·                                  │
│                         (retícula, centro)                      │
│                              ·                                  │
│                                                                  │
│                 [E] Talar el árbol · Recoger                    │  ← prompt de contexto
│                        (Y = 0.60×alto, máx. 3 verbos)            │
│                                                                  │
│              (barra de tensión de pesca, Y = 0.70×alto)          │
│                                                                  │
│                                                                  │
│  Rama seca                                     Pico de piedra    │  ← manos (margen 24 px)
└──────────────────────────────────────────────────────────────┘
       ↑ esquina inf. izq: «Guardando…» (SavingIndicator)     ↑ esquina sup. der.: logro (toast)
```

**[LD]** Decisión de layout para lo nuevo de este documento:

- **Brújula:** nunca una tira permanente de HUD. Solo aparece **sustituyendo la
  etiqueta de la mano** que la sostiene (misma zona que «Rama seca» arriba): un dial
  de aguja dibujado a tinta con los puntos cardinales en vez del nombre del objeto.
  Coherente con el pilar «el mapa lo dibuja el jugador»: un HUD de brújula fijo
  convertiría el propio instrumento en decoración.
- **Estado del barco:** fila nueva justo encima de las etiquetas de mano, **solo
  mientras se está a bordo** y **solo si hay algo que decir** (mismo criterio del
  umbral del 55 %: vela mal trimada, casco por debajo del 40 % o haciendo agua). Con
  la vela bien trimada y el casco sano, la fila no existe — el propio barco (crujido,
  ángulo de escora, chorro de agua entrando) ya lo cuenta.
- **Fila de combinar (Crafteo, §2.6):** ocupa la misma posición Y que el prompt de
  contexto (0.60×alto) porque nunca coexisten (uno pide `E`, el otro `C`, y las dos
  manos ocupadas excluyen tener un interactuable de recolección enfocado con las
  manos llenas en la práctica habitual).

---

## 2. Pantallas

### 2.1 Título — **[AA]**

**Propósito:** primera pantalla del juego; arranca el tono (dedicatoria) y da paso a
Nueva partida / Cargar / Ajustes / Créditos / Salir. Referencia:
`Source/Explored/UI/Widgets/SExploredMainMenu.h`.

```
┌──────────────────────────────────────────────────────────────┐
│                                                                  │
│                         E X P L O R E D                         │
│                    (Caveat Bold, grande, tinta)                 │
│                                                                  │
│                 「Dedicado a Almudena, mi Limón.」               │
│                                                                  │
│                        Nueva partida                            │
│                        Cargar partida                           │
│                        Ajustes                                  │
│                        Créditos                                 │
│                        Salir                                    │
│                                                                  │
└──────────────────────────────────────────────────────────────┘
```
Fondo: escena 3D del archipiélago al atardecer, cámara lenta orbital sobre la Isla del
Amaraje (`SExploredMenuCamera`), nunca una imagen estática.

**Elementos:** logotipo manuscrito, dedicatoria (siempre visible, no se puede quitar
en ajustes — es intencional, no un descuido de accesibilidad), lista vertical de 5
entradas. «Cargar partida» deshabilitado (atenuado, sin acción) si no existe ninguna
ranura legible.

**Estados/transiciones:** Nueva partida → 2.2 Nueva partida. Cargar partida → 2.14 §
Guardado en modo Cargar (`ESaveSlotsMode::Load`). Ajustes → 2.15. Créditos → 2.16.
Salir → confirmación simple («¿Salir de Explored?» Sí/No) → cierra la aplicación.

**Entrada — teclado/ratón:** flechas arriba/abajo o ratón sobre la entrada, Intro o
clic para confirmar. **Entrada — mando:** cruceta o stick izquierdo, `A`/Face Bottom
confirma, `B`/Face Right no hace nada aquí (no hay «atrás» desde el título).

**Textos:**

| ES | EN |
|---|---|
| Nueva partida | New game |
| Cargar partida | Continue |
| Ajustes | Settings |
| Créditos | Credits |
| Salir | Quit |
| ¿Salir de Explored? | Quit Explored? |
| Sí | Yes |
| No | No |

**Sonido:** ambiente de oleaje lejano en bucle; navegación con un «paso de página»
seco y breve; confirmar con un chasquido de brújula abriéndose.

---

### 2.2 Nueva partida: semilla y dificultad — **[AA]**

**Propósito:** elegir semilla del archipiélago y modo de juego antes de generar el
mundo. Referencia: `Source/Explored/UI/Widgets/SExploredModeSelect.h` (selección de
modo); el campo de semilla es una ampliación de esa misma pantalla, no una pantalla
aparte — evita un paso extra de navegación.

```
┌──────────────────────────────────────────────────────────────┐
│  ← Volver                                                       │
│                                                                  │
│   Semilla:  [___________________]  🎲 Aleatoria                 │
│                                                                  │
│   ○ Explorador     Sin apuros. Necesidades lentas, para mirar   │
│                      el archipiélago sin presión.                │
│   ● Superviviente   El ritmo pensado del juego (recomendado).    │
│   ○ Náufrago        Sin reaparición: la partida termina si       │
│                      mueres. Necesidades más rápidas.            │
│   ○ Personalizado    [Editar…] → velocidad de necesidades,      │
│                      caída, daño, reaparición, una por una.      │
│                                                                  │
│                        Empezar                                  │
└──────────────────────────────────────────────────────────────┘
```

**Elementos:** campo de texto de semilla (alfanumérico, máx. 24 caracteres; vacío =
aleatoria al pulsar «Empezar», y el campo se rellena con la semilla generada para que
se pueda copiar); botón de dado que sortea una semilla legible («ISLA-4F2K»);
cuatro filas de modo con nombre, descripción de una línea y selector de radio
(`EExploredGameplayMode`, `SExploredModeSelect.cpp: MakeModeRow`); «Personalizado»
despliega un panel con los mismos deslizadores del modo Superviviente escalados
(`survival_needs.json → customNeedSpeed`, rango 0.25–3.0).

**Estados/transiciones:** «Empezar» deshabilitado hasta que haya un modo elegido
(siempre lo hay: Superviviente por defecto). Al confirmar → fundido a negro → 2.3
Carga. «Volver» o Escape/B → 2.1 Título.

**Entrada — teclado/ratón:** clic o Tab/flechas entre filas, Intro en un radio la
selecciona, clic en el campo de semilla para escribir. **Entrada — mando:** cruceta
arriba/abajo entre filas, `A` selecciona el modo o abre «Editar…», teclado en pantalla
del sistema para la semilla, `B` vuelve.

**Textos:**

| ES | EN |
|---|---|
| Semilla | Seed |
| Aleatoria | Random |
| Explorador | Explorer |
| Sin apuros. Necesidades lentas, para mirar el archipiélago sin presión. | No rush. Slow needs, made for taking in the archipelago. |
| Superviviente | Survivor |
| El ritmo pensado del juego (recomendado). | The game's intended pace (recommended). |
| Náufrago | Castaway |
| Sin reaparición: la partida termina si mueres. Necesidades más rápidas. | No respawn: the run ends if you die. Faster needs. |
| Personalizado | Custom |
| Editar… | Edit… |
| Empezar | Start |

**Sonido:** el dado de semilla suena a hueso/concha al tirarse (coherente con
utensilios del juego, nunca un dado de plástico); cambiar de modo, el mismo «paso de
página» del título.

---

### 2.3 Carga — **[AA]**

**Propósito:** cubrir la generación del archipiélago (o la carga de una partida) sin
enseñar nada roto. Referencia: `SExploredFade`.

```
┌──────────────────────────────────────────────────────────────┐
│                                                                  │
│                                                                  │
│                                                                  │
│                                                                  │
│                    (negro, sin barra de progreso)                │
│                                                                  │
│              Trazando la costa de la Isla del Amaraje…           │
│                                                                  │
│                                                                  │
└──────────────────────────────────────────────────────────────┘
```

**Elementos:** fundido a negro puro; una sola línea de texto centrada en `Caveat`,
tenue, que cambia según la fase real de carga (terreno, vegetación, fauna, guardado) —
nunca un porcentaje ni una barra: no hay presupuesto de trabajo fiable para estimar
tiempo restante en generación procedural, y una barra que miente es peor que no
prometer nada. Nunca una pista de carga genérica ajena al mundo del juego.

**Estados/transiciones:** aparece tras «Empezar» (2.2) o «Cargar» de una ranura
(2.14); desaparece con `SExploredFade::FadeFromBlack` en cuanto el mundo está listo,
directo al HUD en juego (2.4). Si la carga falla (partida dañada sin copia legible),
sustituye el texto por el aviso de error y ofrece «Volver» al selector de ranuras —
nunca se queda colgada sin salida.

**Entrada:** ninguna durante la carga normal; si falla, «Volver» con Intro/`A` o
Escape/`B`.

**Textos** (una frase de la lista actual, rotan; añadir más al catálogo con el tiempo):

| ES | EN |
|---|---|
| Trazando la costa de la Isla del Amaraje… | Charting the coast of Landing… |
| Plantando la primera palma… | Planting the first palm… |
| Despertando a la fauna… | Waking the wildlife… |
| Guardando el rumbo… | Saving your course… |
| No se pudo leer la partida. | Couldn't read this save. |

**Sonido:** silencio o, como mucho, el mismo oleaje lejano del título a volumen bajo;
nunca música de carga nueva (evita un cambio de capa más que gestionar).

---

### 2.4 HUD en juego — **[AA]**

**Propósito:** la interfaz permanente durante el juego, reducida al mínimo
imprescindible (principio 1). Referencia: `AExploredHUD::DrawHUD`
(`Source/Explored/UI/ExploredHUD.cpp:26-47`).

Wireframe: ver el layout maestro (§1.5). Elementos ya implementados, sin cambios de
diseño (solo se documentan aquí como referencia): retícula (`DrawReticle`), prompt de
contexto (`DrawContextPrompt`), etiquetas de mano (`DrawHandLabels`), barra de pesca
(`DrawFishing`), barras de necesidad (`DrawBodyNeeds`), notificaciones
(`DrawNotifications`), reloj de muñeca (`SExploredWristWatch`, tecla `H` / D-Pad
arriba, mantener).

**Elementos nuevos de este documento:**

- **Brújula en mano** — cuando la mano lleva el objeto «Brújula», su etiqueta de texto
  se sustituye por un dial circular pequeño (36 px), aguja fija al norte del mundo,
  trazado a tinta. Sin marcador de isla ni de objetivo: solo el norte, igual que un
  instrumento real.
- **Estado del barco** — fila entre las barras de necesidad y las manos, visible solo
  a bordo y solo si hay algo urgente:

  | Condición | Texto (aparece solo si se cumple) |
  |---|---|
  | Vela mal trimada (>20° de desfase con el viento) | «La vela tira mal» / *«Sail's fighting the wind»* |
  | Casco por debajo del 40 % | «El casco se raja» / *«Hull's giving way»* |
  | Haciendo agua (velocidad de inundación > achique) | «Entra agua» / *«Taking on water»* |
  | Capotado | «Volcado — nada hacia el casco» / *«Capsized — swim to the hull»* |

**Estados/transiciones:** cada elemento aparece/desaparece por su propia condición
(umbral de necesidad, foco de interacción, mano con brújula, a bordo con problema);
no hay una transición de pantalla completa — es el estado por defecto del juego.

**Entrada:** ver tabla completa en §2.8 (Enhanced Input, `ExploredCharacter.cpp:465-
511`) — se resume aquí lo que toca directamente al HUD:

| Acción | Teclado/ratón | Mando |
|---|---|---|
| Interactuar / coger / talar / picar | `E` | Face Left (X/□) |
| Usar mano derecha | Clic izquierdo | Gatillo derecho |
| Usar mano izquierda | Clic derecho | Gatillo izquierdo |
| Soltar | `G` | D-Pad abajo |
| Combinar | `C` | Face Top (Y/△) |
| Mochila | `Tab` | D-Pad derecha |
| Reloj de muñeca (mantener) | `H` | D-Pad arriba |
| Mapa | `M` | View / Special Left |
| Pescar (lanzar/recoger) | `F` | — *(hueco, ver TODO)* |
| Recoger/soltar sedal | `R` / `T` | Gatillos (eje) |

**Textos:** los de `DrawBodyNeeds` y `DrawNotifications` ya existen en código
(`NSLOCTEXT("ExploredUI", ...)`, `ExploredHUD.cpp:239-303`) y no se repiten aquí.

**Sonido:** cada indicador hereda el sonido de su propio sistema (mordisco de hambre,
trago de sed, bostezo de sueño ya definidos en Survival); el HUD en sí no añade
sonido propio, coherente con «diegético y mínimo».

---

### 2.5 Mochila e inventario — **[AA]**

**Propósito:** vista de solo lectura de lo que se lleva encima mientras la mochila
está abierta. Referencia: `Source/Explored/UI/Widgets/SExploredInventoryPanel.h`.

**[LD] Decisión de diseño heredada, reafirmada aquí:** no es una rejilla de iconos
tipo Minecraft. Es una **etiqueta de papel** con texto: manos, bolsillos, cinturón,
bolsa estanca, mochila y angarillas, cada compartimento con lo que contiene, huecos
libres, volumen y el peso frente a la capacidad cómoda. Coherente con «sin HUD
recargado»: el inventario es información, no un tablero de items.

```
┌────────────────────────────┐
│  Mano izq.: Rama seca        │
│  Mano der.: Pico de piedra   │
│  ──────────────────────      │
│  Bolsillos (2/4)             │
│    · Fibra de coco ×3         │
│    · Yesca de hongo           │
│  Cinturón (1/2)               │
│    · Cuchillo de obsidiana    │
│  Mochila (5/9) · 6,2/10 kg    │
│    · Madera dura ×4            │
│    · Coco ×2                   │
│    · …                          │
│  Peso: cómodo                  │  ← verde / ámbar / rojo, con texto siempre
└────────────────────────────┘
```

**Elementos:** una sección por compartimento (se oculta la que no se posee todavía,
p. ej. sin bolsa estanca no aparece esa fila); contador de huecos usados/totales por
sección; línea de peso con palabra de estado (nunca solo color, por accesibilidad
—`ComputeSignature`/`Rebuild`, `SExploredInventoryPanel.h:16-22`).

**Estados/transiciones:** aparece con `Tab`/D-Pad derecha mientras se mantiene
pulsado o alternado (a decidir en implementación: ver TODO — hoy es alternable,
`bBackpackOpen` en `ExploredCharacter.cpp:792-797`); el mundo **no se pausa**, el
personaje se sigue moviendo con la mochila abierta (mismo criterio que el mapa,
§2.8). Se cierra con la misma tecla, con Escape o con `B`.

**Entrada:** `Tab` abre/cierra; sin navegación interna — es solo lectura, no hay foco
que mover. Con mando, D-Pad derecha abre/cierra igual.

**Textos:**

| ES | EN |
|---|---|
| Mano izq. / Mano der. | Left hand / Right hand |
| Bolsillos | Pockets |
| Cinturón | Belt |
| Bolsa estanca | Dry bag |
| Mochila | Backpack |
| Angarillas | Litter |
| Peso: cómodo / cargado / al límite | Load: light / loaded / maxed out |

**Sonido:** roce de tela al abrir/cerrar, un único «frufrú» corto; sin sonido por
cada línea (evita ruido repetitivo con inventarios grandes).

---

### 2.6 Crafteo — **[AA]**

**Propósito:** resolver la ambigüedad cuando combinar dos objetos (`C`) admite más de
un verbo válido. Referencia directa al hueco ya señalado en código:
`AExploredCharacter::HandleCombine` (`Source/Explored/Player/ExploredCharacter.cpp:
758-790`), que hoy aplica siempre `Verbs[0]` y deja el comentario «aquí se aplica el
primero para no bloquear el bucle de fabricación […] FindActions ya deja listos los
candidatos para esa pantalla futura». Esta sección **es** esa pantalla.

**[LD] Decisión:** no es una rejilla de crafteo con receta y lista de materiales tipo
Minecraft — el juego no tiene esa mecánica (`gdd_v2.md` §5: «no hay tienda»; la
biblia de contenido resuelve todo por combinación de dos objetos en las manos con un
verbo de `verbs.json`). Con un solo verbo posible, `C` aplica al instante como ya
hace el código, sin pantalla. Con 2–3 verbos posibles (el máximo de
`UCraftingLibrary::MaxActions`), se abre una lista efímera en la misma zona Y del
prompt de contexto (§1.5): nunca compiten porque una pide manos llenas y foco vacío,
la otra manos llenas y verbo ambiguo.

```
Con un único verbo (caso normal — sin pantalla):
┌──────────────────────────────────────────────────────────────┐
│                    Tallado: Palo recto → Estaca                 │  ← notificación, 2 s
└──────────────────────────────────────────────────────────────┘

Con verbo ambiguo (2–3 candidatos):
┌──────────────────────────────────────────────────────────────┐
│                                                                  │
│                    ¿Cómo combinarlos?                            │
│              [1] Tallar    [2] Atar    [3] Afilar                │
│                                                                  │
└──────────────────────────────────────────────────────────────┘
```

**Elementos:** título corto de una línea; hasta 3 opciones en fila, cada una con la
tecla/número y el nombre del verbo (`verbs.json → nameEs/nameEn`, reutilizado tal
cual, no se inventa vocabulario nuevo); nada de vista previa del objeto resultante
—mismo criterio de «sin spoiler de resultado» que ya aplica en el resto del juego
(coherente con «hecho de código y descubrimiento», sin listas de recetas explícitas).

**Estados/transiciones:** se abre al pulsar `C` si `FindActionsInWorld` devuelve más
de un verbo; **no consume ningún objeto hasta que se confirma** una opción (a
diferencia del comportamiento actual, que aplica `Verbs[0]` de inmediato — cambio de
diseño necesario, ver TODO). Elegir una opción aplica `ApplyInWorld` con ese verbo y
cierra con la notificación de resultado normal («Tallado: X → Y», mismo patrón que
`HandleItemPickedUp`, `ExploredHUD.cpp:165-170`). Si pasan 4 segundos sin elegir, se
cierra sola sin aplicar nada (no bloquea el juego: el personaje se puede seguir
moviendo mientras la lista está abierta, igual que la mochila).

**Entrada — teclado/ratón:** `1`/`2`/`3` elige directamente; `C` de nuevo cicla a la
siguiente opción y una pulsación corta de confirmación (Intro) aplica la resaltada;
Escape cancela. **Entrada — mando:** Face Bottom/Right/Left (A/B/X o equivalente,
mapeo a decidir para no chocar con Interactuar/Saltar/Bucear ya ocupados — ver TODO)
o D-Pad izquierda/derecha para resaltar + Face Top confirma; `B` cancela.

**Textos:**

| ES | EN |
|---|---|
| ¿Cómo combinarlos? | How do you want to combine them? |
| Tallado: {0} → {1} | Carved: {0} → {1} |
| Atado: {0} → {1} | Lashed: {0} → {1} |
| (una entrada por verbo de `verbs.json`, mismo patrón «{Verbo}: origen → resultado») | (same pattern in English) |

**Sonido:** abrir la lista, un papel al desplegarse muy breve (100 ms); elegir,
el sonido propio del verbo (golpe, corte, tensado de atado — ya deben existir por
verbo en el sistema de crafteo); cancelar o expirar, sin sonido.

---

### 2.7 Modo construcción — **[AA]**

**Propósito:** colocar piezas del kit de construcción con encaje en rejilla.
Referencia: `Source/Explored/Building/BuildPreviewComponent.cpp`.

```
┌──────────────────────────────────────────────────────────────┐
│                                                                  │
│                                                                  │
│                    (vista previa fantasma 3D                    │
│                     de la pieza, en el mundo)                   │
│                                                                  │
│                                                                  │
│                                                                  │
│         Pared de bambú · [X]/[Z] cambiar · [R] rotar             │
│              Clic para colocar · [B] salir                       │
└──────────────────────────────────────────────────────────────┘
```

**Elementos:** la propia pieza fantasma (translúcida, `ink.strong` al 60 % si el
encaje es válido, `accent.danger` con **trama de rayas diagonales** además del color
si no hay apoyo suficiente — la trama es obligatoria, no solo el color: ver
accesibilidad §3); una línea de ayuda con el nombre de la pieza actual y los tres
controles activos, en el mismo estilo tipográfico que el prompt de contexto (no es
una pantalla nueva, es una extensión del HUD mientras el modo está activo).

**Estados/transiciones:** `B` (`IA_BuildToggle`) entra y sale del modo
(`BuildPreviewComponent.cpp:63`); dentro del modo, `X`/`Z` ciclan piezas
(`IA_BuildNext`/`IA_BuildPrevious`), `R` rota 90° (`IA_BuildRotate`), clic izquierdo
confirma (`IA_BuildConfirm`, no consume el clic para que fuera del modo siga
sirviendo para usar la mano derecha). Si la pieza requiere una ya construida
(`requiresPieces`, `building_pieces.json`) y no existe, el fantasma se mantiene en
rojo con el aviso «Necesita: {pieza}» en vez de dejar confirmar.

**Entrada — teclado/ratón:** `B` entra/sale, `R` rota, `X`/`Z` cambia de pieza, clic
izquierdo confirma. **Entrada — mando:** sin mapeo hoy (hueco real, ver TODO);
propuesta — `B` del mando ya está ocupado (agacharse/bucear y «Volver» de menús), así
que entrar/salir del modo construcción en mando debe ir a un botón libre en este
contexto: bumper izquierdo (`LB`) entra/sale, bumper derecho (`RB`) rota, D-Pad
izquierda/derecha cicla pieza, Face Bottom (`A`) confirma.

**Textos:**

| ES | EN |
|---|---|
| {Nombre de pieza} | {Piece name} |
| Rotar | Rotate |
| Colocar | Place |
| Salir | Exit |
| Necesita: {pieza} | Needs: {piece} |
| Sin apoyo | No support |

**Sonido:** cambiar de pieza, «clic» de página de catálogo; rotar, chasquido de
madera corta; colocar con éxito, el sonido de encaje del material (madera, bambú,
piedra — ya diferenciado por material en `Building`); colocar sin apoyo válido, un
golpe seco sin efecto (el objeto no se coloca).

---

### 2.8 Mapa y cartografía — **[AA]**

**Propósito:** el mapa físico en las manos, hoja de superficie y —nuevo, GDD §3.2—
hoja subterránea por sistema de galerías. Referencia:
`Source/Explored/UI/Widgets/SExploredMapInHands.h`.

```
┌──────────────────────────────────────────────────────────────┐
│   ╭─────────────────────────────────────────────────╮  Sellos: │
│   │                                                     │  💧 🕳️ │
│   │        (hoja dibujada a mano, costa trazada          │  ⚠ ◆  │
│   │         por donde ha caminado el jugador)             │  🏺 ⚓ │
│   │                                                     │        │
│   │                                                     │ Rumbos │
│   ╰─────────────────────────────────────────────────╯ Colección│
│         ◐ Superficie   Galería del Humo ▸               │
└──────────────────────────────────────────────────────────────┘
```

**Elementos:** hoja interactiva (arrastrar, rueda, flechas/cruceta, `LB`/`RB` para
pasar de hoja); selector de hoja en la parte inferior — «Superficie» y una pestaña por
cada sistema de galerías ya visitado (se crea la primera vez que el jugador entra,
`gdd_v2.md` §3.2: «se dibuja igual que la costa, a mano»; nunca minimapa de mina);
columna de sellos disponibles (agua dulce, cueva, peligro, recurso, ruina, resto del
Albatros — `story_es.json → map_marks`) más «Nota» para texto libre corto; enlace
«Rumbos» (lista de técnicas de wayfinding aprendidas) y «Colección» (abre el museo,
§2.9, desde el propio mapa).

**Estados/transiciones:** se abre con `M`/View; el mundo **sigue corriendo** (tiempo,
marea, lluvia — el papel se puede mojar mientras se lee), el personaje se queda
quieto con el mapa en las manos. Abierto desde la pausa, el juego sigue pausado. Se
cierra con `M` (salvo mientras se escribe una nota), Escape, `B` o Start.

**Entrada — teclado/ratón:** arrastrar con clic para desplazar, rueda para zoom,
clic en un sello y luego en la hoja para colocar una marca, clic en «Marcar donde
estoy». **Entrada — mando:** stick para desplazar, gatillos o `LB`/`RB` para zoom/
cambiar de hoja, D-Pad para elegir sello, Face Bottom confirma la colocación.

**Textos:**

| ES | EN |
|---|---|
| Superficie | Surface |
| Galería de {isla} | {island} tunnels |
| Rumbos | Bearings |
| Colección | Collection |
| Marcar donde estoy | Mark my spot |
| Nota | Note |
| Agua dulce | Fresh water |
| Cueva | Cave |
| Peligro | Danger |
| Recurso | Resource |
| Ruina | Ruin |
| Resto del Albatros | Albatros wreckage |

**Sonido:** papel desplegándose al abrir, pluma sobre papel al escribir una nota o
sellar una marca, goteo/chapoteo ambiental si está lloviendo mientras se lee (coherente
con «el papel se puede mojar mientras se lee: la tinta corre a la vista»,
`SExploredMapInHands.h:24`).

---

### 2.9 Diario, bitácora o museo — **[AA]**

**Propósito:** un único sistema, no tres. Referencia:
`Source/Explored/UI/Widgets/SExploredMuseum.h` + `ExploredScreens::BuildMuseumEntries`
(`Source/Explored/UI/ScreensLogic.h:20-64`).

**[LD] Decisión (una línea de justificación, como pide el encargo):** «diario»,
«bitácora» y «museo» son la misma necesidad de diseño — llevar la cuenta de lo
descubierto — repartida ya en dos sitios que no compiten entre sí: el **Museo**
(catálogo de tesoros con silueta/procedencia, `SExploredMuseum`) y el apartado
**«Rumbos»** dentro del propio Mapa (§2.8, técnicas de wayfinding aprendidas). Crear
una tercera pantalla de «diario» duplicaría datos que ya viven en `RuinsModel` y en
`CartographyComponent`, y el pilar 5 del GDD («ni diálogos ni texto largo») hace que
no haya prosa narrativa que archivar aparte: la historia se lee en el propio objeto
expuesto, no en un cuaderno de notas de texto.

```
┌──────────────────────────────────────────────────────────────┐
│  ← Volver                     Catálogo: 14/40   Coleccionista: 6/12 │
│  ──────────────────────────────────────────────────────────    │
│   ▮▮▮  ???                    ▮▮▮  Anzuelo de nácar             │
│        (silueta)                   Marae · La Meseta             │
│                                                                  │
│   ▮▮▮  Petroglifo, tortuga     ▮▮▮  ???                          │
│        Fotografiado · Ruina       (silueta)                     │
└──────────────────────────────────────────────────────────────┘
```

**Elementos:** cabecera con dos progresos (catálogo registrado/total, tesoros
expuestos frente al objetivo del logro «Coleccionista» —
`ExploredScreens::FMuseumSummary`, `ScreensLogic.h:52-65`); rejilla de fichas, cada una
con silueta o nombre según `EMuseumEntryState` (Desconocido/Fotografiado/Hallado/
Expuesto), procedencia siempre visible aunque no esté identificado
(`gdd_v2.md` §3.11), y lugar/isla si ya se recogió.

**Estados/transiciones:** se abre desde la Pausa («Museo») o desde «Colección» en el
Mapa; «Volver», Escape o `B` regresan a donde se abrió
(`ExploredSettingsLogic::FMenuNavigation`, citado en `SExploredMuseum.h:20`). Las
fichas son botones sin acción propia: solo mueven el foco para desplazar la lista.

**Entrada — teclado/ratón:** flechas o rueda para desplazar, clic no hace nada salvo
mover el foco. **Entrada — mando:** cruceta o stick para desplazar, `B` vuelve.

**Textos:**

| ES | EN |
|---|---|
| Catálogo: {n}/{total} | Catalogue: {n}/{total} |
| Coleccionista: {n}/{objetivo} | Collector: {n}/{goal} |
| ??? | ??? |
| Fotografiado | Photographed |
| Hallado | Found |
| Expuesto | Exhibited |

**Sonido:** pasar página al desplazar (sutil, no en cada fila); seleccionar una ficha
identificada, un tintineo breve de vitrina.

---

### 2.10 Logros — **[AA]**

**Propósito:** lista de logros con progreso, más el aviso emergente al conseguirlos.
Referencia: `SExploredAchievements` + `SExploredAchievementToast` +
`ExploredScreens::BuildAchievementRows` (`ScreensLogic.h:69-92`).

```
┌──────────────────────────────────────────────────────────────┐
│  ← Volver                                    18/42 conseguidos │
│  ──────────────────────────────────────────────────────────    │
│  ✓ Primera hoguera               Enciende tu primer fuego       │
│  ✓ Coleccionista                 ▓▓▓▓▓▓▓░░░ 6/12 expuestos       │
│  ? ??????????                    (oculto)                        │
│  ○ Náufrago a náufrago           Repara el faro                  │
└──────────────────────────────────────────────────────────────┘
```

**Elementos:** cabecera con recuento (`FAchievementsSummary`, `ScreensLogic.h:87-92`);
filas con marca de conseguido/no conseguido, nombre, descripción y barra de progreso
solo si el logro la tiene (`FAchievementRow::Progress`); los ocultos
(`bMasked`) muestran «??????????» sin descripción hasta desbloquearse.

**Estados/transiciones:** se abre desde la Pausa; «Volver»/Escape/`B` cierran. El
aviso emergente (`SExploredAchievementToast`) es independiente: aparece en la esquina
superior derecha en cualquier momento del juego, en cola si llegan varios a la vez,
sin abrir esta pantalla.

**Entrada:** igual que el Museo (§2.9), lista de solo desplazamiento.

**Textos:** salen de `achievements.json` (`nameEs`/`nameEn` ya en el propio dato, no
se listan aquí); textos de UI fijos:

| ES | EN |
|---|---|
| {n}/{total} conseguidos | {n}/{total} unlocked |
| ?????????? | ?????????? |

**Sonido:** el aviso emergente estampa (sello, §1.4); la lista, mismo pasar-página que
el museo.

---

### 2.11 Trueque con los navegantes — **[F3]**

**Propósito:** resolver el intercambio con el pueblo del arrecife (`gdd_v2.md` §3.9).
**No existe código hoy** (`Villages` es un módulo nuevo de fase 3); esta es la
especificación completa a implementar cuando llegue esa fase, no una pantalla
retocada de otra ya construida.

**[LD] Decisión:** «el trueque se resuelve en un objeto en cada mano igual que el
resto de interacciones del juego (máximo tres verbos)» (`gdd_v2.md` §3.9) — así que
**no es un menú de tienda** con dos columnas y precios. Es una extensión del prompt de
contexto normal: te acercas a un navegante con algo en la mano, el juego te dice si le
interesa y qué te da a cambio.

```
Acercarse a un navegante con un objeto en la mano (extensión del prompt de contexto):
┌──────────────────────────────────────────────────────────────┐
│                                                                  │
│         [E] Ofrecer Machete de acero → Cordaje trenzado ×4       │
│                    (o, si no le interesa:)                        │
│              «No le hace falta.» / “Doesn't need it.”             │
│                                                                  │
└──────────────────────────────────────────────────────────────┘

Trueque confirmado (notificación, igual que recoger un objeto):
┌──────────────────────────────────────────────────────────────┐
│                 Trocado: Machete de acero → Cordaje trenzado     │
└──────────────────────────────────────────────────────────────┘
```

**Elementos:** el propio navegante como interactuable con verbo «Ofrecer {objeto de
la mano}» cuando lo que llevas le interesa (según su necesidad real, visible por lo
que hace o pide — nunca una tabla de precios, `gdd_v2.md` §5); si no le interesa nada
de lo que llevas, ningún verbo de trueque aparece (no hay «no gracias» explícito: el
verbo simplemente no existe, igual que un interactuable sin acciones válidas hoy);
indicador de reputación **fuera** de esta pantalla — no hay barra de reputación en
pantalla, se lee en el trato del propio navegante (gestos, mejores ofertas) y en el
detalle de página del Mapa cuando se visita su territorio.

**Estados/transiciones:** aparece/desaparece con el foco de interacción, igual que
cualquier otro objeto (§2.4); confirmar el trueque (misma tecla `E`) consume el objeto
de la mano y entrega el resultado, con la misma notificación de recogida ya existente
(`HandleItemPickedUp`, reutilizada, no un sistema nuevo de notificación).

**Entrada:** idéntica al prompt de contexto general (§2.4): `E`/Face Left para
ofrecer.

**Textos:**

| ES | EN |
|---|---|
| Ofrecer {objeto} → {a cambio} | Offer {item} → {in exchange} |
| No le hace falta. | Doesn't need it. |
| Trocado: {objeto} → {a cambio} | Traded: {item} → {in exchange} |

**Sonido:** el propio navegante emite un gesto/sonido no verbal de aprobación (tono
cálido, sin voz ni texto largo — pilar 5) al aceptar; un sonido neutro y breve si no
le interesa.

---

### 2.12 Durmiendo — **[AA]**

**Propósito:** dormir para recuperar Sueño (`survival_needs.json → sleepRecoveryHours:
6`) y, si se decide en balance, pasar la noche rápido. **No existe widget propio hoy**
— se apoya en `SExploredFade` (ya implementado) en vez de crear un sistema de
transición nuevo.

```
┌──────────────────────────────────────────────────────────────┐
│                                                                  │
│                                                                  │
│                        (negro, fundido)                          │
│                                                                  │
│                       Durmiendo…  06:00 → 12:00                  │
│                                                                  │
│                                                                  │
└──────────────────────────────────────────────────────────────┘
```

**Elementos:** fundido a negro con `SExploredFade::FadeToBlack`; una línea con la hora
de inicio y la hora prevista de despertar (texto, no barra — coherente con «sin
números de necesidad», pero la hora del reloj ya es una excepción admitida, §0.6).
Si algo interrumpe el sueño (ataque, tormenta severa, hambre/sed llegando a 0), el
fundido se corta antes de la hora prevista con la notificación correspondiente en
vez de completar el ciclo.

**Estados/transiciones:** se entra interactuando con una cama/refugio construido
(verbo «Dormir» del propio mueble, mismo sistema de interactuables de siempre — no
hace falta un modo especial de personaje); durante el fundido el tiempo del mundo
avanza rápido (no en tiempo real); al terminar, `FadeFromBlack` y vuelta al HUD con
Sueño recuperado. Un segundo toque de la misma tecla, o `Escape`/`B` durante el primer
segundo del fundido, cancela y despierta de inmediato sin recuperar el ciclo completo
(se recupera proporcionalmente a lo dormido, no todo o nada).

**Entrada:** la misma tecla de interactuar (`E`/Face Left) para acostarse; Escape/`B`
para despertar antes de tiempo.

**Textos:**

| ES | EN |
|---|---|
| Durmiendo… {hora} → {hora} | Sleeping… {time} → {time} |
| Te despiertas antes de tiempo. | You wake up early. |

**Sonido:** grillos/oleaje nocturno que se atenúan al fundir a negro, un latido lento
de fondo casi imperceptible durante el sueño, amanecer con canto de ave al fundir de
vuelta.

---

### 2.13 Muerte — **[AA]**

**Propósito:** cerrar la vida del personaje con el peso justo, sin castigar con una
pantalla de estadísticas fría. **No existe widget propio hoy.**

```
┌──────────────────────────────────────────────────────────────┐
│                                                                  │
│                                                                  │
│                                                                  │
│                       (negro, fundido lento)                     │
│                                                                  │
│              Te quedaste sin fuerzas en Isla del Humo.            │
│                                                                  │
│                       Volver a la hoguera                        │
│                       Cargar partida                              │
│                       Salir al título                             │
│                                                                  │
└──────────────────────────────────────────────────────────────┘
```

**Elementos:** fundido a negro más lento que el de carga normal (2,5 s en vez de 0,8
s: dar el peso del momento sin alargarlo innecesariamente); una sola línea de causa y
lugar (isla actual), sin lista de causas de muerte tipo roguelike, sin estadísticas de
partida en esta pantalla (esas ya viven en Logros, §2.10, si aplica); tres opciones.
En modo **Náufrago** (`gdd_v2.md` §3.1, sin reaparición) desaparece «Volver a la
hoguera» — solo quedan Cargar partida y Salir al título, coherente con «la partida
termina si mueres» (§2.2).

**Estados/transiciones:** se dispara cuando Salud llega a 0
(`survival_needs.json → needs[].atZero` para «salud»: «Muerte»); «Volver a la hoguera»
reaparece en la última hoguera/cama usada con las necesidades básicas parcialmente
restauradas (según balance, no especificado aquí) y una pequeña penalización de
objetos sueltos en el lugar de la muerte (mismo patrón de saqueo ya descrito para
asaltos piratas, `gdd_v2.md` §3.8: lo que no esté en un contenedor cerrado queda
recuperable in situ, nunca se pierde para siempre salvo que otra cosa lo reclame
antes).

**Entrada — teclado/ratón:** flechas + Intro o clic. **Entrada — mando:** cruceta +
`A`. Sin Escape/`B` para cancelar: esta pantalla no tiene «volver», hay que elegir
una salida.

**Textos:**

| ES | EN |
|---|---|
| Te quedaste sin fuerzas en {isla}. | You ran out of strength on {island}. |
| Volver a la hoguera | Return to the fire |
| Cargar partida | Load a save |
| Salir al título | Quit to title |

**Sonido:** silencio brusco (se corta el ambiente del mundo de golpe, no con fundido)
al morir, luego el mismo oleaje tenue del título mientras se elige; sin música
dramática — coherente con «sin cinemáticas» del pilar 5.

---

### 2.14 Pausa — **[AA]**

**Propósito:** punto central de navegación mientras se juega. Referencia:
`Source/Explored/UI/Widgets/SExploredPauseMenu.h`.

```
┌──────────────────────────────────────────────────────────────┐
│                                                                  │
│                          Pausa                                    │
│                                                                  │
│                        Continuar                                  │
│                        Guardar                                    │
│                        Museo                                      │
│                        Logros                                     │
│                        Ajustes                                    │
│                        Salir al título                             │
│                                                                  │
└──────────────────────────────────────────────────────────────┘
```

**Elementos:** lista vertical de 6 entradas sobre el mundo congelado y oscurecido
(no negro puro: se sigue viendo el paisaje, atenuado). «Guardar» abre el selector de
ranuras en modo Guardar (§ ranuras, `ESaveSlotsMode::Save`, solo las 3 manuales).

**Estados/transiciones:** `Escape`/Start abre y cierra; «Continuar» o Escape de nuevo
vuelven al juego; «Museo»/«Logros»/«Ajustes» abren esas pantallas **sobre** la pausa
(al volver de ellas, se vuelve a la pausa, no directo al juego —
`ExploredSettingsLogic::FMenuNavigation`); «Salir al título» pide confirmación si hay
progreso sin guardar.

**Entrada — teclado/ratón:** flechas + Intro o clic; `Escape` abre/cierra. **Entrada —
mando:** cruceta + `A`; Start abre/cierra, `B` cierra.

**Textos:**

| ES | EN |
|---|---|
| Pausa | Paused |
| Continuar | Resume |
| Guardar | Save |
| Museo | Museum |
| Logros | Achievements |
| Ajustes | Settings |
| Salir al título | Quit to title |

**Sonido:** el mundo se amortigua (filtro de graves) en vez de silenciarse del todo;
navegación, mismo paso de página del título.

---

### 2.15 Ajustes — **[AA]**

**Propósito:** gráficos, audio, controles (con remapeo), accesibilidad e idioma.
Referencia: `Source/Explored/UI/Widgets/SExploredSettingsPanel.h` (pestañas
`ETab::{Graphics, Audio, Controls, Game, Accessibility}`) y
`Source/Explored/UI/ExploredGameUserSettings.h`.

```
┌──────────────────────────────────────────────────────────────┐
│  ← Volver     [Gráficos] [Audio] [Controles] [Juego] [Accesib.] │
│  ──────────────────────────────────────────────────────────    │
│                                                                  │
│               (contenido de la pestaña activa)                   │
│                                                                  │
│                                                                  │
│                                                                  │
│                        Restablecer      Aplicar                  │
└──────────────────────────────────────────────────────────────┘
```

**Pestaña Gráficos:** resolución, modo de ventana, VSync, límite de FPS, calidad
general y por categoría, escala de resolución (resueltas por `UGameUserSettings`
base, no se repiten aquí los controles estándar) + **Brillo** (`Brightness`, slider
0–4, con una imagen de calibración con dos siluetas apenas visibles a distinto
contraste — patrón habitual y ya validado en el género).

**Pestaña Audio:** cinco deslizadores independientes (`EExploredAudioChannel`:
`Master`, `Music`, `Effects`, `Ambient`, `Interface`), 0–100, con lectura en palabras
(Silencio/Bajo/Medio/Alto/Máximo) al lado del número — el número solo aparece aquí
porque el volumen es una excepción ya asumida del propio menú de sistema, no del
juego (§0.6 sigue rigiendo el HUD, no `Ajustes`).

**Pestaña Controles:** FOV (slider), sensibilidad de ratón y de mando (sliders
independientes), invertir eje Y (casilla), balanceo de cámara (casilla), mantener para
agacharse frente a alternar (casilla); lista de acciones remapeables
(`ExploredSettingsLogic::GetRemappableActions`) con `SExploredKeyCaptureButton` por
fila — pulsar el botón, pulsar la tecla nueva, o `Escape`/`B` para cancelar sin
cambiar nada.

```
   Interactuar          [ E ]        [Cambiar]
   Usar mano derecha    [Clic izq.]  [Cambiar]
   Combinar             [ C ]        [Cambiar]
   Mochila              [Tab]        [Cambiar]
   Soltar               [ G ]        [Cambiar]
   Saltar               [Espacio]    [Cambiar]
   Correr                [Mayús]      [Cambiar]
   Agacharse/Bucear      [Ctrl]       [Cambiar]
```

**Pestaña Juego:** duración del día en minutos (`DayLengthMinutes`), subtítulos
(casilla), tamaño de texto (`EExploredTextSize`: Pequeño/Mediano/Grande), idioma
(`EExploredLanguage`: Español/English — cambiarlo previsualiza los textos al instante
con `ApplyPreviewSettings`, sin reiniciar el juego).

**Pestaña Accesibilidad:** modo de daltonismo (`EExploredColorblindMode`: Ninguno /
Protanopia / Deuteranopia / Tritanopia), reducir movimiento (casilla,
`bReduceMotion`), desactivar parpadeos (casilla, `bDisableFlashing`), pistas visuales
de sonido (casilla, `bSoundVisualCues` — ver §3). Detalle completo de qué toca cada
opción en §3.

**Estados/transiciones:** cada pestaña se reconstruye al cambiar (`SelectTab`,
`SettingsPanel.h:62`); los cambios se previsualizan en vivo donde es seguro (brillo,
idioma, daltonismo) y se confirman con «Aplicar» o al salir de la pantalla;
«Restablecer» vuelve a los valores por defecto de la pestaña activa, con confirmación.
Se abre igual desde el Título como desde la Pausa; «Volver» regresa a quien la abrió.

**Entrada — teclado/ratón:** `Tab`/flechas laterales para cambiar de pestaña, flechas
verticales + Intro/clic para los controles de la pestaña. **Entrada — mando:**
`LB`/`RB` cambian de pestaña, cruceta navega, `A` confirma/activa, `B` vuelve.

**Textos:**

| ES | EN |
|---|---|
| Gráficos | Graphics |
| Audio | Audio |
| Controles | Controls |
| Juego | Game |
| Accesibilidad | Accessibility |
| Restablecer | Reset |
| Aplicar | Apply |
| Silencio / Bajo / Medio / Alto / Máximo | Muted / Low / Medium / High / Max |
| Pequeño / Mediano / Grande | Small / Medium / Large |
| Ninguno | None |
| Reducir movimiento | Reduce motion |
| Desactivar parpadeos | Disable flashing |
| Pistas visuales de sonido | Visual sound cues |
| Cambiar | Rebind |
| Pulsa una tecla… | Press a key… |

**Sonido:** clic seco al cambiar de pestaña; los deslizadores de Audio reproducen un
pellizco corto del canal que se está ajustando (así se oye el volumen que se está
tocando, sin tener que salir a probarlo).

---

### 2.16 Créditos — **[AA]**

**Propósito:** reconocimiento del equipo, herramientas CC0 usadas (Kenney, KayKit,
Quaternius, Poly Haven — `gdd_v2.md` §7.1) y la dedicatoria completa. Referencia:
`Source/Explored/UI/Widgets/SExploredCredits.h`.

```
┌──────────────────────────────────────────────────────────────┐
│                                                                  │
│                         Explored                                  │
│                                                                  │
│              (lista desplazable de nombres y agradecimientos)     │
│                                                                  │
│                 「Dedicado a Almudena, mi Limón.」                │
│                                                                  │
│                        ← Volver                                   │
└──────────────────────────────────────────────────────────────┘
```

**Elementos:** desplazamiento automático lento sobre el mundo (no un fondo negro
aparte); dedicatoria al final, con el mismo trazo `Caveat` de la pantalla de título;
bloque de agradecimiento a los paquetes CC0 usados, con crédito nominal donde la
licencia lo pida (KayKit, Quaternius, Poly Haven, Kenney).

**Estados/transiciones:** se abre desde el Título; `Escape`/`B`/«Volver» cierran en
cualquier momento del desplazamiento, no hace falta esperar a que termine.

**Entrada:** flechas/rueda o stick para acelerar el desplazamiento manualmente;
`Escape`/`B` para volver.

**Textos:** nombres propios y agradecimientos, fuera del alcance ES/EN de esta
sección (no son diálogo de juego, son créditos reales).

**Sonido:** el mismo ambiente de oleaje del título, sin música nueva.

---

## 3. Accesibilidad

Todas las opciones viven en `Ajustes > Accesibilidad` (§2.15) salvo donde se indica.

### 3.1 Subtítulos

- `bSubtitlesEnabled` ya existe en código. Cubre **todo** sonido con intención
  narrativa, no solo diálogo — el juego no tiene diálogo hablado (pilar 5), así que
  los subtítulos son de **eventos sonoros entre corchetes**: `[cuerno pirata a lo
  lejos]` / *[pirate horn in the distance]*, `[la vela cruje]` / *[the sail creaks]*,
  `[gruñido de cerdo salvaje cerca]` / *[wild boar grunting nearby]*. Sin esto, un
  jugador sordo pierde el único canal narrativo no verbal del juego — es el ajuste de
  accesibilidad más importante de todo el documento, no uno más de la lista.
- Aparecen en la zona de notificaciones del HUD (§1.5), mismo estilo tipográfico,
  entre corchetes para diferenciarlos del texto normal del juego.

### 3.2 Daltonismo

- `EExploredColorblindMode` (Protanopia/Deuteranopia/Tritanopia) ya aplica un filtro
  de postproceso (`ApplyColorblindMode`, `ExploredGameUserSettings.h:207`).
- Regla de diseño que ya se cumple y se deja explícita: **ningún elemento del juego
  depende solo del color.** Las barras de necesidad siempre llevan su etiqueta de
  texto (`DrawText(Bar.Label...)`, `ExploredHUD.cpp:265`); el fantasma de construcción
  inválido añade trama de rayas además del rojo (§2.7); el estado del barco es texto,
  no un semáforo de color. Cualquier elemento nuevo de UI sigue esta misma regla antes
  de entrar en el juego.
- Único punto pendiente identificado en este documento: la barra de tensión de pesca
  (`DrawFishing`, `ExploredHUD.cpp:138-141`) hoy es un degradado verde→rojo sin texto
  de apoyo — ver TODO.

### 3.3 Tamaño de texto

- `EExploredTextSize` (Pequeño/Mediano/Grande) escala el tamaño de fuente de **toda**
  la UI de Slate mediante un multiplicador aplicado en el punto común de estilo (no
  pantalla por pantalla) — Pequeño ×0.85, Mediano ×1.0, Grande ×1.25. No afecta al
  texto pintado a mano en `Doodle` (son trazos, no texto) ni al tamaño de los sellos
  del mapa (son ilustración, no lectura).

### 3.4 Opciones de mareo (motion sickness)

- `bReduceMotion`: desactiva el balanceo de cámara (`bCameraBobEnabled`, ya existe
  como opción independiente en Controles — Reducir movimiento lo fuerza a apagado
  además de dar al jugador el control fino en Controles), el rebote de escala del
  aviso de logro (§1.4), el pulso de opacidad del fantasma de construcción (§1.4) y la
  facilidad («ease») de las transiciones de menú (quedan en fundido puro, sin
  desplazamiento).
- `bDisableFlashing`: apaga el flash de cámara del objeto «Cámara desechable», el
  parpadeo de rayo durante tormentas y cualquier destello súbito de pantalla completa
  (heridas graves, impacto). Se sustituyen por una viñeta o fundido sin parpadeo.
- Deslizador de FOV (Controles, §2.15) ya cubre el otro eje clásico de mareo (campo de
  visión estrecho = más mareo en movimiento rápido/barco); se documenta aquí como
  parte del mismo conjunto de mitigación aunque viva en otra pestaña.

---

## TODO de implementación

- [ ] `AExploredCharacter::HandleCombine` (`Source/Explored/Player/
      ExploredCharacter.cpp:758-790`): cuando `Verbs.Num() > 1`, no aplicar
      `Verbs[0]` de inmediato — abrir la lista de §2.6 y esperar confirmación o
      expiración (4 s) antes de llamar a `ApplyInWorld`.
- [ ] Crear el widget `SExploredCraftChoice` (o similar) para la lista de §2.6: hasta
      3 filas con tecla/botón + nombre de verbo (`verbs.json`), sin vista previa del
      resultado, con expiración automática.
- [ ] `BuildPreviewComponent::SetupInput` (`Source/Explored/Building/
      BuildPreviewComponent.cpp:48-72`): añadir mapeo de mando — hoy `IA_BuildToggle`,
      `IA_BuildRotate`, `IA_BuildConfirm`, `IA_BuildNext`, `IA_BuildPrevious` solo
      tienen tecla/ratón. Propuesta de §2.7: `LB` entra/sale, `RB` rota, D-Pad
      izquierda/derecha cicla pieza, Face Bottom confirma.
- [ ] Añadir trama de rayas diagonales (además del color rojo) al material del
      fantasma de construcción cuando el apoyo es insuficiente
      (`BuildPreviewComponent`, material de vista previa).
- [ ] Añadir mapeo de mando para `IA_Fish` (hoy solo `F` de teclado,
      `ExploredCharacter.cpp:506-509` no tiene entrada de mando para lanzar/recoger,
      solo para el eje de sedal).
- [ ] Diseñar y cablear la fila de «Estado del barco» del HUD (§2.4): nueva función
      `AExploredHUD::DrawBoatStatus`, llamada desde `DrawHUD` solo si
      `AExploredCharacter` está a bordo (`AExploredBoat::IsOccupied`); condiciones de
      aparición según la tabla de §2.4.
- [ ] Sustituir la etiqueta de mano por el dial de brújula (§2.4) cuando
      `CarryComp->GetHandItemPtr(Hand)` es el objeto «Brújula»: nueva función
      `AExploredHUD::DrawCompassDial` invocada desde `DrawHandLabels` en vez de
      `DrawText` para esa mano.
- [ ] Añadir texto de apoyo a la barra de tensión de pesca (`DrawFishing`,
      `ExploredHUD.cpp:138-141`): una palabra de urgencia («A punto de romperse» /
      *«About to snap»*) cuando `Tension01 > 0.8`, para no depender solo del
      degradado verde→rojo (accesibilidad, §3.2).
- [ ] Implementar la pantalla de Muerte (§2.13): nuevo widget `SExploredDeathScreen`,
      disparado quando `FSurvivalState::Health` llega a 0 (`Source/Explored/Survival/
      SurvivalModel.h`); tres opciones (dos en modo Náufrago).
- [ ] Implementar la transición de Dormir (§2.12): interactuable «Dormir» en las
      piezas de cama/refugio (`building_pieces.json`), uso de `SExploredFade` con la
      etiqueta de hora, avance acelerado del reloj del mundo y cancelación con
      recuperación proporcional.
- [ ] Implementar el módulo `Villages` y el verbo de trueque de §2.11 — depende de
      `gdd_v2.md` §3.9 y queda fuera de alcance hasta fase 3; esta especificación
      queda lista para cuando entre en producción.
- [ ] Subtítulos de eventos sonoros (§3.1): catálogo de eventos con clave de
      subtítulo por sonido narrativo (cuerno pirata, vela, fauna cercana, aviso de
      asalto — `gdd_v2.md` §3.8), enrutado a través de `bSubtitlesEnabled` y la cola
      de notificaciones del HUD.
- [ ] Aplicar el multiplicador de `EExploredTextSize` de forma centralizada a los
      estilos de Slate (§3.3) en vez de por widget, para que cualquier pantalla nueva
      lo herede automáticamente.
- [ ] `bReduceMotion` (§3.4): cablear a `bCameraBobEnabled` (forzar apagado), al
      rebote de `SExploredAchievementToast`, al pulso de opacidad del fantasma de
      construcción y a la easing de apertura de menús (§1.4) — hoy `bReduceMotion`
      existe en `ExploredGameUserSettings` pero no está consumido por ninguno de
      estos cuatro sistemas.
- [ ] `bDisableFlashing` (§3.4): cablear al flash de la cámara desechable, al
      parpadeo de rayo de `Weather` y a cualquier destello de pantalla completa por
      daño — mismo estado: el ajuste existe, falta el consumidor.
- [ ] Confirmar si «Mochila» (`Tab`/D-Pad derecha) es alternable o de mantener
      pulsado (§2.5) — hoy `bBackpackOpen` en `ExploredCharacter.cpp:792-797` es un
      alternador simple; este documento asume que se mantiene así, pero conviene
      confirmarlo en playtesting antes de dar la pantalla por cerrada.
