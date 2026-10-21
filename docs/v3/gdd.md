# Baliza Siete — GDD v3 (Explored)

Fecha: 2026-10-21 · rama `reset/v2` · Unreal Engine 5.6, C++ · autor: Rodrigo · estado: definitivo para producción.

Este documento sustituye a `docs/reset/vision.md`, `docs/v1-alcance.md`, `docs/roadmap.md` y `docs/diseno/gdd_v2.md`, que quedan como histórico y se archivan en F1. La historia anterior no se usa en ningún sentido: ni texto, ni nombres, ni arte, ni datos. Las marcas MVP / posterior / fuera indican alcance; todo lo no marcado es MVP.

## 1. Título, pitch y pilares

**Título:** Baliza Siete.

**Pitch.** Iria Valdés, técnica de campo de una empresa que apaga faros, naufraga en una isla cuya baliza era la siguiente de la lista. Para salvarse debe encender esa misma luz: pescar y cocinar aceite, trenzar una mecha y cruzar en balsa a un carguero varado para recuperar la lente. Supervivencia en primera persona, de principio a fin, en una sola isla, en 45-75 minutos.

**Pilares**

1. **El cuerpo marca el ritmo.** Cuatro barras legibles (sed, hambre, energía, calor) y un reloj de 18 minutos por día. Nada obliga a correr, pero todo cuesta tiempo.
2. **Un objeto físico como meta.** Todo lo que se hace (aceite, mecha, lente) acaba en la linterna del faro. El jugador ve el progreso en el mundo, no en una barra.
3. **Corto y sin castigo.** Sin combate, sin muerte, sin estados de fallo definitivos. La única penalización es el desmayo. Se puede terminar en una tarde.

## 2. Historia

**Premisa.** La empresa pública Señales Marítimas del Sur (ficticia) apaga una a una sus balizas luminosas dentro del «Plan Oscuridad»: la navegación por satélite las ha vuelto prescindibles. Iria lleva meses desmontando lentes y cortando suministros. Ha apagado seis. La siete es la Baliza de la Isla de Sal, y debía desmontarla el lunes.

**Personajes.** Iria Valdés (34 años, técnica de campo; seca, práctica, no habla: sus pensamientos aparecen como líneas de subtítulo de ≤ 20 palabras). Anselmo Roig (último farero, ausente; solo existe en su cuaderno). El correo «Mistral» (barco de línea; solo luces y bocina) y su capitana, Ibarra (solo en el epílogo, escrito).

**Texto de inicio (cartel, 6 s):** «Isla de Sal. Martes, 04:10. La galerna se ha llevado la lancha "Gaviota II".»
Primer pensamiento de Iria: «Sal en la boca. Frío. Ni rastro de la lancha.»
Al ver el faro por primera vez: «La Baliza Siete. La que tenía que apagar el lunes.»

**Desarrollo.** Iria despierta en la playa con un cuchillo multiusos, una cantimplora medio vacía y un barómetro de bolsillo. Sobrevive el primer día, llega a la casa del farero y lee la primera página del cuaderno de Roig: él escondió la lente en la bodega del carguero «Sáhara», varado en el bajo, para que nadie pudiera desmontar la luz hasta que alguien volviera a encenderla. El Mistral hace su última travesía de temporada, de noche y pegado a la costa; sin luz no se acerca. Hacen falta tres cosas: lente, aceite y mecha. En el segundo día Iria cruza en balsa, recupera la lente y vuelve antes de que el mar se cierre. Esa noche llega el ciclón: se refugia en la base del faro, mantiene el fuego y recoge agua. El tercer día prepara el aceite y la mecha, monta la linterna y espera la noche.

**Giro y tema.** El cuaderno de Roig revela que mantuvo la luz seis años pagando el aceite de su bolsillo: «Una luz apagada es una promesa rota.» Iria, que apagaba luces por oficio, debe decidir encender esta. Pregunta moral pequeña: ¿merece la pena mantener una luz que nadie paga?

**Final.** Noche del día 3: Iria completa la linterna y la enciende (animación de 3 s; haz giratorio sobre el mar). Luces en el horizonte se acercan, suena la bocina del Mistral. Fundido. Epílogo de 3 líneas según páginas leídas:

- 8 de 8: «Iria entrega el cuaderno completo a la capitana Ibarra. Esa semana firma un informe contra el cierre de la Baliza Siete. La luz sigue encendida.»
- Menos de 8: «Iria sube a bordo en silencio, con el cuaderno a medias. No firma nada esa noche. Pero tampoco vuelve a apagar una luz.»
- Línea común final: «Isla de Sal. Jueves, 23:40.»

Después, créditos (60 s) y vuelta al menú. Sin pantalla de derrota.

**Decisión de F6 (secuencia del final, `FFinaleSequenceModel`):** la llama crece en los 3 s de encender; el cono del haz (`M_BeaconBeam`) se alarga en 1,5 s y gira con el sonido de la lente; de noche, el Mistral aparece al encender y la cámara pasa en 2,5 s a un plano fijo detrás del faro. Bocina a 9,6 s de la aparición (80 % de los 12 s de llegada), fundido a negro a 14,6 s (2 s) y epílogo a 16,6 s. «Continuar» lleva a los créditos y, a los 60 s o con «Volver», al menú.

### 2.1 Cuaderno de guardia (8 páginas; 40-70 palabras; texto definitivo de primera pasada)

Cada página se desbloquea al interactuar con el objeto-página en su lugar. Solo la 1 y la 8 están en el camino crítico. Tras leer una página, Iria añade una nota (≤ 20 palabras) en la misma hoja.

| N.º | Lugar | Crítica | Texto de Roig | Nota de Iria |
|---|---|---|---|---|
| 1 | Casa del Farero (mesa) | Sí | «Cuaderno de guardia de la Baliza Siete. Hoy ha llegado la orden: la luz se apaga con el Plan Oscuridad. He escondido la lente en la bodega del carguero "Sáhara", en el bajo, donde no la buscan. El correo "Mistral" hace su última travesía de temporada pegado a esta costa: sin luz no se acerca. Hacen falta lente, aceite y mecha.» | «Así que fuiste tú. Lente, aceite y mecha. Eso puedo hacerlo.» |
| 2 | Rocas del Pescador | No | «Pesco al amanecer y al atardecer: pican mejor. Para el aceite sirve el pescado, no el coco. Dos tarros alcanzan para una noche entera. El aceite lo pago yo; la empresa dejó de mandarlo hace seis años.» | «Dos tarros. Cuatro peces. Se puede.» |
| 3 | Palmar | No | «La mecha es fibra de palma trenzada. Se recoge del suelo, no de la palma: la palma da cocos y hay que dejarla viva. Cuento ocho palmas en la isla. Con cocos y lluvia se aguanta una semana. Sin lluvia, tres días.» | «Ocho palmas. Dieciséis cocos. Hay que repartirlos.» |
| 4 | Bosque de Bambú | No | «Hice mi balsa con cuatro troncos y dos cuerdas. Sirve para cruzar al bajo con mar en calma. Cuando el barómetro baja de mil, me quedo en tierra: es la regla del faro y la de mi padre.» | «Barómetro bajo mil. Lo tengo en el bolsillo.» |
| 5 | Playa Norte | No | «Desde aquí se ve el bajo. El "Sáhara" varó hace nueve años y el capitán no quiso dejarlo hasta que cayó el último mástil. No sé si fue valor o miedo a la empresa. Rema hacia el mástil: la corriente empuja a la izquierda.» | «Mástil a la vista. Corriente a la izquierda.» |
| 6 | Puente del «Sáhara» | No | «Subí la lente a hombros, treinta y un peldaños de la bodega a cubierta, y la dejé en la caja de mapas. Una lente sola no es una luz. Una luz es lente, aceite y mecha, y alguien que las junte.» | «La caja de mapas. Aquí está.» |
| 7 | Mirador de la Colina | No | «Llevo cuarenta años encendiendo esta luz; los últimos seis, sin que nadie me lo pida ni me lo pague. Dicen que el satélite ve más lejos. No ve a un hombre con una barca sin radio en mitad de la noche. Una luz apagada es una promesa rota.» | «Seis años. Y yo, seis balizas apagadas.» |
| 8 | Linterna del faro | Sí | «Si has llegado hasta aquí, ya sabes más del mar que la empresa. Pon la lente en su aro, vierte los dos tarros, aprieta la mecha y enciende. No es un favor que me hagas a mí. Cuando la veas girar, sabrás por qué la guardé. Gracias, seas quien seas. A. R.» | «De nada, Roig.» |

### 2.2 Pensamientos de Iria (disparadores, máximo 12)

| Disparador | Línea |
|---|---|
| Primer movimiento | «Sal en la boca. Frío. Ni rastro de la lancha.» |
| Ver el faro | «La Baliza Siete. La que tenía que apagar el lunes.» |
| Primera hoguera | «Fuego. Lo demás puede esperar diez minutos.» |
| Entrar en la casa | «Alguien vivió aquí hasta hace poco.» |
| Primera noche, al dormir | «Mañana, el bajo.» |
| Subir a la balsa | «Una balsa. Qué buena idea, Iria.» |
| Ver el carguero | «Nueve años varado y sigue de pie.» |
| Coger la lente | «Pesa menos de lo que parece.» |
| Barómetro bajo 1000 | «Mil. Roig tenía razón: a tierra.» |
| Ojo del ciclón | «Calma. Esto no dura.» |
| Montar la linterna | «Lente, aceite y mecha. Ya está todo.» |
| Encender | «Que se vea desde lejos.» |

## 3. Estructura y duración (primera partida: 45-75 min; objetivo del bot 50 min)

**Decisión de F3e (sustituye a los 18 min de este párrafo y del pilar 1):** 24 h de juego = 24 min reales (1 h = 60 s). Con 18 min la partida competente del bot duraba 36,7 min a 1×, por debajo del mínimo de 47: la duración la fija el reloj (dormir a las 20:00, ciclón de 19:00 a 02:00, encender de noche el día 3), no las tareas. La tabla de abajo conserva los minutos originales; las cifras medidas están en `docs/v3/estado-f3.md`. El ojo sigue durando 65 s (≈ 1,08 h) y el chubasco dura 2 h de juego (120 s).

**Decisión de F6 (sustituye a la de F3e):** 24 h de juego = 26 min reales (1 h = 65 s), para que la partida competente del bot quede en 50-60 min (52,4 min medidos; antes 48,5). La cota del reloj está en `FRunPacingModel` (47 h despierto: 13 + 20 + 14; 50,9 min de juego + 0,3 de final + 1 de créditos = 52,2 min) con su spec. El ojo sigue durando 65 s reales, que ahora son 1 h de juego (22:00-23:00), y el chubasco 2 h de juego (130 s). Verificación pendiente con las 3 rutas del bot: `docs/v3/pendiente-motor.md`.

Reloj: 24 h de juego = 18 min reales (1 h = 45 s). Luz de 06:00 a 20:00 (10,5 min); noche de 20:00 a 06:00. Inicio: día 1, 04:10 (despertar), el jugador pasa a la mañana por fundido corto a las 07:00.

| Fase | Reloj de juego | Minutos reales | Contenido | Objetivo en HUD |
|---|---|---|---|---|
| Prólogo «Galerna» | D1 07:00-08:30 | 4 | Despertar, movimiento, interacción, beber un coco, tutorial de barras | Bebe un coco |
| Día 1 «Orilla» | D1 08:30-20:00 | 11 | Fuego, pesca, pescado asado, camino al faro, página 1, chubasco guiado (15:00, 90 s) para enseñar el barril | Haz fuego → Pesca y cocina un pez → Sube al faro → Lee el cuaderno |
| Noche 1 | D1 20:00-D2 06:00 | 1 | Dormir en refugio (salto), autoguardado al amanecer | Duerme en un refugio |
| Día 2 «El bajo» | D2 06:00-19:00 | 10 | Cuerdas, remo, balsa, travesía 75 s, carguero, lente, vuelta | Prepara la balsa → Cruza al bajo → Encuentra la lente → Vuelve antes de que cierre el mar |
| Ciclón | D2 19:00-02:00 | 5 | Muro 1 (3 h), ojo (1,75 h, 65 s), muro 2 (2,2 h); refugio en la base del faro | Refúgiate en el faro → Sobrevive a la noche |
| Noche 2 | D2 02:00-D3 06:00 | 1 | Dormir, autoguardado al amanecer | Duerme |
| Día 3 «La luz» | D3 06:00-21:00 | 11 | Aceite (2 tarros), mecha, subir y montar la linterna, esperar la noche | Prepara dos tarros de aceite · Trenza la mecha → Monta la linterna → Enciende la baliza |
| Final | D3 ≥ 21:00 | 3 | Encender, Mistral, epílogo | — |
| Créditos | — | 1-2 | 60 s | — |

Total mínimo 47 min; primera partida típica 60-70 min. Autoguardado también el día 3 a las 20:00 (reintento del clímax). El Mistral aparece al encender la baliza si es de noche; si se enciende de día, la linterna queda encendida y el Mistral llega a las 21:00. No hay plazo que cierre la partida: si el jugador tarda, el Mistral «va retrasado» y aparece al encender.

## 4. Bucle de juego

1. Mirar el cuerpo (barras) y el cielo (hora, barómetro).
2. Recolectar en puntos fijos de la isla.
3. Transformar con fuego o con un verbo (encender, trenzar, atar, cocinar, hervir, montar).
4. Consumir o guardar.
5. Avanzar un paso físico hacia la linterna (lente, tarro, tarro, mecha).
6. Cerrar el día con refugio y sueño. El cuaderno da el siguiente objetivo y la historia.

Enseñanza: 8 mensajes contextuales de una línea (una sola vez, sin ventanas bloqueantes): mover, coger, beber, craftear, dormir, cuaderno, barómetro, remar.

## 5. Mecánicas (lista cerrada)

Todos los valores viven en JSON (`survival_needs.json`, `storm_curve.json`, `recipes.json`, `items.json`) para afinar con el bot sin recompilar. Valores de primera pasada; se ajustan en F6 con `PlaytestBot`.

### 5.1 Cuerpo (reutiliza `SurvivalModel`, `BodyModel`, `BodySignals`)

| Barra | Inicio | Pérdida (por minuto real despierta) | Vacía en | Recupera |
|---|---|---|---|---|
| Sed | 55 | -4,5 | 22 min | Coco +25 · cantimplora: 4 sorbos de +20 · barril: 1 carga rellena la cantimplora |
| Hambre | 70 | -2,5 | 40 min | Coco +5 · pescado crudo +20 (y -10 energía) · pescado asado +45 |
| Energía | 70 | -3,0 | 33 min | Dormir (a 100) |
| Calor | 100 | 0 de día; -3 de noche (20:00-06:00) fuera de refugio y de fuego; -4 bajo lluvia | — | Fuego a ≤ 4 m: +25; refugio techado: sin pérdida |

- Con una barra < 25: velocidad -20 % y desaturación por `M_PP_Body` (sin más penalizaciones).
- Aviso de texto a 40 (una vez por barra) y parpadeo del icono bajo 25.
- **Desmayo:** una barra a 0 durante 20 s reales. Fundido a negro, +3 h de juego, despierta en el último refugio usado (por defecto, el de Cala Varada) con sed, hambre y energía a 40 y calor a 60. Sin contador ni recarga de partida.
- **Dormir:** solo en refugio entre 20:00 y 05:00; salta a 06:00. Deltas fijos: sed -25, hambre -15, energía a 100, calor sin cambio.
- Se retira `SExploredWristWatch` de `BodySignalsComponent`.
- Caso límite: dormir con sed < 30 muestra aviso «Tienes demasiada sed para dormir» y no impide dormir (el desmayo posterior lo corrige).

### 5.2 Fuego, cocina y aceite (reutiliza `ExploredFire`, `CookingModel`, `FireModel`)

- Hoguera: 3 leña + cuchillo (verbo «encender»), arde 5 min reales; 1 leña extra = +1,7 min. Radio de calor 4 m.
- Solo el ciclón apaga fuegos al aire libre (muro 1 y muro 2). El interior de la base del faro y el techo del refugio los protegen. Un único volumen `ShelterVolume` marca los techados.
- Pescado asado: 20 s junto a hoguera encendida.
- Aceite: 40 s hirviendo 2 pescados crudos + 1 coco vacío sobre la hoguera = 1 tarro de aceite. Se puede hacer un tercer tarro: sirve de combustible del refugio durante el ciclón (arde 6 min sin gastar leña).
- Sin fuego ni leña de noche: baja el calor, sin otra consecuencia.

### 5.3 Recolección (actores colocados a mano; reutiliza `HarvestModel`, `GroundBranchModel`, `CoconutPalmModel`)

| Recurso | Puntos | Cantidad total | Margen sobre lo necesario |
|---|---|---|---|
| Cocos (8 palmas × 2) | 8 | 16 | +33 % de sed |
| Leña (montones de 2) | 10 | 20 | +30 % |
| Fibra de palma (suelo, montones de 1) | 15 | 15 | +36 % (11 necesarias) |
| Caña de bambú | 3 | 3 | 1 necesaria |
| Troncos | 7 | 7 | 5 necesarios (4 balsa + 1 remo) |
| Páginas del cuaderno | 8 | 8 | — |
| Lente | 1 | 1 | — |

- Interacción de 1 s; sin reaparición. Reaparece solo el pescado.
- Cada recurso es un `AExploredPickup` con `Id` estable (guardado: conjunto de ids recogidos).
- **Caja de suministros** en la base del faro: entrega una sola vez lo mínimo que falte para el camino crítico (1 cuerda, 3 fibras, 2 pescados crudos). `FSupplyCrateModel` calcula qué falta; el bot verifica que ninguna secuencia deja la baliza imposible.
- Mochila: 8 huecos; apilan cocos 4, leña 6, fibra 6, cuerdas 4, troncos 1 (pesado: ocupa una mano), peces 3. Mochila llena: aviso «No te cabe más» y el objeto no se recoge. Objeto no válido al combinar: «No sirve así», sin consumir nada.

### 5.4 Pesca (reutiliza `FishingComponent`, `FishingModel`)

- Caña de pescar en 3 puntos de las Rocas del Pescador. Lanzar, esperar, a los 3 s de mordida pulsar para sacar.
- 1 pez cada ~35 s de media; ×1,5 al amanecer (06:00-08:00) y atardecer (17:00-19:00). No se pesca de noche ni durante el ciclón. La barra de tensión de v1 se elimina: pulsar a tiempo o se escapa.

### 5.5 Agua de lluvia (reutiliza `RainCatchModel`; actor nuevo `ABarrilLluvia`)

- Barril de 4 cargas; cada carga llena la cantimplora (4 sorbos de +20).
- Chubasco guiado D1 15:00 (90 s): llena 2 cargas. Ciclón: llena hasta 4.
- Sin manantial en la isla, para que la lluvia importe.

### 5.6 Crafteo objeto + verbo (reutiliza `Items`, `Crafting`)

Lista cerrada de 6 verbos: encender, trenzar, atar, cocinar, hervir, montar. Interfaz: arrastrar un objeto sobre otro (o sobre un objeto del mundo) abre una lista de máximo 3 verbos válidos. Implementación de F3 (sin arrastrar): las recetas de mano se hacen con el botón de su verbo en el inventario (Tab), y en un objeto del mundo con varios verbos (hoguera: cocinar, hervir, avivar) el aviso enseña «E · verbo   C · siguiente»; C elige y E ejecuta.

| N.º | Receta | Entrada | Verbo | Lugar | Tiempo | Salida |
|---|---|---|---|---|---|---|
| 1 | Hoguera | 3 leña + cuchillo | encender | suelo despejado | 3 s | Hoguera |
| 2 | Cuerda | 2 fibras | trenzar | cualquier | 4 s | 1 cuerda |
| 3 | Caña de pescar | 1 caña de bambú + 1 cuerda | atar | cualquier | 3 s | Caña de pescar |
| 4 | Pescado asado | 1 pescado crudo + hoguera | cocinar | hoguera | 20 s | Pescado asado |
| 5 | Mecha | 3 fibras | trenzar | cualquier | 6 s | Mecha |
| 6 | Tarro de aceite | 2 pescados crudos + 1 coco vacío + hoguera | hervir | hoguera | 40 s | Tarro de aceite |
| 7 | Remo | 1 tronco + 1 cuerda | atar | cualquier | 3 s | Remo |
| 8 | Balsa | 4 troncos + 2 cuerdas | atar | sitio de Playa Norte | 20 s (3 pasos) | Balsa |
| 9 | Montar la linterna | lente + 2 tarros + mecha | montar | linterna del faro (3 ranuras) | 3 interacciones | Linterna montada |
| 10 | Encender la baliza | linterna montada | encender | linterna del faro | 3 s | Final |

Los troncos pesan: se llevan uno a uno (Carry de dos manos). Balsa: los troncos deben estar en el sitio; el jugador los deja uno a uno en el sitio (4 interacciones) antes de «atar».

### 5.7 Balsa y travesía (reutiliza `BoatModel` sin vela, `ExploredBoat::PaddleStroke`, `Ocean`)

- Una balsa fija en Playa Norte, un asiento. Controles: W avanza con remo (2,0 m/s), A/D giran. Sin vela ni timón.
- Distancia a la Isla del Carguero: 150 m (75 s). Corriente lateral débil (0,3 m/s). Sin vuelco: estabilidad forzada y cabeceo amortiguado. Si el jugador cae al agua, nado (`SwimModel`) y re-embarque con E.
- Si calor o sed llegan a 0 en el mar, el desmayo devuelve a la orilla más cercana (Playa Norte).
- A las 18:00 del día 2 el barómetro baja de 1000: aviso «El mar se cierra». A las 18:45, si Iria sigue en el islote, fundido: cruza como puede y aparece en Playa Norte con sed y hambre -20. La balsa no se usa después del día 2 (queda varada como decorado).
- Plan B si la balsa no queda jugable en F3: banco de arena vadeable con marea fija o cruce con fundido a negro (decisión de F3, no cambia la historia).

### 5.8 Clima, barómetro y ciclón (reutiliza `Weather`, `Events`, `Sky`; modelo nuevo `FStormClockModel`)

El barómetro es un objeto en mano del kit inicial; su lectura en hPa con flecha de tendencia aparece en el HUD cuando está en la mochila. Curva fija, sin azar:

| Momento | hPa | Estado |
|---|---|---|
| D1 07:00 | 1014 | Despejado |
| D1 15:00 | 1010 | Chubasco guiado (90 s) |
| D2 06:00 | 1008 | Nubes altas |
| D2 18:00 | 998 | El mar se cierra |
| D2 19:00 | 990 | Inicio del ciclón |
| D2 22:00-23:45 | 965 | Ojo del ciclón (65 s reales: calma, cielo estrellado, música baja) |
| D3 02:00 | 995 | Fin del ciclón |
| D3 06:00 | 1012 | Despejado |

- Muro 1 y muro 2: viento, olas de 3-4 m, lluvia, visión reducida (niebla), fuego al aire libre apagado, calor -4/min fuera de techo. Tras el ciclón el mar vuelve a calma.
- Ojo: 60-90 s. Solo cambian clima y audio; sin cinemática. Iria puede salir a recoger agua del barril.
- Constelaciones y luna siguen siendo correctas (`Sky`), sin función de juego.

### 5.9 Progreso de la historia (reutiliza `ExploredProgress`; `ExploredVoyageSubsystem` reescrito como máquina de etapas)

Etapas (`EBeaconStage`): `Prologo`, `Orilla`, `Faro`, `Balsa`, `Carguero`, `LenteEnMano`, `Ciclon`, `Preparacion`, `LinternaMontada`, `Final`. Hitos guardados: lente recogida, tarros ×2, mecha, linterna montada (3 ranuras), baliza encendida, páginas leídas (bitmask de 8).

### 5.10 Guardado (reutiliza `SaveArchive`, `SaveSlots`)

Una ranura «Continuar». Autoguardado al amanecer de cada día y el día 3 a las 20:00. Guardar manual desde la pausa, en la misma ranura. Secciones: progreso, cuerpo, inventario, fuegos, reloj, ids recogidos, páginas.

## 6. Isla de Sal

Mapa nuevo `/Game/Maps/Map_IslaSal`. **Decisión de F3a (sustituye al `Landscape` de este párrafo):** el terreno es una malla estática facetada low poly (`SM_IslaSal_Terrain`, triángulos de ~3 m, normales planas, color por celda de la hoja de paleta) que genera `Tools/Island` desde `island.json`; `M_Toon` sustituye a `M_ToonLandscape`; las colocaciones viven en `Content/Data/island_props.json` y las crea `AExploredIslandPopulator`. Texto original: terreno: un `Landscape` generado por script (`Tools/Island/gen_heightmap.py` a partir de `Content/Data/island.json`: elipses y gaussianas a mano, determinista) más mallas estáticas de acantilado. Máximo 400 × 400 m. Isla principal ≈ 330 × 260 m; islote del carguero a 150 m al nordeste. Material `M_ToonLandscape`: capas de color plano (arena, hierba, roca) tomadas de la paleta. El mapa incluye `PlayerStart`, `ExploredSkyController`, `ExploredOcean`, `PostProcessVolume` (`M_PP_Body`) y un solo `DirectionalLight`.

| Zona | Tamaño aprox. | Contenido |
|---|---|---|
| Cala Varada (sur) | 80 × 60 m | Inicio del jugador; 2 palmas; 1 refugio de palma; 2 montones de leña; página: ninguna |
| Palmar (centro-sur) | 110 × 80 m | 6 palmas; 8 fibras; 4 montones de leña; página 3 |
| Rocas del Pescador (oeste) | 70 × 50 m | 3 puntos de pesca; roca grande; página 2; 2 montones de leña |
| Bosque de Bambú (noroeste) | 90 × 80 m | 3 cañas de bambú; 7 troncos; 7 fibras; página 4 |
| Playa Norte | 70 × 40 m | Sitio de la balsa; página 5; muelle natural de arena |
| Colina del Faro (centro-norte) | 100 × 90 m, cota 35 m | Torre (12 m), Casa del Farero, base del faro (refugio con techo), caja de suministros, barril de lluvia, hoguera fija del refugio, páginas 1 (casa), 7 (mirador), 8 (linterna); 2 montones de leña |
| Isla del Carguero (islote) | 70 × 50 m | Casco del «Sáhara» varado, puente (página 6, caja de mapas con la lente), rocas |

Recorridos: Cala Varada a Palmar 90 m; Palmar a Colina 80 m; Colina a Playa Norte 100 m. Cruzar la isla de sur a norte a pie: ≈ 70 s. Sin zonas bloqueadas ni llaves: la progresión la marca el material, no la geografía. Dos refugios: Cala Varada y base del faro (`ShelterVolume` y punto de reaparición del desmayo).

## 7. Objetos (lista cerrada: 17)

| Id | Nombre | Pila | Uso / descripción |
|---|---|---|---|
| `cuchillo` | Cuchillo multiusos | 1 | Kit inicial; abre cocos, enciende (pedernal) |
| `cantimplora` | Cantimplora | 1 | 4 sorbos de +20; se rellena en el barril |
| `barometro` | Barómetro de bolsillo | 1 | Lectura en el HUD |
| `coco` | Coco | 4 | Beber: +25 sed, +5 hambre; deja `coco_vacio` |
| `coco_vacio` | Coco vacío | 4 | Cuenco para el aceite |
| `lena` | Leña | 6 | Combustible |
| `fibra` | Fibra de palma | 6 | Material |
| `cuerda` | Cuerda | 4 | Material |
| `cana_bambu` | Caña de bambú | 2 | Material |
| `cana_pescar` | Caña de pescar | 1 | Pesca |
| `tronco` | Tronco | 1 | Material pesado (una mano) |
| `remo` | Remo | 1 | Necesario para la balsa |
| `pez_crudo` | Pez | 3 | +20 hambre, -10 energía |
| `pez_asado` | Pez asado | 3 | +45 hambre |
| `tarro_aceite` | Tarro de aceite | 2 | Combustible de la linterna (o del refugio) |
| `mecha` | Mecha | 1 | Material de la linterna |
| `lente` | Lente de Fresnel | 1 | Objeto clave; pesa; ocupa una mano |

Objetos del mundo no recogibles: hoguera, barril de lluvia, refugio de palma, balsa, caja de suministros, linterna del faro, páginas del cuaderno.

## 8. Fauna (3 especies como máximo)

| Especie | Estado | Comportamiento | Sistema |
|---|---|---|---|
| Pez | MVP | Bancos en boids bajo el agua; cerca de las Rocas del Pescador; sustituye a los peces de v1 por una malla del estilo nuevo | `BoidsModel`, `MarineCreatureBrain` |
| Gaviota | MVP | 6 en boids sobre la costa; huyen al ciclón | `BoidsModel` |
| Cangrejo | Posterior (solo si sobra tiempo en F7) | Ambiental en Cala Varada, sin lógica de juego | `ProceduralGait` (solo modelo hoy) |

Fuera: tortuga, tiburón, ballena y fauna terrestre. Se borran sus entradas de `fauna.json`.

## 9. Interfaz

Resolución de diseño 1920 × 1080. Texto mínimo 22 px; tipografía serif sencilla; iconos planos de un solo color de la paleta. Todo en Slate/canvas reutilizado (`ExploredHUD` se adapta, no se reescribe).

### 9.1 HUD

| Elemento | Posición | Contenido y regla |
|---|---|---|
| Barras de cuerpo | Abajo izquierda | 4 iconos (sed, hambre, energía, calor) con barra fina; parpadean bajo 25; sin números |
| Ranuras rápidas | Abajo centro | 3 ranuras (teclas 1-3) con el objeto en mano resaltado |
| Aviso de interacción | Centro, bajo la retícula | «E · Beber coco» (tecla + verbo + objeto); retícula de punto |
| Objetivo actual | Arriba izquierda | Una línea, máximo 60 caracteres, de `objectives.json` |
| Reloj y día | Arriba derecha | «Día 2 · 15:40» |
| Barómetro | Arriba derecha, bajo el reloj | «1008 hPa ↓»; solo si se tiene el barómetro |
| Pensamiento / aviso | Abajo, sobre las barras | Subtítulo de ≤ 20 palabras, 4 s |
| Barra de acción | Centro-abajo | Progreso de acciones largas (aceite 40 s, balsa 20 s) |

Sin minimapa, sin brújula, sin números salvo en el inventario.

### 9.2 Pantallas

| Pantalla | Contenido | Estado |
|---|---|---|
| Menú principal | Continuar (si hay guardado), Nueva partida, Ajustes, Créditos, Salir. Fondo: la isla al atardecer con la baliza apagada | MVP |
| Pausa | Reanudar, Guardar, Ajustes, Volver al menú | MVP |
| Ajustes | Volumen (música, ambiente, efectos), sensibilidad del ratón, calidad gráfica (3 niveles: reutiliza `SettingsLogic`). Solo español | MVP |
| Inventario (Tab) | 8 huecos; descripción breve; arrastrar sobre otro objeto para combinar; lista de recetas descubiertas | MVP |
| Cuaderno (J) | Páginas desbloqueadas (X/8) con nota de Iria y objetivo actual | MVP |
| Cartel de título | Aparece al empezar cada día: «Día 2» | MVP |
| Fundido / desmayo | Negro con texto «Despiertas en el refugio…» | MVP |
| Pantalla final | Epílogo de 3 líneas | MVP |
| Créditos | 60 s, música de `docs/creditos-musica.md` | MVP |
| Mapa dibujado, logros, museo, cartografía | — | Fuera |
| Mando, inglés | — | Posterior (tras la primera entrega) |

Controles: WASD mover, Shift correr, Espacio saltar, E interactuar, C elegir verbo, 1-3 ranuras y clic para comer o beber lo de la ranura, Tab inventario (con cursor), J cuaderno, Esc pausa, ratón mirar. En la balsa: W, A, D.

## 10. Audio (reutiliza `Audio/*`, `MusicDirector`; audio ya generado)

- Música clásica suave ya generada (`Content/Generated/Audio`): capas por estado: calma de día, noche, ciclón (tensa, sin percusión), ojo (silencio casi total), final (tema de resolución). El director solo necesita 5 estados.
- Ambiente: olas, viento, gaviotas, lluvia, crepitar del fuego, interior del faro. Se retiran las capas de mina y aldea.
- Efectos: pasos (arena, hierba, roca, madera), recoger, cocinar, remar, bocina del Mistral, giro de la lente.
- Sin voces ni doblaje. Mezcla en `Audio/Classes`.

## 11. Dirección de arte (un único estilo, no negociable)

El arte de v1 se rechazó por cutre e incoherente. Todo asset nuevo cumple estas reglas o se rechaza.

| Regla | Valor |
|---|---|
| Estilo | Low poly cartoon, formas simples y redondeadas, siluetas legibles a 30 m |
| Paleta | **41 colores** (32 de base más 9 de la pasada visual de F4: nube, cielo y hierba) en una hoja única (`Tools/Style/paleta.json`, hoja 32 × 3 píxeles: 0-31 en la fila central, 32-63 en la superior); cada polígono mapea a una celda; sin texturas fotográficas ni de detalle |
| Material | `M_Toon` (3 bandas de sombreado, sin normal map, sin specular) para todo; `M_ToonLandscape` para el terreno; `M_Ocean` low poly (facetas planas, bandas de paleta por profundidad y franja de espuma, sin reflejos ni translucidez), cielo `M_SkyGradient` con nubes K5 y discos de sol y luna, y `M_Stars`; variantes por instancia de material, no por textura |
| Iluminación | Una `DirectionalLight` cálida + luz de cielo que captura la cúpula (sombras tintadas, nunca negras); **sin Lumen** (GI ni reflejos: mismo aspecto y 230-250 fps frente a 187-212 en las vistas f4b); sin SSAO, SSR, grano ni aberración; `M_PP_Toon` (bandas) se queda |
| Escala y pivotes | Metros; pivote en la base; nombre `SM_<Nombre>_01` |
| Validador | `Tools/Blender/check_style.py` (en Blender) y commandlet en `ExploredEditor` (en Unreal): rechaza texturas, más de un material, UV fuera de celda, ngones, exceso de polígonos, escala no unitaria |
| Producción | Todo por script en Blender (`Tools/Blender`, `uv run`); lámina de revisión (`render_preview.py`) por kit para la aprobación de Rodrigo |

**Excepción del terreno (F6, «relieve y textura», 2026-10-21).** El terreno no usa `M_ToonLandscape` ni la hoja por UV: es la malla `SM_IslaSal_Terrain` (Tools/Island) con `M_TerrainToon`, cuyo color base es el color por vértice (colores de `paleta.json`, tres tonos por zona con variación de alta frecuencia) multiplicado por una única máscara en escala de grises, `T_TerrainStrokes` (512 × 512, trazos estilizados generados por script en `Tools/Textures`, sin fotos), muestreada en espacio de mundo a dos escalas (3,2 m y 9,7 m). La máscara solo oscurece o aclara el color de paleta (factor 0,7-1,24, media neutra) y se apaga en pendientes de roca y a más de 35-90 m; no introduce colores nuevos. Es la única textura de detalle permitida: el resto de mallas sigue con `T_Paleta`. Normales del terreno: 60 % suave del vértice y 40 % de la cara (lectura low poly moderada). Parámetros en `Tools/Textures/strokes.json`; comparación en `docs/v3/laminas/relieve-antes-despues.png`.

Paleta (primeros 12 de 32, el resto en `paleta.json`): arena clara `#E8D8A8`, arena oscura `#C9B27C`, hierba clara `#8DBB5A`, hierba oscura `#4F8A3C`, verde palma `#3E7D3A`, madera clara `#B98A5A`, madera oscura `#7A5233`, roca media `#7C7C78`, óxido `#B5532F`, rojo faro `#C8402F`, latón `#D9A441`, cristal `#9FD6E0`.

Presupuesto de triángulos por malla (tope duro):

| Tipo | Tris máx. |
|---|---|
| Objeto de mano | 300 |
| Hierba, arbusto, roca pequeña | 200-400 |
| Palmera, bambú, roca grande | 1.500 |
| Refugio, barril, hoguera, balsa | 800 |
| Torre, linterna, casa del faro | 3.000 |
| Casco y puente del carguero | 4.000 |
| Escena visible (media) | < 400.000 |

### 11.1 Lista cerrada de assets únicos (34 mallas de prop; tope firme 35)

| Kit | Mallas | N.º |
|---|---|---|
| K1 Naturaleza | `SM_PalmA`, `SM_PalmB`, `SM_Bamboo`, `SM_Shrub`, `SM_GrassClump`, `SM_RockBig`, `SM_RockSmall`, `SM_CliffRock` | 8 |
| K2 Estructuras | `SM_FaroTorre`, `SM_FaroLinterna` (con 3 ranuras), `SM_FaroCasa`, `SM_CajaSuministros` (también caja de mapas con material alterno), `SM_CargueroCasco`, `SM_CargueroPuente`, `SM_RefugioPalma`, `SM_BarrilLluvia`, `SM_Hoguera`, `SM_Balsa` | 10 |
| K3 Objetos | `SM_Cuchillo`, `SM_Cantimplora`, `SM_Barometro`, `SM_Coco` (vacío = variante), `SM_Lena`, `SM_Fibra`, `SM_Cuerda`, `SM_CanaBambu`, `SM_CanaPescar`, `SM_Tronco`, `SM_Remo`, `SM_Pez` (asado = variante), `SM_TarroAceite`, `SM_Mecha`, `SM_Lente` | 15 |
| Personaje | `SM_ManoIria` (antebrazos en primera persona; sin cuerpo completo ni esqueleto) | 1 |
| **Total** | | **34** |

Además: fauna (pez, gaviota; cangrejo posterior), 1 `MI_Toon` por color usado (instancias), páginas del cuaderno como primitiva `Plane` del motor con `MI_Toon` de papel (no cuenta como malla única). El Mistral no tiene malla: solo luces y bocina.

## 12. Fuera de alcance

Todo lo anterior a la historia de este documento; multijugador; combate y armas; minería y terraformado; construcción libre; granja; aldeas y personajes vivos; piratas; mundo procedural; logros; museo; cartografía y mapa; ruinas y petroglifos; vela y viento de la balsa; enfermedad, heridas y animales hostiles; durabilidad y mejora de herramientas; segunda isla; fauna terrestre, tortuga y eventos de tortuga; inglés, mando y accesibilidad avanzada (posterior); reaparición de recursos (salvo pescado); doblaje de voz; Mistral como malla.

## 13. Riesgos

| # | Riesgo | Prob. | Mitigación |
|---|---|---|---|
| R1 | Isla de autor inexistente; mayor riesgo de plazo | Alta | `Landscape` por script determinista, tope 400 × 400 m, decisión cerrada en F3 |
| R2 | Poda: Wiring, Character, GameMode y PlayerController acoplados a Achievements, Building y Farming | Alta | Un commit por paso con `HostTests` en verde; ver `plan.md` F1 |
| R3 | Balsa: comportamiento hidrostático inestable | Media | Estabilidad forzada, sin vuelco; plan B en 5.7 |
| R4 | Rutas `/Game/Generated/...` cableadas fallan en ejecución | Alta | Barrido de rutas en F1; tabla de rutas en `plan.md` |
| R5 | Arte: estilo v1 rechazado; deriva de estilo | Media | Validador automático + lámina de revisión por kit; sin reutilizar nada sin pasar `check_style.py` |
| R6 | Equilibrio de supervivencia sin playtest | Media | Valores en JSON; criterio medible con bot en F6 |
| R7 | Bloqueo (softlock) por escasez de recursos | Media | Margen del 30 %, caja de suministros, `PlaytestAuditor` |
| R8 | Dependencia de `RainCatchModel` (solo modelo hasta hoy) | Media | Actor `ABarrilLluvia` mínimo en F3 |
| R9 | Recolección actual depende de `ExploredVegetationCell` | Alta | Actor `AExploredPickup` en F3 |
| R10 | Narrativa: exposición larga | Baja | ≤ 70 palabras por página; solo 2 páginas obligatorias |
| R11 | Contradicciones con `docs/reset/inventario.md` | Resuelta | Ver `plan.md` §3 (Cartography, Ruins, Exploration pasan a BORRAR; `ExploredHUD` se adapta en vez de borrarse) |
