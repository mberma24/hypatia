import matplotlib.pyplot as plt
import numpy as np
import re

def extract_sent_values(filepath):
    sent_values = []
    # Regex pattern to capture the sent data in Mbit (the 8th logical field with Mbit unit)
    pattern = re.compile(
        r"""^\s*\d+\s+            # Flow ID (skip)
            \d+\s+                # Source (skip)
            \d+\s+                # Target (skip)
            [\d.]+\s+Mbit\s+      # Size (skip)
            \d+\s+                # Start time (skip)
            \d+\s+                # End time (skip)
            [\d.]+\s+ms\s+        # Duration (skip)
            ([\d.]+)\s+Mbit       # Sent value (capture)
        """, re.VERBOSE)

    with open(filepath) as f:
        for line in f:
            if line.strip().startswith("TCP") or not line.strip():
                continue  # Skip header or empty lines

            m = pattern.match(line)
            if m:
                sent = float(m.group(1))
                sent_values.append(sent)
            else:
                # Optionally print skipped malformed lines if debugging
                # print("Skipping malformed line:", line.strip())
                pass
    return sent_values

# Define datasets
files = {
    "DDEF1": "./tcp_flows_ddef1.txt",
    "DDEF2": "./tcp_flows_ddef2.txt",
    "Cache1": "./tcp_flows_cache1.txt",
    "Cache2": "./tcp_flows_cache2.txt",
    "SF": "./tcp_flows_sf.txt"
}

plt.figure(figsize=(12, 7))

colors = ['blue', 'green', 'red', 'orange', 'black']
markers = ['o', 's', '^', 'x', 'D']

for i, (label, filepath) in enumerate(files.items()):
    sent_values = extract_sent_values(filepath)
    if not sent_values:
        print(f"[{label}] No valid sent data found.")
        continue

    sent_values.sort()
    n = len(sent_values)
    cdf = [j / n for j in range(1, n + 1)]

    mean = np.mean(sent_values)
    median = np.median(sent_values)
    num_zero = sum(1 for v in sent_values if v == 0)

    rich_label = f"{label} (mean={mean:.2f}, med={median:.2f}, 0s={num_zero})"
    plt.plot(sent_values, cdf, label=rich_label, color=colors[i], marker=markers[i], linestyle='-', markersize=4)

plt.xlabel("Sent Data per Flow (Mbit)", fontsize=12)
plt.ylabel("CDF", fontsize=12)
plt.title("CDF of Sent Data Across Configurations", fontsize=14)
plt.grid(True)
plt.legend()
plt.tight_layout()
plt.savefig("cdf_sent_comparison_rich.png")
print("Saved plot as cdf_sent_comparison_rich.png")
