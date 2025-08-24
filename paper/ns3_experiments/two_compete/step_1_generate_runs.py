import exputil
import networkload
import random

local_shell = exputil.LocalShell()

# Clean-up for a fresh run
local_shell.remove_force_recursive("runs")
local_shell.remove_force_recursive("pdf")
local_shell.remove_force_recursive("data")

for movement in ["static", "moving"]:
    protocol_chosen = "tcp"

    run_dir = f"runs/run_two_kuiper_isls_{movement}"
    local_shell.remove_force_recursive(run_dir)
    local_shell.make_full_dir(run_dir)

    # config_ns3.properties
    local_shell.copy_file("templates/template_config_ns3.properties", run_dir + "/config_ns3.properties")
    local_shell.sed_replace_in_file_plain(
        run_dir + "/config_ns3.properties",
        "[SATELLITE-NETWORK-FORCE-STATIC]",
        "true" if movement == "static" else "false"
    )

    # Create logs directory
    local_shell.make_full_dir(run_dir + "/logs_ns3")

    # .gitignore
    local_shell.write_file(run_dir + "/.gitignore", "logs_ns3")

    # Define flows
    nodes = range(1156, 1256)
    src = 1206  #random.choice(nodes)
    dst = 1215#random.choice([n for n in nodes if n != src])
    list_from_to = [(src, dst)]
    print(list_from_to) 

                
    list_from_to = list_from_to * 100

    # 1161 -> 1171
    # 548 582 581 615

    ##############
    #-> 1171
    ##############
    # |548|: 582
    # |581|: 582 615
    # |582|: 548 581
    # |615|: 581
    # 1161 -> 627 -> 626 -> 625 -> 624 -> 623 -> 622 -> 621 
    #      -> 620 -> 586 -> 585 -> 584 -> 583 -> |582| -> 1171
    
    
    # 1162 -> |582| -> 1171

    ##############
    #-> 1157
    ##############
    # 1156 -> 617 -> 583 -> 549 -> 515 -> 514 -> 513 -> 1157 
    # 1162 -> 549 -> -> 515 -> 514 -> 513 -> 1157 
    # 1166 -> 513 -> 1157
    # 1167 -> 479 -> 1157
    # 1171 -> 548 -> 514 -> 513 -> 1157 
    # 1179 -> 549 -> 515 -> 514 -> 513 -> 1157  
    # 1187 -> 616 -> 582 -> 548 -> 514 -> 513 -> 1157 
    # 1189 -> 617 -> 583 -> 549 -> 515 -> 514 -> 513 -> 1157 

    ##############
    #-> 1160
    ##############
    ## 82 48
    ## 82 -> 48
    ## 48 -> 82
    # 1240 -> 123 -> 89 -> -> 88 -> 87 -> 86 -> 85 -> 84 -> 83 -> | 82 | -> 1160
        
   
    ##############
    #-> 1212
    ##############
    ## 993  -> ?
    ## 1026 -> 1060 1027
    ## 1027 -> 993 1026
    ## 1059 -> 1060
    ## 1060 -> 1026 1059 1094
    ## 1093 -> 1059 1094
    ## 1094 -> 1060 1093
    
    # 1234 -> 1101 -> 1100 -> -> 1099 -> 1098 -> 1097 -> 1096 -> 1095 -> | 1094 | -> 1160
    # 1200 -> 47 -> 46 -> 45 -> 44 -> 43 -> 42 -> 8 -> 1130 -> 1096 -> 1095 | 1094 | -> 1160

    # Log all flows
    local_shell.sed_replace_in_file_plain(
        run_dir + "/config_ns3.properties",
        "[FLOW-LOG-SET]",
        ",".join(list(map(str, range(len(list_from_to)))))
    )

    # Schedule writing
    if protocol_chosen == "tcp":
        networkload.write_schedule(
            run_dir + "/schedule_kuiper_630.csv",
            len(list_from_to),
            list_from_to,
            [1000000000000] * len(list_from_to),
            [0] * len(list_from_to)
        )
    elif protocol_chosen == "udp":
       with open(run_dir + "/udp_burst_schedule.csv", "w+") as f_out:
            for i in range(len(list_from_to)):
                f_out.write(
                    f"{i},{list_from_to[i][0]},{list_from_to[i][1]},40.00,0,1000000000000,,\n"
                )

print("Success")



# competition
# poisoning 
# seeing traffic matrix
# making