import exputil
import networkload

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
    list_from_to = [(1156, 1157)] 


    # 1156 -> | 617 -> 583 -> 549 -> 515 -> 514 -> 513 -> 1157 | 0
    # 1162 -> | 549 -> -> 515 -> 514 -> 513 -> 1157 | 2
    # 1166 -> 513 -> 1157
    # 1167 -> 479 -> 1157
    # 1171 -> 548 -> | 514 -> 513 -> 1157 | 5
    # 1179 -> | 549 -> 515 -> 514 -> 513 -> 1157 | 3
    # 1187 -> 616 -> 582 -> 548 -> | 514 -> 513 -> 1157 | 4
    # 1189 -> | 617 -> 583 -> 549 -> 515 -> 514 -> 513 -> 1157 | 1

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