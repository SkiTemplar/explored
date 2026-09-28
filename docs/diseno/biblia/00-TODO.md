# EXPLORED — Lista maestra de TODO

Versión 1.1 · 2026-09-28 (revisión de estado contra `main`; la 1 es del 2026-09-27) ·
Fusiona los «TODO de implementación» de las 7 secciones de la biblia (`01`–`07`), añade
lo transversal que no tenía dueño (rendimiento, arte, audio, terreno editable en
runtime, empaquetado y salida a mercado) y lo agrupa por hitos en orden de ejecución.
Cada casilla indica el sistema o fichero al que toca y la sección de la biblia (o el
GDD) de la que sale. `[x]` se ha verificado con grep contra el `main` del árbol
principal (`C:\Users\Rodrigo\PERSONAL\ProyectosPersonales\Explored\Explored`), no se ha
supuesto; `[ ]` es lo que falta o no se ha podido confirmar en el código. Desde la
versión 1.1 cada `[x]` nuevo cita su commit y su PR de `main`.

Aviso heredado de `docs/roadmap.md` y GDD §8: varios sistemas ya escritos en `main`
llevan tiempo «sin compilar en local» (17 según el roadmap al cierre de esta biblia). Un
`[x]` aquí certifica que el código **existe**, no que compila o que está verificado en
PIE — la verificación de compilación es tarea propia de H0/H1, ya listada abajo.

## Recuento de casillas por hito

| Hito | Hechas `[x]` | En parte | Sin empezar | Total | % hecho | % ponderado¹ |
|---|---|---|---|---|---|---|
| H0 — Porción vertical jugable en Landing | 7 | 10 | 26 | 43 | 16 % | 28 % |
| H1 — Mundo interactivo | 2 | 5 | 29 | 36 | 6 % | 13 % |
| H2 — Minería y construcción | 2 | 11 | 18 | 31 | 6 % | 24 % |
| H3 — Mar y barcos | 1 | 6 | 7 | 14 | 7 % | 29 % |
| H4 — Contenido de acceso anticipado | 0 | 2 | 18 | 20 | 0 % | 5 % |
| H5 — Lanzamiento del acceso anticipado | 1 | 0 | 19 | 20 | 5 % | 5 % |
| F2 | 1 | 4 | 11 | 16 | 6 % | 19 % |
| F3 | 1 | 0 | 26 | 27 | 4 % | 4 % |
| **Total** | **15** | **38** | **154** | **207** | **7 %** | **16 %** |

¹ Cuenta cada casilla «en parte» como media. «En parte» sigue siendo `[ ]`: lleva debajo
una línea `→ **En parte:**` con el commit, la PR y lo que falta.

Revisión del 2026-09-28: auditoría casilla a casilla contra `origin/main` (`266a8cc`,
tras la PR #69) con `git grep` y `git log`. Seis casillas pasan a `[x]` (de 9 a 15), cada
una con su commit y su PR en una línea `→ **Hecho:**`, y 38 quedan anotadas como «en
parte». El patrón es el mismo en casi todas: las PR de la nube (#39 tala, #40 terreno
editable, #46 arena viva, #50 datos de minería y fauna, #53 astillero, #57 raíles, #58
incendio) entregaron el **modelo puro con su spec**, pero ningún actor, componente ni
subsistema los usa todavía, y las casillas piden la mecánica en juego. Por eso cuentan
como «en parte» y no como hechas. Cerrar H0 pasa sobre todo por enganchar
`FTerrainEditModel` (picado, remallado y guardado) y `FFellingModel` a actores, trabajo
que necesita Unreal. Sigue sin haber una sola línea de replicación en `Source/`: las 43
casillas de red están pendientes.

Revisión del 2026-09-27 (tarde): **+43 casillas de red y cooperativo** repartidas de H0
a H5 más dos en F2/F3, tras la decisión del director de meter cooperativo de 2 a 4
jugadores con servidor de escucha por Steam en el acceso anticipado. El diseño completo
está en `08-cooperativo-y-red.md`; el coste estimado de esas casillas es de **69 días de
agente** (§9.3 de esa sección). Ninguna está hecha: hoy no hay una sola línea de
replicación en `Source/`.

---

## H0 — Porción vertical jugable en Landing

Criterio de salida (GDD §6.1): aterrizar, sobrevivir, construir, cavar un agujero que
se queda cavado al recargar, cartografiar y encontrar un tesoro, todo en Landing, sin
salir de la isla.

### Cuerpo y supervivencia

- [ ] `Survival`: añadir `ECondition::ContactBurn` (quemadura de contacto) — daño
      instantáneo 8 pts + herida de profundidad 0.4 que no sangra ni se infecta,
      cicatriza en 24 h (12 h con gel de aloe). *(biblia 01 §6.8)*
- [ ] `Survival`/`Items`: dar a los ítems de gel de aloe la propiedad `Cures` sobre
      `ContactBurn` además de `SunBurn`, con el multiplicador ×0.5 al tiempo de
      cicatrización de esa herida. *(biblia 01 §6.8)*
- [ ] `Player/SwimComponent`: enganchar el delegado de daño por ahogo
      (`OnDrowningDamage`) a `BodySignalsComponent::GetMutableSurvivalState().Health` —
      hoy el delegado existe pero nadie aplica el daño. *(biblia 01 §6.15)*
- [ ] `Survival`/`Player`: confirmar y, si hace falta, ajustar el punto de aparición
      inicial fijo en Isla del Amaraje con el kit cerrado (mochila, reloj, gafas de sol
      puestos; mechero, navaja rota, botiquín, manual, cantimplora en el fuselaje) —
      `SpawnLandingStarterKitIfNeeded` ya existe en `ExploredWiringSubsystem`, verificar
      que su contenido coincide con la lista. *(biblia 01 §4)*
- [ ] `Survival`: fijar `Wetness = 1.0` como valor inicial explícito de
      `FSurvivalState` al arrancar una partida nueva — hoy la struct usa `0.0` por
      defecto. *(biblia 01 §4)*
- [ ] `Survival`/`UI`: implementar los avisos interiores ES/EN de 01 §6 como líneas de
      voz interna (subtítulo opcional) enganchadas a los eventos de `ESurvivalEvent` y a
      los cruces de umbral de cada estado. *(biblia 01 §6, 06 §3.1)*
- [ ] `Core/SystemLinks`: documentar en código (comentario junto a `HasPermadeath`) que
      el modo Personalizado une «necesidades pueden matar» y «permadeath» en un único
      interruptor de «modo duro». *(biblia 01 §8)*
- [ ] `Save`/`GameMode`/`Carry`: confirmar en código que `HandlePlayerDeath` nunca vacía
      el inventario en Explorador/Superviviente/Personalizado, y aplicar el golpe de
      ánimo −6 (`moraleEvents.Injured`, ya existe) al reaparecer. *(biblia 01 §7, 03 §1.7)*
      → **En parte:** `22ec751` (anterior a #41), `ExploredGameMode.cpp:63-138`: no se vacía
        el inventario — falta aplicar el −6 de `moraleEvents.Injured` (hoy es un −15 fijo) y
        dejarlo explícito en código.
- [x] Salud, heridas y sangrado (`FWound`, `TreatWounds`) ya implementados en
      `BodyModel.{h,cpp}` — base sobre la que se engancha `ContactBurn`.
      *(verificado: `Source/Explored/Survival/BodyModel.h`)*
      → **Revisión 2026-09-28:** `FWound` vive en `Survival/SurvivalModel.h:109`;
        `TreatWounds` en `BodyModel.h:98`.

### Crafteo e inventario

- [ ] `Items`/`Templates`: añadir el item `pico` y su plantilla a
      `Content/Data/items.json`/`templates.json` — Cabeza (Contundente≥3 o Rígido≥3),
      Mango (Largo≥2, Rígido≥3), Unión (Ata≥2 o Adhesivo≥2), `baseMaxDurability` 55.
      Bloqueante para la minería manual de Landing. *(biblia 02 §2.2, 03 §2.1 — definición
      única tras resolver la contradicción con la versión antigua de 03)*
      → **En parte:** `46382a9` (PR #50), `items.json:467`, `templates.json:171-180` — falta
        alinear la Cabeza con la biblia: hoy pide Punta≥2 con etiqueta piedra/mineral/metal,
        no Contundente≥3 o Rígido≥3.
- [ ] `Items`: añadir a `items.json` las medicinas nuevas que exige la porción vertical:
      `vendaje_tela`, `antidoto_corteza`, `carbon_activado`, `te_corteza_sauce`,
      `ferula_bambu`, `gel_aloe`. *(biblia 03 §3.6)*
- [ ] `Carry`: fijar `BackpackComfortBonusKg` real para `mochila` (+8 kg),
      `mochila_fibra` (+10 kg) y `mochila_cuero_bambu` (+20 kg) en
      `UCarryComponent::SetCustomBackpack`, que hoy solo recibe volumen/peso del objeto.
      *(biblia 03 §1.1)*
      → **En parte:** `22ec751`, `Carry/InventoryModel.cpp:16-17, 995-1020` — el bonus
        existe pero vale 2/2/6 kg en vez de 8/10/20 y `SetCustomBackpack` lo deja a 0.
- [ ] `Building`: añadir a `building_pieces.json` las piezas de almacenamiento
      `cesta_almacen`, `estanteria_almacen`, `arcon` — sin ellas ni el refugio→cabaña de
      Landing tiene sentido. *(biblia 03 §1.4)*
- [ ] `AExploredCharacter::HandleCombine`: cuando `Verbs.Num() > 1`, no aplicar
      `Verbs[0]` de inmediato — abrir la lista de verbos candidatos y esperar
      confirmación o expiración (4 s). *(biblia 06 §2.6)*
- [ ] Crear el widget `SExploredCraftChoice` (hasta 3 filas, tecla/botón + nombre de
      verbo, sin vista previa del resultado, expiración automática). *(biblia 06 §2.6)*

### Minería y terreno (pipeline mínimo para la cueva de Landing)

- [x] `WorldGen`: añadir `FTerrainEdits` (capa de ediciones dispersa por chunk) sobre
      `FTerrainDensity`. *(GDD §7.3 punto 1, biblia 02 §7.3)*
      → **Hecho:** `cb4e5a6` (PR #40): `FTerrainEditModel`, deltas dispersos por chunk sobre
        la densidad base, `WorldGen/TerrainEditModel.h:136`.
- [ ] `WorldGen`: `FTerrainDensity::Density` consulta primero la capa de ediciones antes
      de evaluar el ruido procedural. *(GDD §7.3 punto 1)*
      → **En parte:** `cb4e5a6` (PR #40), `TerrainEditModel.h:189` (`Density(P, Base)`) —
        falta que `FTerrainDensity::Density` y el horneado consulten la capa.
- [ ] `WorldGen/TerrainChunkBuilder`: invalidar y reconstruir solo los chunks tocados
      por una edición. *(GDD §7.3 punto 3)*
      → **En parte:** `cb4e5a6` (PR #40), `TerrainEditModel.h:44-47` (`DirtyChunks`) — el
        modelo sabe qué chunks tocar; falta el remallado en tiempo de juego.
- [ ] `WorldGen`: implementar el picado por esfera (radio y tiempo por golpe según
      herramienta/estrato, tabla de biblia 02 §2.3) para tierra/arena/arcilla (dureza 1,
      pala tosca) — el resto de estratos no hace falta para Landing. *(biblia 02 §2)*
      → **En parte:** `cb4e5a6` (PR #40), `TerrainEditModel.h:60-69, 144-172` — picado por
        esfera con dureza y nivel de herramienta solo en el modelo puro; falta enganchar
        pala y pico a un actor.
- [x] `Save`: nueva capa `"terrain"` en `FSaveWorldDeltas` (deltas de edición por chunk,
      mismo patrón que `FSaveScatterDeltas`) — condición dura del criterio de salida
      («se queda cavado al recargar la partida»). *(GDD §7.3 punto 2, biblia 02 §2.8)*
      → **Hecho:** `064b73a` (PR #40), `Save/SaveWorldDeltas.cpp:613-625`. Ojo: nada en el
        juego escribe aún en esta capa, así que el criterio «se queda cavado al recargar»
        depende de la casilla de picado.
- [ ] `Cartography`: hoja subterránea por sistema de galerías, generada bajo demanda al
      entrar la primera vez, para la cueva pequeña de Landing. *(GDD §3.2)*
- [x] `Items`: nuevo item `tierra_suelta` (paralelo a `arena`, ya existente) en
      `items.json`. *(biblia 02 §2.7)*
      → **Hecho:** `46382a9` (PR #50), `Content/Data/items.json:96`.

### Tala y recolección

- [ ] `WorldGen/HarvestModel`: revisar la tabla de especies talables de biblia 02 §1.2
      (golpes, tiempo, botín, altura) contra las reglas reales de `HarvestModel.cpp`
      (hoy usa `HitsBareHands`/`HitsWithTool` por especie genérica `Palm`/`JungleGiant`/
      `JungleWide`/`Mangrove`, no la tabla por herramienta de la biblia) y decidir cuál
      manda antes de tocar el código. *(biblia 02 §1.2)*
      → **En parte:** `6fb9833` (PR #39): `FFellingModel` ya tiene golpes por herramienta
        (`WorldGen/FellingModel.cpp:54-194`) — falta la decisión escrita frente a la tabla
        de la biblia y retirar `HitsBareHands`/`HitsWithTool` de `HarvestModel`.
- [ ] `WorldGen`: dirección de caída (golpe + viento) y colisión contra construcción
      ligera/terreno al talar. *(biblia 02 §1.2)*
      → **En parte:** `6fb9833` (PR #39), `FellingModel.h:149` (`ResolveFallDirection`,
        golpes + pendiente) — falta el viento, la colisión con construcción y engancharlo a
        un actor.
- [ ] `WorldGen/VegetationHarvestState`: estado `Stump` con día de rebrote (18/24/4 días
      según especie) — hoy `RegrowHours` es `0.0f` (permanente) para
      `Palm`/`JungleGiant`/`JungleWide` y solo `Mangrove` rebrota (480 h). Decisión de
      diseño nueva de la biblia, pendiente de aplicar al código. *(biblia 02 §1.2, 02 §1.6)*
      → **En parte:** `6fb9833` (PR #39), `FellingModel.h:100-113` — el tocón con rebrote
        existe, pero con 12/20/15/20 días en vez de 18/24/4, y `VegetationHarvestState` aún
        no lo usa.
- [ ] `WorldGen`: generación periódica de `rama_seca` bajo cada árbol (2–4 cada 6 h,
      tope 6). *(biblia 02 §1.3)*
      → **En parte:** `bb07614` (PR #39), `WorldGen/GroundBranchModel.h:55-71` — modelo puro
        con otro ritmo y tope (1,5/día, tope 4); falta ajustar a 2–4 cada 6 h con tope 6 y
        engancharlo.
- [x] Tala de la Palmera de coco ya suelta `coco_maduro` ×1-3 al caer (no un mix de
      `coco_verde`/`coco_maduro`) — corregido en la tabla de biblia 02 §1.2 para que
      coincida con `HarvestModel.cpp::Palm.FellDrops`.
      *(verificado: `Source/Explored/WorldGen/HarvestModel.cpp:29`)*
- [ ] `Items`/`recipes.json`: confirmar que `coco_verde` se recoge directamente de la
      copa de una palmera trepada (§13.1 nueva de escalada), sin golpe ni herramienta,
      distinto del `coco_maduro` que suelta la tala. *(biblia 02 §13.1, §1.2)*

### Granja (huerto y limonero de Landing)

- [x] Confirmar que `FarmModel`/`plants.json` ya cubren riego y ventana de estaciones tal
      como se documenta en biblia 02 §10.1 (sin cambios esperados; solo verificación).
      *(biblia 02 §10.1)*
      → **Hecho:** `22ec751` + riego en `c0a6f1c` (PR #36): `Farming/FarmModel.h:15-24,
        60-63`, `plants.json:23-24`.
- [x] Cultivos (limonero, platanera, taro, batata, piña, maracuyá) ya en `plants.json`
      con etapas estáticas por días, estación y riego.
      *(verificado: `Content/Data/plants.json`)*

### Red y cooperativo — cimientos (biblia 08)

Orden obligatorio: nada de lo de abajo se toca antes de que la tercera casilla
(`GameState`/`PlayerState`) y la cuarta (reparto de `ExploredWiringSubsystem`) estén en
verde en local. Criterio de salida de red de H0: las filas **1, 2 y 5** de la matriz de
biblia 08 §7.3 pasan en condiciones «Normal».

- [ ] `Explored.Build.cs`: añadir `OnlineSubsystem`, `OnlineSubsystemSteam`,
      `OnlineSubsystemUtils`, `SteamSockets`, `NetCore`; `Config/DefaultEngine.ini`: el
      bloque de `NetDriverDefinitions`, `[/Script/OnlineSubsystemSteam]` con
      `SteamDevAppId=480` y los tres topes de tasa (`32000` B/s, `NetServerMaxTickRate=30`).
      Verificable: el editor arranca y `Play As Listen Server` con 2 clientes conecta.
      *(biblia 08 §1.1)*
- [ ] `Core`: nuevo `UExploredSessionSubsystem` (`GameInstance`): crear sesión con
      `bUseLobbiesIfAvailable`, aforo 2–4, privacidad «Solo por invitación»/«Amigos»/
      «En solitario», clave `EXPLORED_WORLD = <WorldId>`, aceptar invitación de Steam.
      *(biblia 08 §4.1, §4.2)*
- [ ] `Core`: nuevos `AExploredGameState` (reloj, clima, semilla, `WorldId`, aforo,
      escalado por jugadores) y `AExploredPlayerState` (nombre visible, `SteamID64`, tono
      de tinta del mapa, estado `Derribado`, ping). *(biblia 08 §2.1)*
- [ ] `Core/ExploredWiringSubsystem`: eliminar `GetPlayerCharacter()`
      (`ExploredWiringSubsystem.cpp:241`) y mover `Sample`/`UpdateBody`/`UpdatePlace`/
      `UpdateBoat`/`UpdateDanger` a un `UExploredPlayerLinksComponent` nuevo por
      personaje; el subsistema pasa a iterar `GameState->PlayerArray`. Verificable: dos
      personajes en PIE tienen necesidades y lugar independientes. *(biblia 08 §2.1)*
- [ ] Sustituir los 8 usos de `UGameplayStatics::GetPlayerController/GetPlayerPawn(…, 0)`
      (`ExploredAmbienceSubsystem`, `ExploredMusicSubsystem`, `ExploredOcean`,
      `ExploredSkyController`, `ExploredPlantActor`, `ExploredFaunaManager`,
      `ExploredShotSubsystem`) por el jugador **local** de cada cliente.
      *(biblia 08 §2.1)*
- [ ] `Player/ExploredCharacter`: movimiento replicado con predicción y corrección
      (`NetworkSmoothingMode = Exponential` para pawns ajenos, corrección invisible por
      debajo de 8 cm); `USwimComponent` pasa a `CustomMovementMode` para que el servidor
      lo vea. *(biblia 08 §1.2)*
- [ ] `Interaction/InteractionComponent`: `Server_Interact` con validación de distancia
      (250 cm + 50 de margen), línea de visión trazada en el servidor, cadencia mínima de
      0,25 s, ventana de gracia de 250 ms y tope de 20 acciones válidas/s por jugador.
      El cliente reproduce solo animación y sonido, nunca el efecto. *(biblia 08 §1.2)*
- [ ] `Survival`: replicar el cuerpo propio a 1 Hz con `COND_OwnerOnly` (24 B: 8
      necesidades a `uint8`, máscara de 19 estados, hasta 4 heridas de 3 B) y 2 B a
      0,5 Hz a los demás (salud + banderas visibles). *(biblia 08 §2.9)*
- [ ] `UI`: nuevo `SExploredCoopLobby` (cuatro renglones, «Invitar por Steam»,
      privacidad, aforo) y avisos de entrada/salida en la cola del HUD, con los textos
      ES/EN de biblia 08 §6.1–§6.2 y §6.5. *(biblia 08 §6)*
- [ ] `Debug`: comando `Explored.NetBudget` que vuelca a CSV los kbps por canal y por
      cliente cada segundo (es la herramienta con la que se verifica el objetivo de
      ancho de banda, no una estimación). *(biblia 08 §3)*
- [ ] `Tools/net-test.ps1` (nuevo): arranca PIE como servidor de escucha con 2 o 4
      clientes, aplica los perfiles «Normal»/«Mala»/«Horrible» de `net pktlag`/
      `pktlagvariance`/`pktloss`/`pktorder` y recoge el CSV de `Explored.NetBudget`.
      *(biblia 08 §7.1, §7.2)*

### Pantallas y HUD de la partida (bucle básico)

- [ ] Implementar la pantalla de Muerte: nuevo widget `SExploredDeathScreen`,
      disparado al llegar `FSurvivalState::Health` a 0; tres opciones (dos en Náufrago).
      *(biblia 06 §2.13)*
- [ ] Implementar la transición de Dormir: interactuable «Dormir» en piezas de
      cama/refugio, `SExploredFade` con etiqueta de hora, avance acelerado del reloj y
      cancelación con recuperación proporcional. *(biblia 06 §2.12)*

---

## H1 — Mundo interactivo

Sistemas que dan vida al archipiélago entero (fuego, escalada, combate básico, fauna,
accesibilidad, HUD general) sin depender de una isla o mecánica concreta de fase
posterior.

### Escalada (mecánica nueva completa)

- [ ] `Player`: verbo contextual «Trepar» sobre palmeras (`Palm` en
      `HarvestModel.cpp`), subida a 0,7 m/s sin herramienta / 1,3 m/s con
      `pie_de_palmera`, coste de Energía −9×peso / −6×peso por segundo.
      *(biblia 02 §13.1)*
- [ ] `Items`/`Templates`: nuevo item `pie_de_palmera` (`cuerda` ×1) en `items.json`.
      *(biblia 02 §13.1)*
- [ ] `Player`/`WorldGen`: escalada de roca sobre pendiente > 60°, tope de 3 m sin
      herramienta; caída y esguince al agotar Energía, reutilizando el sistema de
      caída/esguince ya existente (`SprainFallHeight`, biblia 01 §6.13).
      *(biblia 02 §13.2)*
- [ ] `Player`: animación de trepa en primera persona con manos visibles y bamboleo de
      cámara simple, con «Reducir movimiento» aplicado igual que el resto de cámara.
      *(biblia 02 §13.5, 06 §3.4)*

### Fuego y clima

- [ ] `Weather`/`WorldGen`: contagio de fuego entre celdas de vegetación (45 %/s en
      seco, −70 % en estaciones húmedas, ±25 % por viento). *(biblia 02 §6)*
      → **En parte:** `d0d556c` (PR #58): `FWildfireModel`,
        `WorldGen/WildfireModel.h:109-113` (450 ‰ seco, 135 ‰ húmedo, ±250 ‰ viento) — solo
        modelo puro; ningún actor ni subsistema lo usa.
- [ ] `WorldGen`: rebrote de zona quemada (12 días hierba, 25 días arbustos),
      compartiendo temporizador con el rebrote de tala. *(biblia 02 §6)*
      → **En parte:** `d0d556c` (PR #58), `WildfireModel.h:121-122` — rebrote 12/25 días
        solo en el modelo, sin cablear y con un reloj propio, no el de la tala.

### Combate y fauna peligrosa (sistema, no contenido de fase 3)

- [ ] Fórmulas de daño instantáneo (cortante/perforante ×3, contundente ×4) leyendo la
      propiedad real Filo/Punta/Contundente del objeto. *(biblia 05 §3.0)*
- [ ] Apertura de corte con profundidad = propiedad ÷ 5, enganchada al sistema `wounds`
      ya existente en `BodyModel` (sin una segunda barra de heridas). *(biblia 05 §3.0)*
- [ ] Golpe rápido (×0.7, encadenable ×3 + pausa 0.4 s) y golpe cargado (×1.6, telegraph
      1.2 s) como variantes de la misma acción de ataque. *(biblia 05 §3.1)*
- [ ] Esquiva con invulnerabilidad de 0.3 s y reutilización de 1.2 s. *(biblia 05 §3.1)*
- [ ] Caída de precisión del arco por distancia (100/70/40/0 %). *(biblia 05 §3.2)*
- [ ] `Fauna`: estadísticas de combate de cerdo salvaje, cabra montés y cangrejo de los
      cocoteros. *(biblia 05 §5)*
      → **En parte:** `46382a9` (PR #50), `Content/Data/fauna.json:42, 102, 127` —
        estadísticas solo en datos; no hay especie C++ ni código que las lea.
- [x] `Fauna`: tiburón de arrecife genérico como variante no legendaria del tiburón
      tigre «Sombra» ya descrito en la biblia de contenido §4.6. *(biblia 05 §5)*
      → **Hecho:** `22ec751` (anterior a #41): `EFaunaSpecies::ReefShark` separado de
        `TigerShark`, `Fauna/FaunaTypes.cpp:42`, `MarineCreatureBrain.cpp:310`
        (`ReefSharkAttackRoll`).

### Inventario y UI general

- [ ] `Carry`/`InventoryModel`: implementar el apilado de hasta 10 unidades por hueco
      para objetos sin `maxDurability` ni `LiquidCapacityLiters` — hoy `FInventoryEntry`
      es un objeto por hueco. *(biblia 03 §1.3)*
- [ ] `BuildPreviewComponent::SetupInput`: añadir mapeo de mando (`LB` entra/sale, `RB`
      rota, D-Pad cicla pieza, Face Bottom confirma) — hoy solo tecla/ratón.
      *(biblia 06 §2.7)*
- [ ] Añadir trama de rayas diagonales (además del rojo) al material del fantasma de
      construcción sin apoyo suficiente. *(biblia 06 §2.7, §3.2)*
- [ ] Añadir mapeo de mando para `IA_Fish` (hoy solo tecla `F` para lanzar/recoger).
      *(biblia 06 §2.4, TODO)*
- [ ] Sustituir la etiqueta de mano por el dial de brújula cuando la mano lleva el
      objeto «Brújula». *(biblia 06 §2.4)*
- [ ] Añadir texto de apoyo a la barra de tensión de pesca («A punto de romperse») para
      no depender solo del degradado verde→rojo. *(biblia 06 §2.4, §3.2)*
- [ ] Subtítulos de eventos sonoros (`[cuerno pirata a lo lejos]`, `[la vela cruje]`,
      etc.) enrutados por `bSubtitlesEnabled` a la cola de notificaciones del HUD.
      *(biblia 06 §3.1)*
- [ ] Aplicar el multiplicador de `EExploredTextSize` de forma centralizada a los
      estilos de Slate, no por widget. *(biblia 06 §3.3)*
- [ ] `bReduceMotion`: cablear a `bCameraBobEnabled`, al rebote del aviso de logro, al
      pulso de opacidad del fantasma de construcción y a la easing de apertura de menús
      — el ajuste existe, falta el consumidor en los cuatro sitios. *(biblia 06 §3.4)*
      → **En parte:** `22ec751`, `Player/ExploredCharacter.cpp:277` (bamboleo) y
        `SExploredMapInHands.cpp:86` — faltan el aviso de logro, el pulso del fantasma y la
        apertura de menús.
- [ ] `bDisableFlashing`: cablear al flash de la cámara desechable, al parpadeo de rayo
      de `Weather` y a cualquier destello de pantalla completa por daño.
      *(biblia 06 §3.4)*
- [ ] Confirmar en playtesting si «Mochila» debe ser alternable o de mantener pulsado
      (hoy `bBackpackOpen` es un alternador simple). *(biblia 06 §2.5)*
- [x] Umbral del 55 % para barras de necesidad, máximo tres verbos de contexto y
      «papel y tinta» como piel única ya implementados y verificados en código.
      *(verificado: `Source/Explored/UI/ExploredHUD.cpp:248`,
      `Source/Explored/Interaction/InteractionComponent.cpp:111-114`)*

### Fauna terrestre (sistema, primera pasada)

- [ ] `Fauna`: primera pasada de fauna salvaje terrestre (cerdo, cabra, aves que se
      posan) con LOD (`FFaunaLod` ya existente) y navegación invalidada por chunk
      minado. *(biblia 02 §11)*
      → **En parte:** `46382a9` (PR #50, `fauna.json`) y `bdcc33f` (PR #60, malla del
        jabalí) — solo datos y malla; falta la especie C++, el LOD aplicado y la navegación
        por chunk.

### Red y cooperativo — inventario, fauna, reloj y reglas de grupo (biblia 08)

- [ ] `Carry`: replicar el inventario propio como `FFastArraySerializer` de entradas de
      13 B con `COND_OwnerOnly`, y las dos manos a todos (6 B) para la malla visible.
      Coalescencia a 10 Hz. *(biblia 08 §2.4)*
- [ ] `Items`: tabla de ids `uint16` derivada de ordenar los ids de `Content/Data/*.json`
      (items, plantillas, piezas, plantas, barcos, logros) + `FExploredContentHash`
      (FNV-1a de 64 bits) en el saludo de conexión, con rechazo y el texto de biblia 08
      §6.5 si no coincide. *(biblia 08 §2.4)*
- [ ] `Carry/ExploredContainer`: el contenido no se replica hasta abrir el cofre
      (`Server_SubscribeContainer`, baja al cerrar); la carrera de dos jugadores sobre el
      mismo hueco se resuelve con `EInventoryFail::NotFound`, sin bloqueos.
      *(biblia 08 §2.4)*
- [ ] `Items/ExploredItemActor`: `bReplicateMovement` a 10 Hz, dormir el cuerpo físico a
      los 3 s de quietud (y dejar de replicar), `NetCullDistanceSquared` 6 000 cm y tope
      de 32 objetos sueltos despiertos a la vez. *(biblia 08 §2.5)*
- [ ] `WorldGen/VegetationHarvestState`: estado por instancia replicado como
      `FFastArraySerializer` (clave de 7 B: celda + especie + índice; estado de 3 B:
      etapa, golpes, día de rebrote ×4), tope de 4096 entradas con compactación a
      snapshot por celda reusando `FSaveIndexSet::Encode`. Progreso de tala solo a
      clientes a < 60 m. *(biblia 08 §2.3)*
- [ ] `Fauna`: anclas por grupo cada 2 s (10 B: id, centroide cuantizado, estado) para la
      fauna de ambiente que cada cliente simula en local, y actores replicados (14 B) para
      la terrestre cazable, con el tope duro de 12 a 10 Hz + 24 a 2 Hz enganchado a
      `FFaunaLod`. `ReefSharkAttackRoll` solo en el servidor, una tirada por nadador.
      *(biblia 08 §2.7)*
- [ ] `Sky`/`Weather`: replicar los 11 B de reloj, estación, viento, lluvia, mar y
      tormenta a 0,2 Hz en el `GameState`; el cliente avanza su reloj local y corrige con
      `TimeScale` entre 0,95 y 1,05, con salto duro solo por encima de 6 minutos de juego
      de error. Olas, mareas y corrientes se calculan en local. *(biblia 08 §2.8)*
- [ ] `Core/SystemLinks`: reglas puras de cooperativo con spec de host — dormir en grupo
      (`TimeScale` ×120 solo con todos acostados, vuelta a ×1 al levantarse uno,
      conservando las horas ganadas) y `Derribado` (90 s, reanimación de 6 s o 3 s con
      medicina, alta al 25 % de salud y ánimo −6, tope de 2 reanimaciones por día, sin
      `Derribado` en Náufrago). *(biblia 08 §5.1, §5.2)*
- [ ] `UI`: `SExploredPlayerList` como pestaña de `SExploredPauseMenu` (tinta, nombre,
      retardo, expulsar), nombres sobre la cabeza (hasta 60 m, desvanecido 45–60 m, sin
      verse a través del terreno, sin barra de vida), rueda de ping de tres opciones
      (`Q`, marca de 8 s, 1 cada 3 s, dibujada también en el mapa) y pestaña de ajustes
      de red con los cinco ajustes y los textos ES/EN de biblia 08 §6.3–§6.8.
      *(biblia 08 §6)*

### Tests

- [ ] `Tests`: extender `CarrySpec.cpp` con el apilado de inventario; añadir specs de
      host para `ContactBurn` (patrón de `BodySpec.cpp`). *(biblia 01 §Tests, 03 §Tests)*

---

## H2 — Minería y construcción

Estratos más allá de tierra/arena, riesgos de mina completos, fundición, transporte de
mineral y las piezas de construcción avanzadas que dependen de ellos.

- [ ] `WorldGen`: extender el picado por esfera a caliza, basalto, veta de cobre, hierro
      de meteorito, obsidiana, azufre y cristal (tabla completa de biblia 02 §2.3), con
      la regla de rotura extra del pico de obsidiana contra dureza ≥ 3 (8 % por golpe,
      −15 durabilidad). *(biblia 02 §2)*
      → **En parte:** `cb4e5a6` (PR #40) + `46382a9` (PR #50), `TerrainEditModel.h:12-19`,
        `mining.json:75-132` — el modelo tiene 5 materiales; cobre, hierro, azufre y cristal
        solo están en datos y la rotura de la obsidiana solo en JSON.
- [ ] `Building`: pieza `viga_apoyo` (apuntalamiento) y regla de derrumbe (hueco > 3 m de
      luz sin apoyo, colapsa a los 8 s). *(biblia 02 §2.4, §2.7)*
- [ ] `Survival`/`WorldGen`: indicador de aire viciado en bolsas cerradas a más de 15 m
      de una salida, sin HUD, leído en el cuerpo. *(biblia 02 §2.4)*
- [ ] `WorldGen`/`Ocean`: inundación de galería conectada al mar o al nivel freático
      (1 m/40 s sin sellar) y crecida de monzón (30 % durante la estación).
      *(biblia 02 §2.4)*
- [ ] `WorldGen`: carvings grandes (cenotes, tubos de lava, cavernas de cristal, ríos
      subterráneos, templos enterrados, grutas de marea) como `FCaveDesc` mayores, con
      radio de exclusión de 1,5 m alrededor de un tesoro. *(biblia 02 §2.5, §12)*
- [ ] Prueba de estrés de guardado de minería extensa antes de M3. *(GDD §7.4, biblia
      02 §2 TODO)*
- [ ] `WorldGen`: modo «camino» de la pala (aplanar franja, −15 % coste de movimiento
      sobre camino terminado). *(biblia 02 §3)*
      → **En parte:** `cb4e5a6` (PR #40), `TerrainEditModel.h:87, 163` (`bMarkPath`) — falta
        el −15 % de coste de movimiento y enganchar la pala.
- [ ] `WorldGen`: simulación de ángulo de reposo de arena (34° seca / 45° húmeda,
      revisión de pendiente 1/s por chunk activo). *(biblia 02 §5.1)*
      → **En parte:** `e6c89d1` (PR #46): `FSandModel`, `WorldGen/SandModel.h:139` (34°/45°,
        revisión 1 s) — modelo puro sin enganchar.
- [ ] `WorldGen`/`Ocean`: relleno de arena excavada por oleaje en franja intermareal
      (20 %/35 % por medio ciclo de marea). *(biblia 02 §5.2)*
      → **En parte:** `e6c89d1` (PR #46), `SandModel.h:142-145` (`ApplyHalfTide` 20 %/35 %)
        — nadie lo llama desde la marea.
- [ ] `Building`: pieza `tablon_contencion` (ancla arena, detiene deslizamiento/relleno
      en 1 m). *(biblia 02 §5.3)*
      → **En parte:** `e6c89d1` (PR #46), `SandModel.h:149, 196` (`SetAnchor` a 1 m) — falta
        la pieza en `building_pieces.json`.
- [ ] `Items`/`Templates`: añadir a `items.json`/`templates.json` `lingote_cobre`,
      `lingote_hierro`, `alambre`, `clavos`, `sierra_diente_tiburon`, `tela_fibra`,
      `carretilla`. *(biblia 03 §3.2–3.4)*
- [ ] `Carry`: nuevo `ECarrySlot`/actor `carretilla` (empuje `DosManos`, contenedor
      propio 40 L/25 kg, −30 % velocidad mientras se empuja, sin nadar/correr/escaleras
      enganchada). *(biblia 03 §1.5)*
- [ ] `Building`: añadir a `building_pieces.json` `banco_chatarra`, `horno_fundicion`,
      `yunque`, con su coste. *(biblia 03 §2.2)*
      → **En parte:** `46382a9` (PR #50), `building_pieces.json:1205` (`banco_chatarra`) —
        faltan `horno_fundicion` y `yunque`.
- [ ] `Cooking`/`Fuels`: nuevo nivel de fuego `horno_fundicion` (heat 1.4); recetas de
      fundición en un fichero nuevo `recipes_smithing.json`. *(biblia 03 §2.2, §4.3)*
- [ ] `WorldGen/TerrainDensity`/`WorldGenCommandlet`: verificar que el carving del tubo
      de lava del Humo tiene una boca visible desde el marae de la cumbre, para que las
      ruinas queden junto a una entrada real. *(biblia 04 §7.1 TODO)*
- [ ] `Items`/`Crafting`: nuevo item `clavija_roca` (Punta≥2, sin mango) y verbo de
      colocación con el pico equipado como herramienta de golpeo, para ampliar la
      escalada de roca más allá de 3 m. *(biblia 02 §13.4 — director, 2026-09-27)*
- [ ] `Building`: piezas `escalera_mano` y `cuerda_fija` en `building_pieces.json`.
      *(biblia 02 §13.3)*
- [ ] Confirmar en pipeline de terreno editable en runtime el recorrido completo
      capa-de-ediciones → remallado → guardado → hoja subterránea del mapa, de extremo a
      extremo (GDD §7.3: `FTerrainDensity` + `FTerrainEdits` + `FSurfaceNets`, **no**
      `UDynamicMeshComponent` — el proyecto ya tiene su propio pipeline volumétrico
      procedural y no usa el componente genérico de Unreal). *(GDD §7.3, §7.4)*
      → **En parte:** `cb4e5a6` + `064b73a` (PR #40): capa de ediciones y guardado — faltan
        el remallado en runtime y la hoja subterránea; el recorrido no está cableado.
- [ ] `Tests`: extender `CarrySpec.cpp` con la carretilla; extender `Tools/DataCheck`
      para validar que toda plantilla nueva de crafteo es alcanzable con materiales de
      al menos una isla en AA. *(biblia 03 §Tests)*
      → **En parte:** `46382a9` (PR #50), `Tools/DataCheck/src/datacheck/checks.py:252` — el
        alcance de plantillas es global, no por isla de AA; falta el `CarrySpec` de la
        carretilla.
- [ ] Añadir a `achievements.json` los stats de minería: `terrain_edits_made`,
      `strata_mined`, `max_mining_depth_m`, `air_pocket_survived`,
      `cave_collapse_avoided`, `tools_broken_on_wrong_material`, `crab_stole_item`.
      *(biblia 07 §2.1)*
- [ ] Añadir a `achievements.json` los 6 logros de minería: `primera_palada`,
      `buscador_de_vetas`, `filo_de_obsidiana`, `topo_de_isla`, `el_aire_que_falta`,
      `viga_a_tiempo`. *(biblia 07 §2.3)*
- [ ] Añadir el logro `manazas` («Manazas») ligado a `tools_broken_on_wrong_material`.
      *(biblia 07 §2.3)*
- [x] `FBuildingModel::RecomputeStability` y el sistema de integridad/encaje de piezas
      ya implementados — base sobre la que se añaden `viga_apoyo` y `tablon_contencion`.
      *(verificado: `Source/Explored/Building/BuildingModel.{h,cpp}`)*
- [x] World Partition + capas HLOD ya configuradas por
      `WorldGenCommandlet::SetupWorldPartition`/`CreateHLODLayer` (terreno: celda única
      de 6,4 km; vegetación: HLOD por instancing a 1024 m/3000 m de rango) — pendiente
      de medir contra el objetivo de rendimiento de H0, no de implementar desde cero.
      *(verificado: `Source/ExploredEditor/WorldGenCommandlet.cpp:135-244`)*

### Red y cooperativo — terreno, construcción y arena (biblia 08)

El sistema de red más caro y el de más riesgo. Criterio de salida de red de H2: las
filas **3, 4 y 13** de la matriz de biblia 08 §7.3 pasan en «Normal».

- [ ] `WorldGen`: `FExploredTerrainDeltaPacket` — cabecera de 9 B (versión + chunk) y
      tramos de `uint16` inicio + `uint8` cuenta + `int16` por muestra en milímetros, con
      tope duro de 512 B por paquete. Spec de host: ida y vuelta sin pérdida, fusión de
      dos paquetes del mismo chunk idempotente y conmutativa, paquete truncado o
      manipulado rechazado sin tocar el estado. *(biblia 08 §2.2)*
      → **En parte:** `e6c89d1` (PR #46), `SandModel.h:250` — existe el paquete de arena
        (cabecera de 11 B); falta el volumétrico de 9 B para `FTerrainEditModel`.
- [ ] `WorldGen`: cola de salida por cliente con una entrada por chunk y fusión de
      muestras al reeditar, tope de 8 KB/s con ráfaga de 16 KB/s durante 5 s, prioridad
      para los chunks a menos de 30 m y relevancia limitada a 120 m del receptor.
      *(biblia 08 §2.2)*
- [ ] `WorldGen`: aplicar los deltas recibidos al `FTerrainEditModel` del cliente y
      remallar con `FTerrainChunkBuilder::Build` coalescido a 250 ms por chunk; el cliente
      nunca aplica su propia edición antes de recibirla del servidor. *(biblia 08 §2.2)*
- [ ] `WorldGen`: comprobación de integridad — `uint32` FNV-1a por chunk editado visible
      cada 30 s, y petición del chunk completo (mismo formato de tramos que
      `FTerrainEditModel::ToValue`) cuando no coincide o cuando se entra en un chunk nunca
      recibido. *(biblia 08 §2.2)*
      → **En parte:** `e6c89d1` (PR #46), `SandModel.h:269` (`ChunkChecksum` FNV-1a,
        `EncodeFullChunk`) — solo para arena; falta el volumétrico, el ciclo de 30 s y la
        petición de chunk.
- [ ] `Building`: colocación autoritativa (`Server_PlacePiece` que valida encaje, rejilla
      de 2 m, materiales en la copia del servidor y `RecomputeStability` antes de generar
      el actor); el fantasma de `UBuildPreviewComponent` queda puramente local;
      `CollapseUnsupported` y la degradación por clima solo en el servidor.
      *(biblia 08 §2.10)*
- [ ] `WorldGen`: arena viva simulada **solo en el servidor** — revisión limitada a chunks
      a menos de 80 m de algún jugador (con un máximo de 4 iteraciones acumuladas al
      acercarse), tope de 64 celdas movidas por segundo y por chunk, y salida por la cola
      de terreno con prioridad más baja que las ediciones del jugador. *(biblia 08 §2.6)*
      → **En parte:** `e6c89d1` (PR #46), `SandModel.h:38, 159, 175` (80 m, 64 columnas, 4
        revisiones) — presupuestos en el modelo; falta ejecutarlo en el servidor y sacarlo
        por la cola.
- [ ] Medir la compresión real de los deltas de terreno con `OodleNetwork` y entrenar el
      diccionario (`Tools/net-dictionary.ps1`, nuevo) sobre una captura de 10 min de
      minería; anotar el factor en `docs/tecnico/red.md`. Si no llega a ×2, bajar el tope
      de la cola en vez de tocar el diseño. *(biblia 08 §2.2, §3)*

---

## H3 — Mar y barcos

- [ ] `Boats`: piezas de casco (quilla, cuaderna, tablón, cubierta, mástil, vela,
      balancín, timón, banco de remo, amarre) en un nuevo `Content/Data/boat_pieces.json`.
      *(biblia 02 §8.1)*
      → **En parte:** `58e38da` (PR #53), `Boats/HullAssemblyModel.h:12` (`EHullPieceType`)
        — las piezas están en C++ y son de balsa; faltan `boat_pieces.json` y
        quilla/cuaderna/timón/banco/amarre.
- [ ] `Boats/BoatModel`: sustituir la tabla fija por `EBoatType` por el cálculo de
      `TotalMassKg`/`EquilibriumDraftCm`/`SwampWaterKg`/escora a partir de las piezas
      ancladas. *(biblia 02 §8.2)*
      → **En parte:** `58e38da` (PR #53), `HullAssemblyModel.h:250` (`ToBoatDefinition`) —
        `AExploredBoat` sigue con la tabla fija por `EBoatType`.
- [ ] `Boats`: integridad por unión cuaderna–tablón, daño por impacto sobre
      `SafeImpactSpeedCmS`, vía de agua por brecha (0,5 L/s). *(biblia 02 §8.3)*
      → **En parte:** `58e38da` (PR #53), `RaftYardModel.h:191, 231` — uniones de balsa, no
        cuaderna–tablón; la vía de agua no es 0,5 L/s por brecha.
- [ ] `Boats`: botadura sobre rodillos en tierra (8 s/tonelada) y deriva por corriente si
      no está amarrado en el agua. *(biblia 02 §8.4)*
      → **En parte:** `58e38da` (PR #53), `RaftYardModel.h:256, 269`, `BoatModel.cpp:634`
        (amarre) — falta la regla de 8 s/tonelada y enganchar astillero y amarre a un actor.
- [ ] `Boats`: actualizar `boats.json` para que los cuatro planos canónicos listen piezas
      en vez de un coste plano. *(biblia 02 §8.4)*
- [ ] Diseñar y cablear la fila de «Estado del barco» del HUD: nueva
      `AExploredHUD::DrawBoatStatus`, solo mientras se está a bordo y solo si hay algo
      urgente (vela mal trimada, casco <40 %, haciendo agua, capotado).
      *(biblia 06 §2.4)*
- [ ] Añadir a `achievements.json` el logro `primera_canoa` («Primera canoa»).
      *(biblia 07 §2.3)*
- [x] `FBoatModel` con `TotalMassKg`, `EquilibriumDraftCm`, `SwampWaterKg`,
      `MaxAbsRollDeg`, `ApplyDamage` ya implementado — base sobre la que se calculan las
      nuevas piezas de casco. *(verificado: referencia citada en biblia 02 §8, código en
      `Source/Explored/Boats/BoatModel.h`)*
- [ ] Pendiente de siempre (roadmap): malla del barco «Limón» y astillero final.
      *(GDD §3.10)*
- [ ] `Boats`: verificar que `barco_limon` exige las 4 `requiresShipParts`
      (`Fuselage`, `Wing`, `Tail`, `Engine`) y consume `canoa_balancin` como indica
      `boats.json` — solo falta el mesh `SM_Limon`. *(biblia 03 §3.8 TODO — [F3])*
      → **En parte:** `af649e9`, `boats.json:95-96`, `checks.py:569` — se exigen las 4
        partes, pero `consumesRequiredBoat` es `false` y falta `SM_Limon`.
- [ ] Confirmar en `Tools/DataCheck` que ninguna combinación de daño nuevo rompe el
      invariante «ninguna plantilla produce un objeto sin malla». *(biblia 05 §Tests)*
      → **En parte:** `22ec751`, `checks.py:611-631` (`check_meshes`) — el invariante se
        comprueba por objeto; falta cubrir las combinaciones de daño nuevas.

### Red y cooperativo — barcos (biblia 08)

- [ ] `Boats`: `FExploredBoatNetState` de 19 B a 20 Hz (posición cuantizada, velocidad,
      rumbo, escora, vela y trimado, agua embarcada, integridad) + `Server_SetBoatControls`
      de 4 B a 20 Hz desde el timonel; el cliente extrapola con el mismo
      `FBoatModel::Step` y corrige hacia el estado recibido en 200 ms.
      `NetCullDistanceSquared` 25 000 cm. Olas y corrientes **no se replican**.
      *(biblia 08 §2.5)*
- [ ] `Boats`: pasajeros con `AttachToActor` replicado, aforo por plano canónico (balsa 2,
      canoa 2, canoa con balancín 3, «Limón» 4 — al lleno el verbo «Subir» no se ofrece),
      timón cedible con el verbo de interacción sobre el asiento y liberado si el timonel
      se desconecta. Los 75 kg de `CrewMassKg` por tripulante cuentan de verdad en
      `TotalMassKg`. *(biblia 08 §5.4)*
- [ ] Pasar la fila 8 de la matriz de biblia 08 §7.3 (cuatro jugadores en la canoa con
      balancín, mar de ciclón, uno achicando) en «Normal» y «Mala»: escora y anegamiento
      idénticos en las cuatro pantallas y por debajo de 256 kbps. *(biblia 08 §7.3)*

---

## H4 — Contenido de acceso anticipado

Rellenar Landing, Esmeralda, Isla del Humo y Los Dientes (las 3–4 islas del acceso
anticipado, GDD §6.2) con el contenido que la biblia ya diseñó pero que no existe en
datos todavía.

- [ ] `Content/Data/`: crear `halden_diaries.json` (id, texto ES/EN, isla, POI asociado)
      con las 5 entradas de la biblia 04 §7.1; añadir su parseo a `RuinsSubsystem` o a
      un `HaldenLoreSubsystem` nuevo, con check en `Tools/DataCheck`. *(biblia 04 §7.1)*
- [ ] `Fauna/FaunaTypes.h`: añadir `EFaunaSpecies::WildBoar` y `EFaunaSpecies::WildGoat`
      y sus reglas de aparición (Esmeralda para el jabalí). *(biblia 04 §2.2)*
- [ ] `Content/Data/ruins.json`: asignar `Teaches` y `StarPathTarget` a los 8 `sites`
      según biblia 04 §2 (Landing→Esmeralda, Brújula del Humo→Los Dientes, y las
      técnicas globales del resto). *(biblia 04 §7.2)*
- [ ] `Content/Data/artifacts.json` o un nuevo `map_clues.json`: dar forma de dato a las
      6 pistas en prosa de biblia 04 §6. *(biblia 04 §6)*
- [ ] Landing: confirmar en `PointsOfInterest.cpp` que `SextantCave` solo es accesible
      con `FOceanTide::Level < 0` (bajamar). *(biblia 04 §2.1)*
- [ ] `Villages` (stub, sin bloquear H0-H4): dejar preparado el punto de extensión
      «reputación alta enseña una plantilla de herramienta de cobre» para cuando
      `Villages` exista en F3. *(biblia 03 §2.3)*
- [ ] `Fauna`: extender la primera pasada de fauna salvaje terrestre a cabra montés en
      La Meseta cuando esa isla entre en su parche de contenido. *(biblia 04 §2.7)*
- [ ] Añadir a `achievements.json` el logro `juego_de_anzuelos` («Juego de anzuelos»,
      reunir los tres anzuelos del pueblo navegante) y `bajo_el_templo` («Bajo el
      templo», tesoro en templo enterrado) junto a su stat `underground_treasure_found`
      y `artifact_ids_found`. *(biblia 07 §2.1, §2.3)*
- [ ] `Building`: añadir a `building_pieces.json` las piezas de museo nuevas:
      `pecera_museo`, `bandeja_conchas`, `marco_herbario`, `atril_cuaderno`,
      `vitrina_minerales`, `panel_fosiles` (categoría `museo`). *(biblia 07 §3.3)*
- [ ] Crear `Content/Data/shells.json`, `herbarium.json`, `insects.json` y
      `fossils.json` con las piezas listadas en biblia 07 §3.6, patrón bilingüe
      `nameEs`/`nameEn`. *(biblia 07 §3.6)*
- [ ] Añadir el subconjunto «tesoros» a `artifacts.json` como consulta derivada
      (`rarity` en `["raro", "unico"]`), sin duplicar el catálogo. *(biblia 07 §3.2)*
- [ ] Crear `Content/Data/journal_entries.json` (`id`, `trigger`, `textEs`, `textEn`,
      marcador `{Day}`) con las 15 entradas de biblia 07 §4.2. *(biblia 07 §4.1)*
- [ ] Arte: material del terreno con `Roughness` 0,85–0,95, sin especular en arena seca,
      arena mojada más oscura y algo más brillante solo en la banda de resaca, texturas
      de detalle con la paleta low poly; comprobar con capturas antes/después
      (`M_Terrain.uasset` ya existe, hoy se ve brillante). *(director, 2026-09-27)*
      → **En parte:** `650dfab` (PR #36), `Tools/Unreal/build_materials.py:254-256, 322-324`
        — arena, hierba y bosque a 0,88–0,92; falta la roca (0,78), el especular a 0 en
        arena seca y las capturas antes/después.
- [ ] Arte: vegetación, mobiliario y props no protagonistas seleccionados y retocados en
      materiales/color desde Kenney/KayKit/Quaternius para no romper la paleta por isla
      (GDD §7.1). *(GDD §7.1)*
      → **En parte:** PR #45, #48, #56 y #60 (packs CC0) y #42, #54, #63 (paleta),
        `packs_catalogo.json` — hay herramientas, comida, huerto y jabalí; falta vegetación
        general, mobiliario y props.

### Red y cooperativo — mapa compartido, guardado y sesiones (biblia 08)

- [ ] `Cartography`: mover `UCartographyComponent` del personaje a un
      `UExploredCartographySubsystem` del mundo, con el `FCartographyModel` autoritativo en
      el servidor; trazos replicados como polilíneas (`uint8` autor + `uint8` cuenta +
      puntos de 2 B). El mapa dibujado pasa a ser **uno por mundo**. *(biblia 08 §5.3, §2.11)*
- [ ] `Cartography`/`UI`: tono de tinta por jugador (anfitrión `#2B2016`, sepia `#3A2F1E`,
      azul `#243447`, granate `#3E2A2A`) y marca de ping dibujada sobre la hoja mientras
      dura. Los instrumentos (reloj, brújula, catalejo, sextante) siguen siendo objetos
      individuales de inventario. *(biblia 08 §5.3)*
- [ ] `Save`: las secciones `"inventory"` y `"body"` pasan a una por `SteamID64` bajo
      `"players"`, más una sección nueva `"coop"` (`WorldId`, aforo, último visto, tinta
      asignada, reanimaciones del día); una partida antigua de un jugador se lee como el
      jugador del anfitrión. *(biblia 08 §4.4)*
- [ ] `Save`: perfil de invitado en `Saved/SaveGames/Coop/<SteamID64>/<WorldId>.sav` con
      **solo** cuerpo, inventario, diario y estadísticas de logro (nunca estado del mundo),
      escrito en cada autoguardado del anfitrión y al salir limpiamente; al volver al mismo
      `WorldId` manda la copia del anfitrión, y en un mundo distinto se empieza con el kit
      de inicio. *(biblia 08 §4.4)*
- [ ] `Core`: unirse en caliente con el presupuesto de biblia 08 §4.3 (saludo, semilla y
      reloj, personaje, mundo a < 120/200 m, resto; objetivo por debajo de 20 s sin tirón
      para los que ya estaban), y los tres caminos de salida del anfitrión: cierre ordenado
      con 10 s de cuenta atrás y autoguardado, caída con 3 reintentos en 30 s, y expulsión
      con confirmación. Autoguardado nuevo al entrar o salir un jugador y cada 5 minutos
      con más de uno conectado. *(biblia 08 §4.3, §4.5)*
- [ ] `Achievements`: campo `coopScope` (`"actor"`/`"world"`/`"witness"`, radio de 50 m
      para el tercero) en los 54 logros de `achievements.json`, bandera compartida en el
      `GameState` para los logros de restricción, y escalado por número de jugadores de
      biblia 08 §5.6 como función pura en `ExploredLinks` con su spec de host.
      *(biblia 08 §5.6, §5.7)*

---

## H5 — Lanzamiento del acceso anticipado

Equilibrado, rendimiento objetivo, empaquetado, localización, salida a mercado.

- [ ] Añadir el campo `phase` (`"AA"`/`"F2"`/`"F3"`) a los 30 logros ya existentes en
      `achievements.json` y a los 24 nuevos, con los valores de biblia 07 §2.2–2.3.
      *(biblia 07 §2.1)*
- [ ] Añadir un campo `rarity` (`comun`/`infrecuente`/`raro`/`muy_raro`) a los 54 logros
      de `achievements.json` con los valores de biblia 07 §2. *(biblia 07 §2)*
- [ ] Añadir el logro `banquete_de_mil_cocos` y `el_cangrejo_se_lo_llevo`
      (absurdos/graciosos, §2.3 «humor», máximo 3 de 54) con su stat
      `coconuts_opened`/`crab_stole_item`. *(biblia 07 §2.3)*
- [ ] Añadir el logro candidato `museo_completo` a una revisión posterior de
      `achievements.json` cuando el total lo permita sin salir del rango 40–60, o como
      contenido post-lanzamiento. *(biblia 07 §3.4)*
- [ ] Añadir las 66 entradas del glosario ES/EN de biblia 07 §5.2 a
      `docs/tecnico/localizacion.md` o a un glosario propio referenciado desde ahí.
      *(biblia 07 §5.2)*
- [ ] Actualizar `Tools/Localization/src/l10n/` para comprobar los modificadores de
      plural ICU (`{Count}|plural(...)`), hoy no verificados por el chequeo de
      marcadores. *(biblia 07 §5.3)*
- [ ] Ejecutar `cd Tools/Localization && uv run l10n --strict` sobre cada fichero de
      datos nuevo de esta lista en cuanto exista, antes de darlo por escrito definitivo.
      *(biblia 07 §5.1)*
- [ ] Pasar cada logro, ficha de museo y entrada de diario por el checklist anti-IA de
      biblia 07 §1.5 una segunda vez en revisión de contenido. *(biblia 07 §1.5)*
- [ ] Rendimiento: verificar 60 fps a 1080p con menos de 6,5 GB de VRAM en Landing
      (objetivo de H0) y en el resto de islas del acceso anticipado, sin tirones al
      cargar/descargar celdas de World Partition; medir el tiempo de frame con el modo
      bench ya existente (`-ExploredBench`, `Tools/bench.ps1`). *(director, 2026-09-27;
      infraestructura de medida ya existe, el objetivo en sí no está verificado)*
- [ ] Audio: pipeline de música y efectos con soundfont acústico (sin sintetizador
      genérico), coherente con el director de música adaptativa y la flauta ya
      implementados (`docs/roadmap.md`, P-MUSIC) — hoy no hay soundfont en
      `Tools/Audio`. *(GDD §7.2, transversal)*
- [ ] Empaquetado Win64 reproducible (`Tools/build.ps1` ya existe genérico; falta el
      paso de empaquetado final con configuración Shipping y verificación de tamaño de
      build). *(GDD §7.2, `docs/roadmap.md` P-M9)*
- [ ] Página de Steam: capturas, descripción, tráiler corto — sin ninguna de las tres
      preparada hoy. *(`docs/roadmap.md` P-M9)*
- [ ] Tráiler de lanzamiento del acceso anticipado (30–60 s, sin cinemáticas
      pregrabadas del propio juego: montaje de capturas en PIE, coherente con «sin
      cinemáticas» del pilar 5). *(GDD §7.2)*
- [ ] Beta cerrada: reclutar y correr al menos una ronda antes de abrir el acceso
      anticipado, con foco en el pipeline de minería (mayor riesgo técnico, GDD §8) y en
      el rendimiento de H0–H3. *(GDD §8, transversal)*
- [ ] QA de cierre: pasar `Tools/HostTests/run.sh` (specs de host) y
      `Tools/test.ps1` (Automation Tests del editor) en verde antes de empaquetar.
      *(CLAUDE.md del proyecto, «Tests»)*
- [ ] Red: pasar las 16 filas de la matriz de biblia 08 §7.3 en «Limpia», «Normal» y
      «Mala», y las 16 en «Horrible» sin corrupción de mundo ni desconexión silenciosa.
      *(biblia 08 §7.3)*
- [ ] Red: verificar con el CSV de `Explored.NetBudget` el objetivo de **menos de 64 kbps
      por cliente en reposo** y **menos de 256 kbps en pico** con 4 jugadores, sobre las 16
      filas de la matriz. Criterio de salida, no estimación. *(biblia 08 §3)*
- [ ] Red: ajustar con datos de la beta cerrada las cifras de biblia 08 §5 (×120 al dormir,
      90 s y 6/3 s de `Derribado`, y el escalado de vetas, fauna y asaltos por número de
      jugadores). Están escritas con número justo para poder moverlas de una en una.
      *(biblia 08 §5.1, §5.2, §5.6)*
- [ ] Red: sustituir `SteamDevAppId=480` por el AppId real de Steam y verificar invitación,
      entrada en caliente y relay desde dos redes domésticas distintas antes de abrir el
      acceso anticipado. *(biblia 08 §1.1, §4.2)*
- [x] Modo bench (`-ExploredBench`, `ExploredShotSubsystem::bBenchMode`) y script
      `Tools/bench.ps1` ya existen para medir tiempo de frame — falta el objetivo
      verificado, no la herramienta. *(verificado:
      `Source/Explored/Debug/ExploredShotSubsystem.h:28-64`, `Tools/bench.ps1`)*

---

## F2

Raíles y vagones, animales domésticos, murallas y defensas (piezas, sin IA de asalto),
resto de islas (Manglar, Arenas Blancas, Meseta completa).

- [ ] `Fauna`: extensión a especies terrestres domésticas (gallina, cerdo, cabra) con
      packs CC0 riggeados (Quaternius). *(biblia 02 §10.2)*
- [ ] `Building`: piezas de corral (gallinero, pocilga, corral genérico) con tope de 8
      animales vivos por base. *(biblia 02 §10.2)*
- [ ] `Fauna`: reproducción por pares (15 %/día), cría a adulto en 6 días, regla de
      vuelta a salvaje tras 2 días sin comida. *(biblia 02 §10.2)*
- [ ] Nuevo módulo `Tramway`: pieza de vía (socket `via`), grafo de tramos, vagón sobre
      spline con colisión contra terreno editable. *(biblia 02 §9, 03 §3.9)*
      → **En parte:** `9787b2b` (PR #57): `FTramwayModel`, `Tramway/TramwayModel.h:162, 221`
        — modelo puro sobre rejilla; falta el actor del vagón sobre spline, la pieza con
        socket `via` y la colisión real.
- [ ] `Building`: torno horizontal y ascensor de pozo como piezas de producción.
      *(biblia 02 §9)*
      → **En parte:** `9787b2b` (PR #57), `TramwayModel.h:209` (`AddWinch`); ascensor solo
        en el borrador `fases_futuras.json` — ninguno es pieza construible.
- [ ] Prototipo de PIE del sistema de raíles antes de comprometer alcance (coste real no
      verificado). *(GDD §3.5, biblia 02 §9 TODO)*
- [ ] `Building`: añadir `rail_recto`, `rail_curvo`, `cambio_agujas`, `vagon`,
      `torno_cuerda`, `ascensor_pozo`, `muralla_piedra`, `torre_defensa`,
      `cerca_estacas`, `empalizada`, `torre_vigia`, `gallinero`, `pocilga`, `corral` a
      `building_pieces.json` — verificado: hoy no existe ningún id de muralla ni de
      raíl en el fichero. *(biblia 03 §3.7, §3.9, 05 §4.1, 07 §2.1 — verificado:
      `Content/Data/building_pieces.json` sin coincidencias de `muralla`/`empalizada`/
      `torre_defensa`)*
- [ ] `Building`: trampas de defensa (estacas ocultas) como pieza colocable con verbo
      «Rearmar». *(biblia 05 §4.2)*
- [ ] Tres piezas de armadura nuevas (coraza de cuero, peto de placas) en
      `items.json`/`templates.json`, más el trofeo único «Piel de tiburón curtida» de
      «El Errante». *(biblia 05 §3.4)*
- [ ] `Source/Explored/WorldGen/`: extender `FormationPlacementModel`/`TerrainDensity`
      con el carving de cenote y río subterráneo navegable para La Meseta.
      *(biblia 04 §2.7 TODO)*
- [ ] Nuevo módulo `Villages`: ubicar la sede en La Meseta y el campamento estacional en
      Arenas Blancas; sección de guardado `reputation`. *(biblia 04 §2.6–2.7, GDD §3.9)*
- [ ] `Content/Data/`: dar de alta en `items.json`/`templates.json` los recursos
      exclusivos de Manglar (arcilla, junco), Arenas Blancas (conchas raras, velas de
      lona) y La Meseta (caliza, cultivos) si aún no existen. *(biblia 04 §2.5–2.7)*
      → **En parte:** `22ec751`, `items.json:90, 106, 119-122` (arcilla roja, caliza,
        conchas) — faltan junco, velas de lona y conchas raras.
- [ ] Añadir a `achievements.json` los stats `rail_track_and_cart_used`,
      `livestock_species_raised`, `eggs_collected`, y los logros
      `primer_tren_de_isla`, `primera_empalizada`, `muralla_de_piedra`,
      `primera_pareja`, `corral_completo`, `huevos_por_docenas`. *(biblia 07 §2.1, §2.3)*
- [ ] Confirmar en `Tools/DataCheck` que ninguna combinación de daño de armadura nueva
      rompe el invariante «ninguna plantilla produce un objeto sin malla».
      *(biblia 05 §Tests)*
- [ ] Red: `Tramway` autoritativo en el servidor — vagón sobre spline simulado solo en el
      servidor y replicado como estado (posición sobre el tramo + velocidad, 6 B a 10 Hz);
      un tramo cuyo terreno se reedita por debajo se marca «dañado» y no navegable hasta
      repararlo, igual que una pieza de construcción. *(biblia 08 §1.3, biblia 02 §9)*
      → **En parte:** `9787b2b` (PR #57), `TramwayModel.h:221`
        (`DamageInSphere`/`IsDamaged`/`Repair`) — falta la autoridad de servidor y replicar
        el vagón.
- [x] Estratos de caliza (La Meseta) ya diseñados en la tabla de materiales de biblia 02
      §2.3, pendientes solo de que la propia isla entre en F2 — no de una mecánica
      nueva. *(biblia 02 §2.3, GDD §4)*
      → **Revisión 2026-09-28:** `mining.json:21, 68` (`46382a9`, PR #50) ya lista la caliza
        por isla.

---

## F3

Pueblo del arrecife (comercio y reputación), conflicto pirata completo (patrullas y
asaltos), isla oculta y final.

- [ ] Importar un esqueleto humanoide compatible con Mixamo (pack CC0/licencia
      permisiva, tipo KayKit Adventurers) y verificar el retargeting con el IK
      Retargeter de UE5. *(biblia 05 §0)*
- [ ] Retargetear 10 animaciones de Mixamo (ralentí, caminar, trabajar, saludar,
      ofrecer, huir, ataque cuerpo a cuerpo, disparo de arco, reacción a impacto,
      caída) sobre el esqueleto elegido. *(biblia 05 §0)*
- [ ] Modelar/ajustar la cara low-poly sin rig facial y validar legibilidad de silueta a
      20 m. *(biblia 05 §0)*
- [ ] Crear dos paletas de material (navegantes: paño/tierra; piratas: cuero
      oscuro/metal) sobre la misma malla base. *(biblia 05 §0)*
- [ ] Nuevo módulo `Villages`: spawn de la aldea (Arenas Blancas, 10 NPC) y el puesto de
      trueque (La Meseta, 4 NPC). *(biblia 05 §1.1)*
- [ ] Sección `reputation` en `Save` (por asentamiento, 0–100, sin decaimiento pasivo).
      *(biblia 05 §1.5)*
- [ ] Prop nuevo «tablón de peticiones» (malla + rotación de icono cada 4 días)
      reutilizando `story_es.json.petroglyph_themes`. *(biblia 05 §1.3)*
- [ ] Lógica de trueque: valor 1–5 por objeto × tasa de reputación, ventana horaria
      8:00–18:00. *(biblia 05 §1.4)*
- [ ] Enganchar «devolver objeto ritual» a `Ruins` (+5 reputación, sin trueque de por
      medio). *(biblia 05 §1.5)*
- [ ] Wayfinding enseñado por el guardián del marae con reputación ≥70, sin duplicar
      entre Arenas Blancas y La Meseta. *(biblia 05 §1.6)*
- [ ] Aldeanos invulnerables al daño de arma (solo huida + penalización de reputación).
      *(biblia 05 §1.5)*
- [ ] Enfriamiento de 15 días de juego cuando la reputación cae por debajo de 20.
      *(biblia 05 §1.5)*
- [ ] Especificar e implementar la pantalla de trueque (§2.11 de biblia 06): prompt de
      contexto «Ofrecer {objeto}», sin menú de tienda ni barra de reputación en pantalla.
      *(biblia 06 §2.11)*
- [ ] Nuevo módulo `Raiders`: percepción reutilizando `Fauna`, patrulla por semilla entre
      los dos campamentos, 4 tipos de pirata con sus daños y vidas. *(biblia 05 §2.1–2.2)*
- [ ] Contador `Amenaza pirata` (0–100) en `Save`, con las reglas de subida/bajada de
      biblia 05 §2.3. *(biblia 05 §2.3)*
- [ ] Programador de asaltos: categoría según Amenaza, condición de recursos
      visibles/reputación Hostil, aviso previo (humo + tambor). *(biblia 05 §2.3)*
- [ ] Generar por semilla los dos campamentos fijos (Cala Rota en Los Dientes,
      Fondeadero Podrido en el Manglar) con cofre de botín y barco propio.
      *(biblia 05 §2.5)*
- [ ] Dos plantillas de barco pirata (Piragua de asalto, Balandra negra) sobre el mismo
      `FBoatModel` del resto de embarcaciones. *(biblia 05 §2.5)*
- [ ] Botín: skin «Machete pirata» y accesorio cosmético único «Capa de vigía» al
      derrotar a un Capitán. *(biblia 05 §2.6)*
- [ ] Daño a estructuras por tipo de atacante, incluida la propagación «ardiendo» sobre
      piezas inflamables y su apagado con agua/arena. *(biblia 05 §4.3)*
- [ ] Nuevo módulo `Raiders`: escondite pirata en el islote secundario de Los Dientes;
      rutas de patrulla cerca de Arenas Blancas y La Meseta. *(biblia 04 §2.4 TODO)*
- [x] `Ruins/RuinsModel`: confirmar que `HiddenIslandIndex` exige `RequiredStarPaths = 3`
      de los 4 caminos disponibles, no los 4 completos. *(biblia 04 §2.8 TODO)*
      → **Hecho:** `af649e9` (anterior a #41): `Ruins/RuinsModel.h:89` (`RequiredStarPaths =
        3`), `RuinsModel.h:202`, `RuinsModel.cpp:311-313`.
- [ ] `Ruins`/`Artifacts`: cablear el examen de `figura_navegante`, `figura_gemelos`,
      `figura_mira_cielo`, `carta_varillas`, `carta_oleaje`, `tapa_estrellas` a las
      técnicas de wayfinding (hoy `artifacts.json` no tiene ese vínculo por artefacto
      individual). *(biblia 03 §3.10 TODO)*
- [ ] Arte: el pueblo del arrecife y el campamento pirata necesitan asset propio (marae
      «vivo» con estructuras ligeras); no reutilizar directamente las piezas de ruina
      (deben leerse como abandonadas). *(biblia 04 §2.8 TODO, GDD §7.1)*
- [ ] Añadir a `achievements.json` los stats `barter_trades_completed`,
      `reputation_village_tier`, `wayfinding_taught_by_village`,
      `village_defended_from_raid`, `raids_defended`, `raider_camps_defeated`, y los
      logros `primer_trueque`, `aliado_de_facto`, `otra_forma_de_aprender`,
      `sin_disparar_una_flecha`, `asalto_repelido`, `campamento_tomado`.
      *(biblia 07 §2.1, §2.3)*
- [ ] Red: `Raiders` y `Villages` autoritativos en el servidor — percepción, patrulla y
      asalto solo en el servidor (reutilizan la fauna terrestre replicada de biblia 08
      §2.7, mismo tope y mismo LOD), con el escalado por número de jugadores de biblia 08
      §5.6: asaltantes `×(1 + 0,4·(N−1))` sobre la base de 5 (7/9/11 con 2/3/4 jugadores) y
      categoría **+1** por cada 2 jugadores por encima de 1; la frecuencia no escala.
      *(biblia 08 §5.6, §1.3)*
- [ ] Red: logros de restricción en cooperativo (`sin_disparar_una_flecha` y similares) con
      la bandera compartida del `GameState`: si cualquier jugador rompe la restricción, se
      apaga para todos en esa partida. *(biblia 08 §5.7)*

---

## Fuentes

`docs/diseno/gdd_v2.md`, `docs/diseno/biblia/{01..07}-*.md` (sus respectivos «TODO de
implementación»), `docs/roadmap.md` (estado de compilación/implementación citado como
contexto), y grep directo contra
`C:\Users\Rodrigo\PERSONAL\ProyectosPersonales\Explored\Explored` (main) para cada
`[x]`/verificación de ausencia citada arriba.
