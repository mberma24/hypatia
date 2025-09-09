import pandas as pd
import os
import matplotlib.pyplot as plt

# Folders to process
folders = [
    "Cache",  
    "Cache_No_Lim", 
    "Cache_No_Lim_Poison", 
    "Cache_No_Poison",  
    "Cache_No_Weights",  
    "Hash",
]


def process_pair(folder):
    files = [os.path.join(folder, f) for f in os.listdir(folder) if os.path.isfile(os.path.join(folder, f))]
    
    if len(files) <= 0:
        print(f"⚠️ Skipping {folder}: could not find files")
        return
    
    # Read files
    df1 = pd.read_csv(files[0], sep=r"\s+", names=["time_ns", "deflections"])
    df2 = pd.read_csv(files[1], sep=r"\s+", names=["time_ns", "deflections"])
    
    # Convert ns → seconds (integer binning)
    df1["time_s"] = (df1["time_ns"] // 1_000_000_000).astype(int)
    df2["time_s"] = (df2["time_ns"] // 1_000_000_000).astype(int)
    
    # Average per second
    df1_avg = df1.groupby("time_s")["deflections"].mean().reset_index()
    df2_avg = df2.groupby("time_s")["deflections"].mean().reset_index()
    
    # Merge on time
    merged = pd.merge(df1_avg, df2_avg, on="time_s", how="outer", suffixes=("_path1", "_path2"))
    merged.sort_values("time_s", inplace=True)
    
    # Plot as PNG
    plt.figure(figsize=(10,6))
    plt.plot(
        merged["time_s"].to_numpy(),
        merged["deflections_path1"].to_numpy(),
        label=os.path.basename(files[0]),
        marker="o", markersize=3, linewidth=1.5
    )
    plt.plot(
        merged["time_s"].to_numpy(),
        merged["deflections_path2"].to_numpy(),
        label=os.path.basename(files[1]),
        marker="x", markersize=3, linewidth=1.5
    )
    plt.legend()
    plt.xlabel("Time (s)")
    plt.ylabel("Average deflections")
    plt.title(f"Comparison: {os.path.basename(folder)}")

    # Save first PNG (common output)
    graph_path = "../graph_data//path_lengths/" + os.path.basename(folder) + ".png"
    plt.savefig(graph_path, dpi=150)

    # Now close
    plt.close()
 

def main():
    pre_path = "../two_compete/kuiper/1206_1215/Path_Lengths/"
    for folder in folders:
        if os.path.isdir(pre_path+folder):
            process_pair(pre_path+folder)
        else:
            print(f"⚠️ Folder not found: {pre_path+folder}")

if __name__ == "__main__":
    main()
