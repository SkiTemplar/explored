"""Lectura y validacion de `Tools/Audio/music_sources.json`.

La lista de licencias admitidas vive aqui, en codigo, y no en el JSON: asi
nadie puede colar una licencia nueva editando solo los datos. Cada fuente
tiene su propia lista (encargo 21):

- Wikimedia Commons y Musopen: dominio publico o CC0.
- FreePD: CC0.
- Kevin MacLeod (incompetech): CC-BY 4.0, siempre con credito.
- OpenGameArt: CC0 o CC-BY.

Nada NC ni ND: esas licencias ni siquiera tienen identificador aqui.
"""

from __future__ import annotations

import json
import re
from dataclasses import dataclass
from pathlib import Path
from urllib.parse import urlparse

# Identificador de licencia -> URL canonica de su texto (o de la plantilla de
# Commons que la declara, para el dominio publico, que no tiene texto propio).
LICENSE_URLS: dict[str, tuple[str, ...]] = {
    "CC0-1.0": ("https://creativecommons.org/publicdomain/zero/1.0/",),
    "PD": (
        "https://commons.wikimedia.org/wiki/Template:PD-self",
        "https://commons.wikimedia.org/wiki/Template:PD-author",
        "https://creativecommons.org/publicdomain/mark/1.0/",
    ),
    "CC-BY-3.0": ("https://creativecommons.org/licenses/by/3.0/",),
    "CC-BY-4.0": ("https://creativecommons.org/licenses/by/4.0/",),
}

# Licencias que obligan a dar credito en el juego y en Steam.
ATTRIBUTION_LICENSES = frozenset({"CC-BY-3.0", "CC-BY-4.0"})

# Fuente -> (licencias admitidas, dominios admitidos para source_url/download_url).
SOURCES: dict[str, tuple[frozenset[str], tuple[str, ...]]] = {
    "wikimedia_commons": (frozenset({"PD", "CC0-1.0"}), ("commons.wikimedia.org", "upload.wikimedia.org")),
    "musopen": (frozenset({"PD", "CC0-1.0"}), ("musopen.org",)),
    "freepd": (frozenset({"CC0-1.0"}), ("freepd.com",)),
    "incompetech": (frozenset({"CC-BY-4.0"}), ("incompetech.com",)),
    "opengameart": (frozenset({"CC0-1.0", "CC-BY-3.0", "CC-BY-4.0"}), ("opengameart.org",)),
}

# Momentos de juego del encargo y papel de music_layers.json que les
# corresponde por defecto. El papel concreto de cada pieza se declara en el
# JSON (`layer_role`) y se comprueba contra music_layers.json.
MOMENTS = ("dia", "noche", "lluvia", "mar", "cueva", "ruinas")

_ID_RE = re.compile(r"^mus_rec_[a-z0-9_]+$")
_SHA_RE = re.compile(r"^[0-9a-f]{64}$")


def tools_audio_root() -> Path:
    return Path(__file__).resolve().parents[3]


def repo_root() -> Path:
    return tools_audio_root().parents[1]


def default_sources_path() -> Path:
    return tools_audio_root() / "music_sources.json"


def default_cache_root() -> Path:
    # `.cache/` esta en el .gitignore raiz: nada de lo que caiga aqui se versiona.
    return tools_audio_root() / ".cache" / "music"


@dataclass(frozen=True)
class Piece:
    id: str
    title: str
    composer: str
    performer: str
    instrumentation: str
    source: str
    source_url: str
    download_url: str
    license: str
    license_url: str
    license_evidence: str
    sha256: str
    moment: str
    layer_role: str

    @property
    def needs_credit(self) -> bool:
        return self.license in ATTRIBUTION_LICENSES

    @property
    def extension(self) -> str:
        suffix = Path(urlparse(self.download_url).path).suffix.lower()
        return suffix or ".bin"


@dataclass(frozen=True)
class SourceList:
    version: int
    target_lufs: float
    lufs_tolerance: float
    peak_ceiling_dbfs: float
    pieces: tuple[Piece, ...]


_PIECE_FIELDS = tuple(Piece.__dataclass_fields__)


def parse_sources(data: object) -> SourceList:
    """Convierte el JSON ya decodificado en `SourceList`. Lanza `ValueError`
    con un mensaje claro si falta un campo o tiene el tipo equivocado; las
    reglas de contenido (licencias, dominios, momentos) van en `validate`."""
    if not isinstance(data, dict):
        raise ValueError("music_sources.json: la raiz debe ser un objeto")
    for key in ("version", "target_lufs", "lufs_tolerance", "peak_ceiling_dbfs", "pieces"):
        if key not in data:
            raise ValueError(f"music_sources.json: falta '{key}'")
    if not isinstance(data["pieces"], list):
        raise ValueError("music_sources.json: 'pieces' debe ser una lista")
    for key in ("target_lufs", "lufs_tolerance", "peak_ceiling_dbfs"):
        if isinstance(data[key], bool) or not isinstance(data[key], (int, float)):
            raise ValueError(f"music_sources.json: '{key}' debe ser un numero")
    pieces = []
    for i, raw in enumerate(data["pieces"]):
        if not isinstance(raw, dict):
            raise ValueError(f"music_sources.json: la pieza {i} no es un objeto")
        missing = [f for f in _PIECE_FIELDS if f not in raw]
        if missing:
            raise ValueError(f"music_sources.json: a la pieza {i} le faltan {missing}")
        extra = sorted(set(raw) - set(_PIECE_FIELDS))
        if extra:
            raise ValueError(f"music_sources.json: la pieza {i} tiene campos desconocidos {extra}")
        bad = [f for f in _PIECE_FIELDS if not isinstance(raw[f], str)]
        if bad:
            raise ValueError(f"music_sources.json: en la pieza {i} deben ser texto {bad}")
        pieces.append(Piece(**{f: raw[f] for f in _PIECE_FIELDS}))
    return SourceList(
        version=int(data["version"]),
        target_lufs=float(data["target_lufs"]),
        lufs_tolerance=float(data["lufs_tolerance"]),
        peak_ceiling_dbfs=float(data["peak_ceiling_dbfs"]),
        pieces=tuple(pieces),
    )


def load_sources(path: Path | str | None = None) -> SourceList:
    path = Path(path) if path else default_sources_path()
    with open(path, encoding="utf-8") as f:
        return parse_sources(json.load(f))


def load_layer_roles(path: Path | str | None = None) -> set[str]:
    path = Path(path) if path else repo_root() / "Content" / "Data" / "music_layers.json"
    with open(path, encoding="utf-8") as f:
        data = json.load(f)
    return {p["role"] for p in data["pieces"]}


def _host_allowed(url: str, hosts: tuple[str, ...]) -> bool:
    parsed = urlparse(url)
    if parsed.scheme != "https" or not parsed.hostname:
        return False
    host = parsed.hostname.lower()
    return any(host == h or host.endswith("." + h) for h in hosts)


def validate(sources: SourceList, layer_roles: set[str] | None = None) -> list[str]:
    """Devuelve la lista de problemas (vacia si todo cuadra)."""
    errors: list[str] = []
    if not -24.0 <= sources.target_lufs <= -9.0:
        errors.append(f"target_lufs {sources.target_lufs} fuera de un rango razonable")
    if not 0.0 < sources.lufs_tolerance <= 3.0:
        errors.append(f"lufs_tolerance {sources.lufs_tolerance} debe estar en (0, 3]")
    if not -6.0 <= sources.peak_ceiling_dbfs < 0.0:
        errors.append(f"peak_ceiling_dbfs {sources.peak_ceiling_dbfs} debe estar en [-6, 0)")
    if not 15 <= len(sources.pieces) <= 25:
        errors.append(f"hay {len(sources.pieces)} piezas; el encargo pide de 15 a 25")

    seen_ids: set[str] = set()
    seen_sha: set[str] = set()
    seen_urls: set[str] = set()
    for p in sources.pieces:
        tag = p.id or "<sin id>"
        if not _ID_RE.match(p.id):
            errors.append(f"{tag}: el id debe ser mus_rec_<minusculas_y_guiones_bajos>")
        if p.id in seen_ids:
            errors.append(f"{tag}: id repetido")
        seen_ids.add(p.id)
        for field in ("title", "composer", "performer", "instrumentation", "license_evidence"):
            if not getattr(p, field).strip():
                errors.append(f"{tag}: '{field}' vacio")

        if p.source not in SOURCES:
            errors.append(f"{tag}: fuente '{p.source}' no admitida")
            continue
        allowed, hosts = SOURCES[p.source]
        if p.license not in allowed:
            errors.append(f"{tag}: la licencia '{p.license}' no esta admitida para {p.source}")
        elif p.license_url not in LICENSE_URLS[p.license]:
            errors.append(f"{tag}: license_url '{p.license_url}' no corresponde a {p.license}")
        for field in ("source_url", "download_url"):
            if not _host_allowed(getattr(p, field), hosts):
                errors.append(f"{tag}: {field} no es https en un dominio de {p.source}")
        if p.download_url in seen_urls:
            errors.append(f"{tag}: download_url repetida")
        seen_urls.add(p.download_url)

        if not _SHA_RE.match(p.sha256):
            errors.append(f"{tag}: sha256 debe tener 64 digitos hexadecimales en minuscula")
        elif p.sha256 in seen_sha:
            errors.append(f"{tag}: sha256 repetido (el mismo fichero dos veces)")
        seen_sha.add(p.sha256)

        if p.moment not in MOMENTS:
            errors.append(f"{tag}: momento '{p.moment}' desconocido (validos: {', '.join(MOMENTS)})")
        if layer_roles is not None and p.layer_role not in layer_roles:
            errors.append(f"{tag}: layer_role '{p.layer_role}' no existe en music_layers.json")
    return errors
