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
