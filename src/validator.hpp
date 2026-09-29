#pragma once

#include "graph.hpp"
#include <vector>

class Validator {
public:
    // Calcula la suma total de los pesos de las aristas en la solución
    static int compute_cost(const std::vector<Edge>& solution);

    // Verifica que el conjunto de aristas forme un subgrafo conexo que cubra todos los terminales
    static bool is_valid_steiner_tree(const Graph& g, const std::vector<Edge>& solution, bool verbose = true);

    // Verifica si todos los nodos terminales están presentes en la solución
    static bool covers_all_terminals(const Graph& g, const std::vector<Edge>& solution);

    // Verifica si el subgrafo es acíclico (es un árbol)
    static bool is_tree(int num_nodes, const std::vector<Edge>& solution);
};
