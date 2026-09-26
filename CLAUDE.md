# CLAUDE.md — Explored

## Que es

Carpeta vacia con .gitignore generico y README minimo; sin generador ni dependencias externas.

## Stack

Plantilla: vacio (local). Dependencias: sin dependencias declaradas.

## Como compilar / ejecutar

Ver README.md de este proyecto.

## Tests

- **Sin Unreal (nube, CI):** `Tools/HostTests/run.sh` compila los modelos puros de
  `Source/Explored` contra un shim de Core y ejecuta los Automation Specs que solo usan
  modelos (`pure_specs.txt`) más los tests de `Tools/HostTests/tests`. Con
  `HOST_TESTS_SANITIZE=ON` añade ASan/UBSan. Ver `Tools/HostTests/README.md`.
- **Con Unreal (local):** `Tools/test.ps1` ejecuta todos los Automation Tests del editor.
- Herramientas de contenido: `Tools/Textures` y `Tools/Audio` tienen su propio `pytest`.
- Regla: la lógica de juego nueva va en modelos puros (`F<Algo>Model`, solo `CoreMinimal.h`)
  con su `*Spec.cpp`, para que se pueda validar sin el editor.
