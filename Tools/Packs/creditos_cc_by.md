# Créditos CC-BY 3.0 pendientes de integrar

Excepción a la política CC0-only de `docs/art/paleta.md` / GDD v2 §7.1, autorizada
por el director el 2026-09-28 para poder usar especies de dosel de selva
reconocibles (Quaternius no tiene en Poly Pizza ninguna especie de árbol
tropical identificable; solo copas redondas genéricas de aspecto templado).

CC-BY 3.0 exige atribución. Los modelos siguientes están en uso
(`Tools/Packs/download_polypizza.py`, `Tools/Packs/packs.json` →
`polypizza_ccby_tropical`) y **no tienen todavía** su línea de crédito en el
juego. Pendiente de que la sesión con el editor:

1. Añada una sección "Modelos 3D" a `Source/Explored/UI/Widgets/SExploredCredits.cpp`
   (hay un comentario `TODO(creditos-cc-by)` marcando el punto exacto) con el
   texto de abajo, en español e inglés.
2. Actualice `Content/Localization/Game/*.archive` con la clave nueva (ejecutar
   el commandlet de localización habitual del proyecto; no se ha tocado a mano
   aquí porque este agente no tiene Unreal).
3. Decida si `docs/diseno/gdd_v2.md` §7.1 debe documentar la excepción CC0→CC-BY
   (recomendado, para que no se repita esta pregunta en la próxima revisión de
   arte).

## Texto de atribución (pegar tal cual, por autor)

**Poly by Google** (CC BY 3.0 — <https://creativecommons.org/licenses/by/3.0/>):
"Rubber tree", "Macassar tree", "Tree", "Brazil nut tree", "Balsa tree",
"Tree roots", "Banana Tree", "Heliconia flower", "Bromeliad", "Hibiscus flower"
por Poly by Google, vía Poly Pizza (<https://poly.pizza>).

**Zacharylll** (CC BY 3.0 — <https://creativecommons.org/licenses/by/3.0/>):
"Vine Covered Tree" por Zacharylll, vía Poly Pizza (<https://poly.pizza/u/Zacharylll>).

## Inventario exacto (publicID → título → autor)

| publicID | Título | Autor | Usado como |
|---|---|---|---|
| dlW4hGKBpiS | Vine Covered Tree | Zacharylll | JungleWide A |
| 2PolZJUAMmk | Rubber tree | Poly by Google | JungleWide B |
| 2EW209K0xBw | Macassar tree | Poly by Google | JungleWide C |
| 6pwiq7hSrHr | Tree | Poly by Google | JungleWide D |
| 0CDi5mHR26U | Brazil nut tree | Poly by Google | JungleGiant A |
| ekCgu7iLrz2 | Balsa tree | Poly by Google | JungleGiant B |
| eYfjQLsebfA | Tree roots | Poly by Google | Mangrove (raíces) |
| d0WJSiuOz6o | Banana Tree | Poly by Google | Shrub (bananero) |
| 06_hk2bO2Ix | Heliconia flower | Poly by Google | Shrub (heliconia) |
| 5FZIGjZBWTB | Bromeliad | Poly by Google | Shrub (bromelia) |
| fQVeG1obY8u | Hibiscus flower | Poly by Google | Flower (hibisco) |

Todos verificados modelo a modelo (autor + licencia exacta) por
`Tools/Packs/download_polypizza.py` antes de descargarlos; no se ha aceptado
ninguno por similitud de nombre sin comprobar la página real.
