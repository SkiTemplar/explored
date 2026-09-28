# Música grabada de licencia abierta

Encargo 21 (`Tools/Nube/cola/21-musica-licencia-abierta.md`). Complementa la banda
sonora compuesta por código (`Tools/Audio/src/explored_audio/music`, encargo 03) con
23 grabaciones reales: piano (Satie, Chopin, Schumann, Bach), guitarra clásica, arpa,
violonchelo y cuarteto de cuerda. No sustituye a la música generada: las capas del
director de música (`Content/Data/music_layers.json`) siguen siendo las mismas, y las
grabaciones se suman como piezas que suenan en los huecos tranquilos.

## Qué hay en el repo y qué no

| Fichero | Versionado | Qué es |
|---|---|---|
| `Tools/Audio/music_sources.json` | sí | Una entrada por pieza: título, compositor, intérprete, URL de origen y de descarga, licencia, URL de la licencia, plantilla o texto de la página que la declara, SHA-256 del original, momento de juego y papel de `music_layers.json`. |
| `Tools/Audio/src/explored_audio/recorded/` | sí | Validación, descarga, medidor LUFS BS.1770 con puertas, tratamiento y créditos. |
| `docs/creditos-musica.md` | sí | Créditos CC BY en ES y EN y texto plano para Steam. Generado, no se edita a mano. |
| `Tools/Audio/.cache/music/` | **no** | Originales (`src/`), OGG tratados (`ogg/`) y `manifest.json`. |

Los OGG ocupan unos 60 MB en total, por encima del límite de 10 MB del encargo, así que
no se versionan: el script los regenera byte a byte desde los originales verificados.

## Uso

```sh
cd Tools/Audio
uv run explored-music check      # valida la lista, sin red
uv run explored-music fetch      # descarga, verifica el SHA-256, trata y exporta OGG
uv run explored-music credits    # reescribe docs/creditos-musica.md
EXPLORED_MUSIC_REQUIRE_CACHE=1 uv run pytest -q tests/test_recorded_cache.py
```

`fetch` es incremental: una pieza cuyo original y parámetros no han cambiado no se
reprocesa. Wikimedia limita el ritmo de descarga por IP; el script espera lo que pida la
cabecera `Retry-After` y deja 3 s entre descargas.

## Tratamiento

1. Decodificación con libsndfile (MP3, Vorbis, FLAC), paso a estéreo y remuestreo a
   48 kHz (`resample_poly`), igual que el resto del audio del juego.
2. Recorte de silencios: ventanas de 50 ms por debajo de −60 dBFS al principio y al
   final, con 150 ms de margen, fundido de entrada de 20 ms y de salida de 600 ms.
3. Normalización a **−16 LUFS integrados** (BS.1770-4 con puerta absoluta y relativa;
   el `lufs_approx` de `levels.py` no tiene puertas y mide de menos una grabación con
   silencios).
4. Limitador con anticipación a **−1,5 dBFS** de pico de muestra. La ganancia se
   reajusta hasta quedar a 0,1 LU del objetivo después de limitar.
5. Exportación OGG Vorbis (calidad de libsndfile 0,6, unos 110 kbps) con título,
   intérprete, licencia y origen en las etiquetas del fichero.

Los tests de la cache comprueban en el OGG ya decodificado: hash del original, −16 ± 1
LUFS, pico por debajo de −0,5 dBFS sin rachas de muestras pegadas al máximo, 48 kHz
estéreo y que no quede silencio al principio ni al final.

## Licencias

Solo se admite lo que el encargo permite, fuente por fuente, y la lista vive en código
(`recorded/sources.py`), no en el JSON:

- Wikimedia Commons y Musopen: dominio público o CC0. Cuenta la licencia de la
  **grabación**, no la de la partitura: en Commons muchas grabaciones de Satie o Debussy
  llevan «Public domain» en el resumen porque la obra lo es, pero la grabación es CC BY-SA
  (Michael Laucke, La Pianista, Ivan Ilić). Esas quedan fuera. El campo
  `license_evidence` copia la plantilla de la página (`{{cc-zero}}`, `{{PD-self}}`,
  `{{PD-author|Musopen}}`).
- Kevin MacLeod (incompetech): CC BY 4.0, con crédito. La página de cada pista
  (`index.html?isrc=…`) y la de licencias lo declaran.
- FreePD ha cerrado (la web solo muestra el aviso de cierre) y Musopen responde 403 a las descargas automáticas; sus
  grabaciones CC0 se toman de las copias de Commons, que enlazan la página de Musopen.
- Sin Debussy: en Commons solo hay grabaciones CC BY-SA, históricas de dominio público
  solo en la UE (Marcelle Meyer, 1956) o interpretaciones de sintetizador. Ninguna pasa
  el filtro sin dudas para vender en Steam.

## Momentos de juego

| Momento | Papel en `music_layers.json` | Piezas |
|---|---|---|
| Día | `explore` | Gymnopédie 1, Preludio op. 28 n.º 7, Romance anónimo, Morning, Clear Air, Evening |
| Noche | `night` | Nocturnos op. 15/1, 55/2, 72/1 y 62/1, Träumerei |
| Lluvia | `storm` | Preludio «Gota de agua», Gymnopédie 3, Plaint |
| Mar | `sea` | Nocturno op. 37/2 (barcarola), Suite para violonchelo n.º 1, Enchanted Journey |
| Cueva | `explore` | Gnossienne 1, Evening Fall (Harp), Lamentation |
| Ruinas | `explore` | Aria de las Goldberg, adagios de Beethoven (op. 18/6) y Haydn («La alondra») |

`music_layers.json` no tiene papel propio de cueva ni de ruinas: esas piezas cuelgan de
`explore` y el campo `moment` distingue cuándo suenan.

## Red (biblia 08 §2)

La música es de la fila «Cliente local, sin réplica»: nada de esto viaja por la red.
Cada cliente elige qué grabación suena a partir del estado que ya se replica (hora del
día, clima, bioma o zona del jugador local), igual que hace hoy
`UExploredMusicSubsystem` con las capas generadas. Dos jugadores en la misma partida
pueden oír piezas distintas, y es lo esperado.

## Importación en Unreal (local)

Pendiente, necesita el editor: importar `Tools/Audio/.cache/music/ogg/*.ogg` a
`Content/Audio/MusicaGrabada/` como `SoundWave` con carga bajo demanda (son piezas de
2 a 6 minutos), y dar al director de música una forma de elegir
entre las del mismo `moment`. Ver la casilla en `docs/diseno/biblia/00-TODO.md` (H5).
