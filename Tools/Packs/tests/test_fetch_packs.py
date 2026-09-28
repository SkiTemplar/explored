"""Tests de fetch_packs.py sin red (fuente direct con file://) y del manifiesto real."""

from __future__ import annotations

import hashlib
import io
import json
import urllib.request
import zipfile
from pathlib import Path
from typing import Any, cast

import pytest

import fetch_packs as fp


def _zip(path: Path, files: dict[str, str]) -> str:
    with zipfile.ZipFile(path, "w") as z:
        for name, text in files.items():
            z.writestr(name, text)
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _pack(src: Path, sha: str) -> dict[str, Any]:
    return {"id": "demo", "source": {"kind": "direct", "url": src.as_uri()}, "sha256": sha}


def test_descarga_verifica_y_extrae(tmp_path: Path) -> None:
    src = tmp_path / "src.zip"
    sha = _zip(src, {"Models/a.glb": "x", "License.txt": "CC0"})
    out = fp.fetch(_pack(src, sha), tmp_path / "cache")
    assert (out / "Models" / "a.glb").read_text() == "x"
    assert (out / ".sha256").read_text().strip() == sha
    # Segunda vez: no vuelve a extraer si el sello coincide.
    (out / "Models" / "a.glb").write_text("tocado")
    fp.fetch(_pack(src, sha), tmp_path / "cache")
    assert (out / "Models" / "a.glb").read_text() == "tocado"


def test_sha_distinto_falla_y_borra(tmp_path: Path) -> None:
    src = tmp_path / "src.zip"
    _zip(src, {"a.txt": "x"})
    with pytest.raises(RuntimeError, match="sha256"):
        fp.fetch(_pack(src, "0" * 64), tmp_path / "cache")
    assert not (tmp_path / "cache" / "demo.zip").exists()


def test_sin_sha_exige_update(tmp_path: Path) -> None:
    src = tmp_path / "src.zip"
    sha = _zip(src, {"a.txt": "x"})
    pack = _pack(src, "")
    with pytest.raises(RuntimeError, match="update-sha"):
        fp.fetch(pack, tmp_path / "cache")
    fp.fetch(pack, tmp_path / "cache2", update_sha=True)
    assert pack["sha256"] == sha


def test_rechaza_rutas_fuera_del_zip(tmp_path: Path) -> None:
    src = tmp_path / "src.zip"
    sha = _zip(src, {"../fuera.txt": "x"})
    with pytest.raises(RuntimeError, match="fuera"):
        fp.fetch(_pack(src, sha), tmp_path / "cache")
    assert not (tmp_path / "cache" / "fuera.txt").exists()


def test_manifiesto_real_cc0_con_sha() -> None:
    manifest = fp.load_manifest()
    ids = [p["id"] for p in manifest["packs"]]
    assert len(ids) == len(set(ids))
    for p in manifest["packs"]:
        assert p["license"]["spdx"] == "CC0-1.0", p["id"]
        assert len(p["sha256"]) == 64, p["id"]
        assert p["source"]["kind"] in ("direct", "itch")


def test_cache_por_defecto_ignorada_en_git() -> None:
    gitignore = (fp.REPO_ROOT / ".gitignore").read_text(encoding="utf-8").splitlines()
    assert "Art/Packs/" in gitignore and "Art/Export/Packs/" in gitignore
    assert fp.cache_dir() == fp.REPO_ROOT / "Art" / "Packs" or "EXPLORED_PACKS_CACHE" in __import__("os").environ


def test_catalogo_usa_packs_del_manifiesto() -> None:
    catalog = json.loads((fp.REPO_ROOT / "Content" / "Data" / "packs_catalogo.json").read_text(encoding="utf-8"))
    ids = {p["id"] for p in fp.load_manifest()["packs"]}
    assert {e["pack"] for e in catalog["entries"]} <= ids


# --- descarga -----------------------------------------------------------------

class _Resp(io.BytesIO):
    def __enter__(self) -> _Resp:
        return self

    def __exit__(self, *_: object) -> None:
        self.close()


class _FakeOpener:
    """Opener de urllib que responde desde un diccionario URL → bytes y guarda las peticiones."""

    def __init__(self, routes: dict[str, bytes]) -> None:
        self.routes = routes
        self.requests: list[tuple[str, bytes | None]] = []

    def open(self, req: urllib.request.Request, timeout: float = 0) -> _Resp:
        url = req.full_url
        self.requests.append((url, cast(bytes | None, req.data)))
        return _Resp(self.routes[url])


def test_itch_manda_el_token_y_devuelve_la_url_firmada() -> None:
    page = "https://autor.itch.io/pack"
    op = _FakeOpener({
        page: b'<meta name="csrf_token" value="tok123"/>',
        f"{page}/file/42?source=game_download": json.dumps({"url": "https://cdn/firmada.zip"}).encode(),
    })
    url = fp.itch_file_url(cast(urllib.request.OpenerDirector, op), page, 42)
    assert url == "https://cdn/firmada.zip"
    assert op.requests[1][1] == b"csrf_token=tok123"


def test_itch_sin_url_falla_con_la_respuesta() -> None:
    page = "https://autor.itch.io/pack"
    op = _FakeOpener({page: b"sin token", f"{page}/file/7?source=game_download": b'{"errors": ["no"]}'})
    with pytest.raises(RuntimeError, match="upload 7"):
        fp.itch_file_url(cast(urllib.request.OpenerDirector, op), page, 7)
    # Sin csrf en la página se manda vacío (itch.io lo acepta en precio libre).
    assert op.requests[1][1] == b"csrf_token="


def test_descarga_itch_de_punta_a_punta(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    src = tmp_path / "src.zip"
    sha = _zip(src, {"a.txt": "itch"})
    page = "https://autor.itch.io/pack"
    op = _FakeOpener({
        page: b'name="csrf_token" content="t"',
        f"{page}/file/1?source=game_download": b'{"url": "https://cdn/x.zip"}',
        "https://cdn/x.zip": src.read_bytes(),
    })
    monkeypatch.setattr(fp, "_opener", lambda: op)
    pack = {"id": "demo", "source": {"kind": "itch", "page": page, "uploadId": 1}, "sha256": sha}
    out = fp.fetch(pack, tmp_path / "cache")
    assert (out / "a.txt").read_text() == "itch"
    assert not (tmp_path / "cache" / "demo.part").exists()


def test_fuente_desconocida(tmp_path: Path) -> None:
    with pytest.raises(ValueError, match="fuente desconocida"):
        fp.download({"id": "x", "source": {"kind": "ftp"}}, tmp_path / "x.zip")


def test_zip_en_cache_corrupto_se_vuelve_a_bajar(tmp_path: Path) -> None:
    src = tmp_path / "src.zip"
    sha = _zip(src, {"a.txt": "bueno"})
    cache = tmp_path / "cache"
    cache.mkdir()
    (cache / "demo.zip").write_bytes(b"basura")
    out = fp.fetch(_pack(src, sha), cache)
    assert (out / "a.txt").read_text() == "bueno"
    assert fp.sha256_of(cache / "demo.zip") == sha


def test_sello_distinto_reextrae_desde_cero(tmp_path: Path) -> None:
    src = tmp_path / "src.zip"
    sha = _zip(src, {"a.txt": "x"})
    out = fp.fetch(_pack(src, sha), tmp_path / "cache")
    (out / "sobra.txt").write_text("de otra versión")
    (out / ".sha256").write_text("viejo\n")
    fp.fetch(_pack(src, sha), tmp_path / "cache")
    assert not (out / "sobra.txt").exists()
    assert (out / ".sha256").read_text().strip() == sha


def test_cache_desde_variable_de_entorno(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setenv("EXPLORED_PACKS_CACHE", str(tmp_path))
    assert fp.cache_dir() == tmp_path
    monkeypatch.delenv("EXPLORED_PACKS_CACHE")
    assert fp.cache_dir() == fp.REPO_ROOT / "Art" / "Packs"


# --- CLI ------------------------------------------------------------------------

@pytest.fixture
def manifiesto(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> tuple[Path, str]:
    """packs.json temporal con un pack bueno (sin sha) y uno roto; caché en tmp_path."""
    src = tmp_path / "src.zip"
    sha = _zip(src, {"a.txt": "x"})
    manifest = {"packs": [
        {"id": "bueno", "author": "Kenney", "version": "1.0", "license": {"spdx": "CC0-1.0"},
         "source": {"kind": "direct", "url": src.as_uri()}, "sha256": ""},
        {"id": "roto", "author": "Nadie", "version": "0", "license": {"spdx": "CC0-1.0"},
         "source": {"kind": "direct", "url": (tmp_path / "no-existe.zip").as_uri()}, "sha256": "0" * 64},
    ]}
    path = tmp_path / "packs.json"
    path.write_text(json.dumps(manifest), encoding="utf-8")
    monkeypatch.setattr(fp, "MANIFEST", path)
    monkeypatch.setenv("EXPLORED_PACKS_CACHE", str(tmp_path / "cache"))
    return path, sha


def test_cli_lista_sin_descargar(manifiesto: tuple[Path, str], capsys: pytest.CaptureFixture[str]) -> None:
    assert fp.main(["--list"]) == 0
    out = capsys.readouterr().out
    assert "bueno" in out and "CC0-1.0" in out
    assert not (manifiesto[0].parent / "cache").exists()


def test_cli_ids_desconocidos(manifiesto: tuple[Path, str], capsys: pytest.CaptureFixture[str]) -> None:
    assert fp.main(["bueno", "fantasma"]) == 2
    assert "fantasma" in capsys.readouterr().err


def test_cli_update_sha_rellena_y_sigue_tras_un_fallo(manifiesto: tuple[Path, str],
                                                       capsys: pytest.CaptureFixture[str]) -> None:
    path, sha = manifiesto
    assert fp.main(["--update-sha"]) == 1  # «roto» falla, pero «bueno» se descarga igual
    err = capsys.readouterr().err
    assert "roto: ERROR" in err
    guardado = json.loads(path.read_text(encoding="utf-8"))
    assert guardado["packs"][0]["sha256"] == sha
    assert guardado["packs"][1]["sha256"] == "0" * 64


def test_cli_sin_update_sha_no_toca_el_manifiesto(manifiesto: tuple[Path, str]) -> None:
    path, _ = manifiesto
    antes = path.read_text(encoding="utf-8")
    assert fp.main(["bueno"]) == 1  # sin sha256 y sin --update-sha
    assert path.read_text(encoding="utf-8") == antes
