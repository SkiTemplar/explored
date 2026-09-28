# Packs CC0 (Kenney, KayKit, Quaternius)

GDD v2 §7.1: el arte genérico sale de packs CC0 y se retoca a la paleta por isla
(`docs/art/paleta.md`). Lo propio del juego (marae, petroglifos, campamento Halden, el
«Limón», objetos narrativos) sigue en `Tools/Blender`. **Nunca se versionan los ficheros de
los packs ni los FBX**: solo este código, el manifiesto, el catálogo y hojas de contacto
pequeñas.

| Fichero | Qué es |
|---|---|
| `packs.json` | Manifiesto: autor, página oficial, versión, fuente de descarga, licencia CC0 verificada (fecha y texto leído) y sha256 del zip. |
| `packs_cc_by.json` | Mismo esquema, para assets CC-BY con crédito (Poly Pizza: antiguo Google Poly, autores independientes). Fuera de `packs.json` a propósito: `Tools/DataCheck` exige CC0-1.0 en todo lo que haya ahí. `uv run python fetch_packs.py --manifest packs_cc_by.json` para descargarlo. No pasa por `packs_catalogo.json`; ver `Content/Data/seabed_scatter.json`. |
| `fetch_packs.py` | Descarga reproducible a `Art/Packs/` (ignorado; `$EXPLORED_PACKS_CACHE` lo cambia) y verifica el sha256. `archive` en la entrada (por defecto `zip`) da la extensión real; con otra (p. ej. `glb`) copia el fichero suelto sin descomprimir, para los modelos de un único glb de Poly Pizza. |
| `../../Content/Data/packs_catalogo.json` | Id de juego → fichero del pack, escala en metros, pivote, reglas de color, descartes y pendientes. |
| `normalize.py` | Blender: aplica el catálogo y exporta `Art/Export/Packs/<lote>/SM_Pack_*.fbx` (ignorado). |
| `contact_sheet.py` | Compone `docs/art/packs/<lote>.png` (< 1 MB) con el original y el normalizado. |

```bash
cd Tools/Packs
uv run python fetch_packs.py                    # descarga y verifica todo
uv run pytest -q
cd ../..
blender -b --factory-startup --python Tools/Packs/normalize.py -- --lote lote1-herramientas --tiles
uv run --with pillow python Tools/Packs/contact_sheet.py lote1-herramientas
# Para escribir reglas de color de una entrada nueva:
blender -b --factory-startup --python Tools/Packs/normalize.py -- --lote <lote> --ids <id> --analyze
```

Workbench necesita EGL: en un Linux sin GPU, instala `libegl1 libgl1-mesa-dri`.

## Fuentes

- **Kenney**: URL directa del zip (la ruta lleva el hash de la versión).
- **itch.io** (KayKit, Quaternius): páginas de precio libre. Se usa el flujo del botón
  «No thanks, just take me to the downloads» (POST a `/file/<uploadId>`); `uploadId` fija el
  fichero y el sha256 detecta cualquier cambio del autor.
- **Google Drive** (enlaces de quaternius.com): no se usa, porque la cuota de descarga falla
  («Quota exceeded») y no es reproducible.

## Recoloreado

Cada cara toma su color del pack (textura en el centro UV o color del material) y se le
asigna la muestra de `paleta.json` de la regla más cercana en Oklab. La salida lleva **UV de
la paleta** (u en la columna, v por la altura: arriba claro, abajo oscuro) para `M_LowPoly`
con `T_Palette_<Isla>`, y el mismo degradado como color de vértice `Col`. La UV no cambia
entre islas: el atlas pone el grado de cada isla.

Si dos muestras vecinas se alternan cara a cara en una misma pieza, salen dientes de sierra
(pasó con el mazo del lote 1): esa pieza se deja con una sola muestra.

## Lotes

| Lote | Qué cubre | Hoja |
|---|---|---|
| `lote1-herramientas` | Hacha, cuchillo, pala, mazo, antorcha, cuerda, cordel, tronco, cobre, hierro | `docs/art/packs/lote1-herramientas.png` |
| `lote2-caza` | Lanza, arco y flecha (KayKit Fantasy Weapons Bits) en obsidiana, bambú y fibra | `docs/art/packs/lote2-caza.png` |
| `lote3-comida` | Plátano, limón, piña y seta (Kenney Food Kit) | `docs/art/packs/lote3-comida.png` |
| `lote4-huerto` | Fases de platanera, piña y limonero (Kenney Nature Kit) | `docs/art/packs/lote4-huerto.png` |
| `lote5-fauna` | Cerdo salvaje de Esmeralda con rig y 6 acciones (Quaternius Farm Animals) | `docs/art/packs/lote5-fauna.png` |
| `lote6-fondomarino` | Corales, borde del arrecife, praderas y conchas, algas y erizos, roca submarina (Poly Pizza CC0 y CC-BY). Descargado y catalogado; falta `normalize.py` e importar en Unreal (`necesita-unreal`, ver `Content/Data/seabed_scatter.json`). | `docs/art/packs/lote6-fondomarino-creditos.md` (hoja en texto; el PNG sale del `normalize.py` pendiente) |

Kit de construcción (prioridad 2): Kenney Fantasy Town y Pirate y KayKit Medieval Builder se
revisaron el 2026-09-28 y se descartaron (ver `discarded` del catálogo): ningún pack CC0
trae palma ni bambú, y los de madera y piedra son de pueblo europeo o no cuadran con la
rejilla de 2 m. El kit propio de `Tools/Blender` se mantiene.

Huerto (prioridad 3): las etapas son `kind: planta` con id `<planta>.<etapa>` de
`plants.json`. Taro, batata y maracuyá no tienen candidato (hoja en flecha, rastrera y
trepadora de espaldera): siguen en `pending`. El follaje de Quaternius Stylized Nature
MegaKit usa texturas de hojas con alfa y no se puede recolorear por cara; de ese pack solo
interesan los cantos (`Pebble_Round_*`), pendientes de revisar para `canto_rodado`.

## Fauna con esqueleto

`kind: fauna` (ids de `Content/Data/fauna_terrestre.json`) lleva un bloque `rig`: armadura y
acciones del pack se conservan y salen en `SK_Pack_*.fbx` con una toma por acción. El animal
mira a +X (delante de Unreal) con el pivote en el suelo entre las patas. `behaviors` asigna
a cada comportamiento de la biblia 02 §11.2-11.3 un clip del pack (`null` si falta: el cerdo
no trae comer, beber ni dormir). La hoja de contacto pinta una viñeta por pose de
`tilePoses` para ver que la pose aguanta la escala.

- Se parte del `.blend` del pack cuando el FBX no trae todas las acciones.
- `transform_apply` escala los huesos pero no las claves de `location`: `normalize.py` las
  multiplica por la misma escala.
- Quaternius comparte acciones entre especies: las del cerdo animan `Tail1`..`Tail4`, que
  su esqueleto no tiene, y el exportador FBX descarta cualquier acción con una curva que no
  resuelve. `normalize.py` quita esas curvas antes de exportar.

## Trampas de importación

- Ficheros con varias mallas (puertas con hoja, arcos con cuerda): `import_file` las une.
- Claves de forma (cuerda de los arcos de KayKit): se borran al importar; si no, la escala y
  el giro se aplican a la malla pero no a la clave base y el FBX sale sin normalizar.

## Import en Unreal (local)

Importar `Art/Export/Packs/<lote>/*.fbx` en `/Game/Packs/<lote>/` con `M_LowPoly`, sin
materiales ni texturas del FBX. La fauna se importa como Skeletal Mesh con animaciones
(`SKEL_Pack_*` del bloque `rig`); las tomas llegan como `Armature|<Acción>`. Cambiar el `meshPath` de `items.json` a la malla nueva
(`replaces` dice cuál sustituye) y quitar el script propio de `Tools/Blender` solo cuando la
malla esté en el repo, para que DataCheck siga en verde.
