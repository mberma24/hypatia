import os
from collections import defaultdict
import copy
import random

def calculate_deflections(
    path,
    src_node,
    dst_node,
    S=50,
    step=100_000_000
):
    total_ns = int(S * 1e9) - step

    # Initialize dictionaries for every timestep
    deflections_possible = {t_ns: 0 for t_ns in range(0, total_ns + 1, step)}
    connectedness = {t_ns: 0.0 for t_ns in range(0, total_ns + 1, step)}
    sf_path = {}  # persistent path over time

    def_options = defaultdict(lambda: defaultdict(list))
    debug_list = set([])

    # -----------------------------
    # Read fstate files and build def_options
    # -----------------------------
    for t_ns in range(0, total_ns + 1, step):
        filename = os.path.join(path, f"fstate_{t_ns}.txt")

        if not os.path.exists(filename):
            print(f"Skipping missing file: {filename}")
            # carry forward previous timestep if exists
            if t_ns > 0:
                def_options[t_ns] = copy.deepcopy(def_options[t_ns - step])
            continue

        if t_ns > 0:
            def_options[t_ns] = copy.deepcopy(def_options[t_ns - step])

        with open(filename, "r") as f:
            for line in f:
                parts = line.strip().split(",")
                if len(parts) < 5 or int(parts[1]) != dst_node:
                    continue

                src = int(parts[0])
                length = len(parts)
                options = int((length - 2) / 3)

                def_options[t_ns][src].clear()
                for i in range(options):
                    def_options[t_ns][src].append(int(parts[i*3 + 2]))

    # -----------------------------
    # Build shortest paths
    # -----------------------------
    for t_ns in range(0, total_ns + 1, step):
        curr_hop = src_node
        path_list = [curr_hop]

        while curr_hop != dst_node:
            next_hop_options = def_options[t_ns][curr_hop]
            if not next_hop_options:
                curr_hop = dst_node
            else:
                curr_hop = next_hop_options[0]
                path_list.append(curr_hop)

        sf_path[t_ns] = path_list

    # -----------------------------
    # Compute deflections & connectivity
    # -----------------------------
    for t_ns in range(0, total_ns + 1, step):
        path_list = sf_path[t_ns]
        if len(path_list) < 2 or path_list[-1] != dst_node:
            deflections_possible[t_ns] = 0
            connectedness[t_ns] = 0
            continue

        last_hop = path_list[-2]
        to_visit = set([last_hop])
        visited = set()
        connections = []

        while to_visit:
            node = to_visit.pop()
            visited.add(node)
            options = def_options[t_ns][node]
            n_connections = 0

            for option in options:
                if option == dst_node:
                    continue
                elif option in to_visit or option in visited:
                    n_connections += 1
                else:
                    n_connections += 1
                    to_visit.add(option)
            connections.append(n_connections)

        # assign to dictionaries that already have all timesteps
        deflections_possible[t_ns] = len(visited)
        connectedness[t_ns] = sum(connections) / len(visited) if visited else 0

    return sf_path, deflections_possible, connectedness


# -----------------------------
# Optional: run directly
# -----------------------------
if __name__ == "__main__":
    # Example usage
    nodes = range(1156, 1256)
    src_node = random.choice(nodes)
    dst_node = random.choice([n for n in nodes if n != src_node])
    print(src_node, dst_node)
    path = "../../satellite_networks_state/gen_data_lasthop_splitting/kuiper_630_isls_plus_grid_ground_stations_top_100_algorithm_free_gs_one_sat_uplink_downlink_only_over_isls_lasthop_splitting/dynamic_state_100ms_for_50s"

    sf_path, deflections_possible, connectedness = calculate_deflections(path, src_node, dst_node)

    # Print summary
    print("Time (ns)\t# deflections\tavg connectivity")
    for t in sorted(deflections_possible.keys()):
        print(f"{t}\t{deflections_possible[t]}\t{connectedness[t]:.2f}")
