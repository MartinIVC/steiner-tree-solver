#include "optimizer.hpp"
#include "mst.hpp"
#include <iostream>

std::vector<Edge> Optimizer::constructive_heuristic(const Graph& g) {
    // TODO (Integrante 2): Implementar heurística constructiva basada en caminos más cortos (Dijkstra)
    // entre nodos terminales (ej. Algoritmo KMB / Mehlhorn).
    // Por ahora, se retorna la solución del MST podado como base inicial:
    auto initial_mst = MSTSolver::compute_mst(g);
    return MSTSolver::prune_steiner_leaves(g, initial_mst);
}

std::vector<Edge> Optimizer::local_search(const Graph& g, const std::vector<Edge>& initial_solution, int max_iterations) {
    // TODO (Integrante 2): Implementar búsqueda local (intercambio de aristas o inclusión/exclusión de nodos Steiner)
    // para reducir el costo hacia el óptimo (<= 580).
    return initial_solution;
}

std::vector<Edge> Optimizer::optimize(const Graph& g) {
    std::cout << "[Optimizer] Iniciando algoritmo de optimización...\n";
    auto initial_sol = constructive_heuristic(g);
    auto final_sol = local_search(g, initial_sol, 1000);
    return final_sol;
}
