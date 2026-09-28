TAREA: implementa la escalada de H1 como modelo puro `FClimbModel`. Referencias: 00-TODO H1, apartado «Escalada», y biblia 02.

Requisitos:
- Trepar a palmeras (`Palm`), con y sin `pie_de_palmera`, con consumo de energía y posibilidad de caída.
- Escalada de roca en pendientes de más de 60°, con un tope de 3 m sin clavijas. `clavija_roca` y `cuerda_fija` amplían ese tope.
- Estados: agarrado, trepando, descansando y cayendo. El daño por caída depende de la altura.
- Da de alta en items.json y templates.json `pie_de_palmera` (cuerda ×1) y `clavija_roca` si todavía no existen.
- Replicación según biblia 08: el movimiento propio se predice y el servidor lo valida.
- Textos de aviso en ES y EN.

Specs:
- las transiciones ilegales se rechazan;
- la energía llega a 0 a mitad de la trepa;
- pendiente exactamente de 60°;
- caídas desde 2,99 m, 3 m y 3,01 m.
