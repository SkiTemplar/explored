# Tools/Trailer

Sonido, rótulos y montaje del tráiler de Baliza Siete (75 s, 1920×1080, 30 fps). Las
imágenes las genera el motor en `Saved/Trailer/frames/f_00000.png…` junto con
`Tools/Trailer/trailer.json` (tiempos reales por plano); esta herramienta no abre Unreal.

```bash
cd Tools/Trailer
uv run python assemble.py               # montaje final: frames + trailer.json → MP4 ES y EN
uv run python assemble.py rehearsal     # ensayo con capturas de docs/v3/capturas (zoompan)
uv run python assemble.py mix           # solo Saved/Trailer/audio/mix.wav + levels.{txt,json,png}
uv run python assemble.py cards         # solo los rótulos PNG (Saved/Trailer/cards/<es|en>)
uv run python assemble.py credits       # docs/creditos-trailer.md + Saved/Trailer/descripcion.md
uv run python assemble.py analyze ../../Art/Export/Audio/Grabada/mus_rec_gymnopedie_1.wav --hop 0.25
uv run ruff check . && uv run basedpyright && uv run pytest -q --cov
```

Opciones útiles: `--title-x/--title-y/--line-y/--word-x/--word-y` (posición de los rótulos en
fracciones del fotograma, para no tapar el faro ni el HUD), `--horn-at` y `--mistral-at`
(fuerzan los eventos del final si trailer.json no los trae), `--langs es`, `--threads`.

## trailer.json

```json
{"fps": 30, "frames": 2250,
 "shots": [{"index": 1, "name": "mar_amanecer", "start": 0.0, "end": 4.0}, "..."],
 "events": {"ignite": 50.5, "lit": 53.5, "mistral": 53.5, "horn": 63.1}}
```

Se aceptan segundos (`start/end`, `inicio/fin`) o fotogramas (`startFrame/endFrame`), y
subplanos con el mismo `index`/`plano`. Sin `horn`, la bocina va a `mistral + 9,6 s`
(`FFinaleSequenceModel`: 80 % de una llegada de 12 s); sin `mistral`, a la llama + 3 s.

## Sonido

- Música (piezas de `Tools/Audio/music_sources.json`, WAV tratados en
  `Art/Export/Audio/Grabada`): Gymnopédie n.º 1 (piano, 0-22,5 s), Largo de la sonata para
  violonchelo op. 65 (21-51 s, crece hasta respirar justo en el encendido) y Canon in D de
  MacLeod (clímax desde el plano 7 y salto en fase a la cadencia final, que cae en el título).
  Cortes y razones en `src/explored_trailer/music.py`.
- Ambientes y efectos de `Art/Export/Audio` (`explored-audio build`), normalizados a
  -16 LUFS y colocados por plano (`layers.py`); bocina a +6 LU sobre la música, que se atenúa 5 dB.
- Master: fundidos de 1 s / 2 s, -16 LUFS integrados, limitador de pico verdadero a -2 dBTP;
  la cifra final la mide `ffmpeg loudnorm` y la mezcla se recalibra una vez si se aparta > 0,1 LU.

## Vídeo

H.264 yuv420p CRF 17 preset slow +faststart (BT.709), AAC 256 kbps 48 kHz estéreo, fundido a
negro en el último segundo. ffmpeg corre con prioridad baja y 8 hilos. Cada vídeo se comprueba
con ffprobe (75,0 ± 0,1 s, 1920×1080, 30 fps, audio estéreo 48 kHz) y deja fotogramas de
control en `Saved/Trailer/stills`.
