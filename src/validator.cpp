#include "validator.hpp"
#include <iostream>
#include <queue>
#include <unordered_set>

int Validator::compute_cost(const std::vector<Edge>& solution) {
    int total = 0;
    for (const auto& e : solution) {
        total += e.weight;
    }
    return total;
}

bool Validator::covers_all_terminals(const Graph& g, const std::vector<Edge>& solution) {
    std::unordered_set<int> visited_nodes;
    for (const auto& e : solution) {
        visited_nodes.insert(e.u);
        visited_nodes.insert(e.v);
    }

    for (int t : g.terminals) {
        if (visited_nodes.find(t) == visited_nodes.end()) {
            return false;
        }
    }
    return true;
}

bool Validator::is_valid_steiner_tree(const Graph& g, const std::vector<Edge>& solution, bool verbose) {
    if (solution.empty()) {
        if (verbose) std::cout << "[Validador] Error: La solución está vacía.\n";
        return false;
    }

    // 1. Verificar cobertura de todos los terminales
    if (!covers_all_terminals(g, solution)) {
        if (verbose) std::cout << "[Validador] Error: No todos los terminales están presentes en la solución.\n";
        return false;
    }

    // 2. Construir lista de adyacencia del subgrafo inducido por la solución
    std::vector<std::vector<int>> sub_adj(g.num_nodes + 1);
    std::unordered_set<int> present_nodes;
    for (const auto& e : solution) {
        sub_adj[e.u].push_back(e.v);
        sub_adj[e.v].push_back(e.u);
        present_nodes.insert(e.u);
        present_nodes.insert(e.v);
    }

    // 3. Verificar conexidad usando BFS desde el primer terminal
    int start_node = g.terminals.front();
    std::vector<bool> visited(g.num_nodes + 1, false);
    std::queue<int> q;

    visited[start_node] = true;
    q.push(start_node);
    int reached_count = 0;

    while (!q.empty()) {
        int curr = q.front();
        q.pop();
        reached_count++;

        for (int neighbor : sub_adj[curr]) {
            if (!visited[neighbor]) {
                visited[neighbor] = true;
                q.push(neighbor);
            }
        }
    }

    // Verificar que todos los nodos de la solución se alcanzaron desde el start_node
    if (reached_count != (int)present_nodes.size()) {
        if (verbose) std::cout << "[Validador] Error: El subgrafo no es conexo (nodos aislados o múltiples componentes).\n";
        return false;
    }

    if (verbose) {
        std::cout << "[Validador] OK: Solución factible. Costo total: " 
                  << compute_cost(solution) << " | Aristas: " << solution.size() 
                  << " | Nodos: " << present_nodes.size() << "\n";
    }

    return true;
}

bool Validator::is_tree(int num_nodes, const std::vector<Edge>& solution) {
    // Para un grafo conexo, es árbol sii |E| == |V| - 1
    std::unordered_set<int> present_nodes;
    for (const auto& e : solution) {
        present_nodes.insert(e.u);
        present_nodes.insert(e.v);
    }
    return (int)solution.size() == ((int)present_nodes.size() - 1);
}
