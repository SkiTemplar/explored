TAREA: termina la rama existente `audio/soundfont-acustico`. Tiene un commit WIP a medias. Haz checkout de la rama y rebasa sobre main.

Problema: la música suena «low poly» y retro. Conserva el secuenciador de Tools/Audio, pero renderiza con un soundfont acústico de licencia libre y verificable: GeneralUser GS, FluidR3_GM (MIT) o MuseScore_General (MIT).
- Descárgalo a una caché ignorada por git, desde la URL oficial y comprobando el hash. Anota la licencia.
- No versiones el soundfont si pesa más de 50 MB.

Instrumentos: guitarra de nailon, marimba o vibráfono, flauta, cuerdas suaves, piano, ukelele y percusión de mano. Nada de chiptune.

Render:
- Humaniza la velocidad y el microtiming.
- Mezcla con reverb de sala, apunta a unos -16 LUFS y pon un limitador.
- Genera las capas según Content/Data/music_layers.json.

Tests: que no haya silencio ni clipping, que se cumplan los LUFS y la duración. Genera también los espectrogramas.

Pyright marca tipos mezclados en compose.py, líneas 388-419: arréglalos. pytest en verde. Abre la PR.
