import pandas as pd
import os
import matplotlib.pyplot as plt
import numpy as np

# Folders to process
folders = [
    "Cache",  
    "Cache_All_Off", 
    # "Cache_No_Lim", 
    # "Cache_No_Poison",  
    # "Cache_No_Weights", 
    # "Cache_No_TTL_Sensitivity", 
    "Cache_None_Aside_Weights", 
    # "Cache_None_Aside_Weights2", 
    "Cache_None_Aside_TTL", 
    "Cache_None_Aside_Lim",
    "Cache_None_Aside_Poison" 
    # "Hash",
]

runs = ["A", "B", "C"]


def process_folder(folder, pre_path):
    """Load runs for a folder. Uses A/B/C if they exist, otherwise single file (e.g., Hash)."""
    dfs = []

    if folder == "Hash":
        
        file_path = os.path.join(pre_path, folder, "m_paths_1215.txt")
        print(file_path)
        if not os.path.isfile(file_path):
            print(f"⚠️ Skipping {folder}: m_paths_1215.txt not found")
            return None, None, None

        df = pd.read_csv(file_path, sep=r"\s+", names=["time_ns", "deflections"])
        df["time_s"] = (df["time_ns"] // 1_000_000_000).astype(int)
        df_avg = df.groupby("time_s")["deflections"].mean().reset_index()
        return df_avg["time_s"].to_numpy(), df_avg["deflections"].to_numpy(), None

    # Otherwise, try A, B, C
    for run in runs:
        file_path = os.path.join(pre_path, folder, run, "m_paths_1215.txt")
        print(file_path)
        if not os.path.isfile(file_path):
            print(f"⚠️ Skipping {folder}/{run}: m_paths_1215.txt not found")
            continue

        df = pd.read_csv(file_path, sep=r"\s+", names=["time_ns", "deflections"])
        df["time_s"] = (df["time_ns"] // 1_000_000_000).astype(int)
        df_avg = df.groupby("time_s")["deflections"].mean().reset_index()
        dfs.append(df_avg)

    if not dfs:
        return None, None, None

    # Merge all runs on time_s
    merged = dfs[0]
    for other in dfs[1:]:
        merged = pd.merge(merged, other, on="time_s", how="outer", suffixes=("", "_dup"))

    # Find all deflection columns
    defl_cols = [col for col in merged.columns if "deflections" in col]

    # Compute mean and std
    merged["mean"] = merged[defl_cols].mean(axis=1)
    merged["std"] = merged[defl_cols].std(axis=1)

    return merged["time_s"].to_numpy(), merged["mean"].to_numpy(), merged["std"].to_numpy()


def main():
    pre_path = "../two_compete/kuiper/1206_1215/Path_Lengths_200s/"
    plt.figure(figsize=(12, 7))

    for folder in folders:
        x, y_mean, y_std = process_folder(folder, pre_path)
        if x is None:
            continue

        # Plot mean line
        plt.plot(x, y_mean, label=folder, linewidth=1.5)

        # If std exists, add shaded band
        if y_std is not None:
            plt.fill_between(x, y_mean - y_std, y_mean + y_std, alpha=0.2)

    plt.legend()
    plt.xlabel("Time (s)")
    plt.ylabel("Average deflections")
    plt.title("Path Lengths for m_paths_1215 Across Folders (Mean ± Std Dev, Hash = single run)")
    
    graph_path = "../graph_data/path_lengths/single_change_cache_folders_1215_runs.png"
    plt.savefig(graph_path, dpi=150)
    plt.close()


if __name__ == "__main__":
    main()
