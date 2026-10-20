"""Validador de estilo de Baliza Siete (GDD v3 §11).

    uv run check-style <fbx|blend...> --budget <tipo|número>

Reglas que hacen fallar (código de salida 1):
  texturas     ningún nodo de textura (TEX_*) en los materiales ni imágenes en el fichero
  materiales   exactamente 1 material por malla (M_Toon; variantes por instancia en Unreal)
  uv           cada cara con todas sus UV (canal 0) dentro de UNA celda de la hoja de
               paleta 32 x 1, a más de un cuarto de celda del borde
  ngones       solo tris y quads
  escala       escala del objeto 1,0 y sin rotación pendiente de aplicar
  pivote       origen en la base: z mínima local = 0 (± 5 mm) y origen dentro de la huella XY
  tris         triángulos <= presupuesto del tipo (tabla del GDD §11)

Avisos (no cambian el código de salida): sombreado suave (el estilo usa
normales planas) y nombre sin prefijo SM_.
Código 2: error de uso (fichero inexistente, formato o presupuesto no válidos).
"""

from __future__ import annotations

import argparse
import io
import math
import sys
from dataclasses import dataclass, field
from pathlib import Path

import bpy

from explored_style.meshkit import mesh_of
from explored_style.palette import PALETTE_SIZE, cell_of_u

# Tope duro de triángulos por tipo de malla (GDD v3 §11).
BUDGETS: dict[str, int] = {
    'objeto_mano': 300,
    'vegetacion': 400,     # hierba, arbusto, roca pequeña (200-400)
    'palmera': 1500,       # palmera, bambú, roca grande
    'estructura': 800,     # refugio, barril, hoguera, balsa
    'faro': 3000,          # torre, linterna, casa del faro
    'carguero': 4000,      # casco y puente del carguero
}
PIVOT_TOLERANCE = 0.005
SCALE_TOLERANCE = 1e-4
ROTATION_TOLERANCE = 1e-3
SUPPORTED = ('.fbx', '.blend')
_IMAGE_TYPES_IGNORED = {'RENDER_RESULT', 'COMPOSITING'}


@dataclass(frozen=True)
class Finding:
    rule: str
    message: str


@dataclass(frozen=True)
class MeshReport:
    name: str
    triangles: int
    budget: int
    errors: tuple[Finding, ...]
    warnings: tuple[Finding, ...]
    cells: tuple[int, ...]

    @property
    def ok(self) -> bool:
        return not self.errors


@dataclass(frozen=True)
class FileReport:
    path: Path
    meshes: tuple[MeshReport, ...]
    errors: tuple[Finding, ...] = field(default_factory=tuple)

    @property
    def ok(self) -> bool:
        return not self.errors and all(m.ok for m in self.meshes)


def parse_budget(value: str) -> int:
    """Tipo del GDD («palmera») o número de triángulos («1200»)."""
    if value in BUDGETS:
        return BUDGETS[value]
    if value.isdigit() and int(value) > 0:
        return int(value)
    tipos = ', '.join(f'{k} ({v})' for k, v in BUDGETS.items())
    raise ValueError(f'presupuesto no válido: {value!r}; usa un número o uno de: {tipos}')


def load_file(path: Path) -> None:
    """Deja en la escena solo el contenido del fichero."""
    suffix = path.suffix.lower()
    if suffix == '.blend':
        bpy.ops.wm.open_mainfile(filepath=str(path))
        return
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(path))


def _texture_findings(obj: bpy.types.Object) -> list[Finding]:
    out = []
    for mat in {s.material for s in obj.material_slots if s.material is not None}:
        tree = mat.node_tree
        textures = sorted(n.type for n in tree.nodes if n.type.startswith('TEX_')) if tree else []
        if textures:
            out.append(Finding('texturas', f'el material {mat.name} usa texturas ({", ".join(textures)})'))
    return out


def _material_findings(obj: bpy.types.Object) -> list[Finding]:
    names = sorted({s.material.name for s in obj.material_slots if s.material is not None})
    if len(names) == 1:
        return []
    if not names:
        return [Finding('materiales', 'sin material: se espera exactamente 1 (M_Toon)')]
    return [Finding('materiales', f'{len(names)} materiales ({", ".join(names)}); se permite 1')]


def _uv_findings(mesh: bpy.types.Mesh) -> tuple[list[Finding], set[int]]:
    if not mesh.uv_layers:
        return [Finding('uv', 'sin canal UV: cada cara debe mapear a una celda de la paleta')], set()
    uv = mesh.uv_layers[0].data
    bad, cells = 0, set()
    for poly in mesh.polygons:
        face_cells = set()
        for li in poly.loop_indices:
            u, v = uv[li].uv
            face_cells.add(cell_of_u(u) if 0.0 <= v <= 1.0 else None)
        if len(face_cells) == 1 and None not in face_cells:
            cells |= face_cells  # type: ignore[arg-type]
        else:
            bad += 1
    if bad:
        return [Finding('uv', f'{bad} de {len(mesh.polygons)} caras con UV fuera de una única celda '
                              f'de la hoja {PALETTE_SIZE} x 1')], cells
    return [], cells


def _ngon_findings(mesh: bpy.types.Mesh) -> list[Finding]:
    ngons = sum(1 for p in mesh.polygons if p.loop_total > 4)
    return [Finding('ngones', f'{ngons} caras de más de 4 lados')] if ngons else []


def _transform_findings(obj: bpy.types.Object) -> list[Finding]:
    out = []
    if any(abs(s - 1.0) > SCALE_TOLERANCE for s in obj.scale):
        out.append(Finding('escala', f'escala no unitaria {tuple(round(s, 4) for s in obj.scale)}; aplícala'))
    if any(abs(r) > ROTATION_TOLERANCE for r in obj.rotation_euler):
        rot = tuple(round(math.degrees(r), 2) for r in obj.rotation_euler)
        out.append(Finding('escala', f'rotación sin aplicar {rot} grados'))
    return out


def _pivot_findings(mesh: bpy.types.Mesh) -> list[Finding]:
    if not mesh.vertices:
        return [Finding('pivote', 'malla sin vértices')]
    xs = [v.co.x for v in mesh.vertices]
    ys = [v.co.y for v in mesh.vertices]
    min_z = min(v.co.z for v in mesh.vertices)
    out = []
    if abs(min_z) > PIVOT_TOLERANCE:
        out.append(Finding('pivote', f'el pivote no está en la base: z mínima local = {min_z:.3f} m'))
    inside = min(xs) - PIVOT_TOLERANCE <= 0.0 <= max(xs) + PIVOT_TOLERANCE and \
        min(ys) - PIVOT_TOLERANCE <= 0.0 <= max(ys) + PIVOT_TOLERANCE
    if not inside:
        out.append(Finding('pivote', 'el pivote cae fuera de la huella XY de la malla'))
    return out


def check_object(obj: bpy.types.Object, budget: int) -> MeshReport:
    mesh = mesh_of(obj)
    triangles = sum(p.loop_total - 2 for p in mesh.polygons)
    uv_errors, cells = _uv_findings(mesh)
    errors = [*_texture_findings(obj), *_material_findings(obj), *uv_errors, *_ngon_findings(mesh),
              *_transform_findings(obj), *_pivot_findings(mesh)]
    if triangles > budget:
        errors.append(Finding('tris', f'{triangles} triángulos > presupuesto de {budget}'))
    warnings = []
    smooth = sum(1 for p in mesh.polygons if p.use_smooth)
    if smooth:
        warnings.append(Finding('normales', f'{smooth} caras con sombreado suave; el estilo usa normales planas'))
    if not obj.name.startswith('SM_'):
        warnings.append(Finding('nombre', f'«{obj.name}» no sigue SM_<Nombre>'))
    return MeshReport(obj.name, triangles, budget, tuple(errors), tuple(warnings), tuple(sorted(cells)))


def check_loaded(path: Path, budget: int) -> FileReport:
    """Valida lo que haya en la escena actual (ya cargada)."""
    images = [i.name for i in bpy.data.images if i.type not in _IMAGE_TYPES_IGNORED]
    file_errors = [Finding('texturas', f'el fichero trae imágenes: {", ".join(sorted(images))}')] if images else []
    meshes = sorted((o for o in bpy.data.objects if o.type == 'MESH'), key=lambda o: o.name)
    if not meshes:
        file_errors.append(Finding('mallas', 'el fichero no contiene mallas'))
    return FileReport(path, tuple(check_object(o, budget) for o in meshes), tuple(file_errors))


def check_file(path: Path, budget: int) -> FileReport:
    load_file(path)
    return check_loaded(path, budget)


def format_report(report: FileReport) -> str:
    lines = []
    for finding in report.errors:
        lines.append(f'[FALLA] {report.path.name}: {finding.rule}: {finding.message}')
    for mesh in report.meshes:
        status = 'OK   ' if mesh.ok else 'FALLA'
        cells = ','.join(str(c) for c in mesh.cells) or '-'
        lines.append(f'[{status}] {report.path.name} :: {mesh.name}  {mesh.triangles}/{mesh.budget} tris  '
                     f'celdas [{cells}]')
        lines.extend(f'    x {f.rule}: {f.message}' for f in mesh.errors)
        lines.extend(f'    ! {f.rule}: {f.message}' for f in mesh.warnings)
    return '\n'.join(lines)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog='check-style', description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('files', nargs='+', type=Path, help='ficheros .fbx o .blend')
    parser.add_argument('--budget', required=True, help=f'tipo ({", ".join(BUDGETS)}) o número de triángulos')
    args = parser.parse_args(argv)
    for stream in (sys.stdout, sys.stderr):
        if isinstance(stream, io.TextIOWrapper):  # consola de Windows en cp1252
            stream.reconfigure(encoding='utf-8')
    try:
        budget = parse_budget(args.budget)
    except ValueError as exc:
        print(f'check-style: {exc}', file=sys.stderr)
        return 2
    problems = [p for p in args.files if not p.is_file() or p.suffix.lower() not in SUPPORTED]
    if problems:
        for p in problems:
            reason = 'no existe' if not p.is_file() else f'formato no soportado (usa {", ".join(SUPPORTED)})'
            print(f'check-style: {p}: {reason}', file=sys.stderr)
        return 2
    failed = 0
    for path in args.files:
        report = check_file(path, budget)
        print(format_report(report))
        failed += 0 if report.ok else 1
    print(f'check-style: {len(args.files)} fichero(s), {failed} con fallos')
    return 1 if failed else 0


if __name__ == '__main__':
    raise SystemExit(main())
