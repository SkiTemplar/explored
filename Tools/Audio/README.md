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
a instrumentos de verdad y no a chiptune. Licencia, creditos, commit fijado y
hashes en `THIRD_PARTY_SOUNDFONT.md`.

La partitura (que nota, cuando, con que acordes: `music/compose.py`,
`music/theory.py`) y el secuenciador (`music/sequencer.py`) siguen siendo los
mismos; cambia lo que sostiene cada nota y el master:

```
compose.py --(Track)--> midi_render.py --(MIDI)--> FluidSynth + soundfont
    --> reverb.room_reverb (sala por convolucion)
    --> seamless_loop (bucles)
    --> levels.master_to_lufs (-16 LUFS integrados + limitador de pico verdadero)
```

| Rol en el secuenciador | Preset (banco:programa) | Nota |
|---|---|---|
| `marimba` | Marimba (0:12) | melodia principal |
| `vibraphone` | Vibraphone (0:11) | segunda voz, noche, tormenta |
| `guitar` | Nylon String Guitar (0:24) | arpegios de fingerpicking |
| `ukulele` | Ukulele (8:24) | arpegio de mar abierto |
| `piano` | Mellow Grand Piano (8:0) | menu |
| `flute` (frases) | Flute (0:73) | notas ligadas + pedal de sustain |
| `pad` | Slow Strings (0:49) | "cuerdas suaves" |
| `strings` | Strings (0:48) | |
| `tremolo_strings` | Tremolo Strings (0:44) | tremolo real del soundfont |
| `bass` | Acoustic Bass (0:32) | |
| `shaker` / `wood_block` / `kick` | Maracas / Hi Wood Block / Low Conga (canal 10) | percusion de mano |

Humanizacion (`music/humanize.py`, aplicada en `midi_render.py`): microtiming
por nota, deriva lenta de tempo por pista, variacion de velocidad y acento
metrico, y entrada escalonada de las notas de los acordes de cuerda.

Master (`build.finalize`): ganancia a **-16 LUFS integrados** (BS.1770-4, con
puertas; `levels.integrated_lufs`) y limitador de pico verdadero con
anticipacion (`levels.lookahead_limiter`, techo -1 dBFS), circular en los
bucles para que la ganancia no salte en la union.

### Capas segun `music_layers.json`

`uv run explored-audio music` lee `Content/Data/music_layers.json` (el mismo
JSON que usa el director de musica del juego) y escribe una capa WAV por
pieza en `Art/Export/Audio/Musica`, mas la muestra de la flauta. Falla si el
JSON nombra una pieza que no existe o si la duracion renderizada no cuadra
con la declarada. `uv run explored-audio music-layers` regenera el JSON a
partir de la partitura.

### Espectrogramas

`uv run python scripts/gen_spectrograms.py --musica` escribe un PNG por pieza
en `Saved/AudioPreview/` (ignorado por git) para revisar el render sin
escucharlo. Sin `--musica` genera tambien los de los ambientes.

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
