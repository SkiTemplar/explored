# M0–M1: Cimientos y Archipiélago — Plan de implementación

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development
> (recommended) or superpowers:executing-plans to implement this plan task-by-task.
> Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Proyecto UE 5.6 C++ que compila por CLI, con pipelines de contenido por código
(Blender, audio, importación) y un archipiélago volumétrico procedural recorrible con
océano, cielo y ciclo día/noche.

**Architecture:** Módulo C++ `Explored` (runtime) + `ExploredEditor` (herramientas).
El terreno es un campo de distancia con signo (SDF) evaluado por chunks y poligonizado
con Surface Nets (malla suave, admite cuevas y voladizos), generado en hilos de trabajo y
horneado en el editor como mallas estáticas Nanite dentro de World Partition (LOD, streaming y campos de distancia para Lumen nativos). Los assets salen de
scripts headless de Blender y Python y se importan con Python del editor.

**Tech Stack:** UE 5.6 (C++20), Blender 5.2 (bpy headless), Python 3 + uv + numpy
(audio), Git LFS para binarios.

**Spec:** `docs/superpowers/specs/2026-09-26-explored-design.md` y
`docs/design/biblia-de-contenido.md`.

## Global Constraints

- Motor: Unreal Engine 5.6 en `C:\Program Files\Epic Games\UE_5.6`.
- Lógica solo en C++; ningún Blueprint con lógica.
- Todos los assets generados por scripts versionados; ningún asset externo.
- Arte: estilizado suave (normales interpoladas), no facetado extremo.
- Terreno volumétrico (SDF + Surface Nets) con cuevas y voladizos.
- Python siempre vía `uv run`; nunca `pip` ni `python` directo.
- Rendimiento objetivo: 60 FPS a 1080p en Alta sobre RTX 4060 Laptop.
- Texto de código, comentarios, commits y docs en tono técnico con ortografía completa.

## Review Focus

- Semilla idéntica → mundo idéntico bit a bit (determinismo entre ejecuciones e hilos).
- Bordes entre chunks y entre LOD sin grietas visibles (faldones).
- Generación en hilos sin bloquear el hilo de juego (sin hitches > 16 ms al moverse).
- Scripts de contenido idempotentes: relanzarlos no duplica assets.
- El jugador nunca aparece bajo tierra ni dentro del agua al iniciar.

---

## Estructura de ficheros

```
Explored.uproject
Config/Default{Engine,Game,Input,Editor}.ini
Source/Explored.Target.cs, ExploredEditor.Target.cs
Source/Explored/                      módulo runtime
  Explored.Build.cs, Explored.cpp/.h
  Core/        ExploredRandom (PCG hash, ruido determinista)
  WorldGen/    ArchipelagoLayout, TerrainDensity, SurfaceNets, TerrainChunk, TerrainManager
  Ocean/       OceanActor
  Sky/         TimeOfDaySubsystem, SkyController
  Player/      ExploredCharacter (FP provisional para recorrer)
  Tests/       *.spec.cpp (Automation Spec)
Source/ExploredEditor/                módulo editor (commandlets y utilidades)
Tools/
  build.ps1  test.ps1  build_content.ps1  screenshot.ps1
  Blender/   lib/ (común) + assets/ (un script por familia de mallas) + run_all.py
  Audio/     pyproject.toml, explored_audio/ (síntesis), tests/
  Unreal/    import_meshes.py, import_audio.py, build_test_map.py
Art/Export/  (salida de pipelines, en LFS)
Content/     (en LFS)
```

## Tareas

### Task 1: Esqueleto del proyecto y build por CLI
- Crear `.uproject` (plugins: PythonScriptPlugin, EditorScriptingUtilities,
  ProceduralMeshComponent, GeometryScripting), targets, módulos y `Config/*.ini`
  (Lumen, Virtual Shadow Maps, TSR, Substrate desactivado).
- `.gitignore` de UE y `.gitattributes` con LFS para `*.uasset *.umap *.fbx *.wav *.png`.
- `Tools/build.ps1`: `Build.bat ExploredEditor Win64 Development -Project=...`.
- Verificación: el build termina con código 0.

### Task 2: Arnés de tests
- `Tools/test.ps1`: `UnrealEditor-Cmd.exe <uproject> -ExecCmds="Automation RunTests
  Explored;Quit" -unattended -nullrhi -nosplash -ReportExportPath=Saved/TestReport`,
  parsea `index.json` y sale con código ≠ 0 si falla algo.
- Primer spec: `Explored.Core.Random` (hash determinista, rango, distribución).

### Task 3: Aleatoriedad y ruido deterministas
- `FExploredRandom` (PCG32) y `FExploredNoise` (simplex 2D/3D + fBm + ridged + domain warp)
  sin estado global. Tests: misma semilla → mismos valores; rango [-1, 1]; continuidad.

### Task 4: Disposición del archipiélago
- `FArchipelagoLayout::Generate(Seed)` → 7 islas con arquetipo (§3.2 del GDD), centro,
  radio, altura máxima y rasgos (volcán, laguna, cascada), sin solapes, dentro de 6 km.
- Tests: 7 islas, arquetipos únicos, distancias mínimas, determinismo.

### Task 5: Campo de densidad (SDF)
- `FTerrainDensity::Sample(FVector)`: altura base por isla (máscara radial deformada +
  fBm + ridged en cumbres + cráter volcánico + plataforma de arrecife) → distancia
  vertical; cuevas por ruido 3D «gusano» restringido a zonas marcadas; arcos marinos y
  voladizos en los Dientes. Material por bioma codificado en color de vértice.
- Tests: centro de isla sólido, mar abierto profundo vacío hasta el fondo, determinismo.

### Task 6: Surface Nets
- `FSurfaceNets::Polygonize(const FDensityGrid&) → FTerrainMeshData` (posiciones,
  normales por gradiente, colores, índices), con faldones en los bordes.
- Tests: esfera SDF → malla cerrada, normales hacia fuera, sin triángulos degenerados.

### Task 7: Horneado del mundo (commandlet)
- Commandlet `ExploredWorldGen`: evalúa chunks en paralelo, poligoniza, crea `UStaticMesh`
  Nanite por chunk (64 m) en `/Game/World/Terrain/`, los coloca en el mapa World Partition
  `/Game/Maps/Archipelago` y guarda. Idempotente: borra y regenera.
- Verificación: mapa abierto en el editor, sin grietas entre chunks, 60 FPS al recorrerlo.

### Task 8: Material de terreno estilizado
- Material generado por Python (`build_materials.py`): color de vértice por bioma +
  triplanar de ruido procedural + pendiente → roca, altura → arena/hierba.

### Task 9: Océano, cielo y día/noche
- `AOceanActor`: malla en anillos que sigue al jugador, material con olas Gerstner,
  profundidad → color y espuma. `UTimeOfDaySubsystem` + `ASkyController`
  (DirectionalLight sol/luna, SkyAtmosphere, SkyLight en tiempo real, niebla volumétrica,
  nubes volumétricas). Tests: hora → ángulo solar; ciclo completo.

### Task 10: Pipeline de Blender y kit de vegetación
- `Tools/Blender/lib`: limpiar escena, sombreado suave, color de vértice, LODs, exportar
  FBX. Assets: 3 palmeras, 3 árboles de selva, 4 arbustos/helechos, 5 rocas, hierba.
- Importación por `Tools/Unreal/import_meshes.py` (idempotente) a `/Game/Generated/Meshes`.

### Task 11: Dispersión de vegetación
- `UScatterSubsystem`: por chunk LOD0/1, puntos deterministas (Poisson por celdas) con
  reglas de bioma/pendiente/altura → HISM por especie, culling por distancia.

### Task 12: Pipeline de audio
- `Tools/Audio`: síntesis de olas, viento y ambiente de selva (bucles sin cortes);
  tests con pytest (duración, sin clipping, bucle continuo). Importación a
  `/Game/Generated/Audio` y ambiente básico en el mapa.

### Task 13: Personaje provisional, mapa de prueba y capturas
- `AExploredCharacter` FP con Enhanced Input (andar, correr, saltar, volar en modo debug).
- `Tools/screenshot.ps1`: lanza el juego con cámaras de referencia y `HighResShot`.
- Verificación final de M1: capturas de amanecer, mediodía, atardecer y noche en 3 islas.
