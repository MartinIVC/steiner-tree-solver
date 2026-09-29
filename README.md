# Resolución del Problema del Árbol de Steiner en Grafos (Instancia E18 - SteinLib)

Proyecto de Inteligencia Artificial para encontrar el Árbol de Steiner de peso mínimo en un grafo no dirigido con pesos.

## Integrantes y División del Trabajo

| Integrante | Rol | Módulos de Código asignados | Secciones del Informe asignadas |
| :--- | :--- | :--- | :--- |
| **Martin Verdugo** | Cimientos, MST y Validador | `src/graph.*`, `src/mst.*`, `src/validator.*` | 1. Modelación, 2. Grafo, 3. MST Base, 4. Poda |
| **Nicolas Montecino** | Optimización y Heurísticas | `src/optimizer.*` | 5. Heurísticas de Mejora, 6. Resultados y Tiempos |
| **Ambos** | Cierre y Documentación | `src/main.cpp`, `scripts/plot_tree.py` | Comparativas, Conclusiones y Revisión |

---

## Contexto del Problema (Instancia E18)

* **Grafo:** 2.500 nodos y 62.500 aristas no dirigidas con pesos positivos.
* **Nodos Terminales obligatorios ($|T|$):** 417 nodos que deben quedar interconectados al menor costo posible.
* **Nodos de Steiner:** Nodos opcionales que actúan como "puentes" para reducir la longitud del cableado.
* **Meta de optimización:** Lograr un costo factible **$\le 580$** (el óptimo teórico global es **564**).

---

## Arquitectura del Código y Convenciones Importantes

Para que el código de ambos encaje perfectamente, se definieron estas reglas en los `.hpp`:

1. **Indexación de Nodos:** Base 1 (del `1` al `2.500`), coincidiendo exactamente con el archivo `data/e18.stp`.
2. **Estructura de Aristas:**
   ```cpp
   struct Edge {
       int u;      // Nodo 1 (1 a 2500)
       int v;      // Nodo 2 (1 a 2500)
       int weight; // Costo
   };
   ```
3. **Consulta de Terminales:**
   * Para consultar en tiempo constante si un nodo `x` es terminal: `g.is_terminal[x]` (retorna `bool`).
   * Para iterar sobre todos los terminales: `g.terminals` (vector de `int`).
4. **Validación:** Toda solución generada debe pasar por `Validator::is_valid_steiner_tree(g, aristas_solucion)` para verificar conexidad y cobertura total de los 417 terminales.

---

## Estructura del Repositorio

```text
ia-steiner-tree/
├── data/                       # Instancia oficial e18.stp
├── src/                        # Código fuente en C++
│   ├── graph.hpp / .cpp        # Parser de .stp y estructura del grafo
│   ├── validator.hpp / .cpp    # Verificador de factibilidad (BFS) y cálculo de costos
│   ├── mst.hpp / .cpp          # MST Base (Kruskal) y algoritmo de poda de hojas
│   ├── optimizer.hpp / .cpp    # Heurística constructiva (Dijkstra) y búsqueda local
│   └── main.cpp                # Pipeline principal y medición de tiempos
├── results/                    # CSVs con aristas de soluciones y evidencias
├── scripts/                    # Scripts en Python para visualización
├── report/                     # Informe final en PDF
├── Makefile                    # Compilación automatizada en Linux/WSL
└── README.md                   # Esta guía
```

---

## Compilación y Ejecución

### En Linux o Windows con WSL (Recomendado):
```bash
# Compilar todo el proyecto con máxima optimización (-O3)
make

# Ejecutar el solver
./steiner_solver

# Limpiar archivos compilados
make clean
```

### En Windows nativo (PowerShell / MinGW):
```powershell
# Compilar manualmente
g++ -O3 -Wall -std=c++17 src/*.cpp -o steiner_solver.exe

# Ejecutar
.\steiner_solver.exe
```

---

## Tabla de Hitos y Resultados Esperados

| Método | Costo Esperado | Tiempo Estimado | Estado / Validación |
| :--- | :---: | :---: | :---: |
| **1. MST Base (Kruskal)** | 2.515 | ~3 ms | Factible (Cubre todo el grafo) |
| **2. MST Podado (Poda de hojas)** | 1.155 | ~0.5 ms | Factible (Elimina hojas no terminales) |
| **3. Heurística / Optimización** | **$\le 580$** | < 1 hora | En desarrollo (Integrante 2) |
| **Óptimo Teórico** | **564** | - | Referencia SteinLib |

---

## Flujo de Trabajo en Git

Para no sobreescribir el trabajo del otro:
1. Crear una rama para trabajar: `git checkout -b feature/mi-modulo`
2. Hacer commits descriptivos: `git commit -m "Implementa caminos mínimos en optimizer"`
3. Subir la rama: `git push origin feature/mi-modulo` y hacer Pull Request o Merge a `master` previa revisión.
