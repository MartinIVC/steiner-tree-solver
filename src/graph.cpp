#include "graph.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

Graph::Graph() : num_nodes(0), num_edges(0), num_terminals(0) {}

void Graph::add_edge(int u, int v, int weight) {
    edge_list.push_back({u, v, weight});
    adj[u].push_back({v, weight});
    adj[v].push_back({u, weight});
}

bool Graph::load_from_stp(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error: No se pudo abrir el archivo " << filepath << "\n";
        return false;
    }

    std::string line;
    std::string section;

    while (std::getline(file, line)) {
        // Limpiar retorno de carro \r (compatibilidad Windows / Linux)
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string tag;
        ss >> tag;

        if (tag == "SECTION") {
            ss >> section;
        } else if (tag == "END") {
            section = "";
        } else if (section == "Graph") {
            if (tag == "Nodes") {
                ss >> num_nodes;
                adj.assign(num_nodes + 1, std::vector<Neighbor>());
                is_terminal.assign(num_nodes + 1, false);
            } else if (tag == "Edges") {
                ss >> num_edges;
                edge_list.reserve(num_edges);
            } else if (tag == "E") {
                int u, v, weight;
                if (ss >> u >> v >> weight) {
                    add_edge(u, v, weight);
                }
            }
        } else if (section == "Terminals") {
            if (tag == "Terminals") {
                ss >> num_terminals;
                terminals.reserve(num_terminals);
            } else if (tag == "T") {
                int t;
                if (ss >> t) {
                    terminals.push_back(t);
                    if (t <= num_nodes) {
                        is_terminal[t] = true;
                    }
                }
            }
        }
    }

    file.close();
    return true;
}

void Graph::print_summary() const {
    std::cout << "=== Resumen del Grafo ===\n";
    std::cout << "Nodos (|V|):       " << num_nodes << "\n";
    std::cout << "Aristas (|E|):     " << edge_list.size() << "\n";
    std::cout << "Terminales (|T|):  " << terminals.size() << "\n";
    std::cout << "=========================\n";
}
