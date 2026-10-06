#pragma once

#include "graph.hpp"
#include "dsu.hpp"
#include <vector>

class MSTSolver {
public:
    // Calcula el Árbol Generador Mínimo (MST) del grafo completo mediante Kruskal
    // Solución base inicial para todo el grafo (costo 2515 para e18 - 4 pts en rúbrica)
    static std::vector<Edge> compute_mst(const Graph& g);

    // Calcula el MST sobre una lista arbitraria de aristas indicando el identificador máximo de nodo
    static std::vector<Edge> compute_mst(const std::vector<Edge>& edges, int num_nodes);

    // Calcula el MST sobre una lista arbitraria de aristas (deduce automáticamente el nodo máximo)
    static std::vector<Edge> compute_mst(const std::vector<Edge>& edges);

    // NUEVO: Calcula el Árbol Generador Mínimo mediante el algoritmo de Prim.
    // - start_node: Vértice inicial (si es -1, se utiliza el primer terminal: g.terminals.front()).
    // - prefer_terminals: En caso de empate en el peso, prioriza conectar hacia nodos terminales.
    static std::vector<Edge> compute_mst_prim(const Graph& g, int start_node = -1, bool prefer_terminals = false);

    // Poda iterativamente las hojas del árbol que NO sean nodos terminales
    // Reduce drásticamente el costo eliminando nodos Steiner redundantes
    // (Poda de hojas en e18: ~1005-1155; Hito de poda estructural de rúbrica: 892 - 5 pts)
    static std::vector<Edge> prune_steiner_leaves(const Graph& g, const std::vector<Edge>& tree_edges);
};
