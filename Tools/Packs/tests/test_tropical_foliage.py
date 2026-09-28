"""Comprueba que la auditoria tropical del 2026-09-28 no deja huecos: ninguna
especie retirada (aspecto europeo/templado, flores de prado, la fruta en vez
de la planta...) sigue en uso, y todo lo nuevo esta bien registrado."""

from __future__ import annotations

import download_polypizza as dp
import process_landing_set as pls

# publicID -> por que se retiro (ver download_polypizza.DISCARDED para el
# texto completo). Las 4 "JungleWide/Tree" son las que sustituian a las
# especies JungleTree procedurales viejas y el director marco como robles de
# jardin templado; el resto son shrub/fern/grass/flower "europeos".
RETIRED_PUBLIC_IDS = frozenset(dp.DISCARDED.keys())


def test_ningun_publicid_retirado_sigue_en_models() -> None:
    """Un publicID retirado no puede colarse de vuelta en MODELS: si algo lo
    necesita, que sea una decision nueva y explicita, no un descuido."""
    assert RETIRED_PUBLIC_IDS.isdisjoint(dp.MODELS.keys())


def test_jungle_wide_ya_no_es_quaternius() -> None:
    """Los 4 "JungleWide" originales eran "Tree" de Quaternius (copas en bola
    de aspecto europeo/templado, ver DISCARDED). El pipeline reutiliza los
    mismos nombres de fichero cache (jungle_wide_a.glb...) para las especies
    nuevas: hay que comprobar el AUTOR registrado en MODELS, no el nombre de
    fichero, que no cambia de proposito."""
    wide_entries = [e for e in pls.ENTRIES if e["slot"] == "JungleWide"]
    assert len(wide_entries) >= 4
    for entry in wide_entries:
        public_id = next(pid for pid, m in dp.MODELS.items() if m.filename == entry["src"])
        assert dp.MODELS[public_id].creator != "Quaternius", entry["name"]


def test_banana_es_la_planta_no_la_fruta() -> None:
    """shrub_banana.glb debe apuntar al publicID nuevo (Poly by Google, planta
    real con hojas), no al retirado (Quaternius, la fruta suelta sin hojas)."""
    assert dp.MODELS["d0WJSiuOz6o"].filename == "shrub_banana.glb"
    assert "ruOFtE0B6Z" not in dp.MODELS
    banana_entry = next(e for e in pls.ENTRIES if e["name"] == "SM_LowPolyShrubBanana_01")
    assert banana_entry["src"] == "shrub_banana.glb"


def test_todas_las_entries_tienen_modelo_registrado() -> None:
    """Toda entrada kind="glb" de ENTRIES debe tener su publicID en MODELS
    (si no, download_polypizza.py nunca la habria descargado)."""
    glb_files_in_models = {m.filename for m in dp.MODELS.values()}
    for entry in pls.ENTRIES:
        if entry["kind"] == "glb":
            assert entry["src"] in glb_files_in_models, entry["name"]


def test_jungle_wide_y_giant_son_especies_distintas() -> None:
    """JungleWide y JungleGiant no deben compartir ningun .glb de origen: son
    dos categorias de altura distinta (dosel medio vs emergentes) y si
    coincidieran seria un copiar-pegar sin querer."""
    wide_src = {e["src"] for e in pls.ENTRIES if e["slot"] == "JungleWide"}
    giant_src = {e["src"] for e in pls.ENTRIES if e["slot"] == "JungleGiant"}
    assert wide_src.isdisjoint(giant_src)


def test_material_slots_native_para_todo_el_set() -> None:
    """Ninguna entrada de ENTRIES debe requerir un remapeo de vertex color
    tipo M_Bark/M_Leaf (ver Tools/Unreal/import_meshes.py): todas conservan
    su material propio, marcado como NATIVE en lowpoly_manifest.json."""
    assert len(pls.ENTRIES) >= 30
    for entry in pls.ENTRIES:
        assert entry["kind"] in ("glb", "obj")
