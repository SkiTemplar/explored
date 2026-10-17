"""Stub de `bpy`/`mathutils` para poder importar process_landing_set.py fuera
de Blender en los tests. process_landing_set.py solo los usa DENTRO de
funciones (reset_scene, import_glb, normalize_scale_and_pivot...); nada de
eso corre al importar el modulo ni en los tests de este directorio, que solo
leen ENTRIES y LandingEntry (listas/tipos puros, sin llamar a bpy). Si algun
test futuro necesita invocar de verdad esas funciones, hace falta Blender de
verdad (ver Tools/Packs/README.md), no este stub.
"""
import sys
import types

if "bpy" not in sys.modules:
    sys.modules["bpy"] = types.ModuleType("bpy")
if "mathutils" not in sys.modules:
    mathutils_stub = types.ModuleType("mathutils")
    mathutils_stub.Vector = None  # type: ignore[attr-defined]
    sys.modules["mathutils"] = mathutils_stub
