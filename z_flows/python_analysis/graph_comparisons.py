import os
import re
import random
from collections import defaultdict
import numpy as np
import matplotlib.pyplot as plt
from calculate_num_def import calculate_deflections
from random import shuffle

# -----------------------------
# Parameters
# -----------------------------
# Traffic-matrix
random.seed(123456789)
random.randint(0, 100000000)  # Legacy reasons
seed_from_to = random.randint(0, 100000000)
a = set(range(1156, 1256))
pairs = []
available = [] * 100

for i in range(1156, 1256):
    available.append(i)

random.shuffle(available)
count = 0
while count < 50:
    src = available.pop()
    dst = available.pop()
    pairs.append((src, dst))
    count += 1

print(pairs)

fstate_path = "/home/mberma24/hypatia/paper/satellite_networks_state/gen_data_lasthop_deflection/kuiper_630_isls_plus_grid_ground_stations_top_100_algorithm_free_gs_one_sat_uplink_downlink_only_over_isls_lasthop_splitting/dynamic_state_100ms_for_200s"
S = 50

# Experimental folders
base_dir = "/home/mberma24/hypatia/z_flows/traffic_matrix/kuiper/123456789/"
path_list = [
    "Cache",
    "Cache_No_Deflection_Limit",
    "Cache_No_Poison",
    "Cache_No_TTL_Sensitivity",
    "Cache_No_Weights",
    "All_Disabled",
    "Hash",
    "One_Hop",
    "SF",
]
real_path_list = [os.path.join(base_dir, p) for p in path_list if os.path.isdir(os.path.join(base_dir, p))]

# -----------------------------
# Helper Functions
# -----------------------------
def generate_random_color():
    return (random.random(), random.random(), random.random())

def parse_aggregate_sent_per_source(filepath, duration=S):
    source_sent = defaultdict(list)
    with open(filepath) as f:
        for line in f:
            if line.strip().startswith("TCP") or not line.strip():
                continue
            match = re.match(
                r"^\s*(\d+)\s+(\d+)\s+(\d+)\s+[\d.]+\s+Mbit\s+\d+\s+\d+\s+[\d.]+\s+ms\s+([\d.]+)\s+Mbit", line
            )
            if match:
                _, source, _, sent = match.groups()
                source_sent[int(source)].append(float(sent))
    return [sum(vals)/duration for vals in source_sent.values()]

# -----------------------------
# Compute Optimal Throughput CDF
# -----------------------------
optimal_throughput_list = []
count = 0
for src, dst in pairs:
    count += 1
    print(count / 50 * 100, "%")
    _, deflections_possible, _ = calculate_deflections(fstate_path, src, dst, S=S)
    
    # Use average over time instead of last timestep
    defl_values = list(deflections_possible.values())
    if defl_values:
        avg_defl = (sum(defl_values) + 1) / len(defl_values)
        optimal = min(avg_defl, 4) * 10
    else:
        optimal = 0
    optimal_throughput_list.append(optimal)

opt_data = np.sort(optimal_throughput_list)
opt_cdf = np.arange(1, len(opt_data)+1) / len(opt_data)

# -----------------------------
# Plot
# -----------------------------
plt.figure(figsize=(10,6))
marker_list = ["o", "v", "^", "<", ">", "1", "2", "3", "4", "8", "s", "p", "P", "*", "h", "H", "+", "x", "X", "D", "d"]
shuffle(marker_list)

# Plot traffic-matrix optimal throughput CDF
plt.plot(opt_data, opt_cdf, marker='o', linestyle='-', color='tab:red', label="Optimal", linewidth=1.5)

# Plot aggregate sent CDF for each experiment folder
for dir_path in real_path_list:
    tcp_file = os.path.join(dir_path, "tcp_flows.txt")
    if not os.path.exists(tcp_file):
        continue
    agg = parse_aggregate_sent_per_source(tcp_file, duration=S)
    if not agg:
        continue
    data = np.sort(agg)
    cdf = np.arange(1, len(data)+1) / len(data)
    color = generate_random_color()
    marker = marker_list.pop() if marker_list else 'o'
    label = os.path.basename(dir_path)
    plt.plot(data, cdf, marker=marker, linestyle='-', color=color, linewidth=1.2, markersize=4, label=label)

# Labels and legend
plt.xlabel("Throughput / Aggregate Sent (Mbit/s)")
plt.ylabel("CDF")
plt.title("CDF: Optimal Throughput vs Aggregate Sent per Ground Station")
plt.ylim(0, 1)
plt.yticks(np.arange(0, 1.1, 0.1))
plt.grid(True, linewidth=0.3)
plt.legend(fontsize=9)
plt.tight_layout()

# Save combined plot
plt.savefig("../graph_data/comparisons_50s.png", dpi=150)
plt.close()
print("✅ Saved combined CDF plot as combined_cdf_optimal_vs_sent.png")
