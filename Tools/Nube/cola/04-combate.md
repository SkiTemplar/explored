TAREA: implementa el sistema de combate de H1 como modelo puro `FCombatModel`. Referencias: biblia 05 §3 y 00-TODO H1, apartado «Combate y fauna peligrosa».

Requisitos:
- Daño instantáneo leyendo las propiedades del item en items.json: cortante y perforante ×3, contundente ×4.
- Un corte abre una herida de profundidad = propiedad ÷ 5, enganchada al modelo de heridas que ya existe (`wounds`).
- Golpe rápido: ×0.7, encadenable hasta 3 veces con una pausa de 0.4 s.
- Golpe cargado: ×1.6, con aviso previo (telegraph).
- Esquiva: 0.3 s de invulnerabilidad y 1.2 s de reutilización.
- Precisión del arco por tramos de distancia: 100/70/40/0 %.
- Estadísticas de combate como datos para el cerdo salvaje, la cabra montés, el cangrejo de los cocos y el tiburón de arrecife genérico.
- Replicación según biblia 08: el servidor resuelve los impactos. Define qué se envía y cuántos bytes ocupa.

Specs exhaustivas:
- encadenado de golpes y cortes del combo;
- ventanas de invulnerabilidad al milisegundo;
- daño con propiedades a 0 o ausentes;
- distancias justo en los límites de cada tramo.
