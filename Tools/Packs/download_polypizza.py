"""Descarga modelos concretos de Quaternius alojados en Poly Pizza (CC0),
elegidos a mano tras revisar una hoja de contacto de miniaturas (ver
Tools/Packs/packs.json para el criterio de seleccion).

Poly Pizza no exige API key para el sitio en si (solo para su API REST): la
pagina de cada modelo incrusta el JSON de estado del servidor
(window.__SERVER_APP_STATE__) con la licencia, el autor y la URL directa del
.glb en static.poly.pizza. Este script scrapea esa pagina, verifica que el
autor sea Quaternius y la licencia sea CC0 antes de descargar, y falla alto
y claro si algo no cuadra (nunca descarga en silencio un asset con otra
licencia o de otro autor).

Uso: uv run python Tools/Packs/download_polypizza.py [--force]
"""
from __future__ import annotations

import argparse
import json
import re
import sys
import urllib.request
from pathlib import Path

CACHE_DIR = Path(__file__).resolve().parent / ".cache" / "quaternius-polypizza"
HEADERS = {"User-Agent": "Explored-Tools/1.0 (contacto: proyecto Explored, uso CC0 verificado a mano)"}

# publicID de Poly Pizza -> (nombre de fichero local, slot al que se destina).
# Elegidos a mano el 2026-09-27 tras generar una hoja de contacto con las
# miniaturas de ~110 resultados de busqueda filtrados por creador=Quaternius
# (ver Tools/Packs/packs.json -> "seleccion"). Todos CC0 1.0 verificados por
# este mismo script antes de bajarlos.
MODELS: dict[str, tuple[str, str]] = {
    # --- Palmeras (15-25 m tras normalizar) ---------------------------------
    "A6cKJYFsIb": ("palm_a.glb", "Palm"),
    "DsrrAYmucG": ("palm_b.glb", "Palm"),
    "P0tgwyXBgr": ("palm_c.glb", "Palm"),
    "nr1B5DbICA": ("palm_d.glb", "Palm"),
    # --- Arboles de selva de copa ancha --------------------------------------
    "i4QMw4L64D": ("jungle_wide_a.glb", "JungleWide"),
    "qZtx0AHhcy": ("jungle_wide_b.glb", "JungleWide"),
    "9nvGuZlbpE": ("jungle_wide_c.glb", "JungleWide"),
    "YWjGDJ9F7g": ("jungle_wide_d.glb", "JungleWide"),
    # --- Arbustos --------------------------------------------------------------
    "ooG6CkLyE8": ("shrub_a.glb", "Shrub"),
    "92EytlU1El": ("shrub_b.glb", "Shrub"),
    "U1ymDy8tbY": ("shrub_flowering.glb", "Shrub"),
    "ruOFtE0B6Z": ("shrub_banana.glb", "Shrub"),
    # --- Helechos / plantas de sotobosque -------------------------------------
    "jqcanvH7D6": ("fern_a.glb", "Fern"),
    "xH5gNlQxAZ": ("fern_b.glb", "Fern"),
    # --- Hierba en matas -------------------------------------------------------
    "iw6l7gqcdQ": ("grass_a.glb", "Grass"),
    "JSIYtscPmP": ("grass_b.glb", "Grass"),
    "vUJjrRsFp4": ("grass_c.glb", "Grass"),
    # --- Flores ------------------------------------------------------------
    "NBUxHir6FJ": ("flower_a.glb", "Flower"),
    "hfPzQAedOe": ("flower_b.glb", "Flower"),
    "dOO6kMDd8L": ("flower_c.glb", "Flower"),
}


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
    for public_id, (filename, slot) in MODELS.items():
        dest = CACHE_DIR / filename
        try:
            info = resolve_model(public_id)
        except Exception as exc:
            print(f"[fail] {public_id}: no se pudo resolver la pagina ({exc})")
            failures.append(public_id)
            continue

        if info["creator"].lower() != "quaternius":
            print(f"[fail] {public_id}: autor inesperado '{info['creator']}' (se esperaba Quaternius) -> SE OMITE")
            failures.append(public_id)
            continue
        if not info["licence"].upper().startswith("CC0"):
            print(f"[fail] {public_id}: licencia '{info['licence']}' no es CC0 -> SE OMITE")
            failures.append(public_id)
            continue

        if dest.exists() and not args.force:
            print(f"[skip] {public_id} ({slot}): ya en cache -> {dest.name}")
        else:
            data = fetch(info["glb_url"])
            dest.write_bytes(data)
            print(f"[ok  ] {public_id} ({slot}): '{info['title']}' CC0, {len(data)/1024:.0f} KB -> {dest.name}")

        report.append({"public_id": public_id, "slot": slot, "file": filename, **info})

    report_path = CACHE_DIR / "resolved.json"
    report_path.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"\n[ok] {len(report)}/{len(MODELS)} modelos resueltos. Detalle en {report_path}")
    if failures:
        print(f"[warn] fallaron: {failures}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
