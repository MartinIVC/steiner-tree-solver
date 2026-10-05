#include "validator.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <queue>
#include <unordered_set>
#include <vector>

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

bool Validator::has_cycle(const std::vector<Edge>& solution) {
    if (solution.empty()) {
        return false;
    }

    int max_node = 0;
    for (const auto& e : solution) {
        if (e.u <= 0 || e.v <= 0) return true; // Nodos con índice inválido
        if (e.u > max_node) max_node = e.u;
        if (e.v > max_node) max_node = e.v;
    }

    std::vector<int> parent(max_node + 1);
    std::vector<int> rank(max_node + 1, 0);
    for (int i = 0; i <= max_node; ++i) {
        parent[i] = i;
    }

    auto find = [&](auto& self, int i) -> int {
        if (parent[i] == i) return i;
        return parent[i] = self(self, parent[i]);
    };

    for (const auto& e : solution) {
        if (e.u == e.v) {
            // Auto-bucle (ciclo de longitud 1)
            return true;
        }

        int root_u = find(find, e.u);
        int root_v = find(find, e.v);

        if (root_u == root_v) {
            // Se encontró un ciclo: ambos nodos ya están en la misma componente conexa
            return true;
        }

        // Unión por rango
        if (rank[root_u] < rank[root_v]) {
            parent[root_u] = root_v;
        } else if (rank[root_u] > rank[root_v]) {
            parent[root_v] = root_u;
        } else {
            parent[root_v] = root_u;
            rank[root_u]++;
        }
    }

    return false; // No se detectaron ciclos (es acíclico)
}

bool Validator::is_tree(const std::vector<Edge>& solution) {
    if (solution.empty()) {
        return false;
    }

    std::unordered_set<int> present_nodes;
    for (const auto& e : solution) {
        present_nodes.insert(e.u);
        present_nodes.insert(e.v);
    }

    // Condición necesaria para que un subgrafo conexo sea un árbol: |E| == |V| - 1
    if ((int)solution.size() != (int)present_nodes.size() - 1) {
        return false;
    }

    // Comprobación explícita de aciclicidad (descartar cualquier ciclo)
    if (has_cycle(solution)) {
        return false;
    }

    return true;
}

bool Validator::is_tree(int num_nodes, const std::vector<Edge>& solution) {
    (void)num_nodes;
    return is_tree(solution);
}

bool Validator::has_steiner_leaves(const Graph& g, const std::vector<Edge>& solution) {
    if (solution.empty()) {
        return false;
    }

    std::vector<int> degree(g.num_nodes + 1, 0);
    for (const auto& e : solution) {
        if (e.u >= 1 && e.u <= g.num_nodes) degree[e.u]++;
        if (e.v >= 1 && e.v <= g.num_nodes) degree[e.v]++;
    }

    for (int i = 1; i <= g.num_nodes; ++i) {
        if (degree[i] == 1 && !g.is_terminal[i]) {
            return true; // Existe al menos una hoja que no es terminal (hoja Steiner)
        }
    }

    return false; // No quedan hojas Steiner
}

bool Validator::is_valid_steiner_tree(const Graph& g, const std::vector<Edge>& solution, bool verbose) {
    if (solution.empty()) {
        if (verbose) std::cout << "[Validador] Error: La solución está vacía.\n";
        return false;
    }

    // 1. Verificar cobertura de todos los terminales obligatorios
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

    // Verificar que todos los nodos de la solución se alcanzaron desde start_node (una única componente conexa)
    if (reached_count != (int)present_nodes.size()) {
        if (verbose) std::cout << "[Validador] Error: El subgrafo no es conexo (nodos aislados o múltiples componentes).\n";
        return false;
    }

    // 4. Comprobación explícita de aciclicidad y estructura de árbol (|E| == |V| - 1 y sin ciclos)
    if (!is_tree(solution)) {
        if (verbose) {
            if (has_cycle(solution)) {
                std::cout << "[Validador] Error: La solución contiene ciclos (no es acíclica).\n";
            } else {
                std::cout << "[Validador] Error: La solución no cumple la propiedad de árbol (|E| != |V| - 1).\n";
            }
        }
        return false;
    }

    if (verbose) {
        std::cout << "[Validador] OK: Solución factible. Costo total: " 
                  << compute_cost(solution) << " | Aristas: " << solution.size() 
                  << " | Nodos: " << present_nodes.size() << "\n";
    }

    return true;
}

bool Validator::export_to_csv(const std::string& filepath, const std::vector<Edge>& solution) {
    try {
        std::filesystem::path p(filepath);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
    } catch (...) {
        // Permitir que ofstream gestione la creación si filesystem falla
    }

    std::ofstream out(filepath);
    if (!out.is_open()) {
        std::cerr << "[Validador] Error: No se pudo abrir el archivo para exportar: " << filepath << "\n";
        return false;
    }

    out << "u,v,weight\n";
    for (const auto& e : solution) {
        out << e.u << "," << e.v << "," << e.weight << "\n";
    }

    out.close();
    return true;
}
