"""Láminas de previsualización (render_preview.py y props/preview_kit.py),
renderizadas de verdad a resolución mínima: Workbench para render_preview
(EEVEE sin GPU tarda minutos por imagen) y Cycles a 1 muestra para
preview_kit, que ya usa Cycles en CPU. Workbench necesita EGL de Mesa en el
sistema (libegl1, libegl-mesa0, libgl1-mesa-dri)."""

import json
import struct

import pytest
from conftest import importar


def _tamano_png(ruta):
    """(ancho, alto) leídos de la cabecera IHDR de un PNG."""
    with open(ruta, 'rb') as f:
        cabecera = f.read(24)
    assert cabecera[:8] == b'\x89PNG\r\n\x1a\n', f'{ruta} no es un PNG'
    return struct.unpack('>II', cabecera[16:24])


@pytest.fixture
def preview(monkeypatch, tmp_path):
    render_preview = importar('render_preview')
    monkeypatch.setattr(render_preview, 'RENDER_ENGINE', 'BLENDER_WORKBENCH')
    monkeypatch.setattr(render_preview, 'RESOLUTION_PERCENT', 2)
    monkeypatch.setattr(render_preview, 'DOCS_ART_DIR', str(tmp_path / 'docs'))
    return render_preview


def test_lamina_de_vegetacion(preview, kit_vegetacion, monkeypatch, tmp_path):
    salida, _ = kit_vegetacion
    monkeypatch.setattr(preview, 'EXPORT_DIR', str(salida))
    monkeypatch.setattr(preview, 'MANIFEST_PATH', str(salida / 'manifest.json'))
    monkeypatch.setattr(preview, 'PREVIEW_PATH', str(tmp_path / 'preview.png'))
    preview.main()
    assert _tamano_png(tmp_path / 'preview.png') == (32, 18)


def test_hojas_de_contacto_y_claro_de_selva(preview, kit_vegetacion, monkeypatch, tmp_path):
    salida, _ = kit_vegetacion
    monkeypatch.setattr(preview, 'EXPORT_DIR', str(salida))
    monkeypatch.setattr(preview, 'MANIFEST_PATH', str(salida / 'manifest.json'))
    preview.main_contact_sheets()
    for familia in preview.VEG_CONTACT_ROWS:
        assert _tamano_png(tmp_path / 'docs' / f'hoja_contacto_{familia}.png') == (32, 24)
    assert preview.build_scatter_clearing() is True
    assert _tamano_png(tmp_path / 'docs' / 'escena_claro_selva.png') == (38, 21)


def test_claro_de_selva_sin_mallas_no_renderiza(preview, tmp_path, monkeypatch):
    (tmp_path / 'manifest.json').write_text(json.dumps({'meshes': []}), encoding='utf-8')
    monkeypatch.setattr(preview, 'MANIFEST_PATH', str(tmp_path / 'manifest.json'))
    assert preview.build_scatter_clearing() is False
    assert preview._render_vegetation_group([], 1.0, str(tmp_path / 'x.png')) is False
    assert preview._render_props_group([], 1.0, str(tmp_path / 'x.png')) is False


def test_laminas_de_props_una_por_grupo(preview, kit_props, monkeypatch, tmp_path):
    """Una lámina por grupo narrativo (con un par de props de cada grupo
    para no reimportar el kit entero)."""
    salida, manifest, _ = kit_props
    por_grupo = {}
    for m in manifest['meshes']:
        por_grupo.setdefault(m['group'], [])
        if len(por_grupo[m['group']]) < 2:
            por_grupo[m['group']].append(m)
    reducido = {'meshes': [m for ms in por_grupo.values() for m in ms]}
    (tmp_path / 'manifest.json').write_text(json.dumps(reducido), encoding='utf-8')
    monkeypatch.setattr(preview, 'EXPORT_DIR_PROPS', str(salida))
    monkeypatch.setattr(preview, 'MANIFEST_PATH_PROPS', str(tmp_path / 'manifest.json'))
    escritas = []
    original = preview._render_props_group

    def espia(entries, target_height, out_path, **kw):
        out_path = str(tmp_path / 'props' / out_path.rsplit('/', 1)[-1])
        escritas.append(out_path)
        return original(entries, target_height, out_path, **kw)

    monkeypatch.setattr(preview, '_render_props_group', espia)
    preview.main_props()
    # regresión: ProduccionBase faltaba en GROUP_ORDER_PROPS y su lámina
    # no se generaba nunca
    assert sorted(ruta.rsplit('preview_', 1)[1][:-4] for ruta in escritas) == sorted(por_grupo)
    assert set(preview.GROUP_ORDER_PROPS) == set(preview.GROUP_TARGET_HEIGHT_PROPS)
    for ruta in escritas:
        assert _tamano_png(ruta) == (32, 24)


# ---------------------------------------------------------------------------
# props/preview_kit.py (Cycles en CPU)
# ---------------------------------------------------------------------------
@pytest.fixture
def preview_kit(monkeypatch, tmp_path):
    kit = importar('preview_kit')
    monkeypatch.setattr(kit, 'OUT_DIR', str(tmp_path))
    return kit


def _argv(monkeypatch, *opciones):
    monkeypatch.setattr('sys.argv', ['blender', '--', '--samples=1', '--res=48x32', *opciones])


def test_catalogo_y_montaje_del_kit_de_construccion(preview_kit, monkeypatch, tmp_path):
    _argv(monkeypatch, '--materials=Palm')
    preview_kit.main()
    assert _tamano_png(tmp_path / 'kit-construccion-palma.png') == (48, 32)
    assert _tamano_png(tmp_path / 'kit-construccion-montaje.png') == (48, 32)


def test_lamina_de_modulo_y_mosaico(preview_kit, monkeypatch, tmp_path):
    _argv(monkeypatch, '--mode=module', '--module=beacon', '--out=balizas', '--normalize=1.0', '--cols=2')
    preview_kit.main()
    assert _tamano_png(tmp_path / 'balizas.png') == (48, 32)
    _argv(monkeypatch, '--mode=tiles', '--module=items_botica', '--out=botica', '--cols=2',
          '--only=Item_HijueloPlatano,Item_PlantaMedicinalAloe')
    preview_kit.main()
    assert _tamano_png(tmp_path / 'botica.png') == (48, 32)


def test_shrink_png_reduce_hasta_el_limite(preview_kit, tmp_path):
    import numpy as np
    ruta = tmp_path / 'grande.png'
    common = importar('common')
    w, h = 200, 150
    ruido = np.random.default_rng(0).normal(0.0, 0.05, (h, w, 4))
    px = np.clip(np.linspace(0.0, 1.0, w * h).reshape(h, w, 1) + ruido, 0.0, 1.0).astype(np.float32)
    px[..., 3] = 1.0
    img = common.bpy.data.images.new('Grande', w, h)
    img.pixels = px.ravel()
    img.filepath_raw = str(ruta)
    img.file_format = 'PNG'
    img.save()
    tam = ruta.stat().st_size
    preview_kit._shrink_png(str(ruta), limit=int(tam * 0.7))
    assert ruta.stat().st_size <= tam * 0.7
    assert _tamano_png(ruta)[0] < w
    # por debajo del límite no toca el fichero
    antes = ruta.stat().st_mtime_ns
    preview_kit._shrink_png(str(ruta), limit=tam)
    assert ruta.stat().st_mtime_ns == antes


def test_variante_inexistente(preview_kit):
    with pytest.raises(KeyError):
        preview_kit._variant('Palm', 'NoExiste')
