TAREA: sustituye las texturas pseudo-realistas por texturas estilizadas coherentes con el low poly. Toca solo Tools/Textures y docs.

Criterio del director (2026-09-28):
- Fuera las fotos planas «de juego de móvil que parece realista y no lo es», sin normales, en hierba, hojas y suelo.
- Tampoco quiere color mate liso: siempre un poco de textura estilizada. Sirven la variación pintada a mano, el trazo, el relieve suave o unas normales sutiles.

Pasos:
1. Inventaria las texturas que usan los materiales de terreno, vegetación y rocas: Tools/Textures/gen_textures.py, fetch_polyhaven.py y los materiales que genera Tools/Unreal/build_materials.py.
2. Genera por código, con semilla y de forma determinista, un juego estilizado para hierba, arena seca y mojada, tierra, roca volcánica, caliza y hojas. Cada una lleva albedo que respete la paleta de Tools/Textures/paleta.json por isla, normal suave y roughness entre 0,85 y 0,95 en el suelo.
3. Haz que sean tileables sin costuras, con macro-variación para que no se note la repetición a 50 m.
4. Pon una hoja de contacto por material en docs/art/.
5. Tests: tileado sin costura, rango de colores dentro de la paleta, normales normalizadas y determinismo.

Abre la PR con la etiqueta `necesita-unreal` y explica cómo reimportar y reconstruir los materiales en local.
