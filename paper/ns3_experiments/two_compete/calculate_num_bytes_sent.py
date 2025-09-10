import os


time = 0

def get_mbits_sent(S = 50, n_flows = 100, path = "./runs/run_two_kuiper_isls_static/logs_ns3"):
    time = S
    step = 100_000_000  # 0.1s in ns
    total_ns = int(S * 1e9)
    mbits_sent = {i: 0.0 for i in range(0, total_ns + step, step)}

    for flow in range(n_flows):
        filename = os.path.join(path, f"tcp_flow_{flow}_progress.csv")

        if not os.path.exists(filename):
            print(f"Skipping missing file: {filename}")
            continue

        with open(filename, "r") as f:
            last_time = 0
            last_mbits = 0.0

            for line in f:
                line = line.strip().split(",")
                time = int(line[1])     
                mbits = int(line[2]) * 8.0 / 1e6  

                start_bucket = (last_time // step) * step
                end_bucket   = (time // step) * step

                for i in range(start_bucket, end_bucket, step):
                    mbits_sent[i] += last_mbits

                last_time = time
                last_mbits = mbits

            start_bucket = (last_time // step) * step
            for i in range(start_bucket, total_ns + step, step):
                mbits_sent[i] += last_mbits

    return mbits_sent


if __name__ == "__main__":
    mbits_sent = get_mbits_sent()
    print("Time (s)\tMbits Sent")
    for t in sorted(mbits_sent.keys()):
        print(f"{t/1e9:8.3f}\t{mbits_sent[t] / time:.2f}")
