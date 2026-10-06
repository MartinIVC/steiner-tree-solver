#include "mst.hpp"
#include <algorithm>
#include <queue>
#include <iostream>

std::vector<Edge> MSTSolver::compute_mst(const Graph& g) {
    return compute_mst(g.edge_list, g.num_nodes);
}

std::vector<Edge> MSTSolver::compute_mst(const std::vector<Edge>& edges, int num_nodes) {
    if (edges.empty() || num_nodes <= 1) return {};

    std::vector<Edge> sorted_edges = edges;
    std::sort(sorted_edges.begin(), sorted_edges.end());

    DSU dsu(num_nodes);
    std::vector<Edge> mst_edges;
    mst_edges.reserve(num_nodes - 1);

    for (const auto& e : sorted_edges) {
        if (dsu.unite(e.u, e.v)) {
            mst_edges.push_back(e);
        }
    }
    return mst_edges;
}

std::vector<Edge> MSTSolver::compute_mst(const std::vector<Edge>& edges) {
    int max_node = 0;
    for (const auto& e : edges) {
        if (e.u > max_node) max_node = e.u;
        if (e.v > max_node) max_node = e.v;
    }
    return compute_mst(edges, max_node);
}

// Estructura auxiliar para la cola de prioridad de Prim
struct PrimElement {
    int weight;
    int target_priority; // 1 si el destino es terminal (cuando prefer_terminals es true), 0 en caso contrario
    int u;
    int v;

    bool operator>(const PrimElement& other) const {
        if (weight != other.weight) {
            return weight > other.weight; // Min-heap por peso
        }
        if (target_priority != other.target_priority) {
            return target_priority < other.target_priority; // Mayor prioridad primero
        }
        if (u != other.u) return u > other.u;
        return v > other.v;
    }
};

std::vector<Edge> MSTSolver::compute_mst_prim(const Graph& g, int start_node, bool prefer_terminals) {
    if (g.num_nodes <= 1) return {};

    int root = start_node;
    if (root <= 0 || root > g.num_nodes) {
        root = g.terminals.empty() ? 1 : g.terminals.front();
    }

    std::vector<bool> visited(g.num_nodes + 1, false);
    std::priority_queue<PrimElement, std::vector<PrimElement>, std::greater<PrimElement>> pq;

    visited[root] = true;
    for (const auto& neighbor : g.adj[root]) {
        int priority = (prefer_terminals && g.is_terminal[neighbor.to]) ? 1 : 0;
        pq.push({neighbor.weight, priority, root, neighbor.to});
    }

    std::vector<Edge> mst_edges;
    mst_edges.reserve(g.num_nodes - 1);

    while (!pq.empty() && (int)mst_edges.size() < g.num_nodes - 1) {
        auto top = pq.top();
        pq.pop();

        int u = top.u;
        int v = top.v;
        int w = top.weight;

        if (visited[v]) continue;
        visited[v] = true;

        mst_edges.push_back({u, v, w});

        for (const auto& neighbor : g.adj[v]) {
            int nxt = neighbor.to;
            if (!visited[nxt]) {
                int priority = (prefer_terminals && g.is_terminal[nxt]) ? 1 : 0;
                pq.push({neighbor.weight, priority, v, nxt});
            }
        }
    }

    return mst_edges;
}

std::vector<Edge> MSTSolver::prune_steiner_leaves(const Graph& g, const std::vector<Edge>& tree_edges) {
    // 1. Construir grados y adyacencias del árbol
    std::vector<std::vector<std::pair<int, int>>> tree_adj(g.num_nodes + 1);
    std::vector<int> degree(g.num_nodes + 1, 0);

    for (size_t i = 0; i < tree_edges.size(); i++) {
        const auto& e = tree_edges[i];
        tree_adj[e.u].push_back({e.v, (int)i});
        tree_adj[e.v].push_back({e.u, (int)i});
        degree[e.u]++;
        degree[e.v]++;
    }

    // 2. Cola para hojas no terminales
    std::queue<int> leaves;
    for (int i = 1; i <= g.num_nodes; i++) {
        if (degree[i] == 1 && !g.is_terminal[i]) {
            leaves.push(i);
        }
    }

    std::vector<bool> edge_removed(tree_edges.size(), false);

    // 3. Poda iterativa
    while (!leaves.empty()) {
        int leaf = leaves.front();
        leaves.pop();

        if (degree[leaf] == 0) continue;

        for (const auto& neighbor : tree_adj[leaf]) {
            int adj_node = neighbor.first;
            int edge_idx = neighbor.second;

            if (!edge_removed[edge_idx]) {
                edge_removed[edge_idx] = true;
                degree[leaf]--;
                degree[adj_node]--;

                // Si el vecino se convirtió en hoja y no es terminal, se encola para podar
                if (degree[adj_node] == 1 && !g.is_terminal[adj_node]) {
                    leaves.push(adj_node);
                }
            }
        }
    }

    // 4. Recolectar aristas restantes
    std::vector<Edge> pruned_edges;
    for (size_t i = 0; i < tree_edges.size(); i++) {
        if (!edge_removed[i]) {
            pruned_edges.push_back(tree_edges[i]);
        }
    }

    return pruned_edges;
}
