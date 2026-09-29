#pragma once

#include "graph.hpp"
#include <vector>

class Optimizer {
public:
    // Heurística constructiva basada en caminos mínimos (Dijkstra) entre terminales
    static std::vector<Edge> constructive_heuristic(const Graph& g);

    // Búsqueda local o metaheurística para mejorar una solución existente
    static std::vector<Edge> local_search(const Graph& g, const std::vector<Edge>& initial_solution, int max_iterations = 1000);

    // Método principal de optimización para competir por el óptimo (objetivo costo <= 580)
    static std::vector<Edge> optimize(const Graph& g);
};
