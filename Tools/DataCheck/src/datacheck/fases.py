"""Borradores de fase 2 y 3 (GDD v2 §6.2): ``Content/Data/fases_futuras.json``.

Raíles y vagones (§3.5), animales domésticos (§3.6), murallas y trampas (§3.8) y
trueque con el pueblo del arrecife (§3.9, §5). Comprueba:

- Todo lleva ``fase`` 2 o 3; objetos, piezas y especies a los que se refiere existen en
  ``items.json``/``fauna.json`` o están declarados como ``pendingItems`` del borrador.
- Aislamiento de fases: ningún otro fichero de ``Content/Data`` (fase 1) nombra un id que
  solo existe en el borrador, y ninguna pieza del borrador pisa un id real.
- Economía sin tienda (§5): sin claves de precio o moneda; valores de trueque 1-5; tramos
  de reputación contiguos de 0 a 100 con tasa creciente.
"""

from __future__ import annotations

import json

from . import mining

FILE = "fases_futuras.json"
DRAFT_PHASES = (2, 3)
DRAFT_SOCKETS = {"via"}
MONEY_KEYS = {"price", "precio", "moneda", "currency", "coins", "monedas", "cost_coins", "gold", "oro"}


def _walk_keys(node):
    if isinstance(node, dict):
        for k, v in node.items():
            yield k
            yield from _walk_keys(v)
    elif isinstance(node, list):
        for v in node:
            yield from _walk_keys(v)


def check_future_phases(ds, r, building_sockets: set[str]) -> None:
    doc = ds.data.get(FILE)
    if not doc:
        return
    items = {i["id"] for i in ds.items}
    tags = {t for i in ds.items for t in i.get("tags", [])}
    fauna_ids = {s.get("id") for s in ds.data.get("fauna.json", {}).get("species", [])}
    real_pieces = {p.get("id") for p in ds.building.get("pieces", [])}

    pending = {}
    for p in doc.get("pendingItems", []):
        pid = p.get("id")
        if pid in items:
            r.error(f"{FILE}/pendingItems «{pid}»: ya existe en items.json; quítalo del borrador")
        if p.get("fase") not in DRAFT_PHASES:
            r.error(f"{FILE}/pendingItems «{pid}»: fase {p.get('fase')!r} debe ser 2 o 3")
        pending[pid] = p

    for key in _walk_keys(doc):
        if key.lower() in MONEY_KEYS:
            r.error(f"{FILE}: clave «{key}» de precio o moneda; el GDD v2 §5 prohíbe la tienda")

    sections = {k: doc[k] for k in ("tramway", "livestock", "defenses", "trade") if k in doc}
    for name in ("tramway", "livestock", "defenses", "trade"):
        if name not in sections:
            r.error(f"{FILE}: falta la sección «{name}» (GDD v2 §6.2)")
    draft_ids: set[str] = set(pending)
    pieces: dict[str, dict] = {}
    for name, sec in sections.items():
        if sec.get("fase") not in DRAFT_PHASES:
            r.error(f"{FILE}/{name}: fase {sec.get('fase')!r} debe ser 2 o 3")
        if not sec.get("redNotaEs"):
            r.error(f"{FILE}/{name}: falta redNotaEs (cómo se replica en cooperativo, biblia 08)")
        for group in ("pieces", "traps", "species"):
            for e in sec.get(group, []):
                eid = e.get("id")
                if e.get("fase") not in DRAFT_PHASES or e.get("fase") < sec.get("fase", 0):
                    r.error(f"{FILE}/{name} «{eid}»: fase {e.get('fase')!r} no es 2/3 o es anterior a su sección")
                if eid in draft_ids:
                    r.error(f"{FILE}/{name}: id repetido «{eid}»")
                draft_ids.add(eid)
                if group != "species":
                    pieces[eid] = e
                    if eid in real_pieces or eid in items:
                        r.error(f"{FILE}/{name} «{eid}»: pisa un id que ya existe en fase 1")
    known = items | set(pending) | set(pieces)

    for pid, piece in pieces.items():
        sock = piece.get("socket")
        if sock is not None and sock not in building_sockets | DRAFT_SOCKETS:
            r.error(f"{FILE} «{pid}»: socket «{sock}» no es un EBuildSocket ni «via»")
        for c in piece.get("cost", []):
            if c.get("item") not in known:
                r.error(f"{FILE} «{pid}»: coste con «{c.get('item')}», que no existe ni está en pendingItems")
            if not isinstance(c.get("count"), int) or c["count"] <= 0:
                r.error(f"{FILE} «{pid}»: cantidad {c.get('count')!r} de «{c.get('item')}» debe ser > 0")

    live = sections.get("livestock", {})
    for f in live.get("feedItems", []):
        if f not in items:
            r.error(f"{FILE}/livestock: comida «{f}» no está en items.json")
    live_species = {s.get("id") for s in live.get("species", [])}
    for s in live.get("species", []):
        if s.get("structure") not in pieces:
            r.error(f"{FILE}/livestock «{s.get('id')}»: estructura «{s.get('structure')}» no está en el borrador")
        if s.get("wildSource") is not None and s["wildSource"] not in fauna_ids:
            r.error(f"{FILE}/livestock «{s.get('id')}»: wildSource «{s['wildSource']}» no está en fauna.json")
        prod = s.get("product")
        if prod and prod.get("item") not in known:
            r.error(f"{FILE}/livestock «{s.get('id')}»: producto «{prod.get('item')}» no existe")
    if not 0 < live.get("breeding", {}).get("dailyChance", 0) < 1:
        r.error(f"{FILE}/livestock: breeding.dailyChance fuera de (0, 1)")

    trade = sections.get("trade", {})
    tiers = trade.get("tiers", [])
    tier_ids = [t.get("id") for t in tiers]
    expect_min, last_rate = 0, -1.0
    for t in tiers:
        if t.get("min") != expect_min or t.get("max", -1) < t.get("min", 0):
            r.error(f"{FILE}/trade: tramo «{t.get('id')}» [{t.get('min')}, {t.get('max')}] no continúa en {expect_min}")
        expect_min = t.get("max", 0) + 1
        if t.get("rate", -1) < last_rate:
            r.error(f"{FILE}/trade: la tasa del tramo «{t.get('id')}» baja respecto al anterior")
        last_rate = t.get("rate", -1)
    if tiers and expect_min != 101:
        r.error(f"{FILE}/trade: los tramos de reputación deben acabar en 100")
    islands = mining.cpp_islands(ds)
    for s in trade.get("settlements", []):
        if islands and s.get("island") not in islands:
            r.error(f"{FILE}/trade: asentamiento en «{s.get('island')}», que no es un EIslandArchetype")
        if not 0 <= s.get("initialReputation", -1) <= 100:
            r.error(f"{FILE}/trade: reputación inicial fuera de 0-100")
    for w in trade.get("wants", []):
        if not 1 <= w.get("value", 0) <= 5:
            r.error(f"{FILE}/trade/wants «{w.get('id')}»: valor fuera de 1-5 (biblia 05 §1.4)")
        for tag in w.get("tags", []):
            if tag not in tags:
                r.error(f"{FILE}/trade/wants «{w.get('id')}»: ningún objeto tiene la etiqueta «{tag}»")
        for it in w.get("items", []):
            if it not in known:
                r.error(f"{FILE}/trade/wants «{w.get('id')}»: objeto «{it}» no existe")
    for o in trade.get("offers", []) + trade.get("favors", []):
        if not 1 <= o.get("value", 1) <= 5:
            r.error(f"{FILE}/trade/offers: valor {o.get('value')!r} fuera de 1-5")
        if o.get("minTier") not in tier_ids:
            r.error(f"{FILE}/trade: tramo «{o.get('minTier')}» no existe")
        if "item" in o and o["item"] not in known:
            r.error(f"{FILE}/trade/offers: objeto «{o['item']}» no existe")
        if "livestock" in o and o["livestock"] not in live_species:
            r.error(f"{FILE}/trade/offers: animal «{o['livestock']}» no está en livestock")
    artifacts = {a.get("id") for a in ds.data.get("artifacts.json", {}).get("artifacts", [])}
    for w in trade.get("wants", []):
        for it in w.get("items", []):
            if it in artifacts:
                r.error(f"{FILE}/trade: el tesoro «{it}» no se trueca (GDD v2 §5)")

    # Aislamiento: ningún dato de fase 1 nombra un id que solo existe en el borrador.
    draft_only = (draft_ids | set(pending)) - items - real_pieces
    for name, content in ds.data.items():
        if name == FILE:
            continue
        text = json.dumps(_phase1_view(name, content), ensure_ascii=False)
        for did in sorted(draft_only):
            if f'"{did}"' in text:
                r.error(f"{name}: usa «{did}», que solo existe en el borrador de fase 2/3 ({FILE})")


def _phase1_view(name: str, content):
    """Lo que de verdad es fase 1 en un fichero de datos.

    ``achievements.json`` lleva los logros de F2/F3 marcados con ``phase`` (biblia 07 §2).
    ``fauna_terrestre.json`` lista también las especies de F2/F3 con su ``phase`` y el
    catálogo de packs guarda en ``discarded`` y ``pending`` los ids que aún no tienen malla
    (entre ellos los animales de granja de F2): ninguno de los dos los usa en el acceso
    anticipado.
    """
    if name == "fauna_terrestre.json" and isinstance(content, dict):
        return dict(content, species=[s for s in content.get("species", []) if s.get("phase") == "AA"])
    if name == "achievements.json" and isinstance(content, dict):
        # Los logros y estadísticas de F2/F3 nombran piezas y especies del borrador a propósito.
        return dict(content,
                    stats=[s for s in content.get("stats", []) if s.get("phase", "AA") == "AA"],
                    achievements=[a for a in content.get("achievements", []) if a.get("phase", "AA") == "AA"])
    if name == "packs_catalogo.json" and isinstance(content, dict):
        # iconsPending repite las pistas de icono de los logros (también los de F2/F3), no ids de objetos.
        return {k: v for k, v in content.items() if k not in ("discarded", "pending", "iconsPending")}
    return content
