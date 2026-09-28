# Música grabada de licencia abierta

Encargo 21 (`Tools/Nube/cola/21-musica-licencia-abierta.md`). Complementa la banda
sonora compuesta por código (`Tools/Audio/src/explored_audio/music`, encargo 03) con
21 grabaciones reales: piano (Satie, Chopin), guitarra clásica, arpa, violonchelo y
cuerdas. No sustituye a la música generada: las capas del
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
no se versionan: el script los regenera desde los originales verificados, idénticos byte
a byte (el número de serie del flujo Ogg, que libsndfile elige al azar, se fija a partir
del id de la pieza).

## Uso

```sh
cd Tools/Audio
uv run explored-music check      # valida la lista, sin red
uv run explored-music fetch      # descarga, verifica el SHA-256, trata y exporta OGG
uv run explored-music credits    # reescribe docs/creditos-musica.md
EXPLORED_MUSIC_REQUIRE_CACHE=1 uv run pytest -q tests/test_recorded_cache.py
```

`fetch` es incremental: una pieza cuyo original y parámetros no han cambiado no se
reprocesa. Wikimedia limita el ritmo de descarga por IP (desde la nube, una descarga cada
veinte minutos o ninguna); el script espera lo que pida la cabecera `Retry-After` y deja
3 s entre descargas. Por eso solo quedan tres piezas de Commons en la lista.

## Tratamiento

1. Decodificación con libsndfile (MP3, Vorbis, FLAC), paso a estéreo y remuestreo a
   48 kHz (`resample_poly`), igual que el resto del audio del juego. El FLAC dentro de
   Ogg, que libsndfile no abre, se desenvuelve antes a FLAC nativo sin recodificar
   (`recorded/oggflac.py`).
2. Recorte de silencios: ventanas de 50 ms por debajo de −60 dBFS al principio y al
   final, con 150 ms de margen, fundido de entrada de 20 ms y de salida de 600 ms.
3. Normalización a **−16 LUFS integrados** (BS.1770-4 con puerta absoluta y relativa;
   el `lufs_approx` de `levels.py` no tiene puertas y mide de menos una grabación con
   silencios).
4. Limitador con anticipación a **−1,5 dBFS** de pico de muestra. La ganancia se
   reajusta hasta quedar a 0,1 LU del objetivo después de limitar. El piano solo tiene
   mucho rango dinámico: para llegar a −16 LUFS, los nocturnos y preludios pierden de 3
   a 6 dB en los picos de ataque. Si se nota en el juego, bajar `target_lufs` a −18 en
   `music_sources.json` reduce esa compresión a la mitad.
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
- Musopen responde 403 a las descargas automáticas, pero publica su colección Chopin
  completa en archive.org desde su propia cuenta, con licencia CC0 declarada en el
  elemento (`archive.org/details/musopen-chopin`). Solo se admite ese elemento
  (`MUSOPEN_ARCHIVE_ITEMS`), no cualquier subida de terceros que diga venir de Musopen.
- FreePD ha cerrado: la web solo muestra el aviso de cierre.
- OpenGameArt no hizo falta.
- Sin Debussy: en Commons solo hay grabaciones CC BY-SA, históricas de dominio público
  solo en la UE (Marcelle Meyer, 1956) o interpretaciones de sintetizador. Ninguna pasa
  el filtro sin dudas para vender en Steam.

## Momentos de juego

| Momento | Papel en `music_layers.json` | Piezas |
|---|---|---|
| Día | `explore` | Gymnopédie 1, Preludio op. 28 n.º 7, Romance anónimo (guitarra), Morning, Clear Air, Evening |
| Noche | `night` | Nocturnos op. 15/1, 55/2, 72/1 y 62/2 |
| Lluvia | `storm` | Preludio «Gota de agua», Gymnopédie 3, Plaint |
| Mar | `sea` | Enchanted Journey (arpa), Largo de la sonata para violonchelo op. 65, Preludio op. 28 n.º 13 |
| Cueva | `explore` | Evening Fall (Harp), Lamentation |
| Ruinas | `explore` | Canon en re (cuerdas y arpa), Preludio op. 28 n.º 6, Danse Morialta |

`music_layers.json` no tiene papel propio de cueva ni de ruinas: esas piezas cuelgan de
`explore` y el campo `moment` distingue cuándo suenan.

## Red (biblia 08 §2.1)

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
