#include "graph.hpp"
#include "validator.hpp"
#include "mst.hpp"
#include "optimizer.hpp"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {

bool parse_positive_int(const char* str, int& out_val) {
    try {
        std::string s(str);
        size_t pos = 0;
        int val = std::stoi(s, &pos);
        if (pos != s.size() || val < 0) return false;
        out_val = val;
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

int main(int argc, char* argv[]) {
    std::string dataset_path = "data/e18.stp";
    OptimizerConfig opt_config;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "Uso: " << argv[0] << " [ruta_archivo.stp] [opciones]\n"
                      << "Opciones:\n"
                      << "  --iter <N>          Número máximo de iteraciones para ILS (default: 120)\n"
                      << "  --time-limit <ms>   Tiempo límite en milisegundos para optimización (default: 60000)\n"
                      << "  --seed <N>          Semilla PRNG para reproducibilidad (default: 1337)\n"
                      << "  --optimum <N>       Costo óptimo teórico conocido para cálculo de gap (default: auto)\n"
                      << "  --quiet             Desactivar mensajes informativos del optimizador\n"
                      << "  --verbose           Activar mensajes informativos del optimizador (default)\n"
                      << "  --help, -h          Muestra este mensaje de ayuda\n";
            return 0;
        } else if (arg == "--iter") {
            int val = 0;
            if (i + 1 < argc && parse_positive_int(argv[++i], val)) {
                opt_config.max_iterations = val;
            } else {
                std::cerr << "Error: --iter requiere un entero no negativo válido.\n";
                return 1;
            }
        } else if (arg == "--time-limit") {
            int val = 0;
            if (i + 1 < argc && parse_positive_int(argv[++i], val) && val > 0) {
                opt_config.max_time_ms = val;
            } else {
                std::cerr << "Error: --time-limit requiere un entero positivo en ms.\n";
                return 1;
            }
        } else if (arg == "--seed") {
            int val = 0;
            if (i + 1 < argc && parse_positive_int(argv[++i], val)) {
                opt_config.seed = static_cast<unsigned int>(val);
            } else {
                std::cerr << "Error: --seed requiere un entero no negativo válido.\n";
                return 1;
            }
        } else if (arg == "--optimum") {
            int val = 0;
            if (i + 1 < argc && parse_positive_int(argv[++i], val) && val > 0) {
                opt_config.known_optimum = val;
            } else {
                std::cerr << "Error: --optimum requiere un entero positivo válido.\n";
                return 1;
            }
        } else if (arg == "--quiet") {
            opt_config.verbose = false;
        } else if (arg == "--verbose") {
            opt_config.verbose = true;
        } else if (arg.rfind("--", 0) == 0) {
            std::cerr << "Opción desconocida: " << arg << ". Use --help para más información.\n";
            return 1;
        } else {
            dataset_path = arg;
        }
    }

    if (opt_config.known_optimum == 0 && dataset_path.find("e18.stp") != std::string::npos) {
        opt_config.known_optimum = 564;
    }

    std::cout << "====================================================\n";
    std::cout << "   STEINER TREE PROBLEM IN GRAPHS (STPG) SOLVER     \n";
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

    // 2. Fase 1: Árbol Generador Mínimo (MST Base - Kruskal)
    std::cout << ">>> Fase 1: Calculando MST Base con Algoritmo de Kruskal...\n";
    t_start = std::chrono::high_resolution_clock::now();
    auto mst_edges = MSTSolver::compute_mst(g);
    t_end = std::chrono::high_resolution_clock::now();
    double mst_time = std::chrono::duration<double, std::milli>(t_end - t_start).count();
    int mst_cost = Validator::compute_cost(mst_edges);
    Validator::is_valid_steiner_tree(g, mst_edges, true);
    Validator::export_to_csv("results/mst_base.csv", mst_edges);
    std::cout << "Tiempo Kruskal MST: " << mst_time << " ms\n\n";

    // 3. Fase 2: Poda de Hojas Steiner sobre Kruskal
    std::cout << ">>> Fase 2: Podando hojas Steiner no terminales (Kruskal)...\n";
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
    std::cout << "Tiempo Poda Kruskal: " << prune_time << " ms\n\n";

    // 3b. Fase 1b y 2b: Algoritmo de Prim (Estándar y con Desempate a Terminales)
    std::cout << ">>> Fase 1b: Calculando MST Base con Algoritmo de Prim (Estándar)...\n";
    t_start = std::chrono::high_resolution_clock::now();
    auto prim_std_edges = MSTSolver::compute_mst_prim(g, -1, false);
    t_end = std::chrono::high_resolution_clock::now();
    double prim_std_time = std::chrono::duration<double, std::milli>(t_end - t_start).count();
    Validator::compute_cost(prim_std_edges);
    Validator::is_valid_steiner_tree(g, prim_std_edges, true);
    std::cout << "Tiempo Prim Estándar MST: " << prim_std_time << " ms\n\n";

    std::cout << ">>> Fase 2b-1: Podando hojas Steiner sobre Prim Estándar...\n";
    t_start = std::chrono::high_resolution_clock::now();
    auto prim_std_pruned_edges = MSTSolver::prune_steiner_leaves(g, prim_std_edges);
    t_end = std::chrono::high_resolution_clock::now();
    double prim_std_prune_time = std::chrono::duration<double, std::milli>(t_end - t_start).count();
    int prim_std_pruned_cost = Validator::compute_cost(prim_std_pruned_edges);
    Validator::is_valid_steiner_tree(g, prim_std_pruned_edges, true);
    std::cout << "Tiempo Poda Prim Estándar: " << prim_std_prune_time << " ms\n\n";

    std::cout << ">>> Fase 1c: Calculando MST Base con Prim (Desempate a Terminales)...\n";
    t_start = std::chrono::high_resolution_clock::now();
    auto prim_edges = MSTSolver::compute_mst_prim(g, -1, true);
    t_end = std::chrono::high_resolution_clock::now();
    double prim_time = std::chrono::duration<double, std::milli>(t_end - t_start).count();
    int prim_cost = Validator::compute_cost(prim_edges);
    Validator::is_valid_steiner_tree(g, prim_edges, true);
    Validator::export_to_csv("results/mst_prim.csv", prim_edges);
    std::cout << "Tiempo Prim Priorizado MST: " << prim_time << " ms\n\n";

    std::cout << ">>> Fase 2b-2: Podando hojas Steiner sobre Prim Priorizado...\n";
    t_start = std::chrono::high_resolution_clock::now();
    auto prim_pruned_edges = MSTSolver::prune_steiner_leaves(g, prim_edges);
    t_end = std::chrono::high_resolution_clock::now();
    double prim_prune_time = std::chrono::duration<double, std::milli>(t_end - t_start).count();
    int prim_pruned_cost = Validator::compute_cost(prim_pruned_edges);
    Validator::is_valid_steiner_tree(g, prim_pruned_edges, true);
    if (!Validator::has_steiner_leaves(g, prim_pruned_edges)) {
        std::cout << "[Validador] Verificación post-poda Prim: No quedan hojas Steiner en el árbol.\n";
    }
    Validator::export_to_csv("results/mst_prim_pruned.csv", prim_pruned_edges);
    std::cout << "Tiempo Poda Prim Priorizado: " << prim_prune_time << " ms\n\n";

    // 4. Fase 3: Optimización Avanzada
    std::cout << ">>> Fase 3: Ejecutando Algoritmo de Optimización...\n";
    OptimizationStats stats;
    t_start = std::chrono::high_resolution_clock::now();
    auto opt_edges = Optimizer::optimize(g, opt_config, &stats);
    t_end = std::chrono::high_resolution_clock::now();
    double opt_time = std::chrono::duration<double, std::milli>(t_end - t_start).count();
    int opt_cost = Validator::compute_cost(opt_edges);
    bool opt_valid = Validator::is_valid_steiner_tree(g, opt_edges, true);
    Validator::export_to_csv("results/optimizer_solution.csv", opt_edges);
    std::cout << "Tiempo Optimización: " << opt_time << " ms\n\n";

    // 5. Tabla Resumen de Métricas
    auto format_reduction = [&](int cost) -> std::string {
        if (mst_cost <= 0 || cost == mst_cost) return "0.00% (Base)";
        double red = ((double)(mst_cost - cost) / (double)mst_cost) * 100.0;
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2) << "-" << red << "%";
        return ss.str();
    };

    auto format_gap = [&](int cost) -> std::string {
        int ref = (opt_config.known_optimum > 0) ? opt_config.known_optimum : stats.dual_lower_bound;
        if (ref <= 0) return "-";
        double gap = ((double)(cost - ref) / (double)ref) * 100.0;
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2) << (gap >= 0.0 ? "+" : "") << gap << "%";
        return ss.str();
    };

    auto format_time = [](double ms) -> std::string {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2) << ms << " ms";
        return ss.str();
    };

    std::cout << "============================================ RESULTADOS ============================================\n";
    std::cout << std::left 
              << std::setw(24) << "Fase / Método" 
              << std::setw(10) << "Costo" 
              << std::setw(16) << "Reducción %"
              << std::setw(14) << (opt_config.known_optimum > 0 ? "Gap vs Ópt." : "Gap vs LB")
              << std::setw(12) << "Aristas"
              << std::setw(14) << "Tiempo" 
              << "Estado\n";
    std::cout << "----------------------------------------------------------------------------------------------------\n";
    std::cout << std::left 
              << std::setw(24) << "Kruskal MST Base" 
              << std::setw(10) << mst_cost 
              << std::setw(16) << format_reduction(mst_cost)
              << std::setw(14) << format_gap(mst_cost)
              << std::setw(12) << mst_edges.size()
              << std::setw(14) << format_time(mst_time)
              << "Factible (MST Global)\n";
    std::cout << std::left 
              << std::setw(24) << "Kruskal Podado" 
              << std::setw(10) << pruned_cost 
              << std::setw(16) << format_reduction(pruned_cost)
              << std::setw(14) << format_gap(pruned_cost)
              << std::setw(12) << pruned_edges.size()
              << std::setw(14) << format_time(prune_time)
              << "Factible (Sin hojas Steiner)\n";
    std::cout << std::left 
              << std::setw(24) << "Prim MST Base" 
              << std::setw(10) << prim_cost 
              << std::setw(16) << format_reduction(prim_cost)
              << std::setw(14) << format_gap(prim_cost)
              << std::setw(12) << prim_edges.size()
              << std::setw(14) << format_time(prim_time)
              << "Factible (MST Global)\n";
    std::cout << std::left 
              << std::setw(24) << "Prim Podado (Std)" 
              << std::setw(10) << prim_std_pruned_cost 
              << std::setw(16) << format_reduction(prim_std_pruned_cost)
              << std::setw(14) << format_gap(prim_std_pruned_cost)
              << std::setw(12) << prim_std_pruned_edges.size()
              << std::setw(14) << format_time(prim_std_prune_time)
              << "Factible (Sin hojas Steiner)\n";
    std::cout << std::left 
              << std::setw(24) << "Prim Podado (Prio)" 
              << std::setw(10) << prim_pruned_cost 
              << std::setw(16) << format_reduction(prim_pruned_cost)
              << std::setw(14) << format_gap(prim_pruned_cost)
              << std::setw(12) << prim_pruned_edges.size()
              << std::setw(14) << format_time(prim_prune_time)
              << "Factible (Afinidad Terminal)\n";
    std::cout << std::left 
              << std::setw(24) << "Voronoi KMB (Mehlhorn)" 
              << std::setw(10) << stats.kmb_cost 
              << std::setw(16) << format_reduction(stats.kmb_cost)
              << std::setw(14) << format_gap(stats.kmb_cost)
              << std::setw(12) << "-"
              << std::setw(14) << "-"
              << "Constructiva Fase 1a\n";
    std::cout << std::left 
              << std::setw(24) << "Multi-Start SPH" 
              << std::setw(10) << stats.multi_sph_cost 
              << std::setw(16) << format_reduction(stats.multi_sph_cost)
              << std::setw(14) << format_gap(stats.multi_sph_cost)
              << std::setw(12) << "-"
              << std::setw(14) << format_time(stats.constructive_ms)
              << "Constructiva Fase 1b\n";
    std::cout << std::left 
              << std::setw(24) << "VND (N1+N2+N3)" 
              << std::setw(10) << stats.vnd_cost 
              << std::setw(16) << format_reduction(stats.vnd_cost)
              << std::setw(14) << format_gap(stats.vnd_cost)
              << std::setw(12) << "-"
              << std::setw(14) << format_time(stats.vnd_ms)
              << "Búsqueda Local Fase 2-3\n";
    std::cout << std::left 
              << std::setw(24) << "Optimizador (PR + ILS)" 
              << std::setw(10) << opt_cost 
              << std::setw(16) << format_reduction(opt_cost)
              << std::setw(14) << format_gap(opt_cost)
              << std::setw(12) << opt_edges.size()
              << std::setw(14) << format_time(opt_time)
              << (opt_valid ? "Factible Certificado" : "Inválido") << "\n";
    if (stats.dual_lower_bound > 0) {
        std::cout << std::left 
                  << std::setw(24) << "Wong Dual Ascent LB" 
                  << std::setw(10) << stats.dual_lower_bound 
                  << std::setw(16) << "-"
                  << std::setw(14) << "-"
                  << std::setw(12) << "-"
                  << std::setw(14) << format_time(stats.presolve_ms) 
                  << "Cota Inferior Dual\n";
    }
    if (opt_config.known_optimum > 0) {
        std::cout << std::left 
                  << std::setw(24) << "Óptimo Teórico" 
                  << std::setw(10) << opt_config.known_optimum 
                  << std::setw(16) << format_reduction(opt_config.known_optimum)
                  << std::setw(14) << "0.00%"
                  << std::setw(12) << "-"
                  << std::setw(14) << "-" 
                  << "Referencia SteinLib\n";
    }
    std::cout << "====================================================================================================\n";

    return opt_valid ? 0 : 1;
}
