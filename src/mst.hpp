#pragma once

#include "graph.hpp"
#include <vector>

class MSTSolver {
public:
    // Calcula el Árbol Generador Mínimo (MST) del grafo completo (Kruskal o Prim)
    // Solución base inicial para todo el grafo (costo esperado ~2515 para e18)
    static std::vector<Edge> compute_mst(const Graph& g);

    // Poda iterativamente las hojas del árbol que NO sean nodos terminales
    // Reduce drásticamente el costo del árbol conservando la conectividad de los terminales (costo ~892 para e18)
    static std::vector<Edge> prune_steiner_leaves(const Graph& g, const std::vector<Edge>& tree_edges);
};
