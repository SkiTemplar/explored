TAREA: escribe el contenido de texto de H4 (00-TODO H4). Toca solo datos y docs.

Requisitos:
- `halden_diaries.json`: id, texto ES y EN, isla y POI.
- `journal_entries.json`: id, trigger, textEs, textEn y demás campos de la biblia.
- `shells.json`, `herbarium.json`, `insects.json` y lo que pida la biblia para el museo.
- `ruins.json`:
  - `Teaches` y `StarPathTarget` en los 8 sites;
  - las pistas del mapa, como datos.
- Logro `juego_de_anzuelos` y las piezas de museo en building_pieces.json.

Todo el texto tiene que pasar el checklist anti-IA de biblia 07 y `l10n --strict`. La voz de Halden tiene que ser coherente con biblia 04 y 07.

Amplía DataCheck para que valide las referencias cruzadas: POI, islas y triggers.
