"""Comprueba que todas las mallas de /Game/Art conservan sus colores tras ampliar la paleta.

Para cada malla de los kits lee el UV 0 de todas sus instancias de vértice, lo convierte en celda de
la hoja T_Paleta (32 x 3: índices 0-31 en la fila central, 32-63 en la superior; ver
Tools/Style/palette.py) y compara el conjunto de índices con los colores del manifest de su kit
(Art/Export/K*/manifest.json, campo «colores»). Comprueba además que T_Paleta mide 32 x 3 y que el
terreno no tiene UV fuera de celda. Falla si alguna malla mapea a una fila o celda distinta.
Resumen en Saved/Logs/check_palette_uv.json.

Uso: Tools/unreal_python.ps1 -Script Tools/Unreal/check_palette_uv.py
"""

import glob
import json
import math
import os

import unreal

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
PALETTE_JSON = os.path.join(REPO_ROOT, "Tools", "Style", "paleta.json")
SUMMARY = os.path.join(REPO_ROOT, "Saved", "Logs", "check_palette_uv.json")
COLUMNS, ROWS, BASE_ROW, EXTRA_ROW = 32, 3, 1, 0


def cell_index(u, v):
    """Índice de paleta de una UV de Unreal (v = 0 arriba), o None si cae en un borde o en la fila de copia."""
    col_f, row_f = u * COLUMNS, v * ROWS
    col, row = math.floor(col_f), math.floor(row_f)
    if not (0.25 <= col_f - col <= 0.75 and 0.25 <= row_f - row <= 0.75) or not 0 <= col < COLUMNS:
        return None
    if row == BASE_ROW:
        return col
    return col + COLUMNS if row == EXTRA_ROW else None


def mesh_indices(mesh):
    desc = mesh.get_static_mesh_description(0)
    found, bad = set(), 0
    for i in range(desc.get_vertex_instance_count()):
        uv = desc.get_vertex_instance_uv(unreal.VertexInstanceID(i), 0)
        index = cell_index(uv.x, uv.y)
        if index is None:
            bad += 1
        else:
            found.add(index)
    return found, bad


def main():
    with open(PALETTE_JSON, "r", encoding="utf-8") as f:
        by_name = {c["nombre"]: c["indice"] for c in json.load(f)["colores"]}
    texture = unreal.load_asset("/Game/Art/Materials/T_Paleta")
    size = (texture.blueprint_get_size_x(), texture.blueprint_get_size_y())
    errors = [] if size == (COLUMNS, ROWS) else [f"T_Paleta mide {size}, se espera {(COLUMNS, ROWS)}"]
    report = {"T_Paleta": list(size), "mallas": {}}
    for path in sorted(glob.glob(os.path.join(REPO_ROOT, "Art", "Export", "K*", "manifest.json"))):
        kit = os.path.basename(os.path.dirname(path))
        with open(path, "r", encoding="utf-8") as f:
            manifest = json.load(f)
        for entry in manifest["mallas"]:
            asset = f"/Game/Art/{kit}/{entry['malla']}"
            mesh = unreal.load_asset(asset)
            if mesh is None:
                errors.append(f"falta {asset}")
                continue
            found, bad = mesh_indices(mesh)
            want = {by_name[c] for c in entry["colores"]}
            report["mallas"][entry["malla"]] = {"indices": sorted(found), "esperados": sorted(want), "uv_malas": bad}
            if bad or found != want:
                errors.append(f"{entry['malla']}: índices {sorted(found)} frente a {sorted(want)} ({bad} UV fuera de celda)")
    terrain = unreal.load_asset("/Game/Art/Island/SM_IslaSal_Terrain")
    if terrain is not None:
        found, bad = mesh_indices(terrain)
        report["mallas"]["SM_IslaSal_Terrain"] = {"indices": sorted(found), "uv_malas": bad}
        if bad:
            errors.append(f"SM_IslaSal_Terrain: {bad} UV fuera de celda")
    report["errores"] = errors
    os.makedirs(os.path.dirname(SUMMARY), exist_ok=True)
    with open(SUMMARY, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=1, ensure_ascii=False)
    unreal.log(f"[check_palette_uv] {len(report['mallas'])} mallas, {len(errors)} errores -> {SUMMARY}")
    for e in errors:
        unreal.log_error(f"[check_palette_uv] {e}")
    if errors:
        raise RuntimeError(f"{len(errors)} mallas con colores cambiados")


main()
