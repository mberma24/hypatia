
import re
import matplotlib.pyplot as plt
import os

TIME = 50


path = "../two_compete/kuiper/1206_1215/"
normal_path = path + "Normal_Cache/"
threshhold_path = path + "Threshold_Size/"
expire_path = path + "Expire_Rate/"




def get_aggregate_throughput(path):
    throughputs = []
    with open(path, "r") as f:
        for line in f:
            if line.strip().startswith("TCP") or not line.strip():
                continue

            match = re.match(
                r"^\s*\d+\s+\d+\s+\d+\s+[\d.]+\s+Mbit\s+\d+\s+\d+\s+([\d.]+)\s+ms\s+([\d.]+)\s+Mbit",
                line
            )
            if match:
                duration_ms = float(match.group(1))
                sent = float(match.group(2))
                if duration_ms > 0:
                    throughput = sent / (duration_ms / 1000.0)  # convert ms → s
                    throughputs.append(throughput)

    return sum(throughputs)
    
normal_throughput = get_aggregate_throughput(normal_path + "tcp_flows.txt") 


def save_graph(x, y, filename, xlabel="X", ylabel="Y", title="Graph"):
    """Save a simple line graph to ../graph_data"""
    save_dir = "../graph_data"
    os.makedirs(save_dir, exist_ok=True)

    plt.figure()
    plt.plot(x, y, marker="o", linestyle="-")
    plt.xlabel(xlabel)
    plt.ylabel(ylabel)
    plt.title(title)
    plt.grid(True)

    save_path = os.path.join(save_dir, filename)
    plt.savefig(save_path)
    plt.close()

    print(f"Graph saved to {save_path}")

def make_threshold_graph():  
    first_half = [.01, .1, .2, .25, .3, .35]
    default = .4
    second_half = [.45, .5, .6, .7, .8, .9, 1]
    y = []
    for n in first_half:
        new_path = threshhold_path + str(n) + "/tcp_flows.txt"
        threshhold_throughput = get_aggregate_throughput(new_path)
        y.append(threshhold_throughput)
    y.append(normal_throughput)
    print("Def: ", normal_throughput)
    for n in second_half:
        new_path = threshhold_path + str(n) + "/tcp_flows.txt"
        threshhold_throughput = get_aggregate_throughput(new_path)
        y.append(threshhold_throughput)

    first_half.append(default)
    first_half = first_half + second_half
    x = first_half
    print(x)
    print([f"{val:.3f}" for val in y])

    if True:
        save_graph(x, y, "param_threshold", "Threshold", "Total Throughput")

    return x,y

    

def make_expire_rate_graph():  
    #first_half = [1, 50, 100]
    first_half = [1]
    default = 150
    #second_half = [200, 250, 300, 350, 400, 450, 500, 550, 600, 650, 700, 750, 800, 850, 900, 950, 1000]
    second_half = [500, 1000, 1500, 2000, 2500, 3000, 3500, 4000, 4500, 5000, 5500, 6000, 6500, 7000, 7500, 8000, 8500, 9000, 9500, 10000]
    y = []

    for n in first_half:
        new_path = expire_path + str(n) + "/tcp_flows.txt"
        threshhold_throughput = get_aggregate_throughput(new_path)
        y.append(threshhold_throughput)
    y.append(normal_throughput)
    print("Def: ", normal_throughput)
    for n in second_half:
        new_path = expire_path + str(n) + "/tcp_flows.txt"
        threshhold_throughput = get_aggregate_throughput(new_path)
        y.append(threshhold_throughput)

    first_half.append(default)
    first_half = first_half + second_half
    x = first_half
    print(x)
    print([f"{val:.3f}" for val in y])
    
    if True:
        save_graph(x, y, "param_expiration_rate", "Expire Rate", "Total Throughput")

    return x,y


make_threshold_graph()
make_expire_rate_graph()
