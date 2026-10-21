# Baliza Siete — plan de producción v3

Fecha: 2026-10-21 · rama `reset/v2` · diseño de referencia: `docs/v3/gdd.md`.
Objetivo: juego entregable en 11-12 sesiones de trabajo con agentes (unas horas cada una). F4 y F5 se ejecutan en paralelo.

## 1. Reglas de producción

- La lógica nueva va en modelos puros `F<Algo>Model` (solo `CoreMinimal.h`) con su `*Spec.cpp`, validables con `Tools/HostTests/run.sh`.
- Cada commit de poda deja en verde: `Tools/build.ps1`, `Tools/HostTests/run.sh` (con `HOST_TESTS_SANITIZE=ON` al cierre de fase), `datacheck --strict` y `l10n export --check`. `pure_sources.txt` y `pure_specs.txt` se actualizan en el mismo commit que el borrado.
- Check de cadenas prohibidas (debe devolver 0 resultados al cerrar F1, fuera de `docs/archivo/` y de este plan): `git grep -iE "alm[u]dena|lim[ó]n|lem[o]n|kn[i]ght|halden|limonero" -- . ':!docs/archivo' ':!docs/v3/plan.md'`. La expresión usa corchetes para no coincidir consigo misma.
- Ningún asset entra sin pasar `check_style.py` y la lámina de revisión que aprueba Rodrigo.
- Un fichero o un kit por encargo a subagente; criterio de aceptación medible y verificación por script.
- Agentes: `unreal-engine-engineer` (C++/UE), `technical-artist` (Blender, materiales, audio), `game-designer` (datos, texto, equilibrio), `code-reviewer` (revisión al cierre de cada fase), `debugger` si la build resiste.

## 2. Resumen de fases

| Fase | Sesiones | Ejecuta | Depende de | Criterio de salida (medible) |
|---|---|---|---|---|
| F0 Seguridad | hecha | — | — | Etiqueta `pre-reset-v2` y copia fuera del repo existen |
| F1 Poda | 2 | unreal-engine-engineer | F0 | Build verde, HostTests verde, grep prohibido = 0, juego arranca en mapa vacío con HUD y menú |
| F2 Estilo | 1 | technical-artist | F1 | `check_style.py` pasa 3 mallas piloto, 0 texturas, lámina aprobada por Rodrigo |
| F3 Blockout | 2 | unreal-engine-engineer + game-designer | F1 (F2 no bloquea) | `PlaytestBot` completa el juego en cubos; `PlaytestAuditor` 0 softlocks; 5 specs nuevos verdes |
| F4 Arte por kits | 3 | technical-artist | F2, F3 | 34/34 mallas validadas e importadas, 0 errores de estilo, láminas K1-K3 aprobadas |
| F5 UI completa | 2 (paralela a F4) | unreal-engine-engineer | F3 | 12 pantallas/elementos del GDD §9 presentes; capturas `shots.ps1` a 1080p sin solapes |
| F6 Contenido, audio, final | 2 | game-designer + unreal-engine-engineer | F4, F5 | Partida completa por bot 47-65 min a 1×; 8 páginas y 3 epílogos; final jugable |
| F7 Pulido y paquete | 1,5 | unreal-engine-engineer + code-reviewer | F6 | ≥ 60 fps medios y ≥ 45 de mínimo a 1080p; paquete Win64 arranca; 3 partidas bot sin fallo |

Calendario: S1-S2 F1; S3 F2; S4-S5 F3; S6-S8 F4 (S6-S7 con F5 en paralelo); S9-S10 F6; S11-S12 F7. Total 11-12 sesiones de calendario.

## 3. Módulos C++ para ESTE diseño

Sustituye al veredicto de `docs/reset/inventario.md` donde difiere. Diferencias señaladas al final de la tabla.

### 3.1 `Source/Explored`

| Módulo | Veredicto | Notas |
|---|---|---|
| Achievements, Building, Combat, Farming, Raiders, Tramway, Villages | BORRAR | Sin cambios respecto al inventario |
| Cartography | BORRAR | **Difiere del inventario (ADAPTAR)**: el diseño no tiene mapa; la isla es pequeña |
| Ruins | BORRAR | **Difiere (ADAPTAR)**: sin marae ni petroglifos; incluye `MuseumModel` y `MuseumDisplayComponent` |
| Exploration | BORRAR | **Difiere (ADAPTAR)**: sin catálogo de lugares; las zonas van en `island.json` |
| Boats | ADAPTAR | Conservar `BoatModel` (solo empuje de remo, sin vela) y `ExploredBoat`; borrar `HullAssemblyModel`, `RaftYardModel`, `RaftFurrowModel` y el tipo de barco anterior en `BoatTypes.h`, `SystemLinks.cpp`, `SaveSystemStates.cpp` |
| Carry | ADAPTAR | Conservar `InventoryModel` y `CarryComponent` (quitar `BuildingTypes`); borrar `ContainerReplicationModel` y `ExploredSledge` |
| Cooking | CONSERVAR | Quitar la llamada a Achievements de `ExploredFire`; quitar el objeto de fruta prohibido de `CookingData.inl` |
| Core | ADAPTAR | Conservar `ExploredRandom`, `ExploredNoise`, `ExploredLocalization`; borrar `NetQuantize`; recortar `SystemLinks`; **reescribir `ExploredWiringSubsystem`** (1.714 líneas, a < 500) |
| Crafting | ADAPTAR | `CraftingLibrary` intacto; `recipes.json` a las 10 recetas |
| Debug | ADAPTAR | Conservar `PlaytestBot`, `PlaytestAuditor`, `ShotSubsystem`, `PlaytestReportModel`; borrar `NetBudget*` |
| Events | ADAPTAR | Solo tormenta/ciclón; fuera tortuga; la semilla viene de `island.json`, no de `ArchipelagoLayout` |
| Fauna | ADAPTAR | Solo peces y gaviotas (boids, `MarineCreatureBrain` recortado); borrar `FaunaAnchorNetModel` |
| Fishing | ADAPTAR | Pesca de 3 s sin barra de tensión; quitar Achievements; borrar `ExploredTrap` |
| Interaction | CONSERVAR | — |
| Items | ADAPTAR | `items.json` a 17 objetos |
| Narrative | ADAPTAR | `ExploredProgress` y `ExploredVoyageSubsystem` pasan a la máquina de etapas del GDD §5.9; datos en `objectives.json`; nuevo `FBeaconProgressModel` |
| Ocean | CONSERVAR | `OceanCurrents` se alimenta de `island.json` (ver WorldGen) |
| Player | ADAPTAR | `ExploredCharacter` sin `BuildPreviewComponent` ni Achievements; conservar `SwimModel`; mantener manos de primera persona |
| Save | ADAPTAR | Conservar `SaveArchive`, `SaveFormat`, `SaveSlots`, `SaveValue`; reducir `SaveSystemStates` a las secciones del GDD §5.10; borrar `SaveWorldDeltas` |
| Sky | CONSERVAR | Borrar `WorldClockNetModel`; colocar `ExploredSkyController` en el mapa nuevo |
| Survival | CONSERVAR | Quitar `SExploredWristWatch`; añadir desmayo y dormir como deltas fijos en `BodyModel` |
| Weather | CONSERVAR | `RainCatchModel` se usa por primera vez; el reloj de tormenta lo gobierna `FStormClockModel` |
| WorldGen | ADAPTAR | Conservar `HarvestModel`, `GroundBranchModel`, `CoconutPalmModel`, `VegetationHarvestState`. Sustituir `ArchipelagoLayout`/`PointsOfInterest`/`TerrainDensity` por `FIslandLayout` (misma firma que consumen Ocean, Events, Audio) alimentado por `island.json`; después borrar todo el terreno horneado, terraformado, minería, red y tala (listas del inventario §1.1) |
| UI | ADAPTAR | Conservar menús (principal, pausa, ajustes, ranuras, créditos, fundido, final), `PlayerController`, `SaveSubsystem`, `SettingsLogic`, `ScreensLogic`; **`ExploredHUD` y `SExploredInventoryPanel` se adaptan en vez de borrarse** (**difiere del inventario, paso 4**: la auditoría los da por funcionales y cubren casi todo el HUD); borrar `SExploredAchievements`, `SExploredAchievementToast`, `SExploredMuseum`, `SExploredWristWatch`, `SExploredSavingIndicator`, mapa (`MapViewLogic`, `SExploredMapSheet`, `SExploredMapInHands`, `SExploredDoodle`) y selección de modo |
| Tests | ADAPTAR | Inventario §4 más: borrar también `CartographySpec`, `RuinsSpec`, `RuinsDataSpec`, `ExplorationContentSpec`, `ExplorationContentDataSpec`, `MapViewLogicSpec` |
| `Explored.cpp/.h`, `Explored.Build.cs` | CONSERVAR | Quitar `ProceduralMeshComponent` al borrar el terreno |
| `ExploredGameMode` | ADAPTAR | Sin Building; reaparición del desmayo en el último refugio |

### 3.2 Módulos nuevos

| Módulo / clase | Tipo | Función |
|---|---|---|
| `FBeaconProgressModel` | Modelo puro + Spec | Etapas, hitos, ranuras de la linterna, variante del epílogo |
| `FStormClockModel` | Modelo puro + Spec | Curva del barómetro, chubasco, ciclón, ojo |
| `FNotebookModel` | Modelo puro + Spec | Páginas leídas (bitmask), nota de Iria, epílogo |
| `FSupplyCrateModel` | Modelo puro + Spec | Qué falta para el camino crítico; entrega única |
| `FTutorialHintModel` | Modelo puro + Spec | 8 mensajes de una sola vez |
| `FIslandLayout` | Modelo puro + Spec | Datos de autor de la isla (costa, zonas, islote) |
| `AExploredPickup`, `AExploredHarvestable` | Actores UE | Recursos colocados a mano con `Id` estable |
| `ABarrilLluvia`, `AShelterVolume`, `ALighthouseLamp` | Actores UE | Barril de lluvia, refugio, linterna de 3 ranuras |
| `ExploredEditor` commandlet de estilo | Editor | Valida mallas importadas contra la paleta |

### 3.3 `Content/`, `Art/`, `Tools/`, `Config/`

Se sigue el inventario §2-3 con estas decisiones: crear `Maps/Map_IslaSal` y cambiar `DefaultEngine.ini` antes de borrar `Maps/Archipelago`; borrar `Content/Data`: `halden_diaries`, `journal_entries`, `story_es` antiguos (se reescriben desde el GDD), `plants`, `ruins`, `artifacts`, `exploration`, `map_clues`, `boats` salvo la balsa, `fauna_terrestre`; `Art/Export/Props` y `Art/Export/Meshes` se borran enteros (ya sin excepción del arte de v1); `Tools/Blender/props`, `assets` y `ai` se borran; versionar `Art/Reference` y `Art/Source` antes de borrar. Audio: conservar `Content/Generated/Audio` y revisar pistas.

## 4. Fases

### F0 — Seguridad (hecha)

- Objetivo: poder volver atrás.
- Entregables: etiqueta `pre-reset-v2` y copia comprimida fuera del repositorio de `Content/Generated`, `Content/Maps` y `Art/`.
- Salida: `git tag -l pre-reset-v2` devuelve la etiqueta; el zip existe.
- Pendiente de F0 que se hace al arrancar F1: commit o descarte de los 4 PNG modificados en `docs/art/packs`.

### F1 — Poda del código y de la historia anterior (2 sesiones)

- Objetivo: dejar un árbol limpio que compila, arranca y no contiene rastro de la historia anterior.
- Sesión 1: pasos 1-4 del inventario §5 con el módulo adaptado a esta tabla (§3): hojas y red; Achievements, Farming, Building, Museum, Boats por piezas, Cartography, Ruins, Exploration; terraformado y minería; HUD antiguo (solo widgets de la lista, no `ExploredHUD`).
- Sesión 2: paso 5: crear `FIslandLayout` (con `island.json` provisional de 3 puntos) y cambiar las consumidoras; borrar la columna procedural y el terreno horneado; borrar `Archipelago`; crear `Map_IslaSal` vacío (agua + `ExploredOcean` + `ExploredSkyController` + `PlayerStart`); barrido de las rutas `/Game/Generated/...` cableadas (`ExploredVoyageActors`, `ExploredAmbienceSubsystem`, `ExploredRuinElement`, `SwimComponent`, `BodySignalsComponent`): sustituir por rutas del kit nuevo con carga defensiva (si falta el asset, log y malla de motor, nunca crash); purgar las cadenas prohibidas de código, tests, datos, localización, herramientas y docs (mover a `docs/archivo/` lo sustituido, con cabecera «obsoleto»); marcar `docs/reset/vision.md` y `docs/reset/inventario.md` como superados por `docs/v3/`.
- Ficheros: `Source/Explored/**` (todos los módulos de §3), `Core/ExploredWiringSubsystem.cpp`, `ExploredGameMode.cpp`, `Player/ExploredCharacter.cpp`, `UI/ExploredPlayerController.cpp`, `Save/SaveSystemStates.*`, `Core/SystemLinks.*`, `Tools/HostTests/*.txt`, `Content/Data/*`, `Content/Localization/*`, `Tools/DataCheck`, `Tools/Localization`, `Config/Default*.ini`, `.github/workflows/*.yml`.
- Criterio de salida:
  1. `Tools/build.ps1` termina sin errores.
  2. `Tools/HostTests/run.sh` (normal y con sanitizers) verde; ≈ 40 specs.
  3. `git grep` prohibido (ver §1) = 0 resultados.
  4. `Tools/playtest.ps1` arranca `Map_IslaSal`, camina 60 s sin crash ni errores de carga de assets en log.
  5. `ExploredWiringSubsystem.cpp` < 500 líneas.
  6. CI de GitHub (`host-tests.yml`) verde.
- Agente: `unreal-engine-engineer`; revisión final `code-reviewer`.
- Riesgo: R2 y R4 del GDD. Un commit por paso.

### F2 — Estilo (1 sesión)

- Objetivo: fijar un solo estilo antes de producir arte.
- Entregables:
  - `Tools/Textures/paleta.json` de 32 colores y `palette.py` con hoja 32 × 1 (GDD §11).
  - `M_Toon` (3 bandas, sin texturas) y `M_ToonLandscape` en `Content/Materials` mediante `Tools/Unreal/build_materials.py`.
  - `Tools/Blender/check_style.py` (sustituye a `validate.py`) con reglas del GDD §11; tests `uv run pytest -q --cov` ≥ 80 %.
  - Commandlet de validación en `ExploredEditor`.
  - 3 mallas piloto: `SM_PalmA`, `SM_Cuchillo`, `SM_BarrilLluvia`, importadas en una escena de prueba junto al cielo y el océano.
  - Lámina de revisión (`render_preview.py`) con las 3 piezas, a la luz del mapa.
  - Decisión sobre Lumen y Nanite para un estilo plano con una luz (`DefaultEngine.ini`).
- Ficheros: `Tools/Textures/*`, `Tools/Blender/*`, `Tools/Unreal/build_materials.py`, `Content/Materials/*`, `Source/ExploredEditor/*`, `Config/DefaultEngine.ini`.
- Criterio de salida: las 3 mallas pasan `check_style.py` con 0 errores; un malla con textura o con 2 materiales falla (test negativo); 0 texturas en `Content/Materials`; Rodrigo aprueba la lámina.
- Agente: `technical-artist`.
- Dependencias: F1 (árbol limpio).

### F3 — Isla en cubos con el bucle completo (2 sesiones)

- Objetivo: partida completa jugable de principio a fin con cubos y primitivas, sin HUD final.
- Entregables:
  - `Content/Data/island.json` con zonas y posiciones del GDD §6 y `Tools/Island/gen_heightmap.py` (uv, determinista, tests ≥ 80 %) que genera la altura del `Landscape`; mapa `Map_IslaSal` con las 7 zonas, `ShelterVolume`s, 8 palmas, pickups, puntos de pesca, sitio de balsa, islote.
  - Actores `AExploredPickup`, `AExploredHarvestable`, `ABarrilLluvia`, `ALighthouseLamp` (3 ranuras) sobre primitivas.
  - Modelos puros con Specs: `FBeaconProgressModel`, `FStormClockModel`, `FNotebookModel`, `FSupplyCrateModel`, `FTutorialHintModel`, `FIslandLayout`; ampliación de `BodyModel` (desmayo, dormir).
  - JSON: `survival_needs.json`, `storm_curve.json`, `items.json` (17), `recipes.json` (10), `verbs.json` (6), `objectives.json`.
  - Balsa de cubos con remo, estabilidad forzada y plan B decidido (GDD §5.7). **Decisión de F3d:** balsa cinemática (`FRaftModel` + `AExploredRaft`: 2 m/s, giro 40°/s, corriente lateral 0,3 m/s, cabeceo visual ≤ 4°, encalla al tocar fondo); queda estable en la primera sesión y el plan B (cruce con fundido) no se usa. El ojo del ciclón dura 65 s reales (≈ 1,44 h de juego, 22:00-23:27) y el muro 2 llega hasta las 02:00: la tabla del GDD §5.8 (ojo hasta las 23:45) no cuadraba con los 65 s del §3.
  - Ciclón con ojo (solo clima y audio).
  - Autoguardado en amanecer y D3 20:00; guardar/cargar en cualquier etapa.
  - Final provisional: linterna encendida, luces de prueba, texto de epílogo.
  - `PlaytestBot` con un script de partida completa y modo de reloj acelerado.
- Ficheros: `Source/Explored/Narrative/*`, `Weather/*`, `Survival/*`, `Items/*`, `Crafting/*`, `Boats/*`, `Fishing/*`, `Cooking/*`, `Save/*`, `Debug/*`, `Content/Data/*`, `Content/Maps/Map_IslaSal.umap`, `Tools/Island/*`.
- Criterio de salida:
  1. `PlaytestBot` completa la partida de principio a fin en blockout; el informe (`PlaytestReportModel`) da duración de juego entre 47 y 65 min a 1×.
  2. `PlaytestAuditor`: 0 softlocks en las combinaciones de decisión (orden de tareas, página 1 sin leer, recursos agotados con caja de suministros).
  3. Specs nuevos verdes (6) y `HostTests` verde con sanitizers.
  4. Guardar y cargar en las 10 etapas devuelve el mismo estado (test de ida y vuelta).
  5. Desmayo y dormir verificados por Spec con el caso negativo.
- Agentes: `unreal-engine-engineer` (código y mapa); `game-designer` (JSON, curva, objetivos).
- Dependencias: F1. No espera a F2 (usa primitivas).

### F4 — Arte por kits (3 sesiones; en paralelo a F5)

- Objetivo: sustituir las primitivas por el arte definitivo, por kits.
- Sesión 1, K1 Naturaleza (8 mallas) más material del terreno aplicado.
- Sesión 2, K2 Estructuras (10 mallas) más manos (`SM_ManoIria`).
- Sesión 3, K3 Objetos (15 mallas) más fauna (pez, gaviota; cangrejo solo si sobra).
- Entregables por kit: scripts en `Tools/Blender/props/<kit>.py` (uv), FBX en `Art/Export/<kit>/`, importación con `Tools/Unreal/import_props.py`, lámina de revisión, sustitución de primitivas en el mapa y en los actores (`ExploredVoyageActors`, etc.), `check_style.py` y commandlet en verde.
- Criterio de salida:
  1. 34/34 mallas importadas, 0 errores de `check_style.py`.
  2. Cada malla dentro de su presupuesto de triángulos (GDD §11); escena media < 400.000 triángulos medidos con `stat` en 3 vistas de `shots.ps1`.
  3. 0 texturas en `Content` (script de recuento), todo con `M_Toon`/`MI_Toon`.
  4. Láminas K1, K2 y K3 aprobadas por Rodrigo (si una se rechaza, se rehace solo ese kit).
  5. `shots.ps1` produce 6 capturas de la isla sin materiales por defecto del motor (cuadrícula gris) visibles.
- Agente: `technical-artist`; integración en mapa por `unreal-engine-engineer`.
- Dependencias: F2 (estilo) y F3 (nombres de actores y mapa).

### F5 — UI completa (2 sesiones; en paralelo a F4)

- Objetivo: la interfaz definitiva del GDD §9.
- Entregables: HUD (8 elementos), inventario con arrastrar y lista de verbos (máx. 3), cuaderno (J) con 8 páginas y notas, menús (principal, pausa, ajustes con 3 volúmenes, sensibilidad y calidad), cartel de día, fundido y desmayo, pantalla final con epílogo de 3 líneas, créditos, tutoriales de una línea (8), pensamientos de Iria. Estilo UI: serif sencilla, iconos planos de la paleta.
- Ficheros: `UI/ExploredHUD.cpp`, `UI/Widgets/SExplored*` (inventario, cuaderno, menús, final, créditos), `UI/ScreensLogic`, `UI/SettingsLogic`, `Content/Data/notebook.json`.
- Criterio de salida:
  1. Las 12 pantallas y elementos del GDD §9 existen y son accesibles (lista de comprobación en `Tools/shots.ps1`).
  2. Capturas a 1080p de cada pantalla, sin solapes ni texto < 22 px.
  3. Test de flujo de menús (`ScreensLogicSpec`, `FrontendControllerSpec`) verde.
  4. Desde el menú principal se llega a «Nueva partida», se juega, se guarda, se vuelve al menú y «Continuar» restaura el estado.
- Agente: `unreal-engine-engineer`.
- Dependencias: F3.

### F6 — Contenido, audio y final (2 sesiones)

- Objetivo: la historia completa, el audio y el final escenificado.
- Entregables:
  - Textos definitivos en `story_es`/`notebook.json`: 8 páginas, 8 notas, 12 pensamientos, objetivos, 3 líneas de epílogo (≈ 100 líneas totales); `l10n export` regenerado, solo español.
  - Audio: director musical con 5 estados (calma, noche, ciclón, ojo, final); ambiente por zona; efectos del GDD §10; créditos de música en `docs/creditos-musica.md` actualizados.
  - Final: animación de encender (3 s), haz giratorio (`SpotLight` rotando), luces del Mistral y bocina, epílogo, créditos, vuelta al menú.
  - Equilibrio: ajuste de `survival_needs.json`, `storm_curve.json` y márgenes con el bot.
- Ficheros: `Content/Data/*`, `Content/Localization/*`, `Audio/*`, `Narrative/*`, `Content/Generated/Audio`.
- Criterio de salida:
  1. Partida completa por `PlaytestBot` en 47-65 min de juego a 1×, sin desmayos obligatorios en la ruta competente (≤ 1 desmayo en la ruta lenta).
  2. 3 rutas de bot (competente, lenta, sin leer páginas opcionales) terminan; el epílogo cambia solo con 8/8.
  3. `datacheck --strict` y `l10n export --check` verdes; 0 cadenas sin traducir; 0 cadenas prohibidas.
  4. Rodrigo juega una partida completa y la termina.
- Agentes: `game-designer` (texto, equilibrio); `unreal-engine-engineer` (final y audio); `technical-artist` si falta audio.
- Dependencias: F4 y F5.

### F7 — Pulido, rendimiento y paquete (1,5 sesiones)

- Objetivo: versión entregable.
- Entregables: optimización (Nanite/Lumen/VSM según F2, LOD y distancia de dibujado), corrección de errores de la lista de F6, cangrejo ambiental si sobra, revisión de seguridad del guardado (entrada validada en `SaveArchive`), `Tools/package.ps1` ajustado, README con cómo jugar y compilar, `CLAUDE.md` del proyecto actualizado.
- Criterio de salida:
  1. ≥ 60 fps de media y ≥ 45 fps mínimos a 1080p en la máquina de desarrollo (`Tools/bench.ps1`, recorrido en 3 zonas y ciclón).
  2. `Tools/package.ps1` genera un build Win64 que arranca, y una partida completa por bot sin fallo ni error en log (3 repeticiones).
  3. `/codex:adversarial-review` sin hallazgos CRITICAL ni HIGH abiertos.
  4. Cobertura ≥ 80 % en modelos nuevos; HostTests con sanitizers y CI verdes.
  5. Tamaño del paquete registrado (objetivo orientativo ≤ 1,2 GB con audio).
- Agentes: `unreal-engine-engineer`; `code-reviewer`.
- Dependencias: F6.

## 5. Control de alcance

- Tope firme: 35 mallas de prop (34 usadas), 17 objetos, 10 recetas, 8 páginas, 3 especies, 1 personaje (manos). Añadir algo exige quitar algo del mismo tipo.
- Si al cierre de F4 hay retraso de más de una sesión: se recorta el cangrejo, la variante de la hierba y `SM_Shrub`; no se recorta la lente, la linterna ni el faro.
- Si F3 supera 2 sesiones: aplicar el plan B de la balsa (cruce con fundido a negro).
- Decisión del propietario pendiente: ninguna bloqueante. Valores numéricos y textos son de primera pasada y se ajustan en F6.

## 6. Contradicciones con documentos existentes

1. `docs/reset/inventario.md` marca Cartography, Ruins y Exploration como ADAPTAR y `ExploredHUD` y `SExploredInventoryPanel` como BORRAR; este plan los invierte (§3). El inventario se marca como superado en F1.
2. `docs/reset/vision.md`, `docs/v1-alcance.md`, `docs/roadmap.md` y `docs/diseno/gdd_v2.md` describen la historia anterior y se archivan.
3. El inventario conserva `BoatModel` con vela; este diseño la elimina (solo remo).
4. El inventario ADAPTAR `Fauna` con perro, gallina, jabalí y cangrejo; este diseño solo conserva peces y gaviotas (cangrejo posterior).
