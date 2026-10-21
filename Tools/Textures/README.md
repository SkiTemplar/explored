# Tools/Textures — texturas por script (sin fotos)

Única textura de detalle del juego: la máscara de trazos del terreno
`T_TerrainStrokes` (excepción documentada a «solo T_Paleta», GDD v3 §11).

```bash
uv sync
uv run textures-strokes        # Art/Export/Textures/T_TerrainStrokes.png (512 × 512, gris, ~0,2 MB; no se versiona)
uv run ruff check . && uv run basedpyright && uv run pytest -q --cov
```

- Gris medio (0,5) con 1 100 trazos de punta redonda orientados a ~65° ± 28°
  (hierba peinada), mitad oscuros y mitad claros, borde suavizado de 1 px.
  Dibujados en coordenadas toroidales: se repite sin costuras. Media
  recentrada a 0,5 para que el material no aclare ni oscurezca de media.
- `strokes.json` reúne los parámetros de la imagen y los del material, que
  leen también `Tools/Unreal/materials_f6.py` (`M_TerrainToon`) y la vista
  previa de `Tools/Island` (`island-preview`): dos muestras en espacio de
  mundo (teselas de 3,2 m y 9,7 m girada 37°, pesos 0,6 y 0,4), factor de
  color entre 0,7 y 1,24, que se apaga en pendientes (normal Z 0,55-0,8) y a
  distancia (35-90 m).
- En Unreal: lineal (sin sRGB), `TC_Grayscale`, mips del grupo World y
  repetición; `M_TerrainToon` la muestrea con `SAMPLERTYPE_LINEAR_GRAYSCALE`.

`paleta.json` es la paleta del atlas de F3 (512 px) que aún lee
`Tools/Packs/normalize.py`; este paquete no la usa. La paleta vigente del estilo
es `Tools/Style/paleta.json`.
