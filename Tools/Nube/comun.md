Eres una sesión en la nube del proyecto «Explored», repo SkiTemplar/explored: supervivencia, exploración, construcción y minería en un archipiélago tropical, con UE 5.6 y C++, para Steam y con cooperativo de 2 a 4 jugadores.

En esta nube NO hay Unreal. Verifica siempre con:
- `Tools/HostTests/run.sh`, también con `HOST_TESTS_SANITIZE=ON`;
- `cd Tools/DataCheck && uv run datacheck --strict && uv run pytest -q`;
- los pytest de Tools/Textures, Tools/Audio o Tools/Localization, según lo que toques.

Instala uv si falta.

Qué manda:
- docs/diseno/biblia/ manda en el detalle. Lee 00-indice.md y la sección que te toque.
- docs/diseno/gdd_v2.md manda en el alcance.
- docs/diseno/biblia/00-TODO.md es la lista de tareas: márcala al completar algo.

Reglas:
- La lógica nueva va en modelos puros `F<Algo>Model`, que solo incluyen CoreMinimal.h, con su `Tests/<Algo>ModelSpec.cpp` y registrados en pure_sources.txt y pure_specs.txt.
- Toda mecánica define su replicación según biblia/08.
- Los textos que ve el jugador van en ES y EN y cumplen la guía anti-IA de biblia/07.
- Nunca versiones Content/World ni Content/Maps.
- Commits pequeños en español técnico, con ortografía completa y conventional commits.
- Entrega por rama, con push y PR hacia main que incluya un resumen y cómo verificarla. Nunca hagas push a main.
- Si la PR toca código de motor que aquí no se compila (actores, componentes, subsistemas, Build.cs, Config, materiales), ponle la etiqueta `necesita-unreal` y explica qué hay que verificar en local.
