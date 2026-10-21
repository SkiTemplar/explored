# Baliza Siete

Juego de supervivencia corta en primera persona hecho con Unreal Engine 5.6 y C++ (proyecto
`Explored`). Una sola isla, una partida de 45-75 minutos, sin combate ni muerte.

Iria Valdés, técnica de una empresa que apaga faros, naufraga en la Isla de Sal, cuya baliza
era la siguiente de su lista. Para salir de allí tiene que volver a encenderla antes de que el
correo «Mistral» pase de largo en la noche del tercer día: pescar y hervir aceite, trenzar una
mecha y cruzar en balsa hasta un carguero varado para recuperar la lente. Sed, hambre, energía
y calor marcan el ritmo; la única penalización es el desmayo. El diseño completo está en
[`docs/v3/gdd.md`](docs/v3/gdd.md) y el plan de fases en [`docs/v3/plan.md`](docs/v3/plan.md).

## Controles

Teclado y ratón. Las teclas marcadas con «*» se pueden cambiar en Ajustes → Controles
(`SettingsLogic::GetRemappableActions`); el resto son fijas.

| Acción | Tecla |
|---|---|
| Moverse | W A S D |
| Mirar | Ratón |
| Saltar * | Espacio |
| Correr * | Mayús izquierda |
| Agacharse; bajo el agua, bucear * | Ctrl izquierda (mantener o alternar, según Ajustes) |
| Interactuar (coger, beber, encender, dormir, lanzar y sacar el sedal en un punto de pesca…) * | E |
| Elegir otro verbo del objeto enfocado (p. ej. la hoguera) * | C |
| Comer o beber lo de la ranura rápida elegida * | Clic izquierdo |
| Usar la mano izquierda * | Clic derecho |
| Soltar lo que se lleva en brazos o la última pila suelta de la mochila * | G |
| Mochila (inventario, con cursor) * | Tab |
| Ranuras rápidas | 1, 2, 3 |
| Cuaderno del farero | J |
| Pausa | Esc |
| En la balsa: remar / girar / bajar | W / A, D / E |

El mando tiene asignaciones para el personaje (`AExploredCharacter::RebuildKeyMappings`), pero
queda fuera del alcance de la primera entrega (GDD §9: «Mando, inglés: posterior») y la balsa solo responde al
teclado.

## Requisitos

- Windows 10/11 de 64 bits y una GPU con DirectX 12 y Shader Model 6 (el proyecto solo genera
  SM6, `Config/DefaultEngine.ini`).
- Unreal Engine 5.6 del Epic Games Launcher en `C:\Program Files\Epic Games\UE_5.6` (o la ruta
  que indique la variable de entorno `UE_ROOT`).
- Visual Studio 2022 con la carga de trabajo «Desarrollo de juegos con C++» (MSVC y Windows SDK).
- Git LFS: los `.uasset`, `.umap`, `.png`, `.wav`, `.ogg` y `.ttf` van por LFS (`git lfs pull`).
- Para las herramientas: [uv](https://docs.astral.sh/uv/) (Python), CMake ≥ 3.20 y clang o g++
  con C++20 (tests del host) y Blender 5.2 solo para regenerar arte.

## Compilar

```powershell
powershell -NoProfile -File Tools/build.ps1                                          # editor (ExploredEditor, Development)
powershell -NoProfile -File Tools/build.ps1 -Target Game -Configuration Shipping     # juego (Explored, Shipping)
```

`Tools/build.ps1` llama a `Build.bat` (UnrealBuildTool) con `-WaitMutex` y solo muestra errores,
avisos y el resultado. Parámetros: `-Target Editor|Game` y `-Configuration
Development|DebugGame|Shipping`. Después se abre `Explored.uproject`; arranca en
`/Game/Maps/Map_IslaSal`, el único mapa del juego.

## Probar

- **Sin Unreal** (también en CI, `.github/workflows/host-tests.yml`): compila los modelos puros
  de `Source/Explored` contra un shim de Core y ejecuta sus Automation Specs.

  ```bash
  bash Tools/HostTests/run.sh                          # todos
  bash Tools/HostTests/run.sh Explored.Debug           # filtro por nombre
  HOST_TESTS_SANITIZE=ON bash Tools/HostTests/run.sh   # con ASan y UBSan
  ```

  En Windows con Ninja de Visual Studio:
  `CMAKE_GENERATOR=Ninja CC=clang CXX=clang++ bash Tools/HostTests/run.sh`.
- **Con Unreal:** `powershell -NoProfile -File Tools/test.ps1 [-Filter Explored]` ejecuta todos
  los Automation Tests en el editor sin interfaz.
- **Datos:** `cd Tools/DataCheck && uv run datacheck --strict` valida `Content/Data/*.json`;
  `cd Tools/Localization && uv run l10n --strict` comprueba los textos en español e inglés.
- **Partida por bot:** `Tools/playtest_v3.ps1 [-Route competente|lenta|sin_paginas]` juega una
  partida completa en el editor y deja el informe en `Saved/Shots/`.
- **Rendimiento:** `Tools/bench.ps1` recorre Cala Varada, el Palmar, el faro y el ciclón a 1080p
  y falla si no llega a ≥ 60 fps de media y ≥ 45 de mínimo (detalle en
  [`docs/v3/estado-f7.md`](docs/v3/estado-f7.md)).

## Empaquetar

```powershell
powershell -NoProfile -File Tools/package.ps1                              # Shipping
powershell -NoProfile -File Tools/package.ps1 -Configuration Development   # con herramientas de prueba
```

Valida los datos, compila el editor y el juego, cocina con `BuildCookRun` y deja el juego en
`Saved/Package/Windows/Explored.exe`; al final comprueba que el paquete trae el ejecutable, los
`.pak`/`.utoc` y los JSON de `Content/Data` (que el juego lee del disco y se copian sueltos). Qué
se cocina está en `Config/DefaultGame.ini`, sección `ProjectPackagingSettings`.

## Estructura

- `Source/Explored`: módulo de juego (supervivencia, isla, baliza, balsa, pesca, interfaz Slate,
  guardado). La lógica va en modelos puros `F<Algo>Model` con su `Tests/*Spec.cpp`.
- `Source/ExploredEditor`: módulo de editor, hoy sin código propio (solo el `IMPLEMENT_MODULE`).
- `Content/Data`: datos del juego en JSON (isla, objetos, recetas, cuaderno, objetivos, música…).
- `Tools/`: scripts de compilación, pruebas y paquete, y herramientas Python de contenido
  (isla, texturas, audio, arte en Blender, localización, comprobación de datos).
- `docs/v3`: diseño, plan y estado de cada fase.
