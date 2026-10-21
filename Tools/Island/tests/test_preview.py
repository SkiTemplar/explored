"""Vista previa: los tests no dependen del contenido LFS (la CI hace checkout sin LFS); usan un
terreno grueso y un cubo exportados aquí mismo a tmp_path."""

import subprocess

import numpy as np
import pytest
from explored_style import meshkit, raster

from explored_island import export, preview


@pytest.fixture(scope='module')
def ficheros(tmp_path_factory):
    folder = tmp_path_factory.mktemp('preview')
    terreno = folder / 'terreno.fbx'
    assert export.main(['--out', str(terreno), '--cell', '12']) == 0
    builder = meshkit.MeshBuilder()
    for x, y, z in ((0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0), (0, 0, 1), (1, 0, 1), (1, 1, 1), (0, 1, 1)):
        builder.vert((x - 0.5, y - 0.5, z))
    for face in ((0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)):
        builder.face(face, 7)
    meshkit.reset_scene()
    cubo = folder / 'cubo.fbx'
    meshkit.export_fbx(meshkit.build_object('Cubo', builder), str(cubo))
    mascara = folder / 'mascara.png'
    rng = np.random.default_rng(1)
    raster.write_png(mascara, rng.integers(60, 200, (32, 32, 1)).repeat(3, axis=2).astype(np.uint8))
    return {'terreno': terreno, 'cubo': cubo, 'mascara': mascara}


@pytest.fixture
def sin_lfs(monkeypatch, ficheros):
    real = preview.git_file

    def git_file(revision, rel_path, folder):
        if rel_path == preview.TERRAIN_FBX:
            return ficheros['terreno']
        return real(revision, rel_path, folder)

    monkeypatch.setattr(preview, 'git_file', git_file)
    monkeypatch.setattr(preview, 'kit_fbx', lambda mesh_path: ficheros['cubo'] if mesh_path else None)
    monkeypatch.setattr(preview, 'STROKES_PNG', ficheros['mascara'])


def test_vistas_de_las_capturas(layout):
    views = {v.key: v for v in preview.views(layout)}
    assert set(views) == {'cala', 'palmar', 'faro', 'suelo'}
    assert views['cala'].eye == (-150.0, -12.0, 7.0)
    assert views['faro'].eye[2] > 20.0  # en la ladera de la colina, a 4 m del suelo
    assert preview.to_blender(1.0, 2.0, 3.0) == (1.0, -2.0, 3.0)


def test_mallas_de_kit():
    assert preview.kit_fbx('/Game/Art/K1_Naturaleza/SM_PalmA_01') is not None
    assert preview.kit_fbx('/Game/Art/K1_Naturaleza/SM_NoExiste_01') is None
    assert preview.kit_fbx('/Game/Otra/Cosa') is None


def test_ficheros_de_git_y_punteros_lfs(tmp_path, monkeypatch):
    # Una revisión de git sin LFS (island.json) se copia tal cual.
    copia = preview.git_file('HEAD', 'Content/Data/island.json', tmp_path)
    assert copia.parent == tmp_path and copia.read_bytes().startswith(b'{')
    assert preview.git_file('.', 'Content/Data/island.json', tmp_path) == preview.REPO_ROOT / 'Content/Data/island.json'
    # Un puntero LFS se sustituye por lo que devuelve «git lfs smudge».
    puntero = tmp_path / 'malla.fbx'
    puntero.write_bytes(preview.LFS_POINTER + b'v1\noid sha256:abc\nsize 3\n')
    llamadas = []

    def run(args, **kwargs):
        llamadas.append(args)
        return subprocess.CompletedProcess(args, 0, stdout=b'FBX')

    monkeypatch.setattr(preview.subprocess, 'run', run)
    real = preview.lfs_content(puntero, tmp_path)
    assert real.read_bytes() == b'FBX' and llamadas == [['git', 'lfs', 'smudge']]


def test_columna_mal_escrita(tmp_path, capsys):
    assert preview.main(['--out', str(tmp_path / 'x.png'), '--column', 'SIN_REVISION']) == 2
    assert 'ERROR' in capsys.readouterr().out


def test_lamina_pequena_con_dos_columnas(tmp_path, sin_lfs, capsys):
    out = tmp_path / 'lamina.png'
    assert preview.main(['--out', str(out), '--column', 'ANTES=HEAD', '--column', 'AHORA=.', '--view', 'suelo',
                         '--percent', '10']) == 0
    image = raster.decode_png(out.read_bytes())
    assert image.shape[1] == 2 * (preview.VIEW_W + preview.PAD) + preview.PAD
    assert image.shape[0] == preview.LABEL_H + preview.PAD + preview.VIEW_H + preview.LABEL_H + preview.PAD
    log = capsys.readouterr().out
    assert "'decor'" in log and 'escrito' in log


def test_terreno_sin_color_por_vertice_y_sin_mascara(tmp_path, monkeypatch, ficheros, capsys):
    # Un FBX facetado antiguo (sin «Col») se pinta con la hoja de paleta; sin máscara, aviso.
    monkeypatch.setattr(preview, 'git_file', lambda revision, rel, folder: (
        ficheros['cubo'] if rel == preview.TERRAIN_FBX else preview.REPO_ROOT / rel))
    monkeypatch.setattr(preview, 'kit_fbx', lambda mesh_path: None)
    monkeypatch.setattr(preview, 'STROKES_PNG', tmp_path / 'no_existe.png')
    out = tmp_path / 'l.png'
    assert preview.main(['--out', str(out), '--column', 'AHORA=.', '--view', 'cala', '--percent', '5']) == 0
    assert 'AVISO' in capsys.readouterr().out and out.is_file()
