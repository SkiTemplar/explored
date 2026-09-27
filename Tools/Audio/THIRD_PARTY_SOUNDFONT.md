# Soundfont acustico de terceros

La musica (`explored_audio.music`) se renderiza con **FluidR3Mono_GM.sf3**
(version mono y comprimida de FluidR3 GM) via FluidSynth. Ni el soundfont ni
el binario de FluidSynth se versionan: `Tools/Audio/src/explored_audio/music/
soundfont.py` los descarga a `Tools/Audio/.cache/` (ignorado por git) y
verifica su SHA256 contra el fijado en ese modulo. Descarga manual (opcional,
se hace sola al generar musica): `uv run python scripts/fetch_soundfont.py`.

## Soundfont: FluidR3Mono_GM.sf3

- **Fuente**: <https://github.com/musescore/MuseScore/blob/main/share/sound/FluidR3Mono_GM.sf3>
  (repositorio oficial de MuseScore; usan el mismo fichero como soundfont por
  defecto de su editor).
- **Tamano**: ~24 MB (no se versiona; con el estereo original -FluidR3_GM,
  ~140 MB- se habria superado con holgura el limite de 50 MB para
  versionarlo).
- **Licencia**: MIT. Texto completo (copiado de `FluidR3Mono_License.md` del
  mismo repositorio, que a su vez incluye el `COPYING` original de FluidR3):

  > FluidR3 (original version) by Frank Wen Copyright (c) 2000-2002
  > Mono conversion (FluidR3Mono) by Michael Cowgill Copyright (c) 2014-17
  >
  > Permission is hereby granted, free of charge, to any person obtaining a
  > copy of this software and associated documentation files (the
  > "Software"), to deal in the Software without restriction, including
  > without limitation the rights to use, copy, modify, merge, publish,
  > distribute, sublicense, and/or sell copies of the Software, and to permit
  > persons to whom the Software is furnished to do so, subject to the
  > following conditions:
  >
  > The above copyright notice and this permission notice shall be included
  > in all copies or substantial portions of the Software.
  >
  > THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  > IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  > FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  > THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  > LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  > FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
  > DEALINGS IN THE SOFTWARE.

## FluidSynth (herramienta de render, no se distribuye)

- **Fuente**: build oficial de la organizacion FluidSynth,
  <https://github.com/FluidSynth/fluidsynth/releases/tag/v2.6.1>
  (`fluidsynth-v2.6.1-win10-x64-cpp11.zip`).
- **Licencia**: LGPL-2.1. Se usa unicamente como herramienta de build (un
  proceso externo invocado por linea de comandos para generar los WAV); no se
  versiona, no se enlaza estaticamente ni se redistribuye, asi que la LGPL no
  afecta a la licencia del propio repositorio.
