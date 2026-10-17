#include "dsu.hpp"
#include "graph.hpp"
#include "mst.hpp"
#include "optimizer.hpp"
#include "validator.hpp"

#include <cassert>
#include <iostream>
#include <vector>

namespace {

Graph build_synthetic_graph(int weight_scale = 1) {
    // Grafo con 6 nodos:
    // Terminales: 1, 2, 3
    // Nodo Steiner central: 4 conectado a 1, 2, 3 con costo 1 * scale (estrella óptima = 3 * scale)
    // Aristas directas entre terminales (1-2, 2-3, 1-3) con costo 3 * scale (MST terminal = 6 * scale)
    // Nodo Steiner hoja redundante: 5 conectado a 1 con costo 1 * scale
    // Nodo Steiner puente redundante: 6 conectado a 2 y 3 con costo 2 * scale
    Graph g;
    g.num_nodes = 6;
    g.num_edges = 8;
    g.num_terminals = 3;
    g.adj.assign(g.num_nodes + 1, {});
    g.is_terminal.assign(g.num_nodes + 1, false);

    for (int t : {1, 2, 3}) {
        g.terminals.push_back(t);
        g.is_terminal[t] = true;
    }

    g.add_edge(1, 2, 3 * weight_scale);
    g.add_edge(2, 3, 3 * weight_scale);
    g.add_edge(1, 3, 3 * weight_scale);
    g.add_edge(1, 4, 1 * weight_scale);
    g.add_edge(2, 4, 1 * weight_scale);
    g.add_edge(3, 4, 1 * weight_scale);
    g.add_edge(1, 5, 1 * weight_scale);
    g.add_edge(2, 6, 2 * weight_scale);

    return g;
}

void test_dsu() {
    DSU dsu(5);
    assert(dsu.find(1) != dsu.find(2));
    assert(dsu.unite(1, 2));
    assert(dsu.find(1) == dsu.find(2));
    assert(!dsu.unite(1, 2)); // Ya unidos -> ciclo
    std::cout << "[PASS] test_dsu\n";
}

void test_validator_soundness() {
    Graph g = build_synthetic_graph(1);

    // Árbol estrella óptimo sobre nodo Steiner 4: (1,4,1), (2,4,1), (3,4,1) -> costo 3
    std::vector<Edge> valid_star = {{1, 4, 1}, {2, 4, 1}, {3, 4, 1}};
    assert(Validator::is_valid_steiner_tree(g, valid_star, false));
    assert(Validator::compute_cost(valid_star) == 3);
    assert(!Validator::has_steiner_leaves(g, valid_star));

    // Rechazar arista inexistente en el grafo original (5, 6)
    std::vector<Edge> fake_edge = {{1, 4, 1}, {2, 4, 1}, {5, 6, 1}, {3, 4, 1}};
    assert(!Validator::is_valid_steiner_tree(g, fake_edge, false));

    // Rechazar arista con peso adulterado (1, 2 con peso 1 en lugar de 3)
    std::vector<Edge> tampered_weight = {{1, 2, 1}, {2, 3, 3}};
    assert(!Validator::is_valid_steiner_tree(g, tampered_weight, false));

    // Rechazar ciclo
    std::vector<Edge> cyclic = {{1, 4, 1}, {2, 4, 1}, {3, 4, 1}, {1, 2, 3}};
    assert(Validator::has_cycle(cyclic));
    assert(!Validator::is_valid_steiner_tree(g, cyclic, false));

    // Rechazar terminal faltante
    std::vector<Edge> missing_terminal = {{1, 4, 1}, {2, 4, 1}};
    assert(!Validator::is_valid_steiner_tree(g, missing_terminal, false));

    std::cout << "[PASS] test_validator_soundness\n";
}

void test_mst_and_pruning() {
    Graph g = build_synthetic_graph(1);
    auto kruskal = MSTSolver::compute_mst(g);
    auto prim = MSTSolver::compute_mst_prim(g, -1, true);

    assert(Validator::is_valid_steiner_tree(g, kruskal, false));
    assert(Validator::is_valid_steiner_tree(g, prim, false));
    assert(Validator::compute_cost(kruskal) == Validator::compute_cost(prim));

    auto pruned = MSTSolver::prune_steiner_leaves(g, kruskal);
    assert(Validator::is_valid_steiner_tree(g, pruned, false));
    assert(!Validator::has_steiner_leaves(g, pruned));
    assert(Validator::compute_cost(pruned) == 3);
    std::cout << "[PASS] test_mst_and_pruning\n";
}

void test_optimizer_scaled_weights() {
    // Prueba con pesos grandes (escala x25: pesos 25, 50, 75) para verificar generalización > 4
    Graph g = build_synthetic_graph(25);
    OptimizerConfig cfg;
    cfg.max_iterations = 20;
    cfg.max_time_ms = 2000;
    cfg.verbose = false;

    OptimizationStats stats;
    auto sol = Optimizer::optimize(g, cfg, &stats);
    assert(Validator::is_valid_steiner_tree(g, sol, false));
    assert(!Validator::has_steiner_leaves(g, sol));
    assert(Validator::compute_cost(sol) == 75);
    std::cout << "[PASS] test_optimizer_scaled_weights\n";
}

void test_e18_instance() {
    Graph g;
    assert(g.load_from_stp("data/e18.stp"));
    assert(g.num_nodes == 2500);
    assert(g.num_edges == 62500);
    assert(g.num_terminals == 417);

    auto mst = MSTSolver::compute_mst(g);
    assert(Validator::compute_cost(mst) == 2515);
    assert(Validator::is_valid_steiner_tree(g, mst, false));

    auto prim_prio = MSTSolver::compute_mst_prim(g, -1, true);
    auto prim_pruned = MSTSolver::prune_steiner_leaves(g, prim_prio);
    assert(Validator::compute_cost(prim_pruned) == 779);
    assert(Validator::is_valid_steiner_tree(g, prim_pruned, false));
    assert(!Validator::has_steiner_leaves(g, prim_pruned));
    std::cout << "[PASS] test_e18_instance\n";
}

} // namespace

int main() {
    std::cout << "Running Steiner Tree Solver test suite...\n";
    test_dsu();
    test_validator_soundness();
    test_mst_and_pruning();
    test_optimizer_scaled_weights();
    test_e18_instance();
    std::cout << "All tests passed successfully!\n";
    return 0;
}
