"""Icono del ejecutable (explored_style.app_icon)."""

from __future__ import annotations

import io
from pathlib import Path

from PIL import Image

from explored_style import app_icon
from explored_style.palette import load_palette


def _px(image: Image.Image, x: int, y: int) -> tuple[int, ...]:
    pixel = image.getpixel((x, y))
    assert isinstance(pixel, tuple)
    return tuple(int(v) for v in pixel)


def test_icono_cuadrado_con_esquinas_transparentes_y_colores_de_la_paleta() -> None:
    palette = load_palette()
    icon = app_icon.draw_icon(palette)
    size = app_icon.SIZE
    assert icon.size == (size, size)
    assert icon.mode == 'RGBA'
    assert _px(icon, 0, 0)[3] == 0
    assert _px(icon, size // 2, int(size * 0.06)) == (*palette.by_name('noche_cenit').rgb, 255)
    assert _px(icon, size // 2, int(size * 0.40))[:3] == palette.by_name('blanco_nube').rgb


def test_ico_lleva_todos_los_tamanos() -> None:
    data = app_icon.ico_bytes(app_icon.draw_icon(load_palette()))
    ico = Image.open(io.BytesIO(data))
    assert ico.format == 'ICO'
    assert set(ico.info['sizes']) == set(app_icon.ICO_SIZES)


def test_main_escribe_y_check_detecta_desfase(tmp_path: Path) -> None:
    icon_path = tmp_path / 'Build' / 'Application.ico'
    lamina_path = tmp_path / 'laminas' / 'icono.png'
    assert app_icon.main(['--check'], icon_path, lamina_path) == 1
    assert app_icon.main([], icon_path, lamina_path) == 0
    assert icon_path.exists() and lamina_path.exists()
    assert app_icon.main(['--check'], icon_path, lamina_path) == 0
    icon_path.write_bytes(b'otro')
    assert app_icon.main(['--check'], icon_path, lamina_path) == 1


def test_icono_versionado_al_dia() -> None:
    assert app_icon.main(['--check']) == 0
