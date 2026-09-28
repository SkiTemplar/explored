TAREA: completa el inventario de H1. Referencia: 00-TODO H1, apartados «Inventario y UI general», «Red» y «Tests».

Requisitos:
- Apilado de hasta 10 unidades por hueco en `InventoryModel` y `Carry`, respetando el peso y los huecos de la mochila.
- Tabla de ids `uint16` derivada de ordenar los ids de Content/Data/*.json. Tiene que ser estable: demuestra con un test que un id nuevo no reordena los existentes, o documenta la estrategia de versionado.
- Modelo puro de la serialización compacta del inventario propio para replicarlo (el formato de `FFastArraySerializer`): qué se envía en cada cambio.
- Amplía CarrySpec.cpp y Tools/DataCheck con estas reglas.

Specs:
- apilar, partir y fusionar pilas;
- pila llena;
- peso en el límite;
- ids desconocidos al cargar una partida.
