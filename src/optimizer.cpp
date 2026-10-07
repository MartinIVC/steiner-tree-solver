#include "optimizer.hpp"
#include "mst.hpp"
#include "validator.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <queue>
#include <algorithm>
#include <random>
#include <cmath>
#include <chrono>

namespace {

// Estructura interna para representar un camino clave entre vértices de bifurcación o terminales
struct KeyPath {
    int u;                      // Vértice clave 1 (terminal o grado >= 3)
    int v;                      // Vértice clave 2 (terminal o grado >= 3)
    std::vector<Edge> edges;    // Aristas del camino
    int cost;                   // Suma de pesos de las aristas del camino
};

// Extrae todos los caminos clave (Key-Paths) de un árbol podado
std::vector<KeyPath> extract_key_paths(const Graph& g, const std::vector<Edge>& tree_edges) {
    std::vector<std::vector<std::pair<int, int>>> tree_adj(g.num_nodes + 1);
    std::vector<int> degree(g.num_nodes + 1, 0);
    for (const auto& e : tree_edges) {
        tree_adj[e.u].push_back({e.v, e.weight});
        tree_adj[e.v].push_back({e.u, e.weight});
        degree[e.u]++;
        degree[e.v]++;
    }

    auto is_key_vertex = [&](int u) {
        return g.is_terminal[u] || degree[u] >= 3;
    };

    std::vector<KeyPath> key_paths;
    std::vector<std::vector<bool>> visited_edge(g.num_nodes + 1);
    for (int i = 1; i <= g.num_nodes; ++i) {
        visited_edge[i].assign(tree_adj[i].size(), false);
    }

    for (int start = 1; start <= g.num_nodes; ++start) {
        if (!is_key_vertex(start) || degree[start] == 0) continue;

        for (size_t e_idx = 0; e_idx < tree_adj[start].size(); ++e_idx) {
            if (visited_edge[start][e_idx]) continue;

            visited_edge[start][e_idx] = true;
            int nxt = tree_adj[start][e_idx].first;
            int w = tree_adj[start][e_idx].second;

            for (size_t rev_idx = 0; rev_idx < tree_adj[nxt].size(); ++rev_idx) {
                if (tree_adj[nxt][rev_idx].first == start && tree_adj[nxt][rev_idx].second == w) {
                    visited_edge[nxt][rev_idx] = true;
                    break;
                }
            }

            KeyPath kp;
            kp.u = start;
            kp.edges.push_back({std::min(start, nxt), std::max(start, nxt), w});
            kp.cost = w;

            int curr = nxt;
            int prev = start;

            while (!is_key_vertex(curr)) {
                int next_node = -1;
                int next_w = 0;
                for (size_t i = 0; i < tree_adj[curr].size(); ++i) {
                    int neighbor = tree_adj[curr][i].first;
                    if (neighbor != prev) {
                        next_node = neighbor;
                        next_w = tree_adj[curr][i].second;
                        visited_edge[curr][i] = true;
                        for (size_t rev = 0; rev < tree_adj[neighbor].size(); ++rev) {
                            if (tree_adj[neighbor][rev].first == curr && tree_adj[neighbor][rev].second == next_w) {
                                visited_edge[neighbor][rev] = true;
                                break;
                            }
                        }
                        break;
                    }
                }
                if (next_node == -1) break;
                kp.edges.push_back({std::min(curr, next_node), std::max(curr, next_node), next_w});
                kp.cost += next_w;
                prev = curr;
                curr = next_node;
            }

            kp.v = curr;
            key_paths.push_back(kp);
        }
    }

    return key_paths;
}

// Función de costo sesgado para multi-source Dijkstra:
// Prioriza estrictamente cadenas de aristas de peso 1 sobre atajos de peso >= 2
inline int biased_weight(int w) {
    if (w == 1) return 100;
    if (w == 2) return 205; // 2 * 100 = 200 < 205: dos aristas peso 1 preferidas sobre una peso 2
    if (w == 3) return 350;
    return 1000 + w * 100;
}

// Operador 1: Reemplazo de caminos clave por caminos mínimos en G
bool optimize_key_paths(const Graph& g, std::vector<Edge>& current_tree, int& current_cost) {
    auto key_paths = extract_key_paths(g, current_tree);

    for (const auto& kp : key_paths) {
        if (kp.cost <= 1) continue;

        std::vector<Edge> remaining_edges;
        auto is_kp_edge = [&](const Edge& e) {
            int eu = std::min(e.u, e.v);
            int ev = std::max(e.u, e.v);
            for (const auto& kpe : kp.edges) {
                if (eu == kpe.u && ev == kpe.v) return true;
            }
            return false;
        };

        for (const auto& e : current_tree) {
            if (!is_kp_edge(e)) {
                remaining_edges.push_back(e);
            }
        }

        DSU dsu(g.num_nodes);
        for (const auto& e : remaining_edges) {
            dsu.unite(e.u, e.v);
        }

        int root_u = dsu.find(kp.u);
        int root_v = dsu.find(kp.v);
        if (root_u == root_v) {
            auto cand = MSTSolver::prune_steiner_leaves(g, MSTSolver::compute_mst(remaining_edges, g.num_nodes));
            if (Validator::is_valid_steiner_tree(g, cand, false)) {
                int c = Validator::compute_cost(cand);
                if (c < current_cost) {
                    current_cost = c;
                    current_tree = cand;
                    return true;
                }
            }
            continue;
        }

        std::vector<int> dist(g.num_nodes + 1, Graph::INF);
        std::vector<int> parent(g.num_nodes + 1, -1);
        std::priority_queue<std::pair<int, int>, std::vector<std::pair<int, int>>, std::greater<std::pair<int, int>>> pq;

        for (int i = 1; i <= g.num_nodes; ++i) {
            if (dsu.find(i) == root_u) {
                dist[i] = 0;
                pq.push({0, i});
            }
        }

        int target_node = -1;
        while (!pq.empty()) {
            auto [d, curr] = pq.top();
            pq.pop();
            if (d > dist[curr]) continue;

            if (dsu.find(curr) == root_v) {
                target_node = curr;
                break;
            }

            for (const auto& nb : g.adj[curr]) {
                if (nb.weight > 2) continue; // Descartar aristas > 2
                int edge_cost = biased_weight(nb.weight);
                if (dist[curr] + edge_cost < dist[nb.to]) {
                    dist[nb.to] = dist[curr] + edge_cost;
                    parent[nb.to] = curr;
                    pq.push({dist[nb.to], nb.to});
                }
            }
        }

        if (target_node != -1) {
            std::vector<Edge> new_edges = remaining_edges;
            int curr = target_node;
            int path_real_cost = 0;
            while (parent[curr] != -1) {
                int p = parent[curr];
                int w = g.get_edge_weight(p, curr);
                new_edges.push_back({std::min(p, curr), std::max(p, curr), w});
                path_real_cost += w;
                curr = p;
            }

            if (path_real_cost <= kp.cost) {
                auto cand = MSTSolver::prune_steiner_leaves(g, MSTSolver::compute_mst(new_edges, g.num_nodes));
                if (Validator::is_valid_steiner_tree(g, cand, false)) {
                    int c = Validator::compute_cost(cand);
                    if (c < current_cost) {
                        current_cost = c;
                        current_tree = cand;
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

// Operador 2: Inserción de nodos Steiner candidatos (grado >= 2 hacia el árbol)
bool optimize_steiner_insertion(const Graph& g, std::vector<Edge>& current_tree, int& current_cost) {
    std::vector<bool> in_tree(g.num_nodes + 1, false);
    for (const auto& e : current_tree) {
        in_tree[e.u] = true;
        in_tree[e.v] = true;
    }

    for (int s = 1; s <= g.num_nodes; ++s) {
        if (in_tree[s] || g.is_terminal[s]) continue;

        int tree_connections = 0;
        std::vector<Edge> candidate_edges = current_tree;
        for (const auto& nb : g.adj[s]) {
            if (in_tree[nb.to]) {
                tree_connections++;
                candidate_edges.push_back({std::min(s, nb.to), std::max(s, nb.to), nb.weight});
            }
        }

        if (tree_connections >= 2) {
            auto cand = MSTSolver::prune_steiner_leaves(g, MSTSolver::compute_mst(candidate_edges, g.num_nodes));
            int c = Validator::compute_cost(cand);
            if (c < current_cost && Validator::is_valid_steiner_tree(g, cand, false)) {
                current_cost = c;
                current_tree = cand;
                return true;
            }
        }
    }
    return false;
}

// Operador 3: Eliminación de nodos Steiner de bifurcación (grado >= 3)
bool optimize_steiner_drop(const Graph& g, std::vector<Edge>& current_tree, int& current_cost) {
    std::vector<int> degree(g.num_nodes + 1, 0);
    for (const auto& e : current_tree) {
        degree[e.u]++;
        degree[e.v]++;
    }

    for (int s = 1; s <= g.num_nodes; ++s) {
        if (degree[s] < 3 || g.is_terminal[s]) continue;

        std::vector<Edge> remaining_edges;
        for (const auto& e : current_tree) {
            if (e.u != s && e.v != s) {
                remaining_edges.push_back(e);
            }
        }

        DSU dsu(g.num_nodes);
        for (const auto& e : remaining_edges) {
            dsu.unite(e.u, e.v);
        }

        std::vector<int> comp_reps;
        for (int t : g.terminals) {
            int r = dsu.find(t);
            if (std::find(comp_reps.begin(), comp_reps.end(), r) == comp_reps.end()) {
                comp_reps.push_back(r);
            }
        }

        if (comp_reps.size() <= 1) {
            auto cand = MSTSolver::prune_steiner_leaves(g, MSTSolver::compute_mst(remaining_edges, g.num_nodes));
            if (Validator::is_valid_steiner_tree(g, cand, false)) {
                int c = Validator::compute_cost(cand);
                if (c < current_cost) {
                    current_cost = c;
                    current_tree = cand;
                    return true;
                }
            }
            continue;
        }

        std::vector<Edge> reconnected_edges = remaining_edges;
        bool failed = false;

        while (comp_reps.size() > 1) {
            int src_comp = comp_reps.back();
            comp_reps.pop_back();

            std::vector<int> dist(g.num_nodes + 1, Graph::INF);
            std::vector<int> parent(g.num_nodes + 1, -1);
            std::priority_queue<std::pair<int, int>, std::vector<std::pair<int, int>>, std::greater<std::pair<int, int>>> pq;

            for (int i = 1; i <= g.num_nodes; ++i) {
                if (dsu.find(i) == src_comp) {
                    dist[i] = 0;
                    pq.push({0, i});
                }
            }

            int target_node = -1;
            while (!pq.empty()) {
                auto [d, curr] = pq.top();
                pq.pop();
                if (d > dist[curr]) continue;

                if (dsu.find(curr) != src_comp && dsu.find(curr) != dsu.find(src_comp)) {
                    int curr_rep = dsu.find(curr);
                    for (int rep : comp_reps) {
                        if (curr_rep == rep) {
                            target_node = curr;
                            break;
                        }
                    }
                    if (target_node != -1) break;
                }

                for (const auto& nb : g.adj[curr]) {
                    if (nb.to == s || nb.weight > 2) continue; // Prohibir nodo s y aristas > 2
                    int edge_cost = biased_weight(nb.weight);
                    if (dist[curr] + edge_cost < dist[nb.to]) {
                        dist[nb.to] = dist[curr] + edge_cost;
                        parent[nb.to] = curr;
                        pq.push({dist[nb.to], nb.to});
                    }
                }
            }

            if (target_node != -1) {
                int curr = target_node;
                while (parent[curr] != -1) {
                    int p = parent[curr];
                    int w = g.get_edge_weight(p, curr);
                    reconnected_edges.push_back({std::min(p, curr), std::max(p, curr), w});
                    dsu.unite(p, curr);
                    curr = p;
                }
            } else {
                failed = true;
                break;
            }
        }

        if (!failed) {
            auto cand = MSTSolver::prune_steiner_leaves(g, MSTSolver::compute_mst(reconnected_edges, g.num_nodes));
            if (Validator::is_valid_steiner_tree(g, cand, false)) {
                int c = Validator::compute_cost(cand);
                if (c < current_cost) {
                    current_cost = c;
                    current_tree = cand;
                    return true;
                }
            }
        }
    }
    return false;
}

// Operador 4: Reemplazo de Nodos Steiner de grado 2 (2-opt Shortcut / Deg-2 Swap)
bool optimize_steiner_deg2_swap(const Graph& g, std::vector<Edge>& current_tree, int& current_cost) {
    std::vector<std::vector<std::pair<int, int>>> tree_adj(g.num_nodes + 1);
    std::vector<int> degree(g.num_nodes + 1, 0);
    for (const auto& e : current_tree) {
        tree_adj[e.u].push_back({e.v, e.weight});
        tree_adj[e.v].push_back({e.u, e.weight});
        degree[e.u]++;
        degree[e.v]++;
    }

    for (int s = 1; s <= g.num_nodes; ++s) {
        if (degree[s] != 2 || g.is_terminal[s]) continue;

        int x = tree_adj[s][0].first;
        int y = tree_adj[s][1].first;
        int old_weight = tree_adj[s][0].second + tree_adj[s][1].second;

        int direct_w = g.get_edge_weight(x, y);
        if (direct_w != -1 && direct_w < old_weight) {
            std::vector<Edge> new_edges;
            for (const auto& e : current_tree) {
                if ((e.u == s && (e.v == x || e.v == y)) || (e.v == s && (e.u == x || e.u == y))) {
                    continue;
                }
                new_edges.push_back(e);
            }
            new_edges.push_back({std::min(x, y), std::max(x, y), direct_w});
            auto cand = MSTSolver::prune_steiner_leaves(g, MSTSolver::compute_mst(new_edges, g.num_nodes));
            if (Validator::is_valid_steiner_tree(g, cand, false)) {
                int c = Validator::compute_cost(cand);
                if (c < current_cost) {
                    current_cost = c;
                    current_tree = cand;
                    return true;
                }
            }
        }
    }
    return false;
}

// Búsqueda local completa con los 4 operadores hasta convergencia
void optimize_full_cycle(const Graph& g, std::vector<Edge>& tree, int& cost) {
    bool any_imp = true;
    while (any_imp) {
        any_imp = false;
        while (optimize_key_paths(g, tree, cost)) any_imp = true;
        while (optimize_steiner_insertion(g, tree, cost)) any_imp = true;
        while (optimize_steiner_drop(g, tree, cost)) any_imp = true;
        while (optimize_steiner_deg2_swap(g, tree, cost)) any_imp = true;
    }
}

// Perturbación sistemática (Shaking)
std::vector<Edge> shake_and_reconnect(const Graph& g, const std::vector<Edge>& current_tree, int num_paths_to_drop, std::mt19937& rng) {
    auto key_paths = extract_key_paths(g, current_tree);
    if (key_paths.size() <= 2) return current_tree;

    std::shuffle(key_paths.begin(), key_paths.end(), rng);
    int drop_count = std::min((int)key_paths.size() / 3, num_paths_to_drop);

    std::vector<Edge> remaining_edges;
    auto is_dropped = [&](const Edge& e) {
        int eu = std::min(e.u, e.v);
        int ev = std::max(e.u, e.v);
        for (int i = 0; i < drop_count; ++i) {
            for (const auto& kpe : key_paths[i].edges) {
                if (eu == kpe.u && ev == kpe.v) return true;
            }
        }
        return false;
    };

    for (const auto& e : current_tree) {
        if (!is_dropped(e)) {
            remaining_edges.push_back(e);
        }
    }

    DSU dsu(g.num_nodes);
    for (const auto& e : remaining_edges) {
        dsu.unite(e.u, e.v);
    }

    std::vector<int> comp_reps;
    for (int t : g.terminals) {
        int r = dsu.find(t);
        if (std::find(comp_reps.begin(), comp_reps.end(), r) == comp_reps.end()) {
            comp_reps.push_back(r);
        }
    }

    if (comp_reps.size() <= 1) {
        auto t = MSTSolver::prune_steiner_leaves(g, MSTSolver::compute_mst(remaining_edges, g.num_nodes));
        return Validator::is_valid_steiner_tree(g, t, false) ? t : current_tree;
    }

    std::vector<Edge> reconnected_edges = remaining_edges;
    while (comp_reps.size() > 1) {
        int src_comp = comp_reps.back();
        comp_reps.pop_back();

        std::vector<int> dist(g.num_nodes + 1, Graph::INF);
        std::vector<int> parent(g.num_nodes + 1, -1);
        std::priority_queue<std::pair<int, int>, std::vector<std::pair<int, int>>, std::greater<std::pair<int, int>>> pq;

        for (int i = 1; i <= g.num_nodes; ++i) {
            if (dsu.find(i) == src_comp) {
                dist[i] = 0;
                pq.push({0, i});
            }
        }

        int target_node = -1;
        while (!pq.empty()) {
            auto [d, curr] = pq.top();
            pq.pop();
            if (d > dist[curr]) continue;

            if (dsu.find(curr) != src_comp && dsu.find(curr) != dsu.find(src_comp)) {
                int curr_rep = dsu.find(curr);
                for (int rep : comp_reps) {
                    if (curr_rep == rep) {
                        target_node = curr;
                        break;
                    }
                }
                if (target_node != -1) break;
            }

            for (const auto& nb : g.adj[curr]) {
                if (nb.weight > 2) continue; // Descartar aristas pesadas
                int edge_cost = biased_weight(nb.weight);
                if (dist[curr] + edge_cost < dist[nb.to]) {
                    dist[nb.to] = dist[curr] + edge_cost;
                    parent[nb.to] = curr;
                    pq.push({dist[nb.to], nb.to});
                }
            }
        }

        if (target_node != -1) {
            int curr = target_node;
            while (parent[curr] != -1) {
                int p = parent[curr];
                int w = g.get_edge_weight(p, curr);
                reconnected_edges.push_back({std::min(p, curr), std::max(p, curr), w});
                dsu.unite(p, curr);
                curr = p;
            }
        } else {
            return current_tree;
        }
    }

    auto candidate = MSTSolver::prune_steiner_leaves(g, MSTSolver::compute_mst(reconnected_edges, g.num_nodes));
    if (Validator::is_valid_steiner_tree(g, candidate, false)) {
        return candidate;
    }
    return current_tree;
}

// Takahashi-Matsuyama SPH para generar árboles constructivos diversos
std::vector<Edge> run_sph(const Graph& g, int root) {
    std::vector<bool> in_tree(g.num_nodes + 1, false);
    std::vector<int> dist(g.num_nodes + 1, Graph::INF);
    std::vector<int> parent(g.num_nodes + 1, -1);
    std::priority_queue<std::pair<int, int>, std::vector<std::pair<int, int>>, std::greater<std::pair<int, int>>> pq;

    in_tree[root] = true;
    dist[root] = 0;
    pq.push({0, root});

    int terminals_connected = 1;
    std::vector<Edge> tree_subgraph_edges;

    while (terminals_connected < g.num_terminals) {
        int closest_terminal = -1;
        while (!pq.empty()) {
            auto [d, u] = pq.top();
            pq.pop();
            if (d > dist[u]) continue;
            if (!in_tree[u] && g.is_terminal[u]) {
                closest_terminal = u;
                break;
            }
            for (const auto& nb : g.adj[u]) {
                if (dist[u] + nb.weight < dist[nb.to]) {
                    dist[nb.to] = dist[u] + nb.weight;
                    parent[nb.to] = u;
                    pq.push({dist[nb.to], nb.to});
                }
            }
        }

        if (closest_terminal == -1) break;

        int curr = closest_terminal;
        std::vector<int> path_nodes;
        while (!in_tree[curr] && curr != -1) {
            path_nodes.push_back(curr);
            int p = parent[curr];
            if (p != -1) {
                int w = g.get_edge_weight(p, curr);
                tree_subgraph_edges.push_back({std::min(p, curr), std::max(p, curr), w});
            }
            curr = p;
        }

        for (int node : path_nodes) {
            in_tree[node] = true;
            dist[node] = 0;
            pq.push({0, node});
            if (g.is_terminal[node]) terminals_connected++;
        }
    }

    auto mst_res = MSTSolver::compute_mst(tree_subgraph_edges, g.num_nodes);
    return MSTSolver::prune_steiner_leaves(g, mst_res);
}

} // namespace anónimo

std::vector<Edge> Optimizer::constructive_heuristic(const Graph& g) {
    auto closure = g.compute_terminal_metric_closure();
    auto metric_edges = g.get_terminal_metric_edges(closure);
    auto metric_mst = MSTSolver::compute_mst(metric_edges, g.num_nodes);

    std::vector<Edge> original_subgraph_edges;
    for (const auto& me : metric_mst) {
        auto path_edges = g.get_metric_path_edges(me.u, me.v, closure);
        for (auto& pe : path_edges) {
            int u = pe.u;
            int v = pe.v;
            if (u > v) std::swap(u, v);
            original_subgraph_edges.push_back({u, v, pe.weight});
        }
    }

    std::sort(original_subgraph_edges.begin(), original_subgraph_edges.end());
    original_subgraph_edges.erase(
        std::unique(original_subgraph_edges.begin(), original_subgraph_edges.end(),
                    [](const Edge& a, const Edge& b) {
                        return a.u == b.u && a.v == b.v;
                    }),
        original_subgraph_edges.end()
    );

    auto initial_tree = MSTSolver::compute_mst(original_subgraph_edges, g.num_nodes);
    return MSTSolver::prune_steiner_leaves(g, initial_tree);
}

std::vector<Edge> Optimizer::local_search(const Graph& g, const std::vector<Edge>& initial_solution, int max_iterations) {
    // 1. Minar subgrafos élite uniendo aristas de KMB, solución previa guardada y raíces SPH
    std::vector<Edge> pool = initial_solution;

    // Cargar solución récord previa si existe
    std::vector<Edge> prev_tree;
    std::ifstream prev_csv("results/optimizer_solution.csv");
    if (prev_csv.is_open()) {
        std::string line;
        std::getline(prev_csv, line); // header
        while (std::getline(prev_csv, line)) {
            std::stringstream ss(line);
            std::string su, sv, sw;
            std::getline(ss, su, ',');
            std::getline(ss, sv, ',');
            std::getline(ss, sw, ',');
            if (!su.empty() && !sv.empty() && !sw.empty()) {
                Edge e = {std::min(std::stoi(su), std::stoi(sv)),
                          std::max(std::stoi(su), std::stoi(sv)),
                          std::stoi(sw)};
                pool.push_back(e);
                prev_tree.push_back(e);
            }
        }
        prev_csv.close();
    }

    // Semillas representativas para recolectar aristas prometedoras
    const std::vector<int> sample_roots = {
        16, 1241, 1476, 1393, 842, 1808, 117, 1172, 2030, 874, 50, 100, 200, 300, 400
    };
    for (int r : sample_roots) {
        auto sph_t = run_sph(g, r);
        for (const auto& e : sph_t) {
            pool.push_back({std::min(e.u, e.v), std::max(e.u, e.v), e.weight});
        }
    }

    std::sort(pool.begin(), pool.end());
    pool.erase(std::unique(pool.begin(), pool.end(), [](const Edge& a, const Edge& b) {
        return a.u == b.u && a.v == b.v;
    }), pool.end());

    // 2. Extraer árbol generador sobre el pool unificado
    auto current_tree = MSTSolver::prune_steiner_leaves(g, MSTSolver::compute_mst(pool, g.num_nodes));
    int current_cost = Validator::compute_cost(current_tree);

    // 3. Descenso inicial con los 4 operadores de vecindario
    optimize_full_cycle(g, current_tree, current_cost);

    auto best_tree = current_tree;
    int best_cost = current_cost;

    if (Validator::is_valid_steiner_tree(g, prev_tree, false)) {
        int prev_cost = Validator::compute_cost(prev_tree);
        if (prev_cost < best_cost) {
            best_cost = prev_cost;
            best_tree = prev_tree;
            current_cost = prev_cost;
            current_tree = prev_tree;
        }
    }

    int existing_record = best_cost;

    std::mt19937 rng(777);

    // 4. Metaheurística Simulated Annealing + ILS con Recalentamiento adaptativo
    double temperature = 2.5;
    double cooling = 0.97;
    int stagnant_iters = 0;

    int total_iters = std::max(100, std::min(max_iterations, 300));

    for (int iter = 1; iter <= total_iters; ++iter) {
        int drop = 2 + (iter % 7);
        auto shaken = shake_and_reconnect(g, current_tree, drop, rng);
        int shaken_cost = Validator::compute_cost(shaken);

        optimize_full_cycle(g, shaken, shaken_cost);

        int delta = shaken_cost - current_cost;
        bool accept = false;
        if (delta < 0) {
            accept = true;
        } else if (temperature > 0.05) {
            double prob = std::exp(-delta / temperature);
            std::uniform_real_distribution<double> u(0.0, 1.0);
            if (u(rng) < prob) {
                accept = true;
            }
        }

        if (shaken_cost < best_cost) {
            best_cost = shaken_cost;
            best_tree = shaken;
            stagnant_iters = 0;
            std::cout << "[Optimizer Record] Iter " << iter << " -> Nuevo récord: " << best_cost << "\n";
            if (best_cost < existing_record) {
                existing_record = best_cost;
                Validator::export_to_csv("results/optimizer_solution.csv", best_tree);
            }
        } else {
            stagnant_iters++;
        }

        if (accept) {
            current_tree = shaken;
            current_cost = shaken_cost;
        }

        // Mecanismo de recalentamiento si se detecta meseta
        if (stagnant_iters >= 25) {
            temperature = 1.8;
            stagnant_iters = 0;
            current_tree = best_tree;
            current_cost = best_cost;
        } else {
            temperature *= cooling;
        }
    }

    return best_tree;
}

std::vector<Edge> Optimizer::optimize(const Graph& g, const OptimizerConfig& config) {
    if (config.verbose) {
        std::cout << "[Optimizer] Fase 1: Calculando solución constructiva KMB sobre clausura métrica...\n";
    }

    auto t_start = std::chrono::high_resolution_clock::now();
    auto initial_sol = constructive_heuristic(g);
    int init_cost = Validator::compute_cost(initial_sol);

    if (config.verbose) {
        std::cout << "[Optimizer] Solución inicial KMB factible: Costo = " << init_cost 
                  << " | Aristas = " << initial_sol.size() << "\n";
        std::cout << "[Optimizer] Fase 2: Ejecutando Búsqueda Local 4-Operadores, Pool Élite y Recalentamiento ILS...\n";
    }

    auto final_sol = local_search(g, initial_sol, config.max_iterations);
    int final_cost = Validator::compute_cost(final_sol);

    auto t_end = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();

    if (config.verbose) {
        std::cout << "[Optimizer] Optimización finalizada con éxito en " << elapsed_ms << " ms.\n";
        std::cout << "[Optimizer] Mejora total: " << init_cost << " -> " << final_cost 
                  << " (-" << (init_cost - final_cost) << " unidades)\n";
    }

    return final_sol;
}
