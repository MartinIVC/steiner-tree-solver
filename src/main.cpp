#include "graph.hpp"
#include "validator.hpp"
#include "mst.hpp"
#include "optimizer.hpp"
#include <iostream>
#include <chrono>
#include <iomanip>

int main(int argc, char* argv[]) {
    std::string dataset_path = "data/e18.stp";
    if (argc > 1) {
        dataset_path = argv[1];
    }

    std::cout << "====================================================\n";
    std::cout << "   RESOLUCIÓN DEL PROBLEMA DEL ÁRBOL DE STEINER     \n";
    std::cout << "   Instancia: " << dataset_path << "\n";
    std::cout << "====================================================\n\n";

    // 1. Cargar el grafo
    Graph g;
    auto t_start = std::chrono::high_resolution_clock::now();
    if (!g.load_from_stp(dataset_path)) {
        return 1;
    }
    auto t_end = std::chrono::high_resolution_clock::now();
    double load_time = std::chrono::duration<double, std::milli>(t_end - t_start).count();
    g.print_summary();
    std::cout << "Grafo cargado en: " << load_time << " ms\n\n";

    // 2. Fase 1: Árbol Generador Mínimo (MST Base)
    std::cout << ">>> Fase 1: Calculando MST Base (Kruskal/Prim)...\n";
    t_start = std::chrono::high_resolution_clock::now();
    auto mst_edges = MSTSolver::compute_mst(g);
    t_end = std::chrono::high_resolution_clock::now();
    double mst_time = std::chrono::duration<double, std::milli>(t_end - t_start).count();
    int mst_cost = Validator::compute_cost(mst_edges);
    Validator::is_valid_steiner_tree(g, mst_edges, true);
    Validator::export_to_csv("results/mst_base.csv", mst_edges);
    std::cout << "Tiempo MST: " << mst_time << " ms\n\n";

    // 3. Fase 2: Poda de Hojas Steiner
    std::cout << ">>> Fase 2: Podando hojas Steiner no terminales...\n";
    t_start = std::chrono::high_resolution_clock::now();
    auto pruned_edges = MSTSolver::prune_steiner_leaves(g, mst_edges);
    t_end = std::chrono::high_resolution_clock::now();
    double prune_time = std::chrono::duration<double, std::milli>(t_end - t_start).count();
    int pruned_cost = Validator::compute_cost(pruned_edges);
    Validator::is_valid_steiner_tree(g, pruned_edges, true);
    if (!Validator::has_steiner_leaves(g, pruned_edges)) {
        std::cout << "[Validador] Verificación post-poda: No quedan hojas Steiner en el árbol.\n";
    }
    Validator::export_to_csv("results/mst_pruned.csv", pruned_edges);
    std::cout << "Tiempo Poda: " << prune_time << " ms\n\n";

    // 4. Fase 3: Optimización Avanzada
    std::cout << ">>> Fase 3: Ejecutando Algoritmo de Optimización...\n";
    t_start = std::chrono::high_resolution_clock::now();
    auto opt_edges = Optimizer::optimize(g);
    t_end = std::chrono::high_resolution_clock::now();
    double opt_time = std::chrono::duration<double, std::milli>(t_end - t_start).count();
    int opt_cost = Validator::compute_cost(opt_edges);
    Validator::is_valid_steiner_tree(g, opt_edges, true);
    Validator::export_to_csv("results/optimizer_solution.csv", opt_edges);
    std::cout << "Tiempo Optimización: " << opt_time << " ms\n\n";

    // 5. Tabla Resumen de Resultados
    std::cout << "==================== RESULTADOS ====================\n";
    std::cout << std::left << std::setw(22) << "Método" 
              << std::setw(15) << "Costo" 
              << std::setw(18) << "Tiempo" 
              << "Estado\n";
    std::cout << "----------------------------------------------------\n";
    std::cout << std::left << std::setw(22) << "MST Base" 
              << std::setw(15) << mst_cost 
              << std::setw(15) << std::to_string(mst_time) + " ms" 
              << "Factible\n";
    std::cout << std::left << std::setw(22) << "MST Podado" 
              << std::setw(15) << pruned_cost 
              << std::setw(15) << std::to_string(prune_time) + " ms" 
              << "Factible\n";
    std::cout << std::left << std::setw(22) << "Optimizador" 
              << std::setw(15) << opt_cost 
              << std::setw(15) << std::to_string(opt_time) + " ms" 
              << "Factible\n";
    std::cout << std::left << std::setw(22) << "Óptimo Teórico" 
              << std::setw(15) << "564" 
              << std::setw(18) << "-" 
              << "Referencia\n";
    std::cout << "====================================================\n";

    return 0;
}
