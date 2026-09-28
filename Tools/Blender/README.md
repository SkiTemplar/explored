# Tools/Blender

Scripts de Blender 5.2 (headless) que generan las mallas low poly de Explored:

- `run_all.py`: vegetación y rocas (`assets/`), y encadena props y fauna.
- `props/run_props.py`: props narrativos, kit de construcción, ruinas, objetos de inventario y acantilados.
- `run_animals.py`: fauna por piezas, sin esqueleto (canal de color `Anim` para el shader).
- `validate.py`: reimporta los FBX y comprueba presupuesto, geometría degenerada, color `Col`, materiales y dimensiones.
- `render_preview.py` y `props/preview_kit.py`: láminas de revisión.

Con Blender instalado: `pwsh Tools/Blender/build_meshes.ps1` (o `blender -b --factory-startup --python <script>`).

## Tests sin Blender instalado

Usan el módulo `bpy==5.2.2` de PyPI (Blender 5.2 como módulo de Python 3.13), así que no hace falta el
ejecutable. Desde `Tools/Blender`:

```sh
uv run ruff check .
uv run basedpyright
uv run pytest -q --cov --cov-report=term-missing
```

La batería genera y exporta los kits completos a directorios temporales (nunca escribe en `Art/Export` ni en
`docs/art`), los valida con `validate.py` y renderiza las láminas a resolución mínima. Los escaneos CC0 de
los acantilados se sustituyen por escaneos sintéticos, así que no necesita red. Las láminas de
`render_preview.py` usan Workbench, que en Linux sin GPU necesita EGL de Mesa:
`sudo apt-get install -y libegl1 libegl-mesa0 libgl1-mesa-dri`.
