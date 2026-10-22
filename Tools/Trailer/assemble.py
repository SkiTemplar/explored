"""Montaje del tráiler de Baliza Siete.

    cd Tools/Trailer
    uv run python assemble.py              # montaje final (fotogramas del motor + trailer.json)
    uv run python assemble.py rehearsal    # ensayo con capturas de docs/v3/capturas
    uv run python assemble.py --help
"""

import sys

from explored_trailer.cli import main

if __name__ == "__main__":
    sys.exit(main())
