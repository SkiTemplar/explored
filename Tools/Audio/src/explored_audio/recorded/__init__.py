"""Banda sonora grabada de licencia abierta (encargo 21).

Complementa la musica generada por codigo de `explored_audio.music` sin
sustituirla: son grabaciones reales (piano, guitarra, arpa, cuarteto) de
dominio publico, CC0 o CC-BY, descritas en `Tools/Audio/music_sources.json`.

El audio no se versiona: `explored-music fetch` descarga cada original,
comprueba su SHA-256, lo normaliza a -16 LUFS, recorta los silencios y
exporta OGG a `Tools/Audio/.cache/music/` (ignorada por git).
"""
