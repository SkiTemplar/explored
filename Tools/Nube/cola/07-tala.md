TAREA: completa la tala y la recolección de H0. Referencia: 00-TODO H0, apartado «Tala y recolección».

Requisitos:
- Revisa la tabla de especies talables de biblia 02 §1.2 frente a `HarvestModel`.
- Modelo puro de la caída del árbol: dirección según el golpe y el viento, y colisión contra lo construido.
- Estado `Stump` en `VegetationHarvestState`, con el día de rebrote según la especie: 18, 24 o 4 días.
- Generación periódica de `rama_seca` bajo cada árbol: de 2 a 4 cada 6 h, con tope.
- Confirma que `coco_verde` está en recipes.json.
- Replicación del estado de cada instancia según biblia 08.

Specs:
- rebrote que atraviesa un cambio de día;
- rebrote que atraviesa un guardado y una carga;
- tope de ramas;
- viento a 0 y viento máximo.
