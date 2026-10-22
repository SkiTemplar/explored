from __future__ import annotations

from pathlib import Path

import numpy as np
import pytest
from conftest import tone, write_wav

from explored_trailer.analyze import analyze, format_envelope, rms_envelope, spectral_flux
from explored_trailer.audio_io import (
    SAMPLE_RATE,
    equal_power_curve,
    fade,
    loop_to_length,
    place,
    read_audio,
    to_mono,
    to_stereo,
    write_audio,
)
from explored_trailer.loudness import (
    db_to_linear,
    integrated_lufs,
    limit_true_peak,
    linear_to_db,
    parse_loudnorm,
    true_peak_db,
    window_lufs,
)


def test_equal_power_crossfade_keeps_power() -> None:
    rising, falling = equal_power_curve(1000, True), equal_power_curve(1000, False)
    assert np.allclose(rising**2 + falling**2, 1.0)
    assert equal_power_curve(0, True).size == 0


def test_fade_ends_near_zero_and_does_not_mutate() -> None:
    audio = np.ones((4800, 2))
    out = fade(audio, 480, 480)
    assert out[0, 0] < 0.01 and out[-1, 0] < 0.01
    assert np.all(audio == 1.0)


def test_loop_to_length_has_no_steps() -> None:
    src = tone(1.0, 440.0, 0.5)
    looped = loop_to_length(src, SAMPLE_RATE * 5, 2400, offset=1234)
    assert looped.shape == (SAMPLE_RATE * 5, 2)
    assert np.max(np.abs(np.diff(looped[:, 0]))) < 0.2
    assert loop_to_length(src, 0, 100).shape == (0, 2)
    with pytest.raises(ValueError):
        loop_to_length(np.zeros((0, 2)), 10, 1)


def test_place_clips_both_ends() -> None:
    clip = np.ones((10, 2))
    assert place(20, clip, -5).sum() == 10.0
    assert place(20, clip, 15).sum() == 10.0
    assert place(20, clip, 25).sum() == 0.0
    assert place(20, clip, 2, gain=0.5)[2, 0] == 0.5


def test_channel_helpers() -> None:
    mono = np.arange(4.0)
    assert to_stereo(mono).shape == (4, 2)
    assert to_stereo(mono[:, None]).shape == (4, 2)
    assert to_mono(np.ones((3, 2))).shape == (3,)
    assert to_mono(mono) is not mono
    with pytest.raises(ValueError):
        to_stereo(np.zeros((4, 3)))


def test_wav_round_trip(tmp_path: Path) -> None:
    path = write_audio(tmp_path / "a/x.wav", tone(0.5))
    audio, rate = read_audio(path)
    assert rate == SAMPLE_RATE and audio.shape == (SAMPLE_RATE // 2, 2)


def test_db_helpers() -> None:
    assert db_to_linear(-6.0206) == pytest.approx(0.5, rel=1e-3)
    assert linear_to_db(1.0) == 0.0


def test_lufs_of_full_scale_sine_matches_bs1770() -> None:
    # Seno de 997 Hz a 0 dBFS en los dos canales: -3,01 + 3,01 (dos canales) ≈ 0 LUFS... menos 0,691 de offset.
    t = np.arange(SAMPLE_RATE * 3) / SAMPLE_RATE
    sine = np.sin(2 * np.pi * 997 * t)
    level = integrated_lufs(np.stack([sine, sine], axis=1))
    assert level == pytest.approx(-0.0, abs=0.3)
    assert integrated_lufs(np.zeros((SAMPLE_RATE, 2))) == -np.inf
    assert integrated_lufs(np.zeros((100, 2))) == -np.inf


def test_window_lufs_and_rate_guard() -> None:
    audio = tone(2.0, 997.0, 0.5)
    assert window_lufs(audio, SAMPLE_RATE // 2, SAMPLE_RATE) == pytest.approx(-6.0, abs=0.5)
    assert window_lufs(audio, 10, 10) == -np.inf
    from explored_trailer.loudness import k_weight

    with pytest.raises(ValueError):
        k_weight(audio, 44100)


def test_limiter_holds_ceiling_without_touching_quiet_parts() -> None:
    audio = tone(2.0, 3000.0, 0.2)
    audio[SAMPLE_RATE : SAMPLE_RATE + 2400] *= 5.0
    limited = limit_true_peak(audio, -2.0)
    assert true_peak_db(limited) <= -1.95
    assert np.allclose(limited[:SAMPLE_RATE // 2], audio[:SAMPLE_RATE // 2])
    quiet = tone(1.0, 440.0, 0.1)
    assert np.array_equal(limit_true_peak(quiet, -2.0), quiet)


def test_parse_loudnorm_block() -> None:
    stderr = 'garbage\n{\n "input_i" : "-16.02",\n "input_tp" : "-1.90",\n "input_lra" : "7.1",\n' \
             ' "input_thresh" : "-26.3",\n "output_i" : "-16.0"\n}\n'
    report = parse_loudnorm(stderr)
    assert report.integrated_lufs == -16.02 and report.true_peak_dbtp == -1.9
    with pytest.raises(ValueError):
        parse_loudnorm("nada")


def test_analysis_finds_attack(tmp_path: Path) -> None:
    audio = np.zeros((SAMPLE_RATE * 4, 2))
    audio[SAMPLE_RATE * 2 :] = tone(2.0, 440.0, 0.5)
    path = write_wav(tmp_path / "p.wav", audio)
    env = analyze(str(path), 0.5)
    assert int(np.argmax(env.flux)) == 4
    assert env.rms_db[0] < -100 and env.rms_db[-1] > -15
    text = format_envelope(env, 1.0, 2.5)
    assert text.splitlines()[0].strip().startswith("1.0s")
    assert rms_envelope(np.zeros(10), SAMPLE_RATE).size == 0
    assert spectral_flux(np.zeros(10), SAMPLE_RATE).size == 0
