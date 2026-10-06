#pragma once

#include <vector>
#include <string>
#include <iostream>

// Constante para distancias infinitas o nodos inalcanzables en algoritmos de caminos mínimos
constexpr int GRAPH_INF = 1000000000;

// Estructura para representar una arista
struct Edge {
    int u;          // Nodo origen (1 a N)
    int v;          // Nodo destino (1 a N)
    int weight;     // Peso / Costo de la arista

    bool operator<(const Edge& other) const {
        if (weight != other.weight) return weight < other.weight;
        if (u != other.u) return u < other.u;
        return v < other.v;
    }
};

// Estructura para la lista de adyacencia
struct Neighbor {
    int to;         // Nodo vecino (1 a N)
    int weight;     // Peso de la arista
};

// Estructura que almacena la clausura métrica completa de los terminales
struct TerminalMetricClosure {
    // Matriz de distancias mínimas: dist[t_i][node]
    // indexada por índice de terminal (0 a num_terminals - 1)
    std::vector<std::vector<int>> dist;

    // Matriz de predecesores: parent[t_i][node]
    // para reconstruir caminos hacia cualquier nodo desde el terminal t_i
    std::vector<std::vector<int>> parent;

    // Mapeo rápido: id de nodo -> índice en vector terminals (-1 si no es terminal)
    std::vector<int> terminal_index_map;
};

class Graph {
public:
    static constexpr int INF = GRAPH_INF;

    int num_nodes;                          // Total de nodos (|V| = 2500 para e18)
    int num_edges;                          // Total de aristas (|E| = 62500 para e18)
    int num_terminals;                      // Total de nodos terminales (|T| = 417 para e18)

    std::vector<Edge> edge_list;            // Lista completa de aristas
    std::vector<std::vector<Neighbor>> adj; // Lista de adyacencia (1-indexed: tamaño num_nodes + 1)

    std::vector<int> terminals;             // Lista con los IDs de los terminales
    std::vector<bool> is_terminal;          // Búsqueda O(1) de terminal (1-indexed: tamaño num_nodes + 1)

    Graph();

    // Limpia y reinicia el estado interno del grafo
    void clear();

    // Carga la instancia desde un archivo SteinLib (.stp)
    // Incluye verificación robusta de apertura de archivo, archivo no vacío,
    // validación de formato por secciones e integridad de rangos de nodos y aristas.
    bool load_from_stp(const std::string& filepath);

    // Agrega una arista no dirigida
    void add_edge(int u, int v, int weight);

    // Muestra en consola un resumen de la información del grafo cargado
    void print_summary() const;

    // =========================================================================
    // Funciones de utilidad para Nicolas Montecino (Optimizador y Heurísticas)
    // =========================================================================

    // Comprueba en O(1) si un identificador de nodo pertenece al rango válido [1, num_nodes]
    inline bool is_valid_node(int u) const {
        return u >= 1 && u <= num_nodes;
    }

    // Comprueba en O(1) si un nodo es un terminal obligatorio
    inline bool is_terminal_node(int u) const {
        return is_valid_node(u) && is_terminal[u];
    }

    // Comprueba en O(1) si un nodo es un nodo Steiner (nodo auxiliar opcional que no es terminal)
    inline bool is_steiner_node(int u) const {
        return is_valid_node(u) && !is_terminal[u];
    }

    // Retorna la lista con todos los identificadores de nodos Steiner (|S| = |V| - |T|)
    // Facilita la exploración e iteración de candidatos en búsqueda local / metaheurística
    std::vector<int> get_steiner_nodes() const;

    // Retorna el grado (número de vecinos) de un nodo en el grafo original (0 si el nodo es inválido)
    int get_degree(int u) const;

    // Retorna los vecinos de un nodo de forma segura (vector vacío si el nodo es inválido)
    const std::vector<Neighbor>& get_neighbors(int u) const;

    // Comprueba si existe una arista directa entre u y v
    bool has_edge(int u, int v) const;

    // Retorna el peso de la arista directa entre u y v (-1 si no existe arista)
    int get_edge_weight(int u, int v) const;

    // Retorna la suma total de los pesos de todas las aristas del grafo original
    long long get_total_weight() const;

    // Ejecuta Dijkstra desde start_node calculando distancias mínimas (dist) y predecesores (parent)
    // dist y parent se inicializan en base 1 de tamaño num_nodes + 1
    // Complejidad O((|E| + |V|) log |V|)
    bool dijkstra(int start_node, std::vector<int>& dist, std::vector<int>& parent) const;

    // Reconstruye la secuencia de nodos en el camino más corto entre start_node y target_node
    // a partir del vector parent obtenido previamente con dijkstra. Retorna vector vacío si inalcanzable.
    std::vector<int> get_shortest_path_nodes(int start_node, int target_node, const std::vector<int>& parent) const;

    // Reconstruye la secuencia de aristas (Edge) en el camino más corto entre start_node y target_node
    // Fundamental para la heurística constructiva KMB al transformar aristas métricas en caminos reales
    std::vector<Edge> get_shortest_path_edges(int start_node, int target_node, const std::vector<int>& parent) const;

    // Retorna la distancia mínima directa entre dos nodos ejecutando Dijkstra puntual (INF si inalcanzable)
    int get_shortest_distance(int start_node, int target_node) const;

    // Calcula las distancias mínimas y predecesores desde TODOS los terminales (417 Dijkstras)
    // Complejidad temporal: O(|T| * (|E| + |V| log |V|)) (~300 - 500 ms en e18)
    TerminalMetricClosure compute_terminal_metric_closure() const;

    // Genera la lista completa de aristas métricas entre todos los pares de terminales (|T| * (|T|-1) / 2)
    std::vector<Edge> get_terminal_metric_edges(const TerminalMetricClosure& closure) const;

    // Reconstruye el camino en aristas del grafo original entre dos terminales (o terminal y nodo) usando la clausura métrica
    std::vector<Edge> get_metric_path_edges(int terminal_u, int node_v, const TerminalMetricClosure& closure) const;
};
