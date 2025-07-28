import re
from collections import defaultdict
import matplotlib.pyplot as plt
import numpy as np
from itertools import product
from random import shuffle

color_idx = 0

def parse_aggregate_sent_per_source(filepath):
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
                source = int(source)
                sent = float(sent)
                source_sent[source].append(sent)
            else:
                print(f"Skipping malformed line in {filepath}:", line.strip())
    return [sum(vals) for vals in source_sent.values()]

def plot_cdf_aggregates(files):
    plt.figure(figsize=(10, 6))

    # Create many unique visual combinations
    colors = ['blue', 'green', 'red', 'orange', 'purple', 'cyan', 'brown', 'gray', 'pink', 'olive']
    markers = ['o', 's', '^', 'D', 'v', 'P', '*', 'x', '<', '>']

    # Use only color+marker combinations (not linestyle) to distinguish
    style_combinations = list(product(colors, markers))
    shuffle(style_combinations)
    style_map = {}

    for filepath in files:
        aggregates = parse_aggregate_sent_per_source(filepath)

        duration_match = re.search(r"(\d+)s\.txt$", filepath)
        if duration_match:
            seconds = int(duration_match.group(1))
            duration = f"{seconds}s"
            aggregates = [x / seconds for x in aggregates]
        else:
            print(f"Warning: couldn't detect duration in filename {filepath}. Assuming 1s.")
            duration = '1s'

        if not aggregates:
            print(f"No valid data in {filepath}, skipping.")
            continue

        # Get a unique style per *file*
        if filepath not in style_map:
            if not style_combinations:
                raise ValueError("Ran out of unique (color, marker) combinations!")
            style_map[filepath] = style_combinations.pop(0)

        color, marker = style_map[filepath]

        data = np.sort(aggregates)
        cdf = np.arange(1, len(data) + 1) / len(data)

        label = f"{filepath.split('/')[-1]}"

        plt.plot(
            data, cdf,
            label=label,
            color=color,
            linestyle='-',  # use same line style for all
            marker=marker,
            linewidth=1.2,
            markersize=4,
        )
        


    plt.xlabel("Aggregate Sent per Ground Station (Mbit/s)", fontsize=11)
    plt.ylabel("CDF", fontsize=11)
    plt.title("CDF of Aggregate Sent per Ground Station", fontsize=13)
    plt.legend(fontsize=9)
    plt.grid(True, linewidth=0.3)
    plt.yticks(np.arange(0, 1.01, 0.1))
    plt.tight_layout()
    plt.xticks(np.arange(0, 40, 2))
    plt.savefig("cdf_sent_GScomparison_rich.png", dpi=150)
    print("Saved plot as cdf_sent_GScomparison_rich.png")

    print("\nAssigned styles per file:")
    for file, (color, marker) in style_map.items():
        print(f"{file}: color={color}, marker={marker}")

# === Example Usage ===
filepaths = [
    "flows/competing_kept/tcp_flows_100f_rcache50s.txt",
    "flows/competing_kept/tcp_flows_100f_lcache50s.txt",
    "flows/competing_removed/tcp_flows_100f_dcache50s.txt",
    "flows/competing_removed/tcp_flows_100f_rcache50s.txt",
    "flows/competing_removed/tcp_flows_100f_lcache50s.txt",
    "flows/competing_removed/tcp_flows_100f_dcache50s.txt"
]
plot_cdf_aggregates(filepaths)
