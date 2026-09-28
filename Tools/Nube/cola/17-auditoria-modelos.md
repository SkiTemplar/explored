TAREA: audita la calidad de todos los modelos puros de Source/Explored, es decir, los que figuran en pure_sources.txt.

Pasos:
1. Pasa HostTests con `HOST_TESTS_SANITIZE=ON` y arregla todo lo que salga.
2. Busca bugs reales:
   - divisiones por cero;
   - NaN que se propagan;
   - índices fuera de rango;
   - desbordes;
   - dependencia del orden de iteración;
   - comportamiento no determinista.
3. Añade tests de propiedades o de fuzz sencillo, con semilla fija, a los modelos con más lógica.
4. Escribe un informe en docs/reviews/ con cada hallazgo, su arreglo y su test.

Abre la PR con un commit por arreglo.

PRIORIDAD: el editor compila con matemáticas rápidas y HostTests no. El 2026-09-28 un spec de `FBoatModel::Moor` pasaba en HostTests y fallaba en el editor, porque el truco `!(x > 0)` no descarta NaN con matemáticas rápidas (arreglado en la PR #65 con `FMath::IsFinite`). Hay más sitios con el mismo patrón en Source/Explored; búscalos con `git grep -nE "if \(!\(\w+ *[<>]"`.

1. Añade a HostTests una variante que compile con `-ffast-math`, igual que el editor, y que se ejecute en CI.
2. Sustituye todas las defensas contra NaN basadas en comparaciones por `FMath::IsFinite` o `FMath::IsNaN`.
