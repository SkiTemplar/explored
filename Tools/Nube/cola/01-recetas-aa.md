TAREA: termina la rama existente `datos/recetas-aa`. Tiene un commit WIP que se cortó por el límite de sesión. Haz checkout de la rama y rebasa sobre main.

Objetivo: ampliar las plantillas de crafteo de acceso anticipado. Hoy hay 19 en Content/Data/templates.json. Cada una nueva tiene que desbloquear algo que el jugador quiera hacer:

- Agua: colector de lluvia, destilador solar, odre, filtro.
- Pesca: anzuelo, caña, red, nasa, arpón.
- Caza: lazo, trampa de caída, honda.
- Fuego y luz: arco de fuego, yesca, lámpara de aceite de coco, farol.
- Ropa (enlázala con los estados del cuerpo de biblia/01): sombrero de palma, capa impermeable, sandalias, abrigo.
- Herramientas: pico (unificado con la biblia), azada, hoz, cincel, sierra, garfio, pie_de_palmera.
- Transporte: remo, rodillos, amarre, ancla.
- Base: hamaca, banco de tallado, ahumadero, telar.

Para cada una, usa roles y propiedades reales de items.json, con estación, tiempo, fase y textos ES y EN.

Añade también a biblia/03 un apartado de DESCUBRIBILIDAD, con números y textos ES y EN:
- libreta de recetas;
- pistas en los bocetos de Halden y en los petroglifos;
- inspección del tipo «esto sirve de mango».

datacheck, pytest y l10n tienen que salir en verde. Abre la PR.
