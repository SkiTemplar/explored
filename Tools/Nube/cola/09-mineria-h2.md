TAREA: implementa las reglas de minería y terreno de H2 como modelos puros. Referencia: 00-TODO H2.

Requisitos:
- Picado por esfera extendido a caliza, basalto, veta de cobre, hierro y el resto de materiales de la biblia.
- `viga_apoyo` y la regla de derrumbe: un hueco de más de 3 m sin apoyo se viene abajo.
- Aire viciado en bolsas cerradas a más de 15 m.
- Inundación de una galería conectada al mar o al nivel freático.
- Arena:
  - ángulo de reposo de 34° en seca y 45° en húmeda;
  - relleno por el oleaje en la franja intermareal;
  - `tablon_contencion` detiene ambos.
  Si el modelo de arena viva de la PR #46 ya está en main, reutilízalo.
- Modo «camino» de la pala.

Specs con casos geométricos límite y determinismo con la misma semilla.
