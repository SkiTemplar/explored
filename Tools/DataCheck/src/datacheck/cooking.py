"""Fuego y cocina: comprobaciones de fuels.json y recipes.json y generación de sus tablas C++.

Los modelos puros (``Source/Explored/Cooking``) no leen JSON: sus tablas por defecto
(``FFireData::Default`` y ``FCookingData::Default``) se generan desde los datos con
``uv run datacheck --write-cooking`` en ``FireData.inl`` y ``CookingData.inl``. La
comprobación falla si el ``.inl`` no coincide con lo que saldría de los JSON actuales,
así que los datos son la única fuente de verdad y los specs del host prueban
exactamente lo que se juega.
"""

from __future__ import annotations

from pathlib import Path

LEVELS = {"fogata": "Fogata", "hoguera": "Hoguera", "horno_arcilla": "HornoArcilla"}
LEVEL_ORDER = list(LEVELS)
IGNITION = {"cerillas": "Matches", "pedernal": "Flint", "friccion": "Friction"}
TECHNIQUES = {
    "asar": "Roast", "hervir": "Boil", "guisar": "Stew", "ahumar": "Smoke",
    "salar": "Salt", "secar": "Dry", "hornear": "Bake",
}
FIRE_TECHNIQUES = {"asar", "hervir", "guisar", "hornear"}
WATERTIGHT_TECHNIQUES = {"hervir", "guisar"}
STATES = {"crudo": "Raw", "cocinado": "Cooked", "ahumado": "Smoked", "salado": "Salted", "seco": "Dried"}
STATE_ORDER = list(STATES)

FIRE_INL = Path("Source/Explored/Cooking/FireData.inl")
COOKING_INL = Path("Source/Explored/Cooking/CookingData.inl")
WRITE_HINT = "ejecuta «uv run datacheck --write-cooking» en Tools/DataCheck"


# --------------------------------------------------------------------------- generación


def _f(value: float | int) -> str:
    """Literal float de C++ estable (0.5f, 3.0f)."""
    return f"{float(value)!r}f"


def _s(text: str) -> str:
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def _name(text: str | None) -> str:
    return f"FName(TEXT({_s(text)}))" if text else "FName(NAME_None)"


def _header(source: str) -> list[str]:
    return [
        f"// Generado por Tools/DataCheck (uv run datacheck --write-cooking) desde Content/Data/{source}.",
        "// No editar a mano: se incluye dentro de una función de rellenado con el parámetro D.",
    ]


def render_fire_inl(fuels: dict) -> str:
    out = _header("fuels.json")
    for lv in fuels.get("levels", []):
        out += [
            "{",
            "\tFFireLevelDef& L = D.Levels.AddDefaulted_GetRef();",
            f"\tL.Level = EFireLevel::{LEVELS.get(lv['id'], 'Fogata')};",
            f"\tL.Id = {_name(lv['id'])};",
            f"\tL.NameEs = TEXT({_s(lv['nameEs'])});",
            f"\tL.PieceId = {_name(lv.get('pieceId'))};",
            f"\tL.MaxFuelHours = {_f(lv['maxFuelHours'])};",
            f"\tL.BurnRate = {_f(lv['burnRate'])};",
            f"\tL.HeatScale = {_f(lv['heat'])};",
            f"\tL.EmberHours = {_f(lv['emberHours'])};",
            f"\tL.RainQuench = {_f(lv['rainQuench'])};",
            f"\tL.WindTolerance = {_f(lv['windTolerance'])};",
            f"\tL.bEnclosed = {'true' if lv.get('enclosed') else 'false'};",
            f"\tL.BaseSmoke = {_f(lv['smoke'])};",
            "}",
        ]
    for fu in fuels.get("fuels", []):
        out += [
            "{",
            "\tFFuelDef& F = D.Fuels.AddDefaulted_GetRef();",
            f"\tF.ItemId = {_name(fu['item'])};",
            f"\tF.BurnHours = {_f(fu['burnHours'])};",
            f"\tF.Heat = {_f(fu['heat'])};",
            f"\tF.bTinder = {'true' if fu.get('tinder') else 'false'};",
            f"\tF.bGreen = {'true' if fu.get('green') else 'false'};",
            "}",
        ]
    for ig in fuels.get("ignition", []):
        out += [
            "{",
            "\tFIgnitionDef& I = D.Ignitions.AddDefaulted_GetRef();",
            f"\tI.Method = EIgnitionMethod::{IGNITION.get(ig['id'], 'Matches')};",
            f"\tI.Id = {_name(ig['id'])};",
            f"\tI.NameEs = TEXT({_s(ig['nameEs'])});",
            f"\tI.ToolItemId = {_name(ig.get('toolItem'))};",
            f"\tI.bConsumesTool = {'true' if ig.get('consumesTool') else 'false'};",
            f"\tI.BaseChance = {_f(ig['chance'])};",
            f"\tI.Minutes = {_f(ig['minutes'])};",
            "}",
        ]
    return "\n".join(out) + "\n"


def render_cooking_inl(recipes: dict, items: list[dict]) -> str:
    by_id = {i["id"]: i for i in items}
    per_unit = float(recipes.get("nutrientPointsPerUnit", 8))
    out = _header("recipes.json e items.json")
    for v in recipes.get("vessels", []):
        out += [
            "{",
            "\tFCookVesselDef& V = D.Vessels.AddDefaulted_GetRef();",
            f"\tV.Id = {_name(v['id'])};",
            f"\tV.NameEs = TEXT({_s(v['nameEs'])});",
        ]
        out += [f"\tV.ItemIds.Add({_name(i)});" for i in v.get("items", [])]
        out += [
            f"\tV.PieceId = {_name(v.get('piece'))};",
            f"\tV.Capacity = {int(v['capacity'])};",
            f"\tV.bWatertight = {'true' if v.get('watertight') else 'false'};",
            f"\tV.MaxUses = {int(v.get('maxUses', 0))};",
            "}",
        ]
    for r in recipes.get("recipes", []):
        burn = r.get("burnAfterMinutes")
        out += [
            "{",
            "\tFCookRecipeDef& R = D.Recipes.AddDefaulted_GetRef();",
            f"\tR.Id = {_name(r['id'])};",
            f"\tR.NameEs = TEXT({_s(r['nameEs'])});",
            f"\tR.Technique = ECookTechnique::{TECHNIQUES.get(r['technique'], 'Roast')};",
        ]
        out += [f"\tR.Vessels.Add({_name(v)});" for v in r.get("vessels", [])]
        out.append(f"\tR.MinFireLevel = EFireLevel::{LEVELS.get(r.get('minFireLevel') or 'fogata', 'Fogata')};")
        for ing in r.get("ingredients", []):
            out += [
                "\t{",
                "\t\tFCookIngredientReq& Q = R.Ingredients.AddDefaulted_GetRef();",
                f"\t\tQ.ItemId = {_name(ing.get('item'))};",
                f"\t\tQ.Tag = {_name(ing.get('tag'))};",
                f"\t\tQ.Count = {int(ing.get('count', 1))};",
                "\t}",
            ]
        out += [
            f"\tR.ResultItemId = {_name(r['result'])};",
            f"\tR.ResultCount = {int(r.get('resultCount', 1))};",
            f"\tR.CookMinutes = {_f(r['cookMinutes'])};",
            f"\tR.BurnAfterMinutes = {_f(-1 if burn is None else burn)};",
            "}",
        ]
    for food in recipes.get("foods", []):
        item = by_id.get(food["item"], {})
        out += [
            "{",
            "\tFFoodDef& F = D.Foods.AddDefaulted_GetRef();",
            f"\tF.ItemId = {_name(food['item'])};",
            f"\tF.State = EFoodState::{STATES.get(food['state'], 'Raw')};",
            f"\tF.Family = {_name(food['family'])};",
        ]
        out += [f"\tF.Tags.Add({_name(t)});" for t in item.get("tags", [])]
        out += [
            f"\tF.Effects.Food = {_f(food['food'])};",
            f"\tF.Effects.Water = {_f(food['water'])};",
            f"\tF.Effects.Protein = {_f(item.get('nutritionProtein', 0) * per_unit)};",
            f"\tF.Effects.Carbs = {_f(item.get('nutritionEnergy', 0) * per_unit)};",
            f"\tF.Effects.Vitamins = {_f(item.get('nutritionVitamins', 0) * per_unit)};",
            f"\tF.Effects.Warmth = {_f(food['warmth'])};",
            f"\tF.Effects.Morale = {_f(food['morale'])};",
            f"\tF.Effects.Toxicity = {_f(food['toxicity'])};",
            f"\tF.bCookingRemovesToxicity = {'true' if food.get('cookingRemovesToxicity') else 'false'};",
            "}",
        ]
    pres = recipes.get("preservation", {})
    for state, hours in pres.get("stateHours", {}).items():
        out.append(f"D.Preservation.StateHours[static_cast<int32>(EFoodState::{STATES.get(state, 'Raw')})] = {_f(hours)};")
    for family, factor in pres.get("families", {}).items():
        out.append(f"D.Preservation.FamilyFactor.Add({_name(family)}, {_f(factor)});")
    for key, field in (("staleFraction", "StaleFraction"), ("staleNutrition", "StaleNutrition"),
                       ("spoiledToxicity", "SpoiledToxicity"), ("rottenToxicity", "RottenToxicity"),
                       ("spoiledMorale", "SpoiledMorale"), ("warmHours", "WarmHours")):
        if key in pres:
            out.append(f"D.Preservation.{field} = {_f(pres[key])};")
    imp = recipes.get("improvised", {})
    if imp:
        out += [
            f"D.Improvised.Technique = ECookTechnique::{TECHNIQUES.get(imp.get('technique', 'guisar'), 'Stew')};",
            f"D.Improvised.ResultItemId = {_name(imp.get('result'))};",
            f"D.Improvised.CookMinutes = {_f(imp['cookMinutes'])};",
            f"D.Improvised.BurnAfterMinutes = {_f(-1 if imp.get('burnAfterMinutes') is None else imp['burnAfterMinutes'])};",
            f"D.Improvised.Efficiency = {_f(imp['efficiency'])};",
            f"D.Improvised.Warmth = {_f(imp['warmth'])};",
            f"D.Improvised.Morale = {_f(imp['morale'])};",
        ]
    out.append(f"D.BurntItemId = {_name(recipes.get('burnt'))};")
    return "\n".join(out) + "\n"


def generated_files(data: dict[str, object]) -> dict[Path, str]:
    """Ruta relativa al repo → contenido esperado de cada .inl."""
    files: dict[Path, str] = {}
    if "fuels.json" in data:
        files[FIRE_INL] = render_fire_inl(data["fuels.json"])
    if "recipes.json" in data:
        files[COOKING_INL] = render_cooking_inl(data["recipes.json"], data.get("items.json", []))
    return files


# --------------------------------------------------------------------------- comprobaciones


def _num(value, lo: float, hi: float) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool) and lo <= value <= hi


def check_fuels(fuels: dict, items: list[dict], piece_ids: set[str], error) -> None:
    by_id = {i["id"]: i for i in items}
    levels = fuels.get("levels", [])
    if [lv.get("id") for lv in levels] != LEVEL_ORDER:
        error(f"fuels.json: los niveles deben ser {LEVEL_ORDER} en ese orden (GDD §8.5)")
    for lv in levels:
        lid = lv.get("id")
        if lv.get("pieceId") not in piece_ids:
            error(f"fuels.json nivel «{lid}»: pieceId «{lv.get('pieceId')}» no está en building_pieces.json")
        for key, lo, hi in (("maxFuelHours", 0.5, 48), ("burnRate", 0.1, 3), ("heat", 0.1, 1.5), ("emberHours", 0, 24),
                            ("rainQuench", 0, 10), ("windTolerance", 0, 1), ("smoke", 0, 1)):
            if not _num(lv.get(key), lo, hi):
                error(f"fuels.json nivel «{lid}»: {key}={lv.get(key)!r} fuera de [{lo}, {hi}]")
        if not lv.get("nameEs"):
            error(f"fuels.json nivel «{lid}»: falta nameEs")
    for prev, cur in zip(levels, levels[1:]):
        for key in ("maxFuelHours", "heat", "emberHours"):
            if _num(prev.get(key), 0, 99) and _num(cur.get(key), 0, 99) and cur[key] <= prev[key]:
                error(f"fuels.json: {key} de «{cur.get('id')}» no supera a «{prev.get('id')}»")
    if levels and not levels[-1].get("enclosed"):
        error("fuels.json: el horno de arcilla debe ser cerrado (enclosed) para resistir la lluvia")

    seen: set[str] = set()
    for fu in fuels.get("fuels", []):
        iid = fu.get("item")
        if iid in seen:
            error(f"fuels.json: combustible repetido «{iid}»")
        seen.add(iid)
        item = by_id.get(iid)
        if item is None:
            error(f"fuels.json: combustible «{iid}» no está en items.json")
            continue
        if not _num(fu.get("burnHours"), 0.01, 12) or not _num(fu.get("heat"), 0, 1):
            error(f"fuels.json «{iid}»: burnHours o heat fuera de rango")
        props = {p["name"]: p["value"] for p in item.get("properties", [])}
        if fu.get("tinder") and props.get("Inflamable", 0) < 2:
            error(f"fuels.json «{iid}»: yesca sin Inflamable ≥ 2 en items.json")
        burns = "madera" in item.get("tags", []) or props.get("Combustible", 0) > 0 or props.get("Inflamable", 0) > 0
        if not fu.get("green") and not burns:
            error(f"fuels.json «{iid}»: no es madera ni tiene Combustible/Inflamable en items.json")
    if not any(fu.get("tinder") for fu in fuels.get("fuels", [])):
        error("fuels.json: hace falta al menos una yesca")
    if not any(fu.get("green") for fu in fuels.get("fuels", [])):
        error("fuels.json: hace falta combustible verde para el fuego de señal")

    ids = [ig.get("id") for ig in fuels.get("ignition", [])]
    if sorted(ids) != sorted(IGNITION):
        error(f"fuels.json: formas de encender {ids}; se esperan {sorted(IGNITION)}")
    for ig in fuels.get("ignition", []):
        if ig.get("toolItem") not in by_id:
            error(f"fuels.json encendido «{ig.get('id')}»: toolItem «{ig.get('toolItem')}» no está en items.json")
        if not _num(ig.get("chance"), 0.01, 1) or not _num(ig.get("minutes"), 0, 60):
            error(f"fuels.json encendido «{ig.get('id')}»: chance o minutes fuera de rango")


def check_recipes(recipes: dict, items: list[dict], piece_ids: set[str], error) -> None:
    by_id = {i["id"]: i for i in items}
    all_tags = {t for i in items for t in i.get("tags", [])}
    foods = {f.get("item"): f for f in recipes.get("foods", [])}
    pres = recipes.get("preservation", {})
    families = pres.get("families", {})

    vessels: dict[str, dict] = {}
    for v in recipes.get("vessels", []):
        vid = v.get("id")
        if vid in vessels:
            error(f"recipes.json: utensilio repetido «{vid}»")
        vessels[vid] = v
        for iid in v.get("items", []):
            if iid not in by_id:
                error(f"recipes.json utensilio «{vid}»: objeto «{iid}» no está en items.json")
        if v.get("piece") is not None and v["piece"] not in piece_ids:
            error(f"recipes.json utensilio «{vid}»: pieza «{v['piece']}» no está en building_pieces.json")
        if not v.get("items") and v.get("piece") is None:
            error(f"recipes.json utensilio «{vid}»: ni objetos ni pieza que lo representen")
        if not isinstance(v.get("capacity"), int) or not 1 <= v["capacity"] <= 8:
            error(f"recipes.json utensilio «{vid}»: capacity fuera de [1, 8]")

    seen: set[str] = set()
    for r in recipes.get("recipes", []):
        rid = r.get("id")
        if rid in seen:
            error(f"recipes.json: receta repetida «{rid}»")
        seen.add(rid)
        tech = r.get("technique")
        if tech not in TECHNIQUES:
            error(f"recipes.json «{rid}»: técnica «{tech}» desconocida ({sorted(TECHNIQUES)})")
        if r.get("result") not in by_id:
            error(f"recipes.json «{rid}»: resultado «{r.get('result')}» no está en items.json")
        elif "comida" in by_id[r["result"]].get("tags", []) and r["result"] not in foods:
            error(f"recipes.json «{rid}»: el resultado «{r['result']}» es comida y no está en foods")
        level = r.get("minFireLevel")
        if tech in FIRE_TECHNIQUES and level not in LEVELS:
            error(f"recipes.json «{rid}»: técnica con fuego sin minFireLevel válido")
        if tech not in FIRE_TECHNIQUES and level is not None:
            error(f"recipes.json «{rid}»: minFireLevel en una técnica sin fuego")
        if tech == "hornear" and level != "horno_arcilla":
            error(f"recipes.json «{rid}»: hornear solo se hace en el horno de arcilla")
        units = 0
        for ing in r.get("ingredients", []):
            has_item, has_tag = ing.get("item") is not None, ing.get("tag") is not None
            if has_item == has_tag:
                error(f"recipes.json «{rid}»: cada ingrediente lleva «item» o «tag», no ambos ni ninguno")
            if has_item and ing["item"] not in by_id:
                error(f"recipes.json «{rid}»: ingrediente «{ing['item']}» no está en items.json")
            if has_tag and ing["tag"] not in all_tags:
                error(f"recipes.json «{rid}»: ningún objeto tiene la etiqueta «{ing['tag']}»")
            if not isinstance(ing.get("count"), int) or ing["count"] < 1:
                error(f"recipes.json «{rid}»: count inválido en un ingrediente")
            units += ing.get("count", 1) if isinstance(ing.get("count"), int) else 1
        if units < 1:
            error(f"recipes.json «{rid}»: sin ingredientes")
        for vid in r.get("vessels", []):
            v = vessels.get(vid)
            if v is None:
                error(f"recipes.json «{rid}»: utensilio «{vid}» no existe")
                continue
            if units > v.get("capacity", 0):
                error(f"recipes.json «{rid}»: {units} ingredientes no caben en «{vid}» (capacidad {v.get('capacity')})")
            if tech in WATERTIGHT_TECHNIQUES and not v.get("watertight"):
                error(f"recipes.json «{rid}»: {tech} en «{vid}», que no es estanco")
        if tech in WATERTIGHT_TECHNIQUES and not r.get("vessels"):
            error(f"recipes.json «{rid}»: {tech} necesita un recipiente estanco")
        if not _num(r.get("cookMinutes"), 1, 2880):
            error(f"recipes.json «{rid}»: cookMinutes fuera de [1, 2880]")
        burn = r.get("burnAfterMinutes")
        if burn is not None and not _num(burn, 1, 600):
            error(f"recipes.json «{rid}»: burnAfterMinutes fuera de [1, 600] (o null)")
        if not isinstance(r.get("resultCount"), int) or not 1 <= r["resultCount"] <= 10:
            error(f"recipes.json «{rid}»: resultCount fuera de [1, 10]")

    for key in ("improvised",):
        imp = recipes.get(key, {})
        if imp.get("technique") not in TECHNIQUES or imp.get("result") not in foods:
            error("recipes.json: «improvised» necesita una técnica válida y un resultado que esté en foods")
    if recipes.get("burnt") not in foods:
        error("recipes.json: «burnt» debe ser una comida de foods")

    for iid, food in foods.items():
        if iid not in by_id:
            error(f"recipes.json foods: «{iid}» no está en items.json")
        if food.get("state") not in STATES:
            error(f"recipes.json foods «{iid}»: estado «{food.get('state')}» desconocido")
        if food.get("family") not in families:
            error(f"recipes.json foods «{iid}»: familia «{food.get('family')}» sin factor en preservation")
        if not _num(food.get("toxicity"), 0, 1):
            error(f"recipes.json foods «{iid}»: toxicity fuera de [0, 1]")
        for key in ("food", "water", "warmth", "morale"):
            if not _num(food.get(key), -50, 100):
                error(f"recipes.json foods «{iid}»: {key} fuera de [-50, 100]")
        tox_prop = next((p["value"] for p in by_id.get(iid, {}).get("properties", []) if p["name"] == "Toxico"), 0)
        if tox_prop > 0 and food.get("toxicity", 0) <= 0:
            error(f"recipes.json foods «{iid}»: Toxico={tox_prop} en items.json pero toxicity 0")
    for item in items:
        if "comida" in item.get("tags", []) and item["id"] not in foods:
            error(f"recipes.json foods: falta la comida «{item['id']}» de items.json")

    hours = pres.get("stateHours", {})
    if sorted(hours) != sorted(STATES):
        error(f"recipes.json preservation.stateHours: estados {sorted(hours)}; se esperan {sorted(STATES)}")
    elif not (hours["crudo"] < hours["cocinado"] < min(hours["ahumado"], hours["salado"], hours["seco"])):
        error("recipes.json preservation.stateHours: debe cumplirse crudo < cocinado < ahumado, salado y seco (GDD §8.8)")
    for fam, factor in families.items():
        if not _num(factor, 0, 20):
            error(f"recipes.json preservation.families «{fam}»: factor fuera de [0, 20]")
    if not _num(pres.get("spoiledToxicity"), 0, 1) or not _num(pres.get("rottenToxicity"), 0, 1) \
            or pres.get("rottenToxicity", 0) < pres.get("spoiledToxicity", 0):
        error("recipes.json preservation: spoiledToxicity ≤ rottenToxicity, ambos en [0, 1]")


def check_generated(repo_root: Path, data: dict[str, object], error) -> None:
    for rel, expected in generated_files(data).items():
        path = repo_root / rel
        current = path.read_text(encoding="utf-8") if path.exists() else None
        if current != expected:
            error(f"{rel.as_posix()} no coincide con los datos: {WRITE_HINT}")


def write_generated(repo_root: Path, data: dict[str, object]) -> list[Path]:
    written = []
    for rel, text in generated_files(data).items():
        path = repo_root / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("w", encoding="utf-8", newline="\n") as fh:
            fh.write(text)
        written.append(rel)
    return written
