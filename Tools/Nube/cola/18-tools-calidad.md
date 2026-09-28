TAREA: sube la calidad de las herramientas Python: Tools/Packs, Tools/Textures, Tools/Audio, Tools/DataCheck, Tools/Localization y Tools/Blender.

Pasos:
1. En Tools/Packs, `PALETTE_PNG` no está definido y hay tipos mal según pyright. Arréglalo y añade tests.
2. Pasa pyright (o basedpyright) y ruff en cada paquete con uv, y arregla los errores reales.
3. Sube la cobertura de pytest de cada herramienta al 80 %.
4. CI: si ya hay un workflow de GitHub Actions, que ejecute todo lo anterior; si no lo hay, créalo.
