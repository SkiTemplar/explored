# Fauna salvaje del acceso anticipado: nivel de detalle, población y guano

2026-09-28 · datos en `Content/Data/fauna.json` · reglas en `Tools/DataCheck` (`fauna.py`)

Todo lo de este documento es **propuesta pendiente de validar en PIE**. Las reglas del
nivel de detalle salen de la biblia 02 §11.1, y los topes de red, de la biblia 08 §2.7.

## 1. Qué hace cada especie en cada nivel de detalle

| Especie | Locomoción | Full (< 40 m) | Reduced (40–150 m) | Frozen |
|---|---|---|---|---|
| Cangrejo de los cocoteros | suelo | rutina, percepción, huida/carga, navegación fina, animación | rutina, navegación gruesa | nada |
| Gaviota | vuelo | rutina, percepción, huida, animación | rutina | nada |
| Fragata | vuelo | rutina, percepción, huida/picado, animación | rutina, oído | nada |
| Cerdo salvaje | suelo | las cinco | rutina, oído, navegación gruesa | nada |
| Cabra salvaje [F2] | suelo | las cinco | rutina, oído, navegación gruesa | nada |

Reglas que comprueba DataCheck:

- Huir o cargar solo se resuelve en Full. Un animal a 60 m no embiste a nadie.
- La rutina sigue en Reduced, porque el reloj no se para al alejarse.
- Solo la fauna de **suelo** usa el campo de navegación del servidor. Las aves no lo
  usan: así un remallado de minería (biblia 08 §2.7 b) no toca a las aves.
- El oído en Reduced se da a las especies que cazan o defienden un territorio. Así un
  disparo o una carrera las alerta antes de entrar en Full. El cangrejo, que solo ve a
  4 m, no lo necesita.

## 2. Población por isla

| Isla | Especie | Grupos × tamaño | Máx. vivos | Reposición |
|---|---|---|---|---|
| Landing | cangrejo | 6 × 1 | 6 replicados | 5 días |
| Landing | gaviota | 3 × 4–10 | ambiente | 2 días |
| Esmeralda | cerdo | 3 × 2–4 | 12 replicados | 8 días |
| Humo | — | — | — | — |
| Los Dientes | fragata | 2 × 6–14 | ambiente (colonia) | 6 días |
| Los Dientes | gaviota | 2 × 4–10 | ambiente | 2 días |
| Manglar [F2] | cangrejo | 8 × 1 | 8 replicados | 5 días |
| Meseta [F2] | cabra | 4 × 2–5 | 20 replicados | 8 días |

Topes de red:

- Ninguna isla pasa de **36** terrestres replicados vivos (12 a 10 Hz + 24 a 2 Hz por
  cliente).
- Ningún grupo pasa de 12, así que una piara entera cabe en Full.
- Las aves son fauna de ambiente con ancla. No cuentan como replicadas y su tope es de
  24 grupos.

### Qué da de comer y de materiales

- **Landing:** el cangrejo repone uno cada 5 días por hueco, así que 6 huecos dan
  ~1,2 cangrejos al día, cazando de noche. Es un complemento a la pesca y a los cocos,
  no una fuente principal (ver `2026-09-27-comida-recoleccion.md`).
- **Esmeralda:**
  - Cazar una piara de 3 cerdos da 3–6 `hueso_largo`, 3–9 `hueso_pequeno` y
    3–6 `grasa`.
  - Con 8 días de reposición, cazar a diario vacía la isla en 3 días y después da una
    piara cada ~2,7 días.
- **Los Dientes:** 2 colonias × 1–3 `guano` cada 3 días dan ~1,3 de guano al día. Con
  4 restos por tanda de compost, eso es una tanda cada ~3 días solo con guano.

## 3. Huecos abiertos

- **El cerdo no da carne ni piel.** Siguen en `lootPendiente`:
  - La carne necesita una entrada en `recipes.json/foods` y regenerar
    `Source/Explored/Cooking/CookingData.inl` con `uv run datacheck --write-cooking`.
    Este encargo nocturno no toca `Source/`, así que queda para una sesión local.
  - La piel necesita decidir si el curtido es una receta nueva o si reutiliza la del
    tiburón (`piel_bruto` → `cuero_curtido`, que tampoco tiene receta hoy).
- **Guano sin bonificación propia.** `FFarmModel` solo conoce un abono
  (`CompostGrowth` 1,5 durante 8 días), así que el guano entra como un resto más de la
  pila. Si el director quiere que abone directamente (sin pasar por la pila), hace falta
  una constante nueva en el modelo.
- **Origen del guano:** la biblia 04 §2.4 dice que el guano es exclusivo de Los
  Dientes. Aquí sale solo de las colonias de fragatas, no de las gaviotas.
