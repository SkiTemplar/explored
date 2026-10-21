# Tools/Blender

Scripts de Blender 5.2 (headless). Tras el reset v2 (docs/v3/plan.md) solo queda la base que
reutilizará el pipeline de estilo nuevo (F2):

- `run_animals.py`: fauna por piezas, sin esqueleto (canal de color `Anim` para el shader).
- `validate.py`: reimporta los FBX y comprueba presupuesto, geometría degenerada, color `Col`, materiales y dimensiones.
- `lib/common.py`: utilidades compartidas.

La vegetación (`assets/`), los props (`props/`), la IA 3D (`ai/`), `run_all.py`,
`build_meshes.ps1` y `render_preview.py` eran el arte v1 y se borraron.

## Tests sin Blender instalado

Usan el módulo `bpy==5.2.2` de PyPI (Blender 5.2 como módulo de Python 3.13), así que no hace falta el
ejecutable. Desde `Tools/Blender`:

```sh
uv run ruff check .
uv run basedpyright
uv run pytest -q --cov --cov-report=term-missing
```

La batería genera y exporta la fauna a directorios temporales (nunca escribe en `Art/Export`) y
la valida con `validate.py`. Workbench en Linux sin GPU necesita EGL de Mesa:
`sudo apt-get install -y libegl1 libegl-mesa0 libgl1-mesa-dri`.
