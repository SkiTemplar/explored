TAREA: implementa los sistemas de la fase 3 como modelos puros (00-TODO F3), sin arte ni actores.

Requisitos:
- Reputación por asentamiento, de 0 a 100 y sin decaimiento pasivo, con su sección en el guardado.
- Trueque:
  - valor de 1 a 5 por objeto × tasa de reputación;
  - ventana horaria;
  - enfriamiento de 15 días si la reputación cae por debajo de 20.
- Amenaza pirata de 0 a 100, con sus reglas de subida y bajada, y un programador de asaltos por categoría.
- Campamentos fijos generados por semilla.
- Devolver un objeto ritual suma +5 de reputación.
- Los navegantes nunca son enemigos: es una decisión de diseño cerrada.

Specs deterministas por semilla y en los límites de cada escala.
