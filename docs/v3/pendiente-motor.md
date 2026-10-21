# Pendiente de motor (Baliza Siete)

Trabajo que necesita abrir Unreal (editor, `-game`, commandlets o MCP) y que se dejó apuntado porque
el PC estaba en uso. Cada tramo indica el comando exacto, en orden, y qué comprobar. Todos los
comandos se lanzan desde la raíz del repositorio en PowerShell. Antes de empezar:
`powershell -NoProfile -File Tools/build.ps1` (debe terminar en `Result: Succeeded`).

## F6 — Cierre (2026-10-21)

Estado del código: final escenificado completo y compilado (`Tools/build.ps1` verde); la secuencia
(tiempos, fases, llama, haz, epílogo según páginas, créditos) vive en `FFinaleSequenceModel` y la
cota de duración en `FRunPacingModel`, ambos con spec en los host tests. Textos repasados y
localización regenerada (`l10n export --check` verde).

### 1. Materiales de F6

```powershell
powershell -NoProfile -File Tools/unreal_python.ps1 -Script Tools/Unreal/materials_f6.py
```

Reconstruye `M_BeaconBeam`, `M_RainStreak`, `M_TerrainToon` y el viento de `M_Toon`. Comprobar:

- El log termina sin `Error` y lista los cuatro materiales.
- `M_BeaconBeam` tiene los parámetros `BeamOrigin` (vector), `BeamLength` y `BeamIntensity`
  (escalares): `ALighthouseLamp::UpdateFlame` los escribe por nombre y, si falta alguno, el cono
  se ve a intensidad fija sin apagarse con la distancia.

Si `sfx_ship_horn` o `sfx_lens_rotation_loop` no están en `Content/Generated/Audio/Efectos`
(son locales y no se versionan), generarlos e importarlos antes:

```powershell
cd Tools/Audio; uv run explored-audio build --only sfx_ship_horn,sfx_lens_rotation_loop; cd ../..
powershell -NoProfile -File Tools/unreal_python.ps1 -Script Tools/Unreal/import_audio.py
```

(`build` escribe en `Art/Export/Audio`; `import_audio.py` lo lleva a `Content/Generated/Audio`.)

### 2. Capturas del final

```powershell
powershell -NoProfile -File Tools/shots.ps1 -Set f6 -Width 1920 -Height 1080
```

Deja en `Saved/Shots` cinco capturas; copiarlas a `docs/v3/capturas/`:

| Captura | Qué comprobar |
|---|---|
| `f6-final-encender.png` | A los 2 s de los 3: llama visible y a medio crecer en la linterna; sin cono todavía |
| `f6-final-haz.png` | Plano del final detrás del faro; cono del haz legible sobre el mar de noche, sin cortar en seco al tocar el agua |
| `f6-final-mistral.png` | Luces del Mistral más cerca que en la anterior (captura en el instante de la bocina, ~9,6 s) |
| `f6-final-epilogo.png` | Pantalla de epílogo con la línea variable y «Isla de Sal. Jueves, 23:40.»; fundido terminado |
| `f6-final-creditos.png` | Créditos del final con la música de créditos |

### 3. Las tres partidas del bot (criterios 1 y 2 de F6)

A 1× real (lo que mide el criterio) cada una tarda ~53 min; con `-Clock 6` el bot acelera el reloj y
el informe sigue dando los minutos equivalentes a 1×.

```powershell
powershell -NoProfile -File Tools/playtest_v3.ps1 -Route competente -Clock 6 -TimeoutMinutes 50
powershell -NoProfile -File Tools/playtest_v3.ps1 -Route lenta -Clock 6 -TimeoutMinutes 50
powershell -NoProfile -File Tools/playtest_v3.ps1 -Route sin_paginas -Clock 6 -TimeoutMinutes 50
```

Comprobar en cada informe (`Saved/Shots/.../playtest.md` y `playtest.json`) y en
`Saved/Logs/PlaytestV3.log`:

| Ruta | Duración a 1× | Desmayos | Páginas | Epílogo (línea del log `[Baliza] Epílogo …`) |
|---|---|---|---|---|
| competente | 50-60 min (previsto por `FRunPacingModel`: 52,2; medido en 918c2ba0: 52,4) | 0 | 8/8 | `completo (8/8 páginas)` |
| lenta | ≤ 65 min | ≤ 1 | 8/8 | `completo (8/8 páginas)` |
| sin_paginas | 47-65 min | 0 | < 8 (solo las del camino crítico que lea el guion) | `parcial (N/8 páginas)`, N < 8 |

Además: `reached_epilogue: true` en las tres, 0 `Error` en el log, y la secuencia
`Luces del Mistral` → `Bocina del Mistral` (~9,6 s después) → `Pantalla de epílogo` (~7 s después) →
`Créditos del final` → menú principal.

Cálculo de la cota (sin motor, `Tests/RunPacingSpec.cpp`): despierto D1 07:00-20:00 (13 h), D2
06:00-02:00 con el ciclón (20 h) y D3 06:00-20:00 (14 h) = 47 h × 65 s = 50,9 min; más el final
hasta el epílogo (3 s de encender + 16,6 s) y 60 s de créditos = 52,2 min. Con el día de 24 min de F3e
daba 48,3 (fuera del rango), y con 32 min se pasaría de 60.

### 4. Mezcla de audio en juego

Una partida a mano (o la ruta competente con `-Window`) escuchando:

- Bocina (`sfx_ship_horn`, tono 1): se oye por encima de la música del final sin saturar.
- Lente (`sfx_lens_rotation_loop`, 0,9, atenuada en la linterna): audible arriba, casi nada desde la
  cámara del final; si no se oye en el plano del final, subirla o hacerla 2D durante el final.
- Transición de música: final al encender, créditos al pulsar «Continuar», y silencio limpio al
  volver al menú.
- Ambiente por zona y lluvia del ciclón sin cortes al entrar en la base del faro.

Apuntar en `docs/creditos-musica.md` cualquier cambio de pista.
