# DataCheck

Validador de `Content/Data/*.json` (sin Unreal). Replica en Python la regla de
combinación de `UCraftingLibrary` (cada slot lo cubre al menos una de las dos
piezas; las propiedades de un compuesto son el máximo de sus piezas; gana la
plantilla con más slots y, a igualdad, la primera del fichero).

```bash
cd Tools/DataCheck
uv run datacheck            # informe; código de salida 1 si hay errores
uv run datacheck --strict   # los avisos también fallan
uv run datacheck --write-cooking   # regenera las tablas C++ del fuego y la cocina
uv run datacheck --root RUTA # valida otro checkout (los tests lo usan con repos temporales)
uv run pytest               # tests (datos reales + regresiones sintéticas)
uv run ruff check . && uv run basedpyright   # lint y tipos (CI)
uv run pytest -q --cov --cov-report=term-missing   # cobertura (mínimo 80 %)
```

Un JSON mal formado se informa como error (`JSON mal formado`) en vez de abortar con una
excepción, y los campos de tipo equivocado (texto donde va un número, ids nulos…) también.

Qué comprueba:

- **Esquema** de `items.json`, `templates.json`, `verbs.json`, `story_es.json`,
  `plants.json`, `building_pieces.json`, `survival_needs.json`, `artifacts.json`,
  `ruins.json`, `meshes_pendientes.json`, `achievements.json`, `fuels.json`, `recipes.json`,
  `boats.json`, `fish.json`, `music_layers.json`, `mining.json`, `fauna.json`, `fases_futuras.json` y `recipes_smithing.json` (campos, tipos, rangos: propiedades 0-5, pesos > 0, ids ASCII
  sin tildes…).
- **Referencias cruzadas**: resultados de plantillas, verbos, ingredientes y
  herramientas de construcción, objetos de siembra/cosecha, piezas requeridas.
  Si una planta tiene `birdsEat`, debe existir la pieza `espantapajaros`
  (`FFarmModel::ScarecrowRadius`, GDD §8.7).
- **Huerto (biblia 02 §10.1)**: `plants.json/rules` es espejo de las constantes de
  `FFarmModel` (fuera de estación, días secos hasta marchitarse y morir, lluvia por riego,
  radio del espantapájaros en metros, picoteo de las aves), y un cultivo que se planta con su
  propia cosecha y se arranca al cosechar debe dar al menos 2 (si no, la cosecha neta es nula). Todo cultivo
  cosecha comida (`recipes.json/foods`) o un objeto con `Medicinal ≥ 1`, y al menos uno es
  medicinal sin ser comida (GDD v3 §8.7: especias y plantas medicinales).
- **Minería (GDD v2 §3.4)**: cada estrato (tierra y arena, arcilla, caliza, basalto,
  obsidiana, cobre, hierro de meteorito, azufre, cristal) tiene su objeto en `items.json`.
  `mining.json`: materiales espejo de `ETerrainMaterial`/`FTerrainEditModel::MaterialInfo`
  (dureza, nivel mínimo, `SecondsPerPickaxeHit`, `TierBonus`, `MinHitsPerCubicMeter`) y
  golpes/m³ según la fórmula del GDD; estratos con objeto, islas de `EIslandArchetype`,
  capa, profundidad, fase y vetas finitas; niveles de herramienta 0–4 en los que cada
  cabeza produce de verdad un `pico` (no la captura `hacha`) y toda pieza que cabe como
  cabeza tiene nivel; progresión de picos sin ciclos y completa solo con islas de fase 1.
  Peligros (biblia 02 §2.4): derrumbe con una pieza de apoyo real sobre el terreno que cubre la
  luz máxima y avisa antes de caer, luces que existen, cada una con su ritmo de quema (`lightBurn`) y una `maxDurability` que gastar, aire viciado que nunca mata, sellado con
  un encaje del kit y crecida en una estación conocida. Lugares subterráneos (§2.5): acceso y
  nivel de entrada, fase 1 solo en las islas del acceso anticipado, objetos de estrato presentes
  en esa isla y fase, la cueva de Landing que se cava con la pala sin tocar un estrato más duro
  (GDD v2 §6.1), y los tubos de lava del Humo y las grutas marinas de Los Dientes (§6.2).
- **Progresión**: simula la fabricación desde los materiales en bruto y exige que
  toda plantilla sea alcanzable (no sombreada por otra) y que cada herramienta y
  pieza tenga una cadena finita desde el inicio (sin ciclos de requisitos).
- **Mallas**: `meshPath` de `/Engine/BasicShapes` o `SM_*` generados por
  `Tools/Blender/props`; todo marcador o `null` debe estar en
  `meshes_pendientes.json`, y nada obsoleto puede quedarse allí.
  Nota (no error) con las `SM_Base_*` de mobiliario sin ninguna pieza construible.
- **Espejo del C++**: `survival_needs.json` contra las constantes de
  `Source/Explored/Survival/SurvivalModel.{h,cpp}`; `ruins.json` y `artifacts.json`
  contra los ids de `LexToString` y las constantes de `Source/Explored/Ruins/*Model.{h,cpp}`
  (técnicas, elementos, ruinas por isla, caminos de estrellas, umbral de «Coleccionista»).
- **Museo**: cada tesoro tiene un tipo de `story_es.json`, una malla de `tesoros.py` y
  algún hueco de mueble donde cabe; los muebles apuntan a piezas `museo` existentes.
- **Logros (GDD §16, biblia 07 §2)**: entre 40 y 60 con los 30 del GDD siempre presentes,
  ids ASCII únicos, textos en ES y EN con la longitud de biblia 07 §1.3 en los nuevos, los
  ejemplos del GDD presentes, `phase`, `rarity` y `coopScope` válidos (biblia 08 §5.7),
  `strata_mined` igual a `mining.json → strata`, condiciones con estadísticas conocidas y de tipo
  compatible (la misma regla que `FAchievementsModel::Configure`), ids de conjuntos
  admitidos (listas cerradas o `items`/`plants`/`building_pieces`), metas alcanzables,
  y el catálogo de estadísticas igual al de `docs/tecnico/estadisticas.md`.
- **Fuego y cocina**: niveles fogata → hoguera → horno con piezas de construcción
  reales y mejoras crecientes; combustibles que de verdad arden; recetas con técnica,
  utensilio (estanco para hervir y guisar), capacidad, ingredientes y resultados
  existentes; toda comida de `items.json` con su entrada de conservación y el orden
  crudo < cocinado < ahumado, salado o seco (GDD §8.8). Los modelos puros no leen
  JSON: `--write-cooking` genera `Source/Explored/Cooking/FireData.inl` y
  `CookingData.inl` desde `fuels.json`, `recipes.json` e `items.json`, y la
  comprobación falla si el `.inl` no coincide con los datos.
- **Metal en estación** (`smithing.py`, biblia 03 §2.2 y §4.3): `fuels.json/smeltingLevels`
  (pieza de producción, cerrado, más calor, horas y brasas que el horno de arcilla,
  combustibles existentes, id que no sea de `EFireLevel`) y `recipes_smithing.json`
  (estación de producción, el fuego arde en su estación, sin estaciones con horno en
  frío, cantidades 1-20, ingredientes y resultados existentes).
- **Fuentes de los procesados**: lo que sale de una receta de `recipes.json` o de
  `recipes_smithing.json` solo es obtenible si alguna de sus recetas se puede hacer
  (punto fijo con las piezas construibles: el horno cuesta carbón, el yunque un
  lingote). Un `lingote` sin receta o un ingrediente de fundición sin veta, chatarra
  (`rescatado`) ni receta no cuenta como material en bruto, así que su falta arrastra a
  las piezas y recetas que lo piden. Las vetas que alimentan una fundición existen en
  alguna isla de fase 1; todo `mineral` sale de un estrato y todo `lingote` de una
  receta de fundición.
- **Guía anti-IA** (`textos.py`, biblia 07 §1): lista negra ES/EN, exclamaciones y emoji
  en nombres de objetos, plantillas, piezas, recetas de metal y logros; límites de
  §1.3 (nombre de 4 palabras sin puntuación final, descripción de una frase con 90/110
  caracteres) en los logros que no son de los 30 del GDD.
- **Barcos**: `boats.json` contra `EBoatType` y `FBoatDefinition::MeshName` (`Boats/`) y
  contra `EShipPart` (`Narrative/ExploredProgress.h`).
- **Astillero** (`boats.json`): progresión balsa → canoa → balancín → «Limón», estación
  existente, ingredientes y herramientas obtenibles, y el «Limón» con las cuatro piezas
  del Albatros y material rescatado (GDD §4.3, §8.10).
- **Pesca**: `fish.json` contra las tablas de `Source/Explored/Fishing/FishingModel.cpp`
  (especies, legendarias y trampas); 11 peces de caña + langosta, 5 legendarias, y que
  capturas, cebos, recompensas, trampas, pozas y despiece existan en `items.json`.
- **Música (GDD §14.3)**: `music_layers.json` contra `Audio/MusicDirectorModel.{h,cpp}`:
  papeles de `RoleKeys` (el lector ignora los desconocidos), los que exige
  `FMusicCatalog::Validate`, una pieza de exploración por isla de `IslandKeys`, variaciones
  diurnas (la propia primero, solo exploración), finales de `FinaleKey` con el de por defecto,
  bucles con compases enteros, `seconds_per_bar`/`duration_s` coherentes con el tempo,
  descubrimientos ≤ 5 s y la flauta con `FFluteModel::NumNotes` notas. Los finales distintos
  del «Limón» quedan como nota (GDD §2: sin finales narrativos).
- **Fauna salvaje (GDD v2 §3.7)**: `fauna.json` contra `EFaunaSpecies` (especie existente
  o un `cppSpeciesPropuesto` nuevo) y `FFaunaLodSettings` (radios, histéresis, intervalo);
  vida, percepción, huida, ataque con propiedad equivalente, rutina que cubre las 24 h,
  botín/nidos con objetos reales; las cuatro islas del acceso anticipado con ficha y
  ninguna isla de fase 1 con especies de fase 2/3. Especies sin malla, en
  `meshes_pendientes.json/fauna`.
- **Borradores de fase 2 y 3** (`fases_futuras.json`, GDD v2 §6.2): raíles y vagones,
  animales domésticos, murallas y trampas, y trueque con reputación. Todo con `fase` 2 o 3;
  objetos del catálogo o declarados en `pendingItems`; ningún otro fichero de datos (fase 1)
  nombra un id que solo existe en el borrador; sin claves de precio ni moneda (§5), valores
  de trueque 1–5 y tramos de reputación contiguos de 0 a 100 con tasa creciente.
- **Contenido de texto de H4** (`contenido.py`, `docs/tecnico/contenido-textos.md`):
  `halden_diaries.json` (los 5 cuadernos de biblia 04 §7.1, cada uno en un POI que de verdad
  coloca `PointsOfInterest.cpp` en esa isla y con su `ContentId`), `journal_entries.json`
  (al menos 15 entradas, `{Day}` en los dos idiomas, disparadores con el lenguaje de condiciones
  de `achievements.json` o un suceso declarado y usado, sin sucesos de una fase posterior),
  `map_clues.json` (tesoros raros o únicos, escondite que cuadra con la procedencia, fuente en una
  ruina o un cuaderno de la misma isla), `ruins.json` (`teaches`/`starPathTarget`: cada técnica
  global una vez, caminos a islas distintas de la propia, al menos `requiredStarPaths` y uno a la
  isla oculta) y el museo (`museum_collections.json` con sus ficheros, recuentos, muebles
  `museo` existentes y usados, y fase de cada pieza coherente con sus islas y su colección).
  Islas: `LexToString(EIslandArchetype)` en minúsculas; POI: `EPoiType`.
- **Guía anti-IA** (`estilo.py`, biblia 07 §1): lista negra ES/EN, exclamaciones, emoji, puntos
  suspensivos, rayas de muletilla y límites de longitud y de frases por tipo de texto en los
  ficheros de contenido; en los logros, todo menos la longitud (hay textos fijados que se pasan).
- **Reglas del GDD §12**: sin narrativa eliminada en los datos; la fauna terrestre que
  recupera el GDD v2 (cerdo, cabra, aves posadas) ya no es término prohibido.
- **Cobertura del GDD §8.8** (nota, no error): comida de recolección y marisqueo que
  falta en `items.json` y setas por tipo (2 comestibles, 2 tóxicas por `Toxico`,
  1 con etiqueta `alucinogena`). Ver `docs/balance/2026-09-27-comida-recoleccion.md`.
