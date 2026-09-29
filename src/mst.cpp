#include "mst.hpp"
#include <algorithm>
#include <queue>
#include <iostream>

// Estructura Disjoint Set Union (DSU) para el algoritmo de Kruskal
struct DSU {
    std::vector<int> parent;
    DSU(int n) {
        parent.resize(n + 1);
        for (int i = 0; i <= n; i++) parent[i] = i;
    }
    int find(int i) {
        if (parent[i] == i) return i;
        return parent[i] = find(parent[i]);
    }
    bool unite(int i, int j) {
        int root_i = find(i);
        int root_j = find(j);
        if (root_i != root_j) {
            parent[root_i] = root_j;
            return true;
        }
        return false;
    }
};

std::vector<Edge> MSTSolver::compute_mst(const Graph& g) {
    std::vector<Edge> sorted_edges = g.edge_list;
    std::sort(sorted_edges.begin(), sorted_edges.end());

    DSU dsu(g.num_nodes);
    std::vector<Edge> mst_edges;
    mst_edges.reserve(g.num_nodes - 1);

    for (const auto& e : sorted_edges) {
        if (dsu.unite(e.u, e.v)) {
            mst_edges.push_back(e);
            if ((int)mst_edges.size() == g.num_nodes - 1) {
                break;
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
