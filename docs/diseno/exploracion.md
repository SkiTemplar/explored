# Exploración de las islas — diseño de contenido

Versión 1 · 2026-09-27 · Complementa el GDD (`docs/superpowers/specs/2026-09-26-explored-design.md`
§4) y la biblia de contenido (`docs/design/biblia-de-contenido.md`).

Objetivo: que cada isla tenga motivos concretos para recorrerla entera, no solo para
sobrevivir en ella. Se apoya en lo que ya coloca `FPoiLayout::Generate`
(`Source/Explored/WorldGen/PointsOfInterest.cpp`), en las ruinas de `FRuinsLayout`
(`Source/Explored/Ruins/RuinsModel.h`) y en el kit de rocas nuevo
(`Tools/Blender/props/rocks_cliffs.py`, manifiesto en `Art/Export/Props/manifest.json`).

Convención de posición: **ángulo en grados** (sentido horario desde +X, como
`Island.Rotation`) y **distancia como fracción del radio** de la isla (`Island.Radius`),
igual que reciben `FPoiLayout::FindBeach`/`FindInland`. Así cualquier posición de este
documento se resuelve de forma determinista sobre el terreno real sin guardar
coordenadas de mundo, que cambiarían si se retoca la generación.

---

## 1. Por isla

Las columnas «Ya existe» marcan puntos que `FPoiLayout`/`FRuinsLayout` ya colocan hoy
(EPoiType, ContentId o id de ruina); «Nuevo» son propuestas de este documento, sin
colocar todavía.

### 1.1 Isla del Amaraje (Landing) — inicio seguro

Tono: la playa donde se estrelló el Albatros; protegida, generosa, el primer contacto
con el archipiélago. **Porción vertical prioritaria — ver §3.**

| Lugar | Tipo | Contenido | Cómo se llega | Recompensa | Posición | Estado |
|---|---|---|---|---|---|---|
| Laguna del Amaraje | naufragio | Fuselaje del Albatros hundido | Bucear en el centro de la laguna protegida | Piezas para el barco «Limón» | ángulo 0°, 0,58R | Ya existe (`WreckFuselage`) |
| Playa del Ala Rota | naufragio | Ala varada en la orilla opuesta | Caminar por la costa opuesta a la laguna | Mochila de fibra, tela de paracaídas | 180°, 1,0R | Ya existe (`WreckWing`) |
| Poza de la Marea Baja | poza | Marisco sin depredador | Solo con la bajamar | Cangrejos, erizos, pulpo | 90°, 1,0R | Ya existe (`TidePool`) |
| Mirador de la Cresta | mirador | Vista de la isla y de las cercanas | Subir a la cima | Boceto de contorno en el mapa | cima (búsqueda de altura, sin fracción fija) | Ya existe (`Viewpoint`) |
| Marae del Palmeral | ruina | Plataforma, estatua, altar, 2 petroglifos | Tierra adentro, entre las palmeras | Una técnica de wayfinding | ~250°, 0,4R (aprox.; el ancla real la fija `FRuinsLayout::Generate`) | Ya existe (`ruin_landing`) |
| Cueva del Sextante | cueva | Sextante (instrumento de precisión, GDD §5.5) | Entrada semihundida junto a la laguna; solo con marea baja | Coordenadas exactas en mar abierto | ~30°, 0,65R | **Nuevo** |

Hito visible desde lejos: la copa de palmeral sobre la laguna con el fuselaje asomando
en el agua — silueta reconocible desde el mar y desde el mirador de cualquier isla
vecina.

Microdetalle: huellas desde la playa hacia el interior, equipaje disperso junto al
ala, jirones de paracaídas enganchados en las palmeras.

### 1.2 Esmeralda — selva y ruinas rituales

Tono: densa, húmeda, el primer contacto con madera dura y con el pueblo navegante.

| Lugar | Tipo | Contenido | Cómo se llega | Recompensa | Posición | Estado |
|---|---|---|---|---|---|---|
| Cascada de los Tres Saltos | cascada | Cueva tras la cortina de agua | Nadar contracorriente hasta la base | Acceso a la cueva y al petroglifo | boca de la cueva (`FCaveDesc` de la isla) | Ya existe (`Waterfall`) |
| Cueva de la Cascada | cueva | Petroglifo «petro_01» | Cruzar detrás de la cascada | Un petroglifo (wayfinding) | 70 % de la cueva | Ya existe |
| Campamento Halden I | campamento | Restos de la expedición de 1974 | Tierra adentro, ladera este | Recursos rescatados, cuerda, herramientas | Rotation+2,2 rad, 0,55R | Ya existe (`camp_halden_1`) |
| Motor del Albatros | naufragio | Motor con piezas y cable | Interior, terreno más alto | Piezas para el barco «Limón» | Rotation−0,8 rad, 0,8R | Ya existe (`WreckEngine`) |
| Marae de la Cascada | ruina | Estatua, altar, canoa o cueva ritual según semilla | En el interior boscoso | Una técnica de wayfinding | ancla de `FRuinsLayout` | Ya existe (`ruin_emerald`) |
| Mirador | mirador | Vista de la isla | Cima | Boceto de contorno | cima | Ya existe |

Hito visible: los árboles gigantes por encima del dosel y el penacho de espuma de la
cascada en tres saltos.

Microdetalle que falta: puente de lianas decorativo (no interactivo) visible desde
abajo, marcas de vadeo en las piedras del río.

### 1.3 Isla del Humo — volcán

Tono: hostil y mineral; obsidiana, azufre y la cima ritual más importante del
archipiélago.

| Lugar | Tipo | Contenido | Cómo se llega | Recompensa | Posición | Estado |
|---|---|---|---|---|---|---|
| Brújula Estelar del Humo | cumbre | Monumento de piedra con surcos radiales | Subir al cráter | Camino de estrellas hacia la isla oculta (siempre) | cima | Ya existe (`StarCompass`, `ruin_compass`) |
| Aguas Termales | recurso | Recupera temperatura y ánimo | Interior, ladera oeste | Curación pasiva | Rotation−1,2 rad, 0,85R | Ya existe (`HotSpring`) |
| Campamento Halden II | campamento | Restos de expedición | Interior, ladera este | Recursos rescatados | Rotation+0,6 rad, 0,75R | Ya existe (`camp_halden_2`) |
| Marae del Tubo de Lava | ruina | Cueva ritual bajo el volcán | Tubo de lava (cueva volcánica ya generada) | Una técnica de wayfinding | ancla de `FRuinsLayout` | Ya existe (`ruin_smoke`) |
| Campo de Obsidiana | recurso | Vetas de obsidiana a la vista | Ladera cerca del cráter, terreno negro | Obsidiana (herramientas nivel 3) | ~200°, 0,7R | **Nuevo** |

Hito visible: el penacho de humo del cráter, visible desde cualquier punto alto del
archipiélago — la señal de humo más fiable para orientarse.

### 1.4 Los Dientes — islotes y acantilados

Tono: rocoso, batido por el mar; el faro como objetivo de reparación emergente.

| Lugar | Tipo | Contenido | Cómo se llega | Recompensa | Posición | Estado |
|---|---|---|---|---|---|---|
| Faro en Ruinas | mirador | Torre y linterna rotas, en el islote más alto | Escalar el islote más alto (`FindSummit`) | Objetivo de reparación (baliza, GDD) | islote más alto | Ya existe (`Lighthouse`) |
| Arco del Centinela | arco_marino | Arco tallado por el oleaje | Rodear en canoa o a nado en bajamar | Vista y atajo entre dos calas | boca del `FCaveDesc` tipo arco ya generado en Teeth | **Nuevo** (usar `SM_SeaArch01`) |
| Acantilado de los Nidos | recurso | Nidos vacíos de aves marinas | Trepar la pared marcada de grietas | Plumas, huevos, guano | pared exterior, borde de isla | **Nuevo** |
| Marae de los Acantilados | ruina | Estatua mirando al mar | Entre las rocas, terreno llano escaso | Una técnica de wayfinding | ancla de `FRuinsLayout` | Ya existe (`ruin_teeth`) |
| Mirador | mirador | Vista de todo el archipiélago | Cima de un islote secundario | Boceto de contorno | cima | Ya existe |

Hito visible: el faro roto sobre la silueta dentada de los islotes, y el arco marino
recortado contra el horizonte — el hito más reconocible del archipiélago.

### 1.5 Manglar de las Voces — niebla y peligro

Tono: opresivo, bioluminiscente de noche; el manglar penaliza la prisa.

| Lugar | Tipo | Contenido | Cómo se llega | Recompensa | Posición | Estado |
|---|---|---|---|---|---|---|
| Estación de Radio | campamento | Radio abandonada de Halden | Interior, entre raíces | Recursos, pista narrativa | Rotation+1,8 rad, 0,35R | Ya existe (`RadioStation`) |
| Marae de las Raíces | ruina | Ruina semienterrada en barro | Vadear el barro que atrapa | Una técnica de wayfinding | ancla de `FRuinsLayout` | Ya existe (`ruin_mangrove`) |
| Poza de Arcilla | recurso | Barro apto para cerámica | Orilla de un canal | Arcilla (horno de cerámica) | ~140°, 0,5R | **Nuevo** |
| Raíces Bioluminiscentes | vista | Bioluminiscencia máxima en luna nueva | Canal interior, de noche | Vista, sin recurso | ~60°, 0,45R | **Nuevo** |
| Mirador | mirador | Vista limitada por la niebla | Cima (poca altura) | Boceto de contorno parcial | cima | Ya existe |

Hito visible: ninguno a distancia — es la única isla que se reconoce por su **ausencia**
de silueta (niebla perpetua), lo que la hace fácil de confundir de noche a propósito.

### 1.6 Arenas Blancas — atolón

Tono: postal, pero con un pecio y una playa de desove que le dan textura.

| Lugar | Tipo | Contenido | Cómo se llega | Recompensa | Posición | Estado |
|---|---|---|---|---|---|---|
| Pecio del Velero | naufragio | Casco, mástil, vela, ancla | Nadar o bucear junto a la costa | Lona, cuerda, metal, tesoros | Rotation+0,5 rad, borde+60 m | Ya existe (`Shipwreck`) |
| Playa de las Tortugas | cala | Desove en luna llena | Costa este | Evento estacional, huevos | Rotation+2,5 rad, 1,0R | Ya existe (`TurtleBeach`) |
| Campamento Halden III | campamento | Restos de expedición | Costa oeste | Recursos rescatados | Rotation−1,6 rad, 1,0R | Ya existe (`camp_halden_3`) |
| Marae de las Canoas | ruina | Canoa doble fosilizada | Semienterrada en la arena | Una técnica de wayfinding | ancla de `FRuinsLayout` | Ya existe (`ruin_whitesands`) |
| Jardín de Coral | jardin_coral | Arrecife de colores, peces de colección | Bucear en la laguna del atolón | Conchas raras, fotos para el catálogo | interior de la laguna | **Nuevo** |

Hito visible: el mástil del pecio asomando sobre el arrecife, con el anillo turquesa
del atolón reconocible desde el aire de cualquier mirador.

### 1.7 La Meseta — macizo kárstico

Tono: caliza gris tipo El Nido/Ha Long (§2), viento constante, farallones sueltos.

| Lugar | Tipo | Contenido | Cómo se llega | Recompensa | Posición | Estado |
|---|---|---|---|---|---|---|
| Observatorio de Mareas | campamento | Instrumental de Halden | Interior, terreno alto | Tabla de mareas completa | Rotation+0,3 rad, 0,3R | Ya existe (`TideObservatory`) |
| Campamento Halden IV | campamento | Restos de expedición | Interior | Recursos rescatados | Rotation+2,6 rad, 0,6R | Ya existe (`camp_halden_4`) |
| Marae de la Meseta | ruina | Ruina en terreno kárstico | Ladera menos empinada | Una técnica de wayfinding | ancla de `FRuinsLayout` | Ya existe (`ruin_mesa`) |
| Farallones del Paso | vista | Agujas de caliza en el agua (`SM_SeaStack01/02`) | Rodear en canoa | Hito de navegación, sin recurso | costa exterior, en cayos rocosos | **Nuevo** (ya hay `FCayDesc.bRocky` en esta isla) |
| Mirador de la Cumbre | mirador | Vista del macizo entero | Cima (pendiente pronunciada) | Boceto de contorno | cima | Ya existe |

Hito visible: la silueta quebrada del macizo con farallones sueltos en el agua —
inconfundible frente al resto de islas, todas de perfil más suave.

### 1.8 La isla oculta

Arrecife fósil sin puntos de interés propios: objetivo final opcional, solo alcanzable
de noche guiándose por los `FRuinsLayout::RequiredStarPaths` (3) caminos de estrellas
reunidos. No lleva contenido explorable adicional a propósito — es la recompensa por
haber explorado el resto.

---

## 2. Bucle de exploración

Qué empuja a la siguiente isla, sin texto largo — solo señales visuales y progresión
de herramientas/embarcaciones (`Content/Data/boats.json`: `balsa` → `canoa` →
`canoa_balancin` → `barco_limon`):

| Fase | Habilita | Señal que lleva a la siguiente isla |
|---|---|---|
| Playa de Landing | Herramientas Tosco | Penacho de humo del Humo, visible desde el mirador de Landing |
| Balsa (aguas someras) | Cruzar a Esmeralda/canal corto | Bandadas que regresan al atardecer hacia la isla más próxima (GDD §6.2 «Aves al atardecer») |
| Canoa (rápida) | Mar abierto corto | El faro roto de Los Dientes, visible de noche si se repara parcialmente |
| Canoa con balancín y vela | Mar abierto largo | Nube fija sobre el Humo o la Meseta (técnica «Nubes fijas») |
| 3 técnicas de wayfinding | Lectura del mapa a mano completo | El propio mapa: huecos evidentes donde aún no hay trazo de costa |
| Barco «Limón» + 3 caminos de estrellas | Final opcional | Noche despejada; sin marcador, solo el cielo |

Cada isla, además, se anuncia desde el mar con su propio hito (§1); no hace falta un
marcador de misión porque la silueta ya cumple esa función.

---

## 3. Priorización (porción vertical)

**Landing ya tiene 5 de 6 lugares colocados por el código actual** (§1.1): laguna,
playa del ala, poza de marea, mirador y el marae. Para la porción vertical, en este
orden:

1. **Nombrar y documentar** lo que ya existe (hecho en este documento y en
   `Content/Data/exploration.json`) — sin coste de motor.
2. **Cueva del Sextante**: el único lugar nuevo de Landing. Requiere una entrada de
   `FPoiLayout` (tipo `EPoiType` nuevo o reutilizar `Bottle`-style genérico) y el prop
   del sextante — no se ha tocado `PointsOfInterest.cpp` en este cambio porque otro
   agente está migrando WorldGen a World Partition; queda para cuando ese trabajo
   termine.
3. **Microdetalle de Landing** (huellas, equipaje disperso, paracaídas en las
   palmeras): assets baratos, alto impacto en la primera impresión.
4. Repetir el mismo patrón en Esmeralda y Los Dientes (las siguientes dos islas del
   bucle), que ya tienen la mayoría de sus POIs colocados.
5. Dejar para después: Manglar y Meseta (menos POIs nuevos propuestos, pero más caros
   de construir — arcilla, farallones) y el jardín de coral de Arenas Blancas (pide
   buceo, un sistema ya avanzado).

---

## 4. Implementación con mínimo código nuevo

### 4.1 Esquema de datos (implementado)

`Content/Data/exploration.json`: catálogo de lugares con nombre por isla, sin tocar la
colocación procedural. Cargado por `Exploration/ExplorationContentLoader.cpp` (usa el
módulo Json) hacia el modelo puro `FExplorationCatalog`
(`Source/Explored/Exploration/ExplorationContentModel.h`, solo `CoreMinimal.h`):

- `FExplorationLandmark`: id, isla, tipo, nombres ES/EN, posición (ángulo + fracción de
  radio), `linkedPoi` (ContentId de un punto ya colocado, o vacío si es nuevo),
  `mapMark` (sello de `CartographyModel.h`: `water`/`cave`/`danger`/`resource`/`ruin`/`wreck`),
  cómo se llega y qué da — todo en frases cortas, sin lore largo (pilar del GDD).
- `FExplorationCatalog::Validate`: ids únicos, isla/tipo/sello conocidos, nombres y
  textos no vacíos, ángulo en `[0, 360)` y distancia en `[0, 1.2]` radios.

Implementado solo para Landing (§3, punto 1). Ampliar a las otras islas es añadir
entradas al JSON: no hace falta tocar C++.

### 4.2 Tests (implementados)

- `Source/Explored/Tests/ExplorationContentSpec.cpp`: reglas puras de
  `FExplorationCatalog::Validate` y de las consultas (`FindLandmark`,
  `CountForIsland`) con catálogos construidos a mano — añadido a
  `Tools/HostTests/pure_specs.txt` y su modelo a `pure_sources.txt` (corre sin editor).
- `Source/Explored/Tests/ExplorationContentDataSpec.cpp`: carga real de
  `exploration.json` en el editor (como `RuinsDataSpec.cpp`) y comprueba que Landing
  tiene al menos 3 lugares — no va en `pure_specs.txt` porque el parseo JSON no
  compila contra el shim del host (mismo motivo que `RuinsDataSpec.cpp`).

### 4.3 Reglas de colocación del kit de rocas (propuesta, sin implementar)

No se ha tocado `TerrainDensity.*` ni la vegetación (fuera de alcance de este cambio).
Reglas para cuando se integre el scatter de `Tools/Blender/props/rocks_cliffs.py`:

| Grupo | Regla de colocación | Dónde engancha |
|---|---|---|
| `AcantiladoFormaciones` (`CliffWall_*`, `CliffSpur_*`) | Donde la normal del terreno forme más de 55° con la vertical **y** `Column.Height` esté en el mismo rango que ya usa `RockMask` de `TerrainDensity.cpp` (`SmoothStep(3, 18, Height)`, paredes rocosas por encima de la playa) | Mismo criterio de pendiente que ya filtra el voladizo (`OverhangAmplitude`); no requiere nueva señal, solo un nuevo paso de scatter que lo lea |
| `SeaStack01/02` (farallones) | En los `FCayDesc` con `bRocky = true` que `GenerateCays` ya crea para Humo y Meseta (`ArchipelagoLayout.cpp`), sustituyendo o completando el cono procedural del cayo | Isla → `FCayDesc` (ya existe, sin cambios de datos) |
| `SeaArch01` (arco marino) | En las mismas posiciones donde `TerrainDensity::BuildCaves` ya genera un `FCaveDesc` de tipo arco para Los Dientes (`Count = 2`, `Arch.Start/End` a ±40 m del cabo) — remate visual sobre el hueco ya tallado en el SDF | Los Dientes, sin nuevas posiciones que inventar |
| `AcantiladoBloques` (`RockBoulder*`, `RockCobble*`, `LimestoneSlab*`) | Dispersión de detalle en la base de las paredes anteriores y en playas rocosas (Los Dientes, Meseta), con el mismo generador de ruido que ya usa `VegetationScatter` para evitar un segundo sistema de scatter | Extiende `VegetationScatter.cpp` (no tocado en este cambio) |

Tests que haría falta al implementarlo (no escritos, fuera de alcance): en
`VegetationScatterSpec.cpp`, comprobar que ninguna roca del kit aparece bajo el agua
salvo el arco de Los Dientes, y que la densidad de `AcantiladoBloques` cae a cero
lejos de una pared o de un `FCayDesc.bRocky`.

### 4.4 Microdetalle que falta (props no encontrados en `Tools/Blender/props/`)

- Huellas de pisada (decal o malla plana) hacia el interior desde las playas.
- Restos de red de pesca varada, como decoración (distinta de la nasa utilizable).
- Tramos de cuerda tirada y anillo de fogata apagada, para vestir campamentos Halden.
- Pila de conchas/madera de deriva como mobiliario ambiental no interactivo (hoy solo
  existen versiones recogibles en `items_orilla.py`/`items_materiales.py`).
