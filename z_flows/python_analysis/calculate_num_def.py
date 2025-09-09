import os
from collections import defaultdict
import copy

def calculate_deflections(path, src_node, dst_node, S=50, step=100_000_000):
    total_ns = int(S * 1e9) - step
    deflections_possible = {}
    connectedness = {}
    sf_path = {}
    def_options = defaultdict(lambda: defaultdict(list))
    #print("STEP 1")
    # Build def_options
    for t_ns in range(0, total_ns + 1, step):
        filename = os.path.join(path, f"fstate_{t_ns}.txt")
        if not os.path.exists(filename):
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
                options = (len(parts) - 2) // 3
                def_options[t_ns][src].clear()
                for i in range(options):
                    def_options[t_ns][src].append(int(parts[i*3 + 2]))
    #print("STEP 2")
    # Build paths
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
    #print("STEP 3")
    # Compute deflections
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
            n_connections = 0
            for option in def_options[t_ns][node]:
                if option == dst_node:
                    continue
                n_connections += 1
                if option not in visited and option not in to_visit:
                    to_visit.add(option)
            connections.append(n_connections)
        deflections_possible[t_ns] = len(visited)
        connectedness[t_ns] = sum(connections) / len(visited) if visited else 0
    #print("STEP 4")
    return sf_path, deflections_possible, connectedness
