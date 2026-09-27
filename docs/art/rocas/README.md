# Kit de rocas y acantilados — atribucion

Las 10 piezas de `AcantiladoFormaciones` (paredes, espolones, farallones,
arco) parten de escaneos 3D reales, licencia **CC0** (dominio publico, sin
atribucion obligatoria — se documenta aqui igualmente por trazabilidad),
descargados de [Poly Haven](https://polyhaven.com):

| Asset (id) | Pagina | Uso en el kit |
|---|---|---|
| `namaqualand_cliff_01` | <https://polyhaven.com/a/namaqualand_cliff_01> | CliffWall_Basalt01 (x2, combinado), CliffWall_Sandstone01, CliffSpur01 |
| `namaqualand_cliff_02` | <https://polyhaven.com/a/namaqualand_cliff_02> | CliffWall_Basalt02, CliffSpur03 |
| `coastal_cliff_04` | <https://polyhaven.com/a/coastal_cliff_04> | CliffWall_Sandstone02, SeaArch01 |
| `namaqualand_boulder_02` | <https://polyhaven.com/a/namaqualand_boulder_02> | CliffWall_Sandstone01 (pieza secundaria), CliffSpur02, SeaStack01 |
| `moon_rock_01` (LOD1) | <https://polyhaven.com/a/moon_rock_01> | SeaStack02 |

Las 13 piezas de `AcantiladoBloques` (bloques, cantos, losas) son 100%
procedurales (envolvente convexa / anillo extruido en `_cliffkit.py`) y no
usan ningun escaneo.

## Cache local (no versionada)

Los `.gltf` + `.bin` originales (solo geometria; las texturas fotograficas
no se descargan ni se usan — el look final es color de vertice + material
triplanar `M_Stone`) viven en `Tools/Blender/props/.cache/rocks_cc0/<id>/`,
cubierto por la regla `.cache/` de `.gitignore`. Para regenerar el kit en
una maquina nueva, descargar de nuevo el glTF a 1k de cada asset de la
tabla y colocarlo en esa ruta con el mismo nombre de fichero que el id.

## Proceso de estilizado

Ver `Tools/Blender/props/_cliffscan.py`. Resumen: se queda solo con la isla
de vertices mas grande del escaneo (descarta ruido de fotogrametria),
suelda costuras de UV, le da grosor (Solidify) para que el voxel remesh
tenga un solido bien definido, remesh + suavizado ligero + decimate planar
(facetas grandes) + bisel, con AO real horneado (`bpy.ops.paint.vertex_color_dirt`)
como color de vertice. Rechazado en la primera revision de arte
(2026-09-27) un intento 100% procedural por bmesh — se leia como geometria
generada, no roca natural; sustituido por este pipeline basado en escaneo.
