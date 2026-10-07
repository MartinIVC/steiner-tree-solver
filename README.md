# Resolución del Problema del Árbol de Steiner en Grafos (Instancia E18 - SteinLib)

Proyecto de **Inteligencia Artificial (Evaluación 2)** para encontrar el Árbol de Steiner de peso mínimo en un grafo ponderado no dirigido.

* **Fecha límite de entrega:** **Viernes 16 de Octubre de 2026** (Día de la prueba).
* **Ponderación:** 15 puntos de la Evaluación 2.
* **Instancia oficial:** `data/e18.stp` (SteinLib - Serie E).

---

## Integrantes y División del Trabajo

> [!IMPORTANT]
> **Nota de conformación de equipos:** La pauta oficial de la clase estipula: *"Equipos de 3 o 4 integrantes"*. Se debe confirmar con el docente si el grupo puede mantenerse con 2 integrantes o si se integrará un tercer miembro.

### Asignación de Módulos de Código y Secciones del Reporte

El reporte escrito debe incluir obligatoriamente las **8 secciones requeridas por la pauta oficial**:

| Integrante | Rol | Módulos de Código asignados | Secciones del Informe asignadas |
| :--- | :--- | :--- | :--- |
| **Martin Verdugo** | Cimientos, MST y Validador | `src/graph.*`<br>`src/mst.*`<br>`src/validator.*` | **1. Descripción del problema y modelación**<br>**2. Representación del grafo**<br>**3. Algoritmo base implementado (MST)**<br>**4. Estrategia de generación de solución factible (Poda)** |
| **Nicolas Montecino** | Optimización y Heurísticas | `src/optimizer.*` | **5. Método(s) de mejora utilizado(s)**<br>**6. Resultados obtenidos y tiempos** |
| **Ambos** | Cierre, Visualización y Análisis | `src/main.cpp`<br>`scripts/plot_tree.py` | **7. Comparación entre soluciones**<br>**8. Dificultades encontradas**<br>Revisión global y conclusiones |

---

## Contexto del Problema (Instancia E18)

* **Grafo ($G$):** 2.500 nodos ($|V|$) y 62.500 aristas ($|E|$) no dirigidas con pesos enteros positivos.
* **Nodos Terminales obligatorios ($|T|$):** 417 nodos que deben quedar obligatoriamente interconectados.
* **Nodos de Steiner ($|S|$):** 2.083 nodos opcionales que actúan como "puentes" para reducir la longitud y costo del árbol.
* **Meta de optimización:** Lograr un costo factible **$\le 580$** (rango 11-14 puntos en pauta; el óptimo teórico de 15 puntos es **564**).

---

## Arquitectura del Código y Convenciones

Para garantizar una integración fluida entre los módulos, se definieron estas convenciones:

1. **Indexación de Nodos:** Base 1 (del `1` al `2.500`), coincidiendo exactamente con la numeración de `data/e18.stp`.
2. **Estructura de Aristas:**
   ```cpp
   struct Edge {
       int u;          // Nodo 1 (1 a 2500)
       int v;          // Nodo 2 (1 a 2500)
       int weight;     // Costo de la conexión
       bool operator<(const Edge& other) const; // Ordenamiento determinista
   };
   ```
3. **Consulta de Terminales:**
   * Para consultar en tiempo $O(1)$ si un nodo `x` es terminal: `g.is_terminal[x]` (retorna `bool`).
   * Para iterar sobre todos los terminales: `g.terminals` (vector de `int`).
4. **Validación Formal:** Toda solución generada debe pasar por `Validator::is_valid_steiner_tree(g, aristas_solucion)` para verificar conexidad (BFS), cobertura de los 417 terminales y aciclicidad estricta ($|E| = |V| - 1$ sin ciclos).
5. **Utilidades en `Graph` para Heurísticas y Optimización (Nicolas):**
   * `g.dijkstra(start, dist, parent)`: Distancias mínimas y predecesores en $O((|E| + |V|) \log |V|)$.
   * `g.get_shortest_path_edges(start, target, parent)`: Vector de aristas del camino mínimo entre dos nodos.
   * `g.get_steiner_nodes()`: Lista con los 2.083 nodos Steiner para exploración y búsqueda local.
   * `g.is_steiner_node(u)` / `g.is_terminal_node(u)`: Comprobación segura en $O(1)$.
   * `g.get_edge_weight(u, v)` / `g.has_edge(u, v)`: Consulta directa de pesos o existencia de aristas.
   * `g.get_degree(u)` / `g.get_neighbors(u)`: Consulta segura de conectividad y adyacencia.

---

## Estructura del Repositorio

```text
ia-steiner-tree/
├── data/                       # Instancia oficial e18.stp (SteinLib)
├── src/                        # Código fuente en C++
│   ├── graph.hpp / .cpp        # Parser de .stp, estructura del grafo y clausura métrica
│   ├── dsu.hpp                 # Estructura Disjoint Set Union (DSU) con unión por rango
│   ├── validator.hpp / .cpp    # Validador formal (BFS, aciclicidad) y exportador CSV
│   ├── mst.hpp / .cpp          # MST Base (Kruskal, Prim) y algoritmo de poda de hojas
│   ├── optimizer.hpp / .cpp    # Heurística constructiva (Dijkstra) y búsqueda local
│   └── main.cpp                # Pipeline principal y cálculo de métricas oficiales
├── results/                    # CSVs exportados de las soluciones generadas
├── scripts/                    # Herramientas de visualización gráfica (Python / Cairo / SVG)
│   └── plot_tree.py            # Generador automático de figuras para el informe
├── report/                     # Informe final académico en LaTeX
│   ├── main.tex                # Documento principal del informe (8 secciones)
│   ├── references.bib          # Bibliografía canónica en BibTeX
│   └── figures/                # Gráficos generados de los árboles (PNG y SVG)
├── Makefile                    # Compilación automatizada en Linux/WSL (-O3)
└── README.md                   # Esta guía
```

---

## Compilación y Ejecución

### 1. En Linux o Windows con WSL (Recomendado):
```bash
# Compilar todo el proyecto con máxima optimización (-O3)
make

# Ejecutar el solver (por defecto: data/e18.stp)
./steiner_solver

# Ejecutar con parámetros personalizados para el optimizador
./steiner_solver data/e18.stp --iter 1500 --time-limit 45000

# Limpiar binarios
make clean
```

### 2. En Windows nativo (PowerShell con MinGW/GCC):
```powershell
g++ -O3 -Wall -std=c++17 src/*.cpp -o steiner_solver.exe
.\steiner_solver.exe
```

### 3. Generar Figuras Gráficas para el Informe:
```bash
# Procesa todos los CSV en results/ y genera PNG/SVG en report/figures/
python3 scripts/plot_tree.py
```

---

## Tabla de Hitos, Resultados y Ponderación Oficial

Según la rúbrica de evaluación de la clase (diapositiva 313, escala de 15 puntos):

| Método / Hito | Costo | Reducción % (vs Base) | Tiempo Estimado | Ponderación Oficial (15 pts) | Estado / Observación |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **1. Solución Parcial** | Variable | - | - | **1 - 3 puntos** | No conexo, elimina terminales o contiene ciclos. |
| **2. MST Base (Kruskal)** | **2.515** | $0,00\%$ | $\sim 4$ ms | **4 puntos** | Factible. Conecta los 2.500 nodos del grafo. |
| **3. MST Podado (Poda básica)** | **1.005 - 1.155** | $\sim -54\%$ a $-60\%$ | $< 1$ ms | - | Factible. Poda hojas de grado 1 no terminales. |
| **4. Poda Estructural Óptima** | **892** | **$-64,53\%$** | $< 5$ ms | **5 puntos** | Hito oficial de pauta: elimina redundancias estructurales. |
| **5. Solución Factible Mala Calidad** | $[601, 891]$ | $-64,6\%$ a $-76,1\%$ | - | **6 - 7 puntos** | Heurística preliminar. |
| **6. Solución Factible Calidad Media** | $[581, 600]$ | $-76,1\%$ a $-76,9\%$ | - | **8 - 10 puntos** | Heurística constructiva intermedia. |
| **7. Solución Factible Buena Calidad** | **$[565, 580]$** | **$-76,9\%$ a $-77,5\%$** | $< 1$ hora | **11 - 14 puntos** | **Meta establecida por el equipo ($\le 580$).** |
| **8. Solución Óptima SteinLib** | **564** | **$-77,57\%$** | - | **15 puntos** | Óptimo global teórico documentado en literatura. |

### Resultados Consolidados Obtenidos por el Solver

| Algoritmo Implementado | Costo Obtenido | Reducción % | Brecha vs Óptimo ($564$) | Aristas ($|E|$) | Tiempo de Cómputo | Cumplimiento Rúbrica |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| **MST Base (Kruskal)** | $2.515$ | $0,00\%$ | $+345,9\%$ | $2.499$ | $4,8$ ms | Cumple Hito 4 pts |
| **MST Podado (Kruskal)** | $1.005$ | $-60,04\%$ | $+78,2\%$ | $1.003$ | $0,3$ ms | Factible intermedio |
| **Prim Podado (Bias Terminal)** | $\mathbf{779}$ | $\mathbf{-69,03\%}$ | $+38,1\%$ | $777$ | $0,4$ ms | **Supera Hito $892$ (5 pts)** |
| **Heurística KMB Base** | $650$ | $-74,16\%$ | $+15,2\%$ | $585$ | $243$ ms | Rango 6 - 7 pts |
| **Optimizador Metaheurístico** | $\mathbf{573}$ | $\mathbf{-77,22\%}$ | $\mathbf{+1,60\%}$ | $565$ | $\mathbf{44,5\text{ s}}$ | **Rango Máximo (11 - 14 pts)** |

> [!NOTE]
> La solución de costo **573** cuenta con solo 8 aristas de peso 2 y 557 de peso 1. Se demostró experimentalmente que es estrictamente **1-óptima** frente a cualquier intercambio o eliminación individual de nodos Steiner.


---

## Flujo de Trabajo en Git

Para trabajar en paralelo de forma ordenada y sin conflictos:
1. **Crear una rama para el módulo asignado:**
   ```bash
   git checkout -b feature/mi-modulo
   ```
2. **Realizar commits con mensajes descriptivos:**
   ```bash
   git commit -m "Implementa KMB con Dijkstra en optimizer.cpp"
   ```
3. **Subir la rama y sincronizar mediante Pull Request / Merge:**
   ```bash
   git push origin feature/mi-modulo
   ```
