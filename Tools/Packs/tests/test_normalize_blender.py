"""normalize.py de punta a punta con Blender 5.2 como módulo (``bpy`` de PyPI).

Se genera un pack sintético (glTF con textura, OBJ, FBX y un ``.blend`` con esqueleto y
acciones), se normaliza con ``main()`` a un tmp y se reimportan los FBX para comprobar
medida, pivote, paleta y rig. Sin ``bpy`` (Python distinto de 3.13 o sin el grupo
``blender``) el módulo se salta.
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

import pytest

bpy = pytest.importorskip("bpy")

from bpy_extras import anim_utils  # noqa: E402
from mathutils import Vector  # noqa: E402

import normalize as nz  # noqa: E402
import normalize_core as nc  # noqa: E402

PALETTE = nc.load_json(nz.PALETTE)["islas"]["Landing"]["colores"]
BROWN, GREY, PINK = "#7c533e", "#677c89", "#d8a1a4"
LOTE = "lote-test"


# --- pack sintético ------------------------------------------------------------------

def _material(name: str, hex_color: str, image: Any = None) -> Any:
    mat = bpy.data.materials.new(name)
    bsdf = next(n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    rgb = [nc.srgb_to_linear(c) for c in nc.hex_to_srgb(hex_color)]
    bsdf.inputs["Base Color"].default_value = (*rgb, 1.0)
    if image is not None:
        tex = mat.node_tree.nodes.new("ShaderNodeTexImage")
        tex.image = image
        mat.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    return mat


def _box(name: str, size: tuple[float, float, float], loc: tuple[float, float, float], mat: Any) -> Any:
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=loc)
    obj = bpy.context.active_object
    obj.name = name
    obj.scale = size
    bpy.ops.object.transform_apply(scale=True)
    obj.data.materials.append(mat)
    return obj


def _tool(textured: bool) -> None:
    """Herramienta con el mango a lo largo de +Y (0.8 m) y la cabeza gris en +Y alto."""
    nz.reset_scene()
    image = None
    if textured:
        # Textura 2×2 de un solo color: el muestreo por UV debe dar exactamente GREY.
        image = bpy.data.images.new("head_tex", 2, 2)
        image.pixels[:] = [*nc.hex_to_srgb(GREY), 1.0] * 4
        image.pack()
    _box("mango", (0.05, 0.8, 0.05), (0.0, 0.4, 0.0), _material("M_Mango", BROWN))
    _box("cabeza", (0.3, 0.1, 0.12), (0.1, 0.85, 0.0), _material("M_Cabeza", GREY, image))


def _export(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    ext = path.suffix
    if ext == ".glb":
        bpy.ops.export_scene.gltf(filepath=str(path), export_format="GLB")
    elif ext == ".obj":
        bpy.ops.wm.obj_export(filepath=str(path))
    elif ext == ".fbx":
        bpy.ops.export_scene.fbx(filepath=str(path))


def _rigged(path: Path, orphan: bool = True, armature_modifier: bool = True) -> None:
    """Animal mínimo: armadura de 2 huesos, caja de 1 m deformada y acciones Idle y Run."""
    nz.reset_scene()
    arm_data = bpy.data.armatures.new("Arm")
    arm = bpy.data.objects.new("Arm", arm_data)
    bpy.context.scene.collection.objects.link(arm)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    root = arm_data.edit_bones.new("Root")
    root.head, root.tail = (0, 0, 0), (0, 0, 0.5)
    head = arm_data.edit_bones.new("Head")
    head.head, head.tail, head.parent = (0, 0, 0.5), (0, 0, 1), root
    bpy.ops.object.mode_set(mode="OBJECT")

    body = _box("Cuerpo", (0.4, 0.6, 1.0), (0, 0, 0.5), _material("M_Piel", PINK))
    for bone in ("Root", "Head"):
        body.vertex_groups.new(name=bone).add(list(range(len(body.data.vertices))), 0.5, "REPLACE")
    if armature_modifier:
        body.modifiers.new("Armature", "ARMATURE").object = arm
    body.parent = arm

    ad = arm.animation_data_create()
    for name, dz in (("Idle", 0.0), ("Run", 0.1)):
        act = bpy.data.actions.new(name)
        act.use_fake_user = True
        ad.action = act
        pb = arm.pose.bones["Head"]
        for frame, z in ((1, 0.0), (10, dz)):
            pb.location = (0.0, z, 0.0)
            pb.keyframe_insert("location", frame=frame)
        if orphan:
            # Como Quaternius: la acción anima un hueso que este esqueleto no tiene.
            bag = anim_utils.action_ensure_channelbag_for_slot(act, ad.action_slot)
            fc = bag.fcurves.new('pose.bones["Tail1"].location', index=0)
            fc.keyframe_points.insert(1, 0.0)
    ad.action = None
    path.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(path), copy=True)


def _entry(game_id: str, file: str, **kw: Any) -> dict[str, Any]:
    e: dict[str, Any] = {
        "gameId": game_id, "kind": "item", "lote": LOTE, "pack": "demo", "file": file,
        "mesh": f"SM_Pack_{game_id.title()}", "rotateDeg": [90, 0, 0],
        "size": {"axis": "z", "m": 0.55}, "pivot": {"kind": "agarre", "gripFromEndM": 0.12},
        "recolor": {"rules": [{"from": BROWN, "to": "madera.quemada"}, {"from": GREY, "to": "piedra.basalto"}],
                    "tolerance": 0.05},
    }
    e.update(kw)
    return e


ENTRIES = [
    _entry("hacha", "gltf/hacha.glb"),
    _entry("pico", "obj/pico.obj", stretch=[0.5, 1.0, 1.0], pivot={"kind": "base"},
           size={"axis": "max", "m": 1.1}),
    _entry("maza", "fbx/maza.fbx", pivot={"kind": "agarre", "gripFromEndM": 0.1, "end": "top",
                                          "centerAt": "grip"}),
    _entry("cerdo", "Blends/Pig.blend", kind="fauna", rotateDeg=[0, 0, 90], size={"axis": "z", "m": 2.0},
           pivot={"kind": "base"}, recolor={"rules": [{"from": PINK, "to": "madera.oscura"}]},
           rig={"animations": ["Idle", "Run"],
                "tilePoses": [{"action": "Idle", "frame": 0}, {"action": "Run", "frame": 10}]}),
]


@pytest.fixture(scope="module")
def pack(tmp_path_factory: pytest.TempPathFactory) -> Path:
    """Caché con el pack «demo» generado (una vez por módulo)."""
    root = tmp_path_factory.mktemp("cache")
    demo = root / "demo"
    _tool(textured=True)
    _export(demo / "gltf" / "hacha.glb")
    _tool(textured=False)
    _export(demo / "obj" / "pico.obj")
    _export(demo / "fbx" / "maza.fbx")
    _rigged(demo / "Blends" / "Pig.blend")
    return root


@pytest.fixture
def repo(tmp_path: Path, pack: Path, monkeypatch: pytest.MonkeyPatch) -> Path:
    """Catálogo y packs.json de prueba; exporta a tmp_path/Export."""
    cat = tmp_path / "packs_catalogo.json"
    cat.write_text(json.dumps({"entries": ENTRIES}), encoding="utf-8")
    packs = tmp_path / "packs.json"
    packs.write_text(json.dumps({"packs": [{"id": "demo"}]}), encoding="utf-8")
    monkeypatch.setattr(nz, "CATALOG", cat)
    monkeypatch.setattr(nz, "PACKS_JSON", packs)
    monkeypatch.setattr(nz, "EXPORT", tmp_path / "Export")
    monkeypatch.setenv("EXPLORED_PACKS_CACHE", str(pack))
    return tmp_path


def _reimport(path: Path) -> list[Any]:
    nz.reset_scene()
    bpy.ops.import_scene.fbx(filepath=str(path))
    return list(bpy.data.objects)


def _verts(obj: Any) -> list[Any]:
    return [obj.matrix_world @ v.co for v in obj.data.vertices]


# --- lote completo ---------------------------------------------------------------------

@pytest.fixture
def normalized(repo: Path) -> dict[str, Any]:
    nz.main(["--lote", LOTE, "--tiles"])
    return json.loads((repo / "Export" / LOTE / "_tiles" / "report.json").read_text(encoding="utf-8"))


def test_informe_del_lote(normalized: dict[str, Any], repo: Path) -> None:
    assert set(normalized) == {"hacha", "pico", "maza", "cerdo"}
    hacha = normalized["hacha"]
    assert hacha["mesh"] == "SM_Pack_Hacha"
    assert hacha["dims"][2] == pytest.approx(0.55, abs=1e-3)
    assert hacha["tris"] == 24  # dos cajas de 12 triángulos
    # El mango (4 caras largas) domina el área: madera primero, basalto después.
    assert list(hacha["paleta"]) == ["madera.quemada", "piedra.basalto"]
    assert sum(hacha["paleta"].values()) == pytest.approx(100, abs=0.2)
    assert max(normalized["pico"]["dims"]) == pytest.approx(1.1, abs=1e-3)
    cerdo = normalized["cerdo"]
    assert cerdo["bones"] == 2 and cerdo["animations"] == ["Idle", "Run"]
    assert cerdo["scale"] == pytest.approx(2.0)
    assert cerdo["dims"][2] == pytest.approx(2.0, abs=1e-3)
    assert cerdo["paleta"] == {"madera.oscura": 100.0}
    tiles = repo / "Export" / LOTE / "_tiles"
    for name in ("hacha", "pico", "maza", "cerdo", "cerdo@Run"):
        assert (tiles / f"{name}.png").stat().st_size > 0, name


def test_pivote_de_agarre_y_paleta_en_el_fbx(normalized: dict[str, Any], repo: Path) -> None:
    objs = _reimport(repo / "Export" / LOTE / "SM_Pack_Hacha.fbx")
    assert len(objs) == 1
    obj = objs[0]
    vs = _verts(obj)
    # Agarre a 0.12 m del extremo inferior: ese punto es el origen.
    assert min(v.z for v in vs) == pytest.approx(-0.12, abs=1e-3)
    assert max(v.z for v in vs) == pytest.approx(0.55 - 0.12, abs=1e-3)
    assert [m.name for m in obj.data.materials] == ["M_LowPoly"]
    assert "Col" in obj.data.color_attributes
    # Todas las UV caen en la columna de una de las dos muestras usadas.
    us = {round(d.uv.x, 4) for d in obj.data.uv_layers.active.data}
    assert us == {round(PALETTE[k]["uv"][0], 4) for k in ("madera.quemada", "piedra.basalto")}


def test_pivote_base_con_stretch(normalized: dict[str, Any], repo: Path) -> None:
    obj = _reimport(repo / "Export" / LOTE / "SM_Pack_Pico.fbx")[0]
    vs = _verts(obj)
    assert min(v.z for v in vs) == pytest.approx(0.0, abs=1e-4)
    assert (min(v.x for v in vs) + max(v.x for v in vs)) / 2 == pytest.approx(0.0, abs=1e-4)
    dims = nc.bbox_dims(vs)
    # stretch x 0.5: ancho 0.3 m (cabeza) frente a 0.9 m de alto (mango + media cabeza).
    assert dims[0] / dims[2] == pytest.approx(0.5 * 0.3 / 0.9, rel=0.02)


def test_rig_exportado_con_acciones_y_sin_curvas_huerfanas(normalized: dict[str, Any], repo: Path) -> None:
    objs = _reimport(repo / "Export" / LOTE / "SM_Pack_Cerdo.fbx")
    kinds = sorted(o.type for o in objs)
    assert kinds == ["ARMATURE", "MESH"]
    arm = next(o for o in objs if o.type == "ARMATURE")
    assert {b.name for b in arm.data.bones} == {"Root", "Head"}
    names = {a.name.split("|")[-1] for a in bpy.data.actions}
    assert {"Idle", "Run"} <= names


def test_scale_lleva_las_claves_de_location(repo: Path) -> None:
    """Escala 2: la clave de Run (0.1 m) pasa a 0.2 m; la curva de Tail1 desaparece."""
    nz.reset_scene()
    e = ENTRIES[3]
    mesh, arm = nz.import_rigged(nz.cache_dir() / "demo" / e["file"])
    paths = {fc.data_path for fc in nz.action_fcurves(bpy.data.actions["Run"])}
    assert not any("Tail1" in p for p in paths)
    s = nz.orient_scale_pivot_rig(mesh, arm, e)
    assert s == pytest.approx(2.0)
    fc = next(fc for fc in nz.action_fcurves(bpy.data.actions["Run"])
              if fc.data_path.endswith(".location") and fc.array_index == 1)
    assert max(k.co.y for k in fc.keyframe_points) == pytest.approx(0.2)


def test_solo_algunos_ids_conserva_el_resto_del_informe(normalized: dict[str, Any], repo: Path) -> None:
    report_path = repo / "Export" / LOTE / "_tiles" / "report.json"
    antes = json.loads(report_path.read_text(encoding="utf-8"))
    nz.main(["--lote", LOTE, "--ids", "hacha", "--no-export"])
    despues = json.loads(report_path.read_text(encoding="utf-8"))
    assert despues == antes


def test_analisis_no_exporta(repo: Path, capsys: pytest.CaptureFixture[str]) -> None:
    nz.main(["--lote", LOTE, "--analyze"])
    out = capsys.readouterr().out
    assert out.count("ANALYZE ") == 4
    # El color del mango se reconoce en el análisis (tras el viaje por glTF/OBJ/FBX).
    assert BROWN in out and PINK in out
    assert not (repo / "Export" / LOTE / "SM_Pack_Hacha.fbx").exists()


def test_analisis_agrupa_por_color(pack: Path) -> None:
    nz.reset_scene()
    obj = nz.import_file(pack / "demo" / "obj" / "pico.obj")
    clusters = nz.analyze(obj, "pico")
    assert [nc.to_hex(c["srgb"]) for c in clusters] == [BROWN, GREY]


# --- errores ---------------------------------------------------------------------------

def test_lote_sin_entradas_o_fichero_ausente(repo: Path) -> None:
    with pytest.raises(SystemExit, match="sin entradas"):
        nz.main(["--lote", "no-existe"])
    cat = json.loads(nz.CATALOG.read_text(encoding="utf-8"))
    cat["entries"] = [_entry("fantasma", "no/esta.glb")]
    nz.CATALOG.write_text(json.dumps(cat), encoding="utf-8")
    with pytest.raises(SystemExit, match="fetch_packs.py demo"):
        nz.main(["--lote", LOTE])


def test_accion_ausente_en_el_blend(repo: Path) -> None:
    cat = json.loads(nz.CATALOG.read_text(encoding="utf-8"))
    cerdo = json.loads(json.dumps(ENTRIES[3]))
    cerdo["rig"]["animations"].append("Swim")
    cat["entries"] = [cerdo]
    nz.CATALOG.write_text(json.dumps(cat), encoding="utf-8")
    with pytest.raises(SystemExit, match="Swim"):
        nz.main(["--lote", LOTE])


def test_formatos_no_admitidos(tmp_path: Path) -> None:
    with pytest.raises(ValueError, match="formato"):
        nz.import_file(tmp_path / "a.stl")
    with pytest.raises(ValueError, match="rig"):
        nz.import_rigged(tmp_path / "a.obj")


def test_fichero_sin_mallas(tmp_path: Path) -> None:
    nz.reset_scene()
    bpy.ops.object.empty_add()
    path = tmp_path / "vacio.glb"
    bpy.ops.export_scene.gltf(filepath=str(path), export_format="GLB")
    nz.reset_scene()
    with pytest.raises(RuntimeError, match="sin mallas"):
        nz.import_file(path)


def test_rig_sin_modificador_de_armadura(tmp_path: Path) -> None:
    path = tmp_path / "suelto.blend"
    _rigged(path, orphan=False, armature_modifier=False)
    nz.reset_scene()
    with pytest.raises(RuntimeError, match="no está deformada"):
        nz.import_rigged(path)


def test_stretch_con_rig_se_rechaza(pack: Path) -> None:
    nz.reset_scene()
    mesh, arm = nz.import_rigged(pack / "demo" / "Blends" / "Pig.blend")
    with pytest.raises(ValueError, match="stretch"):
        nz.orient_scale_pivot_rig(mesh, arm, {**ENTRIES[3], "stretch": [1, 2, 1]})


def test_muestra_fuera_de_la_paleta(pack: Path) -> None:
    nz.reset_scene()
    obj = nz.import_file(pack / "demo" / "obj" / "pico.obj")
    entry = _entry("pico", "x", recolor={"default": "no.existe"})
    with pytest.raises(KeyError, match="no.existe"):
        nz.recolor(obj, entry, PALETTE)


def test_tipos_de_objeto(pack: Path) -> None:
    nz.reset_scene()
    obj = nz.import_file(pack / "demo" / "obj" / "pico.obj")
    with pytest.raises(TypeError, match="armadura"):
        nz.armature_of(obj)
    empty = bpy.data.objects.new("vacio", None)
    with pytest.raises(TypeError, match="malla"):
        nz.mesh_of(empty)
    with pytest.raises(ValueError, match="esquinas"):
        nz.write_color_attr(obj, "X", [[0.0, 0.0, 0.0]])


def test_material_sin_bsdf_y_sin_material() -> None:
    assert nz.material_source(None) == (None, nz.GREY)
    mat = bpy.data.materials.new("plano")
    mat.diffuse_color = (1.0, 0.0, 0.0, 1.0)
    for node in [n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED"]:
        mat.node_tree.nodes.remove(node)
    sampler, color = nz.material_source(mat)
    assert sampler is None and color == pytest.approx((1.0, 0.0, 0.0))


def test_muestreo_de_imagen_envuelve_las_uv() -> None:
    img = bpy.data.images.new("t", 2, 1)
    img.pixels[:] = [1, 0, 0, 1, 0, 0, 1, 1]
    s = nz.ImageSampler(img)
    assert s.sample(0.25, 0.5) == pytest.approx((1, 0, 0))
    assert s.sample(0.75, 0.5) == pytest.approx((0, 0, 1))
    assert s.sample(1.25, -0.5) == pytest.approx((1, 0, 0))


def test_nombre_de_hueso_de_la_curva() -> None:
    assert nz.bone_of_path('pose.bones["Tail1"].location') == "Tail1"
    assert nz.bone_of_path("location") is None


def test_rotacion_por_defecto_es_identidad() -> None:
    assert nz.euler_matrix(None) == nz.euler_matrix([0, 0, 0])
    m = nz.euler_matrix([90, 0, 0])
    assert (m @ Vector((0, 1, 0))).z == pytest.approx(1.0)
