# Soundfont acústico de terceros

La música (`explored_audio.music`) se renderiza con **FluidR3Mono_GM.sf3**
(versión mono y comprimida de FluidR3 GM, tal como la distribuye MuseScore)
mediante FluidSynth. Ni el soundfont ni el binario de FluidSynth se versionan:
`Tools/Audio/src/explored_audio/music/soundfont.py` los descarga a
`Tools/Audio/.cache/` (ignorado por git), junto con el texto de la licencia, y
comprueba el SHA256 de cada fichero antes de usarlo. Si un fichero de la caché
no coincide con el hash, se descarta y se vuelve a descargar.

Descarga manual (opcional; se hace sola al generar música):
`uv run python scripts/fetch_soundfont.py`.

## Soundfont: FluidR3Mono_GM.sf3

| Campo | Valor |
|---|---|
| Fuente | repositorio oficial de MuseScore, `share/sound/FluidR3Mono_GM.sf3` |
| Commit fijado | `894e82c1b12937021eb024305ef21c64335e21ce` |
| URL | <https://raw.githubusercontent.com/musescore/MuseScore/894e82c1b12937021eb024305ef21c64335e21ce/share/sound/FluidR3Mono_GM.sf3> |
| SHA256 | `2aacd036d7058d40a371846ef2f5dc5f130d648ab3837fe2626591ba49a71254` |
| Tamaño | 23 712 790 bytes (~24 MB) |
| Licencia | MIT |
| Texto de la licencia | `share/sound/FluidR3Mono_License.md` del mismo commit |
| SHA256 de la licencia | `0fa7d85b3114adb91cebd42fe955e22df6b19917e9c8e19c401080c075975636` |

Se fija un commit y no la rama `main`: si MuseScore actualiza el fichero, el
hash dejaría de cuadrar y el render fallaría sin que nadie hubiera tocado este
repositorio.

No se versiona aunque pese menos de 50 MB: bajarlo y verificarlo cuesta unos
segundos, y así el repositorio no arrastra 24 MB de binario de terceros en
cada clon. La versión estéreo original (FluidR3_GM, unos 140 MB) superaría con
holgura ese límite.

### Créditos y licencia

Créditos que exige el propio fichero de licencia («The acknowledgements and
copyright notices above must be included in any derivative work»):

- FluidR3 (versión original): Frank Wen, Copyright © 2000-2002, 2008.
- Conversión mono (FluidR3Mono): Michael Cowgill, Copyright © 2014-2017.
- Adaptación de MuseScore: S. Christian Collins, Copyright © 2018.
- Temple Blocks: Ethan Winer, Copyright © 2002.
- Percusión Drumline: Michael Schorsch, Copyright © 2016.

Texto de la licencia MIT, copiado de `FluidR3Mono_License.md`:

> Mono version: Copyright (c) 2014-16 Michael Cowgill
> Copyright (c) 2000-2002, 2008 Frank Wen <getfrank@gmail.com>
>
> Permission is hereby granted, free of charge, to any person
> obtaining a copy of this software and associated documentation
> files (the "Software"), to deal in the Software without
> restriction, including without limitation the rights to use,
> copy, modify, merge, publish, distribute, sublicense, and/or sell
> copies of the Software, and to permit persons to whom the
> Software is furnished to do so, subject to the following
> conditions:
>
> The above copyright notice and this permission notice shall be
> included in all copies or substantial portions of the Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
> EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
> OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
> NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
> HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
> WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
> FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
> OTHER DEALINGS IN THE SOFTWARE.

Los WAV que se generan con este soundfont y se empaquetan en el juego son
obra derivada: estos créditos tienen que figurar también en los créditos del
juego (pantalla de créditos o fichero de licencias de terceros del build).

## FluidSynth (herramienta de render, no se distribuye)

- **Linux/macOS**: el `fluidsynth` del sistema (`apt install fluidsynth`,
  `brew install fluid-synth`). Verificado con FluidSynth 2.3.4 (Ubuntu 24.04).
- **Windows**: build oficial de la organización FluidSynth,
  <https://github.com/FluidSynth/fluidsynth/releases/tag/v2.6.1>
  (`fluidsynth-v2.6.1-win10-x64-cpp11.zip`, SHA256
  `fab7a2e4b85675b66970f97a39bbc239729c5e0f237198b5922a6a73cbc8677c`), que
  se descarga sola si no hay uno en el PATH.
- **Licencia**: LGPL-2.1. Se usa únicamente como herramienta de build (un
  proceso externo invocado por línea de comandos para generar los WAV); no se
  versiona, no se enlaza ni se redistribuye, así que la LGPL no afecta a la
  licencia del repositorio ni del juego.

Versiones distintas de FluidSynth pueden dar WAV distintos en el último bit;
cada máquina es determinista consigo misma, que es lo que comprueban los tests.
