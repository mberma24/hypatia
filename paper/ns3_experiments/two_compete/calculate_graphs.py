import calculate_num_bytes_sent
import calculate_num_def  
import csv
import matplotlib.pyplot as plt
import numpy as np

# -------------------------
# Parameters
# -------------------------
src = 1161
dst = 1171
S = 50  # total simulation time in seconds
fstate_path = (
    "../../satellite_networks_state/gen_data_lasthop_splitting/"
    "kuiper_630_isls_plus_grid_ground_stations_top_100_algorithm_free_gs_one_sat_"
    "uplink_downlink_only_over_isls_lasthop_splitting/dynamic_state_100ms_for_50s"
)

# -------------------------
# Load data
# -------------------------
mbits_sent = calculate_num_bytes_sent.get_mbits_sent()
_, deflections_possible, _ = calculate_num_def.calculate_deflections(
    fstate_path, src, dst, S=S
)

# -------------------------
# Prepare arrays
# -------------------------
times = sorted(mbits_sent.keys())
mbits_values = np.array([mbits_sent[t] for t in times])

# Compute instantaneous rate (Mbits/s per timestep)
step_s = (times[1] - times[0]) / 1e9  # step duration in seconds
mbits_rate = np.zeros_like(mbits_values)
mbits_rate[0] = mbits_values[0] / step_s
mbits_rate[1:] = np.diff(mbits_values) / step_s

# Smooth rate with a 1-second moving average
window_size = int(round(1.0 / step_s))  # number of samples in 1 second
if window_size > 1:
    kernel = np.ones(window_size) / window_size
    mbits_rate_smooth = np.convolve(mbits_rate, kernel, mode="same")
else:
    mbits_rate_smooth = mbits_rate

# Compute max throughput from deflections
max_throughput = {
    t: min(40, deflections_possible.get(t, 0) * 10) for t in times
}

# -------------------------
# Save to CSV
# -------------------------
csv_file = "simulation_results_with_rate.csv"
with open(csv_file, "w", newline="") as f:
    writer = csv.writer(f)
    writer.writerow(
        ["Time_s", "Mbits_Sent", "Mbits_per_s", "#Deflections", "Max_Throughput"]
    )
    for i, t_ns in enumerate(times):
        t_s = t_ns / 1e9
        defl = deflections_possible.get(t_ns, 0)
        thr = max_throughput[t_ns]
        writer.writerow([t_s, mbits_values[i], mbits_rate_smooth[i], defl, thr])
print(f"Saved results to {csv_file}")

# -------------------------
# Plot 1: Cumulative Mbits Sent + Max Throughput (same axis)
# -------------------------
plt.figure(figsize=(12, 6))
plt.plot([t / 1e9 for t in times], mbits_values,
         color="tab:blue", label="Mbits Sent")
plt.plot([t / 1e9 for t in times], [max_throughput[t] for t in times],
         color="tab:red", linestyle="--", label="Max Throughput")

plt.xlabel("Time (s)")
plt.ylabel("Mbits")
plt.title("Simulation Metrics Over Time (Cumulative Mbits Sent + Max Throughput)")
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.savefig("simulation_results_mbits_sent.png")

# -------------------------
# Plot 2: Rate + Max Throughput (same axis)
# -------------------------
plt.figure(figsize=(12, 6))
plt.plot([t / 1e9 for t in times], mbits_rate_smooth,
         color="tab:cyan", linestyle="--", label="Mbits/s (smoothed)")
plt.plot([t / 1e9 for t in times], [max_throughput[t] for t in times],
         color="tab:red", linestyle="--", label="Max Throughput")

plt.xlabel("Time (s)")
plt.ylabel("Mbits/s")
plt.title("Simulation Metrics Over Time (Mbits/s Rate + Max Throughput)")
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.savefig("simulation_results_rate.png")
