# Incendio de vegetación: integración en el motor

Nota para la sesión local con Unreal. El modelo puro ya está hecho y probado en
`Tools/HostTests` (`Explored.Wildfire`, 22 casos); falta conectarlo a la capa de Unreal.
Las reglas y los números están en el GDD v2 §3.15 y en la biblia 02 §6.

| Modelo | Fichero | Qué decide |
|---|---|---|
| `FWildfireModel` | `WorldGen/WildfireModel.h` | Qué celdas de 2 m arden, se queman, se mojan o rebrotan; ceniza; qué chunks cambian |

## Dónde se engancha

1. **Subsistema de servidor.** Se crea un `UExploredWildfireSubsystem`
   (`UWorldSubsystem`) que solo existe con autoridad y guarda un `FWildfireModel`.
   - La semilla es la del mundo, pasada por `Hash32` con una sal propia.
   - `FFuelQuery` pregunta a la capa de vegetación si en esa celda hay hierba
     (`SM_GrassClump*` sin recoger) o matorral (`Shrub` en pie, no talado). No pregunta
     al terreno: lo que no es vegetación no arde.
   - La consulta tiene que ser barata, porque se llama hasta 8 veces por celda
     ardiendo. Conviene una rejilla de bits por chunk horneada con la vegetación
     (1 bit de hierba y 1 de matorral por celda de 2 m, 64 B por chunk). Recolectar o
     talar pone el bit a 0.
2. **Reloj.**
   - `Advance(NowSecond, NowMinute, Conditions, Observers)` se llama **una vez por
     segundo** con un temporizador del subsistema. No se llama en `Tick`.
   - `NowSecond` es un contador entero de segundos de simulación del servidor, que se
     guarda con la partida.
   - `NowMinute` es el minuto de juego de `UTimeOfDaySubsystem`.
   - `Conditions` sale de `FWeatherModel`: estación, estado, `Wind` y la dirección del
     viento del cielo.
   - `Observers` son las posiciones XY de todos los `PlayerController` con pawn.
3. **Quién prende.**
   - Cada **5 s**, `AExploredFire` en estado `Burning` y sin techo llama a `Ignite` en
     las celdas a menos de **1 m** de su borde. Una hoguera sin piedras alrededor
     prende la hierba seca que tiene al lado.
   - También prenden la antorcha soltada en el suelo, la brea del pirata
     `Incendiario` (biblia 05 §4.3) y el rayo (`FWeatherSample::Lightning`: 1 celda
     al azar con hierba a menos de 80 m de un jugador).
4. **Quién apaga.**
   - Un cubo de agua o una palada de arena llaman a
     `Douse(Center, 1, NowMinute, 120)`, que apaga y moja durante 2 h de juego.
   - La lluvia no necesita llamada: con un chubasco o más, `Advance` lo apaga todo.
5. **Efectos (solo cosmético, en cada cliente).**
   - Por cada chunk en `DirtyChunks`, el servidor replica un `FExploredWildfireChunkState`
     con 2 bits por celda (sin quemar, ardiendo o quemada), 64 B por chunk. Se envía
     por la cola de terreno de biblia 08 §2.2 con prioridad baja.
   - El cliente dibuja las llamas con un `UNiagaraComponent` por chunk que arde. Recibe
     las celdas como parámetro de array y **nunca crea un actor por celda**.
   - El suelo quemado es una decal por chunk o un canal del material del terreno.
   - La hierba quemada se oculta poniendo a escala 0 las instancias HISM de la celda,
     igual que un tocón en `tala-integracion.md`.
6. **Daño.**
   - Un jugador dentro de una celda que arde recibe la quemadura de contacto de biblia
     01 §6 (8 puntos de salud y herida de quemadura, como mucho una vez por segundo).
   - Una pieza de construcción de madera, bambú o palma en una celda que arde recibe
     «ardiendo» 4/s (biblia 05 §4.3). El modelo no conoce las piezas: las busca el
     subsistema.
7. **Ceniza.** Se interactúa con el suelo quemado: `TakeAsh(Cell, NowMinute)` da 1
   `ceniza_madera` al inventario.

## Persistencia (WorldDeltas)

- La sección nueva `wildfire` de la capa `world` guarda `FWildfireModel::Save()`.
  Contiene `version`, `lastSecond` y la lista `cells`, en la que cada celda ocupa 8
  enteros.
- `Load` rechaza entero cualquier formato raro: celda repetida, fuego sin combustible,
  estado o combustible fuera de rango, o coordenadas fuera de `int32`. Si lo rechaza,
  no toca el estado.
- También rechaza minutos fuera de ±`MaxAbsMinute` (10¹²) y un `lastSecond` por encima
  de `MaxAbsSecond`. Como `Ignite`, `Douse` y `Advance` ignoran esos relojes sin
  guardarlos, todo lo que guarda el juego se vuelve a cargar (hay spec). Si el reloj del
  subsistema pudiera dar un valor absurdo, el fuego no se congela: ese avance se descarta
  y el siguiente segundo bueno sigue.
- Las partidas antiguas no tienen la sección y empiezan sin incendio.
- Al guardar se llama antes a `Prune(NowMinute)` para no arrastrar celdas ya rebrotadas.
  Tras un incendio de 1 ha (2 500 celdas) la sección ocupa unos 60 kB de texto hasta
  que rebrota. Si molesta, se puede comprimir por tramos como los deltas de terreno.
- **Pendiente de decidir:** la biblia quiere que el matorral quemado y el talado
  compartan temporizador. Hoy son dos relojes, `vegetationClock` para la tala y
  `wildfire` para el fuego, con números distintos (GDD v2 §3.15).

## Coste por frame

- **Nada por frame.** Hay un `Advance` por segundo en el servidor, con coste
  proporcional a las celdas que arden en chunks a menos de 80 m de un jugador: 8
  consultas de combustible y 8 hashes por celda. Un frente de 200 celdas son 1 600
  tiradas, muy por debajo de 0,1 ms. Las celdas quemadas o sin combustible no cuestan
  nada. El test `CellsVisited` demuestra que un fuego lejano no se revisa.
- **Memoria:** unos 40 B por celda guardada en el `TMap`; 1 ha quemada ocupa unos 100 kB.
- **Red:** como mucho 64 celdas que prenden por chunk y segundo. Con 64 B de estado por
  chunk sucio, un frente que cruza 4 chunks cuesta unos 256 B/s (2 kbps) mientras arde.
  Es el mismo orden que la arena viva de 08 §2.6 y va en la misma cola, con prioridad
  baja.
- **Cliente:** 1 sistema de Niagara por chunk que arde. Con 4 chunks ardiendo en pantalla
  hay 4 emisores con un presupuesto de unas 2 000 partículas en total: la llama es
  low poly, no volumétrica.
