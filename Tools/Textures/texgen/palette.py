"""Paleta low poly de Explored y atlas de paleta (T_Palette_<Isla>).

Los packs CC0 (Kenney, KayKit, Quaternius) se colorean con un único material (M_LowPoly)
que lee un atlas de colores: cada celda es una muestra de la paleta con un degradado
vertical suave (arriba algo más clara y cálida, abajo algo más oscura y fría). Una cara
que apunte al centro de la celda recibe el color exacto; una malla que reparta su v por la
celda recibe el degradado (estilo KayKit).

Fuente de verdad: este módulo. `gen_palette.py` escribe `Tools/Textures/paleta.json`
(versionado) y los PNG; los tests comprueban que el JSON versionado coincide.

Espacio de color: las muestras se escriben en sRGB (hex) y se manipulan en Oklab
(Björn Ottosson, 2020), que es perceptualmente uniforme: ΔE_ok ≈ 0.02 es la diferencia
apenas visible y 0.1 una diferencia clara. Los grados por isla y los degradados se
aplican en Oklab/OkLCh para que no cambie el tono al aclarar u oscurecer.

Atlas (ver `ATLAS`):
  512×512, rejilla 16×16 de celdas de 32 px, alineadas a potencia de 2.
  Cada celda: 4 px de margen arriba y abajo que repiten el color extremo, 10 px de
  degradado arriba→medio, 4 px planos con el color exacto (centro), 10 px medio→abajo.
  Todas las columnas de una celda son iguales (no hay variación horizontal).
  - Mips por promedio 2×2 (TMGS_SimpleAverage): hasta el mip 5 (celda = 1 texel) una
    celda nunca se mezcla con la vecina porque las celdas están alineadas.
  - Muestreo bilineal: con margen m = 4 px y mip k, el bilineal de un punto con v dentro
    de la zona útil [4, 28] px no toca la celda vecina mientras 2^(k-1) <= m, o sea
    k <= 3. M_LowPoly limita el mip a MaxMip = 3.
  - Sin compresión por bloques (TC_EditorIcon = UserInterface2D RGBA8): BC1 comprime
    bloques de 4×4 y, en mips con celdas de menos de 4 px, un bloque mezclaría celdas.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np

# ---------------------------------------------------------------------------
# Color: sRGB <-> lineal <-> Oklab
# ---------------------------------------------------------------------------

_M1 = np.array([[0.4122214708, 0.5363325363, 0.0514459929],
                [0.2119034982, 0.6806995451, 0.1073969566],
                [0.0883024619, 0.2817188376, 0.6299787005]])
_M2 = np.array([[0.2104542553, 0.7936177850, -0.0040720468],
                [1.9779984951, -2.4285922050, 0.4505937099],
                [0.0259040371, 0.7827717662, -0.8086757660]])
_M2_INV = np.linalg.inv(_M2)
_M1_INV = np.linalg.inv(_M1)

# Límites de albedo del proyecto (docs/art/texturas.md): ni negro ni blanco puros.
ALBEDO_MIN, ALBEDO_MAX = 0.035, 0.95


def srgb_to_linear(c):
    c = np.asarray(c, dtype=np.float64)
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def linear_to_srgb(c):
    c = np.clip(np.asarray(c, dtype=np.float64), 0.0, 1.0)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * c ** (1 / 2.4) - 0.055)


def linear_to_oklab(lin):
    lms = np.cbrt(np.asarray(lin, dtype=np.float64) @ _M1.T)
    return lms @ _M2.T


def oklab_to_linear(lab):
    lms = np.asarray(lab, dtype=np.float64) @ _M2_INV.T
    return (lms ** 3) @ _M1_INV.T


def srgb_to_oklab(c):
    return linear_to_oklab(srgb_to_linear(c))


def oklab_to_srgb(lab):
    return linear_to_srgb(oklab_to_linear(lab))


def hex_to_srgb(h: str) -> np.ndarray:
    h = h.lstrip("#")
    return np.array([int(h[i:i + 2], 16) for i in (0, 2, 4)], dtype=np.float64) / 255.0


def srgb_to_hex(c) -> str:
    q = np.clip(np.round(np.asarray(c) * 255.0), 0, 255).astype(int)
    return "#%02x%02x%02x" % tuple(q)


def quantize(c) -> np.ndarray:
    """sRGB en [0, 1] -> el valor exacto que queda en un PNG de 8 bits."""
    return np.clip(np.round(np.asarray(c, dtype=np.float64) * 255.0), 0, 255) / 255.0


def lch(L: float, C: float, h_deg: float) -> np.ndarray:
    h = np.radians(h_deg)
    return np.array([L, C * np.cos(h), C * np.sin(h)])


def chroma(lab) -> np.ndarray:
    lab = np.asarray(lab)
    return np.hypot(lab[..., 1], lab[..., 2])


def delta_e(a, b) -> np.ndarray:
    return np.linalg.norm(np.asarray(a) - np.asarray(b), axis=-1)


def fit_albedo(lab: np.ndarray) -> np.ndarray:
    """Lleva un color Oklab al rango de albedo del proyecto reduciendo croma (no tono)."""
    lab = np.array(lab, dtype=np.float64)
    for _ in range(40):
        s = oklab_to_srgb(lab)
        lin = oklab_to_linear(lab)
        if (lin >= -1e-9).all() and (lin <= 1 + 1e-9).all() and s.min() >= ALBEDO_MIN and s.max() <= ALBEDO_MAX:
            return lab
        lab[1:] *= 0.9
        if chroma(lab) < 1e-3:
            break
    s = np.clip(oklab_to_srgb(lab), ALBEDO_MIN, ALBEDO_MAX)
    return srgb_to_oklab(s)


# ---------------------------------------------------------------------------
# Paleta
# ---------------------------------------------------------------------------

@dataclass(frozen=True)
class Family:
    key: str
    row: int
    graded: bool      # True: cambia por isla (entorno); False: idéntica en todas (identidad de objeto)
    accent: bool      # True: puede superar el tope de croma (comida, UI: tiene que saltar a la vista)
    desc: str
    swatches: tuple[tuple[str, str, str], ...]   # (clave, sRGB medio, uso)
    pickup: bool = False  # True: se recoge del suelo; debe leerse sobre todos los suelos de la isla


# Tope de croma Oklab para familias de entorno: el agua de M_Ocean (arrecife, C ≈ 0.11;
# laguna 0.09) es lo más saturado del paisaje y el entorno no pasa de ~1.3 veces eso, para
# que el mar siga siendo el protagonista del color. Los acentos (comida, UI, flores) sí
# pueden superarlo: tienen que leerse desde lejos.
CHROMA_CAP = 0.14
# Acentos dentro de familias de entorno (flores): se permiten por encima del tope.
ACCENT_SWATCHES = frozenset({"vegetacion.flor_roja", "vegetacion.flor_amarilla", "fauna.cresta"})
# Pelajes y plumajes de fauna: cubren el cuerpo del animal, que se caza o se esquiva, así
# que tienen que leerse sobre todos los suelos de la isla como lo recogible. Las piezas
# pequeñas (pezuña, cuerno, cresta, pico) no: las rodea el pelaje.
FAUNA_BODY_SWATCHES = frozenset({"fauna.jabali", "fauna.jabali_claro", "fauna.pardo", "fauna.canela",
                                 "fauna.crema", "fauna.rosado", "fauna.plumaje"})

FAMILIES: tuple[Family, ...] = (
    Family("madera", 0, True, False, "Madera de construcción, herramientas y muebles.", (
        ("clara", "#c9a575", "Tabla nueva, mangos, cajas KayKit claras."),
        ("miel", "#b07e4a", "Madera por defecto (tablones, WoodPlanks)."),
        ("caramelo", "#8f5a34", "Vigas, postes, muebles oscuros."),
        ("oscura", "#5e3b27", "Detalles, juntas, madera mojada."),
        ("rojiza", "#8d4a33", "Maderas nobles tropicales, remos."),
        ("deriva", "#a59c8d", "Madera de deriva gris plata (playa)."),
        ("corteza", "#6c5641", "Troncos sin labrar, leña."),
        ("quemada", "#3e2e26", "Carbón, madera quemada, hoguera."),
    )),
    Family("palma", 1, True, False, "Palma, paja y coco: techos, cestos, cuerdas.", (
        ("paja", "#dab86a", "Techo de paja (PalmThatch)."),
        ("dorada", "#c79744", "Paja al sol, cestería."),
        ("seca", "#a78a52", "Hoja seca caída, estera vieja."),
        ("verde", "#8da24b", "Hoja de palma recién cortada."),
        ("tierna", "#b9c56c", "Brote, hoja joven."),
        ("fibra", "#c0a074", "Cuerda de coco, bramante (Rope)."),
        ("coco", "#7b5434", "Cáscara de coco."),
        ("pulpa", "#efe6d2", "Pulpa de coco, interior."),
    )),
    Family("bambu", 2, True, False, "Bambú: estructuras ligeras, cañas, balsas.", (
        ("verde", "#8aa648", "Caña viva."),
        ("verde_oscuro", "#5f7a34", "Caña vieja, sombra."),
        ("maduro", "#c9b061", "Caña curada (Bamboo)."),
        ("seco", "#b09860", "Caña seca, balsa gastada."),
        ("nudo", "#7c693b", "Nudos y anillos."),
        ("interior", "#e2d5a9", "Corte de caña, tablillas."),
    )),
    Family("piedra", 3, True, False, "Piedra: rocas sueltas, muros, herramientas líticas.", (
        ("basalto", "#5b5362", "Basalto del Humo (= terreno VolcanicRock)."),
        ("basalto_claro", "#7c7483", "Caras al sol del basalto, lajas."),
        ("caliza", "#b0a896", "Caliza de Los Dientes (= terreno Limestone)."),
        ("caliza_clara", "#d0c9b8", "Caliza lavada, bloques tallados."),
        ("arenisca", "#c09b72", "Arenisca, cerámica sin cocer."),
        ("coral", "#cab9a2", "Bloques de coral del muro polinesio."),
        ("canto", "#8c8a83", "Cantos rodados, piedra de afilar."),
        ("obsidiana", "#3c3743", "Obsidiana, sílex oscuro (hachas)."),
    )),
    Family("metal", 4, False, False, "Metal: restos del avión, herramientas, clavos.", (
        ("hierro", "#6f6f74", "Hierro forjado, clavos."),
        ("oxido", "#8b5339", "Hierro oxidado, restos del avión."),
        ("cobre", "#b9744b", "Cobre, cazos."),
        ("verdin", "#6fa396", "Cobre con verdín, pecios."),
        ("bronce", "#b6974b", "Latón y bronce, instrumentos."),
        ("acero", "#9ba1a9", "Acero pulido, filos."),
        ("oro", "#d7ae4c", "Oro, tesoros."),
        ("plomo", "#46484f", "Plomo, hierro colado oscuro."),
    )),
    Family("tela", 5, False, False, "Tela y cuero: velas, toldos, ropa, bolsas.", (
        ("lona", "#e5d6b3", "Vela y toldo (Canvas)."),
        ("crudo", "#cbbb97", "Lino crudo, sacos."),
        ("rojo", "#b44b3d", "Tela teñida roja, banderas."),
        ("indigo", "#405b87", "Tela índigo, ropa del náufrago."),
        ("ocre", "#c88f3b", "Tela ocre, cúrcuma."),
        ("verde", "#5b7f50", "Tela verde, mochila."),
        ("tapa", "#b98f66", "Tapa (corteza batida polinesia)."),
        ("cuero", "#8a5a3b", "Cuero, correas."),
    )),
    Family("vegetacion", 6, True, False, "Vegetación de props (arbustos, flores, hierba) de los packs.", (
        ("hoja", "#3f8446", "Hoja por defecto."),
        ("hoja_oscura", "#36632e", "Sombra de copa, selva."),
        ("hoja_clara", "#84a957", "Hoja al sol, brotes."),
        ("hierba", "#60904a", "Matas de hierba (= terreno Grass)."),
        ("hierba_seca", "#b7a861", "Hierba seca, cañizo."),
        ("tronco_palma", "#8c7457", "Tronco de palmera."),
        ("musgo", "#848a3c", "Musgo, liquen."),
        ("flor_roja", "#cf4a41", "Hibisco (acento)."),
        ("flor_amarilla", "#e5c141", "Flor amarilla (acento)."),
    )),
    Family("comida", 7, False, True, "Comida recogible: tiene que saltar a la vista sobre arena, hierba y roca.", pickup=True, swatches=(
        ("mango", "#e8983a", "Mango, papaya."),
        ("platano", "#f0c04a", "Plátano maduro."),
        ("lima", "#9cc24b", "Lima, fruta verde."),
        ("limon", "#d6d940", "Limón (el del barco «Limón» y el escorbuto)."),
        ("carne_cruda", "#d9787a", "Carne y pescado crudos."),
        ("asado", "#8b4a2f", "Carne asada, pan de fruta tostado."),
        ("pescado", "#7b97ad", "Pescado plateado."),
        ("cangrejo", "#d0563b", "Cangrejo y langosta cocidos."),
        ("taro", "#9a7b8c", "Taro, raíces moradas."),
        ("pina", "#bfa332", "Piña (cáscara dorada verdosa: el mango ya es naranja)."),
        ("coco", "#5b4a2b", "Coco maduro, cáscara con fibra (palma.coco no se verifica contra el suelo)."),
        ("maracuya", "#5f2d5c", "Maracuyá morada."),
        ("seta", "#a8583a", "Sombrero de la seta comestible."),
        ("batata", "#9b4a5a", "Batata, piel rosada."),
        ("yuca", "#7a3a36", "Yuca, corteza de la raíz."),
        ("huevo", "#eee8d8", "Huevo de ave marina o de gallina."),
    )),
    Family("ui", 8, False, True, "Interfaz y objetos de UI en el mundo (mapa, marcadores).", (
        ("tinta", "#302b27", "Texto, tinta del mapa."),
        ("papel", "#eee3c7", "Papel del mapa (MapPaper)."),
        ("acento", "#e0a041", "Resaltado, selección."),
        ("peligro", "#c8443b", "Peligro, salud baja."),
        ("ok", "#5ea05a", "Correcto, comestible."),
        ("info", "#4f8ec0", "Información, agua."),
        ("neutro", "#8c8579", "Deshabilitado."),
        ("hueso", "#f1f0ec", "Blanco roto de la UI."),
    )),
    # Recursos sueltos de items.json sin familia propia hasta ahora (huesos, conchas, plumas,
    # resina...): se recogen del suelo, así que son identidad (iguales en las 4 islas) y
    # tienen que despegarse de arena, hierba, basalto, caliza y ceniza como la comida.
    Family("recurso", 11, False, True, "Recursos naturales sueltos: fauna, playa y arrecife.", pickup=True, swatches=(
        ("hueso", "#ece5d3", "Huesos, espinas grandes, anzuelo de hueso."),
        ("concha", "#ebc6c8", "Conchas pequeñas y grandes (rosado pálido: la arena es amarilla)."),
        ("nacar", "#bfd0d4", "Interior nacarado, lapa, cuentas."),
        ("caracola", "#e59a86", "Caracola, labio rosado de las conchas grandes."),
        ("pluma", "#7d6450", "Plumas de ave marina, emplumado de flechas."),
        ("alga", "#56703d", "Alga fibrosa, esponja de mar seca."),
        ("erizo", "#4a2f55", "Erizo de mar, tinta de pulpo."),
        ("resina", "#c47a26", "Resina y ámbar, pegamento, yesca de hongo."),
    )),
    Family("mineral", 12, False, True, "Minerales y restos del avión que se recogen o se extraen.", pickup=True, swatches=(
        ("arcilla", "#9c4f36", "Arcilla roja cruda."),
        ("terracota", "#c5704a", "Barro cocido: vasijas, tejas."),
        ("azufre", "#e3d54c", "Azufre del Humo (acento)."),
        ("cuarzo", "#e2dcec", "Cristal de cuarzo (blanco frío con un punto lila)."),
        ("sal", "#efeee8", "Sal marina."),
        ("aluminio", "#c0c9d3", "Tubo de aluminio y chapa del fuselaje del Albatros."),
        ("malaquita", "#3f8a70", "Mineral de cobre."),
        ("hematites", "#6b3a37", "Hierro del meteorito, óxido rojo."),
    )),
    # Fauna terrestre (fauna_terrestre.json y el kit de Tools/Blender/animals): identidad,
    # como la comida (un jabalí es el mismo en todas las islas). Antes de esta fila el
    # cerdo salvaje del lote 5 se coloreaba con madera.oscura/madera.quemada.
    Family("fauna", 13, False, False, "Pelaje, piel y plumaje de la fauna terrestre y de granja.", (
        ("jabali", "#5a4838", "Cerdas del jabalí (cerdo salvaje), lomo oscuro."),
        ("jabali_claro", "#8b7560", "Flancos y vientre del jabalí, mono."),
        ("pardo", "#94653f", "Cabra salvaje parda, cabra de granja."),
        ("canela", "#d8894a", "Canela (la perra), zorro, gato."),
        ("crema", "#e8e2d2", "Cabra blanca, pecho y hocico claros, gallina blanca."),
        ("rosado", "#df9a96", "Cerdo de granja, orejas y hocico."),
        ("pezuna", "#35302e", "Pezuñas, nariz, ojos, puntas de cuerno."),
        ("cuerno", "#c2b596", "Cuernos y colmillos."),
        ("plumaje", "#a24f2e", "Gallina roja, plumaje cobrizo."),
        ("cresta", "#c93a33", "Cresta y barbilla de la gallina (acento)."),
        ("pico", "#dcae45", "Pico y patas de ave."),
    )),
)

# Terreno: objetivos globales (Oklab L, C, h°) a los que se armonizan las texturas del
# terreno (ver `harmonize_albedo`) y que repiten los props de piedra/vegetación. El croma
# está alineado con los props: la hierba pintada salía a C = 0.14, por encima de cualquier
# prop de vegetación, y se leía «fluorescente» junto a ellos.
TERRAIN_TARGETS: dict[str, tuple[str, float, float, float, str]] = {
    # material: (clave de muestra, L, C, h, capa de color de vértice en TerrainDensity)
    "SandDry": ("arena_seca", 0.840, 0.075, 80.0, "Sand"),
    "SandWet": ("arena_mojada", 0.665, 0.066, 74.0, "Sand"),
    "Grass": ("hierba", 0.600, 0.115, 134.0, "Grass"),
    "VolcanicRock": ("basalto", 0.440, 0.032, -52.0, "Rock"),
    "Limestone": ("caliza", 0.730, 0.030, 85.0, "Rock"),
    "Ash": ("ceniza", 0.745, 0.012, 40.0, "Sand"),
}
TERRAIN_ROW = 9

# Referencias de entorno (no se modifican aquí: salen de build_materials.py y del cielo).
# Colores lineales tal como los usa el motor.
ENTORNO: dict[str, tuple[tuple[float, float, float], str]] = {
    "laguna": ((0.30, 0.86, 0.80), "M_Ocean BaseColorShallow"),
    "arrecife": ((0.05, 0.45, 0.62), "M_Ocean BaseColorMid"),
    "profundo": ((0.02, 0.10, 0.22), "M_Ocean BaseColorDeep"),
    "espuma": ((0.94, 0.97, 0.98), "M_Ocean FoamColor"),
    "cielo": ((0.32, 0.52, 0.80), "ExploredSkyController: niebla de día (FogInscatteringColor)"),
}
ENTORNO_ROW = 10

# Islas de acceso anticipado. `vertex` = paleta de FTerrainDensity::PaletteFor (sRGB 0-255):
# M_Terrain multiplica la textura por el tono de ese color al 18 %; el test
# test_palette_atlas.py comprueba que coincide con TerrainDensity.cpp.
# `grade` = ajuste de las familias de entorno en Oklab: dL, factor de croma, da, db.


@dataclass(frozen=True)
class Island:
    key: str
    archetype: str
    name: str
    vertex: dict
    grade: tuple[float, float, float, float]
    mood: str


ISLANDS: tuple[Island, ...] = (
    Island("Landing", "Landing", "Isla del Amaraje",
           {"Sand": (232, 212, 172), "Grass": (104, 128, 70), "Rock": (124, 116, 104)},
           (0.0, 1.0, 0.0, 0.004), "Luz de playa cálida y limpia: la paleta de referencia."),
    Island("Esmeralda", "Emerald", "Esmeralda",
           {"Sand": (220, 200, 160), "Grass": (74, 112, 58), "Rock": (92, 100, 86)},
           (-0.015, 1.08, -0.006, 0.0), "Selva húmeda: algo más oscura, verdes más ricos."),
    Island("Humo", "Smoke", "Isla del Humo",
           {"Sand": (70, 64, 62), "Grass": (100, 110, 64), "Rock": (58, 46, 44)},
           (-0.025, 0.85, 0.004, -0.004), "Volcán: apagada y cenicienta, sin llegar a gris sucio."),
    Island("Dientes", "Teeth", "Los Dientes",
           {"Sand": (206, 196, 178), "Grass": (118, 140, 88), "Rock": (150, 148, 142)},
           (0.015, 0.88, -0.002, -0.008), "Islotes de caliza batidos por el mar: fríos, salinos, claros."),
)

# Alias orientativos de nombres de material de los packs -> muestra (para el remapeo de
# UV al importar; Quaternius y Kenney nombran sus slots así).
PACK_ALIASES: dict[str, str] = {
    "Wood": "madera.miel", "WoodLight": "madera.clara", "WoodDark": "madera.oscura",
    "Planks": "madera.miel", "Bark": "madera.corteza", "Log": "madera.corteza",
    "Leaves": "vegetacion.hoja", "Leaf": "vegetacion.hoja", "LeavesDark": "vegetacion.hoja_oscura",
    "Grass": "vegetacion.hierba", "Moss": "vegetacion.musgo", "Flower": "vegetacion.flor_roja",
    "Stone": "piedra.canto", "Rock": "piedra.basalto", "RockLight": "piedra.caliza",
    "Metal": "metal.hierro", "Iron": "metal.hierro", "Rust": "metal.oxido", "Gold": "metal.oro",
    "Copper": "metal.cobre", "Steel": "metal.acero",
    "Cloth": "tela.lona", "Fabric": "tela.crudo", "Leather": "tela.cuero", "Rope": "palma.fibra",
    "Straw": "palma.paja", "Thatch": "palma.paja", "Bamboo": "bambu.maduro",
    "Bone": "recurso.hueso", "Shell": "recurso.concha", "Feather": "recurso.pluma",
    "Fur": "fauna.pardo", "Skin": "fauna.rosado", "Hoof": "fauna.pezuna", "Hooves": "fauna.pezuna",
    "Horn": "fauna.cuerno", "Beak": "fauna.pico", "Eye_Black": "fauna.pezuna",
    "Clay": "mineral.terracota", "Crystal": "mineral.cuarzo", "Aluminium": "mineral.aluminio",
    "Sand": "terreno.arena_seca", "Dirt": "terreno.arena_mojada", "Water": "entorno.laguna",
}

# ---------------------------------------------------------------------------
# Atlas
# ---------------------------------------------------------------------------

ATLAS = {
    "size": 512,
    "grid": 16,
    "cell_px": 32,
    "margin_px": 4,
    "ramp_px": 10,
    "flat_px": 4,
    "max_mip": 3,
    "free": "#808080",
}

# Degradado vertical (Oklab) respecto al color medio: arriba más claro y un punto cálido
# (luz de sol), abajo más oscuro y un punto frío (sombra de cielo). ΔL total ≈ 0.14:
# se lee volumen sin ensuciar.
TOP_SHIFT = (0.060, 0.0, 0.008)
BOTTOM_SHIFT = (-0.080, -0.002, -0.010)
TOP_CHROMA, BOTTOM_CHROMA = 1.04, 0.94


def _smooth(t):
    return t * t * (3.0 - 2.0 * t)


def graded(lab: np.ndarray, grade: tuple[float, float, float, float]) -> np.ndarray:
    dl, cmul, da, db = grade
    return np.array([lab[0] + dl, lab[1] * cmul + da, lab[2] * cmul + db])


def _ends(mid_lab: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    top = np.array([mid_lab[0] + TOP_SHIFT[0], mid_lab[1] * TOP_CHROMA + TOP_SHIFT[1],
                    mid_lab[2] * TOP_CHROMA + TOP_SHIFT[2]])
    bottom = np.array([mid_lab[0] + BOTTOM_SHIFT[0], mid_lab[1] * BOTTOM_CHROMA + BOTTOM_SHIFT[1],
                       mid_lab[2] * BOTTOM_CHROMA + BOTTOM_SHIFT[2]])
    return fit_albedo(top), fit_albedo(bottom)


def terrain_target_lab(material: str) -> np.ndarray:
    _, L, C, h, _ = TERRAIN_TARGETS[material]
    return lch(L, C, h)


def vertex_tint(lin: np.ndarray, vertex_rgb: tuple[int, int, int]) -> np.ndarray:
    """Réplica de M_Terrain: color *= lerp(1, vc / media(vc), 0.18)."""
    vc = np.asarray(vertex_rgb, dtype=np.float64) / 255.0
    hue = vc / max(vc.mean(), 0.03)
    return lin * (1.0 + (hue - 1.0) * 0.18)


def island_swatches(island: Island) -> list[dict]:
    """Muestras de una isla con su celda y colores (medio, arriba, abajo) ya cuantizados."""
    out = []
    for fam in FAMILIES:
        for col, (key, hexv, use) in enumerate(fam.swatches):
            lab = srgb_to_oklab(hex_to_srgb(hexv))
            if fam.graded:
                lab = fit_albedo(graded(lab, island.grade))
            out.append(_swatch(fam.key, key, use, col, fam.row, lab))
    for col, (mat, (key, *_rest, layer)) in enumerate(TERRAIN_TARGETS.items()):
        lin = vertex_tint(oklab_to_linear(terrain_target_lab(mat)), island.vertex[layer])
        out.append(_swatch("terreno", key, f"Terreno {mat} tal como lo tiñe M_Terrain en esta isla.",
                           col, TERRAIN_ROW, fit_albedo(linear_to_oklab(lin))))
    for col, (key, (lin, src)) in enumerate(ENTORNO.items()):
        out.append(_swatch("entorno", key, f"Referencia: {src}.", col, ENTORNO_ROW,
                           fit_albedo(linear_to_oklab(np.array(lin)))))
    return out


def _swatch(family: str, key: str, use: str, col: int, row: int, mid_lab: np.ndarray) -> dict:
    top, bottom = _ends(mid_lab)
    mid = quantize(oklab_to_srgb(mid_lab))
    return {"id": f"{family}.{key}", "family": family, "key": key, "use": use, "cell": (col, row),
            "mid": mid, "top": quantize(oklab_to_srgb(top)), "bottom": quantize(oklab_to_srgb(bottom))}


def cell_profile(sw: dict) -> np.ndarray:
    """Columna de 32 px (sRGB) de una celda: margen, rampa, banda plana, rampa, margen.
    Las rampas interpolan en Oklab con suavizado (derivada nula en la banda plana)."""
    a = ATLAS
    m, r, f = a["margin_px"], a["ramp_px"], a["flat_px"]
    top, mid, bottom = (srgb_to_oklab(sw[k]) for k in ("top", "mid", "bottom"))
    rows = [top] * m
    for i in range(r):                        # i = 0 -> top exacto
        rows.append(top + (mid - top) * _smooth(i / r))
    rows += [mid] * f
    for i in range(1, r + 1):                 # i = r -> bottom exacto
        rows.append(mid + (bottom - mid) * _smooth(i / r))
    rows += [bottom] * m
    prof = quantize(oklab_to_srgb(np.array(rows)))
    # Los extremos y la banda central son los valores exactos del JSON (sin error de ida y vuelta).
    prof[: m + 1] = sw["top"]
    prof[m + r: m + r + f] = sw["mid"]
    prof[-(m + 1):] = sw["bottom"]
    return prof


def build_atlas(swatches: list[dict]) -> np.ndarray:
    """Atlas (size, size, 3) sRGB en [0, 1], cuantizado a 8 bits. Determinista."""
    a = ATLAS
    size, cell = a["size"], a["cell_px"]
    img = np.empty((size, size, 3), dtype=np.float64)
    img[:] = hex_to_srgb(a["free"])
    for sw in swatches:
        col, row = sw["cell"]
        img[row * cell:(row + 1) * cell, col * cell:(col + 1) * cell] = cell_profile(sw)[:, None, :]
    return img


def cell_uv(col: int, row: int) -> dict:
    a = ATLAS
    size, cell, m = a["size"], a["cell_px"], a["margin_px"]
    return {"u": (col * cell + cell / 2) / size, "v": (row * cell + cell / 2) / size,
            "v_min": (row * cell + m) / size, "v_max": ((row + 1) * cell - m) / size}


def palette_texture_name(island: Island) -> str:
    return f"T_Palette_{island.key}"


PALETTE_TEXTURES = tuple(f"T_Palette_{i.key}" for i in ISLANDS)


def to_json() -> dict:
    """Contenido de Tools/Textures/paleta.json (determinista, 4 decimales en lineal)."""
    def col(c):
        return {"srgb": srgb_to_hex(c), "lineal": [round(float(x), 4) for x in srgb_to_linear(c)]}

    islands = {}
    for isl in ISLANDS:
        colors = {}
        for sw in island_swatches(isl):
            uv = cell_uv(*sw["cell"])
            colors[sw["id"]] = {
                "celda": list(sw["cell"]),
                "uv": [round(uv["u"], 6), round(uv["v"], 6)],
                "v_rango": [round(uv["v_min"], 6), round(uv["v_max"], 6)],
                "medio": col(sw["mid"]), "arriba": col(sw["top"]), "abajo": col(sw["bottom"]),
            }
        islands[isl.key] = {
            "nombre": isl.name, "arquetipo": isl.archetype, "textura": palette_texture_name(isl),
            "ambiente": isl.mood,
            "grado_oklab": dict(zip(("dL", "croma", "da", "db"), isl.grade)),
            "color_vertice_terreno": {k: list(v) for k, v in isl.vertex.items()},
            "colores": colors,
        }
    families = {f.key: {"fila": f.row, "por_isla": f.graded, "acento": f.accent, "recogible": f.pickup,
                        "descripcion": f.desc,
                        "muestras": {k: use for k, _, use in f.swatches}} for f in FAMILIES}
    families["terreno"] = {"fila": TERRAIN_ROW, "por_isla": True, "acento": False, "recogible": False,
                           "descripcion": "Tono medio del terreno ya teñido por M_Terrain en cada isla.",
                           "muestras": {k: m for m, (k, *_r) in TERRAIN_TARGETS.items()}}
    families["entorno"] = {"fila": ENTORNO_ROW, "por_isla": False, "acento": False, "recogible": False,
                           "descripcion": "Agua y cielo actuales (solo referencia).",
                           "muestras": {k: src for k, (_, src) in ENTORNO.items()}}
    return {
        "version": 1,
        "generador": "Tools/Textures/gen_palette.py (fuente: Tools/Textures/texgen/palette.py)",
        "espacio": "sRGB (hex) y lineal; grados y degradados en Oklab",
        "atlas": {**ATLAS,
                  "filtrado": "bilineal; mips TMGS_SimpleAverage; sin compresión por bloques "
                              "(TC_EditorIcon); M_LowPoly limita el mip a max_mip",
                  "celda": "4 px margen, 10 px rampa arriba->medio, 4 px color medio exacto, "
                           "10 px rampa medio->abajo, 4 px margen",
                  "uv": "u = centro de columna; v dentro de v_rango (centro = color medio exacto)"},
        "tope_croma_entorno": CHROMA_CAP,
        "terreno_objetivo": {m: {"muestra": f"terreno.{k}", "oklab_lch": [L, C, h], "capa_vertice": layer,
                                 "srgb": srgb_to_hex(quantize(oklab_to_srgb(lch(L, C, h))))}
                             for m, (k, L, C, h, layer) in TERRAIN_TARGETS.items()},
        "familias": families,
        "alias_packs": PACK_ALIASES,
        "islas": islands,
    }


# ---------------------------------------------------------------------------
# Armonización del terreno
# ---------------------------------------------------------------------------

def harmonize_albedo(albedo: np.ndarray, material: str) -> np.ndarray:
    """Desplaza el color medio (Oklab L, a, b) de un albedo al objetivo de la paleta y
    conserva el detalle (las desviaciones respecto a la media). Operación por píxel: no
    altera el tileado. Se recorta al rango de albedo del proyecto."""
    lab = srgb_to_oklab(albedo[..., :3])
    mean = lab.reshape(-1, 3).mean(axis=0)
    target = terrain_target_lab(material)
    out = oklab_to_srgb(lab + (target - mean))
    out = np.clip(out, ALBEDO_MIN, ALBEDO_MAX)
    # El recorte (y la no linealidad) mueve un poco la media: una segunda pasada la corrige.
    lab2 = srgb_to_oklab(out)
    out = np.clip(oklab_to_srgb(lab2 + (target - lab2.reshape(-1, 3).mean(axis=0))), ALBEDO_MIN, ALBEDO_MAX)
    if albedo.shape[-1] == 4:
        out = np.concatenate([out, albedo[..., 3:]], axis=-1)
    return out
