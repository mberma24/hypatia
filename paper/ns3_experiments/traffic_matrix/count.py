import re
from collections import defaultdict

source_sent = defaultdict(list)

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

    
    aggregates = [(source, sum(vals)) for source, vals in source_sent.items()]
    return aggregates

total_pairs_m = parse_aggregate_sent_per_source("/home/sat/hypatia/paper/ns3_experiments/traffic_matrix/runs/run_general_tm_pairing_kuiper_isls_moving/logs_ns3/tcp_flows.txt")
#total_pairs_m = parse_aggregate_sent_per_source("tcp_flows_cache10s.txt")
total_pairs_s = parse_aggregate_sent_per_source("/home/sat/hypatia/paper/ns3_experiments/traffic_matrix/runs/run_general_tm_pairing_kuiper_isls_static/logs_ns3/tcp_flows.txt")
total_pairs_s = total_pairs_m
totals_m = [agg for _, agg in total_pairs_m]
overall_average_m = sum(totals_m) / len(totals_m)
totals_s = [agg for _, agg in total_pairs_s]
overall_average_s = sum(totals_s) / len(totals_s)
#
print("\nMOVING:")
for (src, avg) in total_pairs_m:
    print(f"Source {src}: Aggregate Sent = {avg:.2f} Mbit")

print("\nSTATIC:")
for (src, avg) in total_pairs_s:
    print(f"Source {src}: Aggregate Sent = {avg:.2f} Mbit")

print("Static overall average: ", overall_average_m)
print("Moving overall average: ", overall_average_s)
