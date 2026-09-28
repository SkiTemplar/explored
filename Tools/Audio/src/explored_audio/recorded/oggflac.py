"""Convierte FLAC encapsulado en Ogg (`.ogg`/`.oga` con audio FLAC) a FLAC
nativo, sin recodificar.

libsndfile 1.2 falla al abrir Ogg FLAC («unknown error in flac decoder»),
y en Commons hay grabaciones subidas asi. El paso a FLAC nativo es solo
cambiar el envoltorio: las tramas FLAC se copian tal cual, asi que no hay
perdida ni cambio de muestras.

Formato (https://xiph.org/flac/ogg_mapping.html): el primer paquete es
0x7F "FLAC", version, numero de cabeceras, "fLaC" y el bloque STREAMINFO;
los siguientes N paquetes son bloques de metadatos y el resto, tramas.
"""

from __future__ import annotations

import struct

_OGG_MAGIC = b"OggS"
_FLAC_MAPPING = b"\x7fFLAC"


def is_ogg_flac(data: bytes) -> bool:
    if not data.startswith(_OGG_MAGIC) or len(data) < 28:
        return False
    nsegs = data[26]
    body = 27 + nsegs
    return data[body : body + 5] == _FLAC_MAPPING


def ogg_packets(data: bytes) -> tuple[list[bytes], int]:
    """Paquetes del unico flujo logico de `data`, en orden, y la posicion
    de granulo de la ultima pagina (en FLAC, el numero total de muestras)."""
    packets: list[bytes] = []
    granule = 0
    pending = bytearray()
    pos = 0
    serial = None
    while pos < len(data):
        if data[pos : pos + 4] != _OGG_MAGIC:
            raise ValueError(f"pagina Ogg rota en el byte {pos}")
        if pos + 27 > len(data):
            raise ValueError("pagina Ogg truncada")
        page_serial = struct.unpack_from("<I", data, pos + 14)[0]
        if serial is None:
            serial = page_serial
        elif page_serial != serial:
            raise ValueError("el fichero tiene mas de un flujo logico")
        page_granule = struct.unpack_from("<q", data, pos + 6)[0]
        if page_granule >= 0:
            granule = page_granule
        nsegs = data[pos + 26]
        lacing = data[pos + 27 : pos + 27 + nsegs]
        if len(lacing) != nsegs:
            raise ValueError("tabla de segmentos truncada")
        body = pos + 27 + nsegs
        for size in lacing:
            if body + size > len(data):
                raise ValueError("segmento Ogg truncado")
            pending += data[body : body + size]
            body += size
            if size < 255:
                packets.append(bytes(pending))
                pending.clear()
        pos = body
    if pending:
        raise ValueError("el ultimo paquete Ogg no termina")
    return packets, granule


def ogg_flac_to_flac(data: bytes) -> bytes:
    packets, total_samples = ogg_packets(data)
    if not packets or not packets[0].startswith(_FLAC_MAPPING):
        raise ValueError("no es FLAC en Ogg")
    first = packets[0]
    if len(first) < 13 + 4 + 34 or first[9:13] != b"fLaC":
        raise ValueError("cabecera Ogg FLAC incompleta")
    n_headers = struct.unpack_from(">H", first, 7)[0]
    streaminfo = bytearray(first[13:])  # cabecera de bloque + STREAMINFO
    _set_total_samples(streaminfo, total_samples)
    blocks = [bytes(streaminfo)]
    if n_headers == 0:
        # 0 significa «desconocido»: los metadatos siguen hasta la primera
        # trama, que empieza por el codigo de sincronia 0xFFF8/0xFFF9.
        i = 1
        while i < len(packets) and not (packets[i][:1] == b"\xff" and packets[i][1] & 0xFE == 0xF8):
            blocks.append(packets[i])
            i += 1
        frames = packets[i:]
    else:
        blocks += packets[1 : 1 + n_headers]
        frames = packets[1 + n_headers :]
    out = bytearray(b"fLaC")
    for i, block in enumerate(blocks):
        if len(block) < 4:
            raise ValueError("bloque de metadatos FLAC vacio")
        header = block[0] & 0x7F
        if i == len(blocks) - 1:
            header |= 0x80  # marca de ultimo bloque de metadatos
        out.append(header)
        out += block[1:]
    for frame in frames:
        out += frame
    return bytes(out)


def _set_total_samples(block: bytearray, total: int) -> None:
    """Escribe el total de muestras en STREAMINFO si viene a 0 («desconocido»).

    Sin ese dato libsndfile no sabe cuanto dura el flujo y falla al leerlo
    de memoria. Son los 36 bits bajos de los bytes 10 a 17 del bloque
    (tras la cabecera de 4 bytes)."""
    if total <= 0 or total >= 1 << 36:
        return
    off = 4 + 10
    packed = int.from_bytes(block[off : off + 8], "big")
    if packed & ((1 << 36) - 1):
        return  # ya lo trae
    packed |= total
    block[off : off + 8] = packed.to_bytes(8, "big")
