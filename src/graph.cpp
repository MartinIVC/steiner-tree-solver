#include "graph.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <queue>
#include <algorithm>

Graph::Graph() : num_nodes(0), num_edges(0), num_terminals(0) {}

void Graph::clear() {
    num_nodes = 0;
    num_edges = 0;
    num_terminals = 0;
    edge_list.clear();
    edge_list.shrink_to_fit();
    adj.clear();
    adj.shrink_to_fit();
    terminals.clear();
    terminals.shrink_to_fit();
    is_terminal.clear();
    is_terminal.shrink_to_fit();
}

void Graph::add_edge(int u, int v, int weight) {
    if (!is_valid_node(u) || !is_valid_node(v)) {
        return;
    }
    edge_list.push_back({u, v, weight});
    adj[u].push_back({v, weight});
    adj[v].push_back({u, weight});
}

bool Graph::load_from_stp(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[Grafo] Error crítico: No se pudo abrir el archivo '" << filepath 
                  << "'. Verifique que la ruta sea correcta y existan permisos de lectura.\n";
        return false;
    }

    // Verificación de archivo vacío
    if (file.peek() == std::ifstream::traits_type::eof()) {
        std::cerr << "[Grafo] Error crítico: El archivo '" << filepath 
                  << "' está vacío (0 bytes).\n";
        file.close();
        return false;
    }

    clear();

    std::string line;
    std::string section = "";
    bool has_graph_section = false;
    bool has_terminals_section = false;
    bool nodes_declared = false;

    while (std::getline(file, line)) {
        // Limpiar retorno de carro \r (compatibilidad Windows / Linux)
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string tag;
        if (!(ss >> tag)) continue;

        if (tag == "SECTION") {
            ss >> section;
            if (section == "Graph") has_graph_section = true;
            else if (section == "Terminals") has_terminals_section = true;
        } else if (tag == "END") {
            section = "";
        } else if (tag == "EOF") {
            break;
        } else if (section == "Graph") {
            if (tag == "Nodes") {
                if (ss >> num_nodes) {
                    if (num_nodes <= 0) {
                        std::cerr << "[Grafo] Error crítico: Cantidad de nodos inválida (" << num_nodes << ").\n";
                        file.close();
                        return false;
                    }
                    adj.assign(num_nodes + 1, std::vector<Neighbor>());
                    is_terminal.assign(num_nodes + 1, false);
                    nodes_declared = true;
                }
            } else if (tag == "Edges") {
                if (ss >> num_edges) {
                    if (num_edges > 0) {
                        edge_list.reserve(num_edges);
                    }
                }
            } else if (tag == "E") {
                int u, v, weight;
                if (ss >> u >> v >> weight) {
                    if (!nodes_declared || u < 1 || u > num_nodes || v < 1 || v > num_nodes) {
                        std::cerr << "[Grafo] Advertencia: Arista ignorada (" << u << ", " << v 
                                  << "). Nodos fuera de rango [1, " << num_nodes << "].\n";
                        continue;
                    }
                    if (weight < 0) {
                        std::cerr << "[Grafo] Advertencia: Arista (" << u << ", " << v 
                                  << ") con peso negativo (" << weight << ") ajustada a 0.\n";
                        weight = 0;
                    }
                    add_edge(u, v, weight);
                }
            }
        } else if (section == "Terminals") {
            if (tag == "Terminals") {
                if (ss >> num_terminals) {
                    if (num_terminals > 0) {
                        terminals.reserve(num_terminals);
                    }
                }
            } else if (tag == "T") {
                int t;
                if (ss >> t) {
                    if (!nodes_declared || t < 1 || t > num_nodes) {
                        std::cerr << "[Grafo] Advertencia: Terminal " << t 
                                  << " ignorado por estar fuera de rango [1, " << num_nodes << "].\n";
                        continue;
                    }
                    if (!is_terminal[t]) {
                        is_terminal[t] = true;
                        terminals.push_back(t);
                    }
                }
            }
        }
    }

    file.close();

    // Verificaciones de integridad estructural del grafo parseado
    if (!has_graph_section || num_nodes <= 0 || edge_list.empty()) {
        std::cerr << "[Grafo] Error crítico: El archivo '" << filepath 
                  << "' no contiene una sección SECTION Graph válida o faltan nodos/aristas.\n";
        return false;
    }

    if (!has_terminals_section || terminals.empty()) {
        std::cerr << "[Grafo] Error crítico: El archivo '" << filepath 
                  << "' no contiene una sección SECTION Terminals válida o faltan terminales.\n";
        return false;
    }

    if (num_edges > 0 && (int)edge_list.size() != num_edges) {
        std::cerr << "[Grafo] Advertencia: Se declararon " << num_edges 
                  << " aristas, pero se leyeron " << edge_list.size() << ".\n";
        num_edges = (int)edge_list.size();
    }

    if (num_terminals > 0 && (int)terminals.size() != num_terminals) {
        std::cerr << "[Grafo] Advertencia: Se declararon " << num_terminals 
                  << " terminales, pero se registraron " << terminals.size() << " únicos.\n";
        num_terminals = (int)terminals.size();
    }

    return true;
}

void Graph::print_summary() const {
    std::cout << "=== Resumen del Grafo ===\n";
    std::cout << "Nodos (|V|):         " << num_nodes << "\n";
    std::cout << "Aristas (|E|):       " << edge_list.size() << "\n";
    std::cout << "Terminales (|T|):    " << terminals.size() << "\n";
    std::cout << "Nodos Steiner (|S|): " << (num_nodes - (int)terminals.size()) << "\n";
    std::cout << "Peso total aristas:  " << get_total_weight() << "\n";
    std::cout << "=========================\n";
}

std::vector<int> Graph::get_steiner_nodes() const {
    std::vector<int> steiner_nodes;
    if (num_nodes > 0) {
        int expected_count = num_nodes - (int)terminals.size();
        if (expected_count > 0) {
            steiner_nodes.reserve(expected_count);
        }
        for (int i = 1; i <= num_nodes; ++i) {
            if (!is_terminal[i]) {
                steiner_nodes.push_back(i);
            }
        }
    }
    return steiner_nodes;
}

int Graph::get_degree(int u) const {
    if (!is_valid_node(u)) return 0;
    return (int)adj[u].size();
}

const std::vector<Neighbor>& Graph::get_neighbors(int u) const {
    static const std::vector<Neighbor> empty_neighbors;
    if (!is_valid_node(u)) {
        return empty_neighbors;
    }
    return adj[u];
}

bool Graph::has_edge(int u, int v) const {
    if (!is_valid_node(u) || !is_valid_node(v)) return false;
    // Búsqueda sobre la lista de adyacencia más corta para mayor rendimiento
    if (adj[u].size() > adj[v].size()) {
        std::swap(u, v);
    }
    for (const auto& nb : adj[u]) {
        if (nb.to == v) return true;
    }
    return false;
}

int Graph::get_edge_weight(int u, int v) const {
    if (!is_valid_node(u) || !is_valid_node(v)) return -1;
    if (adj[u].size() > adj[v].size()) {
        std::swap(u, v);
    }
    int min_w = -1;
    for (const auto& nb : adj[u]) {
        if (nb.to == v) {
            if (min_w == -1 || nb.weight < min_w) {
                min_w = nb.weight;
            }
        }
    }
    return min_w;
}

long long Graph::get_total_weight() const {
    long long total = 0;
    for (const auto& e : edge_list) {
        total += e.weight;
    }
    return total;
}

bool Graph::dijkstra(int start_node, std::vector<int>& dist, std::vector<int>& parent) const {
    if (!is_valid_node(start_node)) {
        std::cerr << "[Grafo] Error en Dijkstra: Nodo inicial " << start_node 
                  << " fuera del rango válido [1, " << num_nodes << "].\n";
        return false;
    }

    dist.assign(num_nodes + 1, INF);
    parent.assign(num_nodes + 1, -1);

    using PII = std::pair<int, int>; // (distancia, nodo)
    std::priority_queue<PII, std::vector<PII>, std::greater<PII>> pq;

    dist[start_node] = 0;
    pq.push({0, start_node});

    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();

        if (d > dist[u]) continue;

        for (const auto& edge : adj[u]) {
            int v = edge.to;
            int weight = edge.weight;
            if (dist[u] + weight < dist[v]) {
                dist[v] = dist[u] + weight;
                parent[v] = u;
                pq.push({dist[v], v});
            }
        }
    }

    return true;
}

std::vector<int> Graph::get_shortest_path_nodes(int start_node, int target_node, const std::vector<int>& parent) const {
    if (!is_valid_node(start_node) || !is_valid_node(target_node)) return {};
    if ((int)parent.size() <= num_nodes) return {};
    if (start_node == target_node) return {start_node};
    if (parent[target_node] == -1) return {};

    std::vector<int> path;
    int curr = target_node;
    while (curr != -1) {
        path.push_back(curr);
        if (curr == start_node) break;
        curr = parent[curr];
    }

    if (path.back() != start_node) {
        return {}; // Nodo inalcanzable
    }

    std::reverse(path.begin(), path.end());
    return path;
}

std::vector<Edge> Graph::get_shortest_path_edges(int start_node, int target_node, const std::vector<int>& parent) const {
    if (!is_valid_node(start_node) || !is_valid_node(target_node)) return {};
    if ((int)parent.size() <= num_nodes) return {};
    if (start_node == target_node) return {};
    if (parent[target_node] == -1) return {};

    std::vector<Edge> path_edges;
    int curr = target_node;
    while (curr != start_node) {
        int p = parent[curr];
        if (p == -1) {
            return {}; // Inalcanzable
        }
        int w = get_edge_weight(p, curr);
        path_edges.push_back({p, curr, w});
        curr = p;
    }

    std::reverse(path_edges.begin(), path_edges.end());
    return path_edges;
}

int Graph::get_shortest_distance(int start_node, int target_node) const {
    if (!is_valid_node(start_node) || !is_valid_node(target_node)) return INF;
    if (start_node == target_node) return 0;

    std::vector<int> dist;
    std::vector<int> parent;
    if (!dijkstra(start_node, dist, parent)) {
        return INF;
    }
    return dist[target_node];
}

TerminalMetricClosure Graph::compute_terminal_metric_closure() const {
    TerminalMetricClosure closure;
    closure.dist.resize(num_terminals);
    closure.parent.resize(num_terminals);
    closure.terminal_index_map.assign(num_nodes + 1, -1);

    for (int i = 0; i < num_terminals; ++i) {
        closure.terminal_index_map[terminals[i]] = i;
    }

    for (int i = 0; i < num_terminals; ++i) {
        int t = terminals[i];
        closure.dist[i].assign(num_nodes + 1, INF);
        closure.parent[i].assign(num_nodes + 1, -1);
        dijkstra(t, closure.dist[i], closure.parent[i]);
    }

    return closure;
}

std::vector<Edge> Graph::get_terminal_metric_edges(const TerminalMetricClosure& closure) const {
    std::vector<Edge> metric_edges;
    metric_edges.reserve((num_terminals * (num_terminals - 1)) / 2);

    for (int i = 0; i < num_terminals; ++i) {
        int u = terminals[i];
        for (int j = i + 1; j < num_terminals; ++j) {
            int v = terminals[j];
            int w = closure.dist[i][v];
            if (w < INF) {
                metric_edges.push_back({u, v, w});
            }
        }
    }
    return metric_edges;
}

std::vector<Edge> Graph::get_metric_path_edges(int terminal_u, int node_v, const TerminalMetricClosure& closure) const {
    if (!is_valid_node(terminal_u) || !is_valid_node(node_v)) return {};
    if (terminal_u < 1 || terminal_u >= (int)closure.terminal_index_map.size()) return {};
    int idx = closure.terminal_index_map[terminal_u];
    if (idx < 0 || idx >= (int)closure.parent.size()) return {};
    return get_shortest_path_edges(terminal_u, node_v, closure.parent[idx]);
}

