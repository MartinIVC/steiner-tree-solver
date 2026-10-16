#include "optimizer.hpp"
#include "mst.hpp"
#include "validator.hpp"
#include "dsu.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <numeric>
#include <queue>
#include <random>
#include <tuple>
#include <vector>

namespace {

using Clock = std::chrono::high_resolution_clock;

struct FastNeighbor {
    int to;
    int weight;
    int edge_id;
    int dual_score;
};

struct CandidateTree {
    int cost = GRAPH_INF;
    int w2_plus_count = 0;
    int steiner_count = 0;
    std::vector<Edge> edges;
    std::vector<uint8_t> active;

    bool operator<(const CandidateTree& other) const {
        if (cost != other.cost) return cost < other.cost;
        if (w2_plus_count != other.w2_plus_count) return w2_plus_count < other.w2_plus_count;
        return steiner_count < other.steiner_count;
    }
};

class SteinerEngine {
public:
    const Graph& g;
    int N;
    int M;
    int T_count;

    std::vector<int> term_d1;
    std::vector<int> term_d2;
    std::vector<std::vector<FastNeighbor>> fast_adj;
    std::vector<std::vector<int>> w1_adj;
    std::vector<Edge> reduced_edges;
    std::vector<int> edge_dual_score;
    int best_dual_lb = 0;

    explicit SteinerEngine(const Graph& graph)
        : g(graph), N(graph.num_nodes), M((int)graph.edge_list.size()), T_count(graph.num_terminals) {
        term_d1.assign(N + 1, 0);
        term_d2.assign(N + 1, 0);
        fast_adj.resize(N + 1);
        w1_adj.resize(N + 1);
        edge_dual_score.assign(M, 0);
    }

    // =========================================================================
    // FASE 0: Presolve (Reducción del Grafo) y Ascenso Dual (Wong Dual Ascent)
    // =========================================================================
    void presolve_and_dual_ascent() {
        for (int t : g.terminals) {
            for (const auto& nb : g.adj[t]) {
                if (nb.weight == 1) {
                    term_d1[nb.to]++;
                    term_d2[nb.to] += 3;
                } else if (nb.weight == 2) {
                    term_d2[nb.to] += 1;
                }
            }
        }
        for (int u = 1; u <= N; ++u) {
            if (term_d1[u] > 0) {
                for (const auto& nb : g.adj[u]) {
                    if (nb.weight == 1) {
                        term_d2[nb.to] += term_d1[u];
                    }
                }
            }
        }

        std::vector<std::vector<std::pair<int, int>>> w12_adj(N + 1);
        for (int i = 0; i < M; ++i) {
            const auto& e = g.edge_list[i];
            if (e.weight == 1) {
                w1_adj[e.u].push_back(e.v);
                w1_adj[e.v].push_back(e.u);
            }
            if (e.weight <= 2) {
                w12_adj[e.u].push_back({e.v, e.weight});
                w12_adj[e.v].push_back({e.u, e.weight});
            }
        }

        std::vector<int> mark(N + 1, 0);
        std::vector<int> mark_w(N + 1, GRAPH_INF);

        for (int u = 1; u <= N; ++u) {
            for (const auto& [x, wx] : w12_adj[u]) {
                mark[x] = u;
                mark_w[x] = wx;
            }
            for (const auto& nb : g.adj[u]) {
                int v = nb.to;
                int w = nb.weight;
                if (u >= v) continue;

                bool dominated = false;
                if (w >= 4) {
                    dominated = true;
                } else if (w >= 2) {
                    for (const auto& [x, wx] : w12_adj[v]) {
                        if (mark[x] == u) {
                            int two_hop = mark_w[x] + wx;
                            if (two_hop < w || (two_hop == w && g.is_terminal[x])) {
                                dominated = true;
                                break;
                            }
                        }
                    }
                }

                if (!dominated) {
                    int eid = (int)reduced_edges.size();
                    reduced_edges.push_back({u, v, w});
                    fast_adj[u].push_back({v, w, eid, 0});
                    fast_adj[v].push_back({u, w, eid, 0});
                }
            }
        }

        edge_dual_score.assign(reduced_edges.size(), 0);
        run_multi_dual_ascent(10);

        for (int u = 1; u <= N; ++u) {
            for (auto& nb : fast_adj[u]) {
                nb.dual_score = edge_dual_score[nb.edge_id];
            }
            std::sort(fast_adj[u].begin(), fast_adj[u].end(), [&](const FastNeighbor& a, const FastNeighbor& b) {
                if (a.weight != b.weight) return a.weight < b.weight;
                if (a.dual_score != b.dual_score) return a.dual_score > b.dual_score;
                bool ta = g.is_terminal[a.to];
                bool tb = g.is_terminal[b.to];
                if (ta != tb) return ta > tb;
                if (term_d1[a.to] != term_d1[b.to]) return term_d1[a.to] > term_d1[b.to];
                return term_d2[a.to] > term_d2[b.to];
            });
            std::sort(w1_adj[u].begin(), w1_adj[u].end(), [&](int a, int b) {
                if (g.is_terminal[a] != g.is_terminal[b]) return g.is_terminal[a] > g.is_terminal[b];
                if (term_d1[a] != term_d1[b]) return term_d1[a] > term_d1[b];
                return term_d2[a] > term_d2[b];
            });
        }
    }

    void run_multi_dual_ascent(int num_roots) {
        if (T_count <= 1 || reduced_edges.empty()) return;

        int E_red = (int)reduced_edges.size();
        std::vector<std::vector<std::pair<int, int>>> in_arcs(N + 1);
        for (int eid = 0; eid < E_red; ++eid) {
            int u = reduced_edges[eid].u;
            int v = reduced_edges[eid].v;
            in_arcs[v].push_back({u, 2 * eid});
            in_arcs[u].push_back({v, 2 * eid + 1});
        }

        std::vector<int> res_cost(2 * E_red);
        std::vector<int> visited_stamp(N + 1, 0);
        int stamp = 0;

        for (int r_idx = 0; r_idx < num_roots && r_idx < T_count; ++r_idx) {
            int root = g.terminals[(r_idx * 41) % T_count];
            for (int eid = 0; eid < E_red; ++eid) {
                res_cost[2 * eid] = reduced_edges[eid].weight;
                res_cost[2 * eid + 1] = reduced_edges[eid].weight;
            }

            std::vector<int> active_terms;
            active_terms.reserve(T_count - 1);
            for (int t : g.terminals) {
                if (t != root) active_terms.push_back(t);
            }

            int dual_lb = 0;
            bool progress = true;
            std::vector<int> comp_nodes;
            comp_nodes.reserve(N);

            while (progress) {
                progress = false;
                for (size_t i = 0; i < active_terms.size(); ++i) {
                    int t = active_terms[i];
                    ++stamp;
                    comp_nodes.clear();
                    comp_nodes.push_back(t);
                    visited_stamp[t] = stamp;

                    bool reaches_root = false;
                    size_t head = 0;
                    while (head < comp_nodes.size()) {
                        int curr = comp_nodes[head++];
                        for (const auto& [pred, arc_id] : in_arcs[curr]) {
                            if (res_cost[arc_id] == 0 && visited_stamp[pred] != stamp) {
                                if (pred == root) {
                                    reaches_root = true;
                                    break;
                                }
                                visited_stamp[pred] = stamp;
                                comp_nodes.push_back(pred);
                                if ((int)comp_nodes.size() > 80) {
                                    reaches_root = true;
                                    break;
                                }
                            }
                        }
                        if (reaches_root) break;
                    }

                    if (reaches_root) {
                        active_terms[i] = active_terms.back();
                        active_terms.pop_back();
                        --i;
                        continue;
                    }

                    int min_delta = GRAPH_INF;
                    for (int v : comp_nodes) {
                        for (const auto& [u, arc_id] : in_arcs[v]) {
                            if (visited_stamp[u] != stamp) {
                                if (res_cost[arc_id] < min_delta) {
                                    min_delta = res_cost[arc_id];
                                    if (min_delta == 1) break;
                                }
                            }
                        }
                        if (min_delta == 1) break;
                    }

                    if (min_delta > 0 && min_delta < GRAPH_INF) {
                        dual_lb += min_delta;
                        for (int v : comp_nodes) {
                            for (const auto& [u, arc_id] : in_arcs[v]) {
                                if (visited_stamp[u] != stamp) {
                                    res_cost[arc_id] -= min_delta;
                                }
                            }
                        }
                        progress = true;
                    }
                }
            }

            if (dual_lb > best_dual_lb) {
                best_dual_lb = dual_lb;
            }

            for (int eid = 0; eid < E_red; ++eid) {
                if (res_cost[2 * eid] == 0 || res_cost[2 * eid + 1] == 0) {
                    edge_dual_score[eid]++;
                }
            }
        }
    }

    // =========================================================================
    // FASE 2: Evaluación Canónica de 2 Pasadas con Orden de Saturación Estrella
    // =========================================================================
    CandidateTree build_canonical_tree_single_pass(const std::vector<uint8_t>& active_mask,
                                                   const std::vector<int>* node_priority_bias = nullptr) const {
        CandidateTree res;
        res.active.assign(N + 1, 0);

        std::vector<int> active_nodes;
        std::vector<int> steiner_nodes;
        active_nodes.reserve(T_count + 250);
        steiner_nodes.reserve(250);

        for (int u = 1; u <= N; ++u) {
            if (g.is_terminal[u]) {
                active_nodes.push_back(u);
            } else if (active_mask[u]) {
                active_nodes.push_back(u);
                steiner_nodes.push_back(u);
            }
        }

        std::vector<int> w1_active_deg(N + 1, 0);
        for (int s : steiner_nodes) {
            int cnt = 0;
            for (int v : w1_adj[s]) {
                if (g.is_terminal[v] || active_mask[v]) cnt++;
            }
            w1_active_deg[s] = cnt;
        }

        std::sort(steiner_nodes.begin(), steiner_nodes.end(), [&](int a, int b) {
            int ba = node_priority_bias ? (*node_priority_bias)[a] : 0;
            int bb = node_priority_bias ? (*node_priority_bias)[b] : 0;
            if (ba != bb) return ba < bb;
            if (w1_active_deg[a] != w1_active_deg[b]) return w1_active_deg[a] > w1_active_deg[b];
            if (term_d1[a] != term_d1[b]) return term_d1[a] > term_d1[b];
            return term_d2[a] > term_d2[b];
        });

        std::vector<int> node_rank(N + 1, 0);
        for (size_t i = 0; i < steiner_nodes.size(); ++i) {
            node_rank[steiner_nodes[i]] = (int)i + 1;
        }

        struct WeightedEdge {
            int w;
            int max_rank;
            int min_rank;
            int dual;
            int u;
            int v;
            bool operator<(const WeightedEdge& o) const {
                if (w != o.w) return w < o.w;
                if (max_rank != o.max_rank) return max_rank < o.max_rank;
                if (min_rank != o.min_rank) return min_rank < o.min_rank;
                return dual > o.dual;
            }
        };

        std::vector<WeightedEdge> cand_edges;
        cand_edges.reserve(active_nodes.size() * 4);

        for (int u : active_nodes) {
            int ru = node_rank[u];
            for (const auto& nb : fast_adj[u]) {
                int v = nb.to;
                if (u < v && (active_mask[v] || g.is_terminal[v])) {
                    int rv = node_rank[v];
                    cand_edges.push_back({nb.weight, std::max(ru, rv), std::min(ru, rv), nb.dual_score, u, v});
                }
            }
        }

        std::sort(cand_edges.begin(), cand_edges.end());

        DSU dsu(N);
        std::vector<Edge> mst_edges;
        mst_edges.reserve(active_nodes.size());

        for (const auto& ce : cand_edges) {
            if (dsu.unite(ce.u, ce.v)) {
                mst_edges.push_back({ce.u, ce.v, ce.w});
                if (mst_edges.size() + 1 == active_nodes.size()) break;
            }
        }

        int r0 = dsu.find(g.terminals[0]);
        for (int t : g.terminals) {
            if (dsu.find(t) != r0) {
                res.cost = GRAPH_INF;
                return res;
            }
        }

        res.edges = MSTSolver::prune_steiner_leaves(g, mst_edges);

        int total_cost = 0;
        int w2_plus = 0;
        for (const auto& e : res.edges) {
            total_cost += e.weight;
            if (e.weight >= 2) w2_plus++;
            res.active[e.u] = 1;
            res.active[e.v] = 1;
        }
        res.cost = total_cost;
        res.w2_plus_count = w2_plus;

        int s_count = 0;
        for (int u = 1; u <= N; ++u) {
            if (res.active[u] && !g.is_terminal[u]) {
                s_count++;
            }
        }
        res.steiner_count = s_count;
        return res;
    }

    CandidateTree build_canonical_tree(const std::vector<uint8_t>& active_mask,
                                       const std::vector<int>* node_priority_bias = nullptr) const {
        CandidateTree pass1 = build_canonical_tree_single_pass(active_mask, node_priority_bias);
        if (pass1.cost >= GRAPH_INF) return pass1;

        std::vector<int> deg_bias(N + 1, 0);
        for (const auto& e : pass1.edges) {
            deg_bias[e.u]--;
            deg_bias[e.v]--;
        }
        for (int u = 1; u <= N; ++u) {
            deg_bias[u] *= 100;
        }

        CandidateTree pass2 = build_canonical_tree_single_pass(pass1.active, &deg_bias);
        return (pass2 < pass1) ? pass2 : pass1;
    }

    void expand_w2_edges_to_w1_bridges(CandidateTree& sol) const {
        std::vector<int> deg(N + 1, 0);
        for (const auto& e : sol.edges) {
            deg[e.u]++;
            deg[e.v]++;
        }

        DSU w1_dsu(N);
        for (const auto& e : sol.edges) {
            if (e.weight == 1) {
                bool ku = g.is_terminal[e.u] || deg[e.u] >= 3;
                bool kv = g.is_terminal[e.v] || deg[e.v] >= 3;
                if (ku && kv) {
                    w1_dsu.unite(e.u, e.v);
                }
            }
        }

        std::vector<std::vector<int>> s_comps(N + 1);
        for (int s = 1; s <= N; ++s) {
            if (sol.active[s] || g.is_terminal[s]) continue;
            for (int v : w1_adj[s]) {
                if (sol.active[v]) {
                    int c = w1_dsu.find(v);
                    if (std::find(s_comps[s].begin(), s_comps[s].end(), c) == s_comps[s].end()) {
                        s_comps[s].push_back(c);
                    }
                }
            }
        }

        std::vector<uint8_t> cand_mask = sol.active;
        bool added_any = false;

        for (const auto& e : sol.edges) {
            if (e.weight >= 2) {
                int cu = w1_dsu.find(e.u);
                int cv = w1_dsu.find(e.v);
                if (cu == cv) continue;

                int best_s = -1;
                int best_aff = -1;
                for (int s = 1; s <= N; ++s) {
                    if (cand_mask[s] || s_comps[s].size() < 2) continue;
                    bool has_cu = std::find(s_comps[s].begin(), s_comps[s].end(), cu) != s_comps[s].end();
                    bool has_cv = std::find(s_comps[s].begin(), s_comps[s].end(), cv) != s_comps[s].end();
                    if (has_cu && has_cv) {
                        int aff = (int)s_comps[s].size() * 100 + term_d1[s] * 10 + term_d2[s];
                        if (aff > best_aff) {
                            best_aff = aff;
                            best_s = s;
                        }
                    }
                }
                if (best_s != -1) {
                    cand_mask[best_s] = 1;
                    added_any = true;
                }
            }
        }

        if (added_any) {
            CandidateTree expanded = build_canonical_tree(cand_mask);
            if (expanded.cost <= sol.cost) {
                sol = std::move(expanded);
            }
        }
    }

    // =========================================================================
    // FASE 1A: Heurística KMB por Regiones de Voronoi (Mehlhorn)
    // =========================================================================
    CandidateTree voronoi_kmb() const {
        std::vector<int> dist(N + 1, GRAPH_INF);
        std::vector<int> orig(N + 1, -1);
        std::vector<int> parent(N + 1, -1);

        using PII = std::pair<int, int>;
        std::priority_queue<PII, std::vector<PII>, std::greater<PII>> pq;

        for (int t : g.terminals) {
            dist[t] = 0;
            orig[t] = t;
            pq.push({0, t});
        }

        while (!pq.empty()) {
            auto [d, u] = pq.top();
            pq.pop();
            if (d > dist[u]) continue;

            for (const auto& nb : fast_adj[u]) {
                int v = nb.to;
                int nd = d + nb.weight;
                if (nd < dist[v]) {
                    dist[v] = nd;
                    orig[v] = orig[u];
                    parent[v] = u;
                    pq.push({nd, v});
                }
            }
        }

        struct BoundaryEdge {
            int metric_w;
            int tu;
            int tv;
            int u;
            int v;
            bool operator<(const BoundaryEdge& o) const {
                return metric_w < o.metric_w;
            }
        };

        std::vector<BoundaryEdge> b_edges;
        for (const auto& e : reduced_edges) {
            if (orig[e.u] != -1 && orig[e.v] != -1 && orig[e.u] != orig[e.v]) {
                b_edges.push_back({dist[e.u] + e.weight + dist[e.v], orig[e.u], orig[e.v], e.u, e.v});
            }
        }
        std::sort(b_edges.begin(), b_edges.end());

        DSU dsu(N);
        std::vector<uint8_t> active(N + 1, 0);
        for (int t : g.terminals) active[t] = 1;

        for (const auto& be : b_edges) {
            if (dsu.unite(be.tu, be.tv)) {
                int curr = be.u;
                while (curr != -1 && !active[curr]) {
                    active[curr] = 1;
                    curr = parent[curr];
                }
                curr = be.v;
                while (curr != -1 && !active[curr]) {
                    active[curr] = 1;
                    curr = parent[curr];
                }
            }
        }

        return build_canonical_tree(active);
    }

    // =========================================================================
    // FASE 1B: Heurística de Camino Más Corto (SPH / Takahashi-Matsuyama)
    // =========================================================================
    CandidateTree run_sph(int root_terminal, const std::vector<int>* node_jitter = nullptr) const {
        std::vector<uint8_t> in_tree(N + 1, 0);
        in_tree[root_terminal] = 1;
        int remaining_terminals = T_count - 1;

        std::vector<int> dist(N + 1, GRAPH_INF);
        std::vector<int> tie_score(N + 1, GRAPH_INF);
        std::vector<int> parent(N + 1, -1);

        using State = std::tuple<int, int, int>;
        std::priority_queue<State, std::vector<State>, std::greater<State>> pq;

        dist[root_terminal] = 0;
        tie_score[root_terminal] = 0;
        pq.push({0, 0, root_terminal});

        std::vector<int> new_path_nodes;
        new_path_nodes.reserve(32);

        while (remaining_terminals > 0 && !pq.empty()) {
            auto [d, ts, u] = pq.top();
            pq.pop();

            if (d > dist[u] || (d == dist[u] && ts > tie_score[u])) continue;

            if (g.is_terminal[u] && !in_tree[u]) {
                new_path_nodes.clear();
                int curr = u;
                while (curr != -1 && !in_tree[curr]) {
                    in_tree[curr] = 1;
                    if (g.is_terminal[curr]) {
                        remaining_terminals--;
                    }
                    new_path_nodes.push_back(curr);
                    curr = parent[curr];
                }
                for (int node : new_path_nodes) {
                    dist[node] = 0;
                    tie_score[node] = 0;
                    pq.push({0, 0, node});
                }
                continue;
            }

            for (const auto& nb : fast_adj[u]) {
                int v = nb.to;
                int nd = d + nb.weight;
                int jitter = (node_jitter ? (*node_jitter)[v] : 0);
                int nts = ts - (term_d1[v] * 4 + term_d2[v] + nb.dual_score * 3 + (g.is_terminal[v] ? 6 : 0)) + jitter;

                if (nd < dist[v] || (nd == dist[v] && nts < tie_score[v])) {
                    dist[v] = nd;
                    tie_score[v] = nts;
                    parent[v] = u;
                    pq.push({nd, nts, v});
                }
            }
        }

        return build_canonical_tree(in_tree);
    }

    CandidateTree repair_forest_sph(const std::vector<uint8_t>& seed_mask,
                                    int root_node,
                                    const std::vector<int>* node_jitter = nullptr) const {
        std::vector<uint8_t> in_tree(N + 1, 0);
        std::vector<int> dist(N + 1, GRAPH_INF);
        std::vector<int> tie_score(N + 1, GRAPH_INF);
        std::vector<int> parent(N + 1, -1);

        using State = std::tuple<int, int, int>;
        std::priority_queue<State, std::vector<State>, std::greater<State>> pq;

        std::vector<std::vector<int>> seed_adj(N + 1);
        for (int u = 1; u <= N; ++u) {
            if (!seed_mask[u]) continue;
            for (int v : w1_adj[u]) {
                if (u < v && seed_mask[v]) {
                    seed_adj[u].push_back(v);
                    seed_adj[v].push_back(u);
                }
            }
        }

        auto absorb_seed_component = [&](int start_u) {
            int added_terms = 0;
            std::queue<int> q;
            q.push(start_u);
            in_tree[start_u] = 1;
            while (!q.empty()) {
                int curr = q.front();
                q.pop();
                if (g.is_terminal[curr]) added_terms++;
                dist[curr] = 0;
                tie_score[curr] = 0;
                pq.push({0, 0, curr});
                for (int nxt : seed_adj[curr]) {
                    if (!in_tree[nxt]) {
                        in_tree[nxt] = 1;
                        q.push(nxt);
                    }
                }
            }
            return added_terms;
        };

        int connected_terms = absorb_seed_component(root_node);
        int remaining_terminals = T_count - connected_terms;

        while (remaining_terminals > 0 && !pq.empty()) {
            auto [d, ts, u] = pq.top();
            pq.pop();

            if (d > dist[u] || (d == dist[u] && ts > tie_score[u])) continue;

            if ((g.is_terminal[u] || seed_mask[u]) && !in_tree[u]) {
                int curr = u;
                while (curr != -1 && !in_tree[curr]) {
                    if (seed_mask[curr]) {
                        remaining_terminals -= absorb_seed_component(curr);
                    } else {
                        in_tree[curr] = 1;
                        if (g.is_terminal[curr]) remaining_terminals--;
                        dist[curr] = 0;
                        tie_score[curr] = 0;
                        pq.push({0, 0, curr});
                    }
                    curr = parent[curr];
                }
                continue;
            }

            for (const auto& nb : fast_adj[u]) {
                int v = nb.to;
                int nd = d + nb.weight;
                int jitter = (node_jitter ? (*node_jitter)[v] : 0);
                int nts = ts - (term_d1[v] * 4 + term_d2[v] + nb.dual_score * 3 + (g.is_terminal[v] ? 6 : 0)) + jitter;

                if (nd < dist[v] || (nd == dist[v] && nts < tie_score[v])) {
                    dist[v] = nd;
                    tie_score[v] = nts;
                    parent[v] = u;
                    pq.push({nd, nts, v});
                }
            }
        }

        return build_canonical_tree(in_tree);
    }

    // =========================================================================
    // FASE 3: Búsqueda Local por Descenso de Vecindad Variable (VND)
    // =========================================================================

    bool vnd_star_insertion(CandidateTree& sol) const {
        bool any_improved = false;

        expand_w2_edges_to_w1_bridges(sol);

        std::vector<int> tree_deg(N + 1, 0);
        std::vector<int> bias(N + 1, 0);
        DSU comp_dsu(N);
        std::vector<std::vector<int>> s_comps(N + 1);
        std::vector<int> star_cands;

        auto rebuild_super_components = [&]() {
            std::fill(tree_deg.begin(), tree_deg.end(), 0);
            for (const auto& e : sol.edges) {
                tree_deg[e.u]++;
                tree_deg[e.v]++;
            }
            std::fill(bias.begin(), bias.end(), 0);
            for (int u = 1; u <= N; ++u) {
                if (sol.active[u] && !g.is_terminal[u]) {
                    if (tree_deg[u] == 2) bias[u] = 500;
                    else bias[u] = 0;
                }
            }
            comp_dsu.init(N);
            for (const auto& e : sol.edges) {
                if (e.weight == 1 && bias[e.u] == 0 && bias[e.v] == 0) {
                    comp_dsu.unite(e.u, e.v);
                }
            }
            star_cands.clear();
            for (int s = 1; s <= N; ++s) {
                s_comps[s].clear();
                if (sol.active[s] || g.is_terminal[s]) continue;
                for (int v : w1_adj[s]) {
                    if (sol.active[v]) {
                        int c = comp_dsu.find(v);
                        if (std::find(s_comps[s].begin(), s_comps[s].end(), c) == s_comps[s].end()) {
                            s_comps[s].push_back(c);
                        }
                    }
                }
                if (s_comps[s].size() >= 2) {
                    star_cands.push_back(s);
                }
            }
        };

        rebuild_super_components();

        // 1. Inserción rápida de 1-Estrella filtrada por super-componentes
        for (size_t idx = 0; idx < star_cands.size(); ++idx) {
            int s = star_cands[idx];
            if (sol.active[s]) continue;

            int w2_active = 0;
            if (s_comps[s].size() == 2) {
                for (const auto& nb : fast_adj[s]) {
                    if (nb.weight == 2 && sol.active[nb.to]) {
                        int c = comp_dsu.find(nb.to);
                        if (std::find(s_comps[s].begin(), s_comps[s].end(), c) == s_comps[s].end()) {
                            w2_active++;
                            break;
                        }
                    }
                }
            }

            if (s_comps[s].size() >= 3 || (s_comps[s].size() == 2 && w2_active >= 1)) {
                std::vector<uint8_t> cand_mask = sol.active;
                cand_mask[s] = 1;
                bias[s] = -500;

                CandidateTree biased_tree = build_canonical_tree(cand_mask, &bias);
                bias[s] = 0;

                if (biased_tree < sol) {
                    sol = std::move(biased_tree);
                    any_improved = true;
                    rebuild_super_components();
                    idx = 0;
                }
            }
        }

        // 2. Inserción de 2-Estrella y 3-Estrella sobre la lista filtrada star_cands
        for (size_t i = 0; i < star_cands.size(); ++i) {
            int s1 = star_cands[i];
            if (sol.active[s1]) continue;

            for (int s2 : w1_adj[s1]) {
                if (s2 <= s1 || sol.active[s2] || s_comps[s2].size() < 2) continue;

                std::vector<int> union_c = s_comps[s1];
                for (int c : s_comps[s2]) {
                    if (std::find(union_c.begin(), union_c.end(), c) == union_c.end()) {
                        union_c.push_back(c);
                    }
                }

                if (union_c.size() >= 4) {
                    std::vector<uint8_t> cand_mask = sol.active;
                    cand_mask[s1] = 1;
                    cand_mask[s2] = 1;
                    bias[s1] = -500;
                    bias[s2] = -500;

                    CandidateTree biased_tree = build_canonical_tree(cand_mask, &bias);
                    bias[s1] = 0;
                    bias[s2] = 0;

                    if (biased_tree < sol) {
                        sol = std::move(biased_tree);
                        any_improved = true;
                        rebuild_super_components();
                        break;
                    }
                }

                if (union_c.size() >= 3) {
                    for (int s3 : w1_adj[s2]) {
                        if (s3 == s1 || sol.active[s3] || s_comps[s3].size() < 2) continue;
                        int extra = 0;
                        for (int c : s_comps[s3]) {
                            if (std::find(union_c.begin(), union_c.end(), c) == union_c.end()) {
                                extra++;
                            }
                        }
                        if (union_c.size() + extra >= 5) {
                            std::vector<uint8_t> cand_mask = sol.active;
                            cand_mask[s1] = 1;
                            cand_mask[s2] = 1;
                            cand_mask[s3] = 1;
                            bias[s1] = -500;
                            bias[s2] = -500;
                            bias[s3] = -500;

                            CandidateTree biased_tree = build_canonical_tree(cand_mask, &bias);
                            bias[s1] = 0;
                            bias[s2] = 0;
                            bias[s3] = 0;

                            if (biased_tree < sol) {
                                sol = std::move(biased_tree);
                                any_improved = true;
                                rebuild_super_components();
                                break;
                            }
                        }
                    }
                }
            }
        }

        return any_improved;
    }

    bool vnd_steiner_elimination_and_swap(CandidateTree& sol) const {
        bool any_improved = false;

        std::vector<int> tree_deg(N + 1, 0);
        for (const auto& e : sol.edges) {
            tree_deg[e.u]++;
            tree_deg[e.v]++;
        }

        std::vector<int> branching_steiners;
        for (int u = 1; u <= N; ++u) {
            if (sol.active[u] && !g.is_terminal[u] && tree_deg[u] >= 3) {
                branching_steiners.push_back(u);
            }
        }

        for (int s : branching_steiners) {
            if (!sol.active[s]) continue;

            std::vector<uint8_t> cand_mask = sol.active;
            cand_mask[s] = 0;

            int added = 0;
            for (int nb_s : w1_adj[s]) {
                if (!cand_mask[nb_s] && !g.is_terminal[nb_s] && term_d1[nb_s] >= 2) {
                    cand_mask[nb_s] = 1;
                    if (++added >= 5) break;
                }
            }

            CandidateTree swap_cand = build_canonical_tree(cand_mask);
            if (swap_cand < sol) {
                sol = std::move(swap_cand);
                any_improved = true;
            }
        }

        return any_improved;
    }

    bool vnd_key_path_replacement(CandidateTree& sol) const {
        std::vector<std::vector<std::pair<int, int>>> t_adj(N + 1);
        std::vector<int> deg(N + 1, 0);
        for (size_t i = 0; i < sol.edges.size(); ++i) {
            const auto& e = sol.edges[i];
            t_adj[e.u].push_back({e.v, (int)i});
            t_adj[e.v].push_back({e.u, (int)i});
            deg[e.u]++;
            deg[e.v]++;
        }

        auto is_key_node = [&](int u) {
            return g.is_terminal[u] || deg[u] >= 3;
        };

        struct KeyPath {
            int u;
            int v;
            int weight;
            std::vector<int> internal_nodes;
            std::vector<int> edge_indices;
        };

        std::vector<KeyPath> key_paths;
        std::vector<uint8_t> edge_visited(sol.edges.size(), 0);

        for (int u = 1; u <= N; ++u) {
            if (!sol.active[u] || !is_key_node(u)) continue;

            for (const auto& [nxt, e_idx] : t_adj[u]) {
                if (edge_visited[e_idx]) continue;

                KeyPath kp;
                kp.u = u;
                kp.weight = sol.edges[e_idx].weight;
                kp.edge_indices.push_back(e_idx);
                edge_visited[e_idx] = 1;

                int curr = nxt;
                int prev = u;
                while (!is_key_node(curr)) {
                    kp.internal_nodes.push_back(curr);
                    int next_node = -1;
                    int next_edge = -1;
                    for (const auto& [w_node, w_idx] : t_adj[curr]) {
                        if (w_node != prev) {
                            next_node = w_node;
                            next_edge = w_idx;
                            break;
                        }
                    }
                    if (next_edge == -1) break;
                    edge_visited[next_edge] = 1;
                    kp.weight += sol.edges[next_edge].weight;
                    kp.edge_indices.push_back(next_edge);
                    prev = curr;
                    curr = next_node;
                }
                kp.v = curr;
                if (kp.weight >= 3) { // Caminos de peso 2 ya son expandidos por expand_w2_edges_to_w1_bridges
                    key_paths.push_back(std::move(kp));
                }
            }
        }

        if (key_paths.empty()) return false;

        std::sort(key_paths.begin(), key_paths.end(), [](const KeyPath& a, const KeyPath& b) {
            return a.weight > b.weight;
        });

        std::vector<int> comp(N + 1, 0);
        std::vector<int> dist(N + 1, GRAPH_INF);
        std::vector<int> parent(N + 1, -1);
        std::vector<int> q_bfs;
        q_bfs.reserve(N);

        for (const auto& kp : key_paths) {
            std::fill(comp.begin(), comp.end(), 0);
            int blocked_edge = kp.edge_indices.front();

            q_bfs.clear();
            q_bfs.push_back(kp.u);
            comp[kp.u] = 1;
            for (int x : kp.internal_nodes) comp[x] = -1;

            size_t head = 0;
            while (head < q_bfs.size()) {
                int curr = q_bfs[head++];
                for (const auto& [nxt, e_idx] : t_adj[curr]) {
                    if (e_idx == blocked_edge || comp[nxt] != 0) continue;
                    comp[nxt] = 1;
                    q_bfs.push_back(nxt);
                }
            }

            for (int u = 1; u <= N; ++u) {
                if (sol.active[u] && comp[u] == 0) {
                    comp[u] = 2;
                }
            }

            std::fill(dist.begin(), dist.end(), GRAPH_INF);
            std::fill(parent.begin(), parent.end(), -1);

            using PII = std::pair<int, int>;
            std::priority_queue<PII, std::vector<PII>, std::greater<PII>> pq;

            for (int u = 1; u <= N; ++u) {
                if (comp[u] == 1) {
                    dist[u] = 0;
                    pq.push({0, u});
                }
            }

            int best_target = -1;
            int best_dist = kp.weight;

            while (!pq.empty()) {
                auto [d, u] = pq.top();
                pq.pop();

                if (d >= best_dist) break;
                if (d > dist[u]) continue;

                if (comp[u] == 2) {
                    best_dist = d;
                    best_target = u;
                    break;
                }

                for (const auto& nb : fast_adj[u]) {
                    int v = nb.to;
                    int nd = d + nb.weight;
                    if (nd < best_dist && nd < dist[v]) {
                        dist[v] = nd;
                        parent[v] = u;
                        pq.push({nd, v});
                    }
                }
            }

            if (best_target != -1 && best_dist < kp.weight) {
                std::vector<uint8_t> cand_mask = sol.active;
                for (int x : kp.internal_nodes) {
                    if (!g.is_terminal[x]) cand_mask[x] = 0;
                }
                int curr = best_target;
                while (curr != -1) {
                    cand_mask[curr] = 1;
                    curr = parent[curr];
                }

                CandidateTree cand = build_canonical_tree(cand_mask);
                if (cand.cost < sol.cost) {
                    sol = std::move(cand);
                    return true;
                }
            }
        }

        return false;
    }

    void run_vnd(CandidateTree& sol, int max_passes = 6) const {
        for (int pass = 0; pass < max_passes; ++pass) {
            int start_cost = sol.cost;

            bool imp1 = vnd_star_insertion(sol);
            bool imp2 = vnd_steiner_elimination_and_swap(sol);
            bool imp3 = (!imp1 && !imp2) ? vnd_key_path_replacement(sol) : false;

            if (!imp1 && !imp2 && !imp3 && sol.cost == start_cost) {
                break;
            }
        }
    }

    // =========================================================================
    // FASE 4: Path Relinking Rápido con Muestreo de Orden de Saturación
    // =========================================================================
    CandidateTree path_relinking(const CandidateTree& s1, const CandidateTree& s2, std::mt19937& rng) const {
        CandidateTree best = (s1 < s2) ? s1 : s2;

        bool has_diff = false;
        std::vector<uint8_t> union_mask(N + 1, 0);
        for (int u = 1; u <= N; ++u) {
            if (s1.active[u] || s2.active[u]) union_mask[u] = 1;
            if (!g.is_terminal[u] && s1.active[u] != s2.active[u]) has_diff = true;
        }
        if (!has_diff) return best;

        std::vector<int> bias(N + 1, 0);
        CandidateTree best_merged;

        for (int trial = 0; trial < 16; ++trial) {
            for (int u = 1; u <= N; ++u) {
                if (union_mask[u] && !g.is_terminal[u]) {
                    if (trial == 0) {
                        bias[u] = (s1.active[u] && s2.active[u]) ? -250 : 0;
                    } else if (trial == 1) {
                        bias[u] = (s1.active[u] != s2.active[u]) ? -250 : 0;
                    } else {
                        bias[u] = (int)(rng() % 500) - ((s1.active[u] && s2.active[u]) ? 150 : 0);
                    }
                }
            }
            CandidateTree merged = build_canonical_tree(union_mask, &bias);
            if (merged < best_merged) {
                best_merged = std::move(merged);
            }
        }

        if (best_merged.cost <= best.cost + 2) {
            run_vnd(best_merged, 4);
            if (best_merged < best) {
                best = std::move(best_merged);
            }
        }

        return best;
    }
};

} // namespace

std::vector<Edge> Optimizer::constructive_heuristic(const Graph& g) {
    if (g.num_nodes <= 1 || g.terminals.empty()) return {};

    SteinerEngine engine(g);
    engine.presolve_and_dual_ascent();

    CandidateTree best = engine.voronoi_kmb();

    int step = std::max(1, g.num_terminals / 35);
    for (int i = 0; i < g.num_terminals; i += step) {
        CandidateTree sph_sol = engine.run_sph(g.terminals[i]);
        if (sph_sol < best) {
            best = std::move(sph_sol);
        }
    }

    return best.edges;
}

std::vector<Edge> Optimizer::local_search(const Graph& g, const std::vector<Edge>& initial_solution, int max_iterations) {
    if (initial_solution.empty() || g.num_nodes <= 1) return initial_solution;

    SteinerEngine engine(g);
    engine.presolve_and_dual_ascent();

    std::vector<uint8_t> mask(g.num_nodes + 1, 0);
    for (const auto& e : initial_solution) {
        mask[e.u] = 1;
        mask[e.v] = 1;
    }

    CandidateTree current = engine.build_canonical_tree(mask);
    engine.run_vnd(current, std::max(4, max_iterations / 50));
    return current.edges;
}

std::vector<Edge> Optimizer::optimize(const Graph& g, const OptimizerConfig& config, OptimizationStats* out_stats) {
    auto t_global_start = Clock::now();

    if (config.verbose) {
        std::cout << "[Optimizer] Iniciando pipeline completo hacia el mínimo lógico...\n";
    }

    // -------------------------------------------------------------------------
    // FASE 0: Presolve + Dual Ascent
    // -------------------------------------------------------------------------
    auto t0 = Clock::now();
    SteinerEngine engine(g);
    engine.presolve_and_dual_ascent();
    auto t1 = Clock::now();
    double presolve_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    if (config.verbose) {
        std::cout << "  [Fase 0 - Presolve & Dual Ascent] Aristas activas: " << engine.reduced_edges.size()
                  << " / " << g.edge_list.size()
                  << " | Cota Inferior Dual (LB): " << engine.best_dual_lb
                  << " (" << presolve_ms << " ms)\n";
    }

    // -------------------------------------------------------------------------
    // FASE 1: Heurística Constructiva (Voronoi KMB + Multi-Start SPH)
    // -------------------------------------------------------------------------
    t0 = Clock::now();
    CandidateTree kmb_sol = engine.voronoi_kmb();
    int kmb_cost = kmb_sol.cost;

    std::vector<CandidateTree> elite_pool;
    constexpr size_t MAX_ELITE = 18;

    auto add_to_elite = [&](const CandidateTree& cand) {
        for (const auto& el : elite_pool) {
            if (el.active == cand.active) {
                return;
            }
        }
        elite_pool.push_back(cand);
        std::sort(elite_pool.begin(), elite_pool.end());
        if (elite_pool.size() > MAX_ELITE) {
            elite_pool.pop_back();
        }
    };

    CandidateTree best_sol = kmb_sol;
    add_to_elite(kmb_sol);

    for (int i = 0; i < g.num_terminals; ++i) {
        auto now = Clock::now();
        double elapsed = std::chrono::duration<double, std::milli>(now - t_global_start).count();
        if (elapsed > config.max_time_ms * 0.20) break;

        CandidateTree sph_tree = engine.run_sph(g.terminals[i]);
        if (sph_tree < best_sol) {
            best_sol = sph_tree;
        }
        if (elite_pool.size() < MAX_ELITE || sph_tree < elite_pool.back()) {
            add_to_elite(sph_tree);
        }
    }

    t1 = Clock::now();
    double constructive_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    int multi_sph_cost = best_sol.cost;

    if (config.verbose) {
        std::cout << "  [Fase 1 - Constructiva] Costo Mehlhorn KMB: " << kmb_cost
                  << " | Costo Multi-SPH (" << g.num_terminals << " raíces): " << multi_sph_cost
                  << " (" << constructive_ms << " ms)\n";
    }

    // -------------------------------------------------------------------------
    // FASE 2 & 3: Búsqueda Local VND sobre el Pool Élite
    // -------------------------------------------------------------------------
    t0 = Clock::now();
    for (auto& el : elite_pool) {
        auto now = Clock::now();
        if (std::chrono::duration<double, std::milli>(now - t_global_start).count() > config.max_time_ms * 0.45) {
            break;
        }
        engine.run_vnd(el, 6);
        if (el < best_sol) {
            best_sol = el;
        }
    }
    std::sort(elite_pool.begin(), elite_pool.end());
    t1 = Clock::now();
    double vnd_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    int vnd_cost = best_sol.cost;

    if (config.verbose) {
        std::cout << "  [Fase 2 & 3 - VND (N1+N2+N3)] Costo reducido a: " << vnd_cost
                  << " | Nodos Steiner: " << best_sol.steiner_count
                  << " (" << vnd_ms << " ms)\n";
    }

    // -------------------------------------------------------------------------
    // FASE 4: Path Relinking Rápido + ILS Focalizado en Puntos Débiles
    // -------------------------------------------------------------------------
    std::mt19937 rng(1337);

    bool pr_improved = true;
    int pr_rounds = 0;
    while (pr_improved && pr_rounds < 3 && best_sol.cost > 564) {
        pr_improved = false;
        pr_rounds++;
        size_t lim = std::min<size_t>(10, elite_pool.size());
        for (size_t i = 0; i < lim; ++i) {
            for (size_t j = i + 1; j < lim; ++j) {
                auto now = Clock::now();
                if (std::chrono::duration<double, std::milli>(now - t_global_start).count() > config.max_time_ms * 0.60) {
                    break;
                }
                CandidateTree pr_sol = engine.path_relinking(elite_pool[i], elite_pool[j], rng);
                if (pr_sol.cost < best_sol.cost) {
                    best_sol = pr_sol;
                    pr_improved = true;
                    if (config.verbose) {
                        std::cout << "  [Fase 4 - Path Relinking] Nuevo mínimo alcanzado: Costo = " << best_sol.cost
                                  << " (Steiner = " << best_sol.steiner_count << ")\n";
                    }
                } else if (pr_sol < best_sol) {
                    best_sol = pr_sol;
                }
                add_to_elite(pr_sol);
                if (best_sol.cost <= 564) break;
            }
            if (best_sol.cost <= 564) break;
        }
    }

    std::vector<int> node_jitter(g.num_nodes + 1, 0);
    int max_ils_iters = std::min(config.max_iterations, 120);
    int no_improve_iters = 0;

    for (int iter = 0; iter < max_ils_iters && best_sol.cost > 564; ++iter) {
        auto now = Clock::now();
        double elapsed = std::chrono::duration<double, std::milli>(now - t_global_start).count();
        if (elapsed >= config.max_time_ms * 0.90 || no_improve_iters >= 40) break;

        const CandidateTree& base = elite_pool[iter % std::min<size_t>(8, elite_pool.size())];

        std::vector<uint8_t> seed_mask = base.active;
        std::vector<int> active_steiners;
        std::vector<int> tree_deg(g.num_nodes + 1, 0);
        for (const auto& e : base.edges) {
            tree_deg[e.u]++;
            tree_deg[e.v]++;
        }

        std::vector<int> weak_spots;
        for (int u = 1; u <= g.num_nodes; ++u) {
            if (seed_mask[u] && !g.is_terminal[u]) {
                active_steiners.push_back(u);
                if (tree_deg[u] == 2) weak_spots.push_back(u);
            }
            node_jitter[u] = (int)(rng() % 11) - 5;
        }
        for (const auto& e : base.edges) {
            if (e.weight >= 2) {
                weak_spots.push_back(e.u);
                weak_spots.push_back(e.v);
            }
        }

        if (iter % 2 == 0 && !weak_spots.empty()) {
            // Ruin & Recreate focalizado en una bola BFS alrededor de un punto débil (grado 2 o arista de peso >= 2)
            int center = weak_spots[rng() % weak_spots.size()];
            std::queue<int> q;
            std::vector<uint8_t> vis(g.num_nodes + 1, 0);
            q.push(center);
            vis[center] = 1;
            int dropped = 0;
            int target_drop = 6 + (iter % 12);
            while (!q.empty() && dropped < target_drop) {
                int curr = q.front();
                q.pop();
                if (seed_mask[curr] && !g.is_terminal[curr]) {
                    seed_mask[curr] = 0;
                    dropped++;
                }
                for (const auto& nb : engine.fast_adj[curr]) {
                    if (!vis[nb.to] && (seed_mask[nb.to] || nb.weight == 1)) {
                        vis[nb.to] = 1;
                        q.push(nb.to);
                    }
                }
            }
        } else if (!active_steiners.empty()) {
            int drop_count = std::max(4, (int)active_steiners.size() / (5 + (iter % 6)));
            for (int d = 0; d < drop_count; ++d) {
                int drop_u = active_steiners[rng() % active_steiners.size()];
                seed_mask[drop_u] = 0;
            }
        }

        int root_term = g.terminals[rng() % g.num_terminals];
        CandidateTree cand = engine.repair_forest_sph(seed_mask, root_term, &node_jitter);

        if (cand.cost < GRAPH_INF) {
            engine.run_vnd(cand, 4);
            if (cand.cost <= best_sol.cost + 3) {
                CandidateTree relinked = engine.path_relinking(cand, best_sol, rng);
                if (relinked < cand) cand = std::move(relinked);
            }

            if (cand.cost < best_sol.cost) {
                best_sol = cand;
                no_improve_iters = 0;
                if (config.verbose) {
                    std::cout << "  [Fase 4 - ILS Iter " << iter + 1 << "] Nuevo mínimo alcanzado: Costo = "
                              << best_sol.cost << " (Steiner = " << best_sol.steiner_count << ")\n";
                }
            } else {
                if (cand < best_sol) best_sol = cand;
                no_improve_iters++;
            }
            add_to_elite(cand);
        }
    }

    // Cierre final: eliminar cualquier puente Steiner de grado 2 que pueda sustituirse por arista directa sin subir el costo
    std::vector<int> final_deg(g.num_nodes + 1, 0);
    for (const auto& e : best_sol.edges) {
        final_deg[e.u]++;
        final_deg[e.v]++;
    }
    for (int u = 1; u <= g.num_nodes; ++u) {
        if (best_sol.active[u] && !g.is_terminal[u] && final_deg[u] == 2) {
            std::vector<uint8_t> compact_mask = best_sol.active;
            compact_mask[u] = 0;
            CandidateTree compact_cand = engine.build_canonical_tree(compact_mask);
            if (compact_cand.cost <= best_sol.cost && compact_cand.steiner_count < best_sol.steiner_count) {
                best_sol = std::move(compact_cand);
            }
        }
    }

    auto t_global_end = Clock::now();
    double total_ms = std::chrono::duration<double, std::milli>(t_global_end - t_global_start).count();

    if (out_stats) {
        out_stats->dual_lower_bound = engine.best_dual_lb;
        out_stats->kmb_cost = kmb_cost;
        out_stats->multi_sph_cost = multi_sph_cost;
        out_stats->vnd_cost = vnd_cost;
        out_stats->final_cost = best_sol.cost;
        out_stats->reduced_edges_count = (int)engine.reduced_edges.size();
        out_stats->presolve_ms = presolve_ms;
        out_stats->constructive_ms = constructive_ms;
        out_stats->vnd_ms = vnd_ms;
        out_stats->total_ms = total_ms;
    }

    if (config.verbose) {
        double gap = ((double)(best_sol.cost - 564) / 564.0) * 100.0;
        std::cout << "[Optimizer] Finalizado en " << total_ms << " ms | Costo Final: " << best_sol.cost
                  << " | Aristas: " << best_sol.edges.size()
                  << " | Nodos Steiner: " << best_sol.steiner_count
                  << " | Gap vs Óptimo (564): +" << gap << "%\n";
    }

    return best_sol.edges;
}
