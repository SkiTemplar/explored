"""Partitura -> MIDI -> soundfont: presets, humanizacion, cache verificada y
render de capas segun `music_layers.json`. Casi todo sin renderizar audio
(rapido); lo que renderiza usa piezas cortas."""

from __future__ import annotations

import importlib.util
import json
from pathlib import Path

import mido
import numpy as np
import pytest

from explored_audio.build import build_music_layers
from explored_audio.catalog import MUSIC_NAMES
from explored_audio.music import compose, midi_render, soundfont
from explored_audio.music.humanize import TimingDrift, metric_accent
from explored_audio.music.sequencer import Track

AUDIO_ROOT = Path(__file__).resolve().parents[1]

# Programas GM que suenan a sintetizador o a videoconsola (leads, pads y
# efectos sinteticos, 80-103) y el banco 8 del soundfont con ondas puras.
_SYNTH_PROGRAMS = set(range(80, 104))


def _all_tracks():
    for name in MUSIC_NAMES:
        tracks, flute_phrases, *_ = compose._dispatch(name)
        yield name, tracks, flute_phrases


def _messages(mid: mido.MidiFile):
    for track in mid.tracks:
        tick = 0
        for msg in track:
            tick += msg.time
            yield tick, msg


# --- Presets -----------------------------------------------------------------


def test_cada_instrumento_de_la_partitura_tiene_preset():
    known = set(midi_render.VOICES) | midi_render.PERCUSSION_INSTRUMENTS
    for name, tracks, _ in _all_tracks():
        for track in tracks:
            assert track.instrument in known, f"{name}: {track.instrument} no tiene preset en el soundfont"


def test_paleta_acustica_sin_chiptune():
    """Todo lo que suena es acustico: ningun programa de sintetizador y los
    timbres que pide el encargo presentes en la paleta."""
    for inst, voice in midi_render.VOICES.items():
        assert voice.program not in _SYNTH_PROGRAMS, f"{inst}: programa {voice.program} es de sintetizador"
    used = {track.instrument for _, tracks, _ in _all_tracks() for track in tracks}
    used |= {"flute"} if any(fp for _, _, fp in _all_tracks()) else set()
    for required in ("guitar", "marimba", "vibraphone", "flute", "pad", "piano", "ukulele", "shaker", "kick"):
        assert required in used, f"la banda sonora no usa {required}"


def test_presets_existen_en_el_soundfont():
    """Cruza cada (banco, programa) con la tabla de presets real del fichero
    (chunk `phdr`): un preset inexistente haria que FluidSynth sonase otro
    instrumento, o nada."""
    import struct

    data = soundfont.ensure_soundfont().read_bytes()
    at = data.find(b"phdr")
    size = struct.unpack("<I", data[at + 4 : at + 8])[0]
    presets = set()
    for k in range(size // 38):
        rec = data[at + 8 + k * 38 : at + 8 + (k + 1) * 38]
        program, bank = struct.unpack("<HH", rec[20:24])
        presets.add((bank, program))
    for inst, voice in midi_render.VOICES.items():
        assert (voice.bank, voice.program) in presets, f"{inst}: {voice.bank}:{voice.program} no esta en el soundfont"
    assert (128, 0) in presets, "falta el kit de percusion estandar"


def test_ukelele_selecciona_banco_antes_del_programa():
    track = Track(instrument="ukulele", events=[(0.0, 1.0, 440.0, 0.6, 0.0)])
    mid = midi_render.build_song_midi([track], None, 100.0, np.random.default_rng(0))
    msgs = [m for _, m in _messages(mid) if not m.is_meta]
    bank = next(i for i, m in enumerate(msgs) if m.type == "control_change" and m.control == 0)
    prog = next(i for i, m in enumerate(msgs) if m.type == "program_change")
    assert msgs[bank].value == 8 and msgs[prog].program == 24
    assert bank < prog


# --- MIDI generado -----------------------------------------------------------


def test_notas_repetidas_solapadas_no_se_cortan():
    """Dos notas iguales que se solapan (la humanizacion puede adelantar la
    segunda) no deben dejar la segunda sin sonar: su `note_off` va despues de
    su `note_on`, y la primera se apaga al empezar la segunda."""
    builder = midi_render._ChannelBuilder(0.5)
    voice = midi_render.VOICES["marimba"]
    builder.add_note(0, voice, 60, 0.0, 1.0, 0.8, 0.0)
    builder.add_note(0, voice, 60, 0.5, 1.0, 0.8, 0.0)
    mid = builder.build(midi_render.TICKS_PER_BEAT, 120.0)
    active = 0
    ends_with_note = []
    for _tick, msg in _messages(mid):
        if msg.type == "note_on" and msg.velocity > 0:
            active += 1
        elif msg.type == "note_off":
            active -= 1
            assert active >= 0
            ends_with_note.append(_tick)
    assert active == 0
    assert ends_with_note == sorted(ends_with_note) and ends_with_note[0] == 480  # 0,5 s a 120 bpm


@pytest.mark.parametrize("name", ["mus_theme", "mus_sea", "mus_credits", "mus_explore_teeth"])
def test_midi_bien_formado_y_determinista(name, tmp_path):
    tracks, flute, _bars, bpm, *_ = compose._dispatch(name)
    a = midi_render.build_song_midi(tracks, flute, bpm, np.random.default_rng(1))
    b = midi_render.build_song_midi(tracks, flute, bpm, np.random.default_rng(1))
    pa = midi_render.write_midi(a, tmp_path / "a.mid")
    pb = midi_render.write_midi(b, tmp_path / "b.mid")
    assert pa.read_bytes() == pb.read_bytes()

    on: dict[tuple[int, int], int] = {}
    for _tick, msg in _messages(mido.MidiFile(str(pa))):
        if msg.type == "note_on" and msg.velocity > 0:
            on[(msg.channel, msg.note)] = on.get((msg.channel, msg.note), 0) + 1
        elif msg.type == "note_off" or (msg.type == "note_on" and msg.velocity == 0):
            on[(msg.channel, msg.note)] -= 1
    assert all(v == 0 for v in on.values()), "quedan notas colgadas"


def test_percusion_en_el_canal_10():
    tracks, _flute, _bars, bpm, *_ = compose._dispatch("mus_explore_whitesands")
    mid = midi_render.build_song_midi(tracks, None, bpm, np.random.default_rng(0))
    notes_by_channel: dict[int, set[int]] = {}
    for _tick, msg in _messages(mid):
        if msg.type == "note_on":
            notes_by_channel.setdefault(msg.channel, set()).add(msg.note)
    assert set(midi_render.PERCUSSION_NOTES.values()) <= notes_by_channel[midi_render.PERCUSSION_CHANNEL]


# --- Humanizacion ------------------------------------------------------------


def _onsets_and_velocities(track: Track, bpm: float, seed: int) -> tuple[np.ndarray, np.ndarray]:
    played = midi_render._humanized_events(track, 60.0 / bpm, np.random.default_rng(seed))
    return np.array([p.start_s for p in played]), np.array([p.velocity for p in played])


def test_humanizacion_mueve_tiempos_y_velocidades_con_limite():
    bpm = 90.0
    track = Track(instrument="marimba", humanize_timing_s=0.018, humanize_velocity_amt=0.15)
    track.events = [(b * 0.5, 0.4, 440.0, 0.7, 0.0) for b in range(256)]
    onsets, vels = _onsets_and_velocities(track, bpm, 3)
    grid = np.array([e[0] for e in track.events]) * 60.0 / bpm
    dev = onsets - grid
    bound = track.humanize_timing_s + 3 * track.humanize_timing_s * midi_render._DRIFT_PER_JITTER
    assert float(np.max(np.abs(dev[1:]))) <= bound + 1e-9
    assert float(np.std(dev)) > 0.004, "microtiming casi nulo: suena a rejilla"
    assert len(set(np.round(vels, 4))) > 50, "velocidades casi iguales: suena a maquina"
    # Tiempos fuertes mas fuertes que contratiempos, de media.
    on_beat = vels[::2].mean()
    off_beat = vels[1::2].mean()
    assert on_beat > off_beat


def test_humanizacion_no_desordena_las_notas():
    """El microtiming de una corchea (~330 ms a 90 bpm) nunca debe hacer que
    una nota adelante a la anterior."""
    track = Track(instrument="guitar", humanize_timing_s=0.02)
    track.events = [(b * 0.5, 0.4, 440.0, 0.5, 0.0) for b in range(400)]
    onsets, _ = _onsets_and_velocities(track, 90.0, 11)
    assert np.all(np.diff(onsets) > 0)


def test_humanizacion_determinista_y_distinta_por_semilla():
    track = Track(instrument="marimba")
    track.events = [(float(b), 0.9, 440.0, 0.6, 0.0) for b in range(64)]
    a = _onsets_and_velocities(track, 80.0, 5)
    b = _onsets_and_velocities(track, 80.0, 5)
    c = _onsets_and_velocities(track, 80.0, 6)
    assert np.array_equal(a[0], b[0]) and np.array_equal(a[1], b[1])
    assert not np.array_equal(a[0], c[0])


def test_deriva_de_tempo_acotada_y_correlacionada():
    drift = TimingDrift(np.random.default_rng(2), sigma_s=0.01, tau_s=2.0)
    offsets = np.array([drift.offset_at(t * 0.25) for t in range(2000)])
    assert float(np.max(np.abs(offsets))) <= 0.03 + 1e-12
    assert abs(float(np.mean(offsets))) < 0.005
    lag1 = float(np.corrcoef(offsets[:-1], offsets[1:])[0, 1])
    assert lag1 > 0.8, "sin memoria: seria ruido blanco, no deriva"


def test_deriva_con_tiempos_repetidos_o_hacia_atras():
    drift = TimingDrift(np.random.default_rng(0), sigma_s=0.01)
    first = drift.offset_at(5.0)
    assert np.isfinite(drift.offset_at(5.0))
    assert np.isfinite(drift.offset_at(1.0))  # hacia atras: no revienta
    assert np.isfinite(first)
    assert TimingDrift(np.random.default_rng(0), sigma_s=0.0).offset_at(3.0) == 0.0


def test_acento_metrico():
    assert metric_accent(0.0) > metric_accent(1.0) > metric_accent(0.5) > metric_accent(0.25)
    assert metric_accent(4.0) == metric_accent(0.0)


# --- Cache del soundfont -----------------------------------------------------


def test_url_del_soundfont_fijada_a_un_commit():
    assert "/main/" not in soundfont.SOUNDFONT_URL
    assert soundfont.MUSESCORE_COMMIT in soundfont.SOUNDFONT_URL
    assert soundfont.MUSESCORE_COMMIT in soundfont.SOUNDFONT_LICENSE_URL


def test_el_soundfont_no_se_versiona():
    path = soundfont.ensure_soundfont()
    assert path.stat().st_size < soundfont.MAX_VERSIONED_BYTES
    import subprocess

    ignored = subprocess.run(["git", "check-ignore", "-q", str(path)], cwd=AUDIO_ROOT, check=False)
    assert ignored.returncode == 0, "la cache del soundfont no esta ignorada por git"


def test_licencia_descargada_coincide_con_la_documentada():
    soundfont.ensure_soundfont()
    license_path = soundfont._cache_dir() / "soundfont" / soundfont.SOUNDFONT_LICENSE_FILENAME
    text = license_path.read_text(encoding="latin-1")
    assert "MIT" in text and "Frank Wen" in text
    doc = (AUDIO_ROOT / "THIRD_PARTY_SOUNDFONT.md").read_text(encoding="utf-8")
    assert soundfont.SOUNDFONT_SHA256 in doc
    assert soundfont.MUSESCORE_COMMIT in doc


def test_descarga_corrupta_se_rechaza(tmp_path, monkeypatch):
    def fake_download(url, dest):
        Path(dest).write_bytes(b"no es un soundfont")

    monkeypatch.setattr(soundfont.urllib.request, "urlretrieve", fake_download)
    dest = tmp_path / "x.sf3"
    with pytest.raises(RuntimeError, match="SHA256"):
        soundfont._download_verified("https://example.invalid/x.sf3", dest, "0" * 64)
    assert not dest.exists()
    assert not dest.with_name("x.sf3.part").exists()


def test_cache_corrupta_se_vuelve_a_descargar(tmp_path, monkeypatch):
    good = b"contenido correcto"
    import hashlib

    good_sha = hashlib.sha256(good).hexdigest()
    dest = tmp_path / "cache" / "x.sf3"
    dest.parent.mkdir()
    dest.write_bytes(b"truncado")
    calls = []

    def fake_download(url, target):
        calls.append(url)
        Path(target).write_bytes(good)

    monkeypatch.setattr(soundfont.urllib.request, "urlretrieve", fake_download)
    assert soundfont._ensure_cached("https://example.invalid/x.sf3", dest, good_sha) == dest
    assert dest.read_bytes() == good and len(calls) == 1
    # Segunda vez: ya verificado, no descarga.
    soundfont._ensure_cached("https://example.invalid/x.sf3", dest, good_sha)
    assert len(calls) == 1


# --- Capas segun music_layers.json -------------------------------------------


def _layers_json(tmp_path: Path, pieces: list) -> Path:
    path = tmp_path / "music_layers.json"
    path.write_text(json.dumps({"version": 1, "pieces": pieces}), encoding="utf-8")
    return path


def test_render_de_capas_segun_el_json(tmp_path):
    from explored_audio.music.layers import piece_entry

    entry = piece_entry("mus_discovery_01")
    out = tmp_path / "out"
    entries = build_music_layers(_layers_json(tmp_path, [entry]), out, verbose=False)
    assert [e["name"] for e in entries] == ["mus_discovery_01"]
    wav = out / "Musica" / "mus_discovery_01.wav"
    assert wav.exists() and wav.stat().st_size > 1000


@pytest.mark.parametrize(
    "pieces",
    [
        [],
        [{"id": "mus_que_no_existe", "loop": True, "duration_s": 10.0}],
        [{"id": "sfx_ui_click", "loop": False, "duration_s": 0.1}],
        ["no es un objeto"],
    ],
)
def test_render_de_capas_rechaza_json_invalido(tmp_path, pieces):
    with pytest.raises(ValueError):
        build_music_layers(_layers_json(tmp_path, pieces), tmp_path / "out", verbose=False)


def test_render_de_capas_rechaza_duracion_que_no_cuadra(tmp_path):
    from explored_audio.music.layers import piece_entry

    entry = piece_entry("mus_discovery_02")
    entry["duration_s"] = entry["duration_s"] + 30.0
    with pytest.raises(ValueError, match="dura"):
        build_music_layers(_layers_json(tmp_path, [entry]), tmp_path / "out", verbose=False)


# --- Espectrogramas ----------------------------------------------------------


def test_espectrograma_se_genera(tmp_path, rendered):
    spec = importlib.util.spec_from_file_location("gen_spectrograms", AUDIO_ROOT / "scripts" / "gen_spectrograms.py")
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    out = tmp_path / "mus_menu.png"
    module.plot_spectrogram(rendered["mus_menu"], "mus_menu", out)
    assert out.read_bytes()[:8] == b"\x89PNG\r\n\x1a\n"
