# EXPLORED — Biblia de diseño · 03. Crafteo, inventario y objetos

Versión 1 · 2026-09-27 · Complementa `docs/diseno/gdd_v2.md` (manda) y
`docs/design/biblia-de-contenido.md` (catálogo y sistema de combinación, sin
cambios). Este documento cierra los tres huecos que esos dos dejan abiertos:
el inventario completo (manos, mochila, contenedores, transporte, muerte), las
estaciones de crafteo con sus recetas exactas, y una tabla maestra de objetos
con id, nombre, descripción, origen, uso, durabilidad, peso y fase.

**Fuentes leídas:** `Content/Data/items.json` (488 líneas, ~200 definiciones),
`Content/Data/templates.json` (13 plantillas de combinación en mano),
`Content/Data/verbs.json` (8 verbos implementados), `Content/Data/recipes.json`
(cocina y conservación), `Content/Data/building_pieces.json` (44 piezas),
`Content/Data/boats.json`, `Content/Data/fuels.json`, `Content/Data/plants.json`,
`Content/Data/artifacts.json`, `Content/Data/survival_needs.json`,
`Source/Explored/Carry/{CarryComponent.h,cpp,InventoryModel.h,cpp}`,
`Source/Explored/ExploredGameMode.cpp` (`HandlePlayerDeath`).

Fases: **[AA]** acceso anticipado (GDD §6.2), **[F2]** raíles/animales
domésticos/murallas/resto de islas, **[F3]** pueblo del arrecife, piratas,
isla oculta. Un objeto marcado **NUEVO** no existe hoy en `Content/Data/*.json`
ni en `Source/Explored`; el resto reutiliza el id real tal cual está en disco.

---

## 1. El inventario

### 1.1 Dos manos y mochila

El cuerpo del jugador es el inventario. No hay una rejilla abstracta: cada
hueco es un sitio físico, y `UCarryComponent`/`FInventoryModel`
(`Source/Explored/Carry/`) ya implementan exactamente estos huecos:

| Hueco | Cantidad | Qué acepta | Constante real |
|---|---|---|---|
| **Manos** | 2 (`HandLeft`, `HandRight`) | Cualquier cosa; un objeto `DosManos` (tronco, atún, angarillas) ocupa las dos a la vez y `IsHoldingTwoHanded()` lo bloquea como una sola pieza | `EInventorySlot::HandLeft/HandRight` |
| **Bolsillos** | 4 huecos | Solo tamaño `Pequeno` (`Carry_PocketTooSmall`) | `FInventoryModel::PocketSlots = 4` |
| **Cinturón** | 3 enganches (5 con cinturón de cuero equipado, `cinturon_cuero`) | Solo herramientas o recipientes (`Carry_BeltNotTool`) | `FInventoryModel::BaseBeltHooks = 3` |
| **Bolsa estanca** | 2 huecos | Lo que no debe mojarse (mapa, cerillas, botiquín): bolsillo impermeable de la mochila o bolsa colgada del cinturón | `FInventoryModel::PouchSlots = 2` |
| **Mochila** | Variable según la equipada | Todo lo que quepa en volumen y peso | `SetCustomBackpack(Volumen, PesoKg)` |
| **Angarillas** | Arrastradas, no en la espalda | Solo etiqueta `madera` o `piedra` (`Carry_SledgeOnlyMaterial`) | `ECarrySlot::Sledge` |

**Mochilas (`Content/Data/items.json`, reales):**

| id | Volumen | Peso cómodo extra | Peso del objeto | Durabilidad |
|---|---|---|---|---|
| `mochila` (del Albatros, inicial) | 20 L | +8 kg | 0,8 kg | — (rescatada, no se rompe) |
| `mochila_fibra` | 25 L | +10 kg | 0,9 kg | 80 |
| `mochila_cuero_bambu` | 35 L | +20 kg | 1,6 kg | 150 |

*Decisión: `items.json` fija peso/volumen del objeto y `Recipiente`, pero no
la capacidad de carga cómoda adicional (`BackpackComfortBonusKg` vive en C++
sin exponer). Se fijan aquí los tres valores de arriba porque son los que
`UCarryComponent::SetCustomBackpack` necesita recibir; quien implemente lee
esta tabla, no la inventa de nuevo.*

Ponerse la mochila (`EquipFromHand`) exige tenerla en una mano; si ya llevabas
otra, el contenido pasa entero a la nueva (si no cabe, no se puede cambiar) y
la vieja queda en la mano. Quitársela (`UnequipBackpack`) exige que esté
vacía: no se puede quitar de golpe con la carga dentro, para que dejarla en el
suelo sea una decisión, no un descuido.

### 1.2 Peso y volumen

- **Capacidad cómoda sin equipo:** 15 kg (`BaseComfortableKg`). Con mochila de
  cuero con armazón de bambú: 35 kg cómodos.
- **Límite duro:** 2× la capacidad cómoda (`MaxLoadRatio = 2.0`); por encima,
  `EInventoryFail::OverCarryLimit` y no se puede coger más, punto.
- **`GetCarriedWeightRatio()`** = (peso del cuerpo + peso de las angarillas ×
  0,3) / capacidad cómoda. Las angarillas se arrastran, no se cargan encima:
  solo cuentan al 30 % de su peso real para la fatiga, pero si te metes al
  agua tienes que soltarlas (§1.5).
- **Efectos del ratio** (ya implementados, sin números de más que inventar):
  `GetSwimLoadRatio()` cansa nadando en proporción a lo que llevas *encima*
  (sin contar las angarillas, que se sueltan antes de nadar), `GetNoiseLevel()`
  te delata más cargado (biblia §4.2, tabla de pesca), y
  `GetMoveSpeedMultiplier()` te frena por encima de la capacidad cómoda.
- **Volumen:** cada contenedor (bolsillos, cinturón, bolsa estanca, mochila,
  contenedores del mundo) tiene su propio máximo de volumen y peso
  (`FInventoryContainerSpec`); un objeto que pesa poco pero abulta mucho
  (angarillas: 20 L / 6 kg) no cabe en una mochila de fibra por volumen aunque
  el peso sobre.

### 1.3 Pilas

*El motor no apila hoy (`FInventoryEntry` es un objeto por hueco); se
define aquí la regla que sí falta.* **Decisión:** apilan hasta **10 unidades
por hueco** los recursos sin durabilidad ni líquido propio — materiales
naturales, materiales procesados, comida cruda o cocinada, medicina de un
solo uso. **No apilan nunca**: nada con `maxDurability > 0` (herramientas,
armas, contenedores, mochilas), nada con `LiquidCapacityLiters > 0`
(cantimplora, vasija, coco), ni los objetos `DosManos`. Una pila ocupa un solo
hueco visible a efectos de bolsillos/cinturón/bolsa estanca, pero su peso y
volumen suman los de cada unidad para la capacidad de la mochila. Justifica
por qué: sin esto, 4 bolsillos para 400 objetos distintos sería
inmanejable — rompería la regla anti-agobio de la biblia (§8.3, «sin
microgestión»).

### 1.4 Contenedores

Contenedores del mundo (`EWorldContainerKind`, ya en el enum): **Cesta**,
**Estante**, **Arcón**. Los que ya tienen pieza de construcción real en
`building_pieces.json`:

| Contenedor | Pieza | Capacidad | Coste |
|---|---|---|---|
| Cesta | **NUEVO** `cesta_almacen` (el item `cesta` ya existe como objeto portátil de 2 L; falta la versión mueble fija) | 15 L, sin límite de peso propio, acepta todo salvo líquidos sueltos | `bambu_fino` ×6, `cordel` ×4 |
| Estante | **NUEVO** `estanteria_almacen` (distinta de `estanteria_museo`, que es solo exposición) | 30 L, 40 kg, solo objetos `Mediano` o menores | `madera_dura` ×4, `cuerda` ×2 |
| Arcón | **NUEVO** `arcon` (la biblia §3.10 ya lo nombra en «Mobiliario»; no tiene entrada en `building_pieces.json`) | 60 L, 80 kg, todo salvo `DosManos`; único contenedor estanco del mundo (protege de la lluvia) | `madera_dura` ×6, `cuerda` ×3, `resina` ×2 |

Todos **[AA]**: sin almacenamiento de base jugable, ni el refugio→cabaña de la
porción vertical de Landing tiene sentido (GDD §6.1).

### 1.5 Transportar objetos grandes: a hombros y en carretilla

- **A hombros:** cualquier objeto de tamaño `DosManos` (tronco pequeño 8 kg,
  atún 20 kg, angarillas cargadas) ocupa las dos manos con `PickUp` y no se
  puede combinar mientras se lleva: hay que soltarlo o guardarlo primero. Es
  la vía por defecto, sin pieza ni fabricación, coherente con «máximo tres
  verbos» (biblia §8.3): coger, cargar, soltar.
- **Angarillas (`angarillas`, real):** 6 kg, 20 L, durabilidad 100, solo
  `madera`/`piedra`. Se enganchan detrás (`EquipFromHand` → `ECarrySlot::Sledge`)
  y se arrastran: liberan las manos a cambio de una penalización de peso al
  30 % (§1.2) y de quedar bloqueadas si te metes al agua (`DetachSledge`, se
  sueltan solas y quedan tiradas donde estabas).
- **Carretilla — NUEVO, `carretilla`:** las angarillas cubren madera y piedra
  a rastras; la carretilla es la vía general para todo lo demás sin ocupar
  las manos y sin la penalización de arrastre. Objeto `DosManos` que se
  **empuja** (no se lleva en mochila): contenedor propio de 40 L / 25 kg,
  acepta cualquier etiqueta salvo líquido suelto, no reduce la capacidad
  cómoda del cuerpo (el peso lo lleva la rueda, no la espalda) pero sí baja
  la velocidad de movimiento un 30 % mientras se empuja y no se puede correr,
  nadar ni subir escaleras con ella enganchada. Receta: `bambu_grueso` ×2,
  `tronco_pequeno` ×1 (eje), `cuerda` ×2, `piedra_plana` ×1 (rodamiento),
  fabricada en `piedra_trabajo`. Durabilidad 60. **[AA]** — es la
  herramienta que hace jugable llevar el mineral de la mina a la base a
  pie, antes de que existan los raíles (§3.5 del GDD ya avisa de que esa
  fase «se sepa cuánto pesa de verdad transportar mineral a mano»: la
  carretilla es la respuesta intermedia, no un sustituto de los raíles).
  *[Decisión 2026-09-28, datos de H2] La plantilla combina dos piezas en las manos,
  así que la receta de cuatro materiales se traduce a propiedades: **Bastidor** de
  bambú (Largo ≥ 4, Rígido ≥ 3, etiqueta `bambu`), **Rueda** atada con cuerda
  (Contundente ≥ 2, Rígido ≥ 3: `piedra_plana` o `canto_rodado`) y **Unión** (Ata ≥ 3).
  Se hace así: primero `piedra_plana` + `cuerda` (atado), luego `bambu_grueso` + ese
  atado, en `piedra_trabajo`. El eje de `tronco_pequeno` desaparece de la receta: con
  dos manos no hay hueco para un tercer material sin un paso más, y el tronco solo
  añadía peso. Va antes que `hacha` en `templates.json` para que el bambú con piedra
  atada no dé un hacha.*

### 1.6 Soltar y lanzar

- **Soltar (`Drop`, real):** deja el objeto en el suelo 100 cm delante del
  jugador (`DropDistanceCm`), en la dirección a la que mira
  (`SpawnDropped` → `AExploredItemActor`). Un `DosManos` se suelta entero
  aunque pidas soltar solo una mano.
- **Lanzar:** objetos con la etiqueta `lanzable` (la lanza, `items.json`
  tag `"lanzable"`) se arrojan en vez de soltarse; el resto de armas
  arrojadizas de la biblia (flecha de arpón, cuchillo de desollar) heredan
  la misma etiqueta cuando se fabrican con esa intención. No hay lanzamiento
  libre de cualquier objeto: solo lo que el propio objeto declara.

### 1.7 Qué pasa al morir

`AExploredGameMode::HandlePlayerDeath` ya decide, por modo de juego
(`survival_needs.json`, salud a 0 → «Muerte (reaparición en fogata salvo en
modo Náufrago)»):

- **Explorador / Superviviente / Personalizado:** reaparece junto a la
  hoguera o fogata marcada como punto de reaparición más cercana
  (`IsRespawnPoint()`), o en el `PlayerStart` si no hay ninguna. Se baja
  automáticamente de cualquier barco (`Leave()`) para no quedar enganchado.
- **Náufrago:** sin reaparición (`ERespawnDecision::GameOver`): la partida
  termina y vuelve al menú.

**Decisión — qué pasa con el inventario (no está en el código, hace falta
fijarlo):** el jugador **no pierde nada** al morir en Explorador/
Superviviente/Personalizado; reaparece con las manos, bolsillos, cinturón,
bolsa estanca, mochila y angarillas exactamente como estaban. Penalización:
un golpe de ánimo de −6 (`moraleEvents.Injured`, ya existe en
`survival_needs.json`) más lo que la propia causa de la muerte deje como
secuela (una herida sangrando, hipotermia, veneno sin curar — el cuerpo
sigue el mismo modelo que una herida cualquiera, GDD §3.1). Justificación:
la biblia prohíbe la microgestión punitiva (§8.3) y no hay ningún sistema de
«recuperar el cadáver» diseñado ni en el GDD ni en el código; añadir pérdida
de objetos sin una carrera al cadáver sería solo frustración sin mecánica
detrás. En Náufrago la pregunta no aplica: la partida termina y el
inventario desaparece con ella, que es su único castigo real y ya es
suficiente.

---

## 2. El crafteo

### 2.1 Combinar en las manos

Un objeto en cada mano + **C** (combinar) prueba las plantillas de
`Content/Data/templates.json` contra lo que sostienes; el juego solo ofrece
las combinaciones cuyos requisitos cumples, máximo tres a la vez (biblia
§8.3). Los verbos realmente implementados hoy en `Content/Data/verbs.json`
son **8**, no los 17 «aspiracionales» que enumera
`docs/design/biblia-de-contenido.md` §2.2 — ese documento describe la visión
completa del sistema de propiedades; esta tabla es la que existe en datos:

| Verbo | Qué hace | Plantillas reales que lo usan |
|---|---|---|
| **Golpear** | Contundente sobre duro → lasca | `lasca_por_golpeo` |
| **Tallar** | Filo sobre madera/hueso → estaca | `estaca_por_tallado` |
| **Atar** | Ata + dos piezas → herramienta compuesta | `atado_generico`, `angarillas`, `hacha`, `lanza`, `cuchillo`, `martillo`, `antorcha`, `flecha`, `pala`, `arco` |
| **Pegar** | Adhesivo + dos piezas → unión más duradera | `pegado_generico`, `hacha`, `lanza`, `antorcha` |
| **Afilar** | Abrasivo sobre filo → recupera filo y durabilidad | `afilar_generico` |
| **Trenzar** | Fibroso ×2 → cordel, cuerda, cesta | `cordel`, `cuerda`, `cesta` |
| **Machacar** | Contundente sobre blando → pasta | `pasta_medicinal_por_machacado` |
| **Raspar** | Filo sobre coco/piel → recipiente, cuero en bruto | `recipiente_de_coco` |

Cada plantilla define **roles** (Cabeza, Mango, Unión…) con requisitos
mínimos de propiedad (p. ej. `hacha`: Cabeza Filo≥2 y Contundente≥2, Mango
Largo≥2 y Rigido≥3, Unión Ata≥2 o Adhesivo≥2) y genera el nombre y las
estadísticas por fórmula, no por tabla fija — es el sistema descrito en la
biblia §2.3, ya implementado tal cual.

**Plantilla nueva que falta para que la minería (GDD §3.4) sea jugable —
NUEVO, `pico`. [Decisión] Definición única: la biblia 02 §2.2 y esta tabla
citaban dos versiones distintas de la misma plantilla (Cabeza por Punta aquí,
por Contundente/Rígido allí; `baseMaxDurability` 40 aquí, 55 allí); manda la
versión de 02 §2.2 porque ata la Cabeza a las propiedades reales de
`canto_rodado`/`basalto` (Contundente, §3.3 de este documento) en vez de a
una propiedad (Punta) que esos materiales no tienen:**

| Rol | Requisito |
|---|---|
| Cabeza | Contundente ≥ 3 o Rígido ≥ 3 (rompe la roca por impacto, no por punta) |
| Mango | Largo ≥ 2 y Rígido ≥ 3 |
| Unión | Ata ≥ 2 o Adhesivo ≥ 2 |

`nameTemplate`: «Pico de {0} con mango de {1}, atado con {2}» /
«{0} pick with a {1} handle, lashed with {2}» (igual que 02 §2.2; no
«pickaxe», para no tener dos nombres en inglés del mismo objeto).
`baseMaxDurability`: 55. Verbos: Atar, Pegar (igual que hacha/lanza). Sin
este objeto no hay forma de picar caliza, basalto ni obsidiana (§3.3 más
abajo), y el GDD lo exige desde la porción vertical de Landing (§6.1:
«minería manual básica»).

### 2.2 Bancos de trabajo y estaciones

Toda estación real es una pieza de `building_pieces.json` (categoría
`produccion`), con su coste ya fijado en datos:

| Estación | id (real) | Coste | Para qué sirve | Fase |
|---|---|---|---|---|
| Fogata | `fogata` | `canto_rodado` ×6, `rama_seca` ×5 | Cocinar (técnicas asar/hervir de nivel `fogata`), fundir sal, respawn point | [AA] |
| Hoguera de señal | `hoguera_senal` | `rama_seca` ×12, `tronco_pequeno` ×2 | Cocina de nivel superior, señal visible a distancia | [AA] |
| Piedra de trabajo | `piedra_trabajo` | `piedra_plana` ×1, `canto_rodado` ×2 | Tallado fino, afilado con bonificación, ensamblado de piezas grandes (angarillas, carretilla) | [AA] |
| Secadero | `secadero` | `bambu_fino` ×6, `cordel` ×4 | Ahumar y secar (técnicas `ahumar`/`secar`, `recipes.json`) | [AA] |
| Telar de fibra | `telar` | `bambu_fino` ×4, `madera_dura` ×2 | Trenzado avanzado: tela, estera, cordaje de barco | [AA] |
| Horno de barro | `horno_barro` | `arcilla_roja` ×8, `canto_rodado` ×6 | Hornear (vasija cocida, carbón vegetal), técnica `hornear` de `recipes.json`, nivel de fuego `horno_arcilla` | [AA] |
| Mesa de cartografía | `mesa_cartografia` | `madera_dura` ×6, `cuerda` ×2 | Copiar el mapa grande de la base | [AA] |
| Astillero | `astillero` | `tronco_pequeno` ×6, `madera_dura` ×4 | Construir barcos (`boats.json`) | [AA]/[F3] según barco |
| Estantería y vitrina del museo | `estanteria_museo`, `vitrina_museo`, `panel_museo` | ver `building_pieces.json` | Exponer tesoros (`artifacts.json`) | [AA] |

**Estaciones que el GDD exige y no existen aún en datos — NUEVO:**

| Estación | Coste propuesto | Para qué sirve | Fase |
|---|---|---|---|
| `banco_chatarra` | `chapa_fuselaje` ×2, `tubo_aluminio` ×2, `piedra_plana` ×2 | Recupera piezas del Albatros: convierte chatarra suelta en `lingote_aluminio` sin fundir (solo martillo y corte) | [AA] |
| `horno_fundicion` | `arcilla_roja` ×10, `canto_rodado` ×10, `carbon_vegetal` ×4 | Funde mineral: `mineral_cobre`→`lingote_cobre`, `hierro_meteorito`→`lingote_hierro`, `chapa_fuselaje`/`tubo_aluminio`→`lingote_aluminio`. Nivel de fuego propio `horno_fundicion` (heat 1.4, por encima de `horno_arcilla`) | [AA] |
| `yunque` | `basalto` ×4, `lingote_hierro` ×1 (el primer yunque se trae ya forjado en un pecio; después se puede fabricar uno propio) | Forjar con metal fundido: da +1 calidad fija a cualquier herramienta de cobre/hierro fabricada sobre él, y es donde se reparan (biblia §2.4) | [AA] |

*[Decisión 2026-09-28, datos de H2] `horno_fundicion` pide `horno_barro` en
`requiresPieces` (el carbón de su coste sale de ahí) y `yunque` pide `horno_fundicion`
(su lingote de hierro sale de ahí). El nivel de fuego `horno_fundicion` vive en
`fuels.json/smeltingLevels` y no en `levels` hasta que `EFireLevel` tenga su valor:
`levels` genera `FireData.inl` y un id que el enum no conoce se leería como fogata.*

*Decisión: se mantienen fuera de un módulo `Tramway` o `Smelting` aparte —
son piezas del mismo kit de construcción con coste y función propios, igual
que decide el GDD §3.3 para raíles y murallas («reutiliza el mismo modelo,
no es un sistema aparte»).*

### 2.3 Descubrimiento de recetas

Tres vías, ya descritas en la biblia §2.5, con su mecanismo concreto:

1. **Experimentación:** sostener dos objetos cuyas propiedades cumplen una
   plantilla dispara la idea del personaje y el boceto tenue en el mapa
   (`docs/design/biblia-de-contenido.md` §2.5); combinar sin haber visto la
   idea también funciona si los requisitos se cumplen — la idea es una
   pista, no un candado.
2. **Aprendizaje con los navegantes del arrecife [F3]:** con reputación alta,
   el pueblo (GDD §3.9) enseña directamente una plantilla que de otro modo
   solo sale de una ruina — hoy limitado a técnicas de wayfinding
   (§9.2 de la biblia); se extiende aquí a **una plantilla de herramienta
   de cobre por nivel de reputación alta** (empezando por `pico` de cobre),
   coherente con «el pueblo puede enseñar directamente una técnica... como
   alternativa a encontrarla en una ruina» (GDD §3.9).
3. **Hallazgos en ruinas:** examinar un artefacto de `artifacts.json` en una
   ruina (no en el museo, donde ya está a salvo) enseña la plantilla
   asociada a su `kind` la primera vez que se examina; ver §3.10 para el
   listado id-a-plantilla.

---

## 3. Tabla maestra de objetos

Notación de columnas: **origen** (de dónde sale), **uso**, **dur.**
(`maxDurability`; «—» = no se rompe o no aplica), **peso** (`weightKg`),
**fase**. Los ids sin anotación son literales de `Content/Data/items.json`;
**NUEVO** marca lo que no existe todavía.

### 3.1 Recursos naturales

Selección representativa: el catálogo completo por categoría (32
madera/planta, 22 piedra/tierra, 14 mar, 12 animal) está en
`docs/design/biblia-de-contenido.md` §3.1. Aquí se fijan los que alimentan
las recetas de este documento.

| id | ES / EN | Descripción ES / EN | Origen | Uso | Peso | Fase |
|---|---|---|---|---|---|---|
| `rama_seca` | Rama seca / Dry branch | «Sirve de yesca o de mango, según cómo la mires.» / "Tinder or a handle, depending how you look at it." | Suelo bajo cualquier árbol | Combustible, mango corto | 0,3 kg | AA |
| `palo_recto` | Palo recto / Straight stick | «El mango que no falla.» / "The handle that never lets you down." | Recogido, talado | Mango de herramienta | 0,5 kg | AA |
| `tronco_pequeno` | Tronco pequeño / Small log | «Se carga a hombros o no se carga.» / "Shoulder it or leave it." | Talar árboles | Construcción, astillero | 8,0 kg | AA |
| `bambu_fino` / `bambu_grueso` | Caña de bambú / Bamboo cane | «Hueca, ligera, y aguanta agua si tiene nudo.» / "Hollow, light, holds water if it's got a node." | Cañaveral de bambú | Recipientes, mangos, estructura | 0,3 / 0,8 kg | AA |
| `madera_dura` | Madera dura de guayabo / Hardwood | «No se astilla ni a martillazos.» / "Won't splinter no matter how you hit it." | Talar guayabo/ébano isleño | Mangos y estructura de calidad | 1,2 kg | AA |
| `liana` | Liana / Vine | «Ata cualquier cosa a cualquier cosa.» / "Ties anything to anything." | Selva | Atar (Ata 4) | 0,2 kg | AA |
| `fibra_coco` | Fibra de coco / Coconut fibre | «Prende con una chispa y poco más.» / "Catches with one spark and not much else." | Cáscara de coco | Yesca, trenzado | 0,1 kg | AA |
| `resina` | Resina / Resin | «Pega y arde; hay que elegir para qué la quieres.» / "Sticks and burns; pick one." | Tronco de árbol dañado | Adhesivo, combustible | 0,1 kg | AA |
| `canto_rodado` | Canto rodado / Pebble | «Redondo de tanto rodar en la orilla.» / "Rounded from years on the shore." | Playa, río | Golpear, cimientos | 0,8 kg | AA |
| `pedernal` | Pedernal / Flint | «Salta chispa si sabes darle.» / "Sparks if you know how to strike it." | Afloramientos rocosos | Lascas, encendido | 0,5 kg | AA |
| `basalto` | Basalto / Basalt | «Negro, denso, del volcán.» / "Black, dense, straight from the volcano." | Subsuelo, minería (dureza 3) | Herramientas contundentes | 1,0 kg | AA |
| `obsidiana` | Obsidiana / Obsidian | «Corta mejor que nada que tengas — y se rompe igual de fácil.» / "Cuts better than anything you own — and breaks just as easily." | Isla del Humo, minería (dureza 4) | El mejor filo del juego | 0,4 kg | AA |
| `caliza` | Caliza / Limestone | «Blanda para ser piedra.» / "Soft, for a rock." | La Meseta, minería (dureza 2) | Cal, construcción | 1,0 kg | F2 |
| `mineral_cobre` | Mineral de cobre / Copper ore | «Verdoso, se nota el metal por dentro.» / "Greenish — you can tell there's metal in there." | Basalto de cualquier isla, minería (dureza 2) | Fundir → `lingote_cobre` | 1,2 kg | AA |
| `hierro_meteorito` | Hierro de meteorito / Meteoric iron | «Cayó del cielo antes de que llegaras tú.» / "Fell from the sky before you did." | Cráteres de impacto, Los Dientes, minería (dureza 3) | Fundir → `lingote_hierro` | 2,4 kg | AA |
| `cristal_cuarzo` | Cristal de cuarzo / Quartz crystal | «Brilla hasta en la oscuridad de la mina.» / "Catches light even down in the dark." | Cavernas de cristal, minería (dureza 4) | Filo fino, lentes | 0,3 kg | AA |
| `azufre` | Azufre / Sulphur | «Huele a lo que es: el propio volcán.» / "Smells exactly like what it is: the volcano itself." | Fumarolas del Humo | Pólvora (si se decide, GDD §3.8) | 0,2 kg | AA |
| `hueso_largo` | Hueso largo / Long bone | «Punta de sobra en cuanto se afila.» / "Plenty of point once you sharpen it." | Despiece de fauna, pesca | Puntas, mangos | 0,4 kg | AA |
| `tendon` | Tendón / Sinew | «Ata mejor que la fibra, si no te importa el olor.» / "Binds better than fibre, if the smell doesn't bother you." | Despiece | Atar (Ata 3) | 0,05 kg | AA |
| `piel_bruto` (tiburón) | Piel de tiburón en bruto / Raw shark skin | «Lija sola: no hace falta arenisca.» / "Sands things on its own — no need for sandstone." | Despiece de tiburón | Curtir → `cuero_curtido` | 0,5 kg | AA |
| `chapa_fuselaje` | Chapa del fuselaje / Fuselage sheet metal | «Lo único metálico que te queda del Albatros.» / "The only metal you've got left from the Albatros." | Restos del avión | Herramientas Nivel 4, fundir | 0,8 kg | AA |
| `tubo_aluminio` | Tubo de aluminio / Aluminium tube | «Ligero y no se pudre nunca.» / "Light, and it never rots." | Restos del avión | Mangos duraderos, fundir | 0,6 kg | AA |

### 3.2 Materiales procesados y rescatados

| id | ES / EN | Descripción ES / EN | Origen | Uso | Dur. | Peso | Fase |
|---|---|---|---|---|---|---|---|
| `lasca_pedernal` | Lasca de pedernal / Flint flake | «Filo de urgencia.» / "An edge in a pinch." | Golpear pedernal | Cuchillo tosco | — | 0,1 kg | AA |
| `lasca_obsidiana` | Lasca de obsidiana / Obsidian flake | «El filo bueno, mientras dure.» / "The good edge, while it lasts." | Golpear obsidiana | Cuchillo/hacha de calidad | — | 0,1 kg | AA |
| `cordel` | Cordel / Twine | «Ata poco, pero es el primer paso.» / "Doesn't hold much, but it's a start." | Trenzar fibra ×2 | Ata 2 | — | 0,05 kg | AA |
| `cuerda` | Cuerda / Rope | «El cordel, ya crecido.» / "Twine, grown up." | Trenzar cordel ×2 | Ata 4, construcción, barcos | — | 0,15 kg | AA |
| `cuero_curtido` | Cuero curtido / Tanned hide | «Ya no se pudre ni pesa lo que pesaba.» / "Won't rot anymore, and it's lighter than it was." | Curtir piel en bruto | Ropa, mochilas | — | 0,4 kg | AA |
| `carbon_vegetal` | Carbón vegetal / Charcoal | «Arde más caliente y más limpio que la leña.» / "Burns hotter and cleaner than firewood." | Hornear tronco (`horno_barro`) | Combustible de fundición | — | 0,2 kg | AA |
| `vasija_barro` | Vasija de barro cocido / Fired clay pot | «Aguanta el fuego sin rajarse.» / "Holds up to the fire without cracking." | Hornear arcilla (`horno_barro`) | Cocina, recipiente estanco | 60 | 1,2 kg | AA |
| `lingote_cobre` — **NUEVO** | Lingote de cobre / Copper ingot | «El primer metal que es de verdad tuyo.» / "The first metal that's really yours." | Fundir `mineral_cobre` ×2 en `horno_fundicion` | Cabeza de herramienta tier cobre | — | 0,9 kg | AA |
| `lingote_hierro` — **NUEVO** | Lingote de hierro / Iron ingot | «Pesado, pero aguanta lo que ninguna piedra aguanta.» / "Heavy, but it holds up where no stone would." | Fundir `hierro_meteorito` ×2 en `horno_fundicion` | Cabeza de herramienta tier hierro | — | 1,8 kg | AA |
| `lingote_aluminio` | Lingote de aluminio / Aluminium ingot | «Ligero y ya nunca se oxida.» / "Light, and it'll never rust." | Fundir `chapa_fuselaje`/`tubo_aluminio` en `horno_fundicion` o `banco_chatarra` | Herramientas Nivel 4, barco «Limón» | — | 0,7 kg | AA |
| `alambre` — **NUEVO** | Alambre / Wire | «Se dobla en lo que haga falta.» / "Bends into whatever you need." | Estirar `lingote_cobre` en `yunque` | Anzuelos, clavos, bisagras | — | 0,05 kg | AA |
| `clavos` — **NUEVO** | Clavos / Nails | «Ya no dependes de atar todo con liana.» / "No more tying everything with vine." | Forjar `alambre`/`lingote_hierro` en `yunque` | Unión de construcción de calidad | — | 0,02 kg (por unidad) | AA |

### 3.3 Herramientas por tier

Las herramientas **no son ids distintos por tier**: el sistema de la biblia
(§2.1–2.3) genera el nombre y las estadísticas a partir del material que
entra en el rol «Cabeza»/«Hoja»/«Punta». Esta tabla fija, para cada rol,
cuál es el material de cada tier y qué produce por la fórmula ya definida
(«Corte = Filo cabeza», «Durabilidad = min(cabeza, unión)», biblia §2.3):

| Tier | Material de Cabeza/Filo/Punta | Filo / Punta / Contundente | Corte o Daño resultante | Fase |
|---|---|---|---|---|
| **Piedra** | `pedernal` (lasca) o `basalto` | Filo 3 / Punta 1 (pedernal) · Contundente 4 (basalto) | Bajo-medio, se repara fácil con `arenisca` | AA |
| **Hueso y concha** | `hueso_largo`, `concha_pequena` | Punta 2-3 / Filo 1 | Punta buena, filo pobre: mejor para lanzas y anzuelos que para cortar | AA |
| **Obsidiana** | `obsidiana` / `lasca_obsidiana` | Filo 5 / Punta 3-5 | El mejor filo y punta del juego (una lasca tallada perfora igual de bien que corta); frágil contra roca dura (biblia §2.4) | AA |
| **Cobre** | `lingote_cobre` — NUEVO | Filo 3 / Contundente 3 (valores del ingote, a fijar en `items.json` al crearlo) | Durabilidad claramente por encima de piedra, por debajo de hierro | AA |
| **Hierro** | `lingote_hierro` / `hierro_meteorito` | Filo 4 / Contundente 4-5 | La mejor durabilidad; el hierro de meteorito es raro (Los Dientes) | AA |
| **Rescatado (Nivel 4)** | `chapa_fuselaje`, `tubo_aluminio`, `lingote_aluminio` | Filo 2-3 / Rígido 3-4 | Ligero y muy duradero; requiere `banco_chatarra` o `horno_fundicion` | AA |

**Plantillas reales que consumen esos roles** (`templates.json`, ids
literales): `hacha` (dur. base 60), `cuchillo` (40), `martillo` (50),
`lanza` (50), `arco` (45), `flecha` (0, se pierde al fallar o se recupera
del blanco), `pala` (40), `antorcha` (20), `estaca` (15), `lasca_tallada`
(10). Más **`pico`** — NUEVO, §2.1 (dur. base 55, alineada con 02 §2.2): es
la única herramienta que exige el GDD §3.4 y no existe todavía en datos; sin
ella, la porción vertical de Landing (§6.1, «minería manual básica») no se
puede construir.

**Herramienta mínima por estrato** (tabla ya fijada en el GDD §3.4, se
reproduce aquí porque es la que ata objeto-a-uso de esta sección):

| Estrato | Dureza | Herramienta mínima | Tier de `pico`/`pala` |
|---|---|---|---|
| Tierra, arena, arcilla | 1 | Pala tosca | Cabeza piedra/hueso |
| Caliza | 2 | Pico de piedra | Cabeza piedra |
| Basalto | 3 | Pico tallado | Cabeza obsidiana u hueso duro |
| Obsidiana, cristal | 4 | Pico de obsidiana o rescatado | Cabeza obsidiana o Nivel 4 |

### 3.4 Armas

| id | ES / EN | Descripción ES / EN | Origen | Uso | Dur. | Peso | Fase |
|---|---|---|---|---|---|---|---|
| `lanza` | Lanza / Spear | «Empuja o se lanza: tú decides.» / "Thrust it or throw it — your call." | Plantilla `lanza` (Atar/Pegar) | Caza, pesca de empuje, defensa | 50 | 1,2 kg | AA |
| `arco` | Arco / Bow | «Silencioso hasta que sueltas la cuerda.» / "Silent, until you let the string go." | Plantilla `arco` (Atar) | Caza y pesca a distancia | 45 | 1,0 kg | AA |
| `flecha` | Flecha / Arrow | «Se pierde más de las que se recupera.» / "You lose more than you recover." | Plantilla `flecha` (Atar) | Munición del arco | 0 (un solo uso) | 0,1 kg | AA |
| `cuchillo` | Cuchillo / Knife | «Sin mango, te cortas tú antes que nada.» / "No handle, and you'll cut yourself before anything else." | Plantilla `cuchillo` (Atar) | Despiece, corte, defensa cercana | 40 | 0,3 kg | AA |
| `estaca` | Estaca / Stake | «Tosca, pero perfora.» / "Crude, but it punches through." | Plantilla `estaca_por_tallado` | Defensa improvisada, trampas | 15 | 0,4 kg | AA |
| `sierra_diente_tiburon` — **NUEVO** (biblia §3.4 «Nivel 2») | Sierra de dientes de tiburón / Shark-tooth saw | «Corta hueso como si fuera fruta.» / "Cuts through bone like fruit." | Tallar `dientes_sombra`/diente de tiburón sobre `palo_recto` | Despiece rápido, arma de filo largo | 35 | 0,6 kg | AA |

*El juego no tiene una categoría de «armas» separada del resto de
herramientas: cualquier objeto con Filo o Punta suficiente sirve para
defenderse, coherente con «el mundo enseña» (biblia §1). Esta tabla recoge
solo lo pensado explícitamente para el combate o la caza.*

### 3.5 Comida y bebida (con efectos sobre los estados)

Datos reales de `Content/Data/recipes.json` (cocina) y `survival_needs.json`
(qué necesidad recupera cada cosa). `food`/`water`/`warmth`/`morale` son
puntos de `FConsumable`; `toxicity` se limpia cocinando cuando
`cookingRemovesToxicity` es verdadero.

| id | ES / EN | Descripción ES / EN | Origen | food / water / warmth / morale | Toxicidad | Fase |
|---|---|---|---|---|---|---|
| `agua_sin_tratar` | Agua sin tratar / Untreated water | «Bebible. Probablemente.» / "Drinkable. Probably." | Río, charca | 0 / 30 / 0 / 0 | 0,35 (hervir la quita) | AA |
| `agua_hervida` | Agua hervida / Boiled water | «Ya no te la juegas.» / "No more gambling with this one." | Hervir agua sin tratar en `fogata` | 0 / 30 / +1 / +1 | 0 | AA |
| `coco_verde` | Coco verde / Green coconut | «Agua embotellada por la propia palmera.» / "Water, bottled by the palm tree itself." | Palmera | 1 / 25 / 0 / +1 (Nutritivo 1) | 0 | AA |
| `pescado_asado` | Pescado asado en espeto / Spit-roasted fish | «Lo único que sabe mejor que crudo, siempre.» / "The one thing that always tastes better cooked." | Asar cualquier `tag:pescado` en `espeto`, 20 min | 22 / 0 / +3 / +3 | 0 | AA |
| `estofado_pescado` | Estofado de pescado / Fish stew | «Un plato de verdad, para variar.» / "An actual meal, for once." | Guisar pescado + tubérculo + agua dulce en `vasija_barro`, 60 min | 34 / 10 / +8 / +6 | 0 | AA |
| `guiso_improvisado` | Guiso improvisado / Improvised stew | «No preguntes qué lleva.» / "Don't ask what's in it." | Cualquier combinación sin receta nombrada | 18 / 8 / +4 / +2 | 0 | AA |
| `comida_quemada` | Comida quemada / Burnt food | «Come, si tienes hambre de verdad.» / "Eat it, if you're hungry enough." | Dejar pasar el tiempo de quemado (`burnAfterMinutes`) | 4 / 0 / 0 / −4 | 0 | AA |
| `pescado_ahumado` | Pescado ahumado / Smoked fish | «Aguanta dos semanas sin rechistar.» / "Keeps for two weeks, no complaints." | Ahumar en `secadero`, 6 h | 18 / −2 / 0 / +2 | 0 | AA |
| `yuca_cocida` | Yuca cocida / Boiled cassava | «Cruda te mata; cocida, alimenta de verdad.» / "Raw, it'll kill you. Cooked, it's real food." | Hervir yuca + agua dulce | 20 / 2 / +3 / +1 | 0 (cruda: 0,8) | AA |
| `limon` | Limón / Lemon | «Amargo, pero es lo que te mantiene entero.» / "Bitter, but it's what keeps you in one piece." | Limonero (§7.1 biblia) | Nutritivo 1, **Vitamina C 5**: cura y previene el escorbuto | 0 | AA |
| `guiso_improvisado` (dieta monótona) | — | «Otra vez lo mismo.» / "Same thing again." | Comer el mismo `family` repetido 72 h | Ánimo bajando (`MonotonyBalanceBelow`, `survival_needs.json`) | — | AA |

### 3.6 Medicinas

| id | ES / EN | Descripción ES / EN | Origen | Cura | Dur. | Peso | Fase |
|---|---|---|---|---|---|---|---|
| `pasta_medicinal` | Pasta medicinal / Medicinal paste | «Huele mal. Funciona.» / "Smells awful. Works." | Machacar mazo + planta medicinal (`Medicinal ≥1`) | Heridas superficiales, infección leve | 0 (un uso) | 0,1 kg | AA |
| `vendaje_tela` — **NUEVO** | Venda de tela / Cloth bandage | «Para el corte, no para lo de dentro.» / "For the cut, not what's underneath." | Trenzar `cordel` + `tela_fibra` sobre herida | Detiene el sangrado (`WoundBleedDamagePerHour`) | 0 | 0,05 kg | AA |
| `antidoto_corteza` — **NUEVO** | Antídoto de corteza / Bark antidote | «Contra la raya, si llegas a tiempo.» / "For the ray sting, if you're quick about it." | Machacar corteza de sauce isleño + agua | Picadura de raya (`RayStingHours`, `survival_needs.json`) | 0 | 0,1 kg | AA |
| `carbon_activado` — **NUEVO** | Carbón activado / Activated charcoal | «Cuando lo que has comido no era buena idea.» / "For when what you ate wasn't a good idea." | Quemar `carbon_vegetal` con agua en `vasija_barro` | Intoxicación (`toxicity` alta) | 0 | 0,1 kg | AA |
| `te_corteza_sauce` — **NUEVO** | Té de corteza de sauce isleño / Willow-bark tea | «Baja la fiebre, no la sube.» / "Brings the fever down, not up." | Hervir corteza de sauce en `vasija_barro` | Fiebre (por infección sin curar, `WoundInfectionHours`) | 0 | 0,2 kg | AA |
| `ferula_bambu` — **NUEVO** | Férula de bambú / Bamboo splint | «No corre, pero cura recto.» / "It won't run, but it heals straight." | Atar `bambu_fino` + `cordel` sobre la extremidad | Esguince (`SprainHours`), fractura | — (equipable temporal) | 0,3 kg | AA |
| `gel_aloe` — **NUEVO** | Gel de aloe / Aloe gel | «Fresco en cuanto lo tocas.» / "Cool the moment it touches you." | Machacar `planta_medicinal_aloe` | Quemadura solar (`SunBurnDoseHours`) | 0 | 0,1 kg | AA |

### 3.7 Piezas de construcción

Selección; las 44 piezas completas están en `Content/Data/building_pieces.json`.

| id | ES / EN | Origen del material | Coste | Fase |
|---|---|---|---|---|
| `refugio_inclinado` | Refugio inclinado / Lean-to shelter | Landing, día 1 | `hoja_palma` + `palo_recto` (ver JSON) | AA |
| `pared_madera`, `suelo_madera`, `techo_madera`, `puerta_madera` | Pared/Suelo/Techo/Puerta de madera | Kit modular, 3 materiales (hoja/bambú/madera) | Escalado por tier | AA |
| `muro_piedra`, `cimiento_piedra` | Muro y cimiento de piedra | Piedra, tardío | `canto_rodado`, `basalto` | AA |
| `chimenea` | Chimenea / Chimney | Piedra | `canto_rodado`, `arcilla_roja` | AA |
| `pilote_madera`, `pilote_bambu` | Pilote (sobre agua) / Pile | — | Ver JSON | AA |
| `muelle`, `muelle_final` | Muelle / Dock | Costa | Ver JSON | AA |
| `arriate_limonero`, `bancal`, `espaldera` | Arriate, bancal, espaldera | Huerto | `canto_rodado`/`palo_recto`/`bambu_fino` + `arena`/`cordel` | AA |
| `torre_vigia` — **NUEVO** (biblia §3.10 la nombra) | Torre de vigía / Watchtower | Kit de madera | `tronco_pequeno` ×6, `madera_dura` ×4, `cuerda` ×3 | AA (vigía) / F2 (defensa contra piratas) |
| `cerca_estacas`, `empalizada` — **NUEVO** | Cerca de estacas, empalizada / Stake fence, palisade | Kit de madera | `estaca` ×8, `cuerda` ×2 (cerca) · `tronco_pequeno` ×10, `cuerda` ×6 (empalizada) | F2 |
| `muralla_piedra`, `torre_defensa` — **NUEVO** | Muralla de piedra, torre de defensa / Stone wall, defence tower | Kit de piedra | `basalto` ×12, `canto_rodado` ×8 (muralla por tramo) | F2 |
| `gallinero`, `pocilga`, `corral` — **NUEVO** (GDD §3.6) | Gallinero, pocilga, corral / Henhouse, pigsty, pen | Kit de madera + bambú | `bambu_grueso` ×6, `cuerda` ×4, `madera_dura` ×2 | F2 |

### 3.8 Piezas de barco (`Content/Data/boats.json`, reales)

| id | ES / EN | Coste | Herramientas | Minutos | Fase |
|---|---|---|---|---|---|
| `balsa` | Balsa / Raft | `tronco_pequeno` ×8, `liana` ×6 | `hacha` | 90 | AA |
| `canoa` | Canoa / Canoe | `madera_dura` ×6, `tronco_pequeno` ×2, `resina` ×3, `cuerda` ×2 | `hacha`, `cuchillo` | 240 | AA |
| `canoa_balancin` | Canoa con balancín y vela / Outrigger sailing canoe | `madera_blanda` ×2, `bambu_grueso` ×4, `palo_recto` ×2, `hoja_palma` ×12, `cuerda` ×6, `resina` ×2 (requiere `canoa`, la transforma) | `hacha`, `cuchillo` | 180 | AA |
| `barco_limon` | Barco «Limón» / The Lemon | `madera_dura` ×16, `tronco_pequeno` ×6, `chapa_fuselaje` ×6, `tubo_aluminio` ×4, `cable_electrico` ×3, `cinta_americana` ×2, `cuerda` ×12, `resina` ×6, `hoja_palma` ×20 (requiere `canoa_balancin` + 4 piezas del Albatros) | `hacha`, `cuchillo`, `martillo` | 600 | F3 |

### 3.9 Raíles y vagones — NUEVO por completo

GDD §3.5: fuera del acceso anticipado con justificación técnica propia; se
fija aquí el contenido para que la Fase 2 no empiece en blanco.

| id | ES / EN | Descripción ES / EN | Coste | Fase |
|---|---|---|---|---|
| `rail_recto` | Tramo de raíl recto / Straight rail section | «Dos metros de vía, ni uno más.» / "Two metres of track, not an inch more." | `madera_dura` ×3, `lingote_hierro` ×2, `clavos` ×6 | F2 |
| `rail_curvo` | Tramo de raíl curvo / Curved rail section | «Gira lo justo para no descarrilar.» / "Bends just enough to keep the cart on track." | `madera_dura` ×3, `lingote_hierro` ×2, `clavos` ×8 | F2 |
| `cambio_agujas` | Cambio de agujas / Points switch | «Decide por dónde va el vagón sin moverte de sitio.» / "Sends the cart wherever you want without you moving an inch." | `madera_dura` ×5, `lingote_hierro` ×4, `clavos` ×10 | F2 |
| `vagon` | Vagón / Mine cart | «Se empuja, o se tira de él con el torno.» / "You push it, or the winch pulls it." | `madera_dura` ×6, `lingote_hierro` ×3, `tubo_aluminio` ×2 (ruedas), `cuerda` ×2 | F2 |
| `torno_cuerda` | Torno de cuerda / Rope winch | «Sin motor: tira el brazo, no una máquina.» / "No engine — your arm pulls it, not a machine." | `tronco_pequeno` ×4, `lingote_hierro` ×2, `cuerda` ×8 | F2 |
| `ascensor_pozo` | Ascensor de pozo / Shaft lift | «Para cuando la mina ya no tiene fondo a la vista.» / "For when the mine's got no bottom in sight." | `madera_dura` ×10, `lingote_hierro` ×6, `cuerda` ×12, `torno_cuerda` ×1 | F2 |

Costes fijados por analogía directa con `boats.json` (mismo orden de
magnitud que la canoa con balancín, que también es transporte con piezas de
madera+metal+cuerda) — decisión razonada, no arbitraria, a falta de
prototipo de PIE (GDD §3.5).

### 3.10 Tesoros y artefactos

`Content/Data/artifacts.json`, reales — 16 piezas; el vínculo id-a-plantilla
de examen se añade aquí porque el JSON no lo tiene.

| id | ES / EN | Procedencia | Rareza | Enseña al examinar | Fase |
|---|---|---|---|---|---|
| `anzuelo_hueso` | Anzuelo de hueso tallado / Carved bone fish hook | Marae | Común | Plantilla de anzuelo de hueso | AA |
| `anzuelo_nacar` | Anzuelo de nácar / Mother-of-pearl fish hook | Pecio | Común | Plantilla de anzuelo de nácar | AA |
| `anzuelo_ceremonial` | Anzuelo ceremonial de hueso de ballena / Ceremonial whalebone hook | Cueva ritual | Raro | Plantilla de anzuelo grande (peces legendarios) | AA |
| `colgante_concha` | Colgante de concha / Shell pendant | Marae | Común | — (solo colección) | AA |
| `collar_conchas` | Collar de conchas / Shell necklace | Cueva ritual | Común | — (solo colección) | AA |
| `pectoral_nacar` | Pectoral de nácar / Pearl-shell breastplate | Ruina sumergida | Raro | — (solo colección) | F2 |
| `colgante_carey` | Colgante de carey con tortuga / Tortoiseshell turtle pendant | Pecio | Raro | — (solo colección) | AA |
| `figura_navegante` | Figura del navegante / Navigator figure | Marae | Raro | Técnica de wayfinding (biblia §9.2) | AA |
| `figura_gemelos` | Figura de los gemelos / Twin figure | Cueva ritual | Raro | Técnica de wayfinding | AA |
| `figura_mira_cielo` | Figura de basalto que mira al cielo / Basalt sky-gazer figure | Ruina sumergida | Único | Técnica «Camino de estrellas» (siempre la más difícil de encontrar) | F2 |
| `carta_varillas` | Carta de varillas y conchas / Stick and shell chart | Marae | Raro | Anotación de mapa: ruta entre dos islas concretas | AA |
| `carta_oleaje` | Carta del oleaje de las siete islas / Swell chart of the seven islands | Pecio | Único | Técnica «Lectura del oleaje» | AA |
| `tapa_pintada` | Tapa pintada / Painted tapa cloth | Cueva ritual | Raro | — (solo colección) | AA |
| `tapa_estrellas` | Tapa de las estrellas / Star tapa cloth | Marae | Único | Técnica «Camino de estrellas» (segunda fuente, distinta ruina) | AA |
| `remo_ceremonial` | Remo ceremonial / Ceremonial paddle | Marae | Raro | — (solo colección) | AA |
| `remo_canoa_doble` | Remo tallado de la canoa doble / Carved double-canoe paddle | Ruina sumergida | Único | Requisito de la plantilla `barco_limon` (pieza simbólica de la vela) | F3 |

Los muebles de exposición (`estanteria_museo`, `vitrina_museo`,
`panel_museo`) y sus huecos por tamaño ya están completos en
`artifacts.json`; no hace falta redefinirlos aquí.

---

## 4. Tabla maestra de recetas

### 4.1 Combinar en las manos (`templates.json`, reales + `pico` NUEVO)

| Plantilla | Entradas (roles) | Salida | Estación | Tiempo |
|---|---|---|---|---|
| `lasca_por_golpeo` | Percutor (Contundente≥3) + Núcleo (Rígido≥4) | `lasca_tallada` | Ninguna (en la mano) | Instantáneo |
| `estaca_por_tallado` | Filo (Filo≥2) + Madera (Largo≥1, Rígido≥1) | `estaca` | Ninguna | Instantáneo |
| `cordel` | Fibra A + Fibra B (Fibroso≥3 cada una) | `cordel` | Ninguna | Instantáneo |
| `cuerda` | Cordel A + Cordel B (Ata≥2 cada uno) | `cuerda` | Ninguna | Instantáneo |
| `cesta` | Fibra A + Fibra B (Fibroso≥2 cada una) | `cesta` | Ninguna | Instantáneo |
| `recipiente_de_coco` | Herramienta (Filo≥1 o Contundente≥1) + Coco | `recipiente_coco` | Ninguna | Instantáneo |
| `pasta_medicinal_por_machacado` | Mazo (Contundente≥2) + Planta (Medicinal≥1) | `pasta_medicinal` | Ninguna | Instantáneo |
| `hacha` | Cabeza (Filo≥2, Contundente≥2) + Mango (Largo≥2, Rígido≥3) + Unión (Ata≥2 o Adhesivo≥2) | `hacha` | Ninguna (mejor calidad en `piedra_trabajo`) | Instantáneo |
| `pico` — NUEVO | Cabeza (Contundente≥3 o Rígido≥3) + Mango (Largo≥2, Rígido≥3) + Unión (Ata≥2 o Adhesivo≥2) | `pico` | Ninguna (mejor calidad en `piedra_trabajo`) | Instantáneo |
| `lanza` | Asta (Largo≥4, Rígido≥2) + Punta (Punta≥3) + Unión | `lanza` | Ninguna | Instantáneo |
| `cuchillo` | Hoja (Filo≥3) + Mango (Largo≥1) | `cuchillo` | Ninguna | Instantáneo |
| `martillo` | Cabeza (Contundente≥3) + Mango (Largo≥2) | `martillo` | Ninguna | Instantáneo |
| `arco` | Pala (Flexible≥4, Largo≥4) + Cuerda (Ata≥3) | `arco` | Ninguna | Instantáneo |
| `flecha` | Astil (Largo≥2, Rígido≥1) + Punta (Punta≥2) | `flecha` | Ninguna | Instantáneo |
| `antorcha` | Asta (Largo≥2) + Combustible (Combustible≥2, Inflamable≥2) | `antorcha` | Ninguna | Instantáneo |
| `pala` | Superficie (Rígido≥2, Contundente≥1) + Mango (Largo≥2) | `pala` | Ninguna | Instantáneo |
| `afilar_generico` | Herramienta (Filo/Contundente/Punta≥1) + Abrasivo (Abrasivo≥2) | Restaura filo/durabilidad | Ninguna (bonificación en `piedra_trabajo`) | Instantáneo |
| `angarillas` | Varales (Largo≥4, Rígido≥3) + Lecho (Fibroso≥2) + Unión (Ata≥3) | `angarillas` | Ninguna | Instantáneo |

Combinar en la mano no cuesta tiempo de juego: la fricción está en reunir los
materiales, no en esperar delante de un menú (coherente con «máximo tres
verbos», biblia §8.3).

### 4.2 Cocina y conservación

`Content/Data/recipes.json`, reales — 16 recetas con nombre; el resto es
`guiso_improvisado`.

| id | Entrada | Estación / nivel de fuego | Minutos | Se quema a los |
|---|---|---|---|---|
| `agua_hervida` | `agua_sin_tratar` ×1 | `olla_coco`/`vasija_barro`, `fogata` | 10 | — |
| `sal_por_hervido` | `agua_mar` ×1 | `olla_coco`/`vasija_barro`, `fogata` | 60 | — |
| `sal_al_sol` | `agua_mar` ×1 | `bandeja`/`secadero`, sin fuego | 480 | — |
| `pescado_asado` | `tag:pescado` ×1 | `espeto`, `fogata` | 20 | +15 |
| `cangrejo_caparazon` | `cangrejo` ×1 | Sin recipiente, `fogata` | 15 | +10 |
| `batata_asada` | `batata` ×1 | Sin recipiente, `fogata` | 30 | +20 |
| `huevo_brasa` | `huevo` ×1 | Sin recipiente, `fogata` | 8 | +6 |
| `taro_hervido` | `taro` ×1 + `tag:agua_dulce` ×1 | `olla_coco`/`vasija_barro`, `fogata` | 30 | +60 |
| `yuca_cocida` | `yuca` ×1 + `tag:agua_dulce` ×1 | `olla_coco`/`vasija_barro`, `fogata` | 40 | +60 |
| `sopa_pescado` | `tag:pescado` ×1 + `tag:agua_dulce` ×1 | `olla_coco`/`vasija_barro`, `fogata` | 45 | +40 |
| `estofado_pescado` | `tag:pescado` ×1 + `tag:tuberculo` ×1 + `tag:agua_dulce` ×1 | `vasija_barro`, `fogata` | 60 (da 2 raciones) | +40 |
| `pescado_ahumado` | `tag:pescado` ×1 | `secadero`, sin fuego | 360 | — |
| `pescado_salado` | `tag:pescado` ×1 + `sal_marina` ×1 | Sin recipiente, sin fuego | 120 | — |
| `fruta_seca` | `tag:fruta` ×1 | `secadero`, sin fuego | 480 | — |
| `vasija_barro` | `arcilla_roja` ×2 | Sin recipiente, `horno_arcilla` | 180 | — |
| `carbon_vegetal` | `tronco_pequeno` ×1 (da 2) | Sin recipiente, `horno_arcilla` | 240 | — |

**Preservación** (real, `preservation` de `recipes.json`): crudo dura 24 h,
cocinado 72 h, ahumado 336 h (2 semanas), salado 480 h (20 días), seco 400 h.
Pasado el 75 % de su vida (`staleFraction`) la comida pasa a «pasada»
(nutrición ×0,8); al superar el tiempo entero, a «podrida» (toxicidad 0,9).

### 4.3 Estaciones de producción — recetas nuevas necesarias

| Receta | Entrada | Estación | Salida | Minutos |
|---|---|---|---|---|
| Fundir cobre — NUEVO | `mineral_cobre` ×2, `carbon_vegetal` ×1 | `horno_fundicion` | `lingote_cobre` ×1 | 30 |
| Fundir hierro — NUEVO | `hierro_meteorito` ×2, `carbon_vegetal` ×2 | `horno_fundicion` | `lingote_hierro` ×1 | 45 |
| Fundir aluminio — NUEVO | `chapa_fuselaje` ×1 o `tubo_aluminio` ×1, `carbon_vegetal` ×1 | `horno_fundicion` o `banco_chatarra` | `lingote_aluminio` ×1 | 20 |
| Estirar alambre — NUEVO | `lingote_cobre` ×1 | `yunque` | `alambre` ×4 | 10 |
| Forjar clavos — NUEVO | `alambre` ×1 o `lingote_hierro` ×1 | `yunque` | `clavos` ×8 | 10 |
| Tela de fibra — NUEVO | `algodon_silvestre` ×3 o `pita` ×3 | `telar` | `tela_fibra` ×1 | 40 |

*[Decisión 2026-09-28, datos de H2] En `recipes_smithing.json` cada «o» es una receta
aparte (`fundir_chapa`/`fundir_tubo`, `clavos_de_alambre`/`clavos_de_hierro`). En el
`banco_chatarra` el aluminio sale **sin fuego ni carbón** (`batir_chapa`, `batir_tubo`,
con martillo): §2.2 dice que el banco recupera la chatarra «sin fundir», así que no
tiene sentido que gaste combustible. Estirar alambre y forjar clavos piden martillo en
el yunque. La tela de fibra queda para su propia tarea (no es metal).*

### 4.4 Construcción y barcos (referencia)

Ver §3.7–3.8 de este documento para el coste exacto de cada pieza; el
formato de receta es siempre `FBuildingCost[]` consumido de las manos y
contenedores por orden de prioridad (`ConsumeMaterials`, ya implementado),
sin estación intermedia — la propia pieza fantasma es la vista previa de la
receta.

---

## 5. Árbol de progresión tecnológica

```text
Nivel 0 — A mano (Landing, día 1)
  piedra de golpear · palo de cavar · lasca · piedra de moler
  │
  ├─ Golpear pedernal ────────────► lasca_tallada, lasca_pedernal
  ├─ Tallar con lasca ────────────► estaca
  └─ Trenzar fibra ───────────────► cordel ──► cuerda
        │
        ▼
Nivel 1 — Tosco (Landing → Esmeralda)
  hacha (piedra) · cuchillo (lasca) · martillo · pala tosca (concha/madera)
  arco de fuego · cordel · cesta
        │
        ├─ Pala tosca + tierra/arena ────► minería superficial [AA, §3.4]
        ├─ piedra_trabajo (estación) ────► afilado y tallado con bonificación
        └─ fogata / hoguera_senal ───────► cocina, vasija sin cocer
              │
              ▼
Nivel 2 — Tallado (Esmeralda → Isla del Humo)
  hacha de pedernal con mango · machete madera+obsidiana · sierra de diente
  de tiburón · telar de cintura · horno_barro
        │
        ├─ pico (piedra) + caliza ───────► minería en caliza (La Meseta, F2)
        ├─ pico (obsidiana/tallado) + basalto ─► minería en basalto (Humo)
        └─ horno_barro ──────────────────► vasija cocida, carbón vegetal
              │
              ▼
Nivel 3 — Obsidiana (Isla del Humo)
  hacha, cuchillo, navaja de afeitar, puntas de flecha finas de obsidiana
  (el mejor filo del juego; se rompe contra roca dura)
        │
        ├─ pico de obsidiana + basalto/obsidiana ─► minería profunda,
        │     cuevas de cristal, tubos de lava
        └─ mineral_cobre / hierro_meteorito (minado) ─► banco_chatarra /
              horno_fundicion (NUEVO) ─► lingote_cobre, lingote_hierro
                    │
                    ▼
Nivel Cobre/Hierro — NUEVO (Isla del Humo, Los Dientes)
  yunque ─► pico, hacha, cuchillo, lanza, alambre, clavos de metal fundido
        │
        ├─ [F2] rail_recto, rail_curvo, vagon, torno_cuerda, ascensor_pozo
        ├─ [F2] muralla_piedra, torre_defensa, cerca_estacas, empalizada
        └─ [F2] gallinero, pocilga, corral (granja de animales)
              │
              ▼
Nivel 4 — Rescatado y fundido (restos del Albatros, cualquier isla)
  banco_chatarra ─► lingote_aluminio ─► hacha de aluminio, cuchillo de
  chapa, machete Halden restaurado, anzuelos de alambre
        │
        └─ [F3] astillero + 4 piezas del Albatros + canoa_balancin
                    │
                    ▼
              barco_limon [F3] ─► objetivo final: 3 caminos de estrellas
                                    (marae, F3) + navegación nocturna
```

Cada flecha es una dependencia dura de materiales o de estación, no de nivel
de personaje: no hay experiencia ni puntos de habilidad (GDD, sin cambios).
Se puede llegar a Nivel 3 en la Isla del Humo sin haber tocado nunca el
Nivel 2 de otra isla, porque los materiales, no el progreso abstracto, son
la única puerta.

---

## TODO de implementación

- [ ] [AA] `Carry`: fijar `BackpackComfortBonusKg` real para `mochila` (+8 kg), `mochila_fibra` (+10 kg) y `mochila_cuero_bambu` (+20 kg) en el sitio donde `UCarryComponent::SetCustomBackpack` recibe hoy solo volumen/peso del objeto.
- [ ] [AA] `Carry`/`InventoryModel`: implementar el apilado de hasta 10 unidades por hueco para objetos sin `maxDurability` ni `LiquidCapacityLiters` (§1.3); hoy `FInventoryEntry` es un objeto por hueco.
- [ ] [AA] `Items`: añadir a `Content/Data/items.json` las entradas `pico`, `lingote_cobre`, `lingote_hierro`, `alambre`, `clavos`, `sierra_diente_tiburon`, `vendaje_tela`, `antidoto_corteza`, `carbon_activado`, `te_corteza_sauce`, `ferula_bambu`, `gel_aloe`, `tela_fibra`, `carretilla`.
- [ ] [AA] `Items`/`Templates`: añadir la plantilla `pico` a `Content/Data/templates.json` (slots Cabeza/Mango/Unión, §2.1) — bloqueante para la minería manual de la porción vertical (GDD §6.1).
- [ ] [AA] `Carry`: nuevo `ECarrySlot`/actor `carretilla` (empuje `DosManos`, contenedor propio 40 L/25 kg, −30 % velocidad mientras se empuja, sin nadar/correr/escaleras con ella enganchada, §1.5).
- [ ] [AA] `Building`: añadir a `Content/Data/building_pieces.json` las piezas `cesta_almacen`, `estanteria_almacen`, `arcon`, `banco_chatarra`, `horno_fundicion`, `yunque` con el coste de §1.4 y §2.2.
- [ ] [AA] `Cooking`/`Fuels`: nuevo nivel de fuego `horno_fundicion` (heat 1.4) en `Content/Data/fuels.json`, y las recetas de fundición de §4.3 en un fichero nuevo `Content/Data/recipes_smithing.json` (mismo patrón que `recipes.json`).
- [ ] [AA] `GameMode`/`Carry`: confirmar en código (no solo en este documento) que `HandlePlayerDeath` nunca vacía el inventario en Explorador/Superviviente/Personalizado, y aplicar el golpe de ánimo −6 (`moraleEvents.Injured`, ya existe) al reaparecer.
- [ ] [AA] `Villages` (stub): dejar preparado el punto de extensión «reputación alta enseña una plantilla de herramienta de cobre» para cuando `Villages` exista en F3 (§2.3), sin bloquear el AA.
- [ ] [F2] `Building`: añadir `rail_recto`, `rail_curvo`, `cambio_agujas`, `vagon`, `torno_cuerda`, `ascensor_pozo`, `muralla_piedra`, `torre_defensa`, `cerca_estacas`, `empalizada`, `torre_vigia`, `gallinero`, `pocilga`, `corral` a `building_pieces.json` con el coste de §3.7 y §3.9.
- [ ] [F2] `Tramway` (nuevo módulo, GDD §3.5): grafo de vía + física de `vagon`, prototipo de PIE antes de comprometer alcance (riesgo ya señalado en GDD §3.5 y §8).
- [ ] [F3] `Boats`: verificar que `barco_limon` exige las 4 `requiresShipParts` (`Fuselage`, `Wing`, `Tail`, `Engine`) antes de permitir la receta, y que consume `canoa_balancin` como indica `requiresBoat`/`consumesRequiredBoat` (ya en `boats.json`, solo falta el mesh `SM_Limon` pendiente).
- [ ] [F3] `Ruins`/`Artifacts`: cablear el examen de `figura_navegante`, `figura_gemelos`, `figura_mira_cielo`, `carta_varillas`, `carta_oleaje`, `tapa_estrellas` a las técnicas de wayfinding de §3.10 (hoy `artifacts.json` no tiene ese vínculo; solo lo tiene `ruins.json` por sitio, no por artefacto individual).
- [ ] [AA] `Tests`: extender `Source/Explored/Tests/CarrySpec.cpp` con la carretilla y el apilado; extender `Tools/DataCheck` para validar que toda plantilla nueva de §2–4 es alcanzable con materiales de al menos una isla en AA (regla ya exigida por la biblia §12).
