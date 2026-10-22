# CLAUDE.md — Explored

## Qué es

**Baliza Siete**: supervivencia corta en primera persona (45-75 min) en una sola isla. Iria
Valdés naufraga en la Isla de Sal y tiene que volver a encender su faro (lente del carguero
varado, aceite de pescado y mecha) antes de que el correo «Mistral» pase la noche del día 3.
Sin combate ni muerte; la única penalización es el desmayo. Diseño en `docs/v3/gdd.md`, plan
por fases en `docs/v3/plan.md` y estado en `docs/v3/estado-*.md` y `docs/v3/pendiente-motor.md`.
Rama de trabajo: `reset/v2`.

## Stack

- Unreal Engine 5.6, C++20 (`Explored.uproject`, `EngineAssociation` 5.6). Un solo mapa:
  `/Game/Maps/Map_IslaSal`. Sin Lumen (`r.DynamicGlobalIlluminationMethod=0`), Nanite y VSM, DX12
  con SM6. Interfaz en Slate; entrada con Enhanced Input creada por código.
- Módulos: `Source/Explored` (juego; carpetas Audio, Boats, Carry, Cooking, Core, Crafting,
  Debug, Events, Fauna, Fishing, Interaction, Items, Narrative, Ocean, Player, Save, Sky, Survival,
  UI, Weather, WorldGen y Tests) y `Source/ExploredEditor` (editor: solo el commandlet `-run=ExploredStyle`, validador del GDD §11).
  Dependencias en `Explored.Build.cs`: Core, CoreUObject, Engine, InputCore, EnhancedInput,
  ProceduralMeshComponent, PhysicsCore; privadas RenderCore, RHI, Slate, SlateCore, UMG, Json.
- Datos de juego en `Content/Data/*.json` (isla, decorado, props, objetos, recetas, verbos,
  cuaderno, objetivos, pensamientos, curva de tormenta, música…), leídos del disco con
  `FFileHelper` y copiados sueltos al paquete (`DirectoriesToAlwaysStageAsNonUFS`). Fuentes e
  iconos de Slate en `Content/Slate`; localización ES/EN en `Content/Localization`.
- Arte en `/Game/Art` (kits K1-K5, `Art/Island`, `Art/Materials`; el césped irá en
  `/Game/Art/K6_Cesped`), audio en `/Game/Audio` y `/Game/Generated/Audio`. Todo lo que el código
  carga por ruta se cocina por directorio (`Config/DefaultGame.ini`).
- `Tools/`: scripts PowerShell (`build`, `test`, `package`, `bench`, `playtest_v3`, `shots`,
  `build_content`, `unreal_python`; rutas comunes en `common.ps1`, motor en `UE_ROOT`),
  `Tools/Unreal` (Python del editor), `Tools/HostTests` (tests sin Unreal) y herramientas Python
  con uv: `Island`, `Textures`, `Audio`, `Blender`, `Packs`, `DataCheck`, `Localization`, `Style`.

## Cómo compilar / ejecutar

```powershell
powershell -NoProfile -File Tools/build.ps1                                        # ExploredEditor Development
powershell -NoProfile -File Tools/build.ps1 -Target Game -Configuration Shipping   # Explored Shipping
powershell -NoProfile -File Tools/package.ps1 [-Configuration Development]         # paquete Win64 en Saved/Package/Windows
powershell -NoProfile -File Tools/bench.ps1                                        # rendimiento a 1080p (≥ 60 media, ≥ 45 mínimo)
powershell -NoProfile -File Tools/playtest_v3.ps1 [-Route competente]              # partida completa por bot
```

`build.ps1` solo usa UnrealBuildTool; `package.ps1`, `bench.ps1`, `playtest_v3.ps1`,
`shots.ps1`, `test.ps1` y `build_content.ps1` lanzan el editor o el juego. Si otro proceso tiene
el mutex de UBT, `-WaitMutex` espera. Controles y requisitos en `README.md`.

## Tests

- **Sin Unreal (nube, CI):** `Tools/HostTests/run.sh` compila los modelos puros de
  `Source/Explored` contra un shim de Core y ejecuta los Automation Specs que solo usan
  modelos (`pure_specs.txt`) más los tests de `Tools/HostTests/tests`. Con
  `HOST_TESTS_SANITIZE=ON` añade ASan/UBSan. Ver `Tools/HostTests/README.md`.
- **Con Unreal (local):** `Tools/test.ps1` ejecuta todos los Automation Tests del editor.
- Herramientas Python (`Tools/Packs`, `Textures`, `Audio`, `DataCheck`, `Localization`, `Island`,
  `Blender`, `Style`): cada una con su `pyproject.toml` y `uv.lock`. Desde su carpeta:
  `uv run ruff check .`, `uv run basedpyright` y `uv run pytest -q --cov` (mínimo 80 %).
  `Tools/Blender` y los tests de Blender de `Tools/Packs` (`--group blender`) usan
  `bpy==5.2.2` de PyPI con Python 3.13, y el render Workbench necesita
  `libegl1 libegl-mesa0 libgl1-mesa-dri`. CI (`host-tests.yml`) lo ejecuta todo.
- Regla: la lógica de juego nueva va en modelos puros (`F<Algo>Model`, solo `CoreMinimal.h`)
  con su `*Spec.cpp`, para que se pueda validar sin el editor.
