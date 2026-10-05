#pragma once

#include "graph.hpp"
#include <vector>
#include <string>

class Validator {
public:
    // Calcula la suma total de los pesos de las aristas en la solución
    static int compute_cost(const std::vector<Edge>& solution);

    // Verifica que el conjunto de aristas forme un subgrafo conexo que cubra todos los terminales y sea un árbol acíclico
    static bool is_valid_steiner_tree(const Graph& g, const std::vector<Edge>& solution, bool verbose = true);

    // Verifica si todos los nodos terminales están presentes en la solución
    static bool covers_all_terminals(const Graph& g, const std::vector<Edge>& solution);

    // Verifica si el subgrafo es acíclico y forma un árbol (|E| == |V| - 1 y sin ciclos)
    static bool is_tree(int num_nodes, const std::vector<Edge>& solution);
    static bool is_tree(const std::vector<Edge>& solution);

    // Comprobación explícita de aciclicidad (detecta si existen ciclos en el conjunto de aristas mediante DSU)
    static bool has_cycle(const std::vector<Edge>& solution);

    // Verifica empíricamente si existen hojas (grado 1) que sean nodos Steiner (no terminales)
    static bool has_steiner_leaves(const Graph& g, const std::vector<Edge>& solution);

    // Exporta el conjunto de aristas de la solución a un archivo CSV estructurado con cabecera u,v,weight
    static bool export_to_csv(const std::string& filepath, const std::vector<Edge>& solution);
};
