/*
 * Copyright (c) 2020 ETH Zurich
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Simon               2020
 */

#include <map>
#include <iostream>
#include <fstream>
#include <string>
#include <ctime>
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <chrono>
#include <stdexcept>

#include "ns3/basic-simulation.h"
#include "ns3/tcp-flow-scheduler.h"
#include "ns3/udp-burst-scheduler.h"
#include "ns3/pingmesh-scheduler.h"
#include "ns3/topology-satellite-network.h"
#include "ns3/tcp-optimizer.h"
#include "ns3/arbiter-single-forward-helper.h"
#include "ns3/arbiter-deflection-helper.h"
#include "ns3/arbiter-gs-priority-deflection-helper.h"
#include "ns3/ipv4-arbiter-routing-helper.h"
#include "ns3/gsl-if-bandwidth-helper.h"
#include "ns3/packet-loss-counter.h"
#include "ns3/tcp-flow-sink-helper.h"


#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor.h"


#include "ns3/packet.h"
#include "ns3/ipv4-header.h"
#include "ns3/ipv6-header.h"
#include "ns3/tcp-header.h"
#include "ns3/udp-header.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/ethernet-header.h"


#include "ns3/packet.h"

// Network headers
#include "ns3/ipv4-header.h"
#include "ns3/ipv6-header.h"

// Transport headers
#include "ns3/tcp-header.h"
#include "ns3/udp-header.h"

// Link / MAC headers
#include "ns3/ethernet-header.h"
#include "ns3/wifi-mac-header.h"
#include "ns3/lr-wpan-mac-header.h"
#include "ns3/sll-header.h"
#include "ns3/ppp-header.h"
#include "ns3/radiotap-header.h"
#include "ns3/aloha-noack-mac-header.h"
#include "ns3/ampdu-subframe-header.h"
#include "ns3/amsdu-subframe-header.h"

// LTE / Cellular headers
#include "ns3/lte-pdcp-header.h"
#include "ns3/lte-rlc-header.h"
#include "ns3/lte-rlc-am-header.h"
#include "ns3/epc-gtpc-header.h"
#include "ns3/epc-gtpu-header.h"
#include "ns3/epc-x2-header.h"
#include "ns3/lte-rrc-header.h"
#include "ns3/lte-asn1-header.h"

// Routing / control headers
#include "ns3/rip-header.h"
#include "ns3/ripng-header.h"
#include "ns3/arp-header.h"
#include "ns3/icmpv6-header.h"

// Application / sim-specific headers
#include "ns3/seq-ts-header.h"
#include "ns3/seq-ts-size-header.h"
#include "ns3/three-gpp-http-header.h"
#include "ns3/epc-gtpu-header.h"






using namespace ns3;
// Add these includes at the top of your file
#include "ns3/config.h"
#include "ns3/callback.h"
#include "ns3/packet.h"
#include "ns3/address.h"
#include "ns3/tcp-header.h"

#include "ns3/trace-source-accessor.h"



int main(int argc, char *argv[]) {

    // No buffering of printf
    setbuf(stdout, nullptr);

    // Retrieve run directory
    CommandLine cmd;
    std::string run_dir = "";
    cmd.Usage("Usage: ./waf --run=\"main_satnet --run_dir='<path/to/run/directory>'\"");
    cmd.AddValue("run_dir",  "Run directory", run_dir);
    cmd.Parse(argc, argv);
    if (run_dir.compare("") == 0) {
        printf("Usage: ./waf --run=\"main_satnet --run_dir='<path/to/run/directory>'\"");
        return 0;
    }

    // Load basic simulation environment
    Ptr<BasicSimulation> basicSimulation = CreateObject<BasicSimulation>(run_dir);

    // Setting socket type
    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::" + basicSimulation->GetConfigParamOrFail("tcp_socket_type")));
    Config::Set("/TcpFlowScheduler/EnableLogging", BooleanValue(true));
    // Optimize TCP
    TcpOptimizer::OptimizeBasic(basicSimulation);

    // Read topology, and install routing arbiters
    Ptr<TopologySatelliteNetwork> topology = CreateObject<TopologySatelliteNetwork>(basicSimulation, Ipv4ArbiterRoutingHelper());


    ArbiterGSPriorityDeflectionHelper arbiterHelper(basicSimulation, topology->GetNodes());
    GslIfBandwidthHelper gslIfBandwidthHelper(basicSimulation, topology->GetNodes());

    // Schedule flows
    TcpFlowScheduler tcpFlowScheduler(basicSimulation, topology); // Requires enable_tcp_flow_scheduler=true

    // Schedule UDP bursts
    UdpBurstScheduler udpBurstScheduler(basicSimulation, topology); // Requires enable_udp_burst_scheduler=true

    // Schedule pings
    PingmeshScheduler pingmeshScheduler(basicSimulation, topology); // Requires enable_pingmesh_scheduler=true

    std::srand(std::time(nullptr));

    // Run simulation
    basicSimulation->Run();

    // Write flow results
    tcpFlowScheduler.WriteResults();

    // Write UDP burst results
    udpBurstScheduler.WriteResults();

    // Write pingmesh results
    pingmeshScheduler.WriteResults();

    // Collect utilization statistics
    topology->CollectUtilizationStatistics();

    // Finalize the simulation
    basicSimulation->Finalize();

    

    return 0;
}
