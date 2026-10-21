"""Kit K6 Césped: criterios de aceptación medidos sobre los FBX reimportados.

El kit entero (FBX, manifiesto y lámina) se genera UNA vez por sesión en un
directorio temporal con `k6_cesped.main`; nada se escribe en el repo.
"""

import dataclasses
import json
import math

import bpy  # noqa: F401  (debe importarse antes que mathutils)

import bmesh
import numpy as np
import pytest
from conftest import hash_malla
from explored_style import check_style, meshkit, raster
from explored_style.palette import cell_of_uv, load_palette
from mathutils import Vector

from kits import k6_cesped, k6_continuo, k6_formas, k6_lamina, k6_parches

PAL = load_palette()
PALETA_RGB = {c.rgb: c.name for c in PAL.colors}
NOMBRES = ['SM_GrassTuft_A_01', 'SM_GrassTuft_B_01', 'SM_GrassTuft_C_01', 'SM_GrassTall_01',
           'SM_FlowerA_01', 'SM_FlowerB_01', 'SM_Fern_01', 'SM_Clover_01',
           'SM_GrassPatch_A_01', 'SM_GrassPatch_B_01', 'SM_GrassPatch_C_01']
MATAS = [s for s in k6_cesped.K6 if s.pieces is not None]          # matas sueltas y parches
PARCHES = [s for s in k6_cesped.K6 if s.is_patch]
BASE_Z = 0.002          # m: vértices «en el suelo»
BYTE = 1.0 / 255.0


@pytest.fixture(scope='session')
def kit_k6(tmp_path_factory):
    """FBX, manifiesto y lámina (al 10 % de resolución) en un directorio temporal."""
    raiz = tmp_path_factory.mktemp('K6')
    salida, lamina, continuo = raiz / 'K6_Cesped', raiz / 'k6-cesped.png', raiz / 'k6-cesped-continuo.png'
    assert k6_cesped.main(['--out', str(salida), '--lamina', str(lamina), '--continuo', str(continuo),
                           '--percent', '10']) == 0
    manifiesto = json.loads((salida / 'manifest.json').read_text(encoding='utf-8'))
    return salida, lamina, {e['malla']: e for e in manifiesto['mallas']}


@pytest.fixture(scope='session')
def manifiesto_k6(kit_k6):
    salida, _lamina, _entradas = kit_k6
    return json.loads((salida / 'manifest.json').read_text(encoding='utf-8'))


def _reimportar(ruta):
    """Escena vacía con solo el FBX; devuelve su única malla."""
    meshkit.reset_scene()
    bpy.ops.import_scene.fbx(filepath=str(ruta))
    mallas = [o for o in bpy.data.objects if o.type == 'MESH']
    assert len(mallas) == 1, f'{ruta.name}: {len(mallas)} mallas'
    return mallas[0]


def _esquinas(obj):
    """(posición en mundo, RGB sRGB 0-255, alfa, índice de cara, índice de vértice) de cada esquina."""
    me = obj.data
    col = me.color_attributes[k6_cesped.COLOR_ATTR]
    out = []
    for poly in me.polygons:
        for li in poly.loop_indices:
            r, g, b, a = col.data[li].color_srgb
            vi = me.loops[li].vertex_index
            co = obj.matrix_world @ me.vertices[vi].co
            out.append((co, (round(r * 255), round(g * 255), round(b * 255)), a, poly.index, vi))
    return out


def _islas(obj):
    """Lista de conjuntos de índices de vértices conexos."""
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.verts.ensure_lookup_table()
    vistos, islas = set(), []
    for inicio in bm.verts:
        if inicio.index in vistos:
            continue
        pila, isla = [inicio], {inicio.index}
        vistos.add(inicio.index)
        while pila:
            v = pila.pop()
            for e in v.link_edges:
                o = e.other_vert(v)
                if o.index not in vistos:
                    vistos.add(o.index)
                    isla.add(o.index)
                    pila.append(o)
        islas.append(isla)
    bm.free()
    return islas


def test_catalogo_y_criterios():
    assert [s.asset for s in k6_cesped.K6] == NOMBRES
    assert k6_cesped.spec_named('SM_Fern').tris == (40, 120)
    with pytest.raises(KeyError):
        k6_cesped.spec_named('SM_Inexistente')
    topes = {s.asset: s.tris[1] for s in k6_cesped.K6}
    assert topes == {'SM_GrassTuft_A_01': 40, 'SM_GrassTuft_B_01': 40, 'SM_GrassTuft_C_01': 40,
                     'SM_GrassTall_01': 60, 'SM_FlowerA_01': 50, 'SM_FlowerB_01': 50, 'SM_Fern_01': 120,
                     'SM_Clover_01': 40, 'SM_GrassPatch_A_01': 80, 'SM_GrassPatch_B_01': 80,
                     'SM_GrassPatch_C_01': 80}
    sueltas = [s for s in MATAS if not s.is_patch]
    assert all(s.tris[0] == 16 and s.height == (0.25, 0.45) and s.pieces == (5, 9) for s in sueltas)
    assert k6_cesped.spec_named('SM_GrassTall').height == (0.6, 0.8)
    assert [s.name for s in PARCHES] == ['SM_GrassPatch_A', 'SM_GrassPatch_B', 'SM_GrassPatch_C']
    for s in PARCHES:
        assert s.tris == (48, 80) and s.height == (0.25, 0.55) and s.pieces == (12, 18)
        assert s.footprint == (0.35, 0.45) and s.width_max == pytest.approx(0.9)
        forma = k6_parches.PATCHES[s.name]
        assert 12 <= forma.blades <= 18 and 48 <= forma.triangles <= 80


def test_manifiesto(kit_k6):
    salida, _lamina, entradas = kit_k6
    assert list(entradas) == NOMBRES
    for spec in k6_cesped.K6:
        e = entradas[spec.asset]
        assert (salida / e['fbx']).is_file()
        assert spec.tris[0] <= e['tris'] <= e['tope_tris'] == spec.tris[1]
        assert e['colision'] == 'ninguna' and e['viento']['canal'] == 'Col.A'
        assert e['viento']['punta'] == spec.tip_weight
        assert set(e['colores']) <= {c.name for c in PAL.colors}
        assert e['licencia'] and e['origen'].startswith('generado por Tools/Blender/kits/k6_cesped.py')
    assert {entradas[s.asset]['hojas'] for s in MATAS if not s.is_patch} == {7, 9, 5}
    for s in PARCHES:
        e = entradas[s.asset]
        assert 12 <= e['hojas'] <= 18 and e['hojas'] == k6_parches.PATCHES[s.name].blades
        assert 0.35 <= e['radio_huella_m'] <= 0.45 and e['lod'] == 'una sola LOD'
        assert e['tris'] == k6_parches.PATCHES[s.name].triangles


def test_manifiesto_mide_el_cesped_continuo(manifiesto_k6):
    assert 'parches' in manifiesto_k6['notas'] and '11 mallas' in manifiesto_k6['notas']['importacion']
    c = manifiesto_k6['cesped_continuo']
    assert c['instancias_en_16_m2'] == {'matas': 35, 'parches': 88}
    assert c['tris_por_parche'] == {s.asset: manifiesto_k6['mallas'][NOMBRES.index(s.asset)]['tris']
                                    for s in PARCHES}
    assert 48 * 5.5 <= c['tris_por_m2']['parches'] <= 80 * 5.5
    for clave in ('suelo_visible_cenital_pct', 'suelo_visible_cenital_interior_3x3_m_pct',
                  'suelo_visible_desde_1_7_m_a_30_grados_pct'):
        visible = c[clave]
        # alfombra: los parches dejan ver menos de una cuarta parte del suelo y menos de un tercio que las matas
        assert visible['parches'] < 25.0 and visible['parches'] < visible['matas'] / 3


@pytest.mark.parametrize('spec', k6_cesped.K6, ids=lambda s: s.name)
def test_fbx_reimportado_cumple_criterios(kit_k6, spec):
    salida, _lamina, entradas = kit_k6
    obj = _reimportar(salida / f'{spec.asset}.fbx')
    tris = meshkit.triangle_count(obj)
    assert spec.tris[0] <= tris <= spec.tris[1]
    assert tris == entradas[spec.asset]['tris']
    puntos = [obj.matrix_world @ v.co for v in obj.data.vertices]
    xs, ys, zs = ([p[k] for p in puntos] for k in range(3))
    assert abs(min(zs)) < 0.001, 'pivote fuera de la base'
    assert min(xs) < 0.0 < max(xs) and min(ys) < 0.0 < max(ys), 'pivote fuera de la huella'
    assert spec.height[0] <= max(zs) - min(zs) <= spec.height[1]
    assert max(max(xs) - min(xs), max(ys) - min(ys)) <= spec.width_max
    for dim, esperado in zip(obj.dimensions, entradas[spec.asset]['dimensiones_m'], strict=True):
        assert dim == pytest.approx(esperado, abs=0.002)
    if spec.pieces is not None:
        assert spec.pieces[0] <= len(_islas(obj)) <= spec.pieces[1]
    if spec.footprint is not None:
        radio = max(math.hypot(p.x, p.y) for p in puntos)
        assert spec.footprint[0] <= radio <= spec.footprint[1]
        assert radio == pytest.approx(entradas[spec.asset]['radio_huella_m'], abs=0.002)


@pytest.mark.parametrize('spec', k6_cesped.K6, ids=lambda s: s.name)
def test_colores_de_vertice_en_paleta(kit_k6, spec):
    salida, _lamina, _entradas = kit_k6
    obj = _reimportar(salida / f'{spec.asset}.fbx')
    uv = obj.data.uv_layers[0]
    celda = {p.index: cell_of_uv(*uv.data[p.loop_indices[0]].uv) for p in obj.data.polygons}
    for _co, rgb, _a, cara, _v in _esquinas(obj):
        assert rgb in PALETA_RGB, f'{rgb} no está en paleta.json'
        indice = celda[cara]
        assert indice is not None, 'cara con la UV fuera de una celda de la paleta'
        assert PAL.colors[indice].rgb == rgb, 'el color por vértice no coincide con la celda UV'


@pytest.mark.parametrize('spec', k6_cesped.K6, ids=lambda s: s.name)
def test_peso_de_viento_en_alfa(kit_k6, spec):
    salida, _lamina, _entradas = kit_k6
    obj = _reimportar(salida / f'{spec.asset}.fbx')
    esquinas = _esquinas(obj)
    alfas = [a for _co, _rgb, a, *_ in esquinas]
    assert all(0.0 <= a <= spec.tip_weight + BYTE for a in alfas)
    assert max(alfas) == pytest.approx(spec.tip_weight, abs=BYTE)
    suelo = [a for co, _rgb, a, *_ in esquinas if co.z <= BASE_Z]
    assert suelo and max(suelo) <= BYTE, 'la base tiene que pesar 0'
    # el peso crece con la altura (hojas, tallos y flores): correlación clara con z
    d = [co.z for co, *_ in esquinas]
    md, ma = sum(d) / len(d), sum(alfas) / len(alfas)
    cov = sum((x - md) * (y - ma) for x, y in zip(d, alfas, strict=True))
    var = math.sqrt(sum((x - md) ** 2 for x in d) * sum((y - ma) ** 2 for y in alfas))
    # las hojas de roseta de los parches se tumban: su punta (peso 1) queda baja, así que
    # en ellos la correlación con z es menor y se exige además que el peso crezca a lo largo
    # de cada hoja (test_parche_peso_crece_a_lo_largo_de_cada_hoja)
    assert cov / var > (0.5 if spec.is_patch else 0.8)


@pytest.mark.parametrize('spec', PARCHES, ids=lambda s: s.name)
def test_parche_peso_crece_a_lo_largo_de_cada_hoja(kit_k6, spec):
    salida, _lamina, _entradas = kit_k6
    obj = _reimportar(salida / f'{spec.asset}.fbx')
    esquinas = _esquinas(obj)
    for isla in _islas(obj):
        propias = {e[4]: e for e in esquinas if e[4] in isla}.values()
        pie = [co for co, _rgb, a, *_ in propias if a <= BYTE]
        assert len(pie) == 2 and all(co.z <= BASE_Z for co in pie), 'cada hoja nace en el suelo con peso 0'
        centro = (pie[0] + pie[1]) / 2
        niveles = sorted({round(a, 3) for _co, _rgb, a, *_ in propias})
        assert niveles[-1] == pytest.approx(1.0, abs=BYTE)
        distancias = [max((co - centro).length for co, _rgb, a, *_ in propias if round(a, 3) == n) for n in niveles]
        assert distancias == sorted(distancias) and len(set(distancias)) == len(distancias)


@pytest.mark.parametrize('spec', MATAS, ids=lambda s: s.name)
def test_matas_con_punta_mas_clara(kit_k6, spec):
    salida, _lamina, _entradas = kit_k6
    obj = _reimportar(salida / f'{spec.asset}.fbx')
    esquinas = _esquinas(obj)
    verdes = set()
    for isla in _islas(obj):
        propias = [e for e in esquinas if e[4] in isla]
        base = min(propias, key=lambda e: e[2])
        punta = max(propias, key=lambda e: e[2])
        assert raster.luminance(punta[1]) > raster.luminance(base[1])
        verdes |= {PALETA_RGB[e[1]] for e in propias}
    assert 2 <= len(verdes) <= 3 and verdes <= k6_cesped.GREENS


@pytest.mark.parametrize('spec', k6_cesped.K6, ids=lambda s: s.name)
def test_fbx_pasa_check_style(kit_k6, spec):
    salida, _lamina, _entradas = kit_k6
    report = check_style.check_file(salida / f'{spec.asset}.fbx', spec.tris[1])
    assert not report.errors and all(m.ok for m in report.meshes), check_style.format_report(report)
    assert len(report.meshes) == 1


def test_generacion_determinista():
    for spec in k6_cesped.K6:
        meshkit.reset_scene()
        a = hash_malla(k6_cesped.generate(spec))
        meshkit.reset_scene()
        assert hash_malla(k6_cesped.generate(spec)) == a


def test_lamina(kit_k6):
    _salida, lamina, _entradas = kit_k6
    imagen = raster.decode_png(lamina.read_bytes())
    assert imagen.shape[:2] == (k6_lamina.HEIGHT, k6_lamina.WIDTH)
    assert k6_lamina.OUT_PATH.name == 'k6-cesped.png'
    continuo = raster.decode_png((lamina.parent / 'k6-cesped-continuo.png').read_bytes())
    assert continuo.shape[:2] == (k6_continuo.HEIGHT, k6_continuo.WIDTH)
    assert k6_continuo.OUT_PATH.name == 'k6-cesped-continuo.png'


def _cobertura(verts, caras, radio, pitch=None, acimut=0.0, n=81):
    """Fracción del disco de «radio» en el suelo que tapan las caras. Sin «pitch»,
    vista cenital; con él, vista ortográfica que baja «pitch» grados (cada vértice se
    proyecta al suelo a lo largo del rayo de la cámara)."""
    if pitch is not None:
        d = Vector((math.cos(acimut), math.sin(acimut), 0.0)) * math.cos(math.radians(pitch))
        d.z = math.sin(math.radians(pitch))
        verts = [v - d * (v.z / d.z) for v in verts]
    eje = np.linspace(-radio, radio, n)
    x, y = np.meshgrid(eje, eje)
    dentro = x * x + y * y <= radio * radio
    tapado = np.zeros_like(dentro)
    for cara in caras:
        for k in range(1, len(cara) - 1):
            a, b, c = (verts[cara[0]], verts[cara[k]], verts[cara[k + 1]])
            det = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y)
            if abs(det) < 1e-12:
                continue
            l1 = ((b.y - c.y) * (x - c.x) + (c.x - b.x) * (y - c.y)) / det
            l2 = ((c.y - a.y) * (x - c.x) + (a.x - c.x) * (y - c.y)) / det
            tapado |= (l1 >= 0) & (l2 >= 0) & (l1 + l2 <= 1)
    return float((tapado & dentro).sum() / dentro.sum())


@pytest.mark.parametrize('spec', PARCHES, ids=lambda s: s.name)
def test_parche_sin_agujero_central(kit_k6, spec):
    """Las hojas de la base se solapan: el centro de la huella queda tapado desde arriba
    y desde la cámara de juego (30° hacia abajo), mire desde donde mire."""
    salida, _lamina, _entradas = kit_k6
    obj = _reimportar(salida / f'{spec.asset}.fbx')
    verts = [obj.matrix_world @ v.co for v in obj.data.vertices]
    caras = [tuple(p.vertices) for p in obj.data.polygons]
    assert _cobertura(verts, caras, 0.2) >= 0.9
    assert min(_cobertura(verts, caras, 0.1, 30.0, k * math.pi / 4) for k in range(8)) >= 0.8


def test_cobertura_de_un_cuadrado():
    cuadro = [Vector((-0.6, -0.6, 0.0)), Vector((0.6, -0.6, 0.0)), Vector((0.6, 0.6, 0.0)), Vector((-0.6, 0.6, 0.0))]
    assert _cobertura(cuadro, [(0, 1, 2, 3)], 0.5) == 1.0
    alto = [v + Vector((0.0, 0.0, 1.0)) for v in cuadro]
    # elevado 1 m y visto a 30° desde +X: lo que tapa en el suelo se va 1,73 m hacia -X
    assert _cobertura(alto, [(0, 1, 2, 3)], 0.5) == 1.0
    assert _cobertura(alto, [(0, 1, 2, 3)], 0.5, 30.0, 0.0) == 0.0


def test_siembra_del_cesped_continuo():
    parches = k6_continuo.sow(k6_continuo.PATCHES, 5.5, (0.8, 1.3))
    assert parches == k6_continuo.sow(k6_continuo.PATCHES, 5.5, (0.8, 1.3))
    assert len(parches) == 88 and len(k6_continuo.sow(k6_continuo.TUFTS, 2.2, (0.85, 1.2))) == 35
    mitad = k6_continuo.PARCEL / 2
    assert all(-mitad <= x <= mitad and -mitad <= y <= mitad and 0.8 <= s <= 1.3 for _n, x, y, _g, s in parches)
    assert {n for n, *_ in parches} == set(k6_continuo.PATCHES)
    assert k6_continuo.TUFTS == ('SM_GrassTuft_A_01', 'SM_GrassTuft_B_01', 'SM_GrassTuft_C_01')


def test_testigo_cuenta_pixeles_sin_borde():
    imagen = np.zeros((10, 10, 4), dtype=np.float32)
    imagen[..., 3] = 1.0
    imagen[0, :, 0] = 1.0                       # fila de arriba en rojo puro
    assert k6_continuo._marker_count(imagen, (255, 0, 0)) == 10
    assert k6_continuo._marker_count(imagen, (255, 0, 0), border=0.1) == 0
    assert k6_continuo._marker_count(imagen, (0, 0, 0), border=0.1) == 64


def test_continuo_sin_fbx_falla(tmp_path):
    with pytest.raises(FileNotFoundError):
        k6_continuo.make_sheet(tmp_path / 'x.png', export_dir=tmp_path)


def test_siembra_de_la_parcela():
    nombres = list(k6_lamina.MIX)
    assert sorted(nombres) == sorted(s.asset for s in k6_lamina.TUFT_KIT)
    assert not any(s.is_patch for s in k6_lamina.TUFT_KIT)
    plantas = k6_lamina.sow(nombres)
    assert plantas == k6_lamina.sow(nombres)
    assert 300 <= len(plantas) <= 400            # 16 m² a densidad de prado
    mitad = k6_lamina.PARCEL / 2
    assert all(-mitad <= x <= mitad and -mitad <= y <= mitad for _n, x, y, _g, _s in plantas)
    assert {n for n, *_ in plantas} == set(nombres)
    xs = k6_lamina.row_positions({'a': 1.0, 'b': 0.5})
    assert xs['b'] - xs['a'] == pytest.approx(0.75 + k6_lamina.ROW_GAP)


def test_lamina_sin_fbx_falla(tmp_path):
    with pytest.raises(FileNotFoundError):
        k6_lamina.make_sheet(tmp_path / 'x.png', export_dir=tmp_path)


def test_pesos_acotados_y_obligatorios():
    b = k6_formas.WindBuilder()
    assert b.weights == [] and b.vert((0, 0, 0), 2.0) == 0 and b.vert((1, 0, 0), -1.0) == 1
    assert b.weights == [1.0, 0.0]
    spec = k6_cesped.spec_named('SM_Clover')

    def sin_pesos():
        rota = k6_formas.build_clover()
        rota.weights.pop()
        return rota

    with pytest.raises(ValueError, match='peso de viento'):
        k6_cesped.generate(dataclasses.replace(spec, build=sin_pesos))


def test_exportador_sin_color_srgb_falla(monkeypatch, tmp_path):
    meshkit.reset_scene()
    obj = k6_cesped.generate(k6_cesped.spec_named('SM_GrassTuft_A'))
    monkeypatch.setattr(k6_cesped, '_enum_ids', lambda _op, _prop: ['NONE'])
    with pytest.raises(RuntimeError, match='sRGB'):
        k6_cesped.export_fbx(obj, tmp_path / 'x.fbx')


def test_hoja_curvada_y_afilada():
    hoja = k6_formas.Blade(azimuth=0.0, length=0.3, tilt=10.0, bend=40.0, width=0.02,
                           colors=(k6_formas.C['hierba_oscura'], k6_formas.C['hierba_clara']))
    nervio = k6_formas.blade_spine(hoja)
    assert len(nervio) == 3 and hoja.triangles == 3
    assert sum((b - a).length for a, b in zip(nervio, nervio[1:], strict=False)) == pytest.approx(0.3)
    # la punta se inclina más que el primer tramo (curva)
    t0, t1 = (nervio[1] - nervio[0]).normalized(), (nervio[2] - nervio[1]).normalized()
    assert t1.dot(Vector((0.0, 0.0, 1.0))) < t0.dot(Vector((0.0, 0.0, 1.0)))
