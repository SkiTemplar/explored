# Tests del host (sin Unreal)

Compila los **modelos puros** de `Source/Explored` (los que solo incluyen `CoreMinimal.h`)
contra un shim mínimo de Core y ejecuta:

- los **Automation Specs** de UE que solo usan esos modelos (`pure_specs.txt`): el mismo
  fichero `*Spec.cpp` corre en el editor y aquí, así que no hay tests duplicados;
- tests propios del host en `tests/*.cpp` (macro `HOST_TEST`, ver `HostTest.h`).

```bash
Tools/HostTests/run.sh                      # todo (~20 s la primera vez, ~2 s incremental)
Tools/HostTests/run.sh Explored.Survival    # filtro por subcadena del nombre
HOST_TESTS_SANITIZE=ON Tools/HostTests/run.sh   # con AddressSanitizer + UBSan
HOST_TESTS_FASTMATH=ON Tools/HostTests/run.sh   # con matemáticas rápidas, como el editor
```

Requisitos: `cmake` ≥ 3.20 y un compilador C++20 (g++ 13 o clang 18). Funciona en la nube
(Linux) y en Windows con WSL.

## Qué es y qué no es

- `shim/` reproduce **solo** la parte de Core que usan los modelos, con la semántica de
  Unreal donde importa: `Num()` en `int32`, `INDEX_NONE`, `check` que falla (aquí lanza),
  `TMap` en orden de inserción, `FVector`/`FVector2D` en `double`, `FVector3f` en `float`,
  `FLinearColor(FColor)` con la curva sRGB exacta, `FName` sin distinguir mayúsculas.
- **No es Unreal.** Si algo pasa aquí y falla en el editor, manda el editor (y conviene
  arreglar el shim). Lo que incluya `UObject` (`UCLASS`, `USTRUCT`, `*.generated.h`),
  mundo, actores o Slate no se compila aquí.
- `FMath::IsFinite`/`IsNaN` miran los bits, como en Unreal, para que sigan funcionando con
  matemáticas rápidas.
- **Matemáticas rápidas.** El editor compila con `/fp:fast`: el compilador da por hecho que no
  hay NaN, y `!(x > 0)` o `x == x` dejan de descartarlos. `HOST_TESTS_FASTMATH=ON` compila con
  `-ffast-math` para cazar esos sitios aquí; la regla es comprobar siempre con
  `FMath::IsFinite` las entradas reales que vengan de fuera (guardados, red, `DeltaSeconds`).
- `ParallelFor` es secuencial (el determinismo no debe depender del orden de los hilos).

## Añadir código

1. Un modelo nuevo debe incluir solo `CoreMinimal.h` (y otros modelos puros): la lógica
   de juego va en `F<Algo>Model` puro y el `UActorComponent`/subsistema solo lo conecta.
2. Añade su `.cpp` a `pure_sources.txt` y su `*Spec.cpp` a `pure_specs.txt`.
3. Si falta algo de Core en el shim, añádelo con la semántica de Unreal (documenta la
   referencia si no es obvia).

Ya ha encontrado un fallo real: `WorldGenSpec` tomaba un puntero al interior de un
`FArchipelagoLayout` temporal (uso tras liberar; en el editor pasaba por suerte).

## Diagnóstico del terreno

`tools/TerrainDiagnostics.cpp` es un programa aparte (no entra en la suite): muestrea el
archipiélago, imprime las métricas de realismo de `FTerrainSurvey` (bultos sueltos, cota
dominante y picos del histograma del fondo, saltos, varianza fina, orientación de cauces,
pozos), las de jugabilidad de `FTerrainPlayabilitySurvey` (llanos construibles con pendiente
< 15°, acantilados de costa, variación de la plataforma, motas del mar con su índice de
Clark-Evans, patrón de drenaje y lagunas interiores) y escribe en PPM el relieve sombreado, la
pendiente y el mapa de zonas construibles (verde < 15°, rojo > 60°). Se compila con el mismo
include path que la suite y los `.cpp` de `pure_sources.txt`; uso:

```bash
terrain_diag /tmp/terreno 20260926 6        # mundo entero, paso de 6 m
terrain_diag /tmp/mesa 20260926 2 2         # zoom a la isla 2 del layout, paso de 2 m
```

Las imágenes y volcados no se suben (ver `.gitignore`); las métricas que importan ya son
asserts en `TerrainRealismSpec`.
