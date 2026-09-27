TAREA: termina la rama existente `nube/red-h0-2026-09-28`. Tiene un commit WIP a medias: NetBudgetModel y una spec de checksum. Haz checkout de la rama y rebasa sobre main.

Implementa como modelos puros las piezas de red de H0 de biblia/08:
1. Códec de deltas de terreno por chunk: serialización compacta, cuantización, RLE o delta, y tamaño máximo por paquete.
2. Cola de envío con presupuesto de 8 KB/s y prioridad por distancia.
3. Checksum FNV-1a por chunk cada 30 s.
4. Validador de presupuesto para `Explored.NetBudget`.

Tests que cacen bugs reales:
- el round-trip devuelve exactamente lo que entró;
- fusionar es idempotente y conmutativo;
- límites de paquete;
- ráfagas;
- un solo byte cambiado se detecta;
- una entrada corrupta o truncada no provoca crash.

HostTests en verde, también con sanitizers. Abre la PR.
