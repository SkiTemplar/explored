# Packs CC0 (Kenney, KayKit, Quaternius)

GDD v2 §7.1: el arte genérico sale de packs CC0 y se retoca a la paleta por isla
(`docs/art/paleta.md`). Lo propio del juego (marae, petroglifos, campamento Halden, el
«Limón», objetos narrativos) sigue en `Tools/Blender`. **Nunca se versionan los ficheros de
los packs ni los FBX**: solo este código, el manifiesto, el catálogo y hojas de contacto
pequeñas.

| Fichero | Qué es |
|---|---|
| `packs.json` | Manifiesto: autor, página oficial, versión, fuente de descarga, licencia CC0 verificada (fecha y texto leído) y sha256 del zip. |
| `fetch_packs.py` | Descarga reproducible a `Art/Packs/` (ignorado; `$EXPLORED_PACKS_CACHE` lo cambia) y verifica el sha256. |
| `../../Content/Data/packs_catalogo.json` | Id de juego → fichero del pack, escala en metros, pivote, reglas de color, descartes y pendientes. |
| `normalize.py` | Blender: aplica el catálogo y exporta `Art/Export/Packs/<lote>/SM_Pack_*.fbx` (ignorado). |
| `normalize_core.py` | Lógica pura de `normalize.py` (color Oklab, reglas, degradado de paleta, medida y pivote), sin `bpy`. |
| `contact_sheet.py` | Compone `docs/art/packs/<lote>.png` (< 1 MB) con el original y el normalizado. |
| `icons.py` | Iconos de UI (`icons` del catálogo): silueta blanca de 128 px a `Art/Export/Packs/<lote>/T_Pack_Icon_*.png` (ignorado) y su hoja de contacto. |

```bash
cd Tools/Packs
uv run python fetch_packs.py                    # descarga y verifica todo
uv run pytest -q
cd ../..
blender -b --factory-startup --python Tools/Packs/normalize.py -- --lote lote1-herramientas --tiles
uv run --with pillow python Tools/Packs/contact_sheet.py lote1-herramientas
uv run --with pillow python Tools/Packs/icons.py lote9-iconos   # iconos de UI, sin Blender
# Para escribir reglas de color de una entrada nueva:
blender -b --factory-startup --python Tools/Packs/normalize.py -- --lote <lote> --ids <id> --analyze
```

Workbench necesita EGL: en un Linux sin GPU, instala `libegl1 libegl-mesa0 libgl1-mesa-dri`.

### Calidad (lo mismo que ejecuta CI)

```bash
cd Tools/Packs
uv run ruff check .
uv run basedpyright                  # tipos de bpy con fake-bpy-module-5.2
uv run --python 3.13 --group blender pytest -q --cov --cov-report=term-missing
```

El grupo `blender` instala `bpy==5.2.2` (Blender 5.2 como módulo, solo Python 3.13, unos
400 MB): `tests/test_normalize_blender.py` genera un pack sintético (glTF con textura, OBJ,
FBX y un `.blend` con esqueleto) y ejecuta `normalize.py` de punta a punta en un tmp. Sin
ese grupo esos tests se saltan y la cobertura no llega al 80 % exigido.

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
| `lote6-mineria` | Pico (KayKit RPG Tools), canto rodado y caliza (Quaternius Stylized Nature MegaKit) | `docs/art/packs/lote6-mineria.png` |
| `lote7-cuarzo-taro` | Cristal de cuarzo (Quaternius Ultimate Platformer) y tres fases del taro (Quaternius Ultimate Nature) | `docs/art/packs/lote7-cuarzo-taro.png` |
| `lote8-suelo` | Piedra plana, basalto, arenisca, obsidiana, pedernal (Quaternius Stylized Nature MegaKit) y madera flotante (Quaternius Ultimate Nature) | `docs/art/packs/lote8-suelo.png` |
| `lote9-iconos` | Iconos de UI: fuego, refugio y estrella de los logros; laurel, candado y reloj de arena de las pantallas de logros y de guardado (Kenney Board Game Icons y Game Icons) | `docs/art/packs/lote9-iconos.png` |
| `lote10-mar-y-coco` | Mitad de coco, pescado de arrecife y espina de pescado (Kenney Food Kit) | `docs/art/packs/lote10-mar-y-coco.png` |
| `lote11-herreria` | Clavos y yunque de la herrería de la fase de metal (KayKit RPG Tools Bits) | `docs/art/packs/lote11-herreria.png` |

Kit de construcción (prioridad 2): Kenney Fantasy Town y Pirate y KayKit Medieval Builder se
revisaron el 2026-09-28 y se descartaron (ver `discarded` del catálogo): ningún pack CC0
trae palma ni bambú, y los de madera y piedra son de pueblo europeo o no cuadran con la
rejilla de 2 m. El kit propio de `Tools/Blender` se mantiene.

Huerto (prioridad 3): las etapas son `kind: planta` con id `<planta>.<etapa>` de
`plants.json`. Taro, batata y maracuyá no tienen candidato (hoja en flecha, rastrera y
trepadora de espaldera): siguen en `pending`. El follaje de Quaternius Stylized Nature
MegaKit usa texturas de hojas con alfa y no se puede recolorear por cara; de ese pack solo
interesan los cantos (`Pebble_Round_*`), pendientes de revisar para `canto_rodado`.

Minería (lote 6): los cantos y rocas de Quaternius traen musgo en la cara de abajo; se
recolorean enteros a una sola muestra (`piedra.canto`, `piedra.caliza`). Las piedras de
Kenney Nature y Survival (16-44 triángulos) quedan toscas al lado de KayKit. KayKit
Furniture Bits y Restaurant Bits se añadieron al manifiesto y se revisaron: no traen cesta,
y sus tarros no sirven de vasija. Siguen sin candidato `cristal_cuarzo`, `canto_aguzado`,
`basalto_tallado` y `tierra_suelta`.

Cuarzo y taro (lote 7): el Ultimate Nature Pack de Quaternius, a diferencia del Stylized
Nature MegaKit, colorea el follaje con materiales planos, así que se recolorea por cara.
`Plant_4` (hojas anchas sobre peciolo largo) cubre las tres fases del taro con la misma
silueta: brote a 0,3 m en hoja clara, hojas grandes a 0,9 m y listo a 0,8 m en hierba seca
(hojas que amarillean). `Gem_Blue` del Ultimate Platformer es un prisma hexagonal con
puntas, la forma del cuarzo. El cangrejo de ese pack es un enemigo con cara y se descarta;
ningún pack reproducible trae cangrejo cocotero ni gaviota (fauna de Landing).

Suelo (lote 8): piedras sueltas de las primeras horas con los cantos y rocas de Quaternius
Stylized Nature MegaKit, a una sola muestra por piedra como en el lote 6 (el musgo del pack
no se conserva). Rock_Medium_2 se descartó para la obsidiana: trae una cara clara plana que
rompe la pieza. El pedernal no tiene muestra propia y usa `piedra.basalto_claro`. La viga
de apoyo de mina no tiene candidato (ver `pending`).

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

Iconos de UI (lote 9): una sola familia de siluetas blancas redondeadas (Kenney Board Game
Icons; de Game Icons, solo la estrella). No usan `paleta.json`: la UI no cambia por isla y
Slate los tiñe con los colores de `ExploredUIStyle.h` que dice `tint` para cada estado
(conseguido, pendiente, oculto…). Cada pista `icon` de `achievements.json` tiene icono o
está en `iconsPending` (DataCheck lo exige). Kenney no tiene iconos de mar ni de naturaleza
tropical con esta silueta: 25 pistas siguen pendientes de siluetas propias. Revisados y
descartados sin entrar en el manifiesto: Generic Items (objetos planos a color, de oficina),
Minimap Pack (pixel art) y UI Pack Adventure (marcos y paneles, no iconos).

Mar y coco (lote 10): el pez de Kenney Food Kit (233 triángulos) queda bien como pescado genérico de
arrecife, con lomo oscuro y vientre nacarado; los peces con nombre siguen en `Tools/Blender`. Se
descartaron la concha (mejillón `mussel.glb`: bloque hexagonal que recoloreado parece piedra rosada) y,
ya desde el lote 3, el coco entero y el huevo.
