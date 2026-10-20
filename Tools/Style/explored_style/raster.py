"""Raster mínimo y determinista para las hojas de paleta y la composición de
láminas: lienzo RGB en numpy, escritura PNG sin dependencias (zlib) y una
fuente de mapa de bits 3 x 5 para rotular índices y nombres.

No usa Blender: la salida es idéntica byte a byte en cualquier máquina.
"""

from __future__ import annotations

import struct
import zlib
from pathlib import Path

import numpy as np

Rgb = tuple[int, int, int]

# Fuente 3 x 5 (filas de arriba abajo, 3 bits por fila). Solo mayúsculas,
# dígitos y algunos signos: basta para índices, nombres de la paleta y pies.
_FONT: dict[str, tuple[str, ...]] = {
    '0': ('111', '101', '101', '101', '111'), '1': ('010', '110', '010', '010', '111'),
    '2': ('111', '001', '111', '100', '111'), '3': ('111', '001', '111', '001', '111'),
    '4': ('101', '101', '111', '001', '001'), '5': ('111', '100', '111', '001', '111'),
    '6': ('111', '100', '111', '101', '111'), '7': ('111', '001', '010', '010', '010'),
    '8': ('111', '101', '111', '101', '111'), '9': ('111', '101', '111', '001', '111'),
    'A': ('010', '101', '111', '101', '101'), 'B': ('110', '101', '110', '101', '110'),
    'C': ('011', '100', '100', '100', '011'), 'D': ('110', '101', '101', '101', '110'),
    'E': ('111', '100', '110', '100', '111'), 'F': ('111', '100', '110', '100', '100'),
    'G': ('011', '100', '101', '101', '011'), 'H': ('101', '101', '111', '101', '101'),
    'I': ('111', '010', '010', '010', '111'), 'J': ('001', '001', '001', '101', '010'),
    'K': ('101', '101', '110', '101', '101'), 'L': ('100', '100', '100', '100', '111'),
    'M': ('101', '111', '111', '101', '101'), 'N': ('111', '101', '101', '101', '101'),
    'O': ('010', '101', '101', '101', '010'), 'P': ('110', '101', '110', '100', '100'),
    'Q': ('010', '101', '101', '110', '011'), 'R': ('110', '101', '110', '101', '101'),
    'S': ('011', '100', '010', '001', '110'), 'T': ('111', '010', '010', '010', '010'),
    'U': ('101', '101', '101', '101', '111'), 'V': ('101', '101', '101', '101', '010'),
    'W': ('101', '101', '111', '111', '101'), 'X': ('101', '101', '010', '101', '101'),
    'Y': ('101', '101', '010', '010', '010'), 'Z': ('111', '001', '010', '100', '111'),
    '_': ('000', '000', '000', '000', '111'), '#': ('101', '111', '101', '111', '101'),
    '-': ('000', '000', '111', '000', '000'), '.': ('000', '000', '000', '000', '010'),
    ',': ('000', '000', '000', '010', '100'), ':': ('000', '010', '000', '010', '000'),
    '/': ('001', '001', '010', '100', '100'), '(': ('010', '100', '100', '100', '010'),
    ')': ('010', '001', '001', '001', '010'), '=': ('000', '111', '000', '111', '000'),
    ' ': ('000', '000', '000', '000', '000'),
}
GLYPH_W = 3
GLYPH_H = 5


def hex_to_rgb(value: str) -> Rgb:
    """«#RRGGBB» -> (r, g, b) en 0-255. Lanza ValueError si el formato no es ese."""
    text = value.strip()
    if len(text) != 7 or not text.startswith('#'):
        raise ValueError(f'color hexadecimal no válido: {value!r} (se espera #RRGGBB)')
    try:
        return (int(text[1:3], 16), int(text[3:5], 16), int(text[5:7], 16))
    except ValueError as exc:
        raise ValueError(f'color hexadecimal no válido: {value!r}') from exc


def luminance(rgb: Rgb) -> float:
    """Luminancia relativa aproximada (0-1) para elegir tinta clara u oscura."""
    r, g, b = rgb
    return (0.2126 * r + 0.7152 * g + 0.0722 * b) / 255.0


def new_canvas(width: int, height: int, color: Rgb) -> np.ndarray:
    if width <= 0 or height <= 0:
        raise ValueError(f'tamaño de lienzo no válido: {width} x {height}')
    canvas = np.empty((height, width, 3), dtype=np.uint8)
    canvas[:, :] = color
    return canvas


def fill_rect(canvas: np.ndarray, x: int, y: int, w: int, h: int, color: Rgb) -> np.ndarray:
    """Copia del lienzo con un rectángulo relleno (recortado a los bordes)."""
    out = canvas.copy()
    out[max(y, 0):max(y + h, 0), max(x, 0):max(x + w, 0)] = color
    return out


def paste(canvas: np.ndarray, image: np.ndarray, x: int, y: int) -> np.ndarray:
    """Copia del lienzo con «image» (H x W x 3) pegada en (x, y), recortada."""
    out = canvas.copy()
    h = min(image.shape[0], out.shape[0] - y)
    w = min(image.shape[1], out.shape[1] - x)
    if h > 0 and w > 0:
        out[y:y + h, x:x + w] = image[:h, :w, :3]
    return out


def text_width(text: str, scale: int) -> int:
    return max(len(text) * (GLYPH_W + 1) * scale - scale, 0)


def draw_text(canvas: np.ndarray, text: str, x: int, y: int, scale: int, color: Rgb) -> np.ndarray:
    """Copia del lienzo con «text» rotulado con la fuente 3 x 5 escalada.
    Los caracteres sin glifo se dibujan como espacio."""
    out = canvas.copy()
    cursor = x
    for char in text.upper():
        glyph = _FONT.get(char, _FONT[' '])
        for row, bits in enumerate(glyph):
            for col, bit in enumerate(bits):
                if bit == '1':
                    px, py = cursor + col * scale, y + row * scale
                    out[max(py, 0):max(py + scale, 0), max(px, 0):max(px + scale, 0)] = color
        cursor += (GLYPH_W + 1) * scale
    return out


def encode_png(canvas: np.ndarray) -> bytes:
    """PNG RGB de 8 bits, sin entrelazado ni filtros (determinista)."""
    if canvas.ndim != 3 or canvas.shape[2] != 3 or canvas.dtype != np.uint8:
        raise ValueError('se espera un lienzo H x W x 3 de uint8')
    height, width = canvas.shape[:2]
    raw = b''.join(b'\x00' + canvas[row].tobytes() for row in range(height))

    def chunk(tag: bytes, data: bytes) -> bytes:
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data) & 0xFFFFFFFF)

    header = struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', header) + chunk(b'IDAT', zlib.compress(raw, 9))
            + chunk(b'IEND', b''))


def write_png(path: Path, canvas: np.ndarray) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(encode_png(canvas))
    return path


def decode_png(data: bytes) -> np.ndarray:
    """Lee un PNG escrito por encode_png (RGB 8 bits, filtro 0). Sirve para
    los tests y para recomponer láminas sin depender de Blender."""
    if not data.startswith(b'\x89PNG\r\n\x1a\n'):
        raise ValueError('no es un PNG')
    pos, width, height, idat = 8, 0, 0, b''
    while pos < len(data):
        (length,) = struct.unpack('>I', data[pos:pos + 4])
        tag, body = data[pos + 4:pos + 8], data[pos + 8:pos + 8 + length]
        if tag == b'IHDR':
            width, height, depth, ctype = struct.unpack('>IIBB', body[:10])
            if depth != 8 or ctype != 2:
                raise ValueError('solo se leen PNG RGB de 8 bits')
        elif tag == b'IDAT':
            idat += body
        pos += 12 + length
    raw = zlib.decompress(idat)
    stride = width * 3 + 1
    rows = []
    for row in range(height):
        line = raw[row * stride:(row + 1) * stride]
        if line[0] != 0:
            raise ValueError('filtro PNG no soportado (solo 0)')
        rows.append(np.frombuffer(line[1:], dtype=np.uint8).reshape(width, 3))
    return np.stack(rows)
