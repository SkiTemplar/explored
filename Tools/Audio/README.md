# Tools/Audio

Genera todo el catalogo de audio de Explored (efectos, ambientes y musica) por
codigo, sin depender del editor. `uv run explored-audio build` escribe los WAV
y el manifiesto en `Art/Export/Audio/`; `Tools/Unreal/import_audio.py` los
importa despues dentro del editor.

```bash
uv run explored-audio build            # todo el catalogo
uv run explored-audio music-layers     # solo music_layers.json (sin audio)
uv run pytest -q                       # suite completa
uv run python scripts/gen_spectrograms.py   # espectrogramas PNG de revision
```

## Musica: soundfont acustico + FluidSynth

Efectos y ambientes son sintesis pura (osciladores/ruido filtrado, sin
muestras). La musica es la unica excepcion: se renderiza con un **soundfont
acustico real** (`FluidR3Mono_GM.sf3`, MIT) via **FluidSynth**, para que suene
a instrumentos de verdad en vez de a chiptune. Detalle de la licencia,
procedencia y hash de verificacion en `THIRD_PARTY_SOUNDFONT.md`.

La partitura (que nota, cuando, con que acordes: `music/compose.py`,
`music/theory.py`) y el secuenciador (`music/sequencer.py`: humanizacion de
tiempo/velocidad, pistas, reverberacion final) **no han cambiado**. Lo unico
que cambio es el instrumento que sostiene cada nota:

| Rol en el secuenciador | Instrumento GM | Nota |
|---|---|---|
| `marimba` | Marimba | |
| `kalimba` | Kalimba (GM2) | |
| `bass` | Acoustic Bass | |
| `guitar` | Acoustic Guitar (nylon) | arpegios/fingerpicking |
| `ukulele` | Acoustic Guitar (nylon), +12 semitonos | el soundfont no trae ukelele dedicado |
| `pad` | Slow Strings | "cuerdas suaves" |
| `strings` | String Ensemble 1 | |
| `tremolo_strings` | Tremolo Strings | tremolo real del soundfont |
| `flute` (frases) | Flute | notas ligadas + pedal de sustain, no portamento continuo |
| `shaker` / `wood_block` / `kick` | Maracas / Hi Wood Block / Low Conga (percusion, canal MIDI 10) | percusion de mano, no bateria |

Detalle de la conversion Track -> MIDI -> WAV, y las limitaciones deliberadas
(percusion a un solo canal, sin portamento continuo, aproximacion del
ukelele), en el docstring de `music/midi_render.py`.

`music/instruments.py` (los sintetizadores numpy originales: modal, Karplus-
Strong, sierra limitada en banda) ya no lo usa el secuenciador, pero se deja
en el repo: sigue cubierto por `tests/test_new_content.py` y documenta tecnicas
de sintesis reutilizables en otros generadores.

### Cache de herramientas de terceros

`music/soundfont.py` descarga el soundfont y (en Windows) el binario de
FluidSynth a `Tools/Audio/.cache/` la primera vez que hace falta renderizar
musica (verificado por SHA256; necesita red esa primera vez). Para dejarlo
listo de antemano: `uv run python scripts/fetch_soundfont.py`. La cache esta
cubierta por el `.cache/` del `.gitignore` raiz (mismo patron que usa
`Tools/Textures/.cache/` para sus originales CC0).

En Linux/macOS, si ya hay un `fluidsynth` instalado por el gestor de paquetes
del sistema (`apt install fluidsynth`, `brew install fluid-synth`), se usa
ese en vez de descargar nada.
