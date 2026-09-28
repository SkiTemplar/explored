"""Desenvoltorio de FLAC en Ogg: ida y vuelta sin perder una muestra, y
rechazo de ficheros rotos."""

from __future__ import annotations

import io
import struct

import numpy as np
import pytest
import soundfile as sf

from explored_audio.recorded.oggflac import is_ogg_flac, ogg_crc, ogg_flac_to_flac, ogg_packets, set_ogg_serial


def _native_flac(samples: np.ndarray, fs: int) -> bytes:
    buf = io.BytesIO()
    sf.write(buf, samples, fs, format="FLAC", subtype="PCM_16")
    return buf.getvalue()


def _split_native(flac: bytes) -> tuple[list[bytes], bytes]:
    """Bloques de metadatos (con su cabecera) y el resto (tramas)."""
    assert flac[:4] == b"fLaC"
    pos, blocks = 4, []
    while True:
        header = flac[pos]
        size = int.from_bytes(flac[pos + 1 : pos + 4], "big")
        blocks.append(flac[pos : pos + 4 + size])
        pos += 4 + size
        if header & 0x80:
            return blocks, flac[pos:]


def _page(packets: list[bytes], granule: int, serial: int = 7, seq: int = 0) -> bytes:
    lacing = bytearray()
    body = bytearray()
    for p in packets:
        n = len(p)
        lacing += b"\xff" * (n // 255) + bytes([n % 255])
        body += p
    assert len(lacing) <= 255
    head = b"OggS" + bytes([0, 0]) + struct.pack("<qIII", granule, serial, seq, 0) + bytes([len(lacing)])
    return head + bytes(lacing) + bytes(body)


def _to_ogg_flac(flac: bytes, total: int, unknown_total: bool = True, headers_count: bool = True) -> bytes:
    blocks, frames = _split_native(flac)
    streaminfo = bytearray(blocks[0])
    if unknown_total:
        off = 4 + 10
        packed = int.from_bytes(streaminfo[off : off + 8], "big") & ~((1 << 36) - 1)
        streaminfo[off : off + 8] = packed.to_bytes(8, "big")
    others = [bytes([b[0] & 0x7F]) + b[1:] for b in blocks[1:]]
    n = len(others) if headers_count else 0
    first = b"\x7fFLAC\x01\x00" + struct.pack(">H", n) + b"fLaC" + bytes([streaminfo[0] & 0x7F]) + bytes(streaminfo[1:])
    # Las tramas se trocean a capricho: el resultado no depende del corte.
    cut = [frames[i : i + 700] for i in range(0, len(frames), 700)]
    pages = [_page([first], 0, seq=0), _page(others, 0, seq=1)]
    for k in range(0, len(cut), 5):
        last = k + 5 >= len(cut)
        pages.append(_page(cut[k : k + 5], total if last else -1, seq=2 + k))
    return b"".join(pages)


@pytest.fixture(scope="module")
def signal():
    fs = 22_050
    t = np.arange(fs * 3) / fs
    left = 0.4 * np.sin(2 * np.pi * 330 * t)
    right = 0.3 * np.sin(2 * np.pi * 495 * t)
    return np.stack([left, right], axis=1), fs


@pytest.mark.parametrize("headers_count", [True, False], ids=["n_cabeceras", "n_desconocido"])
def test_ida_y_vuelta_identica(signal, headers_count):
    samples, fs = signal
    flac = _native_flac(samples, fs)
    ogg = _to_ogg_flac(flac, total=len(samples), headers_count=headers_count)
    assert is_ogg_flac(ogg)
    back, fs2 = sf.read(io.BytesIO(ogg_flac_to_flac(ogg)), dtype="int16", always_2d=True)
    ref, _ = sf.read(io.BytesIO(flac), dtype="int16", always_2d=True)
    assert fs2 == fs
    np.testing.assert_array_equal(back, ref)


def test_no_confunde_vorbis_ni_flac_nativo(signal, tmp_path):
    samples, fs = signal
    assert not is_ogg_flac(_native_flac(samples, fs))
    vorbis = tmp_path / "v.ogg"
    sf.write(vorbis, samples, fs, format="OGG", subtype="VORBIS")
    assert not is_ogg_flac(vorbis.read_bytes())
    assert not is_ogg_flac(b"")


@pytest.mark.parametrize(
    "damage",
    [
        lambda b: b[:-10],  # truncado a mitad de pagina
        lambda b: b[:200] + b"XXXX" + b[200:],  # bytes de mas: descuadra las paginas
        lambda b: b + _page([b"x"], 0, serial=99),  # segundo flujo logico
    ],
    ids=["truncado", "basura", "dos_flujos"],
)
def test_rechaza_ogg_roto(signal, damage):
    samples, fs = signal
    ogg = _to_ogg_flac(_native_flac(samples, fs), total=len(samples))
    with pytest.raises(ValueError):
        ogg_flac_to_flac(damage(ogg))


def test_granulo_final(signal):
    samples, fs = signal
    ogg = _to_ogg_flac(_native_flac(samples, fs), total=len(samples))
    _, granule = ogg_packets(ogg)
    assert granule == len(samples)


def test_crc_ogg_coincide_con_el_de_libogg(signal, tmp_path):
    # libogg escribe el CRC correcto: recalcularlo sobre su propia pagina
    # (con el campo a cero) tiene que dar el mismo valor.
    samples, fs = signal
    path = tmp_path / "v.ogg"
    sf.write(path, samples, fs, format="OGG", subtype="VORBIS")
    data = bytearray(path.read_bytes())
    nsegs = data[26]
    end = 27 + nsegs + sum(data[27 : 27 + nsegs])
    stored = struct.unpack_from("<I", data, 22)[0]
    struct.pack_into("<I", data, 22, 0)
    assert ogg_crc(bytes(data[:end])) == stored


def test_fijar_serie_da_bytes_identicos_y_audio_intacto(signal, tmp_path):
    samples, fs = signal
    outs = []
    for i in range(2):
        path = tmp_path / f"{i}.ogg"
        sf.write(path, samples, fs, format="OGG", subtype="VORBIS")
        outs.append(set_ogg_serial(path.read_bytes(), 1234))
    assert outs[0] == outs[1]
    fixed = tmp_path / "fijo.ogg"
    fixed.write_bytes(outs[0])
    a, _ = sf.read(fixed)
    b, _ = sf.read(tmp_path / "0.ogg")
    np.testing.assert_array_equal(a, b)


def test_fijar_serie_rechaza_ogg_truncado(signal, tmp_path):
    samples, fs = signal
    path = tmp_path / "v.ogg"
    sf.write(path, samples, fs, format="OGG", subtype="VORBIS")
    with pytest.raises(ValueError):
        set_ogg_serial(path.read_bytes()[:-5], 1)
