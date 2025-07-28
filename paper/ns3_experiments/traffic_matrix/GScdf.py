import re
from collections import defaultdict
import matplotlib.pyplot as plt
import numpy as np

def parse_aggregate_sent_per_source(filepath):
    source_sent = defaultdict(list)

    with open(filepath) as f:
        for line in f:
            if line.strip().startswith("TCP") or not line.strip():
                continue  # skip header or empty lines

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
    
    # Compute aggregate sent per ground station (source)
    aggregates = [sum(vals) for vals in source_sent.values()]
    return aggregates

def parse_avg_rate_per_flow(filepath):
    avg_rates = []

    pattern = re.compile(
        r"\s*(\d+)\s+(\d+)\s+(\d+)\s+[\d.eE+-]+\s+Mbit\s+\d+\s+\d+\s+[\d.eE+-]+\s+ms\s+[\d.eE+-]+\s+Mbit\s+[\d.]+%\s+([\d.eE+-]+)\s+Mbit/s"
    )

    with open(filepath) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("TCP") or "Source" in line:
                continue  # skip headers and empty lines

            match = pattern.match(line)
            if match:
                rate = float(match.group(4))  # average rate (Mbit/s)
                avg_rates.append(rate)
            else:
                print(f"Skipping malformed line in {filepath}:\n{line}")

    return avg_rates

def plot_cdf_aggregates(files):
    plt.figure(figsize=(10, 6))

    # Define visual styles
    type_colors = {
        'sf': 'blue',
        'ddef': 'green',
        'cache': 'red',
    }

    duration_styles = {
        '1s':  {'linestyle': '-',  'marker': 'o'},
        '5s':  {'linestyle': '--', 'marker': 's'},
        '10s': {'linestyle': '-.', 'marker': '^'},
        '50s': {'linestyle': ':',  'marker': 'd'}
    }


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

        # Identify type
        if 'sf' in filepath:
            flow_type = 'sf'
        elif 'ddef' in filepath:
            flow_type = 'ddef'
        elif 'cache' in filepath:
            flow_type = 'cache'
        else:
            flow_type = 'unknown'

        color = type_colors.get(flow_type, 'black')
        style = duration_styles.get(duration, {'linestyle': '-', 'marker': '.'})

        data = np.sort(aggregates)
        cdf = np.arange(1, len(data) + 1) / len(data)
        label = f"{flow_type.upper()} ({duration})"



        plt.plot(
            data, cdf,
            label=label,
            color=color,
            linestyle=style['linestyle'],
            marker=style['marker'],
            linewidth=1.0,
            markersize=4,
        )

    plt.xlabel("Aggregate Sent per Ground Station (Mbit)", fontsize=11)
    plt.ylabel("CDF", fontsize=11)
    plt.title("CDF of Aggregate Sent per Ground Station", fontsize=13)
    plt.legend(fontsize=9)
    plt.grid(True, linewidth=0.3)
    plt.yticks(np.arange(0, 1.01, 0.1))  
    plt.tight_layout()
    plt.savefig("cdf_sent_GScomparison_rich.png", dpi=150)
    print("Saved plot as cdf_sent_GScomparison_rich.png")



# === Example Usage ===
filepaths = [
    #"./tcp_flows_sf1s.txt",
    #"./tcp_flows_ddef1s.txt",
    #"./tcp_flows_cache1s.txt",
    #"./tcp_flows_sf10s.txt",
    #"./tcp_flows_ddef10s.txt",
    #"./tcp_flows_mcache1s.txt",
    #"./tcp_flows_mcache5s.txt",
    #"./tcp_flows_mcache10s.txt",
    #"./tcp_flows_mcache50s.txt",
    #"./tcp_flows_cache50s.txt",
    #"flows/tcp_flows_sf50s.txt",
    "flows/tcp_flows_100f_sf50s.txt",
    #"flows/tcp_flows_10f_50s.txt",
    "flows/tcp_flows_r_100f_cache50s.txt",
    "flows/tcp_flows_100f_cache50s.txt",
    #"runs/run_general_tm_pairing_kuiper_isls_moving/logs_ns3/tcp_flows_50s.txt",
    #"./runs/run_general_tm_pairing_kuiper_isls_moving/logs_ns3/tcp_flows_50s.txt"
    #"./tcp_flows_scache50s.txt",


]  # Replace or expand as needed
plot_cdf_aggregates(filepaths)
