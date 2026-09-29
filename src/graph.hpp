#pragma once

#include <vector>
#include <string>
#include <iostream>

// Estructura para representar una arista
struct Edge {
    int u;          // Nodo origen (1 a N)
    int v;          // Nodo destino (1 a N)
    int weight;     // Peso / Costo de la arista

    bool operator<(const Edge& other) const {
        return weight < other.weight;
    }
};

// Estructura para la lista de adyacencia
struct Neighbor {
    int to;         // Nodo vecino (1 a N)
    int weight;     // Peso de la arista
};

class Graph {
public:
    int num_nodes;                          // Total de nodos (|V| = 2500 para e18)
    int num_edges;                          // Total de aristas (|E| = 62500 para e18)
    int num_terminals;                      // Total de nodos terminales (|T| = 417 para e18)

    std::vector<Edge> edge_list;            // Lista completa de aristas
    std::vector<std::vector<Neighbor>> adj; // Lista de adyacencia (1-indexed: tamaño num_nodes + 1)

    std::vector<int> terminals;             // Lista con los IDs de los terminales
    std::vector<bool> is_terminal;          // Búsqueda O(1) de terminal (1-indexed: tamaño num_nodes + 1)

    Graph();

    // Carga la instancia desde un archivo SteinLib (.stp)
    // Limpia retornos de carro \r para compatibilidad Windows/Linux
    bool load_from_stp(const std::string& filepath);

    // Agrega una arista no dirigida
    void add_edge(int u, int v, int weight);

    // Muestra en consola un resumen de la información del grafo cargado
    void print_summary() const;
};
