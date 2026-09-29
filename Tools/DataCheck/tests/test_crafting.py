"""Tests de la réplica de UCraftingLibrary con catálogos sintéticos pequeños (rápidos y deterministas)."""

from __future__ import annotations

from typing import Any

from datacheck import crafting


def _item(iid: str, tags: list[str] | None = None, **props: float) -> dict[str, Any]:
    return {"id": iid, "tags": tags or [], "properties": [{"name": k, "value": v} for k, v in props.items()]}


def _tpl(tid: str, result: str, slots: list[dict[str, Any]], verbs: list[str] | None = None,
         **extra: Any) -> dict[str, Any]:
    return {"id": tid, "resultDefinitionId": result, "verbs": verbs or ["Atar"], "slots": slots, **extra}


def _slot(role: str, *reqs: tuple[str, float], require_all: bool = False,
          tags: list[str] | None = None) -> dict[str, Any]:
    slot: dict[str, Any] = {"role": role, "requirements": [{"property": p, "min": m} for p, m in reqs]}
    if require_all:
        slot["requireAll"] = True
    if tags:
        slot["tags"] = tags
    return slot


def test_prop_ausente_vale_cero() -> None:
    inst = crafting.leaf(_item("palo", Largo=3))
    assert inst.prop("Largo") == 3.0
    assert inst.prop("Filo") == 0.0


def test_slot_basta_un_requisito_salvo_require_all() -> None:
    palo = crafting.leaf(_item("palo", Largo=3, Rigido=1))
    uno = _slot("Mango", ("Largo", 2), ("Rigido", 2))
    todos = _slot("Mango", ("Largo", 2), ("Rigido", 2), require_all=True)
    assert crafting.slot_satisfied(uno, palo)
    assert not crafting.slot_satisfied(todos, palo)


def test_slot_con_etiquetas_exige_alguna_propia() -> None:
    slot = _slot("Cuerpo", tags=["piedra"])
    assert crafting.slot_satisfied(slot, crafting.leaf(_item("canto", ["piedra"])))
    assert not crafting.slot_satisfied(slot, crafting.leaf(_item("hoja", ["planta"])))


def test_slot_sin_requisitos_acepta_cualquiera() -> None:
    assert crafting.slot_satisfied({"role": "Comodin"}, crafting.leaf(_item("x")))


def test_combinar_hereda_el_maximo_y_suma_profundidad() -> None:
    a = crafting.leaf(_item("lasca", Filo=3, Rigido=1))
    b = crafting.leaf(_item("palo", Largo=3, Rigido=2))
    out = crafting.combine(_item("hacha", ["herramienta"], Rigido=1), a, b)
    assert out.definition == "hacha"
    assert dict(out.props) == {"Filo": 3.0, "Largo": 3.0, "Rigido": 2.0}
    assert out.tags == frozenset({"herramienta"}) and out.depth == 1


def test_gana_la_plantilla_con_mas_slots_y_a_igualdad_la_primera() -> None:
    a = crafting.leaf(_item("a", Largo=3))
    b = crafting.leaf(_item("b", Ata=3))
    uno = _tpl("uno", "x", [_slot("L", ("Largo", 1))])
    dos = _tpl("dos", "y", [_slot("L", ("Largo", 1)), _slot("A", ("Ata", 1))])
    dos_bis = _tpl("dos_bis", "z", [_slot("L", ("Largo", 1)), _slot("A", ("Ata", 1))])
    best = crafting.best_template([uno, dos, dos_bis], "Atar", a, b)
    assert best is dos
    assert crafting.best_template([uno, dos], "Golpear", a, b) is None


def test_umbrales_y_cuantizado() -> None:
    tpls = [_tpl("t", "x", [_slot("L", ("Largo", 2)), _slot("F", ("Filo", 1))]),
            _tpl("u", "y", [_slot("L", ("Largo", 4))])]
    cuts = crafting.thresholds(tpls)
    assert cuts == {"Largo": [2.0, 4.0], "Filo": [1.0]}
    inst = crafting.leaf(_item("palo", Largo=3, Sonoro=5))
    q = crafting.quantize(inst, cuts)
    # 3 solo supera el umbral 2; Sonoro no lo usa ninguna plantilla y se descarta.
    assert q.props == (("Largo", 2.0),)


def _catalogo() -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    items = [
        _item("palo", Largo=3),
        _item("liana", Ata=3),
        _item("lasca", Filo=3),
        _item("mango", ["interno"], Largo=3, Ata=3),
        _item("hacha", ["herramienta"]),
        _item("corona", ["herramienta"]),
    ]
    templates = [
        # El hacha va delante: mango + lasca también casa con «mango» (empate a 2 slots, gana el primero).
        _tpl("hacha", "hacha", [_slot("M", ("Ata", 2), ("Largo", 2), require_all=True), _slot("F", ("Filo", 2))]),
        _tpl("mango", "mango", [_slot("L", ("Largo", 2)), _slot("A", ("Ata", 2))]),
        # Imposible: nada llega a Filo 5.
        _tpl("corona", "corona", [_slot("F", ("Filo", 5)), _slot("L", ("Largo", 1))]),
        _tpl("afilar", "hacha", [_slot("F", ("Filo", 1)), _slot("X", ("Rigido", 9))], verbs=["Afilar"],
             isSharpen=True),
    ]
    return items, templates


def test_simular_alcanza_cadenas_de_dos_pasos() -> None:
    items, templates = _catalogo()
    reach = crafting.simulate(items, templates, max_depth=2)
    assert reach.reached_templates["mango"] == 1
    assert reach.reached_templates["hacha"] == 2
    assert "corona" not in reach.reached_templates
    assert reach.reached_items["palo"] == 0 and reach.reached_items["hacha"] == 2
    # «mango» es interno y resultado: no cuenta como material en bruto.
    assert reach.reached_items["mango"] == 1


def test_simular_respeta_la_profundidad_maxima() -> None:
    items, templates = _catalogo()
    reach = crafting.simulate(items, templates, max_depth=1)
    assert "hacha" not in reach.reached_templates


def test_simular_es_determinista() -> None:
    items, templates = _catalogo()
    first = crafting.simulate(items, templates)
    second = crafting.simulate(list(reversed(items)), templates)
    assert first == second


def test_simular_registra_empates_de_slots() -> None:
    items = [_item("a", Largo=3), _item("b", Ata=3), _item("x"), _item("y")]
    t1 = _tpl("t1", "x", [_slot("L", ("Largo", 1)), _slot("A", ("Ata", 1))])
    t2 = _tpl("t2", "y", [_slot("A", ("Ata", 1)), _slot("L", ("Largo", 1))])
    reach = crafting.simulate(items, [t1, t2])
    assert ("Atar", "t1", "t2") in reach.ties
    assert "t2" not in reach.reached_templates


def test_plantillas_de_una_sola_pieza() -> None:
    items = [_item("canto", Rigido=3, Ata=3), _item("hoja"), _item("mango", ["interno"], Rigido=3, Ata=3)]
    tpl = _tpl("cordel", "hoja", [_slot("R", ("Rigido", 2)), _slot("A", ("Ata", 2))])
    comodin = _tpl("generico", "hoja", [{"role": "A"}, {"role": "B"}])
    uno = _tpl("uno", "hoja", [_slot("R", ("Rigido", 2))])
    out = crafting.single_piece_templates(items, [tpl, comodin, uno])
    assert out == {"cordel": ["canto"]}
