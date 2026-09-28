"""Réplica en Python de la combinación por propiedades de UCraftingLibrary.

Reglas (Source/Explored/Crafting/CraftingLibrary.cpp e Items/ItemTypes.cpp):

- Un slot se cumple si ALGUNA de las dos piezas lo cumple (no hay asignación 1:1).
- ``requireAll``: todos los requisitos (>= min); si no, basta con uno. Los
  ``tags`` del slot exigen que la pieza tenga alguno, solo en su definición
  propia (HasTag no recorre las piezas).
- Propiedad efectiva de un compuesto = máximo entre su definición y sus piezas.
- Entre las plantillas que casan con el verbo gana la de más slots; a igualdad,
  la primera del fichero.
- ``isSharpen`` no crea objeto: devuelve la herramienta reparada.
"""

from __future__ import annotations

import copy
import json
from dataclasses import dataclass, field


@dataclass(frozen=True)
class Instance:
    """Instancia abstracta: definición + propiedades efectivas (ya maximizadas)."""

    definition: str
    props: tuple[tuple[str, float], ...]
    tags: frozenset[str]
    depth: int = field(default=0, compare=False)

    def prop(self, name: str) -> float:
        for key, value in self.props:
            if key == name:
                return value
        return 0.0


def leaf(item: dict) -> Instance:
    props = {p["name"]: float(p["value"]) for p in item.get("properties", [])}
    return Instance(item["id"], tuple(sorted(props.items())), frozenset(item.get("tags", [])))


def combine(result_item: dict, left: Instance, right: Instance) -> Instance:
    props = {p["name"]: float(p["value"]) for p in result_item.get("properties", [])}
    for piece in (left, right):
        for key, value in piece.props:
            props[key] = max(props.get(key, 0.0), value)
    return Instance(
        result_item["id"],
        tuple(sorted(props.items())),
        frozenset(result_item.get("tags", [])),
        depth=max(left.depth, right.depth) + 1,
    )


def slot_satisfied(slot: dict, piece: Instance) -> bool:
    reqs = slot.get("requirements", [])
    if reqs:
        checks = [piece.prop(r["property"]) >= float(r.get("min", 0)) for r in reqs]
        ok = all(checks) if slot.get("requireAll", False) else any(checks)
    else:
        ok = True
    tags = slot.get("tags", [])
    if tags:
        ok = ok and any(t in piece.tags for t in tags)
    return ok


def template_matches(template: dict, left: Instance, right: Instance) -> bool:
    return all(slot_satisfied(s, left) or slot_satisfied(s, right) for s in template.get("slots", []))


def best_template(templates: list[dict], verb: str, left: Instance, right: Instance) -> dict | None:
    best = None
    for template in templates:
        if verb not in template.get("verbs", []):
            continue
        if not template_matches(template, left, right):
            continue
        if best is None or len(template["slots"]) > len(best["slots"]):
            best = template
    return best


@dataclass
class Reachability:
    reached_templates: dict[str, int]  # id -> profundidad mínima (1 = directo)
    reached_items: dict[str, int]  # id -> profundidad mínima (0 = en bruto)
    ties: set[tuple[str, str, str]]  # (verbo, plantilla ganadora, sombreada) empatadas en slots


def thresholds(templates: list[dict]) -> dict[str, list[float]]:
    """Mínimos que usa alguna plantilla, por propiedad (ordenados)."""

    out: dict[str, set[float]] = {}
    for t in templates:
        for slot in t.get("slots", []):
            for req in slot.get("requirements", []):
                out.setdefault(req["property"], set()).add(float(req.get("min", 0)))
    return {k: sorted(v) for k, v in out.items()}


def quantize(inst: Instance, cuts: dict[str, list[float]]) -> Instance:
    """Redondea cada propiedad al mayor umbral que alcanza y descarta las irrelevantes.

    Es exacto para decidir qué plantillas casan (solo importa superar umbrales) y
    compatible con la herencia por máximo, así que la exploración no cambia de
    resultado; solo colapsa instancias equivalentes.
    """

    props = []
    for key, value in inst.props:
        passed = [c for c in cuts.get(key, []) if value >= c and c > 0]
        if passed:
            props.append((key, passed[-1]))
    return Instance(inst.definition, tuple(props), inst.tags, inst.depth)


def _dominates(a: Instance, b: Instance) -> bool:
    pa = dict(a.props)
    return a.tags == b.tags and all(pa.get(k, 0.0) >= v for k, v in b.props)


# Resultados ya calculados, por contenido de (items, templates, max_depth). La simulación es
# pura y cara (segundos con el catálogo real), y los tests y --write-* la repiten con los mismos
# datos; la clave es el JSON completo, así que cualquier cambio de datos recalcula.
_SIMULATE_CACHE: dict[str, Reachability] = {}
_CACHE_SIZE = 8


def simulate(items: list[dict], templates: list[dict], max_depth: int = 2) -> Reachability:
    """Como ``_simulate``, con memoria de los últimos catálogos (devuelve una copia)."""
    key = json.dumps([items, templates, max_depth], sort_keys=True, default=repr)
    if key not in _SIMULATE_CACHE:
        if len(_SIMULATE_CACHE) >= _CACHE_SIZE:
            _SIMULATE_CACHE.pop(next(iter(_SIMULATE_CACHE)))
        _SIMULATE_CACHE[key] = _simulate(items, templates, max_depth)
    return copy.deepcopy(_SIMULATE_CACHE[key])


def _simulate(items: list[dict], templates: list[dict], max_depth: int = 2) -> Reachability:
    """Explora combinaciones desde los materiales en bruto hasta ``max_depth`` pasos.

    Materiales en bruto: todo objeto que no es resultado de ninguna plantilla ni
    lleva la etiqueta ``interno``. Las propiedades se cuantizan a los umbrales de
    las plantillas y, por definición, se conservan solo las instancias no
    dominadas (casar es monótono en las propiedades), así que el espacio es
    pequeño y el resultado es exacto para la alcanzabilidad.
    """

    by_id = {i["id"]: i for i in items}
    cuts = thresholds(templates)
    produced = {t["resultDefinitionId"] for t in templates}
    raw = [i for i in items if i["id"] not in produced and "interno" not in i.get("tags", [])]

    known: dict[str, list[Instance]] = {}

    def add(inst: Instance) -> bool:
        bucket = known.setdefault(inst.definition, [])
        if any(_dominates(k, inst) for k in bucket):
            return False
        bucket[:] = [k for k in bucket if not _dominates(inst, k)] + [inst]
        return True

    for item in raw:
        add(quantize(leaf(item), cuts))

    reached_items = {i["id"]: 0 for i in raw}
    reached_templates: dict[str, int] = {}
    ties: set[tuple[str, str, str]] = set()
    verbs = sorted({v for t in templates for v in t.get("verbs", [])})

    per_verb = {v: [t for t in templates if v in t.get("verbs", [])] for v in verbs}
    frontier = [i for bucket in known.values() for i in bucket]
    for depth in range(1, max_depth + 1):
        pool = [i for bucket in known.values() for i in bucket]
        in_frontier = {id(i): n for n, i in enumerate(frontier)}
        new: list[Instance] = []
        for n, a in enumerate(frontier):
            for b in pool:
                # Casar y combinar son simétricos: cada par sin orden una sola vez.
                if in_frontier.get(id(b), n) < n:
                    continue
                for verb in verbs:
                    tpl = best_template(per_verb[verb], verb, a, b)
                    if tpl is None:
                        continue
                    _record_ties(per_verb[verb], verb, tpl, a, b, ties)
                    reached_templates.setdefault(tpl["id"], depth)
                    if tpl.get("isSharpen"):
                        continue
                    result = by_id.get(tpl["resultDefinitionId"])
                    if result is None:
                        continue
                    inst = quantize(combine(result, a, b), cuts)
                    reached_items.setdefault(inst.definition, depth)
                    if add(inst):
                        new.append(inst)
        new = [i for i in new if any(i is k for k in known.get(i.definition, []))]
        if not new:
            break
        frontier = new
    return Reachability(reached_templates, reached_items, ties)


def _record_ties(templates, verb, winner, a, b, ties) -> None:
    for other in templates:
        if other is winner or verb not in other.get("verbs", []):
            continue
        if len(other["slots"]) == len(winner["slots"]) and template_matches(other, a, b):
            ties.add((verb, winner["id"], other["id"]))


def single_piece_templates(items: list[dict], templates: list[dict]) -> dict[str, list[str]]:
    """Plantillas cuyos slots cubre UNA sola pieza del catálogo en bruto.

    Con la regla «cada slot lo cubre alguna de las dos piezas», esa pieza más
    cualquier otra objeto dispara la plantilla (p. ej. fibra de coco + canto
    rodado → cordel). Se informa como aviso de diseño, no como error.
    """

    out: dict[str, list[str]] = {}
    for tpl in templates:
        if tpl.get("isSharpen") or len(tpl.get("slots", [])) < 2:
            continue
        if any(not s.get("requirements") and not s.get("tags") for s in tpl["slots"]):
            continue  # comodín por diseño (atado/pegado genérico)
        hits = []
        for item in items:
            if "interno" in item.get("tags", []):
                continue
            inst = leaf(item)
            if all(slot_satisfied(s, inst) for s in tpl["slots"]):
                hits.append(item["id"])
        if hits:
            out[tpl["id"]] = hits
    return out


MAX_ACTIONS = 3  # UCraftingLibrary::MaxActions
# Mangos atados de referencia para buscar verbos escondidos: (mango, ligadura).
REFERENCE_HANDLES = (("palo_recto", "cuerda"), ("bambu_grueso", "cuerda"), ("madera_dura", "cuerda"))


def offered_verbs(templates: list[dict], left: Instance, right: Instance) -> list[str]:
    """Todos los verbos que casan, en el orden de FindActionsWithData (plantillas en orden del fichero)."""
    verbs: list[str] = []
    for tpl in templates:
        if template_matches(tpl, left, right):
            for verb in tpl.get("verbs", []):
                if verb not in verbs:
                    verbs.append(verb)
    return verbs


def hidden_templates(items: list[dict], templates: list[dict]) -> list[tuple[str, str, list[str], list[str]]]:
    """Pares cuyo cuarto verbo o siguientes esconden una plantilla que no es genérica.

    ``FindActionsWithData`` corta en ``MaxActions`` verbos: si una plantilla solo tiene
    verbos de los que quedan fuera, ese par nunca la ofrece en la mano. Se miran los pares
    del catálogo y cada objeto con un mango atado de ``REFERENCE_HANDLES``.
    Devuelve (pieza A, pieza B, verbos escondidos, plantillas escondidas).
    """
    by_id = {i["id"]: i for i in items}
    pieces = [leaf(i) for i in items if "interno" not in i.get("tags", [])]
    handles = []
    for handle, lashing in REFERENCE_HANDLES:
        if handle in by_id and lashing in by_id:
            a, b = leaf(by_id[handle]), leaf(by_id[lashing])
            tpl = best_template(templates, "Atar", a, b)
            if tpl is not None and tpl.get("resultDefinitionId") in by_id:
                handles.append((f"{handle} atado con {lashing}", combine(by_id[tpl["resultDefinitionId"]], a, b)))
    pairs = [(a.definition, a, b.definition, b) for n, a in enumerate(pieces) for b in pieces[n:]]
    pairs += [(name, h, p.definition, p) for name, h in handles for p in pieces]
    out = []
    for name_a, a, name_b, b in pairs:
        verbs = offered_verbs(templates, a, b)
        if len(verbs) <= MAX_ACTIONS:
            continue
        hidden = verbs[MAX_ACTIONS:]
        lost = [t["id"] for t in templates if not t["id"].endswith("_generico") and template_matches(t, a, b)
                and all(v in hidden for v in t.get("verbs", []))]
        if lost:
            out.append((name_a, name_b, hidden, lost))
    return out
