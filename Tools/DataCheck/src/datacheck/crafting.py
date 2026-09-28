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

import json
from dataclasses import dataclass, field
from operator import ge


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


_SIMULATIONS: dict[str, Reachability] = {}


def simulate(items: list[dict], templates: list[dict], max_depth: int = 2) -> Reachability:
    """Como :func:`_simulate`, con caché por contenido: los tests de DataCheck ejecutan
    todas las comprobaciones sobre copias que casi nunca tocan objetos ni plantillas."""

    # Solo cuenta lo que lee la simulación: id, etiquetas y propiedades de cada objeto.
    view = [(i.get("id"), i.get("tags", []), i.get("properties", [])) for i in items]
    key = json.dumps([max_depth, view, templates], sort_keys=True, ensure_ascii=False)
    hit = _SIMULATIONS.get(key)
    if hit is None:
        hit = _SIMULATIONS[key] = _simulate(items, templates, max_depth)
    return Reachability(dict(hit.reached_templates), dict(hit.reached_items), set(hit.ties))


def _simulate(items: list[dict], templates: list[dict], max_depth: int) -> Reachability:
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

    # Frente de Pareto por definición y etiquetas: se guarda cada instancia con su vector
    # de propiedades (solo las que tienen umbral tras cuantizar) para comparar la
    # dominancia sin montar diccionarios: «a» domina a «b» si tiene sus mismas etiquetas
    # y ninguna propiedad por debajo (casar es monótono, así que «b» no aporta nada).
    prop_index = {name: n for n, name in enumerate(sorted(cuts))}
    known: dict[tuple[str, frozenset[str]], list[tuple[tuple[float, ...], Instance]]] = {}
    # Una instancia igual a otra ya vista está dominada por ella o por quien la desbancó
    # (la dominancia es transitiva): se descarta sin recorrer el cubo.
    seen: set[Instance] = set()

    def add(inst: Instance) -> bool:
        if inst in seen:
            return False
        seen.add(inst)
        values = [0.0] * len(prop_index)
        for key, value in inst.props:
            values[prop_index[key]] = value
        vec = tuple(values)
        bucket = known.setdefault((inst.definition, inst.tags), [])
        if any(all(map(ge, kv, vec)) for kv, _ in bucket):
            return False
        bucket[:] = [kv for kv in bucket if not all(map(ge, vec, kv[0]))] + [(vec, inst)]
        return True

    def instances() -> list[Instance]:
        return [i for bucket in known.values() for _, i in bucket]

    # Qué huecos cumple cada instancia, como máscara de bits sobre todos los huecos de
    # todas las plantillas; una plantilla casa si la unión de las dos máscaras la cubre.
    slot_bits: list[tuple[int, dict]] = []
    template_mask: list[int] = []
    for t in templates:
        mask = 0
        for s in t.get("slots", []):
            mask |= 1 << len(slot_bits)
            slot_bits.append((len(slot_bits), s))
        template_mask.append(mask)
    masks: dict[Instance, int] = {}

    def mask_of(inst: Instance) -> int:
        m = masks.get(inst)
        if m is None:
            m = 0
            for bit, s in slot_bits:
                if slot_satisfied(s, inst):
                    m |= 1 << bit
            masks[inst] = m
        return m

    for item in raw:
        add(quantize(leaf(item), cuts))

    reached_items = {i["id"]: 0 for i in raw}
    reached_templates: dict[str, int] = {}
    ties: set[tuple[str, str, str]] = set()
    verbs = sorted({v for t in templates for v in t.get("verbs", [])})

    # Mismo orden que best_template: más huecos primero y, a igualdad, el primero del fichero.
    per_verb = {
        v: sorted(
            ((t, template_mask[n]) for n, t in enumerate(templates) if v in t.get("verbs", [])),
            key=lambda tm: -len(tm[0]["slots"]),
        )
        for v in verbs
    }

    def best(verb: str, both: int) -> dict | None:
        for t, m in per_verb[verb]:
            if both & m == m:
                return t
        return None

    def record_ties(verb: str, winner: dict, both: int) -> None:
        for t, m in per_verb[verb]:
            if t is not winner and len(t["slots"]) == len(winner["slots"]) and both & m == m:
                ties.add((verb, winner["id"], t["id"]))

    frontier = instances()
    for depth in range(1, max_depth + 1):
        pool = instances()
        in_frontier = {id(i): n for n, i in enumerate(frontier)}
        new: list[Instance] = []
        for n, a in enumerate(frontier):
            for b in pool:
                # Casar y combinar son simétricos: cada par sin orden una sola vez.
                if in_frontier.get(id(b), n) < n:
                    continue
                both = mask_of(a) | mask_of(b)
                for verb in verbs:
                    tpl = best(verb, both)
                    if tpl is None:
                        continue
                    record_ties(verb, tpl, both)
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
        new = [i for i in new if any(i is k for _, k in known.get((i.definition, i.tags), []))]
        if not new:
            break
        frontier = new
    return Reachability(reached_templates, reached_items, ties)


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

