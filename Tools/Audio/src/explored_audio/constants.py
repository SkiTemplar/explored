"""Constantes globales de audio para Explored."""

# Formato de exportacion fijado por el diseño: 48 kHz / 16 bits.
SAMPLE_RATE = 48_000
BIT_DEPTH = 16

# Techo de pico de seguridad: ningun WAV exportado debe superar esto.
PEAK_CEILING_DBFS = -1.0
PEAK_CEILING_LINEAR = 10 ** (PEAK_CEILING_DBFS / 20.0)

# Duracion por defecto del fundido cruzado usado para cerrar los bucles.
DEFAULT_LOOP_FADE_S = 4.0
