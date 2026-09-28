"""Descarga modelos concretos alojados en Poly Pizza, elegidos a mano tras revisar
miniaturas por especie (ver Tools/Packs/packs.json para el criterio de seleccion y
Tools/Packs/creditos_cc_by.md para la atribucion obligatoria de los CC-BY).

Politica (GDD v2 SS7.1, ampliada 2026-09-28 por el director para vegetacion tropical):
preferir CC0; cuando no hay una especie tropical reconocible en CC0 -caso de los
arboles de dosel selvatico: Quaternius solo tiene arboles redondos de aspecto
templado/europeo en Poly Pizza-, se acepta CC-BY 3.0 con atribucion. Este script
verifica AUTOR y LICENCIA exactos de cada modelo (no solo que sea CC0) antes de
descargarlo, y falla alto y claro si algo no cuadra: nunca descarga en silencio un
asset de otro autor o con otra licencia de la que se registro en MODELS.

Poly Pizza no exige API key para el sitio en si (solo para su API REST): la
pagina de cada modelo incrusta el JSON de estado del servidor
(window.__SERVER_APP_STATE__) con la licencia, el autor y la URL directa del
.glb en static.poly.pizza. Este script scrapea esa pagina.

Uso: uv run python Tools/Packs/download_polypizza.py [--force]
"""
from __future__ import annotations

import argparse
import json
import re
import sys
import urllib.request
from pathlib import Path
from typing import NamedTuple

CACHE_DIR = Path(__file__).resolve().parent / ".cache" / "quaternius-polypizza"
HEADERS = {"User-Agent": "Explored-Tools/1.0 (contacto: proyecto Explored, licencia verificada a mano)"}


class PolyPizzaModel(NamedTuple):
    filename: str
    slot: str
    creator: str
    """Autor exacto reportado por Poly Pizza (comparacion insensible a mayusculas)."""
    licence_prefix: str
    """Prefijo esperado del campo licence ("CC0" o "CC-BY")."""


# publicID de Poly Pizza -> especificacion del modelo. Cada especie fue elegida
# revisando su miniatura (ver Tools/Packs/mapping.md) contra la silueta real de la
# especie tropical que sustituye; el porque de cada eleccion vive en el comentario
# de su bloque y en packs.json.
MODELS: dict[str, PolyPizzaModel] = {
    # --- Palmeras cocoteras: 15-25 m, 4 variantes (Quaternius, CC0) ---------
    "A6cKJYFsIb": PolyPizzaModel("palm_a.glb", "Palm", "Quaternius", "CC0"),
    "DsrrAYmucG": PolyPizzaModel("palm_b.glb", "Palm", "Quaternius", "CC0"),
    "P0tgwyXBgr": PolyPizzaModel("palm_c.glb", "Palm", "Quaternius", "CC0"),
    "nr1B5DbICA": PolyPizzaModel("palm_d.glb", "Palm", "Quaternius", "CC0"),
    # --- Dosel alto de selva (JungleWide), 2026-09-28 ------------------------
    # Sustituyen SM_JungleTreeWide_01 (procedural) y las 4 "Tree" de Quaternius
    # via Poly Pizza (i4QMw4L64D, qZtx0AHhcy, 9nvGuZlbpE, YWjGDJ9F7g: copas
    # redondas en bola sobre tronco en Y, aspecto de roble de jardin) que el
    # director marco como "europeo/templado" el 2026-09-28. Quaternius no tiene
    # en Poly Pizza ninguna especie de dosel tropical reconocible (solo copas
    # redondas genericas): estas 4 son CC-BY 3.0 de la libreria historica
    # "Poly by Google" y de Zacharylll, con nombre de especie real o silueta de
    # dosel plano con lianas -exactamente lo que pidio el director-. Ver
    # Tools/Packs/creditos_cc_by.md para el texto de atribucion obligatorio.
    "dlW4hGKBpiS": PolyPizzaModel("jungle_wide_a.glb", "JungleWide", "Zacharylll", "CC-BY"),
    # ^ "Vine Covered Tree": copa ancha y plana con lianas colgando y contrafuertes
    #   visibles en la base -la pieza que mejor cubre "dosel alto y copa plana,
    #   lianas y epifitas" del brief-.
    "2PolZJUAMmk": PolyPizzaModel("jungle_wide_b.glb", "JungleWide", "Poly by Google", "CC-BY"),
    # ^ "Rubber tree" (Hevea brasiliensis): especie real de selva, copa bulbosa.
    "2EW209K0xBw": PolyPizzaModel("jungle_wide_c.glb", "JungleWide", "Poly by Google", "CC-BY"),
    # ^ "Macassar tree" (ebano de Macasar): especie real, tronco en Y.
    "6pwiq7hSrHr": PolyPizzaModel("jungle_wide_d.glb", "JungleWide", "Poly by Google", "CC-BY"),
    # ^ "Tree": copa ancha y achatada generica de la misma libreria; variedad de
    #   silueta sin repetir especie ya usada.
    # --- Gigantes del dosel (JungleGiant), 2026-09-28 ------------------------
    # Sustituyen SM_JungleTreeGiant_01 (procedural). "Gigante -> arbol grande
    # CC0/CC-BY" (brief del director): estas dos son especies reales de
    # emergentes de selva (mas altas que el dosel medio), CC-BY 3.0.
    "0CDi5mHR26U": PolyPizzaModel("jungle_giant_a.glb", "JungleGiant", "Poly by Google", "CC-BY"),
    # ^ "Brazil nut tree" (Bertholletia excelsa): emergente real de la Amazonia.
    "ekCgu7iLrz2": PolyPizzaModel("jungle_giant_b.glb", "JungleGiant", "Poly by Google", "CC-BY"),
    # ^ "Balsa tree" (Ochroma pyramidale): especie real de crecimiento rapido.
    # --- Sotobosque alto (Understory), 2026-09-28 ----------------------------
    # Sustituyen SM_JungleTreeUnderstory_01 (procedural). Brief: "sotobosque ->
    # JungleWide/arbustos grandes". UnderstoryA reusa el mismo glb que
    # JungleWideA pero se exporta mas bajo (ver process_landing_set.py); el
    # bambu (Quaternius, CC0) es tambien sotobosque/medio dosel real.
    "xBPj13w3JQ": PolyPizzaModel("understory_bamboo.glb", "Understory", "Quaternius", "CC0"),
    # --- Manglar, 2026-09-28 --------------------------------------------------
    # Sustituyen SM_JungleTreeMangrove_01 (procedural, tronco cuadrado).
    "8qABc2Nslz": PolyPizzaModel("mangrove_a.glb", "Mangrove", "Quaternius", "CC0"),
    # ^ "Tree": tronco en Y con dos copas separadas (silueta de manglar), unico
    #   resultado CC0 con esa forma (ver busqueda "mangrove tree"/"root tree").
    "eYfjQLsebfA": PolyPizzaModel("mangrove_roots.glb", "Mangrove", "Poly by Google", "CC-BY"),
    # ^ "Tree roots": raiz zancuda/arqueada aislada, para plantarla junto al
    #   tronco (Mangrove_A) o sola como detalle de orilla; sin esto ninguna
    #   malla de manglar mostraba raices.
    # --- Arbustos y plantas de sotobosque, 2026-09-28 ------------------------
    # Sustituyen shrub_a/shrub_b (bolas de seto generico), shrub_flowering
    # (arbusto con flor lila de jardin templado) y shrub_banana (que en
    # realidad era la FRUTA suelta, no la planta): todas marcadas "europeo" o
    # incorrectas por el director el 2026-09-28.
    "d0WJSiuOz6o": PolyPizzaModel("shrub_banana.glb", "Shrub", "Poly by Google", "CC-BY"),
    # ^ "Banana Tree": planta real (hojas grandes), sustituye la fruta suelta.
    "kZQ2WmnJFI": PolyPizzaModel("shrub_monstera.glb", "Shrub", "Isa Lousberg", "CC0"),
    # ^ "Large Monstera Plant": CC0, sin maceta (lista para plantar en el suelo).
    "06_hk2bO2Ix": PolyPizzaModel("shrub_heliconia.glb", "Shrub", "Poly by Google", "CC-BY"),
    # ^ "Heliconia flower": bracteas rojo/amarillo reconocibles, sin maceta.
    "5FZIGjZBWTB": PolyPizzaModel("shrub_bromeliad.glb", "Shrub", "Poly by Google", "CC-BY"),
    # ^ "Bromeliad": roseta epifita de suelo, sin maceta.
    "MbhbP7JrTI": PolyPizzaModel("shrub_pineapple_top.glb", "Shrub", "Quaternius", "CC0"),
    # ^ "Plant Big": roseta puntiaguda verde/naranja (tipo piña/bromelia), CC0,
    #   sin maceta; variedad de silueta frente a Bromeliad.
    "FUgtfvqgMx": PolyPizzaModel("shrub_bamboo.glb", "Shrub", "Quaternius", "CC0"),
    # ^ "Bamboo": segunda variante de bambu (distinta de understory_bamboo),
    #   como mata de sotobosque en vez de vara aislada.
    # --- Helechos de suelo -----------------------------------------------------
    "jqcanvH7D6": PolyPizzaModel("fern_a.glb", "Fern", "Quaternius", "CC0"),
    # xH5gNlQxAZ ("Plant", Quaternius) se retira: pese al nombre "Fern" en la
    # busqueda, el modelo real es un racimo de petalos morados sin relacion con
    # un helecho (ver Tools/Packs/mapping.md). No se encontro un segundo
    # helecho CC0/CC-BY adecuado: queda como limitacion conocida.
    # --- Hierba en matas (alta, no cesped de prado) --------------------------
    "JSIYtscPmP": PolyPizzaModel("grass_a.glb", "Grass", "Quaternius", "CC0"),
    "vUJjrRsFp4": PolyPizzaModel("grass_b.glb", "Grass", "Quaternius", "CC0"),
    "UGTOzcO3P2": PolyPizzaModel("grass_c.glb", "Grass", "Quaternius", "CC0"),
    # iw6l7gqcdQ ("Grass", Quaternius) se retira: el render de su pagina en
    # Poly Pizza sale en tonos negro/morado irreconocibles como hierba (posible
    # fallo del generador de miniaturas del sitio); sin poder confirmar visualmente
    # la silueta no se usa.
    # --- Flores --------------------------------------------------------------
    # Flower_A/B/C (NBUxHir6FJ, hfPzQAedOe, dOO6kMDd8L; Quaternius, CC0) se
    # retiran: son margaritas/flores de prado de jardin templado, exactamente lo
    # que el brief pide quitar. Unico sustituto tropical encontrado sin maceta:
    "fQVeG1obY8u": PolyPizzaModel("flower_hibiscus.glb", "Flower", "Poly by Google", "CC-BY"),
    # ^ "Hibiscus flower": flor grande reconocible, sin maceta ni tallo de tiesto.
    # Bird of paradise (folDA1yLeEF) y el resto de "Houseplant" de Quaternius se
    # descartaron: todos vienen con maceta modelada (no aptos para plantarlos
    # sueltos en el terreno sin editar el mesh en Blender, fuera de alcance aqui).
}

# Especies evaluadas y NO usadas, con el motivo (mismo patron que packs.json ->
# discarded): se documentan para no repetir la busqueda ni perder el porque.
DISCARDED: dict[str, str] = {
    "i4QMw4L64D": "JungleWide/Tree (Quaternius): copa en bola sobre tronco en Y, aspecto de roble de jardin templado.",
    "qZtx0AHhcy": "JungleWide/Tree (Quaternius): idem, copa de hojas sueltas pero silueta de arbol de jardin.",
    "9nvGuZlbpE": "JungleWide/Tree (Quaternius): tronco en Y con 3 bolas de copa, aspecto de arbol de jardin.",
    "YWjGDJ9F7g": "JungleWide/Tree (Quaternius): tronco desnudo con ramas dispersas, lee como arbol muerto/templado.",
    "ooG6CkLyE8": "Shrub/Bush (Quaternius): bola de seto generico, sin especie reconocible.",
    "92EytlU1El": "Shrub/Bush (Quaternius): bola de seto lisa (topiary), aspecto de jardin.",
    "U1ymDy8tbY": "Shrub/Bush with Flowers (Quaternius): flor lila tipo lila/hortensia de jardin templado.",
    "ruOFtE0B6Z": "Shrub/Banana (Quaternius): es la fruta suelta, no la planta; sustituida por shrub_banana.glb (Poly by Google).",
    "xH5gNlQxAZ": "Fern/Plant (Quaternius): racimo de petalos morados, no es un helecho pese al nombre de busqueda.",
    "iw6l7gqcdQ": "Grass (Quaternius): miniatura en negro/morado irreconocible como hierba.",
    "NBUxHir6FJ": "Flower/Flowers (Quaternius): margaritas de prado.",
    "hfPzQAedOe": "Flower/Flower Group (Quaternius): flores de prado variadas.",
    "dOO6kMDd8L": "Flower/Flowers (Quaternius): margaritas de prado, igual que NBUxHir6FJ.",
    "folDA1yLeEF": "Birds of paradise potted plant (Poly by Google): viene con maceta modelada, no apta para plantar suelta.",
}


def _normalize_licence(text: str) -> str:
    return text.upper().replace(" ", "").replace("-", "")


def fetch(url: str) -> bytes:
    req = urllib.request.Request(url, headers=HEADERS)
    with urllib.request.urlopen(req, timeout=30) as resp:
        return resp.read()


def resolve_model(public_id: str) -> dict:
    html = fetch(f"https://poly.pizza/m/{public_id}").decode("utf-8", errors="replace")
    m = re.search(r'"static\.poly\.pizza\\?/([0-9a-fA-F-]+\.glb)"', html)
    if not m:
        # el iframe de modelviewer lleva la url sin escapar de barras
        m2 = re.search(r'https://static\.poly\.pizza/([0-9a-fA-F-]+\.glb)', html)
        glb_name = m2.group(1) if m2 else None
    else:
        glb_name = m.group(1)
    if not glb_name:
        raise RuntimeError(f"{public_id}: no se encontro URL .glb en la pagina del modelo")

    licence_m = re.search(r'"Licence":"([^"]+)"', html)
    creator_m = re.search(r'"Creator":\{"Username":"([^"]+)"', html)
    title_m = re.search(r'"Title":"([^"]+)"', html)
    return {
        "glb_url": f"https://static.poly.pizza/{glb_name}",
        "licence": licence_m.group(1) if licence_m else "DESCONOCIDA",
        "creator": creator_m.group(1) if creator_m else "DESCONOCIDO",
        "title": title_m.group(1) if title_m else public_id,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()

    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    report = []
    failures = []
    for public_id, model in MODELS.items():
        dest = CACHE_DIR / model.filename
        try:
            info = resolve_model(public_id)
        except Exception as exc:
            print(f"[fail] {public_id}: no se pudo resolver la pagina ({exc})")
            failures.append(public_id)
            continue

        if info["creator"].strip().lower() != model.creator.strip().lower():
            print(f"[fail] {public_id}: autor inesperado '{info['creator']}' (se esperaba '{model.creator}') -> SE OMITE")
            failures.append(public_id)
            continue
        if not _normalize_licence(info["licence"]).startswith(_normalize_licence(model.licence_prefix)):
            print(f"[fail] {public_id}: licencia '{info['licence']}' no coincide con '{model.licence_prefix}' -> SE OMITE")
            failures.append(public_id)
            continue

        if dest.exists() and not args.force:
            print(f"[skip] {public_id} ({model.slot}): ya en cache -> {dest.name}")
        else:
            data = fetch(info["glb_url"])
            dest.write_bytes(data)
            print(f"[ok  ] {public_id} ({model.slot}): '{info['title']}' {info['licence']}, {len(data)/1024:.0f} KB -> {dest.name}")

        report.append({"public_id": public_id, "slot": model.slot, "file": model.filename, **info})

    report_path = CACHE_DIR / "resolved.json"
    report_path.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"\n[ok] {len(report)}/{len(MODELS)} modelos resueltos. Detalle en {report_path}")
    if failures:
        print(f"[warn] fallaron: {failures}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
