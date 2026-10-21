"""Kit de interfaz: cabecera de colores, hoja de iconos y que lo versionado está al día."""

from __future__ import annotations

from explored_style import ui_icons, ui_kit
from explored_style.palette import load_palette


def test_header_has_every_palette_color_and_icon() -> None:
    palette = load_palette()
    header = ui_kit.header_text(palette)
    for color in palette.colors:
        assert f'FColor {ui_kit.camel(color.name)}(' in header
    for name, _painter in ui_icons.ICONS:
        assert f'TEXT("{name}")' in header
    assert 'Count' in header


def test_camel_case() -> None:
    assert ui_kit.camel('arena_clara') == 'ArenaClara'
    assert ui_kit.camel('tinta') == 'Tinta'


def test_icons_fit_and_are_visible() -> None:
    sheet = ui_icons.icon_sheet()
    assert sheet.size == (ui_icons.CELL * ui_icons.COLUMNS, ui_icons.CELL * ui_icons.ROWS)
    for index, (name, _painter) in enumerate(ui_icons.ICONS):
        covered = ui_icons.coverage(sheet, index)
        assert 0.06 < covered < 0.75, f'{name}: {covered:.2f}'
    # La celda sin icono queda vacía.
    assert ui_icons.coverage(sheet, ui_icons.COLUMNS * ui_icons.ROWS - 1) == 0.0


def test_icon_ids_cover_the_17_items() -> None:
    names = {name for name, _painter in ui_icons.ICONS}
    items = {'cuchillo', 'cantimplora', 'barometro', 'coco', 'coco_vacio', 'lena', 'fibra', 'cuerda', 'cana_bambu',
             'cana_pescar', 'tronco', 'remo', 'pez_crudo', 'pez_asado', 'tarro_aceite', 'mecha', 'lente'}
    assert items <= names


def test_generated_files_are_up_to_date() -> None:
    assert ui_kit.main(['--check']) == 0


def test_check_reports_stale(tmp_path, monkeypatch) -> None:
    monkeypatch.setattr(ui_kit, 'HEADER_PATH', tmp_path / 'missing.h')
    assert ui_kit.main(['--check']) == 1


def test_main_writes_outputs(tmp_path, monkeypatch) -> None:
    for attr, name in (('HEADER_PATH', 'p.h'), ('SHEET_PATH', 's.png'), ('LAMINA_PATH', 'l.png')):
        monkeypatch.setattr(ui_kit, attr, tmp_path / name)
    assert ui_kit.main([]) == 0
    assert (tmp_path / 'p.h').read_text(encoding='utf-8').startswith('// Generado')
    assert (tmp_path / 's.png').stat().st_size > 0
    assert ui_kit.main(['--check']) == 0
