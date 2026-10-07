# Resolución del Problema del Árbol de Steiner en Grafos (STPG)
## Instancia SteinLib: `data/e18.stp` | Inteligencia Artificial (Evaluación 2)

[![C++17](https://img.shields.io/badge/Language-C%2B%2B17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![SteinLib e18](https://img.shields.io/badge/Instance-SteinLib%20e18-orange.svg)](http://steinlib.zib.de/)
[![Best Cost](https://img.shields.io/badge/Best%20Cost-573-brightgreen.svg)]()
[![Goal Rubric](https://img.shields.io/badge/Rubric%20Goal-%E2%89%A4%20580%20(11--14%20pts)-success.svg)]()
[![Execution Time](https://img.shields.io/badge/Runtime-44.4s-blueviolet.svg)]()

Repositorio oficial para la resolución del **Problema del Árbol de Steiner en Grafos (STPG)** sobre la instancia de referencia internacional `data/e18.stp` ($2.500$ nodos, $62.500$ aristas y $417$ terminales).

---

## 🚀 Guía Rápida de Ejecución (Quickstart)

El proyecto está diseñado para compilarse y ejecutarse de forma limpia y reproducible en cualquier entorno Linux, macOS o Windows (WSL / MinGW).

### 1. Compilación
```bash
make
```
> Utiliza `g++` con optimizaciones de nivel `-O3` y estándar `-std=c++17`. Genera el ejecutable `./steiner_solver`.

### 2. Ejecución Completa del Benchmark
```bash
./steiner_solver
```
El solver ejecuta automáticamente todas las fases del estudio en **~45 segundos**:
1. Carga del grafo `data/e18.stp` en memoria.
2. **Fase 1:** MST Base con Kruskal ($2.515$).
3. **Fase 2:** Poda iterativa de hojas Steiner no terminales ($1.005$).
4. **Fase 2b:** MST Prim con desempate orientado a terminales y poda ($\mathbf{779}$, batiendo la cota de pauta de $892$ en $0,3$ ms).
5. **Fase 3:** Heurística constructiva KMB sobre clausura métrica ($650$).
6. **Fase 4:** Metaheurística con búsqueda local 4-operadores, Simulated Annealing e ILS ($\mathbf{573}$).
7. Exportación y validación automática de soluciones en la carpeta `results/`.

### 3. Parámetros Personalizados (Opcional)
```bash
# Limitar tiempo de ejecución a 30 segundos
./steiner_solver data/e18.stp --time-limit 30000

# Especificar número de iteraciones
./steiner_solver data/e18.stp --iter 1500 --time-limit 45000
```

### 4. Generación de Figuras para el Informe (Opcional)
```bash
python3 scripts/plot_tree.py
```
> Lee los archivos CSV de `results/` y genera visualizaciones en alta resolución (PNG y SVG) en `report/figures/`.

---

## 📊 Tabla Oficial de Resultados vs Rúbrica de Evaluación

Comparativa entre los requerimientos oficiales de la rúbrica de la asignatura (escala de 15 puntos) y los resultados experimentales obtenidos por el solver:

| Algoritmo / Hito | Costo Obtenido | Reducción % | Brecha vs Óptimo ($564$) | Aristas ($|E|$) | Tiempo de Cómputo | Cumplimiento Rúbrica Oficial |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| **1. Kruskal Base (MST)** | $2.515$ | $0,00\%$ (Base) | $+345,9\%$ | $2.499$ | $7,8$ ms | **4 puntos** (Solución base factible) |
| **2. Kruskal Podado** | $1.005$ | $-60,04\%$ | $+78,2\%$ | $1.003$ | $0,3$ ms | Factible intermedio (poda grado 1) |
| **3. Prim Podado (Sesgo Terminal)** | $\mathbf{779}$ | $\mathbf{-69,03\%}$ | $+38,1\%$ | $777$ | $0,3$ ms | **Supera Hito $892$ (5 puntos)** |
| **4. Heurística Constructiva KMB** | $650$ | $-74,16\%$ | $+15,2\%$ | $585$ | $243$ ms | Rango 6 - 7 puntos |
| **5. Optimizador Metaheurístico** | $\mathbf{573}$ | $\mathbf{-77,22\%}$ | $\mathbf{+1,60\%}$ | $565$ | $\mathbf{44,4\text{ s}}$ | **Rango Máximo (11 - 14 puntos)** |
| *Cota Teórica SteinLib* | *564* | *-77,57%* | *0,00%* | *-* | *-* | *Óptimo Global Teórico (15 pts)* |

> [!NOTE]
> **Demostración de 1-Optimalidad:** Se verificó exhaustivamente que la solución de costo **573** es estrictamente **1-óptima**: ninguna adición, eliminación o intercambio individual de nodos Steiner entre los $291.708$ posibles mejora dicho costo. Para alcanzar $564$ se requiere una reconfiguración coordinada de $k \ge 6$ nodos simultáneamente mediante solvers exactos de Programación Lineal Entera (ILP).

---

## 📁 Estructura del Repositorio y Entregables

```text
steiner-tree-solver/
├── data/
│   └── e18.stp                 # Instancia oficial de prueba SteinLib (2.500 nodos, 62.500 aristas)
├── src/                        # Código fuente modular en C++17
│   ├── graph.hpp / .cpp        # Parser .stp, listas de adyacencia y Dijkstra multi-fuente
│   ├── dsu.hpp                 # Disjoint Set Union (DSU) con unión por rango
│   ├── validator.hpp / .cpp    # Validador formal (BFS, aciclicidad, cobertura) y exportador CSV
│   ├── mst.hpp / .cpp          # Kruskal, Prim con sesgo terminal y poda lineal de hojas
│   ├── optimizer.hpp / .cpp    # Heurística KMB, búsqueda local 4-operadores y SA/ILS
│   └── main.cpp                # Pipeline principal y cálculo de métricas oficiales
├── results/                    # Soluciones exportadas en CSV con encabezado estándar
│   ├── mst_base.csv            # Costo 2.515
│   ├── mst_pruned.csv          # Costo 1.005
│   ├── mst_prim_pruned.csv     # Costo 779
│   └── optimizer_solution.csv  # Costo 573 (Mejor solución oficial)
├── scripts/
│   └── plot_tree.py            # Generación de gráficos comparativos (Python / NetworkX / Matplotlib)
├── report/                     # Informe final académico en LaTeX
│   ├── main.tex                # Documento fuente del informe (8 secciones de rúbrica completas)
│   ├── references.bib          # Bibliografía canónica en formato BibTeX
│   └── figures/                # Gráficos generados en alta resolución (PNG y SVG)
├── Makefile                    # Reglas de compilación automatizada (-O3)
└── README.md                   # Esta documentación
```

---

## 🛠️ Métodos Implementados

1. **Parser y Representación del Grafo (`src/graph.*`):**
   * Representación mediante listas de adyacencia compactas (`std::vector<Edge>`) con indexación Base-1.
   * Manejo robusto de saltos de línea CRLF/LF.
2. **Validador Formal (`src/validator.*`):**
   * Verificación estricta de aciclicidad ($|E| = |V| - 1$ y ausencia de ciclos mediante DSU).
   * Conectividad total y alcanzabilidad de los 417 terminales mediante BFS.
3. **Poda Lineal de Hojas Steiner (`src/mst.*`):**
   * Algoritmo de poda iterativa $O(|V| + |E|)$ que remueve hojas de grado 1 no terminales hasta convergencia.
4. **Prim con Sesgo hacia Terminales (`src/mst.*`):**
   * Modificación del criterio de selección de Prim: ante aristas de igual peso, prioriza conectar terminales obligatorios, produciendo un subárbol nuclear de costo **779**.
5. **Heurística Constructiva KMB (`src/optimizer.*`):**
   * Construcción del grafo de distancias métricas sobre los 417 terminales con Dijkstra multi-fuente, extracción del MST métrico y remapeo de caminos originales.
6. **Búsqueda Local Multi-Operador y Metaheurística SA/ILS (`src/optimizer.*`):**
   * Cuatro operadores de vecindario complementarios: *Key-Path Replacement*, *Star Insertion*, *Key-Vertex Drop* y *Deg-2 Shortcut Swap*.
   * Recocido Simulado (*Simulated Annealing*) con función de aceptación Metropolis, métrica sesgada hacia peso 1 y perturbaciones dirigidas a aristas pesadas (*ILS*).

---

## 👥 Integrantes del Equipo

* **Martín Verdugo**
* **Nicolás Montecino**
