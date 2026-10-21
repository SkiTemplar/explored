"""
ai_props_config.py — registro de props que pasan por el pipeline de IA
(Canva -> Hunyuan3D-2 local -> stylize_prop). Un dict por prop:

  name          nombre del VARIANT procedural que sustituye (SM_<name>)
  group         carpeta bajo Art/Export/Props/ (debe casar con MODULE_GROUPS
                de Tools/Blender/props/run_props.py)
  target_size_m tamaño real objetivo (metros) de la dimension mayor del
                bounding box tras limpiar la malla; se reescala uniforme
  target_faces  presupuesto de triangulos para stylize.decimate()
  material_slot uno de los 7 materiales estables de props/_materials.py
  base_color    color base (RGB 0-1) para stylize.zone_paint()
  rules         lista de reglas geometricas de stylize.zone_paint()

Los ficheros de entrada (referencia y glb) viven en Art/Reference/props/
<name>_512.png y Art/Generated/Props3D/<name>.glb, generados a mano con el
MCP de Canva y hunyuan_gen.py (ver Tools/Blender/ai/README implicito en el
propio pipeline; no hay script que dispare Canva, es un tool de Claude).
"""

AI_PROPS = [
    dict(
        name='ItemStoneAxe', group='ObjetosPequenos', target_size_m=0.34,
        target_faces=900, material_slot='M_Wood', base_color=(0.45, 0.30, 0.16),
        rules=[
            dict(z_frac=(0.75, 1.0), color=(0.55, 0.55, 0.58)),
            dict(z_frac=(0.60, 0.75), color=(0.28, 0.20, 0.11)),
        ],
    ),
    dict(
        name='ItemStoneKnife', group='ObjetosPequenos', target_size_m=0.22,
        target_faces=700, material_slot='M_Wood', base_color=(0.42, 0.28, 0.15),
        rules=[
            dict(z_frac=(0.55, 1.0), color=(0.62, 0.64, 0.68)),
        ],
    ),
    dict(
        name='ItemSpear', group='ObjetosPequenos', target_size_m=1.55,
        target_faces=800, material_slot='M_Wood', base_color=(0.55, 0.40, 0.22),
        rules=[
            dict(z_frac=(0.85, 1.0), color=(0.20, 0.20, 0.22)),
            dict(z_frac=(0.74, 0.85), color=(0.22, 0.45, 0.42)),
        ],
    ),
    dict(
        name='ItemFishingRod', group='ObjetosPequenos', target_size_m=1.25,
        target_faces=700, material_slot='M_Wood', base_color=(0.58, 0.44, 0.24),
        rules=[
            dict(z_frac=(0.95, 1.0), color=(0.55, 0.55, 0.58)),
            dict(z_frac=(0.0, 0.14), color=(0.22, 0.42, 0.40)),
        ],
    ),
    dict(
        name='ItemFishTrap', group='ObjetosPequenos', target_size_m=0.42,
        target_faces=900, material_slot='M_Wood', base_color=(0.62, 0.48, 0.28),
        rules=[
            dict(z_frac=(0.0, 0.06), color=(0.40, 0.30, 0.16)),
        ],
    ),
    dict(
        name='ItemRopeCoil', group='ObjetosPequenos', target_size_m=0.22,
        target_faces=900, material_slot='M_Fabric', base_color=(0.66, 0.54, 0.32),
        rules=[],
    ),
    dict(
        name='ItemCoconut', group='ObjetosPequenos', target_size_m=0.14,
        target_faces=500, material_slot='M_Wood', base_color=(0.42, 0.30, 0.18),
        rules=[
            dict(z_frac=(0.85, 1.0), color=(0.30, 0.20, 0.12)),
        ],
    ),
    dict(
        name='ItemCanteen', group='ObjetosPequenos', target_size_m=0.20,
        target_faces=900, material_slot='M_Wood', base_color=(0.50, 0.36, 0.20),
        rules=[
            dict(z_frac=(0.85, 1.0), color=(0.75, 0.60, 0.36)),
            dict(z_frac=(0.35, 0.55), color=(0.30, 0.22, 0.12)),
        ],
    ),
    dict(
        name='ItemCompassNav', group='Cartografia', target_size_m=0.075,
        target_faces=600, material_slot='M_Metal', base_color=(0.72, 0.58, 0.28),
        rules=[
            dict(z_frac=(0.30, 0.85), normal_axis='z', normal_sign=1, normal_thresh=0.75,
                 color=(0.88, 0.84, 0.68)),
            dict(z_frac=(0.0, 0.08), color=(0.45, 0.36, 0.15)),
        ],
    ),
    dict(
        name='ItemSpyglass', group='Cartografia', target_size_m=0.30,
        target_faces=700, material_slot='M_Metal', base_color=(0.72, 0.58, 0.30),
        rules=[
            dict(z_frac=(0.0, 0.15), color=(0.25, 0.22, 0.18)),
            dict(z_frac=(0.85, 1.0), color=(0.20, 0.30, 0.35)),
        ],
    ),
    dict(
        name='ItemSextant', group='Cartografia', target_size_m=0.22,
        target_faces=800, material_slot='M_Wood', base_color=(0.55, 0.42, 0.24),
        rules=[
            dict(z_frac=(0.70, 1.0), color=(0.72, 0.58, 0.28)),
            dict(island='thinnest', color=(0.30, 0.22, 0.12)),
        ],
    ),
    dict(
        name='ItemMapScroll', group='Cartografia', target_size_m=0.28,
        target_faces=500, material_slot='M_Paper', base_color=(0.80, 0.72, 0.52),
        rules=[
            dict(z_frac=(0.0, 0.08), color=(0.35, 0.24, 0.12)),
            dict(z_frac=(0.92, 1.0), color=(0.35, 0.24, 0.12)),
        ],
    ),
]
