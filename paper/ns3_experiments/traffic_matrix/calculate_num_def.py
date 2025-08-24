import os
from collections import defaultdict
import copy

S = 50
step = 100_000_000 
total_ns = int(S * 1e9) - step
path = "../../satellite_networks_state/gen_data_lasthop_splitting/kuiper_630_isls_plus_grid_ground_stations_top_100_algorithm_free_gs_one_sat_uplink_downlink_only_over_isls_lasthop_splitting/dynamic_state_100ms_for_50s"


src_node = 1161
dst_node = 1157

# track results over time
deflections_possible = {}         # raw reachability counts         [time]
connectedness = {}  # how connected the nodes are from the original [time]

# persistent routing table that carries forward between timesteps
sf_path = {} # [time][path]

# [time][src node][next hop options] of type (dict(dict(list)))
def_options = defaultdict( 
    lambda: defaultdict(list)
)
debug_list = set([])

def getInfo():
    #first, move this into more readable way
    for t_ns in range(0, total_ns + 1, step):
        
        filename = os.path.join(path, f"fstate_{t_ns}.txt")

        if not os.path.exists(filename):
            print(f"Skipping missing file: {filename}")
        else:
            if (t_ns > 0):
                def_options[t_ns] = copy.deepcopy(def_options[t_ns - step])

            with open(filename, "r") as f:
                for line in f:
                    parts = line.strip().split(",")
                    if len(parts) < 5 or int(parts[1]) != dst_node:
                        continue
                    src = int(parts[0])
                    dst = int(parts[1])
                    length = len(parts)
                    options = int((length - 2) / 3)
                    if (src in debug_list):
                        print(t_ns, ":\t", parts)

                    
                    def_options[t_ns][src].clear()
                    for i in range(options):
                        def_options[t_ns][src].append(int(parts[(i*3) + 2]) )
                        
                        
                    if (t_ns == -1) or src == 1160:
                        print(t_ns, "\t:\t", parts)
                        continue


def getPath(time):
    curr_hop = src_node

    path = [curr_hop]

    while curr_hop != dst_node:
        next_hop_options = def_options[time][curr_hop]
        if (len(next_hop_options) < 1):
            sf_path[time] = path
            curr_hop = dst_node
        else:
            next_hop = next_hop_options[0]
            path.append(next_hop)
            curr_hop = next_hop
    sf_path[time] = path
    if len(path) > 0:
        return 0
    else:
        return -1
    
def getDeflections(time):
    if len(sf_path[time]) < 2 or sf_path[time][len(sf_path[time]) - 1] != dst_node:
        deflections_possible[time] = 0
        connectedness[time] = 0
        
    last_hop = sf_path[time][len(sf_path[time]) - 2]
    to_visit = set([last_hop])
    visited = set()
    connections = []
    while len(to_visit) > 0:
        deflection = to_visit.pop()
        visited.add(deflection)
        options = def_options[time][deflection]
        n_connections = 0
        for option in options:
            if option == dst_node:
                continue
            elif option in to_visit or option in visited:
                n_connections += 1
                continue
            else:
                n_connections += 1
                to_visit.add(option)
                
        connections.append(n_connections)
    print(connections)
        
    deflections_possible[time] = len(visited)
    connectedness[time] = sum(connections) / deflections_possible[time]


def compress_time_series(data, step, total_ns):
    times = sorted(data.keys())
    if not times:
        return []

    result = []
    start_time = times[0]
    prev_value = data[start_time]

    for t in times[1:]:
        if data[t] != prev_value:  # value changed
            result.append((start_time, t - step, prev_value))
            start_time = t
            prev_value = data[t]

    result.append((start_time, times[-1], prev_value))
    return result

def print_ranges(ranges, label="value"):
    width = max(len(f"{t}") for r in ranges for t in r[:2])

    print(f"{'Start':>{width}} – {'End':<{width}}    {label}")
    print("-" * (width * 2 + 12))

    for start, end, val in ranges:
        print(f"{start:>{width}} – {end:<{width}}    {label}={val}")

getInfo()

for t_ns in range(0, total_ns + 1, step):
    getPath(t_ns)

for t_ns in range(0, total_ns + 1, step):
    getDeflections(t_ns)


# print("\nPaths: ")
# for t_ns in range(0, total_ns + 1, step):
#     print(t_ns, "\t:\t", sf_path[t_ns])

compressed_defl = compress_time_series(deflections_possible, step, total_ns)
compressed_conn = compress_time_series(connectedness, step, total_ns)
print()
print_ranges(compressed_defl, label="# deflections")
print()
print_ranges(compressed_conn, label="avg connectivity")





                



