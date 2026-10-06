#include "optimizer.hpp"
#include "mst.hpp"
#include <iostream>

std::vector<Edge> Optimizer::constructive_heuristic(const Graph& g) {
    // TODO (Nicolas Montecino): Implementar heurística constructiva basada en caminos más cortos (Dijkstra)
    // entre nodos terminales (ej. Algoritmo KMB / Mehlhorn).
    // Nota: Puedes utilizar g.dijkstra(...) y g.get_shortest_path_edges(...) directamente desde Graph.
    //
    // Por ahora, se retorna la solución del MST podado como base inicial:
    auto initial_mst = MSTSolver::compute_mst(g);
    return MSTSolver::prune_steiner_leaves(g, initial_mst);
}

std::vector<Edge> Optimizer::local_search(const Graph& g, const std::vector<Edge>& initial_solution, int max_iterations) {
    // TODO (Nicolas Montecino): Implementar búsqueda local (intercambio de aristas o inclusión/exclusión de nodos Steiner)
    // para reducir el costo hacia el óptimo (<= 580).
    // Nota: Puedes utilizar g.get_steiner_nodes(), g.is_steiner_node(u), g.get_degree(u), g.get_edge_weight(u, v)
    // directamente desde Graph.
    (void)g;
    (void)max_iterations;
    return initial_solution;
}

std::vector<Edge> Optimizer::optimize(const Graph& g, const OptimizerConfig& config) {
    if (config.verbose) {
        std::cout << "[Optimizer] Iniciando algoritmo de optimización...\n";
    }
    auto initial_sol = constructive_heuristic(g);
    auto final_sol = local_search(g, initial_sol, config.max_iterations);
    return final_sol;
}
