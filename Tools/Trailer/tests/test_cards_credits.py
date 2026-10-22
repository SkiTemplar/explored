from __future__ import annotations

import json
from pathlib import Path

import pytest
from conftest import REPO
from PIL import Image

from explored_trailer.cards import (
    CYCLONE,
    ENGINE,
    FREIGHTER,
    GENRE,
    INTRO,
    STRANDED,
    TAGLINE,
    TITLE,
    WORDS,
    Fonts,
    Palette,
    card_plan,
    render_cards,
)
from explored_trailer.credits import (
    attribution,
    description_markdown,
    load_pieces,
    used_piece_ids,
    write_credits,
)
from explored_trailer.music import music_plan
from explored_trailer.timeline import nominal_timeline

FONTS = REPO / "Content/Slate/Fonts"
PALETTE = REPO / "Tools/Style/paleta.json"
SOURCES = REPO / "Tools/Audio/music_sources.json"


def test_spanish_copy_is_exactly_the_approved_text() -> None:
    assert INTRO.es == "Iria Valdés apaga faros para ganarse la vida."
    assert STRANDED.es == "Esta vez es ella quien naufraga."
    assert [w.es for w in WORDS] == ["Fuego.", "Pesca.", "Cuerda.", "Aceite."]
    assert CYCLONE.es == "El ciclón llega la segunda noche."
    assert FREIGHTER.es == "Una lente. Una llama. Una oportunidad."
    assert (TITLE.es, TAGLINE.es) == ("BALIZA SIETE", "Enciende la luz.")
    assert GENRE.es == "Supervivencia en primera persona · PC (Windows)"
    assert ENGINE.es == "Hecho con Unreal Engine"


def test_english_copy() -> None:
    assert INTRO.en == "Iria Valdés switches off lighthouses for a living."
    assert STRANDED.en == "This time, she is the one who is stranded."
    assert [w.en for w in WORDS] == ["Fire.", "Fish.", "Rope.", "Oil."]
    assert CYCLONE.en == "The cyclone arrives on the second night."
    assert FREIGHTER.en == "One lens. One flame. One chance."
    assert TAGLINE.en == "Light the beacon." and GENRE.en == "First-person survival · PC (Windows)"


def test_card_times_follow_the_brief() -> None:
    plan = {c.card_id: c for c in card_plan(nominal_timeline())}
    assert (plan["01_intro"].start, plan["01_intro"].end) == (0.8, 3.6)
    assert (plan["02_naufraga"].start, plan["02_naufraga"].end) == (6.5, 11.0)
    assert plan["03_palabra_1"].start == 13.0 and plan["03_palabra_4"].end == 21.0
    assert (plan["05_ciclon"].start, plan["05_ciclon"].end) == (33.0, 40.0)
    assert (plan["06_carguero"].start, plan["06_carguero"].end) == (43.0, 49.0)
    assert (plan["08_titulo"].start, plan["08_titulo"].end) == (69.0, 74.0)
    assert all(c.end - c.start >= 1.5 for c in plan.values())


def test_palette_reads_game_colours() -> None:
    palette = Palette.load(PALETTE)
    assert palette.paper == "#F6EEDB" and palette.ink == "#2B2722" and palette.brass == "#D9A441"


def test_missing_font_is_reported(tmp_path: Path) -> None:
    with pytest.raises(FileNotFoundError):
        Fonts.from_dir(tmp_path)


@pytest.mark.parametrize("lang", ["es", "en"])
def test_rendered_cards_are_transparent_cutouts_inside_the_frame(tmp_path: Path, lang: str) -> None:
    cards = render_cards(nominal_timeline(), lang, tmp_path, Fonts.from_dir(FONTS), Palette.load(PALETTE))  # type: ignore[arg-type]
    assert len(cards) == 9
    for rc in cards:
        image = Image.open(rc.path)
        assert image.mode == "RGBA"
        assert image.getpixel((0, 0))[3] < 40  # type: ignore[index]
        assert rc.x >= 0 and rc.y >= 0 and rc.x % 2 == 0 and rc.y % 2 == 0
        assert rc.x + image.width <= 1920 and rc.y + image.height <= 1080


def test_credits_attribute_every_cc_by_piece(tmp_path: Path) -> None:
    cuts = music_plan(nominal_timeline())
    ids = used_piece_ids(cuts)
    assert ids == ["mus_rec_gymnopedie_1", "mus_rec_cello_sonata_largo", "mus_rec_macleod_canon_in_d"]
    pieces = load_pieces(SOURCES, ids)
    canon = pieces[2]
    assert canon.license == "CC-BY-4.0"
    assert "CC BY 4.0" in attribution(canon, "en") and "Kevin MacLeod" in attribution(canon, "es")
    assert "CC0" in attribution(pieces[0], "es") and "CC0" in attribution(pieces[0], "en")
    docs, desc = write_credits(SOURCES, cuts, tmp_path / "docs/c.md", tmp_path / "out/d.md", 75.0)
    text = desc.read_text(encoding="utf-8")
    assert "Canon in D Major" in text and "https://creativecommons.org/licenses/by/4.0/" in text
    assert "Duración 75,0 s" in description_markdown(pieces, 75.0)
    assert "Satie" in docs.read_text(encoding="utf-8")


def test_unregistered_piece_is_rejected(tmp_path: Path) -> None:
    path = tmp_path / "s.json"
    path.write_text(json.dumps({"pieces": []}), encoding="utf-8")
    with pytest.raises(KeyError):
        load_pieces(path, ["mus_x"])


def test_montage_words_follow_engine_subshots(tmp_path: Path) -> None:
    from explored_trailer.timeline import parse_timeline

    names = ["01", "02", "03a", "03b", "03c", "03d", "04", "05", "06", "07", "08"]
    bounds = [0, 4, 12, 14.7, 17, 20, 22, 32, 42, 50, 68, 75]
    shots = [{"name": f"{n}-x", "start": bounds[i], "end": bounds[i + 1]} for i, n in enumerate(names)]
    plan = {c.card_id: c for c in card_plan(parse_timeline({"fps": 30, "shots": shots}))}
    assert plan["03_palabra_2"].start == pytest.approx(15.0) and plan["03_palabra_2"].end == pytest.approx(16.7)
    assert plan["03_palabra_4"].end == pytest.approx(21.7)
