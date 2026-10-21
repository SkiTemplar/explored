import json
import zlib

import numpy as np
import pytest

from explored_textures import strokes


@pytest.fixture(scope='module')
def config():
    return strokes.load_config()


@pytest.fixture(scope='module')
def small(config):
    return strokes.generate({**config, 'size': 128, 'strokes': 120})


def _decode(data: bytes) -> np.ndarray:
    assert data[:8] == b'\x89PNG\r\n\x1a\n'
    w, h = int.from_bytes(data[16:20], 'big'), int.from_bytes(data[20:24], 'big')
    assert data[24:26] == bytes([8, 0])  # 8 bits, escala de grises
    start = data.index(b'IDAT') + 4
    length = int.from_bytes(data[start - 8:start - 4], 'big')
    raw = zlib.decompress(data[start:start + length])
    rows = np.frombuffer(raw, dtype=np.uint8).reshape(h, w + 1)
    assert (rows[:, 0] == 0).all()
    return rows[:, 1:]


def test_config_del_repo_completa(config):
    assert config['texture'] == 'T_TerrainStrokes'
    assert config['size'] == 512
    assert 0.0 < config['dark_factor'] < 1.0 < config['light_factor'] < 1.3
    assert len(config['tiles_m']) == len(config['weights']) == 2


def test_config_invalida(tmp_path, config):
    bad = tmp_path / 'malo.json'
    bad.write_text(json.dumps({k: v for k, v in config.items() if k != 'strokes'}), encoding='utf-8')
    with pytest.raises(ValueError, match='strokes'):
        strokes.load_config(bad)
    bad.write_text(json.dumps({**config, 'size': 100}), encoding='utf-8')
    with pytest.raises(ValueError, match='potencia'):
        strokes.load_config(bad)


def test_media_neutra_y_contraste(small):
    assert small.shape == (128, 128)
    assert small.mean() == pytest.approx(0.5, abs=0.01)
    assert 0.0 <= small.min() and small.max() <= 1.0
    # Trazos oscuros y claros bien separados del fondo.
    assert (small < 0.4).mean() > 0.1 and (small > 0.6).mean() > 0.1


def test_tileable_sin_costura(small):
    interior = np.abs(np.diff(small, axis=1)).mean()
    seam = np.abs(small[:, 0] - small[:, -1]).mean()
    assert seam < interior * 1.6
    interior_v = np.abs(np.diff(small, axis=0)).mean()
    seam_v = np.abs(small[0] - small[-1]).mean()
    assert seam_v < interior_v * 1.6


def test_trazos_orientados(small):
    # Los trazos van a ~65°: la imagen varía más a lo ancho del trazo que a lo largo.
    angle = np.radians(65.0)
    gy, gx = np.gradient(small)
    along = np.abs(gx * np.cos(angle) + gy * np.sin(angle)).mean()
    across = np.abs(-gx * np.sin(angle) + gy * np.cos(angle)).mean()
    assert across > along * 1.3


def test_trazo_envuelve_el_borde():
    canvas = np.full((32, 32), 0.5)
    out = strokes.draw_stroke(canvas, (1.0, 16.0), 0.0, 10.0, 3.0, 0.0)
    assert out[16, 0] < 0.1 and out[16, 30] < 0.1  # el trazo cruza el borde izquierdo y sale por el derecho
    assert canvas[16, 0] == 0.5  # no muta la entrada


def test_determinista(config):
    a = strokes.generate({**config, 'size': 64, 'strokes': 30})
    b = strokes.generate({**config, 'size': 64, 'strokes': 30})
    assert np.array_equal(a, b)


def test_main_escribe_png(tmp_path, config):
    cfg = tmp_path / 'cfg.json'
    cfg.write_text(json.dumps({**config, 'size': 64, 'strokes': 40}), encoding='utf-8')
    out = tmp_path / 'T.png'
    assert strokes.main(['--config', str(cfg), '--out', str(out)]) == 0
    pixels = _decode(out.read_bytes())
    assert pixels.shape == (64, 64)
    assert abs(pixels.mean() - 127.5) < 3.0
