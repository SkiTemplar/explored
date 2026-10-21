# Inventario de poda del reset v2

> **Superado en parte:** `docs/v3/plan.md` §3 sustituye a este inventario donde difiere (veredictos de módulos, `Content/` y `Tools/`). Lo que no contradice al plan sigue vigente como detalle fichero a fichero.

Fecha: 2026-10-18 · rama `reset/v2` · referencia de alcance: `docs/reset/vision.md` y la
lista de cortes acordada (sin red, combate, aldeas, piratas, tranvía, granja, minería,
terraformado, barcos por piezas, logros, museo ni HUD; mundo de kit curado y no procedural;
sin barco hidrostático complejo).

Criterio de veredicto:

- **CONSERVAR**: el núcleo se queda intacto; como mucho se retira un fichero de red o una
  llamada a un sistema borrado.
- **ADAPTAR**: se queda parte del módulo; se borran ficheros o se reescribe la capa de Unreal.
- **BORRAR**: el módulo entero sale del árbol (recuperable desde el historial de git).

Antes de tocar nada: etiqueta `pre-reset-v2` en el commit actual y copia comprimida fuera
del repositorio de todo lo que no está en git (ver riesgo R3).

---

## 1. Código: `Source/Explored` y `Source/ExploredEditor`

Recuento: **6 CONSERVAR · 20 ADAPTAR · 7 BORRAR** (33 entradas, `ExploredEditor` incluido).

«Incluido por» lista los módulos de fuera (sin contar `Tests`) que hacen `#include` del
módulo; el número es la cantidad de ficheros.

| Módulo | Veredicto | Motivo | Incluido por (fuera de Tests) |
|---|---|---|---|
| Achievements | BORRAR | Logros fuera del primer lanzamiento; toasts y panel son HUD | Boats 1, Building 1, Cooking 1 (`ExploredFire`), Core 1 (Wiring), Farming 1, Fishing 2 (`ExploredTrap`, `FishingComponent`), Player 1, Save 1 (`SaveSystemStates.h`), UI 3 (`PlayerController`, `ScreensLogic.h`, `SExploredAchievements`) |
| Audio | ADAPTAR | Ambiente y director musical sirven; quitar capas de sistemas borrados (mina, aldea) | Core 1, Narrative 1, UI 1, Weather 1 |
| Boats | ADAPTAR | Conservar `BoatModel` (viento, vela) simplificado y `ExploredBoat`; borrar `HullAssemblyModel`, `RaftYardModel`, `RaftFurrowModel` (barcos por piezas, surco de arena) | Core 2 (Wiring, `SystemLinks.h`), `ExploredGameMode.cpp`, Save 1 (`SaveSystemStates.h`) |
| Building | BORRAR | Construcción libre cortada; el Limón por 4 piezas vive en Narrative | Carry 1 (`CarryComponent.h` usa `BuildingTypes.h`), Core 2, `ExploredGameMode.cpp`, Player 1 (`BuildPreviewComponent`), Save 1 |
| Carry | ADAPTAR | `InventoryModel` es la base de la página Mochila; borrar `ContainerReplicationModel` (red) y `ExploredSledge` | Building, Cooking 2, Core, Debug 2, Farming 2, Fishing, Player 3, Ruins, Save, UI 3, Weather |
| Cartography | ADAPTAR | Mapa a mano ya existe (`CartographyModel`, `MapStroke`, `CoastlineTrace`); falta llevarlo al cuaderno y desligar `CoastlineTrace` de `TerrainDensity` | Core, Narrative, Player, Save, UI 4 |
| Combat | BORRAR | Sin combate | Ninguno |
| Cooking | CONSERVAR | Fuego y cocina son soporte del cuerpo legible; quitar la llamada a Achievements de `ExploredFire` | Core, `ExploredGameMode.cpp`, Player, Save |
| Core | ADAPTAR | Conservar `ExploredRandom`, `ExploredNoise`, `ExploredLocalization`; borrar `NetQuantize`; recortar `SystemLinks` y reescribir `ExploredWiringSubsystem` | 20 módulos (base común) |
| Crafting | ADAPTAR | Combinación objeto + verbo; reducir a las pocas recetas de la visión (fuego, balsa) | Items 2, Player, UI |
| Debug | ADAPTAR | `PlaytestBot`, `PlaytestAuditor`, `ShotSubsystem` y `PlaytestReportModel` sirven para probar sin HUD (R9); borrar `NetBudgetModel` y `NetBudgetTableModel` | Ninguno |
| Events | ADAPTAR | Eventos deterministas (tortuga, tormenta, ciclón) sirven; recortar tipos sin uso | Core 3, Save |
| Exploration | ADAPTAR | Catálogo narrativo de lugares; pasar de `FPoiLayout` procedural a lugares de autor | Ninguno |
| Farming | BORRAR | Granja cortada | Save 1 (`SaveSystemStates.h`) |
| Fauna | ADAPTAR | Boids, marcha procedural y cerebro marino sirven; borrar `FaunaAnchorNetModel` (red); faltan perro, gallina, jabalí y cangrejo | Core 3, Debug |
| Fishing | ADAPTAR | Pesca simple de 3 s; simplificar `FishingTension`; quitar Achievements | Core, Player, Save, UI |
| Interaction | CONSERVAR | Interfaz y componente de interacción, neutros | 12 módulos |
| Items | ADAPTAR | Registro de objetos válido; recortar `items.json` al alcance nuevo | 12 módulos |
| Narrative | ADAPTAR | `ExploredProgress` y `ExploredVoyageSubsystem` ya modelan pecios, grada, Limón y salida de noche; reescribir a actos, cartas y recuerdos | Core, Debug, UI |
| Ocean | CONSERVAR | Olas Gerstner y corrientes; `OceanCurrents` depende de `ArchipelagoLayout` (ver R1) | Audio, Boats 3, Events, Fauna, Fishing 2, Player 2, Sky, Weather |
| Player | ADAPTAR | `SwimModel` se conserva; `ExploredCharacter` está acoplado a Building, Achievements y UI | Audio, Boats, Cartography, Core, Debug 2, `ExploredGameMode.cpp`, Fauna, Fishing, Survival, UI 3 |
| Raiders | BORRAR | Piratas cortados (solo modelos) | Ninguno |
| Ruins | ADAPTAR | Ruinas de Marae con patrones astronómicos sirven; borrar `MuseumModel` y `MuseumDisplayComponent` | Core 3, Save, UI 6, Villages |
| Save | ADAPTAR | `SaveArchive`, `SaveFormat`, `SaveSlots`, `SaveValue` son neutros; recortar `SaveSystemStates` (agregador) y `SaveWorldDeltas` (deltas de terreno) | 13 módulos |
| Sky | CONSERVAR | Hora del día, luna y estrellas son la base de la navegación final; borrar `WorldClockNetModel` (red) | 17 módulos |
| Survival | CONSERVAR | `SurvivalModel`, `BodyModel` y `BodySignals` ya son «el cuerpo como HUD»; quitar `SExploredWristWatch` de `BodySignalsComponent` | Combat 2, Cooking, Core 3, `ExploredGameMode.cpp`, Player 2, Save, UI 3 |
| Tests | ADAPTAR | Ver sección 4 | — |
| Tramway | BORRAR | Raíles cortados (solo modelo) | Ninguno |
| UI | ADAPTAR | Conservar menús (principal, pausa, ajustes, ranuras, créditos, fundido, final), `PlayerController`, ajustes, entrada, `SaveSubsystem`, `SettingsLogic`; borrar `ExploredHUD`, `SExploredAchievements`, `SExploredAchievementToast`, `SExploredInventoryPanel`, `SExploredMuseum`, `SExploredWristWatch`, `SExploredSavingIndicator`; `MapViewLogic`, `SExploredMapSheet`, `SExploredMapInHands` y `SExploredDoodle` pasan al cuaderno | Achievements 2, Audio 2, Building, Cooking, Core 2, Debug, Events, `ExploredGameMode.cpp`, Farming, Narrative, Player, Ruins, Sky, Survival |
| Villages | BORRAR | Aldeas y trueque cortados (solo modelos) | Ninguno |
| Weather | CONSERVAR | Clima y `RainCatchModel` (agua de lluvia) son supervivencia directa | 14 módulos |
| WorldGen | ADAPTAR | Mapa procedural cortado: conservar la interfaz de `ArchipelagoLayout` alimentada por datos de autor, `HarvestModel`, `GroundBranchModel`, `CoconutPalmModel`, `VegetationHarvestState`; borrar el resto (ver tabla siguiente) | 19 módulos y `ExploredEditor` |
| ExploredEditor | ADAPTAR | Borrar `WorldGenCommandlet` (horneado procedural); dejar el módulo vacío para el futuro commandlet de validación de estilo | — |

Raíz del módulo: `Explored.cpp/.h` CONSERVAR; `ExploredGameMode.cpp` ADAPTAR (incluye Boats,
Building, Cooking, Player, Survival y `ExploredHUD`). `Explored.Build.cs` CONSERVAR
(`ProceduralMeshComponent` puede salir cuando se borre el terreno).

### 1.1 WorldGen fichero a fichero

| Grupo | Ficheros | Veredicto |
|---|---|---|
| Recogida | `HarvestModel`, `GroundBranchModel`, `CoconutPalmModel`, `VegetationHarvestState` | CONSERVAR (quitar `SaveValue` si el guardado cambia) |
| Columna procedural | `ArchipelagoLayout`, `PointsOfInterest`, `TerrainDensity` | ADAPTAR: mantener interfaces hasta que exista el layout de autor; luego borrar `TerrainDensity` |
| Terreno horneado | `SurfaceNets`, `TerrainChunkBuilder`, `TerrainErosion`, `VegetationScatter`, `BeachDebrisModel`, `FormationPlacementModel`, `ExploredVegetationCell` | BORRAR tras sustituir el mundo |
| Terraformado y minería | `TerrainEdits`, `TerrainEditModel`, `TerrainDeltaCodecModel`, `TerrainDeltaQueueModel`, `TerrainChunkChecksumModel`, `MiningModel`, `MineHazardModel`, `ShovelPathModel`, `SandModel` | BORRAR |
| Red | `VegetationNetStateModel` | BORRAR |
| Sistemas sin cabida | `WildfireModel`, `FellingModel`, `FelledDriftModel`, `VegetationClockModel` | BORRAR (rescatables del historial si vuelve la tala) |

### 1.2 Modelos puros rescatables aunque su módulo se borre

Ninguno es imprescindible. Quedan en el historial (etiqueta `pre-reset-v2`) y merecen nota:

| Modelo | Por qué guardarlo en la recámara |
|---|---|
| `AchievementsModel` + `AchievementsSpec` | Logros diferidos a después del lanzamiento; el modelo es puro y no depende de HUD |
| `RaftYardModel` | Uniones que ceden: útil si la balsa vuelca o se rompe en el ciclón (Acto IV) |
| `FellingModel`, `FelledDriftModel` | Tala y deriva de troncos si se amplía la recogida |

---

## 2. `Content/`

Total actual: ≈ 1.034 MB. Casi todo está fuera de git: solo 45 ficheros de `Content/` están
versionados (`Data`, `Localization`, `Materials`, 4 materiales de `Generated/Materials`);
`Maps/`, `World/` y `__ExternalActors__/` están en `.gitignore`.

| Carpeta | Tamaño | Veredicto | Motivo |
|---|---|---|---|
| `__ExternalActors__/` | 366,3 MB | BORRAR | Actores OFPA del archipiélago horneado |
| `World/` (`Terrain`, `HLOD`) | 269,8 MB | BORRAR | Terreno y HLOD horneados por `WorldGenCommandlet` |
| `Maps/Archipelago.umap` | < 0,1 MB | BORRAR | Mapa procedural; antes, crear el mapa de la isla 1 y cambiar `DefaultEngine.ini` |
| `Generated/Meshes` | 41,6 MB | BORRAR | Todo el arte v1 (31 grupos) |
| `Generated/Textures` | 82,5 MB | BORRAR | Texturas de detalle prohibidas por la visión (§9.1) |
| `Generated/Materials` | 0,2 MB | BORRAR | `M_LowPoly`, `M_LP_*`, `MI_*` por color; se sustituyen por `M_Toon` + hoja de paleta |
| `Generated/Audio` | 272,5 MB | CONSERVAR (revisar) | Música clásica suave (203 MB), ambiente (63 MB), efectos (6 MB); quitar pistas de sistemas borrados |
| `Materials/` | 0,1 MB | ADAPTAR | `M_PP_Body` (cuerpo legible) y `M_Ocean` ADAPTAR; `M_Stars` CONSERVAR; `M_Terrain` BORRAR |
| `Audio/Classes` | < 0,1 MB | CONSERVAR | Sound classes y mezcla |
| `Data/` | 0,4 MB | ADAPTAR | Ver tabla siguiente |
| `Localization/` | 0,5 MB | ADAPTAR | Regenerar el catálogo tras podar los textos |
| `Collections/`, `Developers/` | 0 | BORRAR | Vacías |

**Content que se borraría: ≈ 760 MB** (World + ExternalActors + Generated sin Audio + `M_Terrain`).

### 2.1 `Content/Data`

| Veredicto | Ficheros |
|---|---|
| BORRAR | `achievements`, `building_pieces`, `combat`, `mining`, `minerals`, `recipes_smithing`, `museum_collections`, `fossils`, `herbarium`, `insects`, `shells`, `fases_futuras`, `packs_catalogo`, `meshes_pendientes` |
| ADAPTAR | `items`, `recipes`, `templates`, `verbs`, `plants`, `fish`, `fauna`, `fauna_terrestre`, `fuels`, `survival_needs`, `boats`, `ruins`, `artifacts`, `exploration`, `map_clues`, `journal_entries`, `story_es`, `music_layers` |

---

## 3. `Art/`, `Tools/`, `Config/`, `docs/`, `Plugins/`

### 3.1 `Art/` (≈ 699 MB; solo 91 FBX de `Export/Meshes|Props` en git)

| Carpeta | Tamaño | Veredicto | Motivo |
|---|---|---|---|
| `Export/Props` | 87,9 MB | BORRAR | Kit v1 (incluido el Limón: se rehace con el estilo nuevo; la forma vive en `_limon_shape.py`) |
| `Export/Meshes` | 59,5 MB | BORRAR | Vegetación, rocas y fauna v1 |
| `Export/Textures` | 69,9 MB | BORRAR | Texturas prohibidas |
| `Generated/Props3D` | 69,9 MB | BORRAR | GLB de Hunyuan3D (pipeline de IA descartado) |
| `Export/Audio` | 407,4 MB | ADAPTAR | No es arte visual: caché regenerable de `Tools/Audio` (ignorada por git) |
| `Export/Animals` | 0 | BORRAR | Vacía |
| `Reference/` | 3,6 MB | CONSERVAR | Referencias personales (Canela, limón); fuera de git: versionar o copiar |
| `Source/Canela.blend` | 0,5 MB | CONSERVAR | Referencia del perro isleño; fuera de git |

**Art que se borraría: ≈ 287 MB** (más 407 MB de caché de audio si se regenera).

### 3.2 `Tools/`

| Elemento | Veredicto | Motivo |
|---|---|---|
| `build.ps1`, `common.ps1`, `test.ps1`, `unreal_python.ps1`, `package.ps1` | CONSERVAR | Neutros |
| `shots.ps1`, `playtest.ps1`, `bench.ps1` | ADAPTAR | Sets por isla en vez de por archipiélago; clave para probar sin HUD |
| `build_content.ps1` | ADAPTAR | Quitar el paso de horneado y los importadores borrados |
| `worldgen.ps1` | BORRAR | Va con `WorldGenCommandlet` |
| `Unreal/import_audio.py` | CONSERVAR | Audio se queda |
| `Unreal/build_materials.py`, `import_props.py` | ADAPTAR | Pasan a `M_Toon` + hoja de paleta e importador del kit nuevo |
| `Unreal/import_lowpoly*.py`, `import_meshes.py`, `import_textures.py`, `fix_lowpoly_materials.py`, `fix_material_usage.py` | BORRAR | Pipeline de arte v1 |
| `Blender/render_preview.py`, `lib/` | CONSERVAR | Lámina de revisión y utilidades |
| `Blender/validate.py`, `run_all.py`, `run_animals.py`, `tests/` | ADAPTAR | Base de `check_style.py` y del lanzador nuevo |
| `Blender/animals/` | ADAPTAR | `rig`, `quadrupeds`, `birds`, `turtle`, `fish` como base; el resto BORRAR |
| `Blender/props/` | BORRAR | Salvo `_limon_shape.py`, `_albatros_shape.py`, `_prims.py` (ADAPTAR) |
| `Blender/assets/`, `Blender/ai/` | BORRAR | Vegetación procedural v1 e IA 3D |
| `Packs/` | ADAPTAR | `normalize*.py` y `remap_*` sirven si se admiten packs de terceros bajo el validador; `download_*`, `landing`, `contact_sheet` BORRAR |
| `Textures/gen_palette.py`, `paleta.json` | ADAPTAR | Paleta de 48 y hoja 64×16 |
| `Textures/texgen/`, `gen_textures.py` | BORRAR | Texturas prohibidas |
| `Audio/` | CONSERVAR | Generador de música y ambientes |
| `DataCheck/` | ADAPTAR | Borrar `achievements.py`, `combat.py`, `farm.py`, `mining.py`, `smithing.py`, `fases.py`; revisar `packs.py` y `estilo.py` |
| `Localization/` | ADAPTAR | Regenerar traducciones tras la poda |
| `HostTests/` | ADAPTAR | Listas y `tests/TerrainEditPropertyTest.cpp` (BORRAR) |
| `Nube/` | ADAPTAR | Cola de tareas del juego v1; vaciar `cola/` |
| `Playtest/contact_sheet.py` | CONSERVAR | Neutro |

### 3.3 `Config/`

| Fichero | Veredicto | Motivo |
|---|---|---|
| `DefaultEngine.ini` | ADAPTAR | `GameDefaultMap`/`EditorStartupMap` apuntan a `Archipelago`; revisar Lumen, VSM y Nanite para un estilo plano con una sola luz |
| `DefaultGame.ini` | ADAPTAR | `DirectoriesToAlwaysCook` incluye `/Game/Generated` |
| `DefaultInput.ini`, `DefaultEditor.ini`, `DefaultGameUserSettings.ini` | CONSERVAR | Neutros |
| `Localization/` | ADAPTAR | Rutas de recogida de textos |

### 3.4 `docs/`

| Elemento | Veredicto |
|---|---|
| `reset/` | CONSERVAR (visión e inventario) |
| `v1-alcance.md`, `creditos-musica.md` | CONSERVAR (histórico y créditos) |
| `diseno/` (`gdd_v2.md`, `biblia`, `exploracion.md`), `design/`, `superpowers/`, `roadmap.md` | ADAPTAR: mover a `docs/archivo/` (sustituidos por la visión) |
| `tecnico/` | ADAPTAR: conservar `cuerpo`, `guardado`, `localizacion*`, `contenido-textos`, `musica-grabada`, `glosario`, `fauna`, `lluvia-recipientes`, `cocos-palmeras`; BORRAR `arena-viva`, `astillero-balsas`, `casco-por-piezas`, `incendio-integracion`, `navegantes-piratas`, `railes-vagones`, `tala-integracion`, `terreno-editable` |
| `balance/` | BORRAR salvo `comida-recoleccion` y `fauna-salvaje` (ADAPTAR) |
| `reviews/` (24) | BORRAR o archivar |
| `art/` (56,9 MB de láminas v1) | BORRAR; hay 4 PNG de `art/packs` modificados sin commit |

`Plugins/UnrealMCPython` (97,9 MB): CONSERVAR (playtest por MCP).

---

## 4. Tests

Hoy: 86 specs en `Source/Explored/Tests` (74 en `pure_specs.txt`), 87 fuentes en
`pure_sources.txt` y 1 test propio en `Tools/HostTests/tests`.

| Resultado | N.º | Specs |
|---|---|---|
| Se borran | 41 | `AchievementsDataSpec`, `AchievementsSpec`, `BarterModelSpec`, `BeachDebrisSpec`, `BuildingDataSpec`, `BuildingSpec`, `CombatModelSpec`, `ContainerReplicationModelSpec`, `CoopRulesSpec`, `FarmSpec`, `FaunaAnchorNetModelSpec`, `FelledDriftModelSpec`, `FellingModelSpec`, `FormationPlacementSpec`, `HullAssemblyModelSpec`, `MineHazardModelSpec`, `MiningModelSpec`, `MuseumSpec`, `NetBudgetModelSpec`, `NetBudgetTableModelSpec`, `NetQuantizeSpec`, `PirateThreatModelSpec`, `RaftFurrowModelSpec`, `RaftYardModelSpec`, `RaiderCampsModelSpec`, `ReputationModelSpec`, `SandModelSpec`, `ShovelPathModelSpec`, `TerrainChunkChecksumModelSpec`, `TerrainDeltaCodecModelSpec`, `TerrainDeltaQueueModelSpec`, `TerrainEditModelSpec`, `TerrainEditsSpec`, `TerrainErosionSpec`, `TramwayModelSpec`, `VegetationClockModelSpec`, `VegetationNetStateModelSpec`, `VegetationScatterSpec`, `WildfireModelSpec`, `WorldClockNetModelSpec`, `WorldGenSpec` |
| Se adaptan | 21 | `BoatSpec`, `CartographySpec`, `CoconutPalmModelSpec`, `ExplorationContentDataSpec`, `ExplorationContentSpec`, `FrontendControllerSpec`, `FrontendInputSpec`, `FrontendSettingsSpec`, `GroundBranchModelSpec`, `HarvestModelSpec`, `MapViewLogicSpec`, `MusicDirectorSpec`, `NarrativeSpec`, `OceanCurrentsSpec`, `PropertyFuzzSpec`, `RuinsDataSpec`, `RuinsSpec`, `SaveSystemsSpec`, `ScreensLogicSpec`, `SystemLinksSpec`, `WorldEventsSpec` |
| Se conservan | 24 | `AmbienceSpec`, `BodySpec`, `BoidsSpec`, `CarrySpec`, `CookingSpec`, `CraftingSpec`, `ExploredRandomNoiseSpec`, `FireSpec`, `FishingSpec`, `GaitSpec`, `InventorySpec`, `ItemsSpec`, `LocalizationSpec`, `MarineFaunaSpec`, `MoonSpec`, `PlaytestReportSpec`, `RainCatchModelSpec`, `SaveArchiveSpec`, `SaveSlotsSpec`, `SettingsLogicSpec`, `SkyOceanSpec`, `SurvivalSpec`, `SwimSpec`, `WeatherSpec` |

Quedan **45 specs** (35 en `pure_specs.txt`). `pure_sources.txt` pierde 40 entradas y queda
en 47 (menos aún cuando se retire `TerrainDensity`).

Riesgos para el host y la CI (`.github/workflows/host-tests.yml`):

- `CMakeLists.txt` lee `pure_sources.txt` y `pure_specs.txt` con `file(STRINGS)`: una línea
  que apunte a un fichero borrado rompe la configuración. Cada borrado actualiza ambas listas
  en el mismo commit.
- Cadenas puras ocultas: `TerrainDensity.h` incluye `TerrainEdits.h` (y este,
  `TerrainEditModel.h` y `SaveWorldDeltas.h`); `PointsOfInterest` incluye `TerrainDensity` y
  `VegetationScatter`; `CoastlineTrace` y `RaiderCampsModel` incluyen `TerrainDensity`;
  `RaftFurrowModel` incluye `SandModel`. Borrar el terraformado exige desacoplar antes
  `FTerrainDensity` de `FTerrainEdits`.
- `PropertyFuzzSpec` toca 10 módulos (Tramway y WorldGen incluidos) y `NetBudgetTableModel`
  agrega todos los modelos de red: hay que editarlos antes de borrar lo que referencian.
- Matriz `python-tools`: `datacheck --strict` falla si se borra un JSON y queda su validador (o
  al revés); `l10n export --check` falla hasta regenerar el catálogo; las entradas `Packs`,
  `Textures` y `Blender` deben reflejar lo que se borre de esas carpetas.
- Los tres pasos de `cpp-models` (normal, ASan/UBSan, fast-math) deben seguir en verde tras
  cada paso de la poda.

---

## 5. Orden de poda seguro

Cada paso termina con: compilación del editor (`Tools/build.ps1`), `Tools/HostTests/run.sh`
en verde, `datacheck --strict` y `l10n export --check` en verde, y un commit propio.

| Paso | Qué se borra | Ediciones obligatorias en el mismo commit |
|---|---|---|
| 0. Red de seguridad | Nada | Etiqueta `pre-reset-v2`; zip fuera del repo de `Content/Generated`, `Content/Maps`, `Art/` (sobre todo `Reference/` y `Source/`); commit o descarte de los PNG modificados de `docs/art/packs` |
| 1. Hojas y red | Combat, Raiders, Villages, Tramway; `NetQuantize`, `NetBudget*`, `ContainerReplicationModel`, `FaunaAnchorNetModel`, `WorldClockNetModel`, `VegetationNetStateModel`, `CoopRulesSpec`; sus specs y JSON (`combat`) | `pure_sources.txt`, `pure_specs.txt`, `PropertyFuzzSpec` (parte Tramway), DataCheck (`combat.py`) |
| 2. Sistemas de juego cortados | Achievements, Farming, Building, Museum (`MuseumModel`, `MuseumDisplayComponent`, `SExploredMuseum`), `HullAssemblyModel`, `RaftYardModel`, `RaftFurrowModel`, `ExploredSledge`; JSON asociados | `SaveSystemStates.h`, `SystemLinks.h`, `ExploredWiringSubsystem`, `ExploredCharacter`, `ExploredGameMode`, `CarryComponent.h` (`BuildingTypes`), `ExploredFire`, `ExploredTrap`, `FishingComponent`, `ExploredPlayerController`, `ScreensLogic`, `RuinsSubsystem.h`; DataCheck (`achievements`, `farm`, `smithing`) |
| 3. Terraformado y minería | `TerrainEdits`, `TerrainEditModel`, `TerrainDelta*`, `TerrainChunkChecksumModel`, `MiningModel`, `MineHazardModel`, `ShovelPathModel`, `SandModel`, `WildfireModel`, `Felling*`, `VegetationClockModel`; `Tools/HostTests/tests/TerrainEditPropertyTest.cpp`; JSON `mining`, `minerals` | Desacoplar `TerrainDensity` de `FTerrainEdits`; recortar `SaveWorldDeltas`; DataCheck (`mining.py`) |
| 4. HUD | `ExploredHUD`, `SExploredAchievements`, `SExploredAchievementToast`, `SExploredInventoryPanel`, `SExploredWristWatch`, `SExploredSavingIndicator` | `ExploredGameMode` (clase de HUD), `BodySignalsComponent`, `ExploredPlayerController`; el juego queda sin interfaz en partida hasta el cuaderno (H1) |
| 5. Mundo procedural y arte v1 | Tras crear el mapa de la isla 1 y el layout de autor con la interfaz de `ArchipelagoLayout`: `WorldGenCommandlet`, `worldgen.ps1`, `SurfaceNets`, `TerrainChunkBuilder`, `TerrainDensity`, `TerrainErosion`, `VegetationScatter`, `BeachDebrisModel`, `FormationPlacementModel`, `ExploredVegetationCell`; `Content/World`, `__ExternalActors__`, `Maps/Archipelago`, `Generated/Meshes|Textures|Materials`, `M_Terrain`; `Art/Export/Meshes|Props|Textures`, `Art/Generated`; importadores y generadores de arte v1 | `DefaultEngine.ini` (mapa), `DefaultGame.ini` (cocinado), `PointsOfInterest`, `CoastlineTrace`, las 33 rutas `/Game/Generated/Meshes` del código, `build_content.ps1`, matriz de CI |

El paso 5 depende de trabajo nuevo (layout de autor, mapa de la isla 1, `M_Toon`), así que
encaja con el hito H0/H1 de la visión y no debe adelantarse a él.

---

## 6. Riesgos principales

| # | Riesgo | Mitigación |
|---|---|---|
| R1 | La columna procedural (`ArchipelagoLayout`, `TerrainDensity`, `PointsOfInterest`) la incluyen Cartography, Ocean, Events, Ruins, Exploration, Audio, Narrative y `SystemLinks`; `TerrainDensity.h` arrastra el terraformado | Mantener las interfaces y cambiar solo la fuente (datos de autor); desacoplar `FTerrainEdits` en el paso 3; borrar la columna solo en el paso 5 |
| R2 | Agregadores con muchos `#include` (`SaveSystemStates.h`, `SystemLinks.h`, `ExploredWiringSubsystem.cpp`, `ExploredCharacter.cpp`, `ExploredGameMode.cpp`, `PropertyFuzzSpec`) y listas de CI que fallan si no se actualizan en el mismo commit | Un commit por paso con las ediciones de la tabla 5; host tests, DataCheck y Localization en verde antes de seguir |
| R3 | Contenido sin control de versiones: `Content/Generated`, `Content/Maps`, `Art/Export`, `Art/Reference` y `Art/Source` (Canela) no están en git; borrarlos es irreversible. Las 33 rutas `/Game/Generated/...` fallan en ejecución, no al compilar | Copia comprimida fuera del repo en el paso 0; versionar `Art/Reference` y `Art/Source`; buscar rutas `/Game/` en cada paso y probar en el editor |
