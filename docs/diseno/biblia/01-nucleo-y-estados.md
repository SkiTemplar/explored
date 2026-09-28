# EXPLORED — Biblia de diseño · 01. Núcleo y estados

Versión 1 · 2026-09-27 · Sección de la biblia de diseño definitiva.

Manda `docs/diseno/gdd_v2.md`. Complementa `docs/design/biblia-de-contenido.md` (catálogo
de objetos y verbos) y los datos reales de `Content/Data/*.json` (citados por id). Refleja
el código existente en `Source/Explored/Survival/` (`SurvivalModel.{h,cpp}`,
`BodyModel.{h,cpp}`, `BodySignals.{h,cpp}`) y `Source/Explored/Player/SwimModel.{h,cpp}`
para todo lo ya implementado; marca explícitamente lo que es diseño nuevo aún sin código.

Fases: **[AA]** acceso anticipado, **[F2]**, **[F3]** (`gdd_v2.md` §6). Cuando una decisión
de este documento no estaba cerrada en las fuentes, se decide aquí y se justifica en una
línea, marcada **[Decisión]**.

---

## 1. Pilares y fantasía del jugador

La fantasía: **eres un náufrago sin nada, y la isla se convierte en algo tuyo.** No hay
clase, no hay build, no hay nivel de personaje — solo lo que aprendes a hacer con las
manos y lo que dejas grabado en el terreno. El juego no cuenta una épica; cuenta lo que
hace un superviviente real: buscar agua, no perder el filo del cuchillo, acordarse de
dónde estaba esa cueva.

Los cinco pilares (`gdd_v2.md` §2.1), en la fantasía del jugador:

1. **Sobrevivir con el cuerpo** — el cuerpo es la interfaz. No hay barras que leer;
   hay temblor, respiración y frío que se sienten en la cámara y en las manos (§6).
2. **Transformar la isla** — cada golpe de pico, cada árbol talado, cada pieza
   construida se queda ahí. La isla que dejas al salir no es la isla en la que
   aterrizaste.
3. **Cartografiar a mano** — nada de minimapa. Lo que no has dibujado tú, no lo sabes.
4. **Libertad total** — sin misión principal, sin orden obligatorio entre islas,
   sistemas o superficie/subsuelo.
5. **Historia mínima y ambiental** — un pueblo vivo tratado con respeto (nunca un
   enemigo) y un conflicto real (los piratas) que dan textura sin una sola cinemática.

---

## 2. Bucle de juego por escala de tiempo

| Escala | Bucle | Ejemplo concreto | Fase |
|---|---|---|---|
| **~30 s** | Ver algo → acercarse → coger, talar o picar | Un coco al alcance, una veta de cobre a la vista | [AA] |
| **~5 min** | Necesidad → plan → fabricar/cavar/cazar → resolver | «Necesito agua» → tallar un cuenco → llenarlo en el río → hervir | [AA] |
| **~1 día** (40 min reales, §5) | Amanecer → trabajo en superficie o en la mina → volver antes de la noche → fuego → dormir | Turno de mina con antorchas de sobra para volver antes de que oscurezca | [AA] |
| **~1 semana** | Proyecto grande | Abrir una galería con apuntalamiento, amurallar la base [F2], criar el primer estanque | [AA]/[F2] |
| **~1 estación** (8 días, §5) | Adaptarse | Apuntalar y llenar cisternas antes del monzón; aprovechar la seca para cavar y explorar | [AA] |
| **Partida completa** | Explorar, coleccionar, transformar | Mapa de las siete islas completo, museo lleno, barco «Limón» botado | [AA]→[F3] |

---

## 3. Progresión completa: de manos vacías al final del acceso anticipado

La curva de dificultad (`biblia-de-contenido.md` §5.1) fija **qué aprieta**; el nivel de
herramienta (`gdd_v2.md` §4) fija **qué se puede hacer**. Se combinan aquí en una sola
línea de tiempo. «Día» es un día de juego de 40 minutos reales (§5).

| Momento | Nivel de herramienta | Isla / lugar | Qué se puede hacer ya | Qué aprieta |
|---|---|---|---|---|
| **Día 1** | Nivel 0 (a mano) | Isla del Amaraje (Landing) | Coco, sombra, refugio inclinado, fuego con el mechero del Albatros | Sed, sol, la primera noche |
| **Días 2–4** | Nivel 1 (tosco): hacha de piedra, cuchillo de lasca, pala tosca | Landing | Pesca en la orilla, agua hervida, arco de fuego, primer agujero cavado a mano (§3.4 del GDD) | Hambre, agua limpia |
| **Días 5–10** | Nivel 1→2 (tallado) | Landing → Esmeralda | Cabaña, conservar comida, trampas, primera técnica de wayfinding, minería superficial (tierra/arcilla) | Herramientas, lluvias |
| **Semanas 2–3** | Nivel 2→3 (obsidiana); pico de piedra y tallado | Esmeralda → Isla del Humo → Los Dientes | Balsa, pesca de altura, medicina, cerámica, minería en basalto y vetas de cobre, cuevas y tubos de lava | Otras islas, peligros nuevos, primer ciclón |
| **Mes 2+** | Nivel 3 consolidado; Nivel 4 (rescatado/fundido) si hay banco de chatarra | Las 3–4 islas del acceso anticipado | El reto pasa a ser explorar, las capturas legendarias (§4.6 de la biblia de contenido) y completar el mapa | La supervivencia deja de ser protagonista (sigue presente, nunca desaparece) |
| **Cierre del acceso anticipado** | Nivel 4 | Landing, Esmeralda, Humo, Los Dientes | Mapa de superficie y hojas subterráneas de esas islas completas, huerto y limonero maduros, faro reparado como objetivo emergente | — |

**Fuera del acceso anticipado** (`gdd_v2.md` §6.2, sin cambios aquí): raíles y vagones,
animales domésticos, murallas y defensa, resto de islas (Manglar, Arenas Blancas, Meseta
completa) entran en **[F2]**; pueblo del arrecife, conflicto pirata completo, isla oculta
y el barco «Limón» zarpando entran en **[F3]**.

---

## 4. Inicio de partida

**[AA]** [Decisión] La partida arranca siempre en el mismo instante de juego: **día 4,
07:30**, coordenadas del reloj mundial por defecto (`UTimeOfDaySubsystem`: `Day = 4`,
`Hours = 7.5`) — a mitad de la estación seca (días 1–8, `biblia-de-contenido.md` §6.1), la
más amable, coherente con el criterio de salida de la porción vertical (`gdd_v2.md` §6.1).
No hay elección de hora de inicio: es parte de la identidad del amanecer del Albatros.

**Lugar:** la playa de la Isla del Amaraje (Landing), a pocos metros de los restos
varados del hidroavión.

**Contexto (ambiental, sin cinemática ni diálogo):** el jugador despierta en la arena,
empapado, con el fuselaje del Albatros humeando cerca. No hay tutorial de texto: el
personaje piensa en voz baja («tengo que salir del agua») igual que en cualquier otro
momento del juego (`biblia-de-contenido.md` §2.5).

**Estado inicial del cuerpo** (`Content/Data/survival_needs.json`, campo `initial`):

| Necesidad | Valor inicial | Nota |
|---|---|---|
| Salud (`salud`) | 100 | |
| Hambre (`hambre`) | 85 | |
| Sed (`sed`) | 70 | |
| Sueño (`sueno`) | 90 | |
| Proteína (`proteina`) | 50 | |
| Hidratos (`energia_dieta`) | 50 | |
| Vitamina C (`vitamina_c`) | 50 | |
| Ánimo (`animo`) | 60 | sube pronto: el primer fuego y el primer trazo de mapa dan +4 y +5 (§6.16) |
| Temperatura corporal | 37 °C | objetivo de equilibrio, ver §6.5 |
| Mojado | 1.0 (empapado) | [Decisión] el jugador llega nadando desde el amaraje forzoso; se seca en los primeros minutos junto al fuselaje o al sol (§6.6) — es el primer gesto de juego, no un castigo |

**Kit inicial** [Decisión: lista cerrada de lo que ya lleva encima o encuentra en el
fuselaje a menos de 20 m, para que la primera media hora no dependa del azar]. Todo tomado
del catálogo de rescatados del Albatros (`biblia-de-contenido.md` §3.3):

- Puesto (encima del cuerpo): mochila, reloj de muñeca, cinturón de seguridad cortado
  (fibra para atar), gafas de sol.
- En el fuselaje, a la vista, sin buscar: mechero (poco gas — dura las primeras 3–4
  fogatas), navaja multiusos rota (falta una pieza: no corta hasta repararla, empuja al
  jugador a tallar su primer cuchillo de lasca), botiquín (vendas, antiséptico), manual del
  avión (papel: yesca de sobra para el primer fuego), cantimplora vacía.
- **No** lleva: comida, agua, ninguna herramienta tosca ya hecha. La primera hacha, la
  primera pala y el primer cuenco los hace el jugador.

---

## 5. Ciclo de día y noche, y clima

### 5.1 Reloj

- **1 día de juego = 40 minutos reales** por defecto (`UTimeOfDaySubsystem::DayLengthMinutes
  = 40.0`), ajustable en Ajustes → Juego. [Decisión] rango de ajuste **20–90 minutos
  reales por día**: por debajo de 20 el ciclo se siente arritmia; por encima de 90 un
  jugador con poco tiempo libre no llega a ver una noche completa en una sesión.
- Latitud del archipiélago: **12° Sur** (trópico); noches y días de duración similar todo
  el año, sin estaciones extremas de luz — el clima cambia por lluvia y viento, no por
  horas de sol.
- **Año de juego: 32 días**, cuatro estaciones de 8 días cada una
  (`biblia-de-contenido.md` §6.1). La partida empieza en la estación seca (§4).
- **Ciclo lunar: 12 días** (`FMoonModel::DaysPerCycle`), con 8 fases nombradas
  (nueva, creciente, cuarto creciente, gibosa creciente, llena, gibosa menguante, cuarto
  menguante, menguante). La fase lunar mueve la amplitud de las mareas (§5.3) y es la base
  del wayfinding «camino de estrellas» (`biblia-de-contenido.md` §9.2).
- Dormir con `EActivity::Sleeping` avanza el reloj a metabolismo 0.5× (§6.4); no hay salto
  de tiempo instantáneo al dormir: el mundo (clima, mareas, fauna) sigue su curso a
  velocidad normal mientras el jugador duerme.

### 5.2 Estaciones

| Estación | Días | Clima dominante | Qué trae | Qué aprieta |
|---|---|---|---|---|
| **Seca** | 1–8 | Cielo limpio, calor, viento alisio | Mango, anacardo, tortugas desovando, pesca fácil, mareas vivas, noches estrelladas | Ríos bajos, sed, insolación, incendios |
| **Primeras lluvias** | 9–16 | Chubascos por la tarde, tormentas eléctricas | Setas, ranas, charcas, flores, abejas activas, fruta de la pasión | Mosquitos, rayos, caminos embarrados |
| **Monzón** | 17–24 | Lluvia casi diaria, niebla, ríos crecidos | Fruta del pan, taro, peces remontando ríos, cascadas, cuevas con agua | Frío por humedad (Mojado, §6.6), crecidas, comida que se pudre |
| **Ciclones** | 25–32 | Calma bochornosa entre tormentas, mar de fondo | Restos arrastrados, contenedores naufragados, ballenas | Ciclones, mar peligroso, navegación arriesgada |

(Tabla completa de temporales — tormenta eléctrica, crecida, deslizamiento, galerna,
niebla cerrada, ola de calor, ciclón cat. 1–3 — en `biblia-de-contenido.md` §6.2; sin
cambios. Cada temporal se anuncia, golpea y deja un después que invita a explorar.)

### 5.3 Mareas

Dos pleamares y dos bajamares al día; amplitud según fase lunar (mareas vivas en luna
llena y nueva, cada 6 días dentro del ciclo de 12). La bajamar abre pasos a pie, pozas de
marea y cuevas marinas; la pleamar los cierra (`biblia-de-contenido.md` §6.3, sin
cambios).

---

## 6. Estados del cuerpo

### 6.0 Cómo se lee el cuerpo

**Sin HUD de números ni iconos.** El jugador diagnostica por sensación: respiración,
temblor de cámara y de manos, viñeta de color en el borde de pantalla, desaturación,
visión borrosa, latido audible (`Source/Explored/Survival/BodySignals.h`,
`FBodySignals`). El **reloj de muñeca** del Albatros (`FWristWatchReadout`) es la única
lectura semi-numérica: si el jugador activa «mostrar necesidades» en ajustes, al levantar
la muñeca ve cuatro niveles gruesos (Bien / Regular / Bajo / Crítico) de sed, hambre,
sueño y temperatura — nunca una cifra.

Cada estado de esta sección tiene, cuando aplica, un **aviso interior** (ES/EN): una frase
corta que el personaje «piensa» al cruzar un umbral, en el tono seco y algo irónico de un
superviviente que ya lleva unos días en esto. Nunca un mensaje de sistema. Los textos
viven en `survival_needs.json#innerVoice` y `FInnerVoiceModel` decide cuándo suena cada
uno: los de umbral se dicen una vez al entrar y se rearman al salir con margen (5 puntos
en las necesidades, 0,3 °C en la temperatura); los de suceso (quemadura, jadeo) suenan
cada vez; como mucho uno por paso del cuerpo, el más urgente, y al cargar o reaparecer
no se recita lo que ya dolía. «Calor aprieta» salta por encima de 38,5 °C sin fiebre
[Decisión: la biblia no daba umbral]. En red lo evalúa el dueño sobre su réplica.

Todas las necesidades van de **0 a 100** salvo que se diga lo contrario. El tiempo se mide
en horas de juego. `Scale` es el multiplicador de modo (§8); `Metabolism` es el
multiplicador de actividad (§6.3).

### 6.1 Hambre

Fuente: `Content/Data/survival_needs.json#hambre`, `SurvivalModel.cpp` (`HungerHours`).

- **Rango:** 0–100. Inicial: **85**.
- **Velocidad:** se vacía de 100 a 0 en **36 horas** de juego caminando en modo
  Superviviente (`HungerHours = 36`). Fórmula: `drenaje/hora = 100/36 × Scale ×
  Metabolism(actividad)`. Ejemplo: caminando en Superviviente (`Scale=1.0`,
  `Metabolism=1.0`) → 2.78 pts/hora.
- **Umbral de aprieto (<40):** reduce la Energía máxima (§6.3) en `0.8 × (40 − Hambre)`
  puntos. Con Hambre en 20, la Energía máxima baja 16 puntos.
- **Umbral crítico (<25):** Ánimo −3/hora adicionales.
- **En 0:** Salud −2/hora, evento `Starving`.
- **Cómo se cura:** comer (cualquier ítem con propiedad Nutritivo/`Food`, catálogo
  `biblia-de-contenido.md` §3.6).
- **Aviso interior:**
  - Aprieta (<40) — ES: «El estómago empieza a quejarse.» / EN: «Stomach's putting in a
    complaint.»
  - Crítico (0) — ES: «Ya ni me acuerdo de la última comida.» / EN: «Can't remember my
    last meal.»

### 6.2 Sed

Fuente: `survival_needs.json#sed`, `SurvivalModel.cpp` (`ThirstHours`).

- **Rango:** 0–100. Inicial: **70**.
- **Velocidad base:** se vacía en **20 horas** (`ThirstHours = 20`). Fórmula: `drenaje/hora
  = 100/20 × Scale × Metabolism × (1 + Heat × 0.6)`, con `Heat = max(0, AirTemp−30)/6 +
  SunExposure × 0.5`. Ejemplo: a 36 °C a pleno sol (`Heat=1.5`), el drenaje sube de 5 a 9.5
  pts/hora (se vacía en ~10.5 h en vez de 20).
- **En 0:** visión borrosa (señal `Blur`), Salud −5/hora, evento `Dehydrated`.
- **Umbral crítico (<25):** Ánimo −3/hora adicionales.
- **Cómo se cura:** agua de coco (sin riesgo, limitada), lluvia recogida, agua hervida o
  filtrada (río/charca), destilador solar (agua de mar). Beber agua de mar sin tratar
  recupera 15 pts de golpe pero cuenta como fuente de intoxicación (§6.10, toxicidad 0.35
  sin hervir — ver `hazards.unboiledWater`/`seaWaterThirst`).
- **Aviso interior:**
  - Aprieta (<40) — ES: «Se me está secando la boca.» / EN: «Mouth's gone dry.»
  - Crítico (0) — ES: «Veo doble. Necesito agua, ya.» / EN: «Seeing double. Need water,
    now.»

### 6.3 Energía (aguante inmediato)

Distinta del Sueño (§6.4): es el aguante para correr, nadar fuerte o trepar, no la
necesidad de dormir. Fuente: `SurvivalModel.cpp` (`EnergyDrainPerSecond`,
`FSurvivalState::MaxEnergy`).

- **Rango:** 0 a un máximo variable (20–100, ver abajo). No mata por sí sola; limita el
  esfuerzo físico.
- **Velocidad por actividad** (puntos por segundo real, ya con el peso cargado si supera
  el 50 % de la carga cómoda):
  | Actividad | Δ Energía/s |
  |---|---|
  | Esprintar | −11 × peso |
  | Nadar | −5 × peso |
  | Trabajar (talar, picar, construir) | −3 |
  | Caminar / nado suave | +8 |
  | Descansar de pie | +14 |
  | Dormir | +20 |
- **Energía máxima** (`MaxEnergy()`, 20–100): baja con Hambre<40 (−0.8/pt faltante),
  Sueño<30 (−0.8/pt faltante), Salud<60 (−0.4/pt faltante), dieta media<25 (−0.6/pt
  faltante), −20 si hay Fiebre o Intoxicación activa, hasta −10 por dieta monótona (§6.18)
  y hasta −15 por escorbuto pleno (§6.9). La Energía actual nunca supera este máximo.
- **En el máximo mínimo (20):** ni esprintar ni nadar fuerte son sostenibles; solo queda
  caminar y trabajar despacio.
- **Cómo se cura:** caminar, descansar o dormir; y, de fondo, arreglar lo que le está
  robando techo (comer, dormir, curarse).
- **Aviso interior:** no tiene aviso propio; se siente en la respiración y en que el
  personaje deja de poder esprintar (Vignette + Breathing altos, §6.0).

### 6.4 Sueño

Fuente: `survival_needs.json#sueno`, `SurvivalModel.cpp` (`RestHours`,
`SleepRecoveryHours`), `FSurvivalState::SleepDeprivation`.

- **Rango:** 0–100. Inicial: **90**.
- **Velocidad:** se vacía en **30 horas** despierto (`RestHours = 30`, `Scale` aplica,
  `Metabolism` no). Durmiendo sube a **100/6 pts/hora** (`SleepRecoveryHours = 6`: una
  cama llena el sueño en 6 horas de juego = 15 minutos reales a ritmo por defecto), al 70 %
  de esa velocidad si no hay techo sobre la cabeza.
- **Falta de sueño** (`SleepDeprivation`, 0–1): `clamp((30 − Sueño)/30, 0, 1)` — solo
  cuenta por debajo de 30.
- **Umbral de aprieto (<30):** Eficiencia de trabajo −0.25 × falta de sueño; Energía
  máxima −0.8/pt faltante (§6.3); Torpeza (§6.19) sube hasta 0.7.
- **En 0:** evento `Exhausted`; alucinación leve posible en el último tercio de privación
  (ver §6.11 y §6.19: intensidad hasta 0.35, nunca tan fuerte como la de una seta).
- **Cómo se cura:** dormir en cualquier superficie de descanso (estera, cama, hamaca);
  una cama techada es más eficaz que dormir al raso.
- **Aviso interior:**
  - Aprieta (<30) — ES: «Los párpados pesan una barbaridad.» / EN: «My eyelids weigh a
    ton.»
  - Crítico (0) — ES: «No sé si eso lo he soñado o lo he visto de verdad.» / EN: «Not sure
    if I dreamed that or actually saw it.»

### 6.5 Temperatura corporal

Fuente: `survival_needs.json#bodyTemperature`, `SurvivalModel.cpp`
(`EffectiveTemperature`, `Tick`).

- **Rango:** en °C. Normal: **37**. Hipotermia por debajo de **35**. Golpe de calor por
  encima de **39.5**.
- **Temperatura efectiva del entorno** (`EffectiveTemperature`): temperatura del aire,
  menos viento (×6, atenuado ×0.2 bajo techo), menos 7×Mojado (§6.6), menos 6 si está
  sumergido, más 14×calor de fuego cercano, más 6×exposición al sol, más 8×aislamiento de
  la ropa, más 3 si está bajo techo.
- La temperatura corporal **persigue** un objetivo de `37 + clamp((Efectiva−24)×0.2, −4.5,
  3.5)` (+1.6 adicional con Fiebre activa, §6.14), acercándose con una constante de tiempo
  de 1.2 horas — no cambia de golpe.
- **Hipotermia (<35 °C):** Salud −(35−T)×6/hora. Con T=33, son 12 pts de salud por hora.
- **Golpe de calor (>39.5 °C):** Salud −(T−39.5)×6/hora.
- **Umbral de aprieto (<36 °C):** Ánimo −3/hora adicionales; Eficiencia de trabajo baja
  con cualquier desviación de más de 1 °C sobre los 37 (hasta −0.2).
- **Cómo se cura:** fuego, ropa de abrigo (aislante, `biblia-de-contenido.md` §3.8), techo,
  secarse (§6.6); para el calor, sombra, agua y quitarse ropa.
- **Aviso interior:**
  - Frío aprieta (<36 °C) — ES: «Tengo la piel de gallina.» / EN: «Skin's crawling with
    goosebumps.»
  - Hipotermia (<35 °C) — ES: «No paro de tiritar. Necesito fuego.» / EN: «Can't stop
    shaking. Need fire.»
  - Calor aprieta — ES: «El sol está pegando fuerte.» / EN: «Sun's hitting hard today.»
  - Golpe de calor (>39.5 °C) — ES: «Todo me da vueltas. Sombra, ya.» / EN: «Everything's
    spinning. Shade, now.»

### 6.6 Mojado

Fuente: `survival_needs.json` (`FSurvivalState::Wetness`), `SurvivalModel.cpp` (`Tick`).

- **Rango:** 0–1 (0 seco, 1 empapado). En el agua: siempre 1.0 al instante.
- **Fuera del agua:** `Mojado += (Mojando − Secando×(1−Lluvia)) × Δh`, con `Mojando =
  Lluvia×2` (0 si hay techo) y `Secando = 0.35 + SolExposición×1.2 + CalorFuego×2.5 +
  Viento×0.4`.
- **Efecto:** entra directo en la temperatura efectiva (−7×Mojado, §6.5); estar empapado y
  sin sol ni fuego es la vía más rápida a la hipotermia. Bajo lluvia sin techo, Ánimo
  −2/hora adicionales.
- **Cómo se cura:** sol, fuego cercano, viento, o simplemente tiempo bajo techo sin
  lluvia.
- **Aviso interior (Mojado > 0.7 y frío, §6.5 activo):** ES: «Empapado hasta los huesos.»
  / EN: «Soaked through to the bone.»

### 6.7 Heridas y sangrado

Fuente: `BodyModel.{h,cpp}` (`FWound`, `TreatWounds`, `WoundBleedDamagePerHour`,
`WoundClotPerHour`, `WoundInfectionHours`, `WoundHealHours`, `BandageMaxDepth`).

- **Un corte** tiene Profundidad 0–1 (`Depth`) y Sangrado 0–1 (`Bleeding`, empieza igual a
  la profundidad).
- **Daño mientras sangra:** `Profundidad_del_corte × 10 pts de salud/hora` por cada
  corte abierto (`WoundBleedDamagePerHour = 10`).
- **Coagulación natural** (sin tratar): `Bleeding -= 0.25 × (1 − Depth) × Δh`
  (`WoundClotPerHour = 0.25`) — un corte superficial coagula solo; uno profundo, casi
  nada.
- **Tratamientos** (`EWoundTreatment`):
  - *Agua limpia:* no detiene el sangrado, pero es el primer paso obligado antes de
    vendar.
  - *Venda de tela u hojas:* detiene el sangrado salvo que la profundidad supere **0.8**
    (`BandageMaxDepth`), en cuyo caso lo reduce como mucho a `Depth × 0.25` — sigue
    rezumando.
  - *Sutura* (aguja de hueso y tendón): detiene el sangrado siempre, cualquier
    profundidad.
- **Infección:** un corte sin vendar durante **12 horas** de juego (`WoundInfectionHours`,
  escalado por el modo) se infecta: aplica Infección (48 h) y Fiebre (24 h,
  `S.AddCondition`), evento `WoundInfected`. Una herida infectada deja de coagular sola
  hasta curarse la infección.
- **Cicatrización:** vendada, `Healed += Δh / (36 × (1+Depth)) × Cuidado` con
  `WoundHealHours = 36`; `Cuidado = 1.5` si el vendaje es medicinal (cataplasma de noni,
  gel de aloe), `1.0` si es solo tela/hojas, `0.35` si sigue sin vendar. Ejemplo: un corte
  de profundidad 0.4 vendado con hojas medicinales cicatriza en `36×1.4/1.5 ≈ 33.6 h`.
- **Escorbuto pleno (§6.9, etapa Sangrado):** la coagulación y la cicatrización van a la
  mitad de velocidad mientras dura.
- **Cómo se cura:** limpiar con agua, vendar (tela u hojas), suturar los cortes profundos,
  usar vendaje medicinal para acelerar.
- **Aviso interior:**
  - Corte abierto sangrando — ES: «Esto sangra más de lo que me gustaría.» / EN:
    «Bleeding more than I'd like.»
  - Infectada — ES: «Esta herida no tiene buena pinta.» / EN: «This wound doesn't look
    good.»

### 6.8 Quemaduras

Dos fuentes, con reglas distintas.

**Quemadura solar** (`SunDose`, `SunBurnDoseHours`, `HatSunFactor`,
`SunDoseDecayPerHour`) — [AA], ya implementada:

- A pleno sol sin sombrero ni sombra, la dosis sube 1 pt/hora; con sombrero (`HatSunFactor
  = 0.3`) sube solo 0.3 pt/hora. A la sombra o bajo techo, la dosis baja **0.5 pt/hora**.
- Al llegar a **2.0** horas-equivalentes (`SunBurnDoseHours = 2`), se quema: aplica la
  condición Quemadura Solar durante 10 horas (evento `SunBurned`), que resta Ánimo
  1.0/hora mientras dura.
- **Cómo se cura:** gel de aloe (`biblia-de-contenido.md` §3.7), sombra, esperar a que
  pase.

**Quemadura de contacto** (fuego, brasas, agua o vapor hirviendo) — [AA], implementada
(`ECondition::ContactBurn`, `FBodyModel::ApplyContactBurn`/`SootheBurns`, `FWound::bBurn`,
constantes `ContactBurnDamage`, `ContactBurnDepth`, `ContactBurnHealHours`,
`AloeBurnHealFactor` en `survival_needs.json#body.burns`). Es un tipo distinto de la
quemadura solar porque su daño y su cura son distintos, con el patrón de heridas (§6.7):

- Contacto con fuego abierto, brasas o líquido hirviendo aplica de golpe **8 pts de
  salud** y abre una «herida de quemadura» de profundidad **0.4** que no sangra ni se
  infecta por el mecanismo de §6.7 (el fuego cauteriza), pero sí duele (Dolor, §6.19) y
  cicatriza en **24 horas** sin tratar, o en **12 horas** con gel de aloe.
- El estado `ContactBurn` dura lo que le quede a la peor quemadura abierta; cada contacto
  abre su propia herida. Ni el agua ni las vendas la tratan. Resta ánimo como herida
  (`Injured`, −6) y el evento `Burned` dispara el aviso interior.
- **Cómo se cura:** gel de aloe (`gel_aloe`, `Cures` sobre `SunBurn` y `ContactBurn`): la
  quemadura cicatriza al doble de velocidad desde ese momento — 12 h si se aplica nada más
  quemarse; aplicado a mitad, solo acorta lo que queda. O simplemente esperar.
- **Red (08 §2.9):** la aplica y la simula el servidor; al dueño le llega en su réplica
  (bit de estado y bandera de quemadura en cada herida) y en el RPC fiable de `Burned`.
- **Aviso interior:** ES: «Eso ha dolido. Cuidado con las brasas.» / EN: «That hurt. Watch
  the embers.»

### 6.9 Escorbuto

Fuente: `survival_needs.json#body.scurvy`, `BodyModel.cpp` (`ScurvyStage`, `Tick`).

- **Severidad:** 0–1. Sube mientras la Vitamina C (§6.18) está por debajo de **5**
  (`ScurvyVitaminThreshold`): `+ Δh/240 × Scale` (`ScurvyOnsetHours = 240` = 10 días de
  juego de 0 a escorbuto pleno). Baja mientras la Vitamina C supera **15**
  (`ScurvyRecoveryVitamins`): `− Δh/48` (`ScurvyRecoveryHours = 48` = 2 días para curar del
  todo).
- **Etapas** (`EScurvyStage`, por severidad):
  | Etapa | Umbral | Efecto |
  |---|---|---|
  | Encías | ≥ 0.15 | Ánimo −0.5/hora mientras dura |
  | Visión | ≥ 0.45 | Pérdida de color en pantalla (señal `Desaturation`) |
  | Sangrado | ≥ 0.75 | Sangrado total +0.1 y Salud −0.5/hora (`ScurvyBleedDamagePerHour`); coagulación y cicatrización de heridas a mitad de velocidad (§6.7) |
- Escorbuto pleno también resta hasta **15 pts** a la Energía máxima (§6.3).
- **Cómo se cura:** cítricos y fruta fresca (limón del limonero, `biblia-de-contenido.md`
  §7.1, es la fuente estable desde el día 1 de tenerlo plantado).
- **Aviso interior:**
  - Encías (≥0.15) — ES: «Las encías se quejan. Necesito fruta.» / EN: «Gums are
    complaining. Need fruit.»
  - Visión (≥0.45) — ES: «Los colores se están apagando.» / EN: «Colours are fading out.»
  - Sangrado (≥0.75) — ES: «Esto es escorbuto de verdad. Fruta, ya.» / EN: «This is proper
    scurvy. Fruit, now.»

### 6.10 Intoxicación y venenos

Fuente: `survival_needs.json#body.hazards`, `SurvivalModel.cpp` (`Consume`, condición
`Poisoned`).

- **Origen:** comer o beber algo con propiedad Tóxico. Toxicidades del catálogo:
  agua sin hervir 0.35, agua de charca/manglar 0.85, seta tóxica 0.9, yuca cruda 0.8,
  anacardo crudo 0.5. Al consumir, se tira un dado contra la toxicidad del ítem; si
  sale mal, se aplica Intoxicación durante `8 + 16 × Toxicidad` horas (con la yuca cruda,
  hasta 20.8 h).
- **Mientras dura:** Salud −3/hora, Sed −4/hora adicionales (deshidrata), Energía máxima
  −20.
- **Cómo se cura:** carbón activado (`biblia-de-contenido.md` §3.7), o esperar a que pase.
  Cocinar/hervir antes de consumir evita el riesgo (la yuca cocida, por ejemplo,
  «desintoxica» según receta conocida).
- **Aviso interior:** ES: «Algo de lo que comí no me ha sentado bien.» / EN: «Something I
  ate didn't agree with me.»

### 6.11 Alucinación

Fuente: `BodyModel.cpp` (`HallucinationIntensity`), condición `Hallucinating`.

- **Origen:** seta alucinógena (toxicidad 0.15, `HallucinogenHours = 4`: 4 horas de
  efecto) o falta de sueño extrema (§6.4: solo en el último tercio de privación, intensidad
  máxima 0.35 — nunca tan fuerte como la de la seta, que llega a 1.0).
- **Efecto:** deformación y deriva de color en pantalla (señal `Hallucination`); no daña
  ni engaña las mecánicas (no hace fallar un tallado ni desaparecer un objeto real), solo
  la percepción visual.
- **Cómo se cura:** esperar a que pase (setas), dormir (privación de sueño).
- **Aviso interior:** ES: «¿Eso lo he visto de verdad?» / EN: «Did I really just see
  that?»

### 6.12 Picaduras (medusa y raya)

Fuente: `survival_needs.json#body.stings`, `BodyModel.cpp` (`ApplySting`, `Antidote`).

| Picadura | Duración | Daño/hora | Extra | Cura |
|---|---|---|---|---|
| Medusa | 6 h | 1 pt | — | Vinagre |
| Raya | 12 h | 3 pts | Deja además un corte punzante de profundidad 0.35 (§6.7) | Antídoto de corteza |

Ambas restan Ánimo (evento `Injured`, −6 puntos, §6.16) y son evitables: la raya solo pica
si se le pisa (se evita arrastrando los pies por el fondo); la medusa, quedándose fuera de
su radio.

**Aviso interior:**
- Medusa — ES: «Menudo escozor de medusa.» / EN: «Nasty jellyfish sting, that.»
- Raya — ES: «La raya me ha dado bien.» / EN: «That ray got me good.»

### 6.13 Esguince

Fuente: `survival_needs.json#body.falls`, `BodyModel.cpp` (`FallDamage`, `ApplyFall`).

- **Origen:** caída desde **4.5 m** o más en tierra (`SprainFallHeight`), 1.3× más alto en
  arena (más blanda). En agua, el límite seguro sube a **12 m** (`WaterSafeFallHeight`) y
  no hay esguince, solo daño si se supera.
- **Duración:** `36 × clamp(exceso_de_altura/5, 0.5, 2.0)` horas (`SprainHours = 36`):
  entre 18 y 72 horas según lo alto que fue la caída.
- **Efecto:** Eficiencia de trabajo −0.15 mientras dura; Torpeza (§6.19) +0.25.
- **Cómo se cura:** férula de bambú (`biblia-de-contenido.md` §3.7) cura la condición al
  aplicarla; sin férula, esperar a que pase sola.
- **Aviso interior:** ES: «El tobillo no aguanta mi peso.» / EN: «Ankle won't take my
  weight.»

### 6.14 Fiebre e infección

Fuente: `BodyModel.cpp` (`Tick`), condiciones `Fever`/`Infection`.

- **Origen:** una herida sin vendar más de 12 horas se infecta (§6.7): aplica Infección
  (48 h) y Fiebre (24 h) juntas.
- **Infección — mientras dura:** Salud −1.5/hora.
- **Fiebre — mientras dura:** Energía máxima −20 (comparte el mismo −20 que Intoxicación,
  no se acumulan); temperatura corporal objetivo +1.6 °C (empuja hacia el golpe de calor,
  §6.5).
- **Cómo se cura:** té de corteza de sauce isleño para la fiebre
  (`biblia-de-contenido.md` §3.7); pasta de cúrcuma silvestre para la infección (limpia la
  herida y detiene el ciclo de reinfección, §6.7).
- **Aviso interior:** ES: «Tengo la cabeza ardiendo.» / EN: «My head's burning up.»

### 6.15 Apnea y ahogo

Fuente: `Source/Explored/Player/SwimModel.{h,cpp}` (`FSwimTuning`,
`FSurvivalModel::OxygenDrainPerSecond/OxygenRecoveryPerSecond`).

- **Rango:** Oxígeno 0–100 (0 = ahogándose, 100 = pulmones llenos).
- **Apnea de referencia: 40 segundos** con pulmón base (`BaseBreathHoldSeconds = 40`) sin
  esfuerzo ni peso. Drena a `100 / (40 × RatioPulmón)` pts/segundo, ×1.6 si hay esfuerzo
  (nadar fuerte, perseguir algo), y algo más rápido si se carga más del 30 % del peso
  cómodo. Una mejora de pulmones fabricada o encontrada multiplica `RatioPulmón` (>1
  aguanta más).
- **Recuperación al respirar:** `45 × RatioPulmón` pts/segundo — se rellena en poco más de
  2 segundos con pulmón base.
- **Umbral de jadeo (Oxígeno < 35 al sacar la cabeza):** el personaje jadea de forma
  audible y visible (evento de jadeo), sin daño.
- **En 0 (ahogo):** Salud **−6 pts/segundo** mientras siga sumergido sin aire
  (`DrowningDamagePerSecond = 6`) — a diferencia del resto de necesidades, esto se mide en
  segundos reales, no en horas de juego: ahogarse mata rápido.
- **Cómo se cura:** salir a respirar. No hay objeto que cure el ahogo; solo lo evita
  (mejora de pulmones, tubo de respiración de bambú en aguas someras,
  `biblia-de-contenido.md` §3.4).
- **Aviso interior:**
  - Jadeo (Oxígeno<35 al emerger) — ES: «Casi me quedo sin aire ahí abajo.» / EN: «Almost
    ran out of air down there.»
  - Ahogo (Oxígeno=0) — ES: «¡Aire, necesito aire!» / EN: «Air, I need air!»

### 6.16 Ánimo

Fuente: `survival_needs.json#animo`, `BodyModel.cpp` (`MoraleEventDelta`,
`ApplyMoraleEvent`), `SurvivalModel.cpp` (`Tick`).

- **Rango:** 0–100. Inicial: **60**. **Nunca mata**; solo afecta a la Eficiencia de
  trabajo (§6.19) y a la frecuencia de «ideas» del personaje
  (`biblia-de-contenido.md` §2.5).
- **Eventos puntuales** (`EMoraleEvent`):
  | Evento | Δ Ánimo |
  |---|---|
  | Descubrimiento | +8 |
  | Progreso de mapa | +5 |
  | Isla completamente cartografiada | +15 |
  | Tocar música | +4 |
  | Comida caliente | +3 |
  | Dormir en cama | +5 |
  | Herido (caída grave, picadura, corte) | −6 |
  | Golpe de tormenta | −4 |
- **Flujo continuo (por hora):** +4×calor de fuego cercano, +2 si hay un compañero
  cerca (animal doméstico, [F2]), +1 si está bajo techo; −3 si Hambre<25, −3 si Sed<25, −3
  si Temperatura<36 °C, −2×lluvia si no hay techo, −0.8×factor de monotonía (§6.18),
  −2×dolor (§6.19), −0.5 constante (la soledad pesa), + hasta 3 si suena música
  ambiental cerca; y una tormenta activa resta 1–3 según intensidad (más si no hay
  refugio).
- **Cómo se cura:** fuego, techo, comer caliente, dormir en cama, explorar, tocar la
  flauta — la rutina de un hogar cuidado (`biblia-de-contenido.md` §5.4).
- **Sin aviso interior propio:** el ánimo bajo se nota en que las ideas del personaje se
  espacian y el trabajo cunde menos (§6.19), no en una frase.

### 6.17 Salud

- **Rango:** 0–100. Inicial: **100**. En 0: muerte (§7).
- **Se recupera sola** cuando el cuerpo está estable: Hambre>45, Sed>45 y sin daño activo
  ese instante → **+1.5 pts/hora** caminando o descansando, **+6 pts/hora** durmiendo.
- **Se cura directamente** con cualquier ítem con propiedad Medicinal (miel, aloe,
  antiséptico del botiquín…) que declare puntos de Curación.
- Es la **suma de todo lo demás**: cada estado de esta sección que hace daño (hambre y
  sed en 0, hipotermia/golpe de calor, sangrado, infección, escorbuto pleno, picaduras,
  intoxicación, ahogo, caídas) resta directamente aquí. No tiene aviso propio: el aviso es
  el de la causa. Cerca de 0, la viñeta de pantalla se vuelve intensa y desaturada
  independientemente de la causa — es la señal de «puedo morir ya».

### 6.18 Dieta y monotonía (proteína, hidratos, vitamina C)

Fuente: `survival_needs.json#proteina/energia_dieta/vitamina_c`, `SurvivalModel.cpp`
(`DietBalance`), `BodyModel.cpp` (`MonotonyFactor`).

- **Proteína e Hidratos:** 0–100, inicial **50**, se vacían en **72 horas** (3 días) cada
  una (`NutrientHours`). Proteína sube con pescado y marisco; Hidratos con tubérculo y
  fruta.
- **Vitamina C:** 0–100, inicial **50**, se vacía en **336 horas** (14 días,
  `VitaminHours`) — aguanta mucho más que el resto; el escorbuto (§6.9) es lo que castiga
  descuidarla a largo plazo, no un drenaje rápido.
- **Dieta media** `(Proteína+Hidratos+Vitaminas)/3` por debajo de **25** resta Energía
  máxima adicional (§6.3).
- **Equilibrio de dieta** (`DietBalance`, 0–1): 1 si los tres nutrientes están dentro de
  25 puntos entre sí; baja según se abre la diferencia entre el más alto y el más bajo.
- **Monotonía:** solo cuenta si hay hambre real (Hambre>40) y la dieta está desequilibrada
  (`DietBalance < 0.5`): las horas de monotonía suben a ritmo 1×, bajan al doble (2×) en
  cuanto se come algo variado. A monotonía plena (**72 horas**, `MonotonyFullHours`):
  Ánimo −0.8/hora, Energía máxima −10, Eficiencia de trabajo −0.05. Comer solo coco tres
  días seguidos activa el efecto pleno.
- **Cómo se cura:** variar la dieta (pescado + tubérculo + fruta, no solo una cosa);
  vitamina C con cítricos.
- **Aviso interior (monotonía plena, ≥72 h):** ES: «Coco otra vez. Necesito comer otra
  cosa.» / EN: «Coconut again. I need to eat something else.»

### 6.19 Torpeza, dolor y señales derivadas

No son necesidades propias: se calculan a partir de las de arriba y se sienten en cómo
responde el personaje, nunca en una barra.

- **Torpeza** (`Clumsiness`, 0–1): `0.7×FaltaDeSueño + 0.25 si hay esguince + hasta
  0.2 si BodyTemp<35.5°C + 0.1 si hay intoxicación`. Alta torpeza aumenta los fallos de
  tallado/puntería y el temblor de manos (§6.0).
- **Dolor** (`Pain`, 0–1): compuesto por sangrado total y heridas activas; alimenta la
  pérdida de Ánimo por hora (§6.16, −2×Dolor) y el temblor.
- Ambas se leen en pantalla: temblor de cámara, desplazamiento de manos en primer plano,
  cambio en el paso — nunca un número.

---

## 7. Muerte y reaparición

Fuente: `Source/Explored/ExploredGameMode.cpp`, `Source/Explored/Core/SystemLinks.cpp`
(`DecideRespawn`, `MakeRespawnState`), `BodySignalsComponent::ApplyRespawn`.

- **Muerte:** Salud llega a 0. Si el jugador está en un barco, se le desembarca antes de
  nada (no reaparece enganchado al asiento). Los mandos se bloquean.
- **Decisión de reaparición** (`DecideRespawn`):
  - Si el modo tiene **permadeath** (Náufrago, y Personalizado si se elige así): no hay
    reaparición — a los **3 segundos** (`RespawnDelaySeconds`) se vuelve al menú principal.
  - Si no: reaparece junto al punto de reaparición más cercano (una hoguera marcada como
    tal, o una pieza de construcción marcada como punto de reaparición), a **1.5 m** de
    distancia del fuego (nunca encima); si no hay ningún punto marcado en el mundo,
    reaparece en el punto de inicio de la partida.
  - El retraso antes de reaparecer es el mismo: **3 segundos**.
- **Estado del cuerpo al reaparecer** (`MakeRespawnState`) — «vivo, con el cuerpo tocado»:
  | Necesidad | Tras reaparecer |
  |---|---|
  | Salud | 50 (fija) |
  | Hambre | la que tenía, con suelo de 30 |
  | Sed | la que tenía, con suelo de 30 |
  | Energía | 60 (fija) |
  | Sueño | la que tenía, con suelo de 40 |
  | Ánimo | la que tenía menos 15, con suelo de 20 |
  | Temperatura corporal | 37 °C (fija) |
  | Mojado | 0 (fija) |
  | Heridas | todas curadas |
  | Todos los estados (fiebre, infección, intoxicación, escorbuto…) | despejados |

  Es decir: morir **no** es un reinicio limpio — el jugador vuelve con la mitad de salud
  y algo peor de ánimo, pero sin arrastrar heridas ni venenos, para que la siguiente vida
  no empiece ya perdida. El escorbuto también se despeja al reaparecer [Decisión]:
  penalizar 10 días de fruta perdidos por una muerte sería un castigo desproporcionado
  frente al resto de estados, que sí se limpian.
- **Objetos:** [Decisión, sin código de referencia en este documento] no se pierden al
  morir — el juego no tiene inventario tirable por muerte; morir cuesta tiempo, salud y
  ánimo, no posesiones. Coherente con que no hay economía de moneda que castigar (§5 del
  GDD) y con el tono «duro pero justo» del resto de estados.

---

## 8. Dificultad y modos de juego

Fuente: `survival_needs.json#modeScale/customNeedSpeed/activityMetabolism`,
`SurvivalModel.h/.cpp` (`ESurvivalMode`, `FSurvivalModeSettings`).

| Modo | Multiplicador de necesidades | ¿Las necesidades pueden matar? | ¿Reaparición? | Para quién |
|---|---|---|---|---|
| **Explorador** | ×0.6 (más lento que el estándar) | No — el daño de hambre/sed/frío/etc. nunca baja la Salud de más de 10 | Sí, siempre | Quien quiere el mundo, la exploración y la construcción sin presión de supervivencia (`biblia-de-contenido.md` §8.3) |
| **Superviviente** | ×1.0 (referencia de todos los números de §6) | Sí | Sí | La experiencia prevista del juego |
| **Náufrago** | ×1.35 (más duro) | Sí | **No** — permadeath; morir vuelve al menú (§7) | Quien quiere la tensión completa |
| **Personalizado** | Elegible entre **×0.25 y ×3.0** sobre el ritmo de Superviviente | Elegible (sí/no) | [Decisión] elegible junto con «las necesidades matan»: si las necesidades no pueden matar, tampoco tiene sentido activar el permadeath — se ofrecen como un único interruptor «modo duro» en vez de dos independientes, para no dejar una combinación absurda (permadeath sin poder morir de hambre) | Quien quiere ajustar la velocidad exacta a su ritmo de juego |

La actividad también escala el gasto de necesidades (`activityMetabolism`, independiente
del modo): Descansar ×0.8, Caminar ×1.0, Esprintar ×1.8, Nadar ×1.7, Trabajar ×1.5, Dormir
×0.5 (dormir casi no gasta hambre/sed, coherente con que el sueño en sí se está
recuperando).

---

## 9. Guardado

Fuente: `Source/Explored/Save/SaveSlots.{h,cpp}`, `SaveFormat.cpp`,
`BodySignalsComponent::RestoreSurvival`.

- **Ranuras:** 3 manuales (`manual1`–`manual3`) + 1 automática (`auto`). Cada ranura tiene
  fichero principal (`.sav`) y copia de seguridad (`.bak`) con la versión anterior; si la
  principal está dañada, se lee la copia sin que el jugador tenga que hacer nada.
- **Escritura atómica:** se escribe primero a un temporal (`.tmp`), luego se mueve
  reemplazando la copia y la principal en ese orden — si el juego se cierra a mitad, una
  de las dos versiones sigue siendo una partida completa y legible.
- **Cuándo se guarda** (`ESaveTrigger`):
  | Disparador | Ranura | Cuándo |
  |---|---|---|
  | Manual (menú de pausa) | La manual elegida | Siempre que se pide |
  | Dormir | `auto` | Siempre, al empezar a dormir |
  | Hoguera (encender o usar) | `auto` | Con un mínimo de **300 segundos reales** (5 min) entre dos guardados por hoguera, para no escribir a disco en bucle |
  | Salir al menú o al escritorio | `auto` | Siempre |
- **Integridad:** cada partida lleva una suma de control; si no coincide, se marca dañada
  y se prueba la copia antes de darla por perdida.
- **Versionado:** cada partida guarda su versión de formato; abrir una partida de una
  versión futura del juego (más nueva que el ejecutable actual) se rechaza con un aviso
  claro en vez de cargar datos que no entiende. Las versiones antiguas se migran paso a
  paso hasta la actual.
- **«Continuar»:** ordena las ranuras legibles de la más reciente a la más antigua; a
  igualdad de fecha, gana la de más tiempo jugado.
- **Qué se guarda del cuerpo:** el estado completo de §6 (todas las necesidades, heridas,
  severidad de escorbuto, dosis de sol, horas de monotonía, condiciones activas con su
  tiempo restante) viaja en la sección `body` de la partida y se restaura tal cual
  (`RestoreSurvival`) — no hay pérdida de progreso de supervivencia al cargar, ni siquiera
  de una condición a medio curar.

---

## TODO de implementación

- [x] [AA] `Survival`: añadir `ECondition::ContactBurn` (quemadura de contacto, §6.8) —
      distinta de `SunBurn`; daño instantáneo 8 pts + herida de profundidad 0.4 que no
      sangra ni se infecta, cicatriza en 24 h (12 h con gel de aloe).
- [x] [AA] `Survival`/`Items`: dar a los ítems de gel de aloe la propiedad `Cures` sobre
      `ContactBurn` además de `SunBurn`, y el multiplicador ×0.5 al tiempo de
      cicatrización de esa herida concreta (§6.8).
- [ ] [AA] `Player/SwimComponent`: exponer el evento de daño por ahogo
      (`OnDrowningDamage`) a `BodySignalsComponent::GetMutableSurvivalState().Health` —
      hoy el delegado existe pero, según el comentario del propio fichero
      (`SwimComponent.h`), nadie aplica el daño de ahogo a la salud todavía (§6.15).
- [ ] [AA] `UI`/`Settings`: exponer el rango de duración del día (20–90 min reales, §5.1)
      en Ajustes → Juego con el valor por defecto de 40 min ya existente
      (`UTimeOfDaySubsystem::DayLengthMinutes`).
- [ ] [AA] `Survival`/`Player`: decidir e implementar el punto de aparición inicial fijo
      en Isla del Amaraje con el kit de §4 (mochila, reloj, gafas de sol puestos; mechero,
      navaja rota, botiquín, manual del avión, cantimplora en el fuselaje) — hoy
      `SpawnLandingStarterKitIfNeeded` existe en `ExploredWiringSubsystem` pero su
      contenido exacto debe alinearse con esta lista.
- [x] [AA] `Survival`: fijar `Wetness = 1.0` como valor inicial explícito de
      `FSurvivalState` al arrancar una partida nueva (§4): hoy la struct usa `0.0` por
      defecto y nada lo sobrescribe a `1.0` en el arranque.
- [x] [AA] `Survival`/`UI`: implementar los avisos interiores ES/EN de §6 como líneas de
      voz interna del personaje (subtítulo opcional, coherente con
      `biblia-de-contenido.md` §2.5) enganchadas a los eventos de `ESurvivalEvent` y a los
      cruces de umbral de cada estado. *(datos, `FInnerVoiceModel` y
      `UBodySignalsComponent::OnInnerVoice`; falta el subtítulo en pantalla)*
- [ ] [AA] `UI`: mostrar `UBodySignalsComponent::OnInnerVoice` como subtítulo opcional
      (Ajustes → Accesibilidad), con el texto de `FInnerVoiceModel::Text` en el idioma activo.
- [ ] [AA] `Cooking`/`Player`: llamar a `UBodySignalsComponent::ApplyContactBurn` al tocar
      una hoguera encendida, brasas o una vasija hirviendo (§6.8).
- [x] [AA] `Core/SystemLinks`: documentar en código (comentario junto a `HasPermadeath`)
      que el modo Personalizado une «necesidades pueden matar» y «permadeath» en un único
      interruptor de «modo duro» (§8) — hoy son campos separados en
      `FSurvivalModeSettings` (`bNeedsCanKill`) sin que quede escrito si el permadeath del
      Personalizado depende de él o es un campo propio a añadir.
- [ ] [AA] `Save`: confirmar si el inventario del jugador se conserva íntegro tras morir
      (§7) — no se ha encontrado código de referencia que lo confirme ni lo contradiga;
      si hoy se vacía o se tira algo al morir, hay que decidir entre alinear el código con
      esta decisión o revisar la decisión con el resto del equipo.
- [ ] [F2] `Fauna`/`Building`: cuando entren los animales domésticos, engancharlos al
      bonus de Ánimo «compañero cerca» (+2/hora, §6.16) que hoy ya existe en el modelo
      (`In.bCompanionNearby`) pero no tiene ninguna fuente que lo active.
- [x] [AA] `Tests`: añadir specs de host (`Source/Explored/Tests/`) para
      `ContactBurn` una vez implementado *(`MedicineModelSpec.cpp`, `InnerVoiceModelSpec.cpp`)*, siguiendo el patrón de `BodySpec.cpp` para
      `SunBurn` y los cortes.
