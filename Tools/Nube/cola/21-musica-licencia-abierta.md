TAREA: consigue una banda sonora grabada de licencia abierta que complemente la música generada del encargo 03, sin sustituirla.

Criterio del director: música clásica suave, acústica, que invite a explorar. Nada retro, nada de chiptune, nada épico.

Fuentes admitidas (licencia comprobada pieza a pieza en la página oficial):
- Musopen: grabaciones de dominio público o CC0.
- Wikimedia Commons: grabaciones de dominio público o CC0.
- FreePD: CC0.
- Kevin MacLeod (incompetech): CC-BY 4.0, solo con crédito.
- OpenGameArt: solo CC0 o CC-BY.
Nada con NC ni ND, y nada cuya licencia no se pueda verificar.

Qué hacer:
1. Selecciona de 15 a 25 piezas: Satie, Debussy, Chopin tranquilo, guitarra clásica, arpa, cuerdas lentas. Reparte día, noche, lluvia, mar, cueva y ruinas según Content/Data/music_layers.json.
2. Crea `Tools/Audio/music_sources.json` con una entrada por pieza: título, intérprete, URL de origen, licencia, URL de la licencia, SHA-256 y momento de juego.
3. Escribe un script con uv que descargue, verifique el hash, normalice a unos -16 LUFS, recorte los silencios y exporte OGG a una caché ignorada por git. Los ficheros de audio no se versionan si pasan de 10 MB en total; en ese caso el script los regenera.
4. Genera `docs/creditos-musica.md` con los créditos CC-BY en ES y EN, listos para los créditos del juego y para Steam.
5. Tests: que todas las licencias pertenezcan a la lista permitida, que los hashes cuadren, que no haya clipping, que se cumplan los LUFS y que ninguna pieza CC-BY se quede sin crédito.

Abre la PR con la etiqueta `necesita-unreal`: la importación a Content/Audio se hace en local.
