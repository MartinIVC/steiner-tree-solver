#!/usr/bin/env python3
"""
scripts/plot_tree.py
Visualizador gráfico de árboles para el Problema del Árbol de Steiner (STPG).
Genera figuras en formato PNG y SVG en report/figures/ a partir de archivos en results/*.csv.
Distingue claramente entre Nodos Terminales (rojo/coral) y Nodos Steiner (azul/cian).
"""

import os
import sys
import csv
import math
from collections import defaultdict, deque

try:
    import cairo
    HAS_CAIRO = True
except ImportError:
    HAS_CAIRO = False


def load_terminals_from_stp(stp_path="data/e18.stp"):
    """Carga el conjunto de terminales desde el archivo .stp."""
    terminals = set()
    if not os.path.exists(stp_path):
        return terminals
    with open(stp_path, "r", encoding="utf-8") as f:
        for line in f:
            parts = line.strip().split()
            if not parts:
                continue
            if parts[0] == "T" and len(parts) >= 2:
                terminals.add(int(parts[1]))
    return terminals


def load_solution_csv(csv_path):
    """Carga aristas de una solución CSV: u,v,weight."""
    edges = []
    nodes = set()
    total_cost = 0
    with open(csv_path, "r", encoding="utf-8") as f:
        reader = csv.reader(f)
        header = next(reader, None)
        for row in reader:
            if len(row) < 3:
                continue
            try:
                u, v, w = int(row[0]), int(row[1]), int(row[2])
                edges.append((u, v, w))
                nodes.add(u)
                nodes.add(v)
                total_cost += w
            except ValueError:
                continue
    return edges, sorted(list(nodes)), total_cost


def compute_radial_layout(edges, nodes, width=1600, height=1600, margin=100):
    """
    Calcula una disposición radial en base al árbol.
    Encuentra el nodo central o raíz y ubica los nodos por niveles en coordenadas polares.
    """
    if not nodes:
        return {}
    adj = defaultdict(list)
    for u, v, w in edges:
        adj[u].append(v)
        adj[v].append(u)

    # Buscar nodo raíz con mayor grado o centro aproximado
    root = max(nodes, key=lambda n: len(adj[n]))

    # BFS para calcular niveles y padres
    level = {root: 0}
    parent = {root: None}
    children = defaultdict(list)
    q = deque([root])

    while q:
        curr = q.popleft()
        for nxt in adj[curr]:
            if nxt not in level:
                level[nxt] = level[curr] + 1
                parent[nxt] = curr
                children[curr].append(nxt)
                q.append(nxt)

    max_level = max(level.values()) if level else 1
    max_radius = (min(width, height) / 2.0) - margin

    # Asignar ángulos radiales
    angles = {}

    def assign_angles(node, angle_min, angle_max):
        angles[node] = (angle_min + angle_max) / 2.0
        node_children = children[node]
        if not node_children:
            return

        # Calcular pesos por tamaño de subárbol
        def subtree_size(n):
            count = 1
            for ch in children[n]:
                count += subtree_size(ch)
            return count

        sizes = [subtree_size(ch) for ch in node_children]
        total_sub = sum(sizes)
        curr_min = angle_min
        span = angle_max - angle_min

        for ch, sz in zip(node_children, sizes):
            ch_span = (sz / total_sub) * span
            assign_angles(ch, curr_min, curr_min + ch_span)
            curr_min += ch_span

    assign_angles(root, 0, 2 * math.pi)

    # Convertir a coordenadas cartesianas (x, y)
    center_x = width / 2.0
    center_y = height / 2.0
    positions = {}

    for node in nodes:
        r = (level.get(node, 0) / max(1, max_level)) * max_radius
        theta = angles.get(node, 0)
        x = center_x + r * math.cos(theta)
        y = center_y + r * math.sin(theta)
        positions[node] = (x, y)

    return positions


def render_with_cairo(csv_path, png_path, terminals, edges, nodes, total_cost, width=1600, height=1600):
    """Renderiza el árbol a PNG de alta resolución usando pycairo."""
    surface = cairo.ImageSurface(cairo.FORMAT_ARGB32, width, height)
    ctx = cairo.Context(surface)

    # Fondo oscuro profesional (estilo reporte moderno)
    ctx.set_source_rgb(0.08, 0.09, 0.12)
    ctx.paint()

    positions = compute_radial_layout(edges, nodes, width, height)
    if not positions:
        return False

    # Dibujar aristas
    ctx.set_line_width(1.0)
    ctx.set_source_rgba(0.4, 0.5, 0.65, 0.45)
    for u, v, w in edges:
        if u in positions and v in positions:
            x1, y1 = positions[u]
            x2, y2 = positions[v]
            ctx.move_to(x1, y1)
            ctx.line_to(x2, y2)
            ctx.stroke()

    # Contar nodos presentes
    num_terminals_present = sum(1 for n in nodes if n in terminals)
    num_steiner_present = len(nodes) - num_terminals_present

    # Dibujar nodos Steiner (capa inferior)
    steiner_radius = 2.5
    ctx.set_source_rgba(0.25, 0.75, 0.90, 0.85)  # Cyan brillante
    for n in nodes:
        if n not in terminals and n in positions:
            x, y = positions[n]
            ctx.arc(x, y, steiner_radius, 0, 2 * math.pi)
            ctx.fill()

    # Dibujar nodos Terminales (capa superior, destacados)
    terminal_radius = 5.0
    for n in nodes:
        if n in terminals and n in positions:
            x, y = positions[n]
            # Borde blanco fino
            ctx.set_source_rgba(1.0, 1.0, 1.0, 0.9)
            ctx.arc(x, y, terminal_radius + 1.2, 0, 2 * math.pi)
            ctx.fill()
            # Relleno rojo coral
            ctx.set_source_rgba(0.95, 0.25, 0.25, 0.95)
            ctx.arc(x, y, terminal_radius, 0, 2 * math.pi)
            ctx.fill()

    # Panel de Información y Leyenda (Esquina superior izquierda)
    base_name = os.path.basename(csv_path)
    ctx.select_font_face("Sans", cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_BOLD)

    # Encabezado
    ctx.set_font_size(24)
    ctx.set_source_rgb(0.95, 0.95, 0.98)
    ctx.move_to(40, 50)
    ctx.show_text(f"Visualización de Árbol: {base_name}")

    ctx.select_font_face("Sans", cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_NORMAL)
    ctx.set_font_size(16)
    ctx.set_source_rgb(0.75, 0.80, 0.85)
    ctx.move_to(40, 85)
    ctx.show_text(f"Costo Total: {total_cost} | Aristas: {len(edges)} | Nodos Totales: {len(nodes)}")

    # Leyenda: Terminales
    ctx.set_source_rgba(0.95, 0.25, 0.25, 1.0)
    ctx.arc(50, 120, 6.0, 0, 2 * math.pi)
    ctx.fill()
    ctx.set_source_rgb(0.90, 0.90, 0.95)
    ctx.move_to(65, 125)
    ctx.show_text(f"Terminales Obligatorios (|T|): {num_terminals_present} / {len(terminals)}")

    # Leyenda: Steiner
    ctx.set_source_rgba(0.25, 0.75, 0.90, 1.0)
    ctx.arc(50, 150, 4.0, 0, 2 * math.pi)
    ctx.fill()
    ctx.set_source_rgb(0.90, 0.90, 0.95)
    ctx.move_to(65, 155)
    ctx.show_text(f"Nodos Steiner Conectores (|S|): {num_steiner_present}")

    surface.write_to_png(png_path)
    return True


def render_svg(csv_path, svg_path, terminals, edges, nodes, total_cost, width=1600, height=1600):
    """Genera vector SVG escalable como formato complementario."""
    positions = compute_radial_layout(edges, nodes, width, height)
    if not positions:
        return False

    with open(svg_path, "w", encoding="utf-8") as f:
        f.write(f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">\n')
        f.write('<rect width="100%" height="100%" fill="#14171f"/>\n')

        # Aristas
        f.write('<g stroke="rgba(100, 130, 170, 0.45)" stroke-width="1.0">\n')
        for u, v, w in edges:
            if u in positions and v in positions:
                x1, y1 = positions[u]
                x2, y2 = positions[v]
                f.write(f'  <line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}"/>\n')
        f.write('</g>\n')

        # Nodos Steiner
        f.write('<g fill="#40bfff" opacity="0.85">\n')
        for n in nodes:
            if n not in terminals and n in positions:
                x, y = positions[n]
                f.write(f'  <circle cx="{x:.1f}" cy="{y:.1f}" r="2.5"/>\n')
        f.write('</g>\n')

        # Nodos Terminales
        f.write('<g fill="#f24040" stroke="#ffffff" stroke-width="1.2">\n')
        for n in nodes:
            if n in terminals and n in positions:
                x, y = positions[n]
                f.write(f'  <circle cx="{x:.1f}" cy="{y:.1f}" r="5.0"/>\n')
        f.write('</g>\n')

        # Texto informativo
        base = os.path.basename(csv_path)
        f.write(f'<text x="40" y="50" font-family="sans-serif" font-size="24" font-weight="bold" fill="#ffffff">{base}</text>\n')
        f.write(f'<text x="40" y="85" font-family="sans-serif" font-size="16" fill="#cccccc">Costo: {total_cost} | Aristas: {len(edges)} | Nodos: {len(nodes)}</text>\n')
        f.write('</svg>\n')

    return True


def main():
    terminals = load_terminals_from_stp("data/e18.stp")
    print(f"[Plotter] Cargados {len(terminals)} nodos terminales de referencia.")

    os.makedirs("report/figures", exist_ok=True)

    target_files = []
    if len(sys.argv) > 1:
        target_files = sys.argv[1:]
    else:
        # Procesar todos los CSV en results/
        if os.path.exists("results"):
            for f in sorted(os.listdir("results")):
                if f.endswith(".csv"):
                    target_files.append(os.path.join("results", f))

    if not target_files:
        print("[Plotter] No se encontraron archivos CSV de solución en results/.")
        return 0

    for csv_file in target_files:
        if not os.path.exists(csv_file):
            print(f"[Plotter] Archivo no encontrado: {csv_file}")
            continue

        base_name = os.path.splitext(os.path.basename(csv_file))[0]
        edges, nodes, total_cost = load_solution_csv(csv_file)
        if not edges:
            print(f"[Plotter] Solución vacía o inválida en {csv_file}")
            continue

        png_path = os.path.join("report/figures", f"{base_name}.png")
        svg_path = os.path.join("report/figures", f"{base_name}.svg")

        print(f"[Plotter] Procesando {csv_file} (Costo: {total_cost}, Nodos: {len(nodes)}, Aristas: {len(edges)})...")

        # Generar SVG
        render_svg(csv_file, svg_path, terminals, edges, nodes, total_cost)
        print(f"  -> SVG generado en: {svg_path}")

        # Generar PNG si pycairo está disponible
        if HAS_CAIRO:
            render_with_cairo(csv_file, png_path, terminals, edges, nodes, total_cost)
            print(f"  -> PNG generado en: {png_path}")

    print("[Plotter] Generación de figuras completada exitosamente.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
