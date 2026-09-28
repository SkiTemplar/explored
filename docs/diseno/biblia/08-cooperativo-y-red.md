# EXPLORED — Biblia de diseño · 08. Cooperativo y red

Versión 1 · 2026-09-27 · Decisión del director del 2026-09-27: **el juego tendrá
cooperativo de 2 a 4 jugadores en el acceso anticipado**, con **servidor de escucha**
(uno de los jugadores hospeda) a través de **Steam** (Online Subsystem Steam + Steam
Sockets). Se decide ahora, antes de escribir la mayoría de los sistemas que faltan,
para que **todo sistema nuevo nazca preparado para red** y no haya que reconvertirlo
dos veces.

Esta sección **deroga** la regla «sin multijugador» de `gdd_v2.md` §7.2 y añade el
cooperativo al alcance de acceso anticipado de `gdd_v2.md` §6.2. Manda el GDD en el
alcance; esta sección manda en el detalle técnico y de diseño del cooperativo, igual
que el resto de la biblia (ver `00-indice.md`, «Precedencia»).

**Estado real del código al escribir esto:** cero ficheros con replicación. `grep -rl
"Replicated\|GetLifetimeReplicatedProps\|Server_\|NetMulticast" Source/` no devuelve
ninguna coincidencia funcional; `Explored.Build.cs` no depende de ningún módulo de
Online Subsystem; no hay `GameState` ni `PlayerState` propios (solo
`AExploredGameMode : AGameModeBase`). Todo lo que sigue es trabajo por hacer, y está
estimado como tal en §9.

Fases: **[H0]**–**[H5]** son los hitos de `00-TODO.md`; **[AA]** es el acceso
anticipado completo; **[F2]**/**[F3]** heredan el recorte de `gdd_v2.md` §6.2.

---

## 1. Modelo de autoridad

### 1.1 Topología

| Decisión | Valor | Por qué |
|---|---|---|
| Topología | Servidor de escucha (`listen server`) | Sin coste de servidores dedicados; el anfitrión ya tiene el mundo cargado en memoria, que es el recurso caro (terreno volumétrico + deltas). |
| Transporte | `SteamSockets` (relay de Steam Datagram) sobre `OnlineSubsystemSteam` | Atraviesa NAT sin que el anfitrión abra puertos; oculta la IP del anfitrión. `SteamNetDriver` como reserva si el relay falla. |
| Jugadores | 2–4 (tope duro 4; el mundo de 1 jugador es el mismo código con `Standalone`) | 4 es donde el presupuesto de ancho de banda de §3 y el tope de fauna replicada siguen cumpliéndose sin recortar el mundo. |
| Migración de anfitrión | **No hay** en acceso anticipado | Transferir el estado de terreno de una partida madura (§2.2) a otra máquina en caliente no cabe en el presupuesto; se documenta la alternativa honesta en §4.5. |
| Dedicado | Fuera de alcance (candidato a 1.0) | El mismo código lo admitiría (`AExploredGameMode` no asume pawn del anfitrión tras §2.1), pero no se prueba ni se empaqueta en AA. |

Configuración de proyecto que hay que añadir (`Config/DefaultEngine.ini`):

```ini
[/Script/Engine.GameEngine]
+NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="SteamSockets.SteamSocketsNetDriver",DriverClassNameFallback="OnlineSubsystemUtils.IpNetDriver")

[/Script/OnlineSubsystemSteam]
bEnabled=true
SteamDevAppId=480          ; sustituir por el AppId real antes de la beta cerrada
bInitServerOnClient=true

[/Script/Engine.NetDriver]
NetServerMaxTickRate=30
MaxClientRate=32000
MaxInternetClientRate=32000

[/Script/Engine.Player]
ConfiguredInternetSpeed=32000
```

`32000` B/s = **256 kbps**, exactamente el techo de pico de §3. `NetServerMaxTickRate=30`
fija el presupuesto por paquete: 32000 / 30 ≈ **1066 bytes por tick** en el peor caso,
y ≈ 266 bytes por tick en reposo (8000 B/s).

`Explored.Build.cs` gana: `OnlineSubsystem`, `OnlineSubsystemSteam`, `OnlineSubsystemUtils`,
`SteamSockets`, `NetCore`.

### 1.2 Servidor autoritativo, con predicción acotada

**Regla dura: el servidor es la única fuente de verdad de todo el estado del mundo y
del cuerpo.** El cliente predice exactamente dos cosas:

1. **Movimiento propio** — `UCharacterMovementComponent` con su predicción y corrección
   estándar de Unreal (nado incluido: `USwimComponent` pasa a ser un `MovementMode`
   personalizado o a alimentar `CustomMovementMode`, no una capa paralela que el
   servidor no vea). Parámetros: `NetworkSmoothingMode = Exponential` para pawns
   ajenos, `ClientNetSendMoveDeltaTime` 1/30, corrección aceptada sin reproducción
   visible por debajo de **8 cm** de error.
2. **Acciones instantáneas** — golpe de herramienta, recoger del suelo, encender un
   fuego, soltar un objeto. El cliente reproduce **de inmediato** el efecto audiovisual
   (animación, sonido, partículas, retroceso del pico) y **no** el efecto sobre el
   mundo. La consecuencia (el hueco en el terreno, el objeto en el inventario, el árbol
   que cae) llega del servidor. Así el golpe se siente inmediato sin poder desincronizar
   nunca el mundo.

**Nada más se predice.** En concreto no se predicen: inventario, terreno, tala,
construcción, barcos, fauna, reloj, clima, necesidades, muerte. El coste de equivocarse
en cualquiera de ellos (un hueco fantasma, un objeto duplicado) es mucho peor que los
80–120 ms de latencia que ahorra la predicción.

**Ventana de gracia en el servidor** para acciones instantáneas: el servidor acepta una
acción cuyo objetivo el cliente tenía a la vista hasta **250 ms** antes (una
retroproyección sencilla del foco, no `lag compensation` completa de hitscan). Fuera de
esa ventana, la rechaza en silencio y el cliente vuelve al estado del servidor.

**Validación en el servidor de toda acción** (ninguna se da por buena por venir del
cliente):

| Comprobación | Valor |
|---|---|
| Distancia al objetivo | ≤ `UInteractionComponent::TraceDistanceCm` (250 cm) + 50 cm de margen de latencia |
| Línea de visión | Trazado del servidor desde la cámara del cliente al objetivo |
| Cadencia de golpe | ≥ `FTerrainEditModel::SecondsPerPickaxeHit` (0,9 s) − 15 % de tolerancia; más rápido = descartado |
| Cadencia de interacción genérica | ≥ 0,25 s por jugador |
| Herramienta en mano | El servidor lee **su** copia del inventario, nunca la que afirma el cliente |
| Presupuesto de acciones | 20 acciones válidas/s por jugador; por encima, se encola y se avisa en el registro del anfitrión |

### 1.3 Tabla de autoridad por sistema

| Sistema | Quién decide | Quién simula | Qué ve el cliente |
|---|---|---|---|
| Movimiento del propio personaje | Servidor (corrige) | Cliente + servidor | Predicción + corrección |
| Movimiento de los otros | Servidor | Servidor | Interpolación suavizada |
| Necesidades y heridas (`FSurvivalModel`, `FBodyModel`) | Servidor | Servidor | Réplica de su propio cuerpo (§2.9) |
| Inventario (`UCarryComponent`, `FInventoryModel`) | Servidor | Servidor | Réplica solo del propio |
| Terreno editable (`FTerrainEditModel`) | Servidor | Servidor | Deltas replicados + remallado local (§2.2) |
| Tala y recolección (`FFellingModel`, `FHarvestModel`) | Servidor | Servidor | Estado por instancia (§2.3) |
| Arena viva (§2.6) | Servidor | **Solo servidor** | Deltas de terreno normales |
| Construcción (`FBuildingModel`) | Servidor | Servidor | Actores replicados + fantasma local (§2.10) |
| Barcos (`FBoatModel`) | Servidor | Servidor | Estado replicado + extrapolación (§2.5) |
| Olas y corrientes (`FOceanWaves`) | Nadie: función pura | **Cada cliente, local** | Idéntico por construcción (§2.5) |
| Fauna marina (boids, `FMarineCreatureBrain`) | Servidor para el daño | Cada cliente, con ancla (§2.7) | Local, corregido |
| Fauna terrestre cazable | Servidor | Servidor | Actor replicado |
| Reloj, estaciones, mareas (`UTimeOfDaySubsystem`) | Servidor | Servidor + cliente en paralelo | Sincronización a 0,2 Hz (§2.8) |
| Clima (`FWeatherModel`) | Servidor | Servidor | Muestra replicada a 0,2 Hz |
| Cartografía (`UCartographyComponent`) | Servidor | Servidor | Hoja **compartida** (§5.3) |
| Museo, ruinas, progreso (`FMuseumModel`, `FRuinsModel`) | Servidor | Servidor | Réplica compartida |
| Logros (`UAchievementsSubsystem`) | Servidor decide el hecho; cada cliente lo desbloquea en su Steam | Cliente | §5.7 |
| Guardado (`UExploredSaveSubsystem`) | **Solo el anfitrión** | Anfitrión | §4.4 |

---

## 2. Qué se replica y cómo, sistema por sistema

### 2.1 Cimiento: partir el jugador único en cuatro

Antes de replicar nada hay que deshacer el supuesto de **un solo jugador**, que hoy
está cableado en 21 sitios. Los más graves:

- `UExploredWiringSubsystem::GetPlayerCharacter()` (`ExploredWiringSubsystem.cpp:241`)
  devuelve `World->GetFirstPlayerController()->GetPawn()`. El subsistema entero
  (`Sample`, `UpdateBody`, `UpdatePlace`, `UpdateBoat`, `UpdateDanger`, 1714 líneas)
  está escrito contra ese personaje único, y es el mismo que registra **once** secciones
  de guardado.
- `UGameplayStatics::GetPlayerController(World, 0)` / `GetPlayerPawn(this, 0)` en
  `ExploredAmbienceSubsystem`, `ExploredMusicSubsystem`, `ExploredOcean`,
  `ExploredSkyController`, `ExploredPlantActor`, `ExploredFaunaManager`,
  `ExploredShotSubsystem`.

Reparto de responsabilidades tras la conversión:

| Dónde vive | Qué | Criterio |
|---|---|---|
| `AExploredGameState` (**nuevo**) | Reloj, clima, marea, semilla, `WorldId`, contador de amenaza [F3], escalado por número de jugadores (§5.6) | Uno por partida, replicado a todos |
| `AExploredPlayerState` (**nuevo**) | Nombre visible, `SteamID64`, color de tinta del mapa (§5.3), estado `Derribado` (§5.2), ping | Uno por jugador, replicado a todos |
| `UExploredWiringSubsystem` | Enlaces de mundo (fuegos, eventos, tormentas) | Sigue siendo uno, pero sin personaje propio |
| `UExploredPlayerLinksComponent` (**nuevo**, en el personaje) | Todo lo que hoy hace `Sample`/`UpdateBody`/`UpdatePlace`/`UpdateBoat`/`UpdateDanger` para *un* jugador | Uno por personaje; el subsistema de enlaces itera `GameState->PlayerArray` |
| Cliente local, sin réplica | Cámara, ambiente sonoro, música, cielo, HUD | Cada cliente usa **su** `LocalPlayer`, no el 0 |

Regla de estilo que sale de aquí y aplica a todo código nuevo: **ningún sistema vuelve
a llamar a `GetPlayerController(World, 0)`**. Para «el jugador local» se usa
`GetWorld()->GetFirstLocalPlayerFromController()` desde una clase de cliente; para «los
jugadores» se itera `GameState->PlayerArray`.

### 2.2 Ediciones del terreno: deltas replicados por chunk

Es el sistema con más riesgo y el que mejor encaja, porque `FTerrainEditModel` ya está
escrito de la forma correcta para red:

- Guarda **muestras editadas, no operaciones** («deltas de densidad en milímetros
  enteros» sobre rejilla de 0,25 m, chunk de edición de 8 m = 32³ = 32 768 muestras).
  Consecuencia de red: el estado es **idempotente y conmutativo** — reenviar la misma
  muestra dos veces no acumula error, y dos ediciones sobre el mismo chunk se pueden
  **fusionar** antes de enviarlas quedándose con el valor final. No hace falta orden
  total ni reproducción de operaciones.
- `FTerrainEditResult::DirtyChunks` ya dice exactamente qué remallar, y
  `ChunksReadingSample` ya resuelve el solape de 1 muestra entre chunks vecinos.

**Formato de cable** (`FExploredTerrainDeltaPacket`, RPC fiable servidor → cliente):

```
uint8   Version           // 1
int16   ChunkX, ChunkY, ChunkZ
uint16  NumRuns
por tramo:  uint16 FirstSample (0..32767)   // índice lineal en el chunk
            uint8  Count (1..255)
            int16  Delta[Count]            // milímetros, mismo entero que el guardado
```

Cabecera 9 B + 3 B por tramo + 2 B por muestra. **Tope duro por paquete: 512 bytes**
(≈ 240 muestras), por debajo de `MaxPacketSize` para que nunca fragmente. Una edición
más grande se parte en varios paquetes del mismo chunk, en orden.

Implementado y probado en el host: `FTerrainDeltaCodecModel` (códec, `DecodeAndApply`
atómico), `FTerrainDeltaQueueModel` (la cola de abajo) y `FTerrainChunkChecksumModel` (la
comprobación de cada 30 s). **Límite del `int16`:** un delta de más de ±32,767 m no cabe.
Como la densidad base es una distancia a la superficie, una galería a más de ~32 m de
profundidad lo supera; el códec rechaza esas muestras y las cuenta en vez de truncarlas.
Pendiente de decidir: acotar el delta en `FTerrainEditModel` o subir de formato.

**Versión 2, con capa** (para la arena de §2.6, que no es densidad sino un campo de
alturas de 32×32 columnas por chunk de 8 m; ver 02 §5.1):

```
uint8   Version           // 2
uint8   Layer             // 0 = densidad (igual que la versión 1), 1 = arena
uint8   Flags             // bit 0: vaciar el chunk antes de aplicar (chunk completo)
int16   ChunkX, ChunkY, ChunkZ   // arena: ChunkZ = 0
uint16  NumRuns
por tramo:  uint16 FirstSample (arena: 0..1023, índice de columna)
            uint8  Count (1..255)
            int16  Delta[Count]   // mm de altura absolutos, también 0
```

Cabecera 11 B y el mismo tope de 512 B. Los valores son absolutos, así que la capa de
arena conserva la idempotencia y la fusión por chunk de arriba (el valor final gana), y
la comprobación de cada 30 s es el mismo FNV-1a sobre los 1 024 deltas del chunk.
Implementado y probado en el host: `FSandModel::EncodePackets`, `EncodeFullChunk`,
`ApplyPacket` y `ChunkChecksum`.

**Volumen real por herramienta** (medido sobre la geometría del propio modelo, no
estimado a ojo):

| Acción | Muestras que cambia | Tramos | Bytes en crudo | Cada | kbps en crudo | kbps con compresión ×2,5 |
|---|---|---|---|---|---|---|
| Golpe de pico (r 0,5 m ± 20 %) | ≈ 50 | ≈ 12 | ≈ 145 | 0,9 s | 1,3 | **0,5** |
| Pasada de pala (r 1 m + borde 0,75 m) | ≈ 460 | ≈ 42 | ≈ 1 060 | 1,2 s | 7,1 | **2,8** |
| Echar tierra (esfera 0,5 m) | ≈ 50 | ≈ 12 | ≈ 145 | 1,2 s | 1,0 | **0,4** |
| Escalón de escalera (uno) | ≈ 120 | ≈ 20 | ≈ 300 | por golpe | 2,7 | **1,1** |

**Compresión:** los deltas son enteros de milímetros con muchísima redundancia local.
Se activa `OodleNetwork` con diccionario entrenado sobre una captura de 10 minutos de
minería (`Tools/net-dictionary.ps1`, nuevo). Factor que se asume en el presupuesto:
**×2,5 conservador**; medido antes de cerrar H2 y anotado en `docs/tecnico/red.md`. Si
no llega a ×2, se baja el tope de la cola (abajo) en vez de tocar el diseño.

**Cola y tope por cliente:**

- Cola FIFO por cliente, **una entrada por chunk**. Si el mismo chunk se vuelve a
  editar antes de que salga su paquete, se **fusionan** las muestras (el valor final
  gana). Correcto por la idempotencia de arriba.
- Tope de salida de la capa de terreno: **8 KB/s (64 kbps) por cliente**, y **16 KB/s
  (128 kbps)** durante 5 segundos como ráfaga. Pasado el tope, la cola prioriza por
  distancia al jugador receptor: primero los chunks a menos de 30 m, luego el resto.
- **Relevancia:** un cliente solo recibe chunks de edición dentro de **120 m** de su
  personaje o de cualquier chunk de terreno que tenga cargado por World Partition. Lo
  que edita el anfitrión en otra isla no cuesta nada hasta que el cliente se acerca.
- **Al acercarse a un chunk nunca recibido** (o al unirse en caliente, §4.3) el servidor
  manda el chunk completo con el **mismo formato de tramos**, que es el mismo que
  `FTerrainEditModel::ToValue` ya produce para el guardado. Un chunk con una galería
  entera ocupa ≈ 6–9 KB en crudo, ≈ 3 KB comprimido: se manda en 6 paquetes de 512 B
  a lo largo de 0,2 s.

**Remallado en el cliente:** el cliente aplica los deltas a su propio
`FTerrainEditModel` y llama a `FTerrainChunkBuilder::Build` sobre los chunks sucios,
exactamente igual que el servidor. Agrupación: remallado **cada 250 ms** como máximo
por chunk (coalescencia de varios golpes), con el mismo criterio que ya pide
`gdd_v2.md` §7.4. El cliente **nunca** aplica su propia edición antes de recibirla del
servidor: lo que ve inmediatamente es la animación y las partículas del golpe (§1.2).

**Verificación de integridad:** cada 30 segundos el servidor manda, por chunk editado
cargado en ese cliente, un `uint32` de comprobación (FNV-1a de las muestras del chunk).
Si el cliente no coincide, pide el chunk completo. Coste: 4 B × chunks editados
visibles (rara vez más de 40) cada 30 s ≈ 5 B/s. Es el seguro contra un paquete
fiable perdido por un error de lógica, y sale casi gratis.

### 2.3 Vegetación talada: estado por instancia

Hoy el estado vive en `FVegetationRuntimeState` indexado por
`FVegetationInstanceKey { FIntPoint Cell, FName Component, int32 Index }` y persiste en
la capa `"harvested"` de `FSaveWorldDeltas`. `FName` no se pone en el cable.

**Clave de red** (7 bytes): `int16 CellX, int16 CellY, uint8 Species, uint16 Index`.
`Species` es el índice del HISM dentro de la celda (0–15), resuelto por una tabla
ordenada y determinista que sale de los mismos datos en las dos puntas.

**Estado de red** (3 bytes): `uint8` empaquetado con etapa
(`Intacta`/`Talandose`/`Tocon`/`Brote`, 2 bits) + golpes acumulados (4 bits, tope 15) +
`uint16 RegrowAtQuarterDays` (día total × 4: 0,25 días de resolución, hasta 16 383
días — muy por encima de una partida).

Total **10 B por instancia**, + 4 B de sobrecarga de `FFastArraySerializer` = **14 B por
cambio**. Se replica como `FFastArraySerializer` en el `GameState`, así que solo viajan
las entradas que cambian.

| Situación | Coste |
|---|---|
| Talar un árbol (8–14 golpes) | 1 entrada por golpe **solo a clientes a < 60 m** + 1 final a todos: ≈ 140 B en 10 s = **0,11 kbps** |
| Desbrozar un arbusto (1 golpe) | 14 B |
| Tocón que rebrota | 14 B, una vez |
| Recoger una `rama_seca` | 14 B |
| Entrar en una celda nueva | Snapshot de la celda con **el mismo texto del guardado** (`FSaveIndexSet::Encode`, `"r:0-4,9,12-40"` o base64): 20–200 B por celda |

**Tope:** el array replicado no pasa de **4096 instancias modificadas**. Al llegar, el
servidor compacta: las instancias en estado `Tocon` sin rebrote pendiente salen del
array y pasan a formar parte del snapshot por celda (el mismo camino que el guardado).
Una partida que tale 4096 árboles ya está más allá de cualquier sesión razonable, pero
el tope existe para que el coste sea acotado y no «probablemente pequeño».

### 2.4 Inventario

**Solo el dueño ve su inventario.** `UCarryComponent` replica con `COND_OwnerOnly`; el
resto de jugadores solo ven **lo que hay en las manos** (para dibujar la malla y
resolver «llevo un pico» a la vista), replicado a todos como 2 × (`uint16` id de
definición + `uint8` calidad) = 6 B.

Estructura replicada del inventario propio: `FFastArraySerializer` de entradas de
**13 bytes**:

```
uint8  Slot           // EInventorySlot
uint8  SlotIndex
uint16 DefinitionId   // índice en la tabla de items.json, no FName
uint32 InstanceId     // el id estable que ya usa FInventoryModel
uint8  Quality01x255
uint8  Durability01x255
uint8  Count           // apilado (biblia 03 §1.3, tope 10)
uint8  Flags           // mojado, encendido, etc.
```

Coste: fabricar mueve 2–3 huecos → **39 B por operación**; coalescido a 10 Hz da un
techo de **3 kbps** para el dueño mientras fabrica a máquina, y **0** en reposo. Un
inventario completo (24 huecos) son 312 B: lo que se manda al unirse.

**Piezas y nombre generado** de un objeto fabricado (que `UCarryComponent` guarda
aparte de `FInventoryModel`) no van en el array: se piden por RPC fiable la primera vez
que ese `InstanceId` aparece, y el cliente los cachea. Un objeto compuesto son ≈ 40 B,
una sola vez en su vida.

**Contenedores del mundo** (`AExploredContainer`): el contenido **no** se replica
hasta que alguien lo abre. Al abrirlo, `Server_SubscribeContainer` y el servidor manda
el contenido y las actualizaciones mientras siga abierto; al cerrarlo, se da de baja.
**Dos jugadores en el mismo cofre:** permitido; el servidor es el árbitro y las
operaciones de `FInventoryModel` ya son atómicas («si falla, el estado no cambia y se
devuelve el motivo», `InventoryModel.h`). Quien pierde la carrera recibe
`EInventoryFail::NotFound` y el hueco se le refresca. Sin bloqueos, sin colas.

**Tabla de ids de contenido y verificación de versión.** `DefinitionId` es un `uint16`
sacado de ordenar los ids de `Content/Data/items.json`; lo mismo para plantillas,
piezas de construcción, plantas, barcos y logros. Para que las dos puntas coincidan
siempre, el saludo de conexión lleva un `FExploredContentHash`: FNV-1a de 64 bits de
todos los `Content/Data/*.json` concatenados en orden de nombre. Si no coincide, el
servidor rechaza la conexión con el texto de §6.5. Es la única defensa realista contra
un cliente con datos distintos, y cuesta 8 bytes.

### 2.5 Barcos y física

`FBoatModel` es la pieza mejor preparada de todo el proyecto para red: paso fijo
`FixedStepS = 1/60`, **cinemático** (`Hull->SetSimulatePhysics(false)`,
`ExploredBoat.cpp:100`), y su entrada es `FBoatControls` + `FBoatEnvironment`. No hay
Chaos que sincronizar.

- **Simula el servidor.** Punto. El timonel manda sus controles
  (`Server_SetBoatControls`, sin fiabilidad, 20 Hz, 4 B: escota `uint8`, timón `int8`,
  banderas `uint8`, palada `uint8`).
- **Se replica el estado, no la transformación** (`FExploredBoatNetState`, 20 Hz, no
  fiable, último gana):

| Campo | Tipo | Bytes |
|---|---|---|
| Posición | `FVector_NetQuantize100` | 7 |
| Velocidad horizontal | `FVector2D` cuantizada a 10 cm/s | 4 |
| Rumbo | `uint16` (0,0055°) | 2 |
| Escora | `int8` (grados) | 1 |
| Vela izada + trimado | `uint8` | 1 |
| Agua embarcada (`SwampWaterKg`) | `uint16` (×0,1 kg) | 2 |
| Integridad de casco | `uint8` | 1 |
| Reservado (piezas dañadas, H3) | `uint8` | 1 |
| **Total** | | **19 B** |

  19 B × 20 Hz = 380 B/s = **3,0 kbps por barco ocupado**. `NetCullDistanceSquared` de
  25 000 cm (250 m): un barco lejano no cuesta nada.
- **El cliente extrapola** con el mismo `FBoatModel::Step` que el servidor, alimentado
  con los últimos controles conocidos, y corrige hacia el estado recibido en 200 ms
  (interpolación crítica, sin rebote). Porque el modelo es determinista y de paso fijo,
  la extrapolación de 100 ms de latencia se equivoca por centímetros.
- **Olas y corrientes no se replican en absoluto.** `FOceanWaves::HeightAt(Position,
  Time)` es una función pura de 4 ondas de Gerstner; `FOceanCurrents` lo mismo. El
  cliente las calcula localmente a partir del **tiempo de ola** y del **estado de mar**
  (`SeaState`), que ya viajan en el paquete de clima de §2.8 (0,2 Hz). Coste: cero.
  Esta es la mayor economía de todo el diseño y sale gratis porque el código ya está
  escrito como funciones puras.
- **Pasajeros:** `AttachToActor` replicado; el movimiento del pasajero se resuelve en el
  espacio del barco y no añade tráfico más allá de su actualización normal de personaje.
  Aforo por plano canónico: **balsa 2, canoa 2, canoa con balancín 3, «Limón» 4**. Cada
  tripulante pesa `FBoatModel::CrewMassKg` = 75 kg en `TotalMassKg`, así que cuatro
  jugadores en una canoa **la anegan de verdad** (§2 de esta tabla no es cosmética: la
  hidrostática de 02 §8.2 ya castiga el exceso). Es contenido emergente, no un bug.
- **La única física de Chaos del juego es el objeto soltado**
  (`ExploredItemActor.cpp:15`, `SetSimulatePhysics(true)`). Decisión: **simula el
  servidor**, `bReplicateMovement = true` a 10 Hz y **se duerme** (`PutRigidBodyToSleep`)
  a los **3 segundos** de quedarse quieto; dormido deja de replicar por completo.
  `NetCullDistanceSquared` 6 000 cm (60 m). Tope de objetos sueltos despiertos a la vez:
  **32**; al llegar, los más antiguos se duermen a la fuerza en su posición actual. Sin
  esto, un jugador que vuelca una cesta de 40 objetos es un pico de tráfico de varios
  segundos.

### 2.6 Arena viva

`02-mecanicas-del-mundo.md` §5.1 pide una revisión de pendiente **una vez por segundo
por chunk activo**, con deslizamiento a la celda vecina más baja. Es una simulación con
estado que consulta densidad base + deltas.

**Decisión: solo la simula el servidor.** Los motivos son dos y los dos son de fondo:

1. El deslizamiento depende del **orden** en que se visitan las celdas (una avalancha es
   una cascada), y de la densidad base en `float`. Un `float` calculado en dos máquinas
   distintas con optimizaciones distintas no garantiza el mismo bit, así que «de forma
   determinista en cada cliente» sería deriva garantizada a los pocos minutos.
2. Su salida son **exactamente** deltas de terreno. Ya tenemos un canal barato para eso
   (§2.2), así que no hace falta inventar nada. La arena va en la capa 1 del paquete
   versión 2 de §2.2: mismo canal, misma cola, mismo formato de tramos.

Reglas de presupuesto, porque una playa que se derrumba podría inundar el canal:

- El servidor solo revisa chunks de edición **a menos de 80 m de algún jugador** (fuera
  de eso la arena se congela, y al acercarse se resuelve de golpe con un máximo de 4
  iteraciones acumuladas: una playa no «recuerda» diez minutos de avalancha).
- Tope de **64 celdas movidas por segundo y por chunk**; el resto espera al siguiente
  segundo. Una avalancha grande tarda más en asentarse, que es lo que hace de verdad.
- Los deltas de arena entran en la **misma cola** de §2.2 con **prioridad más baja** que
  los del jugador: si el canal está lleno porque alguien está cavando, la arena se
  asienta un poco más tarde. Nadie lo nota; una edición del jugador que llegue tarde sí
  se nota.
- Presupuesto medido: 64 celdas/s × 2 B + tramos ≈ 200 B/s = **1,6 kbps** en crudo,
  **0,6 kbps** comprimido, y solo mientras hay una avalancha en curso.

El relleno por oleaje de 02 §5.2 (20 %/35 % por medio ciclo de marea) es más fácil: se
resuelve **una vez por medio ciclo** (≈ 6 h de juego = 10 minutos reales), en el
servidor, y son unos pocos cientos de bytes cada vez.

### 2.7 Fauna e IA

Hay dos clases de fauna con presupuestos muy distintos.

**(a) Fauna de ambiente: marina, bandadas, insectos.** Hasta
`MaxFishPerSchool = 80` por banco. Replicar 80 peces por banco es inviable y además
innecesario: `FFaunaSpawning::SpawnsForCell(WorldSeed, Cell, Context, Out)` ya es
determinista a partir de la semilla del mundo.

**Decisión: no se replica ninguna. Cada cliente la simula localmente**, con la misma
semilla de mundo y el mismo reloj, y con **corrección de ancla**: el servidor manda,
por grupo (banco, bandada, enjambre) y **cada 2 segundos**, un ancla de 10 bytes
(`uint16` id de grupo, `FVector_NetQuantize100` del centroide, `uint8` estado del
cerebro). El cliente arrastra su grupo hacia el ancla en 1 s. Se replica el **grupo**,
no el individuo, porque lo que el jugador percibe es dónde está el banco, no cuál es el
pez 37.

Es correcto hacerlo así y no «determinista puro» porque `FMarineCreatureBrain` lleva
`RandomCounter` interno: su trayectoria depende de su historia, y la historia de dos
máquinas divergirá. El ancla acota la divergencia a algo invisible sin exigir
determinismo bit a bit, que nunca se conseguiría.

**Lo que sí es autoritativo del servidor:** el daño. `FMarineCreatureBrain::
ReefSharkAttackRoll(Seed, Encounter, Smell, bPeaceful)` lo tira **solo el servidor**,
una vez por jugador y encuentro — cada nadador se juega su propia tirada, no hay una
tirada compartida. El mordisco llega como RPC al jugador afectado y como multicast
cosmético a los cercanos.

Coste: grupos dentro de 150 m, tope **24 grupos** × 10 B × 0,5 Hz = 120 B/s =
**0,96 kbps**.

**(b) Fauna terrestre cazable** (cerdo salvaje, cabra montés, cangrejo de los
cocoteros; y en [F3] piratas y aldeanos, que reutilizan la misma percepción):
**actores replicados normales**, simulados en el servidor, porque el jugador les
dispara y los despieza y ahí no cabe divergencia.

- `NetUpdateFrequency` 10 Hz cerca (< 60 m), 2 Hz lejos, `NetCullDistanceSquared`
  15 000 cm (150 m).
- Estado replicado: 14 B (posición cuantizada 7 B, rumbo 2 B, `uint8` estado de la
  máquina, `uint8` salud, `uint8` banderas, `uint8` especie).
- **Tope duro: 12 animales terrestres replicados a 10 Hz y 24 a 2 Hz por cliente.**
  Presupuesto: 12 × 14 × 10 + 24 × 14 × 2 = 1 680 + 672 = 2 352 B/s = **18,8 kbps**.
  Es la partida más cara del presupuesto en reposo y el motivo por el que el tope es
  duro: `FFaunaLod` ya tiene los tres niveles (Full 40 m / Reduced 150 m / Frozen) y la
  histéresis del 10 %; la red se engancha a ese LOD, no inventa otro.
- La **navegación** sobre terreno editable se reconstruye **solo en el servidor** (los
  clientes no navegan nada), lo que elimina de golpe el riesgo de `gdd_v2.md` §7.4 en el
  lado cliente.

### 2.8 Ciclo de día, noche, clima y mareas

`UTimeOfDaySubsystem` ya tiene `DayLengthMinutes = 40.0f` y `TimeScale`, y
`ExploredSky::MoonPhase(TotalDays)` es puro. `FOceanWaves`, `FWeatherModel::SampleAt` y
`FBoatModel::TideOffsetCm(TotalDays, MoonPhase01)` son funciones del tiempo.

**Se replica el reloj, no sus consecuencias.** `AExploredGameState` lleva:

| Campo | Tipo | Bytes | Frecuencia |
|---|---|---|---|
| `Day` | `uint16` | 2 | 0,2 Hz (cada 5 s) |
| `Hours` | `uint16` (×0,001 h ≈ 3,6 s) | 2 | 0,2 Hz |
| `TimeScale` | `uint8` (×0,5, 0–127) | 1 | al cambiar (§5.1) |
| `Season` + fenómeno de clima | `uint8` | 1 | al cambiar |
| `Wind01`, `WindFromDeg` | `uint8` + `uint8` | 2 | 0,2 Hz |
| `Rain01`, `SeaState01` | `uint8` + `uint8` | 2 | 0,2 Hz |
| `StormIntensity01`, categoría de ciclón | `uint8` | 1 | al cambiar |
| **Total** | | **11 B** | |

11 B × 0,2 Hz = 2,2 B/s = **0,018 kbps**. El cliente **hace avanzar su reloj local** con
su propio `Tick` entre paquetes y lo ajusta suavemente hacia el del servidor (nunca de
un salto: un salto de reloj mueve el sol y la marea a tirones). Tolerancia de ajuste:
si el error pasa de **6 minutos de juego**, salto duro; por debajo, se corrige con
`TimeScale` local entre 0,95 y 1,05 hasta cerrar el error.

Cielo, sol, luna, mareas, olas, viento y lluvia se derivan localmente de esos 11 bytes.
El rayo de un ciclón y su trueno se mandan como multicast cosmético (4 B) para que los
cuatro jugadores vean el mismo relámpago.

### 2.9 Necesidades de cada jugador

**Cada cuerpo es suyo y lo simula el servidor.** `FSurvivalState` tiene ~20 `float` +
`ConditionTime[19]` + heridas: nunca va entero por el cable.

- **A su dueño** (`COND_OwnerOnly`), a **1 Hz**: 8 necesidades a `uint8` (salud,
  hambre, sed, energía, descanso, ánimo, temperatura ×2 para el rango 30–43 °C, humedad)
  + máscara de estados activos (`uint32`, 19 bits usados) + heridas (hasta 4, 3 B cada
  una: profundidad, sangrado, banderas) = **8 + 4 + 12 = 24 B**. 24 B/s =
  **0,19 kbps**. A 1 Hz basta porque el HUD solo dibuja barras por debajo del 55 %
  (`ExploredHUD.cpp:248`) y el cuerpo cambia despacio por diseño.
- **A los demás**, a 0,5 Hz, solo lo que se ve desde fuera: **2 B** (salud a `uint8`
  para el color de la etiqueta de nombre y el estado `Derribado`, + banderas
  visibles: sangrando, cojeando, tiritando, ardiendo). 3 jugadores × 2 B × 0,5 Hz =
  3 B/s = **0,024 kbps**.
- **Eventos** (`ESurvivalEvent`: intoxicación, fractura, muerte, ahogo) como RPC fiable
  al dueño, con el aviso interior ES/EN de 01 §6 que ya está especificado. Los demás
  reciben solo el sonido si están a menos de 15 m.
- **Sin escalado por número de jugadores:** las necesidades bajan igual con 1 jugador
  que con 4 (§5.6). Cuatro bocas gastan cuatro veces la comida, y eso ya es la
  dificultad añadida del cooperativo; multiplicar además la velocidad de hambre sería
  castigar dos veces.

### 2.10 Construcción

`AExploredBuildingPiece` ya es un actor, y `FBuildingModel` vive en
`UBuildingSubsystem`. Conversión directa:

- Colocar: `Server_PlacePiece(DefinitionId, Transform)`. El servidor valida encaje,
  rejilla de 2 m, materiales en **su** copia del inventario y
  `FBuildingModel::RecomputeStability`, y **luego** genera el actor replicado.
- El fantasma de vista previa (`UBuildPreviewComponent`) es **puramente local**: verde
  o rojo se calcula en el cliente con su copia del modelo. Si el servidor rechaza, la
  pieza no aparece; el cliente no ha mentido, solo ha propuesto.
- `CollapseUnsupported` y la degradación por clima corren **solo en el servidor**; los
  escombros son objetos soltados normales (§2.5, con su regla de dormir).
- **Dos jugadores colocando en el mismo hueco:** el servidor procesa en orden de
  llegada; el segundo recibe fallo de solape. Sin reserva previa de hueco.
- Coste: un actor de pieza son ≈ 40 B al aparecer y **0 B** después (estático). Una base
  de 200 piezas son 8 KB al unirse, en el flujo normal de relevancia de actores.

### 2.11 Cartografía, museo, ruinas y progreso

- **Cartografía:** `UCartographyComponent` está hoy **en el personaje**, y el mapa pasa
  a ser **compartido** (§5.3). Migra a `UExploredCartographySubsystem` (mundo) con el
  `FCartographyModel` autoritativo en el servidor. Los trazos nuevos se replican como
  polilíneas cuantizadas: `uint8` autor + `uint8` número de puntos + puntos a 2 B (X, Y
  en celdas de 8 m sobre el archipiélago de 6,4 km → 800 celdas por eje, cabe en
  `uint16` con holgura). Caminar por la costa genera ≈ 1 punto cada 8 m: **0,3 kbps**
  mientras se explora, 0 en reposo.
- **Museo, ruinas, artefactos, progreso de exploración** (`FMuseumModel`,
  `FRuinsModel`, `FExploredProgress`): compartidos, replicados como estado del
  `GameState` con `bNetLoadOnClient`, unas decenas de bytes al cambiar. Una técnica de
  wayfinding aprendida la aprende **el grupo** (§5.5).
- **Diario del jugador** (`journal_entries.json`): **individual**, cliente, sin red. Es
  la voz del personaje, y cada jugador tiene el suyo.

---

## 3. Presupuesto de ancho de banda

**Objetivo (duro, es criterio de salida de H5):** menos de **64 kbps por cliente en
reposo** y menos de **256 kbps en pico**. Medido en el sentido servidor → cliente, que
es el caro; cliente → servidor no pasa de 20 kbps (movimiento + controles + RPC de
acción).

**En reposo** — 4 jugadores juntos en la base, nadie cavando ni talando, fauna de
ambiente alrededor:

| Canal | Cálculo | kbps |
|---|---|---|
| 3 personajes ajenos | 20 Hz × 24 B × 3 | 11,5 |
| Fauna terrestre (12 a 10 Hz + 24 a 2 Hz) | 2 352 B/s | 18,8 |
| Anclas de grupos de fauna de ambiente | 24 × 10 B × 0,5 Hz | 1,0 |
| Cuerpo propio | 24 B × 1 Hz | 0,2 |
| Cuerpo de los otros | 3 × 2 B × 0,5 Hz | 0,02 |
| Reloj y clima | 11 B × 0,2 Hz | 0,02 |
| Verificación de chunks de terreno | ≈ 5 B/s | 0,04 |
| Mantenimiento de canales de actor y acuses | medido, reserva | 1,5 |
| **Total en reposo** | | **≈ 33 kbps** |

Queda el **48 % de margen** sobre los 64 kbps. El margen es deliberado: la fauna
terrestre es lo único que puede crecer sin avisar, y su tope duro de §2.7 es lo que
protege el presupuesto.

**En pico** — el peor caso realista y el que se prueba en la matriz de §7: un jugador
abriendo camino con la pala, otro talando un gigante, dos navegando en ciclón, una
avalancha de arena en marcha y un cofre abierto:

| Canal | kbps |
|---|---|
| Base de reposo | 33 |
| Deltas de pala (comprimidos) | 2,8 |
| Deltas de pico de un segundo jugador | 0,5 |
| Arena viva | 0,6 |
| Tala (2 árboles a la vez) | 0,3 |
| 2 barcos ocupados | 6,0 |
| Objetos sueltos despiertos (hasta 32 a 10 Hz × 12 B) | 30,7 |
| Inventario propio fabricando + cofre abierto | 4,0 |
| Cartografía mientras se explora | 0,3 |
| Multicast cosméticos (rayos, impactos, sonidos) | 2,0 |
| **Total en pico** | **≈ 80 kbps** |

**Sobra muchísimo.** Y eso deja sitio para el gasto que no se puede planificar: la
**entrada de un chunk de terreno nunca visto** (3 KB comprimido = 24 kbps durante un
segundo) y la **ráfaga de unirse en caliente**, que tiene su propio presupuesto en §4.3.
Incluso sumando la ráfaga completa de terreno (128 kbps durante 5 s) el pico queda en
**≈ 208 kbps**, por debajo de los 256.

**Los tres topes que sostienen todo el presupuesto** (si uno se relaja, el objetivo se
cae): 12+24 animales terrestres replicados, 32 objetos sueltos despiertos, 8 KB/s de
cola de terreno con 16 KB/s de ráfaga.

**Cómo se mide** (no se da por bueno un cálculo en una tabla): `stat net`,
`net.PackageMap.DebugAll 0`, y sobre todo el comando nuevo `Explored.NetBudget` (H0),
que escribe un CSV con kbps por canal y por cliente cada segundo. El criterio de salida
de H5 se cumple con ese CSV sobre las 12 filas de la matriz de §7, no con una
estimación.

---

## 4. Sesiones

### 4.1 Crear partida

Una partida cooperativa es una partida normal con una casilla. En la pantalla de ranura
de guardado (`SExploredSaveSlots`, ya existe) y en «Partida nueva»
(`SExploredModeSelect`, ya existe) se añade:

- **Privacidad:** «Solo por invitación» (por defecto) / «Amigos» / «En solitario».
- **Aforo:** 2 / 3 / 4 (por defecto 4).
- El modo de supervivencia (Explorador / Superviviente / Náufrago / Personalizado) lo
  fija el anfitrión y **manda para todos**: no hay modos mezclados en una misma partida.

Internamente: `UExploredSessionSubsystem` (nuevo, `GameInstance`) crea la sesión con
`IOnlineSessionPtr`, ajustes `NumPublicConnections = Aforo − 1`,
`bUsesPresence = true`, `bUseLobbiesIfAvailable = true`, y la clave de sesión
`EXPLORED_WORLD = <WorldId>`. `WorldId` es un **GUID que se genera al crear el mundo** y
se guarda en la partida; es lo que ata un perfil de invitado (§4.4) a un mundo concreto.

### 4.2 Invitar por Steam

- Superposición de Steam (`Shift+Tab` → Amigos → Invitar a jugar), y **botón propio**
  «Invitar por Steam» en la lista de jugadores (§6.3) que abre
  `ShowInviteUI`/`TriggerOnSessionUserInviteAcceptedDelegates`.
- Aceptar una invitación **con una partida en curso**: aviso de que se cerrará la
  partida actual, con autoguardado antes (texto en §6.5).
- Sin código de partida escrito a mano ni lista de servidores públicos: es cooperativo
  con amigos, no un juego de sesiones abiertas. Decisión firme, no pendiente.

### 4.3 Unirse en caliente

Se puede entrar **en cualquier momento**, sin reiniciar el mundo. Es un requisito de
diseño, no un extra: la partida de cuatro amigos con horarios distintos es el caso
normal.

Secuencia y presupuesto (objetivo: **por debajo de 20 segundos** en una línea de
10 Mbps):

| Paso | Qué viaja | Tamaño | Tiempo |
|---|---|---|---|
| 1. Saludo | `FExploredContentHash` + versión de build + `WorldId` | 24 B | inmediato |
| 2. Semilla y reloj | Semilla del mundo, día, hora, estación, clima | 32 B | inmediato |
| 3. Personaje | Su perfil de invitado (§4.4) o el kit de inicio | 0,5–2 KB | < 1 s |
| 4. Mundo cercano | Chunks de terreno editados a < 120 m + celdas de vegetación a < 200 m + piezas de construcción a < 200 m | 20–120 KB comprimido | 2–10 s |
| 5. Resto | Museo, ruinas, progreso, hoja de mapa compartida | 5–40 KB | 1–4 s |
| 6. Aparición | Junto al anfitrión si está a menos de 100 m de un fuego encendido; si no, en el fuego encendido más cercano al anfitrión; sin ninguno, en el `PlayerStart` | — | — |

Durante los pasos 4–5 el que entra ve la pantalla de carga con la ilustración del
cuaderno (misma piel que el resto, 06 §1) y **los que ya estaban siguen jugando sin
tirón**: el envío va por la cola de §2.2 con prioridad baja y el tope de ráfaga de
16 KB/s, así que el anfitrión no se congela. Esto es más importante que entrar rápido.

**Si el mundo no coincide** (hash de contenido o versión de build distintos): se rechaza
con el texto de §6.5. Nunca se intenta jugar con datos distintos.

### 4.4 Guardado: del anfitrión, y qué se lleva cada invitado

**El mundo es del anfitrión.** `UExploredSaveSubsystem` corre **solo** en el servidor:
las once secciones que hoy registra `UExploredWiringSubsystem` más las de construcción,
huerto, ruinas, eventos y logros se guardan en la ranura del anfitrión, tal cual, sin
formato nuevo. Un cliente que llame a `RequestSave` no hace nada (y se registra en el
log del servidor si insiste, por si hay un bug).

Cambios en el formato de guardado:

- La sección `"inventory"` y la sección `"body"` pasan de **una** a **una por
  `SteamID64`**: `"players": { "<SteamID64>": { "inventory": …, "body": …, "journal": … } }`.
  Compatible hacia atrás: una partida antigua de un jugador se lee como el jugador del
  anfitrión.
- Sección nueva `"coop"`: `WorldId`, aforo, último visto de cada invitado, color de
  tinta asignado (§5.3), revividas del día (§5.2).

**Lo que conserva cada invitado, entre sesiones y entre anfitriones:** un **perfil de
invitado** local, en `Saved/SaveGames/Coop/<SteamID64>/<WorldId>.sav`, con **su
personaje y su inventario** (cuerpo completo, heridas, estados, equipo, manos, mochila,
diario, estadísticas de logro). **Nada del mundo.** Se escribe en el cliente cada vez
que el anfitrión autoguarda y al salir limpiamente.

- Vuelve al **mismo** mundo (`WorldId` coincide) → recupera cuerpo e inventario tal como
  los dejó. El anfitrión también tiene su copia en la sección `"players"`; si las dos
  existen, **manda la del anfitrión** (es la autoritativa; la del invitado es un
  respaldo para cuando el anfitrión pierde su partida).
- Entra en un mundo **distinto** → empieza con el kit de inicio estándar (01 §4). No hay
  personaje que viaje entre mundos con el botín de otra partida: rompería la progresión
  por islas del GDD §4.
- El invitado **no** se lleva nada del mundo: ni terreno, ni construcción, ni mapa, ni
  museo. Eso es del anfitrión, y así el mundo tiene un único dueño y un único
  respaldo.

Autoguardado: los mismos disparadores que ya existen (`ESaveTrigger`), más uno nuevo
**al unirse o irse un jugador** (para que un corte no pierda la sesión entera) y uno
**cada 5 minutos reales** mientras haya más de un jugador conectado.

### 4.5 Si el anfitrión se va

Sin migración de anfitrión (§1.1). Lo que sí hay, y está definido:

- **Cierre ordenado** (el anfitrión sale por el menú): aviso de **10 segundos** con
  cuenta atrás visible a todos, autoguardado del mundo, escritura del perfil de invitado
  en cada cliente, y todos al menú principal con el texto de §6.5.
- **Caída** (se le cae la conexión o el juego): los clientes detectan la pérdida a los
  **15 segundos** de silencio, intentan reconectar **3 veces en 30 segundos**
  (el relay de Steam recupera muchos cortes cortos), y si no, al menú. El perfil de
  invitado que se escribió en el último autoguardado del anfitrión es lo que se
  conserva: **hasta 5 minutos de pérdida en el peor caso**, y se dice así, sin
  maquillarlo.
- **Un invitado se va:** su personaje desaparece del mundo a los **30 segundos** (tiempo
  de gracia para un corte breve, durante el cual su cuerpo se queda dormido y a salvo,
  invulnerable). Lo que llevaba encima se va con él. Lo que había dejado en cofres,
  construido o cavado **se queda**: es del mundo, no suyo.
- **El anfitrión puede expulsar** desde la lista de jugadores (§6.3), con confirmación.
  Un expulsado conserva su perfil de invitado, y volver a entrar exige una invitación
  nueva.

---

## 5. Reglas de diseño del cooperativo

### 5.1 Dormir: sí se salta la noche, pero solo entre todos

**Decisión: la noche se salta únicamente si todos los jugadores conectados están
acostados.** La alternativa (que duerma uno y avance el reloj) le roba la noche a los
demás, y la noche es contenido: pesca nocturna, navegación por estrellas, luciérnagas.

| Situación | Qué pasa |
|---|---|
| 1 de 4 acostado | El reloj sigue a ×1. El que duerme recupera `Rest` igual (la cama funciona, solo no acelera el mundo) |
| 3 de 4 acostados | Igual: ×1. El HUD del que falta muestra el aviso de §6.6 |
| 4 de 4 acostados | `TimeScale` pasa a **×120** (8 horas de juego en 6,7 segundos reales) hasta el amanecer o hasta las 8 h de sueño |
| Alguien se levanta a mitad | `TimeScale` vuelve a **×1** de inmediato, y **se conservan las horas ya ganadas** con recuperación proporcional (mismo criterio que la cancelación de dormir en solitario, 06 §2.12) |
| Alguien está `Derribado` (§5.2) | No cuenta como acostado: **no se puede dormir mientras hay alguien en el suelo** |

`TimeScale` va en el `GameState` (§2.8) y lo fija **solo el servidor**. El fundido
(`SExploredFade`) es local en cada cliente y todos ven la misma etiqueta de hora.

### 5.2 Muerte y reanimación

En cooperativo, llegar a `Health = 0` **no mata de inmediato** en Explorador y
Superviviente: abre un estado `Derribado`.

| Regla | Valor |
|---|---|
| Duración de `Derribado` | **90 segundos** de juego |
| Movimiento | Gateo a **0,6 m/s**, sin manos usables, sin interactuar con nada |
| Visión | Bordes cerrados, sonido apagado (el mismo lenguaje del mareo por aire viciado, 02 §2.4) |
| Reanimar | Interacción sostenida de **6 s** de otro jugador; **3 s** si lleva `botiquin`, `vendaje_tela` o `gel_aloe` en la mano (que se consume) |
| Al levantarse | **25 % de salud**, ánimo **−6** (`moraleEvents.Injured`, ya existe), heridas abiertas sin curar |
| Si expiran los 90 s | Muerte normal de 01 §7: reaparición en el fuego encendido más cercano, mitad de salud, ánimo **−15**, **sin perder objetos** |
| Tope antiabuso | **2 reanimaciones por jugador y día de juego**; de la tercera en adelante, `Derribado` dura solo **30 s** |
| Modo Náufrago (permadeath) | **No hay `Derribado`.** Morir es morir; el jugador pasa a espectador (cámara libre atada a los vivos) hasta que el anfitrión recargue. Coherente con que el permadeath sea permadeath |
| Todos derribados a la vez | Muere el primero que agote sus 90 s y los demás mueren con él (evita el empate absurdo de cuatro personas gateando) |

### 5.3 El mapa dibujado se comparte

**Decisión: una sola hoja por mundo, compartida.** Cuatro mapas medio dibujados
convierten el pilar «cartografiar a mano» (GDD §2.1.3) en cuatro tareas burocráticas
paralelas. Compartirlo lo convierte en el cuaderno de la expedición, que es mejor.

- El trazado de costa, los bocetos de mirador, las marcas, los sellos y las hojas
  subterráneas de mina (GDD §3.2) son del mundo.
- **Cada jugador dibuja con su propio tono de tinta**, sacado de la paleta que ya existe
  (06 §1.2): anfitrión `ink.strong` (`#2B2016`), y los otros tres derivados
  (`#3A2F1E` sepia, `#243447` azul de tinta, `#3E2A2A` granate). Así la hoja se lee como
  un registro de grupo sin necesidad de leyenda ni de nombres escritos.
- Los **instrumentos siguen siendo individuales**: reloj de muñeca, brújula, catalejo,
  sextante son objetos de inventario, uno por jugador. Compartir la hoja no regala los
  instrumentos.
- El mapa se abre a la vez en varias manos sin problema: es un modelo autoritativo del
  servidor y cada cliente dibuja su propia vista (`SExploredMapInHands`, ya existe).

### 5.4 Llevar a otro jugador en el barco

- Aforo por plano canónico: **balsa 2, canoa 2, canoa con balancín 3, «Limón» 4**
  (§2.5). Al lleno, el verbo «Subir» desaparece del prompt de contexto (no aparece un
  mensaje de error: si no cabe, no se ofrece).
- **Un solo timonel:** quien tiene el timón recibe el contexto de Enhanced Input del
  barco (el que ya existe, `ExploredBoat.h`). El resto conserva **sus** controles: puede
  pescar, leer el mapa, achicar agua (**B**, ya existe) o remar en su banco
  (`TryStroke` por banda, ya existe). Remar entre dos por bandas opuestas es
  cooperativo de verdad y no hace falta código nuevo para ello.
- **El timón se cede** con el mismo verbo de interacción sobre el asiento del timonel;
  el timonel actual puede soltarlo, y si se desconecta, el timón queda libre.
- **Peso real:** 75 kg por tripulante en `TotalMassKg`. Cuatro jugadores más carga en
  una canoa con balancín pasan del 95 % de flotabilidad y **embarcan agua** (02 §8.2).
  No se ajusta a la baja para que quepan: que la balsa sobrecargada se hunda es la
  lección.
- Bajarse sobre agua profunda deja al jugador nadando (`USwimComponent`); sobre tierra,
  junto a la borda. Ya está resuelto por `AExploredBoat::Leave`.

### 5.5 Progresión: el mundo es del grupo, el cuerpo es tuyo

| Compartido | Individual |
|---|---|
| Terreno editado, galerías, caminos, escaleras | Cuerpo: necesidades, heridas, estados, temperatura |
| Construcción, huerto, corrales [F2] | Inventario, manos, equipo, mochila |
| Vegetación talada y rebrote | Diario del jugador |
| Mapa dibujado y hojas subterráneas (§5.3) | Instrumentos (reloj, brújula, catalejo, sextante) |
| Museo y colecciones | Logros de Steam (§5.7) |
| Técnicas de wayfinding aprendidas en ruinas | Tinta del mapa (§5.3) |
| Barcos, contenedores, fuegos | — |
| Reloj, estación, clima, mareas | — |
| Amenaza pirata y reputación [F3] | — |

Dos decisiones que se justifican porque son las discutibles:

- **Las técnicas de wayfinding se aprenden en grupo.** Cualquier jugador que esté a
  menos de 15 m de la ruina cuando se lee la técnica la aprende. Obligar a cuatro
  jugadores a visitar la misma ruina cuatro veces es relleno, y el objetivo final
  («3 caminos de estrellas», GDD §4) es del barco, no de una persona.
- **El museo es uno.** La base es la base del grupo, y las vitrinas están en ella. Un
  tesoro es único en el mundo: lo coge quien llega primero y lo expone donde quiera.

### 5.6 Escalado por número de jugadores

Regla general: **se escala lo que el grupo agota, no lo que le hace daño.** Cuatro
jugadores talan y minan cuatro veces más rápido; no pegan cuatro veces más fuerte.

| Qué | Escalado con N jugadores | Con N=2 / 3 / 4 |
|---|---|---|
| Necesidades del cuerpo | **Sin escalado** (§2.9) | ×1 / ×1 / ×1 |
| Salud y daño de fauna y piratas | **Sin escalado** | ×1 / ×1 / ×1 |
| Vetas finitas por celda (cobre, hierro de meteorito) | `×(1 + 0,25·(N−1))`, redondeo abajo | ×1,25 / ×1,5 / ×1,75 |
| Densidad de fauna cazable por celda | `×(1 + 0,25·(N−1))` | igual |
| Aparición de pescado y cangrejos | `×(1 + 0,25·(N−1))` | igual |
| Tesoros, artefactos, ruinas, técnicas | **Sin escalado**: son únicos en el mundo | ×1 |
| Rebrote de tala, crecimiento de cultivos, mareas | **Sin escalado**: van con el calendario | ×1 |
| Asaltos piratas [F3]: número de asaltantes | `×(1 + 0,4·(N−1))` sobre la base de 5 | 7 / 9 / 11 |
| Asaltos piratas [F3]: categoría | **+1** por cada 2 jugadores por encima de 1 | +0 / +1 / +1 |
| Frecuencia de asalto [F3] | **Sin escalado**: la fija el contador de Amenaza | ×1 |
| Tiburón: `ReefSharkAttackRoll` | Una tirada **por nadador**, no una compartida | — |

Ninguna de estas cifras se da por buena sin playtesting: son el punto de partida
declarado, y su ajuste es tarea explícita de H5 (§9).

### 5.7 Logros en cooperativo

**Los logros no se desactivan en cooperativo.** Desactivarlos castiga el modo que el
director acaba de decidir que es parte del juego.

`achievements.json` gana un campo `coopScope` con tres valores:

| `coopScope` | Quién lo desbloquea | Ejemplos |
|---|---|---|
| `"actor"` | Solo quien hace la acción | `primera_palada`, `manazas`, `primera_canoa`, `banquete_de_mil_cocos` |
| `"world"` | Todos los conectados en ese momento | `asalto_repelido`, `museo_completo`, `muralla_de_piedra`, los de hito del mundo |
| `"witness"` | Quien esté a menos de **50 m** del hecho | `bajo_el_templo`, hallazgo de tesoro, `el_aire_que_falta` |

- Logros **de restricción** (`sin_disparar_una_flecha` y similares): solo cuentan si
  **ningún** jugador de la sesión rompió la restricción. El servidor lleva la bandera en
  el `GameState`; si alguien dispara, se apaga para todos en esa partida.
- Logros de **Náufrago** (permadeath): cuentan igual en cooperativo, con la regla de
  §5.2 (sin `Derribado`).
- El servidor decide **el hecho** y manda un RPC al cliente correspondiente; el
  desbloqueo en Steam lo hace **cada cliente en su propia cuenta**
  (`UAchievementsSubsystem`, ya existe). Las estadísticas acumuladas
  (`terrain_edits_made`, `coconuts_opened`…) son **individuales**: cuentan lo que ha
  hecho ese jugador, en cualquier partida, cooperativa o no.

---

## 6. Interfaces nuevas

Todas siguen los principios de 06 §0 sin excepción: papel y tinta, nada en pantalla que
no haga falta, máximo tres opciones simultáneas, sin números de supervivencia. Textos
ES/EN pasados por el checklist anti-IA de 07 §1.5: frases cortas, sin exclamaciones
fuera de peligro real, inglés británico escrito desde cero, no calcado.

### 6.1 `SExploredCoopLobby` — sala de espera del anfitrión [H0]

Una hoja de cuaderno con el nombre del mundo escrito en Caveat, cuatro renglones (uno
por sitio) y dos botones. No hay lista de servidores, no hay chat, no hay barra de carga
de nadie.

| Elemento | Español | Inglés |
|---|---|---|
| Título | «Partida cooperativa» | "Co-op game" |
| Renglón vacío | «Sitio libre» | "Free seat" |
| Renglón esperando | «Entrando…» | "Coming in…" |
| Botón principal | «Invitar por Steam» | "Invite through Steam" |
| Botón secundario | «Empezar» | "Start" |
| Privacidad | «Solo por invitación» / «Amigos» / «En solitario» | "Invite only" / "Friends" / "On my own" |
| Pie | «Puedes empezar sin esperar: se puede entrar en cualquier momento.» | "You can start without waiting; people can join any time." |

### 6.2 Invitación y entrada [H0]

| Situación | Español | Inglés |
|---|---|---|
| Invitación recibida (superposición de Steam) | «{Name} te invita a su archipiélago.» | "{Name} is inviting you to their archipelago." |
| Aceptar con partida en curso | «Se guardará tu partida y se cerrará. ¿Seguimos?» | "Your game will be saved and closed. Carry on?" |
| Entrando | «Cargando el archipiélago de {Name}.» | "Loading {Name}'s archipelago." |
| Entrada completada (aviso del HUD, 3 s) | «Ya estás dentro.» | "You're in." |
| Aviso a los demás | «{Name} se ha unido.» | "{Name} joined." |
| Salida de alguien | «{Name} se ha ido.» | "{Name} left." |

### 6.3 `SExploredPlayerList` — lista de jugadores [H0]

Se abre desde el menú de pausa (`SExploredPauseMenu`, ya existe) como una pestaña más,
no como pantalla propia. Una línea por jugador: tono de tinta (§5.3), nombre, retardo en
milisegundos (solo si «Mostrar retardo» está activo, §6.7) y, para el anfitrión,
«Expulsar».

| Elemento | Español | Inglés |
|---|---|---|
| Pestaña | «Jugadores» | "Players" |
| Etiqueta de anfitrión | «anfitrión» | "host" |
| Acción del anfitrión | «Expulsar» | "Remove" |
| Confirmación | «¿Sacar a {Name} de la partida?» | "Remove {Name} from the game?" |
| Acción común | «Invitar por Steam» | "Invite through Steam" |

### 6.4 Nombres sobre la cabeza y ping de posición [H0]

**Nombres.** Etiqueta en Nunito Sans, tinta del jugador (§5.3), **sin fondo ni marco**.
Reglas de aparición, coherentes con el umbral del 55 % de 06 §0.2:

| Regla | Valor |
|---|---|
| Distancia de aparición | Hasta **60 m**; se desvanece linealmente entre 45 y 60 m |
| Oclusión | **No se ve a través del terreno ni de la construcción**. Si hay algo en medio, no hay etiqueta |
| Tamaño | Fijo en pantalla (no crece al acercarse), **sin** barra de vida ni icono |
| Estado visible | Se añade **una** palabra si hace falta: «en el suelo» (`Derribado`) o «durmiendo» |
| Ajuste | Siempre / Cerca (por defecto) / Nunca (§6.7) |

**Ping de posición.** No hay chat de texto obligatorio (ni lo habrá en AA: Steam ya da
voz y texto en su superposición, y el juego no tiene que duplicarlo). Lo que sí hay es
un ping diegético: se **señala con la mano**.

- Rueda de **tres** opciones (regla de los tres verbos, 06 §0.3) con `Q` mantenido, o
  `Q` corto para el primero.
- Marca en el mundo durante **8 segundos**, con el tono de tinta de quien la puso, y un
  sonido corto distinto por tipo. Máximo **1 ping cada 3 segundos por jugador**.
- La marca se dibuja **también sobre la hoja del mapa compartido** mientras dure, lo que
  la convierte en la forma natural de decir «vamos aquí».
- Coste de red: 8 B por ping. Nada.

| Opción | Español | Inglés |
|---|---|---|
| Primera (`Q` corto) | «Aquí» | "Over here" |
| Segunda | «Cuidado» | "Careful" |
| Tercera | «Mira esto» | "Look at this" |

### 6.5 Avisos de sesión [H0]

Cola de notificaciones del HUD que ya existe (`AExploredHUD::DrawNotifications`), sin
pantallas modales salvo las dos últimas.

| Situación | Español | Inglés |
|---|---|---|
| El anfitrión va a cerrar | «{Name} cierra la partida en {Count} s.» | "{Name} is closing the game in {Count}s." |
| Conexión perdida | «Se ha cortado la conexión.» | "The connection dropped." |
| Reintentando | «Intentando volver a entrar.» | "Trying to get back in." |
| No se ha podido (modal) | «No se ha podido volver a entrar. Tu personaje está guardado.» | "Couldn't get back in. Your character is saved." |
| Datos distintos (modal) | «Tu versión del juego no es la misma que la de {Name}.» | "Your version of the game isn't the same as {Name}'s." |
| Partida llena | «No queda sitio.» | "No room left." |
| Expulsado | «{Name} te ha sacado de la partida.» | "{Name} removed you from the game." |

### 6.6 Aviso de dormir en grupo [H0]

Al acostarse sin que estén todos, en lugar del fundido: una línea sobre la cama, en la
misma tinta del resto del HUD, que se va sola.

| Situación | Español | Inglés |
|---|---|---|
| Faltan otros | «Hay {Count} en pie todavía.» | "{Count} still up." |
| Falta uno | «Queda uno en pie.» | "One still up." |
| Alguien está derribado | «No se duerme con alguien en el suelo.» | "Nobody sleeps with someone down." |
| Todos listos | (fundido normal, sin texto) | — |

### 6.7 Ajustes de red [H0]

Pestaña nueva en `SExploredSettingsPanel` (ya existe), con la misma piel. Cinco ajustes,
ni uno más: el resto se decide solo.

| Ajuste | Español | Inglés | Valores |
|---|---|---|---|
| Nombre visible | «Nombre que ven los demás» | "Name others see" | Texto, 16 caracteres; por defecto el de Steam |
| Etiquetas | «Nombres sobre la cabeza» | "Names above heads" | Siempre / Cerca / Nunca — "Always / Nearby / Never" |
| Tope de subida (solo anfitrión) | «Tope de subida» | "Upload cap" | 0,5 / 1 / 2 MB/s (por defecto 1) |
| Retardo | «Mostrar retardo» | "Show latency" | Sí / No — "Yes / No" |
| Suavizado | «Suavizar el movimiento ajeno» | "Smooth other players" | Sí / No — "Yes / No" |

El tope de subida del anfitrión reparte entre clientes; por debajo de 0,5 MB/s con 3
invitados el juego avisa una vez y baja la cola de terreno a 4 KB/s por cliente (cavar
se ve llegar más despacio, nada más se degrada).

### 6.8 Reanimar [H0]

El estado `Derribado` (§5.2) no trae HUD nuevo: se lee en el cuerpo, como todo lo demás.
Lo único que se añade es el prompt de contexto sobre el compañero caído, con el mismo
formato que cualquier otro interactuable.

| Situación | Español | Inglés |
|---|---|---|
| Verbo de contexto | «Levantar a {Name}» | "Help {Name} up" |
| Con botiquín en la mano | «Atender a {Name}» | "Patch {Name} up" |
| Mientras se reanima | (barra de progreso del prompt, ya existe) | — |
| Aviso a los demás | «{Name} está en el suelo.» | "{Name} is down." |
| Al levantarse | «{Name} vuelve a estar en pie.» | "{Name} is back up." |

---

## 7. Pruebas

### 7.1 Base: PIE con clientes

Perfil de PIE obligatorio para todo trabajo de red: **`Play As Listen Server`, 2
clientes**, ventanas de 1280×720, `Run Under One Process` desactivado (para que cada
cliente tenga su propio estado y no comparta estáticos por accidente, que es la trampa
clásica). Perfil de 4 clientes para las filas que lo piden.

Script nuevo `Tools/net-test.ps1` (H0), que arranca PIE con el perfil pedido, aplica
condiciones de red y vuelca el CSV de `Explored.NetBudget`.

### 7.2 Condiciones de red que se prueban

Con los emuladores propios del motor, siempre desde la consola del **servidor**:

```
net pktlag=80        ; latencia base objetivo (ida y vuelta 160 ms)
net pktlagvariance=30
net pktloss=2        ; pérdida normal de una conexión doméstica
net pktloss=10       ; conexión mala
net pktdup=1
net pktorder=1       ; reordenación: descubre suposiciones de orden en RPC no fiables
```

Combinaciones nombradas, que son las que cita la matriz:

| Nombre | Ajustes |
|---|---|
| **Limpia** | Sin emulación |
| **Normal** | `pktlag=80 pktlagvariance=30 pktloss=2` |
| **Mala** | `pktlag=200 pktlagvariance=80 pktloss=10 pktorder=1` |
| **Horrible** | `pktlag=400 pktlagvariance=150 pktloss=20 pktdup=1` |

«Horrible» no tiene que jugarse bien; tiene que **no corromper el mundo ni desconectar
sin avisar**. Ese es el criterio.

### 7.3 Matriz de pruebas

Cada fila se pasa en las condiciones indicadas y se verifica en el CSV de
`Explored.NetBudget` que el ancho de banda cumple §3. Fila que falla en «Normal»
bloquea el hito.

| # | Prueba | Clientes | Condiciones | Qué se verifica |
|---|---|---|---|---|
| 1 | Cuatro personajes corriendo en círculo alrededor del fuego | 4 | Normal, Mala | Sin teletransportes; corrección invisible; < 64 kbps |
| 2 | Interacción autoritativa: dos jugadores recogen **el mismo** coco en el mismo fotograma | 2 | Normal, Mala | Uno se lo lleva, el otro no; nunca dos cocos |
| 3 | Un cliente cava una galería de 1×2×5 m en basalto (180 golpes) | 2 | Normal | El hueco es idéntico en las dos máquinas (comprobación de §2.2); < 256 kbps |
| 4 | El anfitrión abre un camino de 40 m con la pala mientras un cliente cava al lado | 2 | Normal, Mala | Cola de terreno respeta 8 KB/s; sin tirón en el anfitrión |
| 5 | Recargar la partida del anfitrión y volver a entrar el cliente | 2 | Limpia | El terreno editado sigue cavado en las dos máquinas |
| 6 | Talar un gigante entre dos, con golpes alternos de hacha y a mano | 2 | Normal | Los golpes suman fracciones (GDD §3.12); el árbol cae en la misma dirección en las dos pantallas |
| 7 | Dos jugadores en el mismo cofre, sacando el mismo objeto | 2 | Mala | Nunca se duplica; el que pierde ve el hueco vacío |
| 8 | Cuatro en la canoa con balancín, mar de ciclón, uno achicando | 4 | Normal, Mala | Anegamiento y escora iguales en todos; < 256 kbps |
| 9 | Unirse en caliente a una partida con 3 h de minería y una base de 200 piezas | 2→3 | Normal | < 20 s; los que estaban no pierden fotogramas |
| 10 | El anfitrión mata el proceso a lo bruto | 3 | Limpia | Los clientes vuelven al menú con aviso; el perfil de invitado está en disco |
| 11 | Dormir: 3 de 4 acostados, luego los 4, luego uno se levanta | 4 | Normal | El reloj hace exactamente lo de §5.1 |
| 12 | Derribado: uno cae, otro reanima; luego expiran los 90 s | 2 | Normal, Mala | 25 % de salud al levantarse; muerte normal al expirar |
| 13 | Avalancha de arena provocada a propósito junto a un jugador que cava | 2 | Normal | La arena se asienta igual en ambos; los deltas del jugador no se retrasan más de 1 s |
| 14 | Cliente con datos de `Content/Data` modificados | 2 | Limpia | Rechazo con el texto de §6.5, sin entrar |
| 15 | Sesión de 2 horas con 4 jugadores activos | 4 | Normal | Sin deriva de terreno (comprobación en verde todo el rato); memoria estable |
| 16 | Todo lo anterior en «Horrible» | 2–4 | Horrible | Nada se corrompe; se puede salir al menú siempre |

### 7.4 Specs de host

El modelo puro se prueba **sin Unreal**, en `Tools/HostTests` (regla del `CLAUDE.md`
del proyecto), y lo de red que se puede modelar puro es bastante:

- `FTerrainDeltaPacketSpec`: codificar y decodificar un lote de muestras da el estado
  original; la fusión de dos paquetes del mismo chunk es idempotente y conmutativa; un
  paquete truncado o manipulado se rechaza sin tocar el estado.
- `FVegetationNetStateSpec`: la clave de 7 bytes va y vuelve sin pérdida; el tope de
  4096 compacta en el orden esperado.
- `FCoopRulesSpec`: reglas de §5.1 (dormir), §5.2 (derribado, tope de 2 reanimaciones) y
  §5.6 (escalado) como funciones puras en `ExploredLinks`, probadas con tablas.
- `FContentHashSpec`: el hash de `Content/Data` es estable entre ejecuciones y cambia al
  cambiar cualquier fichero.

Lo que **no** se puede probar en host (replicación real, relevancia, Steam) va en la
matriz de §7.3 y en `Tools/test.ps1`.

---

## 8. Riesgos

| Riesgo | Tipo | Gravedad | Mitigación |
|---|---|---|---|
| Convertir 21 puntos de «jugador único» rompe sistemas hoy no compilados (17 según `docs/roadmap.md`) | Técnico | **Alta** | §2.1 primero y solo, en H0, con la compilación en local en verde antes de tocar nada más. No se replica ni un sistema hasta que el reparto `GameState`/`PlayerState`/componente por jugador esté cerrado |
| El terreno replicado desincroniza y el mundo deja de ser el mismo en dos máquinas | Técnico | **Alta** | Idempotencia del formato (§2.2), comprobación de 4 bytes por chunk cada 30 s, y petición de chunk completo al menor desacuerdo. La fila 15 de la matriz lo caza |
| La partida del anfitrión crece y unirse en caliente tarda minutos | Rendimiento | Media | Presupuesto de §4.3 medido como criterio, prioridad por distancia, y envío por la cola de baja prioridad para no congelar al anfitrión |
| `FMarineCreatureBrain` deriva entre cliente y servidor | Técnico | Baja | No se pretende determinismo: ancla por grupo cada 2 s (§2.7). El daño lo tira el servidor |
| Tramposos en una partida entre amigos | Seguridad | Baja | Validación completa en el servidor (§1.2) porque es lo correcto de todos modos; no se invierte en anti-cheat. Un anfitrión que hace trampas en su propia partida es su problema |
| El cooperativo se come el presupuesto de H0–H5 y retrasa el acceso anticipado | Alcance | **Alta** | Reparto por hitos de §9 con los cimientos (y solo los cimientos) en H0; ningún sistema se replica antes de estar terminado en solitario |
| Las reglas de §5 se sienten mal en la práctica (dormir, escalado, mapa compartido) | Diseño | Media | Están escritas con números para poder cambiarlas de una en una; ajuste declarado como tarea de H5, con la beta cerrada como fuente |
| Steam falla o el AppId real llega tarde | Producción | Media | Desarrollo con `SteamDevAppId=480` y reserva de `IpNetDriver` con IP directa para las pruebas internas; el AppId real es requisito de la beta cerrada, no de H0 |

---

## 9. Plan de migración por fases y coste

### 9.1 Regla que gobierna el plan

**Nada se replica antes de funcionar en solitario.** El orden es: sistema terminado y
verificado en un jugador → se replica → se prueba en la matriz. Y a la vez: **todo
sistema nuevo se escribe ya con la autoridad en el servidor** (una `Server_` RPC y una
validación, aunque en solitario el servidor sea el mismo proceso), porque ese trabajo es
casi gratis al escribirlo y caro al reconvertirlo. Esa es la razón de decidir el
cooperativo ahora.

### 9.2 Qué hay que convertir en H0, y por qué solo eso

H0 se lleva los **cimientos** y la **prueba de que el modelo funciona** en el caso más
simple: moverse y tocar cosas en Landing. Nada más. Concretamente: Steam y los módulos,
`GameState`/`PlayerState`, el reparto del `WiringSubsystem` (§2.1), el movimiento
replicado, la interacción autoritativa con las validaciones de §1.2, el cuerpo por
jugador, el lobby y la invitación, y el comando de presupuesto `Explored.NetBudget`.

Lo que **no** entra en H0, a propósito: terreno, tala, inventario completo, barcos,
fauna. Ninguno de los cinco está terminado en solitario todavía (`00-TODO.md` lo dice
sistema por sistema), y replicar un sistema a medias es hacer el trabajo dos veces.

### 9.3 Orden y coste, sistema por sistema

Estimación **en días de agente** (una sesión de trabajo completa de un agente, con su
compilación en local y sus specs). Es una estimación honesta de trabajo, no un calendario:
incluye la lectura del código existente, el cambio, la compilación en Windows y las
filas de matriz que le toquen; **no** incluye el tiempo de Rodrigo revisando.

| Orden | Sistema | Hito | Días | Coste relativo | Por qué ese coste |
|---|---|---|---|---|---|
| 1 | Steam OSS + Steam Sockets + `Build.cs` + `DefaultEngine.ini` + `UExploredSessionSubsystem` | H0 | **3** | Bajo | Trabajo de configuración conocido; el riesgo es el AppId, no el código |
| 2 | `AExploredGameState` + `AExploredPlayerState` + reparto de `UExploredWiringSubsystem` (§2.1) | H0 | **4** | **Alto** | 1714 líneas escritas contra un personaje único, y es quien registra 11 secciones de guardado. Es el cambio con más superficie de todo el plan |
| 3 | Movimiento replicado (personaje + `SwimComponent` como `MovementMode`) | H0 | **4** | Medio | `CharacterMovement` hace lo difícil; el nado paralelo es lo que cuesta |
| 4 | Interacción autoritativa (`UInteractionComponent` + los 6 `IExploredInteractable`) | H0 | **3** | Medio | Patrón repetitivo una vez resuelto el primero |
| 5 | Cuerpo y necesidades por jugador (§2.9) | H0 | **4** | Medio | Empaquetado de 24 B, eventos y avisos interiores por dueño |
| 6 | Lobby, invitación, aviso de entrada/salida (§6.1, §6.2) + `Explored.NetBudget` | H0 | **2** | Bajo | Slate sobre widgets que ya existen |
| 7 | Inventario y contenedores (§2.4) + tabla de ids + hash de contenido | H1 | **5** | **Alto** | Es donde un fallo duplica objetos; exige el `FastArray`, la suscripción a cofres y la tabla de ids de todos los `Content/Data` |
| 8 | Vegetación talada por instancia (§2.3) | H1 | **3** | Medio | La clave de red y el tope; el estado ya existe |
| 9 | Fauna: anclas de grupo + fauna terrestre replicada (§2.7) | H1 | **4** | Medio | Dos mecanismos distintos y un tope duro que hay que medir |
| 10 | Reloj, clima, mareas (§2.8) | H1 | **2** | Bajo | 11 bytes y una corrección suave. El código ya es puro |
| 11 | Lista de jugadores, nombres, ping, ajustes de red (§6.3–§6.7) | H1 | **2** | Bajo | Slate y un multicast de 8 B |
| 12 | Dormir en grupo + derribado y reanimación (§5.1, §5.2) | H1 | **3** | Medio | Reglas puras en `ExploredLinks` + estado de pawn + UI |
| 13 | **Terreno: deltas replicados por chunk, cola, comprobación, remallado en cliente (§2.2)** | H2 | **6** | **Muy alto** | El sistema más caro y el de más riesgo: formato de cable, cola con fusión, prioridad por distancia, ráfaga, verificación, y la fila 15 de la matriz durante 2 horas |
| 14 | Construcción replicada (§2.10) | H2 | **3** | Medio | Los actores ya existen; el fantasma local y el orden de colocación son el trabajo |
| 15 | Arena viva solo en el servidor (§2.6) | H2 | **2** | Bajo | Se apoya entera en el canal de terreno ya hecho en el paso 13 |
| 16 | Barcos: estado replicado, pasajeros, timón cedible (§2.5, §5.4) | H3 | **4** | Medio | `FBoatModel` está perfecto para esto; el trabajo es el adjuntado y el aforo |
| 17 | Mapa compartido (`UCartographyComponent` → subsistema, §5.3) + museo y progreso | H4 | **3** | Medio | Mover un componente del pawn al mundo toca guardado y UI |
| 18 | Guardado por jugador, perfiles de invitado, unirse en caliente, salida del anfitrión (§4.3–§4.5) | H4 | **5** | **Alto** | Formato de guardado, ficheros de perfil, secuencia de entrada con presupuesto y los tres caminos de salida |
| 19 | Escalado por número de jugadores + logros en cooperativo (§5.6, §5.7) | H4 | **2** | Bajo | Campos de datos y una función pura |
| 20 | Matriz completa, `pktlag`/`pktloss`, CSV de presupuesto, ajuste de las reglas de §5 | H5 | **5** | **Alto** | 16 filas × varias condiciones, y el ajuste con datos de la beta cerrada |
| | **Total** | | **69** | | |

Reparto por hito: **H0 = 20**, **H1 = 19**, **H2 = 11**, **H3 = 4**, **H4 = 10**,
**H5 = 5**. Y en fases posteriores, ya con los cimientos puestos: **[F2]** raíles y
vagones replicados **+2**; **[F3]** piratas, aldeanos y asaltos autoritativos **+4**.

### 9.4 Honestidad sobre la estimación

69 días de agente es **mucho**: del orden de un tercio del trabajo que `00-TODO.md` ya
tenía pendiente para el acceso anticipado. Decidirlo ahora, antes de escribir los
sistemas que faltan, es lo que evita que sean bastante más de 100: cada sistema que nazca
con `Server_` y validación desde el principio no paga el paso 7 ni el 13 dos veces. El
coste real de la decisión no es 69 días, es la diferencia entre 69 y lo que costaría
convertir todo al final, y esa diferencia es la que justifica decidirlo hoy.

Las tres partidas que pueden desviarse de verdad, y en qué dirección:

- **Paso 2** (reparto del jugador único): puede ser 6 en vez de 4 si los 17 sistemas sin
  compilar en local traen sorpresas al tocarlos. Es lo primero que se hace justamente
  por eso.
- **Paso 13** (terreno): puede ser 9 en vez de 6 si la compresión Oodle no llega a ×2 o
  si el remallado en el cliente no cabe en el presupuesto de fotograma. Las dos cosas se
  miden antes de H2, no después.
- **Paso 20** (pruebas y ajuste): es el que más se estira, porque depende de lo que
  encuentre la beta cerrada. 5 es el suelo, no el techo.

---

## 10. TODO de implementación

Las tareas atómicas y verificables están repartidas por hito en `00-TODO.md` (sección
«Red y cooperativo» de H0 a H5, más las dos entradas de F2 y F3). Aquí solo el resumen
de dependencias, que es lo que no cabe en una casilla:

- Nada de red antes de **§2.1** (paso 2 de §9.3): es la raíz de todo el árbol.
- **§2.6** (arena viva) depende de **§2.2** (terreno): el canal es el mismo.
- **§5.3** (mapa compartido) depende de mover `UCartographyComponent` al mundo, que toca
  el guardado: va después de **§4.4**, en el mismo hito.
- **§5.7** (logros) depende de que `achievements.json` ya tenga `phase` y `rarity`
  (tareas de H5 que ya estaban en la lista): `coopScope` se añade en la misma pasada.
- La matriz de **§7.3** no se pasa entera hasta H5, pero **las filas 1, 2 y 5 son
  criterio de salida de H0** y las filas 3, 4 y 13 lo son de H2.

---

## Fuentes

`docs/diseno/gdd_v2.md` (§3.4, §6.2, §7.2 derogado, §7.3, §7.4), las secciones 01–07 de
esta biblia, y lectura directa del código del árbol principal:
`Source/Explored/Core/ExploredWiringSubsystem.{h,cpp}` (1714 líneas, `GetPlayerCharacter`
en :241), `Source/Explored/Core/SystemLinks.h`,
`Source/Explored/Save/SaveWorldDeltas.h` (`FSaveIndexSet`, `FSaveScatterDeltas`, capa
`Terrain`), `Source/Explored/WorldGen/TerrainEditModel.h` (rejilla de 0,25 m, chunk de
8 m, deltas en milímetros, `ToValue`), `Source/Explored/WorldGen/VegetationHarvestState.h`
(`FVegetationInstanceKey`), `Source/Explored/WorldGen/{FellingModel,HarvestModel}.h`,
`Source/Explored/Carry/{CarryComponent,InventoryModel}.h`,
`Source/Explored/Boats/{BoatModel.h,ExploredBoat.{h,cpp}}` (`FixedStepS`, `CrewMassKg`,
`SetSimulatePhysics(false)` en :100), `Source/Explored/Ocean/OceanWaves.h`,
`Source/Explored/Survival/{SurvivalModel,BodyModel,BodySignalsComponent}.h`,
`Source/Explored/Player/{ExploredCharacter,SwimComponent}.h`,
`Source/Explored/Sky/TimeOfDaySubsystem.h` (`DayLengthMinutes = 40`),
`Source/Explored/Fauna/{FaunaSpawning,MarineCreatureBrain,ExploredFaunaManager}.h`,
`Source/Explored/Building/{BuildingSubsystem,ExploredBuildingPiece}.h`,
`Source/Explored/Cartography/CartographyComponent.h`,
`Source/Explored/Interaction/InteractionComponent.h` (`TraceDistanceCm = 250`),
`Source/Explored/UI/{ExploredHUD,ExploredSaveSubsystem}.h`, `Source/Explored/Items/ExploredItemActor.cpp`
(:15, la única física simulada del juego), `Source/Explored/Explored.Build.cs`.
