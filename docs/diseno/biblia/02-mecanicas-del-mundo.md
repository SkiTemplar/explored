# EXPLORED — Biblia de diseño · 02. Mecánicas del mundo

Complementa `docs/diseno/gdd_v2.md` (documento rector) y
`docs/design/biblia-de-contenido.md` (catálogo y sistema de combinación).
Desarrolla al detalle las decisiones del director de diseño del 2026-09-27
(mundo interactivo, minería volumétrica, barcos por piezas, raíles, granjas,
fauna CC0, murallas, pueblos, piratas). Toda mecánica de esta sección hereda
el sistema de propiedades/plantillas/verbos ya definido en la biblia de
contenido §2 — no se repite aquí salvo cuando una mecánica añade una
propiedad, un verbo o una plantilla nueva.

Fases: **[AA]** acceso anticipado, **[F2]** / **[F3]** según el recorte de
alcance de `gdd_v2.md` §6.2. Cuando esta sección no dice lo contrario, hereda
la fase de la mecánica madre en el GDD.

Convenciones numéricas (coherentes con `Content/Data/*.json`): propiedades
0–5, integridad de pieza 1–100, durabilidad de herramienta en unidades
enteras sin barra visible (se lee por desgaste del modelo), 1 día = 24 h de
juego, año = 32 días (biblia §6.1).

---

## 1. Recolección y tala universal [AA]

### 1.1 Reglas generales

- Un único verbo contextual, **E**, cubre recoger del suelo, talar un árbol
  y picar terreno; el juego decide cuál según lo que se mira (regla de
  «máximo tres verbos», biblia §8.3).
- Recoger del suelo (fruta, rama, concha, mineral suelto) es instantáneo
  (0,4 s, una animación de agacharse). Talar y picar tienen un tiempo por
  golpe (tabla 1.2) y consumen resistencia como cualquier actividad de
  `Working` (GDD §3.1, `FSurvivalModel`).
- Sin herramienta, a mano solo se recolectan objetos ya sueltos (fruta caída,
  ramas, conchas) y se golpea vegetación blanda (hierba, helecho, taro
  silvestre) al ritmo de la tabla; la tala de troncos exige hacha.

### 1.2 Tala por especie

Golpes y madera calculados para un hacha de piedra (Filo 2, Contundente 2,
la mínima que corta madera dura, biblia §3.4 «Plantillas»); cada nivel de
hacha superior (pedernal/obsidiana tallada → obsidiana → aluminio) resta un
golpe, con un mínimo de 1. Botín final: madera + rama-suelo (véase 1.3).

| Especie | Mallas (`Art/Export`) | Golpes (hacha piedra) | Tiempo/golpe | Madera que suelta | Fruto | Altura | Cae hacia |
|---|---|---|---|---|---|---|---|
| Palmera de coco | `Fauna`/kit palma existente | 6 | 1,1 s | 2× `tronco_pequeno`, 4× `hoja_palma` | 1–3× `coco_maduro` al caer (`HarvestModel.cpp`, `Palm.FellDrops`); parte se pierde o se abre con el golpe, nunca los 100 % de los que tenía. El `coco_verde` no cae solo: se coge trepando vivo el tronco (§13.1) — talar da el fruto maduro del suelo, trepar da el agua del verde | 9 m | Lado del último golpe, ±15° por viento |
| Árbol de la jungla (dosel) | `SM_JungleTreeCanopy_01` | 9 | 1,3 s | 3× `tronco_pequeno`, 1× `madera_dura` | — | 14 m | Lado del último golpe |
| Árbol de la jungla (redondo) | `SM_JungleTreeRound_01` | 8 | 1,3 s | 2× `tronco_pequeno`, 2× `madera_dura` | — | 11 m | Lado del último golpe |
| Árbol de la jungla con lianas | `SM_JungleTreeVines_01` | 9 | 1,3 s | 3× `tronco_pequeno`, 3× `liana` extra | — | 13 m | Lado del último golpe; las lianas se sueltan antes de que caiga el tronco |
| Guayabo (madera dura) | por generar | 10 | 1,4 s | 1× `tronco_pequeno`, 3× `madera_dura` | 1–2× `guayaba` (ya en catálogo de comida) | 8 m | Lado del último golpe |
| Balsa (madera blanda) | por generar | 4 | 0,9 s | 3× `madera_blanda` | — | 7 m | Lado del último golpe, más sensible al viento (±25°) |
| Arbusto de hoja ancha | `SM_ShrubBroadleaf_01` | 2 | 0,8 s | — | — | 1,2 m | No cae con dirección: se arranca entero |
| Helecho arbustivo | `SM_ShrubFern_01` | 1 | 0,6 s | — | — | 0,8 m | No cae con dirección |
| Taro arbustivo silvestre | `SM_ShrubTaro_01` | 1 | 0,6 s | — | 1× `taro` | 0,6 m | No cae con dirección |
| Mata de hierba (A/B) | `SM_GrassClumpA_01`/`B_01` | 1 (a mano) | 0,4 s | — | ocasional: `fibra_coco`, `algodon_silvestre` | 0,3 m | No cae con dirección |
| Limonero | ver plants.json (`limonero`) | — | — | — | — | — | **No se puede talar** (`neverRemoved: true`) |

- **Dirección de caída:** vector de caída = dirección del golpe final
  proyectada al plano horizontal, con hasta ±20° de desviación por la
  velocidad del viento del momento (`FWeatherModel::SampleAt.Wind`, ya
  existente). Un árbol que cae puede aplastar construcción ligera (tier
  `palma`/`bambu`, daño = 40 % de integridad) y hacer de puente sobre un
  arroyo o un hueco de menos de 3 m (contenido emergente, biblia §11, ya
  documentado como intención de diseño).
- **Rebrote:** un tocón (`FVegetationHarvestState`, ya existe para scatter)
  vuelve a un árbol talable a los **18 días de juego** para especies de
  fruta (palmera, guayabo) y **24 días** para madera dura sin fruto; los
  arbustos y matas de hierba rebrotan a los **4 días**. El tocón en sí no se
  puede talar de nuevo hasta que ha rebrotado del todo.

### 1.3 Ramas del suelo

Bajo cada árbol talado o intacto se generan 2–4 `rama_seca` sueltas por
ciclo de 6 horas de juego (tope de 6 por árbol a la vez); se recogen con **E**
instantáneo, sin herramienta. Es la fuente de yesca/leña de bajo esfuerzo que
sostiene el fuego sin tener que talar.

### 1.4 Feedback audiovisual

- Cada golpe: sonido de impacto por material (madera dura/blanda/bambú/hoja),
  partículas de astillas, sacudida de la copa.
- Penúltimo golpe: la textura del tronco muestra una grieta blanca (aviso
  legible de «un golpe más» sin HUD, coherente con biblia §8.3 «legibilidad
  low-poly»).
- Caída: sonido de crujido + impacto, temblor de cámara si el jugador está a
  menos de 5 m, pájaros que levantan el vuelo del árbol vecino.

### 1.5 Casos límite

- Talar bajo la copa de otro árbol no afecta al vecino; solo el árbol que
  recibe el golpe cae.
- Talar cerca de un marae (menos de 15 m) baja la reputación con el pueblo
  del arrecife si esa isla ya tiene pueblo activo (GDD §3.9); un aviso sutil
  (el personaje duda: «esto no me parece buena idea») avisa antes del primer
  golpe, no después.
- Un árbol que cae al agua flota (`Flota` heredado del material) y se pesca
  como madera flotante normal si no se recoge entero.

### 1.6 Persistencia

Tocón + rebrote van en la misma capa `"harvested"` de `FSaveWorldDeltas` que
ya usa el scatter recolectado hoy; no hace falta una sección de guardado
nueva, solo un estado adicional por instancia (`Stump`, con el día en que
puede rebrotar).

---

## 2. Minería y edición del terreno [AA]

### 2.1 Reglas generales

- Picar = restar densidad (`FTerrainDensity::Density`, GDD §7.3) en una
  esfera centrada en el punto de impacto de la herramienta. El radio de la
  esfera depende de la herramienta (tabla 2.3), no del material.
- Cada golpe efectivo (herramienta ≥ dureza mínima del estrato) resta una
  esfera completa; un golpe con herramienta insuficiente **no resta
  densidad**: rebota (sonido metálico agudo, chispa si es piedra contra
  piedra, sin coste de durabilidad de la herramienta) y solo cuesta
  resistencia del jugador, para que quede claro que hace falta mejor pico
  sin penalizar el intento.
- El botín de un golpe efectivo es determinista por golpe (no por RNG de
  loot): un golpe = una unidad del ítem de ese estrato (tabla 2.3), salvo
  donde se indique lo contrario (vetas finitas). En el terreno volumétrico del
  GDD v2 §3.4 eso son **6 unidades por m³** (`mining.json/unitsPerM3`).

### 2.2 Herramientas de minería (catálogo nuevo)

Sigue el mismo patrón que `hacha`/`martillo`/`pala` en `Content/Data/items.json`
y `templates.json`: una única plantilla `pico` cuyo resultado hereda de sus
piezas, con cuatro combinaciones canónicas que dan nombre y curva de
progresión (igual que «hacha de piedra» → «hacha de pedernal» → … en la
biblia §3.4).

**Item base (a añadir a `items.json`):**

```json
{ "id": "pico", "nameEs": "Pico", "nameEn": "Pick",
  "weightKg": 1.7, "volumeLiters": 1.3, "size": "Mediano",
  "tags": ["herramienta", "mineria"], "maxDurability": 55, "properties": [] }
```

**Plantilla (a añadir a `templates.json`, mismo patrón que `hacha`):**

```json
{ "id": "pico", "nameEs": "Pico", "nameEn": "Pick",
  "verbs": ["Atar", "Pegar"],
  "resultDefinitionId": "pico",
  "nameTemplate": "Pico de {0} con mango de {1}, atado con {2}",
  "nameTemplateEn": "{0} pick with a {1} handle, lashed with {2}",
  "baseMaxDurability": 55,
  "slots": [
    { "role": "Cabeza", "requireAll": false,
      "requirements": [ { "property": "Punta", "min": 2 } ], "tags": ["piedra", "mineral", "metal"] },
    { "role": "Mango", "requireAll": true,
      "requirements": [ { "property": "Largo", "min": 2 }, { "property": "Rigido", "min": 3 } ] },
    { "role": "Union", "requireAll": false,
      "requirements": [ { "property": "Ata", "min": 2 }, { "property": "Adhesivo", "min": 2 } ] }
  ] }
```

| Nombre | Cabeza (material) | Radio de esfera | Tiempo/golpe | Durabilidad | Peso | Nivel |
|---|---|---|---|---|---|---|
| **Pico de piedra** | `canto_aguzado` (lasca + `canto_rodado`, Tallar) | 0,40 m | 1,3 s | 55 | 1,6 kg | Nivel 1 (tosco); `ToolTier` 2 |
| **Pico tallado** | `basalto_tallado` (lasca + `basalto`, Tallar) | 0,42 m | 1,2 s | 75 | 1,9 kg | Nivel 2 (tallado); `ToolTier` 3 |
| **Pico de obsidiana** | `obsidiana` | 0,50 m | 1,0 s | 30 | 1,2 kg | Nivel 3 (obsidiana); `ToolTier` 4 |
| **Pico rescatado** | `cabeza_pico_rescatada` (`chapa_fuselaje`/`hierro_meteorito` golpeados en el `banco_chatarra`) | 0,55 m | 1,1 s | 95 | 2,1 kg | Nivel 4 (rescatado); `ToolTier` 4 |

- **Cabeza con Punta** (integración 2026-09-28): el slot `Cabeza` exige `Punta ≥ 2`
  para que `canto_rodado`/`piedra_plana`/`basalto` + mango sigan dando el **hacha de
  piedra** (§1.2); cada cabeza de pico se prepara antes (tabla). Numeración: el «Nivel»
  de esta biblia es el `ToolTier` del C++ menos uno (0 mano, 1 pala); `mining.json/tools`
  lleva el `ToolTier`.
- **Fragilidad del pico de obsidiana** (biblia §2.4, «se rompen si golpeas
  piedra»): cada golpe efectivo contra un estrato de dureza ≥ 3 (basalto,
  obsidiana, hierro, cristal) tiene un **8 % de probabilidad** de perder de
  golpe 15 puntos de durabilidad, además del desgaste normal (1 punto por
  golpe efectivo, igual que el resto de herramientas). Contra dureza ≤ 2 no
  hay ese riesgo extra.
- **A mano/pala** basta para azufre y para tierra/arena/arcilla (dureza 1);
  ningún pico es necesario en superficie hasta la caliza.

### 2.3 Material por material

`Golpes/m³` = golpes efectivos para vaciar un hueco de un metro cúbico con la
herramienta mínima; entre paréntesis, con la herramienta un nivel por encima.
`Botín/golpe` = unidades del ítem de `items.json` (todos ya existen en el
catálogo salvo donde se indica «nuevo»).

| Estrato | Dureza | Herramienta mínima | Golpes/m³ (con superior) | Botín/golpe | Notas |
|---|---|---|---|---|---|
| Tierra | 1 | Pala tosca | 3 (2 con pico) | `tierra_suelta` **(nuevo item, ver 2.7)** | Transportable en angarillas/cesta |
| Arena | 1 | Pala tosca | 3 (2 con pico) | `arena` | Se comporta como arena viva (§5) |
| Arcilla | 1 | Pala tosca | 3 (2 con pico) | `arcilla_roja` | Bolsas costeras y de manglar |
| Caliza | 2 | Pico de piedra | 5 (4 tallado, 3 obsidiana) | `caliza` | La Meseta |
| Basalto | 3 | Pico tallado | 7 (5 obsidiana, 5 rescatado) | `basalto` | Subsuelo general |
| Veta de cobre | 2 | Pico de piedra | 6 (5 tallado, 4 obsidiana) | `mineral_cobre` | Veta finita: 10 golpes por bolsa, reaparece en 20 días |
| Hierro de meteorito | 3 | Pico tallado | 8 (6 obsidiana, 6 rescatado) | `hierro_meteorito` | Veta finita: 4 golpes por cráter, no reaparece (recurso de exploración, no de granja) |
| Obsidiana (veta) | 4 | Pico de obsidiana | 9 (7 rescatado) | `obsidiana` | Isla del Humo, cerca del cráter |
| Azufre | 1 | A mano o pala | 4 a mano (2 con pala) | `azufre` | Fumarolas; a mano no hay riesgo de rotura |
| Cristal (caverna) | 4 | Pico de obsidiana o rescatado | 10 (7 rescatado) | `cristal_cuarzo` | Solo en carvings de §2.5 |

- Golpear un estrato con herramienta por debajo del mínimo no hace nada
  (regla 2.1); el juego lo comunica con el mismo «rebote» en todos los
  casos, sin mensaje de texto (coherente con «sin HUD de minería»).
- El botín se **carga directamente** (mismo `CarryComponent` que el resto
  del juego); si el jugador va sobrecargado, el mineral se acumula en el
  suelo como cualquier otro objeto soltado.

### 2.4 Peligros

| Peligro | Regla numérica | Aviso | Contramedida |
|---|---|---|---|
| **Derrumbe** | Un hueco sin apoyo con más de **3 m de luz** en cualquier dirección colapsa a los **8 segundos** de quedar expuesto | Crujido + polvo cayendo desde 2 s antes | Colocar una `viga_apoyo` **(pieza nueva, ver 2.7)** dentro de un radio de 1,5 m del centro del hueco |
| **Oscuridad** | Sin luz propia más allá de **6 m** de la boca de la galería (o de un tragaluz natural) | Visión reducida, igual que de noche en superficie | Antorcha (20 de duración, ya en catálogo) o lámpara de aceite |
| **Aire viciado** | Bolsas cerradas a más de **15 m** de la boca o de una chimenea: el aire baja **4 %/min** tras 2 minutos de gracia; por debajo del 20 % empieza el mareo (visión estrecha, como baja energía) | Respiración agitada, visión que se cierra por los bordes | Airear (picar hacia la superficie o una chimenea) o retirarse; nunca mata por sí solo, solo empuja a salir |
| **Inundación** | Galería que conecta con el mar o con el nivel freático (por debajo de la cota 0 del mapa) sin sellar: el agua sube **1 m cada 40 s** | Sonido de agua entrando, reflejo en el suelo | Sellar la brecha con una pieza `pared` del kit de construcción calzada contra la roca |
| **Crecida de monzón** | Una mina bajo el nivel del río se inunda al **30 %** durante una crecida (GDD §6.1 «Monzón», biblia §6.2) y evacúa en **2 días** tras acabar | Aviso previo idéntico al de la crecida en superficie (biblia §6.2) | Apuntalar y sellar antes de la estación, o evitar minar bajo el cauce en monzón |

### 2.5 Lugares increíbles generados

Carvings deterministas por semilla, variantes de tamaño y forma de
`FCaveDesc` (mismo mecanismo que las cuevas y arcos de superficie ya
existentes; no necesitan capa de ediciones propia, GDD §7.3 punto 4):

| Lugar | Isla | Contenido | Acceso |
|---|---|---|---|
| Cenotes de agua turquesa | La Meseta | Agua dulce potable sin hervir, peces de río | Boca vertical en el karst, cuerda o escalera picada (§4) |
| Tubos de lava | Isla del Humo | Vetas de obsidiana y azufre, cristal ocasional | Boca en superficie cerca del cráter |
| Cavernas de cristal | Cualquier isla, profundidad alta | `cristal_cuarzo` | Solo con pico de obsidiana o rescatado |
| Ríos subterráneos navegables | La Meseta | Balsa pequeña obligatoria; corriente que arrastra | Cenote o galería conectada |
| Ruinas antiguas enterradas | Bajo cualquier marae de superficie | Templos con petroglifos y tesoros de museo (§12) | Solo picando; nunca visibles desde fuera |
| Grutas marinas con bajamar | Los Dientes | Cámara que solo se abre con marea viva baja | Nadando, sin picar |
| Cavernas bioluminiscentes | Manglar, Meseta | Sin recurso de minería; valor de vista/fotografía | A pie o en balsa |

### 2.6 Feedback audiovisual

Sonido de impacto por estrato (tierra sorda, roca aguda, obsidiana
cristalina), partículas de polvo/esquirlas del color de `SurfaceLayers`
(ya existente), grieta visible en el último golpe antes de que ceda la
esfera (mismo lenguaje que la tala, 1.4). Aire viciado y derrumbe se leen en
el cuerpo del personaje, nunca en un HUD (regla dura de la biblia).

### 2.7 Casos límite y objetos nuevos requeridos

- **`tierra_suelta` (nuevo item):** falta un objeto de tierra transportable
  paralelo a `arena` (que ya existe). Propuesta: peso 0,35 kg, volumen 0,3 L,
  sin propiedades de combate, usado para rellenar (§3) y para bancales fuera
  de zona de playa.
- **`viga_apoyo` (nueva pieza de `building_pieces.json`, tier `madera`,
  socket `terreno`, uso exclusivo bajo tierra):** coste 2× `tronco_pequeno` +
  1× `cuerda`, `buildMinutes` 15, `integrity` 60. Es la misma lógica de
  «apoyo» que ya usa `FBuildingModel::RecomputeStability`, aplicada a huecos
  de mina en vez de a pisos de construcción.
- Picar bajo una construcción de superficie invalida su apoyo si la
  excavación le quita la roca de debajo: se comporta como una pieza sin
  soporte (mismo `CollapseUnsupported` que ya usa `FBuildingModel`).
- Picar el fondo del mar (por debajo del nivel freático) siempre cuenta como
  «conectado al mar» a efectos de inundación (2.4), sin excepción.

### 2.8 Persistencia

Nueva capa `"terrain"` en `FSaveWorldDeltas` (GDD §7.3 punto 2): deltas por
celda de chunk, mismo patrón de rangos/mapa de bits que `FSaveScatterDeltas`.
Los carvings deterministas de 2.5 no se guardan (se regeneran de la
semilla); solo se guarda lo que el jugador ha picado encima de ellos.

---

## 3. La pala y los caminos [AA]

- **Camino:** con la pala, mantener **E** sobre una franja de terreno
  (1,5 m de ancho, hasta 4 m de largo por uso) rebaja su pendiente a menos
  de **15°** y compacta la superficie (menos fricción de movimiento, −15 %
  de coste de resistencia al caminar sobre un camino terminado). Coste: 6
  golpes de pala tosca por tramo de 4 m; el material sobrante se recoge como
  `tierra_suelta`/`arena` según el estrato superficial.
- **Rellenar:** verter `tierra_suelta` o `arena` sobre un hueco lo tapa
  (suma densidad, GDD §7.3); un hueco de mina rellenado por completo borra
  su delta de la capa `"terrain"`, igual que se difumina un trazo de mapa
  mojado (GDD §3.2), y su trazo en la hoja subterránea se descarta.
- **Transportar tierra:** angarillas (item ya existente, 20 L de capacidad)
  o cesta/mochila; sin vehículo dedicado de tierra en el acceso anticipado
  (el vagón de vía, §9, es la mejora natural y llega en fase 2).

## 4. Escaleras picadas [AA]

- Se tallan con cualquier pico que cumpla el mínimo del estrato: cada
  escalón consume **2 golpes efectivos** (independiente del estrato,
  siempre que la herramienta lo permita) y mide **0,4 m de alto × 0,8 m de
  huella**, el mismo paso que la escalera de construcción de madera para que
  se sientan iguales al subir.
- **Pendiente máxima tallable: 45°.** Intentar tallar más empinado que eso
  se trata como un derrumbe del propio tramo (2.4): el escalón no se forma y
  cae material sobre el jugador (daño menor, empujón hacia atrás).
- No sueltan botín propio (el material tallado se pierde en polvo de
  escalón, a diferencia de un golpe de minería normal) — es una decisión
  deliberada para que tallar escaleras no sea la forma más barata de
  minar un estrato.
- Se guardan igual que cualquier edición de terreno (capa `"terrain"`,
  §2.8); una escalera picada se dibuja en la hoja subterránea del mapa
  igual que una galería (GDD §3.2).

## 5. Arena viva y agua [AA]

### 5.1 Ángulo de reposo

- **34° en seco, 45° en húmeda** (la franja intermareal y cualquier arena
  regada o bajo lluvia activa cuenta como húmeda).
- Revisión de pendiente local una vez por segundo por chunk activo: una
  celda de arena cuya pendiente supera el ángulo de reposo desliza hacia la
  celda vecina más baja (resta densidad arriba, suma abajo), hasta que todas
  las pendientes locales vuelven a estar por debajo del ángulo. Es el mismo
  mecanismo de edición de densidad que picar, aplicado automáticamente por
  el sistema en vez de por el jugador.
- **Nota de implementación [director, 2026-09-27]:** el encargo del mundo
  interactivo pide que la arena sea «un campo de alturas local de deltas: solo
  capa de superficie, más barato que lo volumétrico». Por eso la arena de playa
  no edita densidad: es una capa propia de deltas de altura (`FSandModel`,
  GDD §3.13) sobre la misma rejilla de 0,25 m y chunks de 8 m. Sale por la red
  por el mismo canal y el mismo formato de tramos que la densidad, con la capa
  marcada en la cabecera (biblia 08 §2.2). Picar tierra, roca o cualquier cosa
  que no sea arena de playa sigue siendo edición de densidad.

### 5.2 Relleno por oleaje

- Dentro de la franja intermareal (entre la línea de bajamar y la de
  pleamar del día, GDD/biblia §6.3), cada medio ciclo de marea (~6 h) la
  arena excavada por debajo de la línea de pleamar actual se rellena un
  **20 %** hacia su altura original.
- En marea viva (luna llena o nueva) ese relleno sube a **35 %** por medio
  ciclo — coherente con «mareas vivas» ya descritas en la biblia §6.1.
- Un agujero completamente por debajo de la línea de bajamar se rellena del
  todo en **2–3 ciclos de marea** sin intervención del jugador.
- **Cómo crece hacia el agua [director, 2026-09-27]:** el encargo del mundo
  interactivo pide que el oleaje rellene y alise «con una tasa que crece cuanto
  más cerca está del agua». El 20 % (35 % en marea viva) es la tasa **en la línea
  de pleamar**. De ahí sube en línea recta hasta el **60 %** en la línea de
  bajamar y por debajo (**75 %** en marea viva). La última onda remata lo que
  quede por debajo de 2 cm. Con el 20 % plano, el punto anterior no se cumpliría:
  a un hoyo le quedaría un 26 % tras 3 ciclos. Con la rampa, el hoyo más hondo
  posible (1,5 m) se cierra en 2,5 ciclos y uno de 30 cm, en 1,5.

### 5.3 Anclaje

- **Tablón de contención** (nueva pieza de construcción, tier `bambu`,
  socket `pared`, coste 3× `bambu_grueso` + 2× `cordel`, `buildMinutes` 15,
  `integrity` 40) y **pilote** (pieza ya existente, `pilote_bambu`/
  `pilote_madera`) ancoran la arena: cualquier celda dentro de **1 m** de una
  de estas piezas no desliza (5.1) ni se rellena por oleaje (5.2) mientras la
  pieza siga en pie. Es la base de todo muelle, terraplén o playa artificial
  estable.

### 5.4 Mareas, lluvia y ríos

- **Mareas:** sin cambios respecto a la biblia §6.3 (dos pleamares/dos
  bajamares al día, amplitud por fase lunar); la minería añade que una
  galería que cruza la cota de marea se inunda y vacía con el ciclo, igual
  que cualquier poza de marea de superficie.
- **Lluvia:** cualquier recipiente abierto se llena con la lluvia activa
  (ya documentado, biblia §11); en minería, la lluvia intensa sostenida
  sube el nivel freático local y acelera la inundación (2.4) en un **50 %**
  durante la lluvia y las 2 horas siguientes.
- **Ríos:** su caudal sigue la estación (biblia §6.1: bajos en la seca,
  crecidos en el monzón); un río que cruza una ladera minada puede
  desviarse hacia una galería abierta si esta queda más baja que el cauce —
  mismo caso que «inundación» (2.4), sin mecanismo aparte.

## 6. El fuego y su propagación [AA]

Ya implementado en su mayor parte (`Cooking/ExploredFire`, `fuels.json`,
biblia §5.3/§11); aquí se fija el número que faltaba por definir.

- **Contagio entre celdas de vegetación:** mientras una celda de hierba/
  matorral está en llamas, cada celda vecina no mojada tiene una
  probabilidad de prender de **45 % por segundo** en la estación seca; el
  viento del momento (`FWeatherModel`) suma **+25 %** a las celdas situadas
  a favor del viento y resta ese mismo 25 % a las que están a barlovento.
- En `primeras_lluvias`, `monzon` y `ciclones` esa probabilidad base baja a
  **13,5 %** (−70 %, todo está húmedo); en niebla matinal, a **9 %**.
- Un incendio de ladera se apaga solo cuando no quedan celdas inflamables
  contiguas sin quemar o cuando empieza a llover (chubasco o más).
- **Rebrote de zona quemada:** hierba a los **12 días**, arbustos a los
  **25 días** (mismo número que el rebrote de tala de arbustos, 1.2, para
  que ambos sistemas compartan un solo temporizador de vegetación).
- Ceniza recogible tras el incendio (`ceniza_madera`, ya en catálogo) en la
  zona quemada durante los primeros 3 días.

## 7. Construcción libre [AA]

Sistema ya implementado (`Building/BuildingModel.{h,cpp}`,
`building_pieces.json`); se documenta aquí para que las piezas nuevas de
minería (`viga_apoyo`), arena (`tablon_contencion`), barcos (§8), raíles
(§9) y murallas (GDD §3.8, F2) encajen en el mismo lenguaje sin inventar
reglas paralelas.

### 7.1 Piezas y snapping

- Rejilla de encaje de **2 m** (`docs/art/kit-construccion.md`); cada pieza
  declara un `socket` (pilar, suelo, pared, puerta, techo, escalera, mueble,
  terreno) que determina dónde puede encajar, con vista previa fantasma
  (verde = válido, rojo = sin apoyo o solapado) y resaltado del punto de
  apoyo que falta.
- Controles: **B** construir, **R** rotar (incrementos de 90°, o 15° con
  modificador fino para piezas de terreno libre como caminos y muelles),
  clic confirma, **X**/**Z** cambian de pieza dentro de la misma categoría.

### 7.2 Estabilidad estructural

- Cada pieza tiene `integrity` (1–100, tabla ya en `building_pieces.json`).
  `FBuildingModel::RecomputeStability` (ya existente) propaga apoyo desde el
  terreno o desde una pieza ya estable; una pieza por debajo del umbral
  `WellSupportedStability` tolera una categoría de ciclón menos
  (`maxCycloneCategory − 1`) antes de dañarse.
- Retirar una pieza que sostiene a otras dispara `CollapseUnsupported`
  (ya existente): las piezas huérfanas caen y se convierten en escombro
  recolectable al 50 % de su coste original.

### 7.3 Materiales y daño

- Cuatro tiers (`tiers` de `building_pieces.json`): hoja → bambú → madera →
  piedra, cada uno con su herramienta mínima (`requiresTools`).
- **Degradación:** `Def->Integrity * DecayPerDay * RainFactor` por hora
  (fórmula ya en `BuildingModel.cpp`); lo orgánico sin techo se pudre más
  rápido con la lluvia (`RainDecayMultiplier`). Reparar es la misma acción
  que construir (biblia §2.4): reponer el material gastado en la pieza
  dañada.
- **Reutilización explícita para el resto de mecánicas nuevas:** raíles
  (§9), muralla y torres (GDD §3.8, F2) y las piezas de casco de barco (§8)
  son plantillas de pieza dentro de este mismo `FBuildingModel`, no
  sistemas de integridad aparte.

## 8. Barcos por piezas [AA]

Pivote respecto al `boats.json` actual (una plantilla por tipo de barco
completo): las cuatro embarcaciones canónicas — **balsa**, **canoa**,
**canoa con balancín**, **barco «Limón»** — pasan a ser **planos**
(conjuntos de piezas de casco predefinidos) sobre un catálogo de piezas
libre; el jugador puede seguir un plano tal cual o combinar piezas por su
cuenta. `FBoatModel` (ya con `TotalMassKg`, `EquilibriumDraftCm`,
`SwampWaterKg`, `MaxAbsRollDeg`, `ApplyDamage`) deja de derivar sus números
de una tabla fija por `EBoatType` y pasa a calcularlos a partir de la suma
de piezas ancladas al casco — el mismo pivote de «tabla fija → suma de
piezas» que la construcción de tierra ya usa.

### 8.1 Piezas de casco (nuevas, tier `madera`/`bambu`/`piedra` según planos)

| Pieza | Función | Aporta |
|---|---|---|
| Quilla | Columna del casco, primera pieza obligatoria | `IntegridadBase`, ancla el resto |
| Cuaderna | Costilla que da forma y apoyo a los tablones | Apoyo estructural (como un pilar) |
| Tablón de casco | Piel exterior, hueco = flotabilidad | `VolumenFlotacionLitros`, `MasaKg` |
| Cubierta | Suelo interior, carga y tripulación | Espacio de carga (kg), protege de agua embarcada |
| Mástil | Soporta vela | Habilita `SetSailRaised` |
| Vela | Empuje del viento | Superficie vélica (m²), ya modelada en `FBoatWind` |
| Balancín/flotador | Estabilidad lateral | Reduce escora máxima (ver 8.3) |
| Timón | Gobierno | Habilita cambio de rumbo fino (ya existe en controles) |
| Banco de remo | Propulsión manual | Habilita `TryStroke` por esa banda |
| Amarre/noray | Sujeción | Ver 8.4 |

### 8.2 Hidrostática

- **Flotabilidad máxima (kg):** Σ `VolumenFlotacionLitros` de los tablones ×
  **1,025** (densidad del agua de mar; agua dulce en ríos y cenotes, 1,0).
- **Calado de equilibrio:** el casco se hunde hasta que el volumen sumergido
  desplaza un peso de agua igual a `TotalMassKg` (casco + tripulación +
  carga + agua embarcada, fórmula ya expuesta por
  `FBoatModel::EquilibriumDraftCm`); el calado de diseño de un plano
  canónico es el **60 %** de la altura de sus cuadernas.
- **Anegarse:** si `TotalMassKg` supera el **95 %** de la flotabilidad
  máxima, el casco empieza a embarcar agua (`SwampWaterKg` sube 2 kg/s
  mientras dure el exceso); por encima del **115 %** se hunde.
- **Escora y vuelco:** cada pieza fuera del eje longitudinal (vela izada,
  carga mal repartida, tripulación a un lado) genera momento de escora.
  Vuelca si la escora supera **55°** durante más de **4 segundos** sin
  corregir, o de inmediato si supera **75°** (pico de ola, golpe de viento
  racheado). Un balancín instalado reduce el momento de escora un **60 %**
  en el lado del flotador — es la razón mecánica, no solo estética, de que
  la canoa con balancín sea el primer barco seguro en mar abierto.

### 8.3 Integridad por uniones y daño

- Cada unión cuaderna–tablón tiene su propia `integrity` (1–100, mismo
  campo que cualquier pieza de construcción). Un choque contra roca o
  arrecife por encima de `SafeImpactSpeedCmS` (80 cm/s, ya definido en
  `BoatModel.h`) daña las uniones más cercanas al punto de impacto
  proporcionalmente a la velocidad de sobra.
- Una unión a 0 abre una vía de agua: entra agua a razón de **0,5 L/s** por
  brecha mientras no se repare; varias brechas simultáneas se suman y
  pueden hundir el barco sin necesidad de superar el 115 % de golpe (8.2).
- **Reparar** es la misma acción que construir: reemplazar el tablón o la
  cuaderna dañada con material nuevo, en el agua o en dique seco.

### 8.4 Astillero, botadura y amarre

- El astillero (pieza ya existente en `building_pieces.json`) sigue siendo
  el lugar donde se colocan las piezas de casco; fuera de él no se puede
  empezar un casco nuevo, pero sí repararlo donde esté.
- **Construido en tierra:** se bota sobre rodillos (troncos, reutiliza
  `tronco_pequeno`); empujarlo hasta el agua cuesta un canal de esfuerzo de
  **8 segundos por tonelada** de `TotalMassKg` (empuje continuo, se puede
  interrumpir y retomar).
- **Construido en el agua** (astillero sobre pilotes junto a la costa):
  el casco **deriva** a la velocidad de la corriente local
  (`FOceanCurrents`, ya existente) en cuanto se suelta, salvo que esté
  amarrado con la pieza «amarre/noray» a un muelle o a un pilote en tierra
  (propiedad `Ata`, mismo verbo que atar cualquier otra cosa).
- Los planos canónicos (balsa, canoa, canoa con balancín, «Limón») siguen
  el coste y los `buildMinutes` ya fijados en `boats.json`; ese fichero gana
  un listado de **qué piezas** componen cada plano, sin cambiar los números
  de progresión ya balanceados.

---

## 9. Raíles y vagones [F2]

Fuera del acceso anticipado (justificación técnica ya en GDD §3.5: grafo de
vía + física de vagón sobre terreno editable, sin prototipo verificado).
Se fija aquí el número que faltaba para cuando entre en producción:

- **Vía:** pieza de construcción (tier `madera`, socket nuevo `via`, tramos
  de 2 m como el resto de la rejilla), coste 2× `tronco_pequeno` + 1×
  `cuerda` por tramo, `buildMinutes` 10.
- **Vagón:** capacidad de carga 200 kg (4× lo que carga un jugador a
  cuestas), empujado a mano a 1,2 m/s o tirado por torno de cuerda a 2 m/s
  en tramos con pendiente descendente ≤ 5°; en pendiente mayor, el torno es
  obligatorio (sin motores, coherente con «nunca hay combustión interna»).
- **Ascensor de pozo:** torno vertical, mismo principio que el torno
  horizontal, para minas de más de 25 m de profundidad.
- El vagón sigue una spline con colisión contra el terreno editado; si una
  mina se reedita bajo una vía existente, el tramo afectado se marca
  «dañado» (no navegable) hasta repararlo, igual que una pieza de
  construcción dañada por un derrumbe.

## 10. Granja: cultivos y animales

### 10.1 Cultivos [AA]

Sin cambios de fondo respecto a `plants.json` (ya implementado): cinco
cultivos (limonero, platanera, taro, batata, piña, maracuyá) con etapas
estáticas por días, estación válida y riego.

- **Riego:** `waterPerDay` ya fijado por planta (0–2 riegos/día); regar es
  la acción **Llenar/verter** (biblia §2.2) desde cualquier recipiente con
  agua sobre el bancal. Sin riego los días que lo requieren, la planta deja
  de avanzar de etapa (no muere) hasta que se riega — coherente con «sin
  microgestión» (biblia §8.3): no hay penalización punitiva, solo pausa.
- **Estaciones:** una planta fuera de su ventana de `seasons` no se puede
  plantar; una ya plantada que entra en una estación no válida (p. ej. la
  piña al llegar el monzón) deja de fructificar hasta que vuelve su
  estación, pero no se pudre en la mata.
- **Compost y espantapájaros:** ya documentados en la biblia §7.2; el
  espantapájaros reduce a 0 el picoteo de las especies con `birdsEat: true`
  en un radio de 4 m.

### 10.2 Animales domésticos [F2]

- **Estructuras:** gallinero, pocilga, corral — piezas de construcción con
  una especie doméstica asociada (gallina, cerdo, cabra), reutilizando
  Quaternius CC0 con rig (GDD §3.6, pivote razonado frente al GDD v3).
- **Adquisición:** capturar una pareja salvaje (gallina/cerdo/cabra ya
  presentes como fauna salvaje, §11) con una trampa simple, o conseguirla
  por trueque con el pueblo del arrecife (F3) con reputación alta.
- **Alimentar y recoger:** comida en el comedero del corral (cualquier
  fruta o tubérculo básico), 1 vez/día; huevos cada **1 día** por gallina
  adulta, leche de cabra cada **1 día** si hay cría reciente.
- **Reproducción:** un macho y una hembra adultos en el mismo corral con
  comida llena tienen un **15 % de probabilidad diaria** de producir una
  cría; la cría tarda **6 días** en ser adulta.
- **Tope:** máximo **8 animales domésticos vivos por base** (mitigación de
  rendimiento, GDD §3.6), leído en el propio corral (lleno se ve lleno, sin
  contador).

## 11. Animales salvajes e IA [AA salvaje / F2 doméstico]

### 11.1 Nivel de detalle (LOD)

Ya implementado para fauna marina (`FFaunaLod`, `Fauna/FaunaSpawning.h`);
la fauna terrestre nueva reutiliza los mismos tres niveles y los mismos
radios por defecto:

| Nivel | Radio | Comportamiento |
|---|---|---|
| **Full** | 40 m (`FullRadiusCm = 4000`) | Se actualiza cada fotograma: rutina, percepción, huida/carga |
| **Reduced** | 150 m (`ReducedRadiusCm = 15000`) | Se actualiza 1 de cada 4 fotogramas (`ReducedInterval = 4`), con el tiempo acumulado aplicado de golpe |
| **Frozen** | Más allá de 150 m o fuera de vista | Sin actualizar; posición y estado se congelan |

Histéresis del **10 %** entre niveles para no oscilar en el borde (ya
implementado, `FFaunaLod::Tier`). La fauna terrestre añade navegación sobre
el terreno editable (NavMesh o campo propio); cada remallado de minería que
toque una zona con fauna cerca invalida y reconstruye solo el campo de
navegación de ese chunk, no el del archipiélago entero (GDD §7.4).

### 11.2 Fauna salvaje terrestre (nueva)

- Especies iniciales: cerdo salvaje, cabra montés (pacíficas, huyen al
  detectar al jugador a menos de 12 m); aves que ahora sí se posan y anidan
  en tierra (se descarta la restricción «siempre en vuelo» del GDD v3 para
  estas especies nuevas).
- Percepción: vista (cono de 110°, 15 m), oído (radio 8 m, +50 % si el
  jugador corre), olfato (a favor del viento, 20 m).
- Rutina diaria: pastar (06:00–10:00 y 17:00–19:00), beber junto a agua
  dulce al mediodía, dormir/reposar el resto.

### 11.3 Caza

- Con lanza o arco (ya en catálogo); un impacto en el cuerpo reduce la
  salud del animal, uno en la cabeza es muerte instantánea si el arma tiene
  `Punta ≥ 3`.
- Despiece con cuchillo: carne (comida), piel en bruto, hueso, grasa —
  mismo patrón que el despiece de pescado ya implementado (biblia §4.4).
- Cazar cerca de un territorio del pueblo del arrecife (F3) baja reputación
  (GDD §3.9); no hay restricción en islas sin pueblo activo.

### 11.4 Pesca

Sin cambios respecto al sistema ya implementado (`FishingModel.cpp`,
biblia §4): leer las aguas, acercarse, capturar con arpón/caña/trampa,
despiezar y conservar. La minería no lo modifica.

### 11.5 Doma [F2]

- Domesticar un animal salvaje capturado (trampa simple, mismo verbo que
  atrapar cangrejos) lo convierte en candidato de corral; no hay minijuego
  de doma aparte, el animal simplemente empieza dócil si se le alimenta a
  diario los primeros **3 días** en el corral.
- Un animal doméstico sin alimentar 2 días seguidos vuelve a comportarse
  como salvaje (huye si se abre el corral) pero no muere ni desaparece.

## 12. Tesoros enterrados y cuevas o subterráneos [AA]

- **Fuente nueva de tesoros:** los templos enterrados de §2.5 amplían el
  catálogo de la biblia §10 con el mismo tipo de artefactos (anzuelos
  tallados, adornos de concha, cartas de navegación de varillas y conchas,
  tapa, remos ceremoniales) y las mismas técnicas de wayfinding que las
  ruinas de superficie (`ruins.json`, `RuinsModel`) — **no es un catálogo
  aparte**, es la misma tabla con una procedencia «subterránea» en vez de
  «superficie» a efectos del catálogo del mapa.
- **Cuevas rituales bajo el nivel del mar** (biblia §9.1) y **ruinas
  sumergidas visibles solo con marea viva extrema** son casos particulares
  de los carvings de 2.5, no mecanismos nuevos.
- Un tesoro enterrado nunca se puede destruir picando alrededor: el carving
  que lo contiene tiene un radio de exclusión de 1,5 m donde el terreno no
  se edita, para que no desaparezca por accidente de un golpe de pico mal
  dirigido.
- Persistencia: el propio tesoro se guarda como cualquier objeto de museo
  ya recolectado (`MuseumModel`); el carving que lo alojaba, como cualquier
  otro carving de 2.5 (no se guarda, se regenera de la semilla, solo se
  guarda si el jugador lo ha vaciado o alterado).

---

## 13. Escalada [AA]

Decisión del director de 2026-09-27: hoy no existe ni en el código ni en la
biblia — verticalidad nueva sobre los mismos sistemas ya descritos (Energía,
§6.3 de 01-núcleo-y-estados; caída y esguince, §6.13 de 01-núcleo-y-estados),
no un sistema de movimiento aparte.

### 13.1 Trepar palmeras

- Verbo contextual **E** mantenido sobre el tronco de una palmera abrazándolo
  (misma interacción que talar, distinta según lo que se mira, biblia §8.3
  «máximo tres verbos»).
- **Sin herramienta:** sube a **0,7 m/s**, coste de Energía **−9 × peso**
  puntos/segundo real (entre `Nadar` −5×peso y `Esprintar` −11×peso de la
  tabla de 01 §6.3: trepar a pulso cansa casi tanto como esprintar).
- **Con cuerda de fibra o pie de palmera** (anilla de cuerda que se pasa
  alrededor del tronco y de los tobillos, **NUEVO**, `pie_de_palmera`:
  `cuerda` ×1, `Ata` heredada de la propia cuerda): sube a **1,3 m/s**, coste
  de Energía **−6 × peso**/s — el mismo ahorro relativo que una herramienta
  buena da en cualquier otra actividad de la biblia.
- **Arriba:** recolección instantánea de `coco_verde` (agua, §3.5 de la
  biblia 03) directamente de la copa, sin golpe ni herramienta — es la vía
  de agua de emergencia que talar no da (talar solo suelta `coco_maduro` ya
  caído, §1.2).
- Quedarse sin Energía a mitad de la trepada hace caer: mismo sistema de
  caída y esguince que cualquier otra caída del juego (01 §6.13), con la
  altura ya recorrida como altura de caída.

### 13.2 Escalar roca por salientes

- Solo en paredes de **más de 60° de pendiente**; sin herramienta, el tramo
  escalable de una sola vez está limitado a **3 m** de altura — pasado ese
  límite no hay saliente de sobra para seguir sin clavijas (§13.4).
- Mismo coste de Energía que trepar sin herramienta (−9×peso/s, §13.1);
  agotar la Energía en la pared hace soltarse y caer — la altura ya subida
  cuenta como altura de caída a efectos de esguince y daño (01 §6.13): 3 m
  es justo el umbral de esguince en tierra (`SprainFallHeight = 4,5 m` en
  01 §6.13 no se alcanza soltándose desde el límite de 3 m sin ayuda, pero sí
  si se combina con clavijas para subir más alto, §13.4).
- Sin indicador de agarre en pantalla: la propia pendiente del terreno
  (`FTerrainDensity`, GDD §7.3) decide si el tramo es escalable, leído por el
  propio personaje (se para y no sube más si no hay saliente, sin mensaje).

### 13.3 Escaleras de mano y cuerdas fijas — piezas de construcción

Dos piezas nuevas del mismo kit de construcción (§7, sin sistema aparte):

| Pieza | Socket | Coste | `buildMinutes` | `integrity` | Qué hace |
|---|---|---|---|---|---|
| `escalera_mano` — NUEVO | escalera | `palo_recto` ×6, `cuerda` ×3 | 12 | 40 | Sube a velocidad de escalera normal (biblia §7.1), sin coste de Energía extra: es una pieza construida, no una trepada |
| `cuerda_fija` — NUEVO | terreno | `cuerda` ×2 por tramo de 4 m | 5 | 25 (se corta si un pirata la daña, F3) | Ancla un punto de descenso/ascenso rápido en una pared ya escalada una vez; sube/baja a 1,3 m/s (igual que con pie de palmera) sin volver a gastar Energía de escalada |

Ambas se colocan con la misma vista previa fantasma del resto del kit (§7.1):
verde si hay apoyo en los dos extremos, rojo si no.

### 13.4 Clavijas en roca — pico [H2]

- Con el pico equipado como herramienta de golpeo (cualquier tier, GDD
  §3.4/§2 de esta sección) se clava una **clavija** (`clavija_roca` —
  NUEVO, `hueso_largo` o `lingote_hierro` tallado, `Punta ≥ 2`, plantilla de
  un solo rol sin mango) en una pared de más de 60°: consume una clavija del
  inventario y un golpe de resistencia, no desgasta el pico. Cada clavija
  ancla un tramo nuevo de hasta 3 m antes de necesitar la siguiente.
- Llega con la propia minería (H2, §2 de esta sección): no es un desbloqueo
  aparte, es el pico haciendo un segundo trabajo, coherente con «máximo tres
  verbos» (biblia §8.3) y con que el juego no añade una herramienta nueva
  solo para escalar.
- Las clavijas se quedan clavadas (persistencia igual que una edición de
  terreno menor, capa `"terrain"`, GDD §7.3): una pared ya clavada una vez
  se vuelve a subir sin gastar clavijas nuevas.

### 13.5 Feedback y cámara

- **Animación:** trepa en primera persona con manos visibles agarrando cada
  saliente/tronco y un bamboleo sencillo de cámara al ritmo del movimiento
  (mismo lenguaje que nadar, sin cámara en tercera persona ni QTE).
- Sin HUD de aguante de escalada: se lee en el mismo temblor/respiración del
  cuerpo que el resto de esfuerzo físico (01 §6.0).

---

## Textos de feedback (ES/EN)

Frases cortas, tono de superviviente seco, sin exclamaciones grandilocuentes.

| Situación | Español | Inglés |
|---|---|---|
| Derrumbe inminente | «Esto no aguanta.» | "This isn't holding." |
| Aire viciado | «Me falta el aire aquí abajo.» | "Air's getting thin down here." |
| Inundación de galería | «Está entrando agua.» | "Water's coming in." |
| Pico insuficiente | «No corta esta piedra.» | "Won't cut this rock." |
| Pendiente de escalera excesiva | «Muy empinado para tallar aquí.» | "Too steep to carve here." |
| Barco anegándose | «Estamos embarcando agua.» | "We're taking on water." |
| Barco a punto de volcar | «Vamos a volcar.» | "We're going over." |
| Cerca de territorio del pueblo | «Esto no me parece buena idea.» | "Doesn't feel right doing this here." |
| Talar cerca de ruinas | «Mejor no toco esto.» | "Better not touch this." |
| Sin fuerzas para seguir trepando | «No llego más arriba así.» | "Not making it up there like this." |
| Pared sin saliente para clavija | «Aquí no hay donde clavar nada.» | "Nothing to drive a piton into here." |

---

## TODO de implementación

### Minería y terreno

- [x] [AA] `WorldGen`: añadir `FTerrainEdits` (capa de ediciones dispersa por chunk) según GDD §7.3 punto 1.
- [x] [AA] `WorldGen`: `FTerrainDensity::Density` consulta primero la capa de ediciones antes de evaluar el ruido.
- [ ] [AA] `WorldGen/TerrainChunkBuilder`: invalidar y reconstruir solo los chunks tocados por una edición.
- [x] [AA] `WorldGen`: implementar picado por esfera con radio y tiempo por golpe de la tabla 2.3, por estrato y herramienta.
- [ ] [AA] `Items`/`Crafting`: añadir item `pico` y plantilla `pico` a `items.json`/`templates.json` (sección 2.2), con las cuatro combinaciones canónicas balanceadas por `Rigido`/`Contundente`/`Filo` de la cabeza.
- [ ] [AA] `Crafting`: regla de rotura extra del pico de obsidiana contra dureza ≥ 3 (8 % por golpe, −15 durabilidad).
- [ ] [AA] `Survival`/`WorldGen`: indicador de aire viciado (bolsas cerradas a > 15 m de una salida), sin HUD, leído en el cuerpo.
- [ ] [AA] `Building`: pieza `viga_apoyo` (apuntalamiento de galería) y regla de derrumbe (hueco > 3 m de luz sin apoyo, 8 s).
- [ ] [AA] `WorldGen`/`Ocean`: inundación de galería conectada al mar o al nivel freático (1 m/40 s sin sellar).
- [ ] [AA] `Cartography`: hoja subterránea por sistema de galerías (GDD §3.2), generada bajo demanda al entrar la primera vez.
- [x] [AA] `Save`: nueva capa `"terrain"` en `FSaveWorldDeltas` (deltas de edición por chunk).
- [ ] [AA] `WorldGen`: carvings grandes (cenotes, tubos de lava, cavernas de cristal, ríos subterráneos, templos enterrados, grutas de marea) como `FCaveDesc` de mayor tamaño, con radio de exclusión de 1,5 m alrededor de un tesoro.
- [x] [AA] Prueba de estrés de guardado de minería extensa antes de M3 (GDD §7.4).

### Tala y recolección

- [ ] [AA] `WorldGen/HarvestModel`: tabla de especies talables (1.2) con golpes, tiempo, botín y altura por especie.
- [ ] [AA] `WorldGen`: dirección de caída (golpe + viento) y colisión contra construcción ligera/terreno.
- [ ] [AA] `WorldGen/VegetationHarvestState`: estado `Stump` con día de rebrote (18/24/4 días según especie).
- [ ] [AA] `WorldGen`: generación periódica de `rama_seca` bajo cada árbol (2–4 cada 6 h, tope 6).

### Pala, caminos y arena

- [ ] [AA] `Items`: nuevo item `tierra_suelta` en `items.json`.
- [ ] [AA] `WorldGen`: modo «camino» de la pala (aplanar franja, −15 % coste de movimiento sobre camino terminado).
- [ ] [AA] `WorldGen`: simulación de ángulo de reposo de arena (34°/45°, revisión de pendiente 1/s por chunk activo).
- [ ] [AA] `WorldGen`/`Ocean`: relleno de arena excavada por oleaje en franja intermareal (20 %/35 % por medio ciclo de marea).
- [ ] [AA] `Building`: pieza `tablon_contencion` (ancla arena y detiene el deslizamiento/relleno en 1 m).

### Fuego

- [ ] [AA] `Weather`/`WorldGen`: contagio de fuego entre celdas de vegetación (45 %/s seco, −70 % en estaciones húmedas, ±25 % por viento).
- [ ] [AA] `WorldGen`: rebrote de zona quemada (12 días hierba, 25 días arbustos), compartiendo temporizador con el rebrote de tala.

### Construcción y barcos

- [ ] [AA] `Boats`: piezas de casco (quilla, cuaderna, tablón, cubierta, mástil, vela, balancín, timón, banco de remo, amarre) en un nuevo `Content/Data/boat_pieces.json`.
- [ ] [AA] `Boats/BoatModel`: sustituir la tabla fija por `EBoatType` por el cálculo de `TotalMassKg`/`EquilibriumDraftCm`/`SwampWaterKg`/escora a partir de las piezas ancladas.
- [ ] [AA] `Boats`: integridad por unión cuaderna–tablón, daño por impacto sobre `SafeImpactSpeedCmS`, vía de agua por brecha (0,5 L/s).
- [ ] [AA] `Boats`: botadura sobre rodillos en tierra (8 s/tonelada) y deriva por corriente si no está amarrado en el agua.
- [ ] [AA] `Boats`: actualizar `boats.json` para que los cuatro planos canónicos listen piezas en vez de un coste plano.

### Granja y fauna

- [ ] [AA] Confirmar que `FarmModel`/`plants.json` ya cubren riego y ventana de estaciones tal como se documenta en 10.1 (sin cambios esperados).
- [ ] [F2] `Fauna`: extensión a especies terrestres domésticas (gallina, cerdo, cabra) con packs CC0 riggeados (Quaternius).
- [ ] [F2] `Building`: piezas de corral (gallinero, pocilga, corral genérico) con tope de 8 animales vivos por base.
- [ ] [F2] `Fauna`: reproducción por pares (15 %/día), cría a adulto en 6 días, regla de vuelta a salvaje tras 2 días sin comida.
- [ ] [AA] `Fauna`: primera pasada de fauna salvaje terrestre (cerdo, cabra, aves que se posan) con LOD (`FFaunaLod` ya existente) y navegación invalidada por chunk minado.

### Raíles [F2]

- [ ] [F2] Nuevo módulo `Tramway`: pieza de vía (socket `via`), grafo de tramos, vagón sobre spline con colisión contra terreno editable.
- [ ] [F2] `Building`: torno horizontal y ascensor de pozo como piezas de producción.
- [ ] [F2] Prototipo de PIE antes de comprometer alcance (coste real no verificado, GDD §3.5).

### Escalada (§13)

- [ ] [AA] `Player`: verbo contextual «Trepar» sobre `Palm` (`HarvestModel.cpp`), subida a 0,7 m/s sin herramienta / 1,3 m/s con `pie_de_palmera`, coste de Energía −9×peso / −6×peso por segundo (§13.1).
- [ ] [AA] `Items`/`Templates`: nuevo item `pie_de_palmera` (`cuerda` ×1) en `items.json`.
- [ ] [AA] `Player`/`WorldGen`: escalada de roca sobre pendiente > 60°, tope de 3 m sin herramienta, mismo coste de Energía que trepar sin ayuda; caída y esguince al agotar Energía reutilizando el sistema de 01 §6.13 (§13.2).
- [ ] [AA] `Building`: piezas `escalera_mano` y `cuerda_fija` en `building_pieces.json` (coste y `integrity` de la tabla de §13.3).
- [ ] [H2] `Items`/`Crafting`: nuevo item `clavija_roca` (Punta≥2, sin mango) y verbo de colocación con el pico equipado como herramienta de golpeo, ampliando el tramo escalable de roca más allá de 3 m (§13.4).
- [ ] [AA] `Player`: animación de trepa en primera persona con manos visibles y bamboleo de cámara simple, con la misma opción «Reducir movimiento» que el resto de animaciones de cámara (biblia 06 §3.4).
- [ ] [AA] `Items`/`recipes.json`: confirmar que `coco_verde` se puede recoger directamente de la copa de una palmera trepada (sin golpe ni herramienta), distinto del `coco_maduro` que suelta la tala (§13.1, §1.2).
