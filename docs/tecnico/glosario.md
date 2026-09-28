# Glosario ES/EN de Explored

Fuente: biblia 07 §5.2 (decisiones de nombre propio y términos del juego). Este fichero es
la copia de trabajo que usa `Tools/Localization`: si cambias un término, cámbialo aquí y en
la biblia a la vez. La guía general de localización está en `localizacion.md`.

Regla: un término del glosario se traduce siempre igual. Cuando había más de una opción
razonable, la nota lo marca como **(decisión)**.

## Comprobación automática

La última columna es la regla que aplica `uv run l10n`: si el español de un texto casa con
la expresión de la izquierda (expresión regular, distingue mayúsculas), el inglés tiene que
contener la de la derecha (sin distinguir mayúsculas). Si no, sale un aviso, y con
`--strict` falla. Solo se comprueban los nombres propios y los términos donde el inglés
obvio es el equivocado (galería → *tunnel*, no *gallery*); los términos comunes
(«mina», «huerto») se dejan a la revisión humana porque cambian de forma con la frase.

## Términos (69)

| Español | Inglés | Nota | Comprobación |
|---|---|---|---|
| Explored | Explored | Título, no se traduce | — |
| Archipiélago | Archipelago | — | — |
| Isla del Amaraje / Landing | Landing (Island) | El nombre de juego ya es «Landing» en los dos idiomas | `Isla del Amaraje` → `Landing` |
| Esmeralda | Emerald | — | `Esmeralda` → `Emerald` |
| Isla del Humo | Smoke Island | — | `Isla del Humo` → `Smoke Island` |
| Los Dientes | The Teeth | — | `Los Dientes` → `Teeth` |
| Manglar de las Voces | Mangrove of Voices | — | `Manglar de las Voces` → `Mangrove of Voices` |
| Arenas Blancas | White Sands | — | `Arenas Blancas` → `White Sands` |
| La Meseta | The Mesa | **(decisión)** «mesa» ya es término geográfico inglés (préstamo del español); evita confundir con «plateau», más genérico | `Meseta` → `Mesa` |
| Isla oculta | Hidden Island | — | `[Ii]sla oculta` → `hidden island` |
| Albatros | Albatross | El avión conserva su nombre propio, con la grafía inglesa correcta del ave | `Albatros` → `Albatross` |
| Barco «Limón» | The boat «Limón» | El nombre propio no se traduce nunca (dedicatoria) | `«Limón»` → `«Limón»` |
| Náufrago | Castaway | También nombre del modo de juego | `Náufrago` → `Castaway` |
| Hidroavión | Seaplane | — | `[Hh]idroavión` → `seaplane` |
| Pueblo del arrecife | Reef village | **(decisión)** descriptivo, no nombre propio de cultura — evita el riesgo de sensibilidad cultural del GDD §8 | `[Pp]ueblo del arrecife` → `reef village` |
| Piratas | Raiders | **(decisión)** el módulo de código ya se llama `Raiders`; se usa también en el texto que ve el jugador para mantener una sola palabra en los dos idiomas | `\b[Pp]iratas?\b` → `raid` |
| Wayfinding | Wayfinding | El propio español ya usa el préstamo inglés (biblia §9.2) | — |
| Marae | Marae | Término real, no se traduce (biblia §9.1) | `\b[Mm]arae\b` → `marae` |
| Petroglifo | Petroglyph | — | `[Pp]etroglifos?` → `petroglyph` |
| Camino de estrellas | Star path | — | `[Cc]aminos? de estrellas` → `star path` |
| Lectura del oleaje | Swell reading | — | `[Ll]ectura del oleaje` → `swell reading` |
| Aves al atardecer | Birds at dusk | — | `[Aa]ves al atardecer` → `birds at dusk` |
| Nubes fijas | Fixed clouds | — | `[Nn]ubes fijas` → `fixed cloud` |
| Color del agua | Water colour | — | `[Cc]olor del agua` → `water colour` |
| Trueque | Barter | — | — |
| Reputación | Standing / reputation | «Standing» en UI corta (cabe mejor), «reputation» en texto largo | — |
| Museo | Museum | — | — |
| Vitrina | Display case | — | `[Vv]itrinas?` → `display case` |
| Estantería | Shelf | — | — |
| Herbario | Herbarium | — | — |
| Cuaderno de bocetos | Sketchbook | — | — |
| Colección | Collection | — | — |
| Mina | Mine | — | — |
| Galería (minera) | Tunnel | **(decisión)** «gallery» en inglés suena a arte; «tunnel» es el término minero correcto | `[Gg]alerías?` → `tunnel` |
| Veta | Ore seam | — | `\b[Vv]etas?\b` → `seam` |
| Estrato | Stratum (pl. strata) | — | — |
| Derrumbe | Cave-in | — | `[Dd]errumbes?` → `cave-in` |
| Aire viciado | Foul air | — | `[Aa]ire viciado` → `foul air` |
| Inundación | Flooding | — | — |
| Cenote | Cenote | Préstamo geológico ya internacional, se mantiene en los dos idiomas | — |
| Tubo de lava | Lava tube | — | `[Tt]ubos? de lava` → `lava tube` |
| Caverna de cristal | Crystal cavern | — | — |
| Río subterráneo | Underground river | — | — |
| Bioluminiscencia | Bioluminescence | — | — |
| Marea viva | Spring tide | — | `[Mm]areas? vivas?` → `spring tide` |
| Marea muerta | Neap tide | — | `[Mm]areas? muertas?` → `neap tide` |
| Bajamar | Low tide | — | `[Bb]ajamar` → `low tide` |
| Pleamar | High tide | — | `[Pp]leamar` → `high tide` |
| Monzón | Monsoon | — | — |
| Ciclón | Cyclone | — | — |
| Escorbuto | Scurvy | — | — |
| Limonero | Lemon tree | — | `[Ll]imoneros?` → `lemon tree` |
| Huerto | Garden | — | — |
| Granja | Farm | — | — |
| Corral | Pen | — | — |
| Gallinero | Coop | — | `[Gg]allineros?` → `coop` |
| Pocilga | Sty | — | `[Pp]ocilgas?` → `sty` |
| Colmena | Beehive | — | — |
| Estanque de peces | Fish pond | — | — |
| Vagón (de mina) | Mine cart | — | `[Vv]ag[oó]n(es)?` → `cart` |
| Vía (de mina) | Track | — | — |
| Torno | Winch | — | `\b[Tt]ornos?\b` → `winch` |
| Apuntalamiento | Shoring | — | — |
| Muralla | Wall | — | — |
| Empalizada | Palisade | — | `[Ee]mpalizadas?` → `palisade` |
| Torre de vigía | Watchtower | — | `[Tt]orres? de vigía` → `watchtower` |
| Integridad estructural | Structural integrity | — | — |
| Modo Explorador | Explorer mode | — | — |
| Dedicatoria | Dedication | «Para Almudena, mi Limón», idéntica en los dos idiomas (`localizacion.md`) | `Para Almudena, mi Limón` → `Para Almudena, mi Limón` |
