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
INCLUDE_OPTIMAL = False   # Toggle: include the "Optimal" bar in the graph or not

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
    "Hash",
    "One_Hop",
    #"SF",
]
real_path_list = [os.path.join(base_dir, p) for p in path_list if os.path.isdir(os.path.join(base_dir, p))]

# -----------------------------
# Helper Functions
# -----------------------------
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
# Compute Optimal Throughput Stats
# -----------------------------
optimal_throughput_list = []
if INCLUDE_OPTIMAL:
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

# -----------------------------
# Collect Results (mean + std)
# -----------------------------
results = {}

# Optimal
if INCLUDE_OPTIMAL:
    opt_mean = np.mean(optimal_throughput_list)
    opt_std = np.std(optimal_throughput_list)
    results["Optimal"] = (opt_mean, opt_std)

# Experimental folders
for dir_path in real_path_list:
    tcp_file = os.path.join(dir_path, "tcp_flows.txt")
    if not os.path.exists(tcp_file):
        continue
    agg = parse_aggregate_sent_per_source(tcp_file, duration=S)
    if not agg:
        continue
    mean_val = np.mean(agg)
    std_val = np.std(agg)
    label = os.path.basename(dir_path)
    results[label] = (mean_val, std_val)

# -----------------------------
# Plot Bar Graph
# -----------------------------
labels = list(results.keys())
means = [results[k][0] for k in labels]
print(means)
stds = [results[k][1] for k in labels]

plt.figure(figsize=(10,6))
x_pos = np.arange(len(labels))

bars = plt.bar(x_pos, means, yerr=stds, capsize=5, alpha=0.8)

plt.xticks(x_pos, labels, rotation=30, ha="right")
plt.ylabel("Throughput / Aggregate Sent (Mbit/s)")
plt.title("Average Throughput with Std Dev (50 pairs)")
plt.grid(axis="y", linewidth=0.3)

# Annotate bar values
for i, bar in enumerate(bars):
    height = bar.get_height()
    plt.text(bar.get_x() + bar.get_width()/2.0, height + stds[i] + 0.5,
             f"{means[i]:.1f}", ha='center', va='bottom', fontsize=8)

plt.tight_layout()
plt.savefig("../graph_data/comparisons_50s_bar.png", dpi=150)
plt.close()

print("✅ Saved bar graph as comparisons_50s_bar.png")
