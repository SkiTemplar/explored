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

| Módulo | Estado | Modelo puro | Capa UE | Notas |
|---|---|---|---|---|
| WorldGen | ✅ base | ✅ | ✅ | Arreglos pendientes M6, M7, M9–M11 de la revisión |
| Scatter | ✅ base | ✅ | ✅ | Recolección persistente pendiente (P-SAVE) |
| Ocean | ✅ base | ✅ | ✅ | Flotabilidad para barcos (P-BOATS) |
| Sky / tiempo | ✅ base | parcial | ✅ | Fases lunares en modelo (P-EVENTS) |
| Weather | ✅ | ✅ | ✅ | |
| Survival | ✅ modelo | ✅ | parcial | Heridas, escorbuto y HUD corporal (P-BODY) |
| Interaction / Carry / Crafting | ✅ base | parcial | ✅ | Mochila 3D, cinturón, angarillas (P-CARRY) |
| Nado | ✅ | ❌ | ✅ | Bugs H4, H5, M1, M4 → extraer a modelo (P-SWIM) |
| Building | ❌ | ❌ | ❌ | Datos en `building_pieces.json` (P-BUILD) |
| Farming | ❌ | ❌ | ❌ | Datos en `plants.json` (P-FARM) |
| Cooking | ❌ | ❌ | ❌ | (P-COOK) |
| Cartography | ❌ | ❌ | ❌ | Sistema central del juego (P-MAP) |
| Ruins / museo | ❌ | ❌ | ❌ | Mallas ya generadas (P-RUINS) |
| Boats | ❌ | ❌ | ❌ | Mallas en `Tools/Blender/props/boats.py` (P-BOATS) |
| Fauna | parcial | gait | ❌ | Bandadas, bancos, tiburones (P-FAUNA) |
| Fishing | ❌ | ❌ | ❌ | (P-FISH) |
| Events | ❌ | ❌ | ❌ | (P-EVENTS) |
| Save | stub | ❌ | stub | (P-SAVE) |
| Achievements | ❌ | ❌ | ❌ | 30 logros (P-ACH) |
| Audio | ✅ base | ✅ director de música y flauta | parcial | `UExploredMusicSubsystem` y arreglo M5 sin compilar (P-MUSIC); falta cablear descubrimientos, peligro y barcos |
| UI / frontend | ✅ base | — | parcial | Bugs H2, H6, H7, M8, M11–M14; mapa y museo (P-UI) |
| Localización | ❌ | — | ❌ | ES/EN (P-L10N) |

## Paquetes de trabajo

| Id | Paquete | Estado |
|---|---|---|
| P-HOST | Tests del host + CI | en curso (#5) |
| P-SWIM | `FSwimModel`: estados con histéresis (H5), apnea por profundidad (H4), oxígeno a FPS altos (M1), corrientes como velocidad (M4) | pendiente |
| P-BUILD | `FBuildingModel`: piezas, encaje por rejilla y sockets, grafo de apoyo, integridad, daño por viento/ciclón, reparación, coste y herramientas desde `building_pieces.json` | pendiente |
| P-FARM | `FFarmModel`: limonero y huerto por etapas y días, riego, estación, compost, cosecha desde `plants.json` | pendiente |
| P-MAP | `FCartographyModel`: trazo de costa con temblor, brújula, bocetos de mirador que se confirman al recorrer, marcas y sellos, catalejo, sextante, mojado y copia en limpio | pendiente |
| P-RUINS | `FRuinsModel` + `FMuseumModel`: ruinas completadas → técnicas de wayfinding, caminos de estrellas, tesoros, catálogo, exposición | pendiente |
| P-COOK | `FCookingModel`: niveles de fuego, vasijas, recetas, técnicas, conservación y deterioro | pendiente |
| P-EVENTS | `FWorldEventsModel`: fases lunares (12 días), desove, lluvia de estrellas, ballenas, barco en el horizonte, erupción, marea viva extrema | pendiente |
| P-BOATS | `FBoatModel`: flotación, remo, vela con viento aparente, balsa → canoa → balancín → «Limón» | pendiente |
| P-FAUNA | `FFlockModel` (boids) + máquinas de estados de fauna marina y percepción | pendiente |
| P-FISH | `FFishingModel`: minijuego de tensión, nasas y trampas | pendiente |
| P-SAVE | Archivo de guardado versionado: semilla + deltas del mundo + jugador + progreso; 3 ranuras + copia + autoguardado | pendiente |
| P-ACH | `FAchievementsModel`: 30 logros y estadísticas | pendiente |
| P-BODY | Heridas, escorbuto, nutrición y señales corporales del HUD | pendiente |
| P-CARRY | Cinturón, mochila con volumen, contenedores del mundo, angarillas | pendiente |
| P-UI | Arreglos H2/H6/H7/M8/M11–M14 y pantallas de mapa y museo | pendiente |
| P-MUSIC | Director de música adaptativa por capas | en curso (nube/musica-2026-09-26) |
| P-L10N | Tabla de textos ES/EN y selector | pendiente |
| P-M9 | Equilibrado, rendimiento, empaquetado Win64 y página de tienda | pendiente |

## Lo que solo puede hacerse en local (con UE 5.6)

- Compilar y pasar `Tools/test.ps1` con cada capa de UE nueva.
- Ajustes de sensación en PIE (nado, barcos, cámara).
- Capturas de referencia por isla y hora (§18) y perfilado con Unreal Insights.
- Empaquetado Win64.
