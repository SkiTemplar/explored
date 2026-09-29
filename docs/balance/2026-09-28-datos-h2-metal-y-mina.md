# Datos de H2: metal, piezas de mina y escalada, logros de minería

Fecha: 2026-09-28. Casillas de H2 en `docs/diseno/biblia/00-TODO.md`. Solo datos
(`Content/Data`), su validación (`Tools/DataCheck`) y esta documentación; el código de
juego que los usa sigue pendiente y tiene sus casillas propias en el TODO.

## 1. Qué entra

| Fichero | Qué se añade | Fuente |
|---|---|---|
| `items.json` | `lingote_cobre`, `lingote_hierro`, `lingote_aluminio`, `alambre`, `clavos`, `clavija_roca`, `carretilla` | biblia 03 §1.5, §3.2–3.3; 02 §13.4 |
| `templates.json` | `clavija_roca` (Tallar) y `carretilla` (Atar, en `piedra_trabajo`) | biblia 02 §13.4; 03 §1.5 |
| `building_pieces.json` | `horno_fundicion`, `yunque`, `viga_apoyo`, `tablon_contencion`, `escalera_mano`, `cuerda_fija` (`banco_chatarra` ya estaba) | biblia 03 §2.2; 02 §2.7, §5.3, §13.3 |
| `fuels.json` | `smeltingLevels`: `horno_fundicion`, heat 1.4, solo carbón vegetal | biblia 03 §2.2 |
| `recipes_smithing.json` (nuevo) | 9 recetas: fundir cobre, hierro, chapa y tubo; batir chapa y tubo; estirar alambre; clavos de alambre y de hierro | biblia 03 §4.3 |
| `achievements.json` | 7 estadísticas de minería; `primera_palada`, `buscador_de_vetas`, `filo_de_obsidiana`, `topo_de_isla`, `el_aire_que_falta`, `viga_a_tiempo`, `manazas`, `el_cangrejo_se_lo_llevo` | biblia 07 §2.1, §2.3 |
| `fases_futuras.json` | Salen `lingote_hierro` y `clavos` de `pendingItems`: ya existen | — |

`el_cangrejo_se_lo_llevo` no estaba en el encargo, pero `crab_stole_item` sí, y una
estadística que no usa ningún logro es un aviso de DataCheck (falla con `--strict`).

## 2. Cuánto rinde el metal

Con `mining.json` (6 unidades por m³, vetas finitas) y las recetas nuevas:

| Cadena | Entra | Sale | Tiempo de estación |
|---|---|---|---|
| Carbón | 1 `tronco_pequeno` en `horno_barro` | 2 `carbon_vegetal` | 240 min |
| Cobre | 2 `mineral_cobre` + 1 carbón | 1 `lingote_cobre` | 30 min |
| Hierro | 2 `hierro_meteorito` + 2 carbón | 1 `lingote_hierro` | 45 min |
| Aluminio (horno) | 1 chapa o tubo + 1 carbón | 1 `lingote_aluminio` | 20 min |
| Aluminio (banco) | 1 chapa o tubo, martillo | 1 `lingote_aluminio` | 20 min |
| Alambre | 1 `lingote_cobre`, martillo | 4 `alambre` | 10 min |
| Clavos | 1 `alambre` o 1 `lingote_hierro`, martillo | 8 `clavos` | 10 min |

- **Una veta de cobre** (10 golpes, vuelve a los 20 días) da **5 lingotes**, y un
  lingote de cobre da 32 clavos por la vía del alambre.
- **Un cráter de hierro** (4 golpes, no vuelve) da **2 lingotes**. El yunque cuesta
  uno: el primer cráter de Los Dientes paga el yunque y deja un lingote libre.
- **Construir el horno de fundición** cuesta 4 carbones, o sea 2 troncos y 8 h de
  horno de barro. Es el precio de entrada a la edad del metal.
- **Pregunta abierta de balance:** con las cifras de biblia 03 §4.3, un lingote de
  hierro da 8 clavos y uno de cobre da 32 (vía alambre). Los clavos de hierro solo
  tienen sentido si los de cobre son peores (menos integridad en las piezas, o que
  los raíles de F2 pidan clavos de hierro). Se deja tal cual hasta que haya piezas que
  gasten clavos en el acceso anticipado; hoy solo los usa el borrador de F2.

## 3. Red y cooperativo (biblia 08)

| Mecánica | Autoridad y replicación |
|---|---|
| Recetas de `recipes_smithing.json` | Como fabricar (§2.4): el cliente pide, el servidor comprueba estación, fuego, herramientas y materiales en **su** copia del inventario, consume y entrega. El avance de una tanda es estado de la estación en el servidor; los clientes cercanos ven llama y humo por la misma vía que las hogueras. |
| Nivel de fuego `horno_fundicion` | Igual que los demás niveles de `FFireModel`: combustible, brasas y apagado solo en el servidor. |
| `horno_fundicion`, `yunque`, `viga_apoyo`, `tablon_contencion`, `escalera_mano`, `cuerda_fija` | Piezas de construcción (§2.10): `Server_PlacePiece`, fantasma local, derrumbe y anclaje de arena solo en el servidor. Actor estático, ≈ 40 B al aparecer y 0 B después. |
| `carretilla` | Contenedor propio que se empuja: su contenido va como el de un contenedor del mundo (§2.4, solo se replica a quien lo abre); quién la empuja y su posición, como un objeto `DosManos` en las manos (6 B). |
| `clavija_roca` clavada | Edición menor del terreno: sale por la cola de deltas de §2.2, la misma que el picado. |
| Logros nuevos | `coopScope` de §5.7 ya en el JSON: `actor` para todos salvo `el_aire_que_falta` (`witness`, 50 m). Las estadísticas son de cada jugador. |

## 4. Reglas nuevas de DataCheck

- **Fuentes** (`smithing.py`): un objeto que sale de una receta de `recipes.json` o de
  `recipes_smithing.json` solo es obtenible si alguna de sus recetas se puede hacer
  (ingredientes, estación, fuego, herramientas y utensilios). Se resuelve por punto
  fijo porque el horno cuesta carbón y el yunque cuesta un lingote. Un `lingote` sin
  receta, o un ingrediente de fundición que no sale de veta, chatarra ni receta, no
  cuenta como recogible del suelo. Así caen en cadena el yunque, los clavos y el
  alambre si se borra la receta del hierro, y los hornos si falta la del carbón.
- **Vetas del acceso anticipado:** un ingrediente de fundición que sale de una veta
  tiene que aparecer en alguna isla de fase 1 (`mining.json`).
- **Minerales y lingotes:** todo `mineral` sale de un estrato de `mining.json`; todo
  `lingote`, de una receta de fundición.
- **Nivel de fundición:** pieza de producción existente, cerrado, por encima del horno
  de arcilla en calor, horas de combustible y brasas, combustibles de `fuels.json`, y
  nunca con un id que ya sea de `EFireLevel`.
- **Recetas de metal:** estación de producción, el fuego arde en su propia estación, una
  estación con horno no se usa en frío, cantidades enteras de 1 a 20, ningún
  ingrediente que sea su propio resultado, entradas corruptas sin reventar.
- **Logros:** los 30 del GDD §16 siempre presentes y como mucho 54 (biblia 07 §2);
  `strata_mined` igual a `mining.json → strata`; `coopScope` válido y obligatorio en
  los logros nuevos; condiciones con ids que no son cadenas se rechazan en vez de
  romper el comprobador.
- **Guía anti-IA** (`textos.py`, biblia 07 §1): lista negra ES/EN, exclamaciones y emoji
  en nombres de objetos, plantillas, piezas, recetas de metal y logros; y los límites de
  §1.3 (4 palabras de nombre, 90/110 caracteres de descripción, una frase) en los
  logros nuevos. Los 30 antiguos conservan su texto, como dice biblia 07 §2.2.

La regla nueva cazó dos nombres de la propia biblia 07 con 5 palabras: «El cangrejo se
lo llevó» pasa a «Cangrejo ladrón» y «The Air That Ran Out» a «Running Out of Air».

## 5. Verbos escondidos: un fallo previo que los lingotes destaparon

`UCraftingLibrary::FindActionsWithData` ofrece como mucho 3 verbos, en el orden de las
plantillas de `templates.json`, y el cuarto desaparece sin aviso. Con Rígido 4 los
lingotes disparaban solos `lasca_por_golpeo` (Golpear), y con un mango atado Atar
quedaba cuarto: no había hacha de metal. Los lingotes quedan en Rígido 3 (el metal no se
talla a golpes como un núcleo de sílex) y la clavija de hierro pide Contundente ≥ 4.

El fallo de fondo ya existía: con un mango de `madera_dura` atado con cuerda, Golpear
(la madera dura hace de núcleo), Tallar y Trenzar ocupan los tres huecos y Atar no sale
nunca, ni con obsidiana ni con pedernal. DataCheck lo lista ahora como nota y queda una
casilla nueva de `Crafting` en H2 para arreglarlo en el C++.
