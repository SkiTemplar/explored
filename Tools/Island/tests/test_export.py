import json

import numpy as np
import pytest
from explored_style import meshkit

from explored_island import export
from explored_island.terrain import build_terrain


def test_main_exporta_fbx_y_cifras(tmp_path):
    out = tmp_path / 'SM_IslaSal_Terrain.fbx'
    assert export.main(['--out', str(out), '--cell', '12', '--field', str(out.with_suffix('.bin'))]) == 0
    assert out.stat().st_size > 10_000
    info = json.loads(out.with_suffix('.json').read_text(encoding='utf-8'))
    assert info['mesh'] == 'SM_IslaSal_Terrain'
    assert info['triangles'] > 1000
    assert info['bounds_min_m'][2] == pytest.approx(-30.0)
    assert info['facet_mix'] == pytest.approx(0.4)


def test_objeto_de_blender_conserva_nivel_del_mar_y_paleta(layout, props):
    from dataclasses import replace
    coarse = replace(layout, terrain=replace(layout.terrain, cell=15.0))
    mesh = build_terrain(coarse, props)
    meshkit.reset_scene()
    obj = export.build_blender_object(mesh)
    data = meshkit.mesh_of(obj)
    zs = [v.co.z for v in data.vertices]
    assert min(zs) == pytest.approx(coarse.terrain.skirt, abs=1e-4)
    assert max(zs) == pytest.approx(float(mesh.verts[:, 2].max()), abs=1e-4)
    material = data.materials[0]
    assert len(data.materials) == 1 and material is not None and material.name == meshkit.MATERIAL_NAME
    assert meshkit.face_colors(obj) == mesh.colors.tolist()
    # El eje Y se invierte (el importador FBX de Unreal lo vuelve a invertir).
    ys = np.array([v.co.y for v in data.vertices])
    assert ys.max() == pytest.approx(-float(mesh.verts[:, 1].min()), abs=1e-4)
    # Normales hacia arriba también en Blender.
    assert all(p.normal.z > 0 for p in data.polygons)
    # Normales por esquina: las de la malla (mezcla suave/cara) con Y invertida y en el orden de los bucles.
    assert data.has_custom_normals
    corners = np.array([(n.vector[0], n.vector[1], n.vector[2]) for n in data.corner_normals])
    expected = mesh.corner_normals[:, [0, 2, 1], :].reshape(-1, 3) * np.array([1.0, -1.0, 1.0])
    # Blender guarda las normales personalizadas codificadas: error de ~1° como mucho.
    assert np.abs(corners - expected).max() < 0.03 and np.abs(corners - expected).mean() < 0.005
    meshkit.reset_scene()
