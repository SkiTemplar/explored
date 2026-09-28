# Estadísticas de juego (contrato para todos los sistemas)

Los logros (GDD §16; 30 de partida, hasta 54 según biblia 07 §2) no escuchan a ningún sistema concreto: cada sistema **informa de
lo que pasa** con un evento genérico de estadística y el modelo de logros decide. Este es el
único sitio que lista todas las estadísticas; la versión que lee el juego está en
`Content/Data/achievements.json` (`stats`) y `Tools/DataCheck` comprueba que ambas coinciden
(id, tipo y ámbito).

## Cómo informar

Desde cualquier `UObject` con mundo:

```cpp
#include "Achievements/AchievementsSubsystem.h"

if (UAchievementsSubsystem* Achievements = UAchievementsSubsystem::Get(this))
{
	Achievements->ReportStat(TEXT("fires_lit"));                                   // contador +1
	Achievements->ReportStat(TEXT("max_dive_depth_m"), DepthMeters);               // máximo
	Achievements->ReportStatItem(TEXT("islands_visited"), TEXT("Emerald"));        // conjunto
	Achievements->ReportStat(TEXT("coast_drawn"));                                 // marca
}
```

- **counter**: suma `Amount` (por defecto 1). Las cantidades ≤ 0 se ignoran.
- **max**: guarda `Amount` si supera el valor anterior. Se informa el **total actual**
  (días vividos, tesoros expuestos ahora), no el incremento.
- **set**: añade `Item` (un id); repetirlo no cuenta dos veces.
- **flag**: se marca la primera vez y ya no se desmarca.

Se puede informar tantas veces como se quiera (cada metro navegado, cada día): el modelo solo
revisa los logros que dependen de esa estadística y anuncia cada logro **una sola vez** en la
vida del perfil. Una estadística que no está en esta tabla se ignora (y la capa de Unreal
avisa en el log una vez).

**Ámbito:** `profile` acumula a lo largo de todas las partidas; `run` pertenece a la partida
en curso, se vacía al empezar otra (`BeginRun`) y viaja con su guardado. Los logros ya
conseguidos son siempre del perfil.

**Modos:** algunos logros solo cuentan en ciertos modos (`modes` en el JSON: `Explorer`,
`Survivor`, `Castaway`, `Custom`). Fuera de partida (menú) esos logros no se desbloquean.

**Fases:** cada logro lleva `phase` (`AA`, `F2`, `F3`, biblia 07 §2). El juego publica solo los
de su fase y las anteriores (`FAchievementsModel::SetReleasedPhase`); los demás no se
desbloquean ni salen en la lista, pero sus estadísticas se siguen contando, así que al llegar
la fase 2 un `Evaluate` desbloquea lo que ya se hubiera cumplido.

**Cooperativo:** `coopScope` dice a quién llega el logro cuando lo decide el servidor (biblia 08
§5.7): `actor`, solo a quien hace la acción; `world`, a todos los conectados; `witness`, a
quien esté a menos de 50 m del hecho (`FAchievementsModel::ReachesPlayer`). Las estadísticas
son siempre individuales: cuentan lo que ha hecho cada jugador en su propia cuenta.

## Catálogo

| Id | Tipo | Ámbito | Qué se informa | Quién lo informa | Ids admitidos | Logros |
|---|---|---|---|---|---|---|
| `fires_lit` | counter | profile | Hogueras y fogatas encendidas (cada encendido cuenta uno). | `AExploredFire` (suceso `Ignited`) | — | primer_fuego |
| `palms_climbed` | counter | profile | Veces que se trepa hasta la copa de un cocotero. | Pendiente: aún no se puede trepar | — | rey_del_cocotero |
| `coconuts_opened` | counter | profile | Cocos abiertos para beber o comer. | `AExploredCharacter::UseHand` (comer algo con etiqueta `coco`) | — | rey_del_cocotero, banquete_de_mil_cocos |
| `fish_caught` | counter | profile | Peces capturados con cualquier técnica (caña, arpón, red, nasa). | `UFishingComponent` (caña) y `AExploredTrap` (peces de nasa, no marisco) | — | primera_captura |
| `legendary_catches` | set | profile | Capturas legendarias conseguidas (biblia §4.6). | `UFishingComponent` (ids de `fish.json` → `legendary`) | `el_viejo`, `sombra`, `manta_negra`, `el_errante`, `rey_de_plata` | una_historia_que_contar |
| `max_dive_depth_m` | max | profile | Profundidad máxima alcanzada buceando, en metros. | `UExploredWiringSubsystem` (agua sobre la cámara al bucear) | — | pulmones_de_perla |
| `distance_sailed_m` | counter | profile | Metros recorridos a vela (no a remo). | `UExploredWiringSubsystem` (`ExploredLinks::FSailingOdometer`) | — | mar_abierto |
| `foods_eaten` | set | profile | Comidas distintas probadas. | `AExploredCharacter::UseHand` (lo que tiene ficha de comida en `recipes.json`) | ids de `items.json` | cocina_de_isla |
| `events_witnessed` | set | profile | Eventos del mundo presenciados (GDD §9.3): los de todo el mar siempre; los de una isla, a menos de 1,5 km de su costa. | `UExploredWiringSubsystem` (`ExploredLinks::IsEventWitnessed`) | `LexToString(EWorldEventType)`: `TurtleNesting`, `TurtleHatchlings`, `Bioluminescence`, `MeteorShower`, `WhalePassage`, `ShipOnHorizon`, `MinorEruption`, `ExtremeSpringTide` | luz_en_el_agua, madrugada_de_tortugas, canto_de_ballenas, deseos_a_punados |
| `cyclones_survived_intact` | counter | profile | Ciclones superados sin que ninguna pieza de la base pierda integridad. | `UExploredWiringSubsystem` (`ExploredLinks::FCycloneWatch`) | — | ojo_de_ciclon |
| `flute_played_by_fire` | counter | profile | Melodías tocadas con la flauta de bambú junto a un fuego encendido. | `UExploredWiringSubsystem` (`ExploredLinks::FFluteMelodyWatch`); pendiente: objeto flauta y su entrada | — | melodia_junto_al_fuego |
| `days_survived` | max | run | Días completos vividos en la partida actual (se informa el total). | `UExploredWiringSubsystem` (`ExploredLinks::DaysSurvived`) | — | diez_amaneceres, un_ano_de_islas |
| `islands_visited` | set | run | Islas pisadas. La isla oculta no cuenta: ver `hidden_island_reached`. | `UExploredWiringSubsystem` (`ExploredLinks::FindIslandAt`, en tierra) | `LexToString(EIslandArchetype)`: `Landing`, `Emerald`, `Smoke`, `Teeth`, `Mangrove`, `WhiteSands`, `Mesa` | tierra_firme, las_siete_islas |
| `islands_mapped` | set | run | Islas cuyo contorno está entero en trazo firme. | `UExploredWiringSubsystem` (`UCartographyComponent::OnIslandCharted`) | como `islands_visited` | cartografo, el_mapa_entero |
| `coast_drawn` | flag | run | Primer trazo de costa del mapa, también el automático al caminar por la orilla. | `UExploredWiringSubsystem` (`UCartographyComponent::OnFirstCoastDrawn`) | — | sin_mapa |
| `places_visited` | set | run | Lugares singulares visitados. | `UExploredWiringSubsystem` (`ExploredLinks::PlaceStatId`); pendiente: `cueva_aire` no tiene punto de interés | `crater_humo`, `tubo_lava`, `cascada_esmeralda`, `faro_dientes`, `estacion_halden`, `observatorio_mareas`, `pecio_velero`, `cueva_aire` | bajo_el_volcan |
| `museum_treasures_on_display` | max | run | Tesoros expuestos a la vez en estanterías y vitrinas (se informa el total actual). | `UExploredWiringSubsystem` (`URuinsSubsystem::OnMuseumChanged`) | — | coleccionista |
| `wayfinding_techniques` | set | run | Técnicas de navegación ancestral aprendidas (GDD §6.2). | `UExploredWiringSubsystem` (`URuinsSubsystem::OnRuinDiscovery`) | ids de `ruins.json` (`LexToString(EWayfindingTechnique)`): `star_path`, `swell_reading`, `birds_at_dusk`, `fixed_clouds`, `water_colour` | wayfinder |
| `albatros_parts_recovered` | set | run | Restos del Albatros recuperados. | Pendiente: Narrative / Carry (aún no se recogen piezas) | `fuselaje`, `ala`, `cola`, `motor` | restos_del_albatros |
| `building_pieces_built` | set | run | Piezas de construcción colocadas al menos una vez. | `UBuildingSubsystem::TryPlacePiece` | ids de `building_pieces.json` y piezas futuras de `fases_futuras.json` | primer_techo, primera_empalizada [F2], muralla_de_piedra [F2] |
| `building_tier_max` | max | run | Nivel de material más alto construido (`order` de `building_pieces.json`: 0 palma … 3 piedra). | `UBuildingSubsystem::TryPlacePiece` | — | cimientos_de_piedra |
| `crops_harvested` | set | run | Cultivos cosechados al menos una vez. | `UFarmSubsystem::Harvest` | ids de `plants.json` | el_limonero, huerto_en_flor |
| `boats_built` | set | run | Embarcaciones terminadas en el astillero. | `AExploredBoat` con `bBuiltByPlayer` (pendiente: el astillero que la crea) | ids de `boats.json`: `balsa`, `canoa`, `canoa_balancin`, `barco_limon` | limon_zarpa, primera_canoa |
| `hidden_island_reached` | flag | run | Desembarco en la isla oculta. | `UExploredWiringSubsystem` (a menos de 400 m del centro de la isla oculta) | — | limon_zarpa, naufrago_de_verdad, sin_mapa |
| `terrain_edits_made` | counter | run | Ediciones de terreno hechas (golpes de pala o pico que restan o suman densidad). | Pendiente: `Mining` (`FTerrainEditModel`) | — | primera_palada |
| `strata_mined` | set | run | Estratos de los que se ha extraído al menos una unidad. | Pendiente: `Mining` | ids de `mining.json → strata` | buscador_de_vetas, filo_de_obsidiana |
| `max_mining_depth_m` | max | run | Profundidad máxima cavada bajo la superficie, en metros. | Pendiente: `Mining` | — | topo_de_isla |
| `air_pocket_survived` | flag | run | El aire llega al mínimo en una bolsa cerrada y el jugador sale con vida. | Pendiente: `Mining` | — | el_aire_que_falta |
| `cave_collapse_avoided` | flag | run | Viga de apoyo colocada en una galería a punto de derrumbarse. | Pendiente: `Mining` / `Building` | — | viga_a_tiempo |
| `artifact_ids_found` | set | run | Tesoros encontrados por id, estén expuestos o no. | Pendiente: `URuinsSubsystem` (hallazgo) | ids de `artifacts.json` | juego_de_anzuelos |
| `underground_treasure_found` | flag | run | Primer tesoro encontrado en un templo enterrado. [F2] | Pendiente: `Ruins` / `Mining` | — | bajo_el_templo [F2] |
| `tools_broken_on_wrong_material` | counter | profile | Herramientas rotas por golpear un material más duro del que soportaban. | Pendiente: `Crafting` / `Harvest` | — | manazas |
| `crab_stole_item` | flag | profile | Un cangrejo se lleva un objeto dejado en la arena. | Pendiente: `Fauna` (el cangrejo aún no roba) | — | el_cangrejo_se_lo_llevo |
| `rail_track_and_cart_used` | flag | run | Vagón cargado movido por una vía tendida por el jugador. [F2] | Pendiente: `Tramway` (`FTramwayModel`) | — | primer_tren_de_isla [F2] |
| `livestock_species_raised` | set | run | Especies domésticas con al menos una cría nacida en un corral. [F2] | Pendiente: `Fauna` (granja) | ids de `fases_futuras.json → livestock.species`: `gallina`, `cerdo`, `cabra` | primera_pareja [F2], corral_completo [F2] |
| `eggs_collected` | counter | profile | Huevos recogidos del gallinero. [F2] | Pendiente: `Fauna` (granja) | — | huevos_por_docenas [F2] |
| `barter_trades_completed` | counter | profile | Trueques completados con el pueblo del arrecife. [F3] | Pendiente: `Villages` | — | primer_trueque [F3] |
| `reputation_village_tier` | max | run | Nivel de reputación alcanzado (0 neutral, 1 básica, 2 alta, 3 aliado de facto). [F3] | Pendiente: `Villages` | — | aliado_de_facto [F3] |
| `wayfinding_taught_by_village` | flag | run | Técnica de wayfinding aprendida directamente del pueblo. [F3] | Pendiente: `Villages` | — | otra_forma_de_aprender [F3] |
| `village_defended_from_raid` | flag | run | Asalto sobre el pueblo repelido con ayuda del jugador y sin que nadie de la sesión dispare una flecha (restricción de biblia 08 §5.7). [F3] | Pendiente: `Villages` / `Raiders` (bandera en el `GameState`) | — | sin_disparar_una_flecha [F3] |
| `raids_defended` | counter | profile | Asaltos piratas a la propia base repelidos sin pérdidas. [F3] | Pendiente: `Raiders` | — | asalto_repelido [F3] |
| `raider_camps_defeated` | counter | profile | Campamentos pirata derrotados. [F3] | Pendiente: `Raiders` | — | campamento_tomado [F3] |

Los ids de las listas cerradas son un contrato: si un paquete necesita otro nombre, cambia
la lista aquí y en `achievements.json` en el mismo commit (DataCheck lo comprueba).
Los conjuntos que reflejan otro catálogo usan sus ids tal cual y DataCheck exige que
coincidan: `wayfinding_techniques` con `ruins.json → techniques`, `legendary_catches` con
`fish.json → legendary`, `boats_built` con `boats.json`, `strata_mined` con
`mining.json → strata` y `livestock_species_raised` con `fases_futuras.json → livestock.species`. Los ids de los enums del C++
(`islands_visited`, `events_witnessed`) son su `LexToString`, igual que en el juego. La
traducción de cada sistema a estos ids está en `ExploredLinks` (`Core/SystemLinks.h`, con
specs en el host).

## Añadir una estadística o un logro

1. Añade la fila a esta tabla y la entrada a `stats` en `achievements.json`.
2. Si es para un logro, escribe su condición con el lenguaje del JSON (`units.condition`) y
   dale `phase` (`AA`/`F2`/`F3`), `rarity` (`comun`/`infrecuente`/`raro`/`muy_raro`) y
   `coopScope` (`actor`/`world`/`witness`). El total se queda entre 40 y 60 (biblia 07 §2 fija
   54). Una estadística que informa un sistema de F2/F3 lleva también `phase`: DataCheck no deja
   que un logro de una fase anterior dependa de ella.
3. `cd Tools/DataCheck && uv run datacheck` y `Tools/HostTests/run.sh Explored.Achievements`.
