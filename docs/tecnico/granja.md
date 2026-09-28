# Granja: cómo enganchar `FLivestockModel` en el motor [F2]

Estado: el modelo es puro y tiene spec en el host (`Explored.Fauna.Livestock`, 32 casos).
Falta la integración con Unreal, que tiene que hacer una sesión con el editor. El diseño
de juego sale del GDD v2 §3.6 y la biblia 02 §10.2 y §11.5, y la red de la biblia 08
§2.7 b y §5.5. Es de fase 2 y no compromete nada del acceso anticipado.

## Qué hay

- `Source/Explored/Fauna/LivestockModel.h`: un `FLivestockModel` es la granja de una base.
  Guarda sus corrales (`FLivestockPen`: tipo, comida del comedero, huevos y leche por
  recoger) y sus animales (`FLivestockAnimal`: especie, sexo, corral, edad, si es adulto,
  si está domado y las rachas de días comiendo y sin comer).
- Especies y piezas: `gallina`, `cerdo` y `cabra` (`SpeciesId`) viven en `gallinero`,
  `pocilga` o `corral` (`PenPieceId`). El gallinero solo admite gallinas, la pocilga solo
  cerdos y el corral genérico admite cualquiera de las tres.
- Topes: 8 animales por corral y 8 vivos por base. Con un solo corral los dos coinciden.
  `IsPenFull` es lo que se pinta («lleno se ve lleno», sin contador).
- Día a día: `EndDay(Día)` hace crecer a las crías, da de comer por orden de id, doma o
  devuelve a salvaje, recoge huevos y leche, y tira la cría de cada pareja (15 %). Si
  faltan días los cierra todos, hasta 60 de golpe. Un día ya cerrado no hace nada.
- Acciones: `AddAnimal` (capturado llega sin domar, trueque llega domado), `AddFeed`,
  `Collect`, `OpenGate` (se escapan los que no están domados), `MoveAnimal`,
  `RemoveAnimal` y `RemovePen` (solo vacío).
- Guardado: `ToValue()` y `FromValue()`. `FromValue` rechaza datos rotos (ids repetidos,
  corral inexistente, especie en un corral que no le toca, más del tope, crías con edad
  de adulto…) y deja la granja vacía.
- Determinismo: las tiradas son un hash de semilla, corral, día y pareja. Si faltan huecos
  para todas las crías del día, entran por un orden que también sale del hash, así que
  no gana siempre el corral con id menor.

## Enganche propuesto

1. **Subsistema `ULivestockSubsystem`** (de mundo, solo en el servidor). Tiene un
   `FLivestockModel` por base, creado con la semilla del mundo. Escucha el cierre de día
   de `UTimeOfDaySubsystem` y llama a `EndDay` igual que `UFarmSubsystem` con el huerto.
2. **Piezas.** Al colocar `gallinero`, `pocilga` o `corral` (`building_pieces.json`,
   `phase: "F2"`), `UBuildingSubsystem` llama a `AddPen(ParsePenPiece(id))` y guarda el id
   del corral en la pieza. Al retirarla llama a `RemovePen`. Si devuelve `PenNotEmpty`, se
   rechaza el derribo con el prompt de contexto. El menú de construcción aún no filtra por
   `phase`: hay que añadirlo antes de que estas piezas lleguen a una build del acceso
   anticipado.
3. **Comedero.** Echar comida es el verbo del prompt con fruta o tubérculo en la mano
   (`feedItems` de `fases_futuras.json`). Una unidad por objeto. Lo que no cabe
   (`OutAccepted`) se queda en la mano.
4. **Actores.** Cada animal es un actor de fauna terrestre con malla de Quaternius. El
   estado visual sale de `MakeAnimalNet` (cría o adulto, domado o arisco, comido o no).
   Cuando nace una cría (`FLivestockDayReport::Births`) aparece junto a la madre.
   `OpenGate` devuelve los ids que huyen: sus actores pasan a fauna salvaje o desaparecen
   al salir del corral.
5. **Estadísticas** (`docs/tecnico/estadisticas.md`, biblia 07 §2.1): por cada cría de
   `Births` se llama a `ReportStatItem("livestock_species_raised", SpeciesId(Species))`, y
   al recoger se llama a `ReportStat("eggs_collected", Collect.Eggs)` si hubo huevos. Así
   se desbloquean `primera_pareja`, `corral_completo` y `huevos_por_docenas`.
6. **Guardado.** Una capa `"livestock"` en el guardado del mundo con `ToValue()` por base.
   Si falla al cargar, la granja se queda vacía y se avisa en el registro, igual que la
   vía de los vagones.

## Red (biblia 08)

- **Autoridad.** El modelo vive solo en el servidor (biblia 08 §5.5: corrales y animales
  son del grupo). Echar comida, recoger, meter un animal o abrir la puerta son RPC al
  servidor. El servidor valida el inventario de quien lo pide (§2.4) y llama al modelo.
  El cliente no predice nada de la granja.
- **Animales.** Son fauna terrestre replicada (§2.7 b): actores normales simulados en el
  servidor con los 14 B de estado. `FLivestockAnimalNet` rellena el `uint8` de especie y
  el `uint8` de banderas (macho, adulto, domado, comido hoy). Cuentan dentro del tope duro
  de 12 animales a 10 Hz y 24 a 2 Hz por cliente, y el tope de 8 por base ayuda a
  respetarlo.
- **Corral lejano.** A más de 60 m (`AggregateNetDistanceCm`) basta con
  `FLivestockPenNet`: 7 B con adultos y crías por especie, comida, huevos, leche y si está
  lleno. Es la propuesta de `fases_futuras.json` para no gastar el tope de fauna en
  animales que nadie ve de cerca. Se manda solo cuando cambia (al cerrar el día o tras un
  RPC), sin frecuencia fija.
- **Escalado** (§5.6). La cría y el crecimiento van con el calendario y no escalan con el
  número de jugadores.
- **Logros** (§5.7). `primera_pareja` y `corral_completo` llevan `coopScope: "world"`: la
  cría es un hecho del corral del grupo y se informa a todos los conectados.
  `huevos_por_docenas` lleva `"actor"`: `eggs_collected` solo sube para quien recoge.

## Qué verificar en el editor

- `Explored.Achievements.Data` acepta ahora entre 30 y 60 logros: los 30 del acceso
  anticipado más los tres de granja con `phase: "F2"`.
- `Explored.Building.Datos` sigue parseando `building_pieces.json` con las tres piezas de
  granja (`mesh: null`, en `meshes_pendientes.json`) y el campo nuevo `phase`, que el
  parser ignora.
