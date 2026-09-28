TAREA: implementa los barcos por piezas de H3 como modelos puros. Referencia: 00-TODO H3.

La PR #53 (astillero, botadura, uniones y amarre) puede estar ya en main o seguir abierta. Léela y construye encima de ella; no la dupliques.

Requisitos:
- Casco por piezas: quilla, cuaderna, tablón, cubierta, mástil, vela y el resto que marque la biblia. Sustituye la tabla fija por `EBoatType` por un cálculo de flotación y estabilidad.
- Integridad por cada unión cuaderna–tablón, y daño por impacto.
- boats.json:
  - los cuatro planos canónicos listan sus piezas;
  - `barco_limon` exige sus 4 `requiresShipParts`.
- `FExploredBoatNetState` de 19 B a 20 Hz (biblia 08), con cuantización y round-trip.
- Logro `primera_canoa`.

Specs:
- casco incompleto;
- sobrecarga;
- vuelco;
- cuantización en los extremos del rango.
