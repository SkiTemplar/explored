# Casco por piezas: cómo engancharlo en el motor

Modelo: `FHullAssemblyModel` (`Source/Explored/Boats/HullAssemblyModel.h`), puro (solo
`CoreMinimal.h`). Spec: `Explored.HullAssembly` (host y editor). Diseño: GDD v2 §3.14.

## Qué hace y qué no

- **Hace:** hidrostática estática exacta para piezas en caja. Calcula calado,
  francobordo, centro de masas y de carena, KB, KG, KM, GM transversal y longitudinal,
  escora y asiento de equilibrio, ángulo de estabilidad nula y un veredicto (flota,
  escora, anegada, vuelca, se hunde). Además da propulsión y maniobra: empuje, velocidad
  sostenida, giro y estabilidad de rumbo.
- **No hace:** dinámica en el mar. Olas, viento aparente, balance, vuelco en marcha y
  varadas son de `FBoatModel`. El puente entre los dos es
  `FHullAssemblyModel::ToBoatDefinition`, que devuelve un `FBoatDefinition`.

## Enganche previsto (otra sesión con Unreal)

1. **Astillero (`Building`).** Mientras el jugador coloca piezas de casco en el
   astillero, un `UHullAssemblyComponent` guarda un `FHullAssemblyModel` y lo reevalúa
   en cada colocación. `Evaluate()` cuesta del orden de 10⁶ recortes de rectángulo con
   20 piezas y va de sobra para hacerlo al soltar una pieza, pero no en cada fotograma
   del fantasma: mientras se arrastra, se evalúa como mucho cada 0,2 s.
2. **Vista previa sin HUD.** El actor del casco en construcción se coloca con
   `DraftCm`, `HeelDeg` y `TrimDeg`: se ve hundido y escorado donde quedaría. Si el
   veredicto es `Capsizes`, se reproduce un balanceo que se pasa de 55° al botarlo.
3. **Botadura.** `AExploredBoat` necesita un constructor de `FBoatModel` a partir de un
   `FBoatDefinition` arbitrario (hoy solo acepta `EBoatType`). Es un cambio pequeño en
   `FBoatModel`: guardar la definición en el estado en vez de consultar la tabla
   estática.
4. **Carga y pasajeros en marcha.** Al subir o bajar alguien o mover carga, se
   actualizan las cargas del modelo, se reevalúa y se pasa a `FBoatModel` la nueva GM
   y el ángulo de vuelco. Si el nuevo veredicto es `Capsizes` o `Sinks`, `FBoatModel`
   lo aplica con su condición `Capsized` o `Wrecked`.
5. **Guardado.** Hay que añadir la lista de piezas (tipo, centro y tamaño) a
   `FBoatSaveData`. Las cargas no se guardan: se reconstruyen desde el inventario del
   barco.

## Convenciones

- Marco del casco en cm: X hacia proa, Y hacia estribor, Z hacia arriba. La quilla es
  la Z mínima de las piezas que flotan.
- Escora positiva: estribor abajo. Asiento positivo: proa abajo.
- El mástil flota (tiene volumen), pero no cuenta como cubierta para el francobordo.
