TAREA: corrige el perfil de las playas en la densidad del terreno (Source/Explored/WorldGen/TerrainDensity y afines). Es un modelo puro que se verifica con HostTests.

Problema, visto por el director en el juego: la playa no baja recta y suave hasta el mar. Justo antes de la orilla se curva hacia abajo, y queda poco natural.

Objetivo:
- Perfil de playa casi lineal: de 2° a 6° de pendiente desde la berma hasta la línea de agua, y que continúe bajo el agua con la misma pendiente o una algo mayor.
- Sin escalón ni caída convexa en la orilla.
- Que la transición a la duna o la vegetación siga siendo natural.

Specs: muestrea transectos perpendiculares a la costa en todas las islas con playa y comprueba:
- la pendiente máxima en la franja de ±5 m alrededor del nivel del mar;
- la monotonía, es decir, que el perfil no vuelva a subir;
- que la segunda derivada no sea negativa en la orilla, o sea, sin curvatura hacia abajo.
Mantén el determinismo por semilla y no rompas las specs existentes de TerrainDensity.

Abre la PR con la etiqueta `necesita-unreal`: hay que rehornear el terreno en local.
