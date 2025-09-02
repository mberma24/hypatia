
dir = "/home/mberma24/hypatia/z_flows/traffic_matrix/kuiper/123456789/"
path_list = [
    "Cache",
    "Cache_No_Deflection_Limit",
    "Cache_No_Poison",
    "Cache_No_TTL_Sensitivity",
    "Cache_No_Weights",
    "Hash",
    "SF",
]



if __name__ == "__main__":
    for path_ending in path_list:
        path = dir + path_ending + "tcp_flows.txt"
