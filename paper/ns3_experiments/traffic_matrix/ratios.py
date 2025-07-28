import re
from collections import defaultdict
import numpy as np
import os


def parse_aggregate_sent_per_source_dict(filepath):
    source_sent = defaultdict(list)
    duration = None
    method = None
    num_flows = None

    with open(filepath) as f:
        lines = f.readlines()

    # REMOVED Infer duration from any line ending in something like "50s"

    # Infer method and flow count from filename
    filename = os.path.basename(filepath).lower()

    # First try to infer duration from a line ending with "Xs"
    for line in reversed(lines):
        match = re.search(r"(\d+)s\s*$", line.strip())
        if match:
            duration = int(match.group(1))
            break

    # Fallback: try to infer duration from filename like "50s"
    if duration is None:
        filename_match = re.search(r"(\d+)s", filename)
        if filename_match:
            duration = int(filename_match.group(1))

    if duration is None:
        raise ValueError(f"Duration not found in {filepath}")


    # Method
    if "sf" in filename:
        method = "sf"
    elif "ddef" in filename:
        method = "ddef"
    elif "lcache" in filename:
        method = "lcache"
    elif "rcache" in filename:
        method = "rcache"
    elif "dcache" in filename:
        method = "dcache"
    else:
        method = "unknown"

    # Flow count
    flow_match = re.search(r"(\d+)f", filename)
    if flow_match:
        num_flows = int(flow_match.group(1))
    else:
        num_flows = "?"

    for line in lines:
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

    rate_per_source = {src: sum(vals) / duration for src, vals in source_sent.items()}
    label = f"{method} ({num_flows}f)"
    return label, rate_per_source


def compare_all_nodes(filepaths):
    data_by_label = {}

    for filepath in filepaths:
        label, rate_dict = parse_aggregate_sent_per_source_dict(filepath)
        data_by_label[label] = data_by_label.get(label, {})
        for src, rate in rate_dict.items():
            if src not in data_by_label[label]:
                data_by_label[label][src] = []
            data_by_label[label][src].append(rate)

    # Print comparison table
    print(f"{'Source':<8}", end="\t")
    labels = sorted(data_by_label.keys())
    for label in labels:
        print(f"{label:^20}", end="")
    print()

    all_sources = set()
    for method_data in data_by_label.values():
        all_sources.update(method_data.keys())

    for src in sorted(all_sources):
        print(f"{src:<8}", end="")
        for label in labels:
            rate = np.mean(data_by_label[label].get(src, [0.0]))
            print(f"{rate:>20.2f}", end="")
        print()

    print("\n" + "="*60)
    print("Per-method statistics (across all sources)\n")

    for label in labels:
        all_rates = []
        for src_rates in data_by_label[label].values():
            all_rates.extend(src_rates)

        if not all_rates:
            continue

        avg = np.mean(all_rates)
        mx = np.max(all_rates)
        mn = np.min(all_rates)
        std = np.std(all_rates)

        print(f"--- Stats for {label} ---")
        print(f"Avg: {avg:.2f} Mbit/s")
        print(f"Max: {mx:.2f} Mbit/s")
        print(f"Min: {mn:.2f} Mbit/s")
        print(f"Std: {std:.2f} Mbit/s\n")


# === Example usage ===
if __name__ == "__main__":
    filepaths = [
        "flows/competing_kept/tcp_flows_100f_rcache50s.txt",
        "flows/competing_kept/tcp_flows_100f_lcache50s.txt",
    ]
    compare_all_nodes(filepaths)




