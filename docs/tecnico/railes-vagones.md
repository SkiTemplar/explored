# Raíles y vagones: cómo enganchar `FTramwayModel` en el motor [F2]

Estado: el modelo es puro y tiene spec en el host (`Explored.Tramway`, 27 casos). Falta
la integración con Unreal, que tiene que hacer una sesión con el editor. Diseño de
juego y números: GDD v2 §3.5. La mecánica es de fase 2, así que este documento no
compromete nada del acceso anticipado.

## Qué hay

- `Source/Explored/Tramway/TramwayModel.h`: la vía es un grafo sobre una rejilla de
  2 m × 2 m × 0,125 m. Solo se guarda por nodo la subida de cada una de sus 4 salidas,
  los tramos dañados, la palanca del cambio y si tiene torno. `Place` y `CanPlace`
  devuelven por qué no encaja un tramo (`NotAdjacent`, `TooSteep`, `Occupied`…), y
  `NodeKind` dice qué malla poner (fin, recta, curva o cambio).
- `FMineCart` es un dato plano: tramo dirigido `From → To`, posición `S`, velocidad `V`
  con signo, carga y causa de descarrilo. Cada vagón tiene una parte delantera. Con
  `FCartControl` se empuja hacia delante o hacia atrás, se tira con el torno o se echa
  el freno.
- `Step(Cart, Control, Dt, Accumulator)` integra en pasos fijos de 1/120 s. Da lo mismo
  con cualquier troceo de `Dt`, así que el resultado en el servidor no depende de su
  tasa de fotogramas (los clientes no simulan, ver el punto 6).
- `DamageInSphere(Centro, Radio)` marca dañados los tramos que pasan a menos del radio y
  devuelve los recién dañados. `Repair` los arregla.
- Guardado: `ToValue()` y `FromValue()` para la vía, y `CartToValue()` y
  `CartFromValue()` para cada vagón. `FromValue` rechaza datos rotos y deja la vía vacía.

## Enganche propuesto

1. **Subsistema de mundo `UTramwaySubsystem`.** Es dueño de un `FTramwayModel` y de
   los vagones (`TArray<FMineCart>` más el actor de cada uno). En `Tick` llama a `Step`
   para cada vagón con el control que le haya dado el jugador que interactúa (máximo
   tres verbos: empujar, enganchar el torno y frenar). El actor del vagón se coloca con
   `CartPosition` y se orienta con el tramo actual.
2. **Colocación.** Se reutiliza la vista previa del kit de construcción
   (`BuildPreviewComponent`). Se ajusta con `SnapNode` al nodo más cercano del punto
   trazado contra el terreno y se valida con `CanPlace`. La vista previa se pinta en
   rojo según el `ERailPlaceResult`. El coste se cobra según `NodeKind` después de
   colocar: una curva o un cambio sustituye a la pieza recta de ese nodo.
3. **Mallas de vía.** Cada tramo es un `USplineMeshComponent` de 2 m entre
   `NodePosition(A)` y `NodePosition(B)`. Las curvas y los cambios son mallas propias
   en el nodo. Si hay muchas, conviene agruparlas por chunk de 64 m en un
   `UInstancedStaticMeshComponent` y reconstruir solo el chunk afectado.
4. **Terreno editado bajo la vía.** Justo después de cada `Pickaxe`, `Shovel`,
   `PlaceSoil` o `CarveStairs` de `FTerrainEditModel` que cambie algo
   (`Result.Changed()`), `UTerrainEditSubsystem` llama a
   `DamageInSphere(centro del pincel, radio del pincel + 0,3 m)`. El margen de 0,3 m es
   el balasto: cavar justo al lado de la vía también la descalza. Para la escalera se
   usa la caja de `CarveStairs`: basta con una esfera que la envuelva. Los tramos que
   devuelve cambian a la malla de vía rota. El remallado del terreno sigue siendo el de
   `docs/tecnico/terreno-editable.md` (`UDynamicMeshComponent` en lugar del Nanite
   horneado del chunk editado); la vía no necesita remallar terreno.
5. **Guardado.** La vía se guarda en una capa opaca `"tramway"` de `FSaveWorldDeltas`,
   junto a `"terrain"`. Hay que añadir un campo `FSaveValue Tramway` como el que ya
   existe para el terreno, y guardar los vagones como lista de `CartToValue` en la misma
   capa. Si falla al cargar, la vía se queda vacía y se avisa en el registro, igual que
   el terreno.
6. **Red (biblia 08 §1.2 y §1.3; `00-TODO.md`, F2).** El servidor es la autoridad y el
   único que simula los vagones: el cliente **no** predice (biblia 08 §1.2, «nada más
   se predice»). Se replica el estado, la posición sobre el tramo y la velocidad, a
   10 Hz (unos 6 B cuantizados; el tramo `From → To` solo cuando cambia), y el cliente
   interpola. Que `Step` dé lo mismo con cualquier `Dt` sirve para que el servidor no
   dependa de su tasa de fotogramas, no para predecir en el cliente.

## Límites conocidos del modelo

- Los vagones no chocan entre sí. Si en PIE hace falta, basta con comprobar la distancia
  entre vagones del mismo tramo y del siguiente.
- La cuerda del torno busca el camino más corto por la vía sin mirar las palancas. Si
  hay un cambio entre el torno y el vagón y la palanca apunta a otra rama, el vagón se
  va por esa rama y el torno lo vuelve a traer. En la interfaz basta con avisar de que
  la palanca no lleva al torno.
- Las curvas de 90° se recorren como dos rectas de 2 m (la longitud del camino no cambia
  y el radio solo cuenta para volcar). Si en PIE el vagón «pega un salto» visual en la
  esquina, se corrige en la presentación interpolando por la spline de la malla de
  curva, sin tocar el modelo.
- No hay ascensor de pozo (biblia 02 §9). Es un torno vertical y se añadirá como un
  tramo vertical especial cuando exista su malla.
