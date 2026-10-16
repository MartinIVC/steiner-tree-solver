#pragma once

#include "graph.hpp"
#include <vector>

// Configuración para el módulo de optimización y búsqueda local
struct OptimizerConfig {
    int max_iterations = 1000;
    int max_time_ms = 60000;      // 1 minuto por defecto
    bool verbose = true;
};

// Estadísticas detalladas de la ejecución del optimizador (para reporte y análisis)
struct OptimizationStats {
    int dual_lower_bound = 0;     // Cota inferior obtenida mediante Dual Ascent
    int kmb_cost = 0;             // Costo tras heurística KMB / Voronoi (Mehlhorn)
    int multi_sph_cost = 0;       // Costo tras heurística constructiva Multi-Start SPH
    int vnd_cost = 0;             // Costo tras Búsqueda Local VND (N1 + N2 + N3)
    int final_cost = 0;           // Costo final tras ILS + Path Relinking
    int reduced_edges_count = 0;  // Cantidad de aristas activas tras Presolve
    double presolve_ms = 0.0;
    double constructive_ms = 0.0;
    double vnd_ms = 0.0;
    double total_ms = 0.0;
};

class Optimizer {
public:
    // Heurística constructiva basada en caminos mínimos (Mehlhorn KMB + Multi-Start SPH con afinidad terminal)
    static std::vector<Edge> constructive_heuristic(const Graph& g);

    // Búsqueda local por Descenso de Vecindad Variable (VND: Inserción Estrella, Eliminación Steiner y Reemplazo de Key-Paths)
    static std::vector<Edge> local_search(const Graph& g, const std::vector<Edge>& initial_solution, int max_iterations = 1000);

    // Pipeline completo de optimización (Presolve + Dual Ascent + Constructiva + VND + ILS/Path Relinking)
    static std::vector<Edge> optimize(const Graph& g, const OptimizerConfig& config = OptimizerConfig(), OptimizationStats* out_stats = nullptr);
};
