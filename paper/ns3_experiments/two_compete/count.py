import os
def sum_avg_rates(filename):
    if not os.path.exists(filename):
        print(f"File not found: {filename}")
        return 0.0
    total_rate = 0.0
    with open(filename, 'r') as f:
        for line in f:
            line = line.strip()
            # Skip empty lines or headers
            if not line or line.startswith("TCP Flow ID") or line.startswith("Source"):
                continue
            parts = line.split()
            # The "Avg. rate" value is just before the "Mbit/s" string at the 10th index (0-based: 9)
            # Sometimes your columns may have varying spaces, so let's find the "Mbit/s" token
            if "Mbit/s" in parts:
                idx = parts.index("Mbit/s")
                try:
                    rate = float(parts[idx-1])
                    total_rate += rate
                except ValueError:
                    # Ignore lines where conversion fails
                    pass
    return total_rate

if __name__ == "__main__":
    filename_moving = "runs/run_two_kuiper_isls_moving/logs_ns3/tcp_flows.txt"  
    filename_static = "runs/run_two_kuiper_isls_static/logs_ns3/tcp_flows.txt"
    total = sum_avg_rates(filename_moving)
    print(f"MOVING: Total Avg. rate sum = {total:.2f} Mbit/s")
    total = sum_avg_rates(filename_static)
    print(f"STATIC: Total Avg. rate sum = {total:.2f} Mbit/s")
