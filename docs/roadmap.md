# Hoja de ruta hasta el juego completo

Estado a 2026-09-26 frente al GDD (`docs/superpowers/specs/2026-09-26-explored-design.md`) y la
biblia (`docs/design/biblia-de-contenido.md`). **Las rutinas de la nube trabajan desde esta
lista**: coge el primer paquete `pendiente` de tu ámbito, márcalo `en curso (rama)` en tu PR y
`hecho (#PR)` cuando se fusione.

## Regla de validación (sin Unreal en la nube)

Cada sistema de juego se parte en dos:

1. **Modelo puro** `F<Sistema>Model` (solo `CoreMinimal.h`): toda la lógica y el equilibrio.
   Su `*Spec.cpp` corre en el editor **y** en `Tools/HostTests` (CI en cada PR, con ASan/UBSan).
2. **Capa fina de Unreal** (`UActorComponent`/subsistema/actor): solo conecta el modelo con el
   mundo, la entrada y la UI. Esta parte se marca *sin compilar: verificar en local* hasta que
   pase `Tools/build.ps1` + `Tools/test.ps1` en la máquina con UE 5.6.

Un paquete está **hecho** cuando el modelo tiene specs en verde en el host y la capa de UE
compila y pasa sus tests en local.

## Estado por sistema (GDD §17.2)

Leyenda: **host ✅** = modelo puro con specs en verde en `Tools/HostTests` (CI en cada PR);
**UE ⚠** = capa de Unreal escrita pero *sin compilar: verificar en local*.

| Módulo | Modelo puro | Capa UE | PR | Pendiente |
|---|---|---|---|---|
| WorldGen / Scatter | host ✅ | ✅ | — | Revisión: M6, M7, M9–M11 |
| Ocean | host ✅ | ✅ | — | Flotabilidad de barcos (P-BOATS) |
| Sky / Luna / eventos | host ✅ `FMoonModel`, `FWorldEventsModel` | UE ⚠ `UWorldEventsSubsystem` | #7 | Enganchar océano (bioluminiscencia, bajamar extrema), `M_Stars`, erupción, hoguera de señal, obsidiana |
| Weather | host ✅ | ✅ | — | Categoría de ciclón en el modelo (la usa construcción) |
| Survival / cuerpo | host ✅ `FSurvivalModel`, `FBodyModel`, `FBodySignalsModel` | UE ⚠ `UBodySignalsComponent`, reloj | integración | Material `M_PP_Body` en `build_materials.py`; muerte y reaparición; calor del fuego, sombrero y sueño como entradas |
| Nado | host ✅ `FSwimModel` | UE ⚠ | #6 | Ajustar umbrales en PIE |
| Interaction / Carry / Crafting | host ✅ `FInventoryModel` | UE ⚠ contenedores, angarillas | integración | Plantillas de fabricación para mochilas, cinturón y angarillas; vista 3D de la mochila |
| Building | host ✅ `FBuildingModel` | UE ⚠ subsistema, pieza, vista previa | #11 | Consumir materiales (P-CARRY), importar mallas del kit, piezas de ventana/barandilla/hastial |
| Farming | host ✅ `FFarmModel` | UE ⚠ `UFarmSubsystem`, `AExploredPlantActor` | #8 | Objeto de compost, gasto de agua al regar, parcelas desde construcción |
| Cooking / fuego | host ✅ `FFireModel`, `FCookingModel` | UE ⚠ `AExploredFire` | #13 | Frescura en el inventario, calor → supervivencia, reaparición → GameMode |
| Cartography | host ✅ `FCartographyModel` | UE ⚠ componente + `SExploredMapSheet` | #9 | Mapa en las manos, mirador → boceto, Los Dientes por islote |
| Ruins / museo | host ✅ `FRuinsModel`, `FMuseumModel` | UE ⚠ subsistema + expositor | #12 | Actores de estatua/altar/canoa, `Notify*` desde la interacción, piezas de museo |
| Boats | host ✅ `FBoatModel` | UE ⚠ `AExploredBoat` | integración | Malla del «Limón» y del astillero; marea en `AExploredOcean`; dirección del viento en el clima |
| Fauna | host ✅ boids, cerebro marino, aparición, LOD | UE ⚠ gestor + criaturas | integración | Importar mallas de fauna y materiales animados por shader; daño → supervivencia |
| Fishing | host ✅ `FFishingModel`, tensión | UE ⚠ componente, trampas | integración | Aparejos como objetos; zonas de pesca legendaria; máscara de arrecife desde WorldGen |
| Save | host ✅ formato, ranuras, deltas | UE ⚠ `UExploredSaveSubsystem` | #14 | Registrar las secciones de cada sistema (P-WIRE) |
| Achievements | host ✅ `FAchievementsModel` | UE ⚠ subsistema + aviso | #10 | Que los sistemas reporten estadísticas (P-WIRE), pantalla de logros |
| Audio / música | host ✅ `FMusicDirectorModel`, flauta | UE ⚠ `UExploredMusicSubsystem`, M5 | integración | Re-renderizar audio (`sfx_flute_note`); `NotifyDiscovery`/`SetDanger` desde los sistemas |
| UI / frontend | host ✅ `SettingsLogic` | UE ⚠ H6, H7, M8, M11–M16 | #17 | Pantallas de mapa y museo, entrada «Logros» |
| Localización | host ✅ selector ES/EN | catálogo + archivos de Unreal | integración | Compilar `.locres` en el editor; nombres de objetos por cultura en el registro |

## Paquetes de trabajo

| Id | Paquete | Estado |
|---|---|---|
| P-HOST | Tests del host + CI | hecho (#5) |
| P-SWIM | Nado en modelo puro; H4, H5, M1, M4, M15 | hecho, sin compilar (#6) |
| P-EVENTS | Luna única + calendario de eventos | hecho, sin compilar (#7) |
| P-FARM | Limonero y huerto | hecho, sin compilar (#8) |
| P-MAP | Cartografía a mano | hecho, sin compilar (#9) |
| P-ACH | 30 logros y estadísticas | hecho, sin compilar (#10) |
| P-BUILD | Construcción por piezas, apoyo, integridad | hecho, sin compilar (#11) |
| P-RUINS | Ruinas, wayfinding, tesoros, museo | hecho, sin compilar (#12) |
| P-COOK | Fuego, cocina, conservación | hecho, sin compilar (#13) |
| P-SAVE | Formato, ranuras, deltas | hecho, sin compilar (#14) |
| P-UI | Arreglos de la revisión (H6, H7, M8, M11–M16, L3, L5, L6, L8) | hecho, sin compilar (#17) |
| P-INT | Rama de integración con todo fusionado y resuelto | hecho (#17) |
| P-BOATS | Balsa → canoa → balancín → «Limón» | hecho, sin compilar (#17) |
| P-FAUNA | Boids, fauna marina, percepción | hecho, sin compilar (#17) |
| P-FISH | Pesca con tensión, nasas, pozas | hecho, sin compilar (#17) |
| P-CARRY | Cinturón, bolsillos, mochila, contenedores, angarillas | hecho, sin compilar (#17) |
| P-BODY | Heridas, escorbuto, nutrición, señales corporales | hecho, sin compilar (#17) |
| P-MUSIC | Director de música adaptativa, flauta | hecho, sin compilar (#17) |
| P-L10N | Catálogo ES/EN y exportación a Unreal | hecho (#17) |
| P-WIRE | Conectar sistemas entre sí: secciones de guardado, estadísticas de logros, interacción con ruinas/mirador, calor del fuego → supervivencia, consumo de materiales | pendiente |
| P-UI2 | Pantallas: mapa en las manos, museo, logros, selector de ranura | pendiente |
| P-M9 | Equilibrado, rendimiento, empaquetado Win64 y página de tienda | pendiente |

## Lo que solo puede hacerse en local (con UE 5.6)

- Compilar y pasar `Tools/test.ps1` con cada capa de UE nueva.
- Ajustes de sensación en PIE (nado, barcos, cámara).
- Capturas de referencia por isla y hora (§18) y perfilado con Unreal Insights.
- Empaquetado Win64.
