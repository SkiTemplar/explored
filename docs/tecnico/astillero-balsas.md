# Astillero de balsas: cómo enganchar `FRaftYardModel` en el motor

Estado: el modelo es puro y tiene spec en el host (`Explored.RaftYard`, 24 casos), además
de los casos nuevos de `Explored.Boat` (amarre y ficha propia) y de
`Explored.SaveSystems` (amarre guardado). Falta la integración con Unreal, que tiene que
hacer una sesión con el editor. Diseño de juego: GDD v2 §3.17.

## Qué hay

- `Source/Explored/Boats/RaftYardModel.h`: `FRaftYardModel` lleva dentro un
  `FHullAssemblyModel` (piezas y cargas; GDD §3.14) y añade:
  - **Uniones** entre piezas (`FRaftJoint`: cordel, cuerda o clavos, con salud 0–1).
  - **Camino de botadura** (`FLaunchPath`), que es una línea en planta con tramos de
    suelo y pendiente.
  - **Rodillos**, que son posiciones a lo largo del camino.
  - Estado `Ashore` o `Afloat`.
- `FBoatModel` (ya en main) gana:
  - `FBoatModel(const FBoatDefinition&, …)` y `SetDefinition`: navega con la ficha de
    un casco armado. Este es el paso 3 de `casco-por-piezas.md`.
  - `Moor` y `CastOff`: el amarre es una restricción de cabo inextensible que se aplica
    dentro del paso fijo. El barco sigue cabeceando y balanceándose.
  - En `FBoatState`, el registro de golpes (`ImpactCount`, `LastImpactSpeedCmS`,
    `LastImpactDirection`) y el roce varado acumulado (`GroundScrapeWorkNm`).
  - `FBoatSaveData` guarda el amarre, y `SaveBoat` solo lo escribe si existe, así que
    las partidas antiguas no cambian.

## Enganche previsto

1. **Actor del astillero (`ARaftYardActor`, `Building`).** Contiene un
   `FRaftYardModel`. Cada pieza es un `UStaticMeshComponent` hijo: hay pocas, de 6 a 30,
   y cada una necesita su propia transformación para soltarse. Por eso no usa HISM.
   - Las uniones se dibujan como un único `UInstancedStaticMeshComponent` con la malla
     de atadura o de clavo, una instancia por unión. El «deshilachado» es un
     `PerInstanceCustomData` con la salud.
   - Los rodillos son otro ISM con la malla del tronco de la tala.
2. **Construcción.**
   - Al soltar una pieza, `AddPiece`. Al atarla con la herramienta, `AddJoint`: si
     devuelve `INDEX_NONE`, la pieza está lejos o ya estaba atada.
   - La vista previa de flotación sale de `GetHydrostatics()`, que se cachea y solo se
     recalcula al cambiar piezas o cargas.
3. **Botadura desde tierra.**
   - Al pulsar «botar» (o al primer empujón), se genera el `FLaunchPath` con una traza
     desde el centro del casco hacia el agua más cercana: un tramo cada 1 m.
     - El suelo del tramo sale del material físico del terreno (`PhysMat_Sand`,
       `PhysMat_Rock`…) o de la malla de rampa (`PlankRamp`).
     - `WaterLevelZCm` es el mar medio más `FBoatModel::TideOffsetCm`.
   - Cada fotograma se llama a `Push(Σ empujes, DeltaSeconds)`: cada jugador que empuja
     aporta `PushForcePerPersonN`. El actor se coloca en `Path.WorldAt(GetCenterS())`
     y los rodillos, en sus S.
   - `Report.Damage.Released`: se crea un actor físico por pieza suelta (tronco o
     tablón que cae) con la misma malla.
   - `Report.bLaunched`: `MakeBoat()` y se hace el relevo a `AExploredBoat`.
4. **`AExploredBoat` con ficha propia.** Hace falta un `InitFromYard(FRaftYardModel&&)`
   que guarde el astillero en el barco y cree el modelo con `MakeBoat()`. En cada `Tick`,
   después de `Model.Step`:
   - Si `ImpactCount` ha cambiado: `Yard.ApplyImpact(LastImpactSpeedCmS, LastImpactDirection)`.
   - Si `GroundScrapeWorkNm` ha crecido: `Yard.ApplyScrapeWork(Δ, suelo bajo el casco)`.
   - Si algo se ha soltado: `Model.SetDefinition(Yard.ToBoatDefinition())` y un actor
     por pieza suelta que flota a la deriva. Un `FBoatModel` de tipo balsa con la ficha
     de un tronco sirve.
   - Se sincroniza el daño: `Model.Repair(1); Model.ApplyDamage(Yard.HullDamage01())`.
   - Amarre: la interacción «amarrar» junto a un poste o a un muelle (`Building`) llama
     a `Model.Moor(PosteXY, LargoCabo)`, con un cabo de 3 m por defecto. El cabo se
     dibuja con `UCableComponent`.
5. **Construida en el agua.** Igual que el paso 4, pero desde la primera pieza se llama
   a `SetAfloat()` y hay un `FBoatModel` vivo. Cada `AddPiece` o `AddJoint` va seguido
   de `SetDefinition`.

## Red (biblia 08 §2.5)

Autoridad en el servidor, como el resto de `FBoatModel`. El cliente no decide nada: solo
extrapola con el mismo código.

- **Barco a flote.** `FExploredBoatNetState` (19 B a 20 Hz) necesita crecer para que el
  cliente extrapole bien con amarre:
  - `bMoored` va en un bit de las banderas que ya hay en el byte de vela + trimado.
  - Poste y largo solo cambian al amarrar o soltar, así que no van en el estado de 20 Hz.
    Van en una propiedad fiable aparte, `FExploredMooringNetState`: poste en
    `FVector2D` cuantizado a 1 cm (4 B) y largo en `uint16` en cm (2 B). Son 6 B por
    cambio, con coste de ancho de banda sostenido nulo.
  - La ficha propia (`FBoatDefinition` de un casco por piezas) tampoco va en el estado.
    El cliente la reconstruye con `FHullAssemblyModel::ToBoatDefinition` a partir de las
    piezas, que ya se replican como piezas de construcción (biblia 08, construcción).
    Cuando se suelta una pieza, se replica la retirada de esa pieza (fiable, una vez) y
    cada cliente llama a `SetDefinition`.
  - La integridad del casco (`uint8`, ya en el estado) es `HullDamage01` del astillero.
    El byte reservado para «piezas dañadas» puede llevar el índice de la última unión que
    ha cambiado, para el efecto visual.
- **Balsa en tierra (astillero):**
  - Empuje: `Server_PushRaft(uint8 Fuerza)`, sin fiabilidad, a 10 Hz mientras se mantiene
    pulsado. El servidor suma los empujes de todos los jugadores que están a menos de
    2 m del casco y acota cada uno a `PushForcePerPersonN`. Un cliente no puede empujar
    más fuerte ni desde lejos.
  - Estado replicado: `CenterS` (`uint16` en cm) y `VelocityCmS` (`int16`) a 10 Hz
    mientras se mueve, y nada cuando está quieta: 4 B × 10 Hz = 0,3 kbps con una balsa
    en movimiento. El camino (`FLaunchPath`) se replica una vez, de forma fiable, al
    empezar la botadura.
  - Rodillos: `TArray<uint16>` fiable, solo cuando se pone o se recoge uno. Mientras la
    balsa rueda, el cliente los mueve con la misma regla (la mitad del avance), sin
    tráfico.
  - Uniones: la salud (`uint8` por unión) se replica solo cuando cambia, junto a la pieza
    de construcción. Las roturas y las piezas sueltas son eventos fiables, y la pieza
    suelta nace como objeto soltado de Chaos (biblia 08 §2.5).
  - Reparar: `Server_RepairJoint(int32)`. El servidor comprueba la distancia y el
    inventario y gasta `ItemsPerRepair`.
- **Aforo.** La balsa admite 2 tripulantes (biblia 08 §5.4). Eso es cosa de
  `AExploredBoat` (adjuntar pasajeros, tarea 16 de H3), no de estos modelos:
  `FBoatModel` solo sabe si hay tripulante (`bCrewAboard`). Cuando se añadan pasajeros,
  cada uno entra como `FHullLoad` de 75 kg en el astillero.

## Persistencia

- **Barco a flote:** `FBoatSaveData` (con amarre) en la sección de barcos, como hoy.
  Además hay que guardar la lista de piezas (tipo, centro, tamaño) y la de uniones
  (A, B, tipo, salud). Es el mismo pendiente que en `casco-por-piezas.md` §5, y ahora
  incluye las uniones. `FromSaveData(Data, &Yard.ToBoatDefinition())` reconstruye el
  barco.
- **Balsa en tierra:** piezas, uniones, rodillos, `CenterS` y el `FLaunchPath` en una
  capa opaca `"raftyard"`, en la sección `world` junto a `WorldDeltas`. Es la misma idea
  que las capas `"terrain"` y `"sand"`: no son índices de scatter, así que no van en
  `FSaveScatterDeltas`. El camino se puede regenerar con la traza, pero se guarda para
  que la balsa no salte si el terreno ha cambiado.
- **Troncos consumidos como rodillos:** se retiran del inventario al ponerlos y vuelven
  al recogerlos. Los que se queden en el camino se guardan en la capa del astillero.

## Coste por fotograma

- `Push` y `Substep` son O(piezas + rodillos) por subpaso a 60 Hz: unas 30 × 60
  operaciones por segundo y balsa. Es despreciable.
- `GetHydrostatics()` es lo caro (`FHullAssemblyModel::Evaluate`, del orden de 10⁶
  recortes con 20 piezas). Solo se recalcula al cambiar piezas o cargas; al empujar se
  usa la caché.
- `ApplyImpact` y `ApplyScrapeWork` son O(uniones) y solo se llaman cuando hay golpe o
  roce. `ReleaseLoosePieces` es una unión-búsqueda O(piezas + uniones), que solo se
  ejecuta cuando se rompe una unión.
- Solo se simula la balsa que se está empujando. Las balsas en tierra que nadie toca
  no hacen `Tick`.
- El amarre suma un producto escalar por subpaso de `FBoatModel`.

## Pendiente y riesgos

- El objeto `clavo` no existe en `items.json` (lo tiene que decidir la rutina de datos).
- El camino recto no rodea obstáculos: si la traza choca con algo, se corta ahí y la
  balsa se detiene al final (`bAtPathEnd`).
- El surco en la arena viva ya está en `FRaftFurrowModel`; cómo engancharlo, más abajo
  en «Surco en la arena».

## Surco en la arena

`FRaftFurrowModel` (`Source/Explored/Boats/RaftFurrowModel.h`, spec `Explored.RaftFurrow`)
enlaza el astillero con la arena viva. Reglas y números: GDD v2 §3.17, «Surco en la arena».

- **Dónde se llama.** En el servidor, en el mismo sitio que `FRaftYardModel::Push` (el
  componente del astillero de `AExploredBoat` o del actor de la balsa en tierra):

  ```cpp
  const float SBefore = Yard.GetCenterS();
  const FRaftPushReport Push = Yard.Push(ForceN, DeltaSeconds);
  if (!Push.bOnRollers && Push.MovedCm != 0.0f)
  {
      const FRaftFurrowResult Furrow = FRaftFurrowModel::Drag(Yard, SBefore, Yard.GetCenterS(),
          SandEnv.HighTide, SandEnv.bRaining, SandSubsystem->Model(), SandSubsystem->BaseHeight());
      SandSubsystem->QueueChanged(Furrow.Sand);   // lo mismo que tras Dig o Pile
  }
  ```

  `HighTide` y `bRaining` son los del `FSandEnvironment` del fotograma. `Base` es la misma
  función de altura base que usa el subsistema de arena.
- **Actores y HISM.** No hay ninguno nuevo. El surco y los cordones son deltas de la capa
  de arena: los remalla el `UDynamicMeshComponent` de cada chunk de `Furrow.Sand.DirtyChunks`,
  como una pasada de pala (`docs/tecnico/arena-viva.md`). Los rodillos siguen siendo los
  troncos de la tala.
- **Red.** `Furrow.Sand.ChangedColumns` sale por la cola de terreno con
  `FSandModel::EncodePackets`, igual que la pala. Un arrastre de 1 m con la balsa de 6
  troncos cambia unas 4 filas × 7 columnas por metro (surco y cordones), unas 28 columnas:
  un paquete por chunk.
- **Persistencia.** Nada nuevo: los deltas van en la capa `"sand"` de `WorldDeltas`, como
  los de la pala. La balsa en tierra ya guarda `CenterS` y el camino en la capa
  `"raftyard"`.
- **Coste por fotograma.** Solo con la balsa arrastrándose sin rodillos. Muestrea el tramo
  del fotograma cada media celda (12,5 cm): a 1 m/s son 1 o 2 muestras por fotograma, cada
  una con unas 15 × 4 columnas por tronco del fondo. En el host, con el `TMap` lineal del
  shim, el test de empuje (unos 300 fotogramas de `Push` más `Drag`) tarda unos 25 ms en
  total: menos de 0,1 ms por fotograma. Las columnas ya rasadas no mueven nada. Hay que
  medirlo en PIE con el `TMap` de Unreal.
  `MaxStations` (512) acota un salto de tiempo enorme.
- **Por verificar en PIE.**
  - El `CharacterMovement` de quien empuja pisa el cordón (hasta 3 × 60 mm con mucha
    carga): no debe engancharse.
  - La balsa no se hunde visualmente en el surco: el camino es una traza del terreno
    anterior. Si se nota, bajar el casco `DrySinkMm` en el actor mientras se arrastra.
  - El camino recto puede pasar junto a unos tablones de contención: el cordón se va
    entero al otro lado (test «echa el cordón al otro lado si un costado está bajo un
    muelle»).
