#pragma once
#include <vector>

// Estructura Disjoint Set Union (DSU) con unión por rango y compresión de caminos (1-indexed)
struct DSU {
    std::vector<int> parent;
    std::vector<int> rank;

    explicit DSU(int n = 0) {
        init(n);
    }

    void init(int n) {
        parent.resize(n + 1);
        rank.assign(n + 1, 0);
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
            if (rank[root_i] < rank[root_j]) {
                parent[root_i] = root_j;
            } else if (rank[root_i] > rank[root_j]) {
                parent[root_j] = root_i;
            } else {
                parent[root_j] = root_i;
                rank[root_i]++;
            }
            return true;
        }
        return false;
    }

    bool connected(int i, int j) {
        return find(i) == find(j);
    }
};
