from __future__ import annotations

from pathlib import Path

import numpy as np
import pytest
from conftest import tone

from explored_trailer.audio_io import SAMPLE_RATE, seconds_to_samples
from explored_trailer.layers import build_layers, one_shot
from explored_trailer.loudness import integrated_lufs, true_peak_db
from explored_trailer.mix import (
    TARGET_LUFS,
    MixResult,
    active_window,
    build_mix,
    discontinuity_ratio,
    duck_curve,
    gain_curve,
    make_loader,
    normalize,
)
from explored_trailer.music import (
    CANON,
    CANON_CADENCE,
    CANON_CLIMAX,
    CANON_CYCLE,
    GYMNOPEDIE,
    LARGO,
    LARGO_BREATH,
    align_cadence,
    best_alignment,
    music_plan,
)
from explored_trailer.timeline import load_timeline, nominal_timeline


def test_music_plan_is_anchored_to_the_ignition_shot(trailer_json: Path) -> None:
    tl = load_timeline(trailer_json)
    cuts = music_plan(tl)
    entry = tl.shot(7).start
    assert [c.piece_id for c in cuts] == [GYMNOPEDIE, LARGO, CANON, CANON]
    largo = cuts[1]
    assert largo.dst_start + (LARGO_BREATH - largo.src_start) == pytest.approx(entry)
    climax, cadence = cuts[2], cuts[3]
    assert climax.dst_start + (CANON_CLIMAX - climax.src_start) == pytest.approx(entry)
    assert cadence.dst_start + (CANON_CADENCE - cadence.src_start) == pytest.approx(entry + CANON_CYCLE)


def test_canon_jump_keeps_the_ground_bass_phase() -> None:
    # Mismo punto del ciclo de 12 s (fase 0,12 s) a los dos lados del salto.
    phase = 0.12
    assert ((CANON_CLIMAX + CANON_CYCLE - phase) % CANON_CYCLE) == pytest.approx((CANON_CADENCE - phase) % CANON_CYCLE)


def test_music_sections_follow_the_brief() -> None:
    cuts = music_plan(nominal_timeline())
    gym, largo, climax, cadence = cuts
    assert gym.dst_start == 0.0 and gym.dst_end <= 23.0
    assert 20.0 <= largo.dst_start <= 22.5 and largo.dst_end >= 50.0
    assert climax.dst_start < 50.0 < climax.dst_end
    assert cadence.dst_start < 68.0 < cadence.dst_end <= 75.0


def test_best_alignment_recovers_a_known_lag() -> None:
    rng = np.random.default_rng(3)
    signal = rng.standard_normal(6000)
    a = signal[1000:3000]
    b = signal[1000 - 200 + 64 : 3000 + 200 + 64]  # el tramo de `a` empieza 64 muestras ANTES en `b`
    assert best_alignment(a, b, 200) == -64
    assert best_alignment(a, signal[1000 - 200 - 64 : 3000 + 200 - 64], 200) == 64
    assert best_alignment(a, b, 0) == 0


def test_align_cadence_shifts_only_the_cadence() -> None:
    cuts = music_plan(nominal_timeline())
    assert align_cadence(cuts, {}) == cuts
    canon = tone(343.0, 220.0, 0.3)
    aligned = align_cadence(cuts, {CANON: canon})
    assert aligned[:3] == cuts[:3]
    assert abs(aligned[3].src_start - cuts[3].src_start) <= 0.026


def test_layers_stay_inside_the_trailer() -> None:
    tl = nominal_timeline()
    for layer in build_layers(tl):
        assert -1.0 <= layer.start < tl.duration
        assert layer.gain_points and all(db < 0 for _, db in layer.gain_points)
    assert one_shot("sfx_x", 3.0, -10.0).loop is False


def test_gain_and_duck_curves() -> None:
    curve = gain_curve(((1.0, 0.0), (2.0, -20.0)), 0.0, SAMPLE_RATE * 3)
    assert curve[0] == pytest.approx(1.0) and curve[-1] == pytest.approx(0.1)
    duck = duck_curve(SAMPLE_RATE * 10, 4.0, 6.0, 6.0)
    assert duck[seconds_to_samples(5.0)] == pytest.approx(10 ** (-6 / 20))
    assert duck[seconds_to_samples(1.0)] == pytest.approx(1.0)


def test_active_window_of_a_burst() -> None:
    audio = np.zeros((SAMPLE_RATE * 4, 2))
    audio[SAMPLE_RATE : SAMPLE_RATE * 2] = tone(1.0, 440.0, 0.5)
    start, end = active_window(audio)
    assert start == pytest.approx(0.8, abs=0.41) and end == pytest.approx(2.0, abs=0.41)


def test_normalize_hits_target_and_keeps_silence() -> None:
    assert integrated_lufs(normalize(tone(3.0, 997.0, 0.05, channels=1))) == pytest.approx(-16.0, abs=0.2)
    silent = np.zeros((SAMPLE_RATE, 2))
    assert np.array_equal(normalize(silent), silent)


def test_discontinuity_ratio_flags_a_step() -> None:
    smooth = tone(1.0, 200.0, 0.3)
    stepped = smooth.copy()
    stepped[SAMPLE_RATE // 2 :] += 0.6
    assert discontinuity_ratio(smooth, 0.5) < 2.0
    assert discontinuity_ratio(stepped, 0.5) > 3.0
    assert discontinuity_ratio(smooth, 5.0) == 0.0


@pytest.fixture(scope="module")
def mixed(tmp_path_factory: pytest.TempPathFactory) -> MixResult:
    from conftest import write_wav

    from explored_trailer.layers import HORN

    root = tmp_path_factory.mktemp("audio")
    tl = nominal_timeline()
    for layer in (*build_layers(tl), HORN):
        target = root / layer.relpath
        if not target.exists():
            write_wav(target, tone(4.0 if layer.loop else 1.5, 220.0 + len(layer.name) * 7, 0.2, channels=1))
    for piece, length in ((GYMNOPEDIE, 40.0), (LARGO, 40.0), (CANON, 342.6)):
        write_wav(root / "Grabada" / f"{piece}.wav", tone(length, 330.0, 0.25))
    return build_mix(tl, make_loader(root, root / "Grabada"))


def test_full_mix_meets_levels(mixed: MixResult) -> None:
    assert mixed.mix.shape == (seconds_to_samples(75.0), 2)
    assert mixed.own_lufs == pytest.approx(TARGET_LUFS, abs=0.3)
    assert true_peak_db(mixed.mix) <= -1.9
    assert mixed.horn_margin_db >= 4.0
    assert np.max(np.abs(mixed.mix[:10])) < 1e-3  # fundido de entrada
    assert np.max(np.abs(mixed.mix[-10:])) < 1e-3  # fundido de salida
    assert all(discontinuity_ratio(mixed.mix, t) < 3.0 for t in mixed.splices if t > 0.0)


def test_loader_rejects_other_sample_rates(tmp_path: Path) -> None:
    import soundfile as sf

    sf.write(str(tmp_path / "x.wav"), np.zeros((4410, 2)), 44100)
    with pytest.raises(ValueError, match="44100"):
        make_loader(tmp_path, tmp_path)("x.wav")
