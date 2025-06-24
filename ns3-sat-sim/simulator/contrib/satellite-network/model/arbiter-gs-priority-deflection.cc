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

#include "arbiter-gs-priority-deflection.h"
#include "ns3/mobility-model.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/traffic-control-layer.h"
#include "ns3/queue-disc.h"
#include <cmath>



namespace ns3 {


NS_OBJECT_ENSURE_REGISTERED (ArbiterGSPriorityDeflection);
TypeId ArbiterGSPriorityDeflection::GetTypeId (void)
{
    static TypeId tid = TypeId ("ns3::ArbiterGSPriorityDeflection")
            .SetParent<ArbiterSatnet> ()
            .SetGroupName("BasicSim")
    ;
    return tid;
}

ArbiterGSPriorityDeflection::ArbiterGSPriorityDeflection(
        Ptr<Node> this_node,
        NodeContainer nodes,
        std::vector<std::vector<std::tuple<int32_t, int32_t, int32_t>>> next_hop_list
) : ArbiterSatnet(this_node, nodes)
{
    m_next_hop_list = next_hop_list;
}

/*
QUESTIONS:
*/


std::tuple<int32_t, int32_t, int32_t> ArbiterGSPriorityDeflection::TopologySatelliteNetworkDecide(
        int32_t source_node_id,
        int32_t target_node_id,
        Ptr<const Packet> pkt,
        Ipv4Header const &ipHeader,
        bool is_request_for_source_ip_so_no_next_header
) {

    // Hash-based selection of next hop
    const auto& next_hops = m_next_hop_list[target_node_id];

    // if next_hops has nothing available
    if (next_hops.empty()) {
        // Return an invalid entry if no next hops are available
        std::cout << "ERR: NO HOPS AVAILABLE, RETURNING -2" << std::endl;
        return std::make_tuple(-2, -2, -2);
    }
    
    //get default (straightforward) routing option + its info
    std::tuple<uint32_t, uint32_t, uint32_t> to_return = next_hops[0];
    uint32_t next_node_id, my_if_id, next_if_id;
    std::tie(next_node_id, my_if_id, next_if_id) = to_return;

    //if something went wrong, we will simply return here (dropped, no path found)
    if (next_node_id < 0 || my_if_id < 0 ||  next_if_id < 0) {
        std::cout << "ERR: SOMETHING WENT WRONG" << std::endl;
        return to_return;
    }
    
    //if we are at the last hop before gs and have options
    if (IsFinalHopToGS(m_node_id, next_node_id, my_if_id) && next_hops.size() > 1) { 
        printf("STARTING: \tcnid: %d nnid: %d my_if_id: %d next_if_id: %d\n", m_node_id, next_node_id, my_if_id, next_if_id);
        //the core logic is in hashFunc
        to_return = next_hops[hashFunc(pkt, ipHeader, is_request_for_source_ip_so_no_next_header, next_hops)];
        
        //check for errors
        uint32_t next_node_id, my_if_id, next_if_id; 
        std::tie(next_node_id, my_if_id, next_if_id) = to_return;
        if (next_node_id < 0 || my_if_id < 0 ||  next_if_id < 0) {
            std::cout << "ERR: SOMETHING WENT WRONG" << std::endl;
            return next_hops[0];
        }
        //signal the end for this packet
        printf("ENDING\n\n");
    } 
    return to_return;
}

void ArbiterGSPriorityDeflection::SetDeflectionState(int32_t target_node_id, const std::vector<std::tuple<int32_t, int32_t, int32_t>>& next_hops) {
    // Check that all next_hops are valid
    for (const auto& next_hop : next_hops) {
        int32_t next_node_id = std::get<0>(next_hop);
        int32_t own_if_id = std::get<1>(next_hop);
        int32_t next_if_id = std::get<2>(next_hop);
        NS_ABORT_MSG_IF(next_node_id == -2 || own_if_id == -2 || next_if_id == -2, "Not permitted to set invalid (-2).");
    }
    m_next_hop_list[target_node_id] = next_hops;
}

std::string ArbiterGSPriorityDeflection::StringReprOfForwardingState() {
    std::ostringstream res;
    res << "Deflection--forward state of node " << m_node_id << std::endl;
    for (size_t i = 0; i < m_nodes.GetN(); i++) {
        res << "  -> " << i << ": ";
        const auto& next_hops = m_next_hop_list[i];
        if (next_hops.empty()) {
            res << "(no next hops)" << std::endl;
        } else {
            for (size_t j = 0; j < next_hops.size(); ++j) {
                res << "(" << std::get<0>(next_hops[j]) << ", "
                    << std::get<1>(next_hops[j]) << ", "
                    << std::get<2>(next_hops[j]) << ")";
                if (j != next_hops.size() - 1) res << ", ";
            }
            res << std::endl;
        }
    }
    return res.str();
}



ArbiterGSPriorityDeflection::Checkpoint ArbiterGSPriorityDeflection::GetCheckpoint(int flag_index) {
    switch (flag_index){
        case  0: return { OVER_0,  0.0   };
        case  1: return { OVER_20, 0.2   };
        case  2: return { OVER_30, 0.3   };
        case  3: return { OVER_40, 0.4   };
        case  4: return { OVER_50, 0.5   };
        case  5: return { OVER_55, 0.55  };
        case  6: return { OVER_60, 0.6   };
        case  7: return { OVER_65, 0.65  };
        case  8: return { OVER_70, 0.7   };
        case  9: return { OVER_75, 0.75  };
        case 10: return { OVER_80, 0.8   };
        case 11: return { OVER_85, 0.85  };
        case 12: return { OVER_90, 0.9   };
        case 13: return { OVER_95, 0.95  };
        case 14: return { CRITICAL, 1.0  };
        default: return { CRITICAL, 1.0 }; // fallback
    }
}

// Checks if a node is a ground station
bool ArbiterGSPriorityDeflection::IsGroundStation(uint32_t node_id) {
    Ptr<Node> node = m_nodes.Get(node_id);
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    for (uint32_t i = 0; i < ipv4->GetNInterfaces(); ++i) {
        for (uint32_t j = 0; j < ipv4->GetNAddresses(i); ++j) {
            if (ipv4->GetNetDevice(i)->GetObject<PointToPointLaserNetDevice>() != nullptr) {
                return false;
            }
        }
    }
    return true;
}

// Checks if a node is a ground station
bool ArbiterGSPriorityDeflection::IsFinalHopToGS(uint32_t node_id, uint32_t next_node_id, uint32_t my_if_id) {
    bool is_gsl = m_nodes.Get(m_node_id)->GetObject<Ipv4>()->GetNetDevice(my_if_id)->GetObject<GSLNetDevice>() != 0;
    bool this_is_gs = IsGroundStation(node_id);
    bool next_is_gs = IsGroundStation(next_node_id);
    return is_gsl && next_is_gs && !this_is_gs;
}

// Gets the ratio the netdevice if filled given the if id for the link
double ArbiterGSPriorityDeflection::GetQueueRatio(uint32_t if_id) {
    Ptr<Queue<Packet>> dev_queue = m_nodes.Get(m_node_id)
        ->GetObject<Ipv4>() 
        ->GetNetDevice(if_id)
        ->GetObject<GSLNetDevice>()
        ->GetQueue();

    uint32_t max_ND_queue_size = dev_queue->GetMaxSize().GetValue();
    uint32_t curr_ND_queue_size = dev_queue->GetNPackets();
    
    // //DEBUG INFO on netdevice queue info
    // Ptr<Queue<Packet>> dev_queue =  dev->GetObject<GSLNetDevice>()->GetQueue();
    // if (dev_queue) {
    //     uint32_t max_dev_size = dev_queue->GetMaxSize().GetValue();
    //     uint32_t curr_dev_size = dev_queue->GetNPackets();
    //     printf("NetQueue (ND) max: %d curr: %d\n", max_dev_size, curr_dev_size);
    // } else {
    //     printf("NetQueue (ND): not found\n");
    // }
    
    // //DEBUG INFO on traffic control queue
    // Ptr<TrafficControlLayer> tc = node->GetObject<TrafficControlLayer>();
    // if (tc) {
    //     Ptr<QueueDisc> qdisc = tc->GetRootQueueDiscOnDevice(dev);
    //     if (qdisc) {
    //         uint32_t max_tc_size = qdisc->GetMaxSize().GetValue();
    //         uint32_t curr_tc_size = qdisc->GetNPackets();
    //         printf("TrafficControl (QD) max: %d curr: %d\n", max_tc_size, curr_tc_size);
    //     } else {
    //         printf("TrafficControl (QD): not installed\n");
    //     }
    // } else {
    //     printf("TrafficControlLayer: not found\n");
    // }
    //calculate filled percentage
        
    return double(curr_ND_queue_size) / (max_ND_queue_size); // how filled the queue is (0 <- empty, 1 <- full)

}

uint32_t ArbiterGSPriorityDeflection::GenerateIpHash(
        Ptr<const Packet> pkt,
        Ipv4Header const &ipHeader,
        bool is_request_for_source_ip_so_no_next_header
) {
    //basic packet info
    uint32_t src_ip = ipHeader.GetSource().Get();
    uint32_t dst_ip = ipHeader.GetDestination().Get();
    uint8_t proto = ipHeader.GetProtocol();
    uint16_t src_port = 0;
    uint16_t dst_port = 0;
    uint8_t ttl = ipHeader.GetTtl();

    if (!is_request_for_source_ip_so_no_next_header) {
        if (proto == 6) { // TCP
            TcpHeader tcpHeader;
            pkt->PeekHeader(tcpHeader);
            src_port = tcpHeader.GetSourcePort();
            dst_port = tcpHeader.GetDestinationPort();
        } else if (proto == 17) { // UDP
            UdpHeader udpHeader;
            pkt->PeekHeader(udpHeader);
            src_port = udpHeader.GetSourcePort();
            dst_port = udpHeader.GetDestinationPort();
        }
    }
    uint32_t hash = src_ip;
    hash ^= (dst_ip << 7) | (dst_ip >> 25);
    hash ^= (proto << 13);
    hash ^= (src_port << 3) | (src_port >> 13);
    hash ^= (dst_port << 11) | (dst_port >> 5);
    hash ^= (ttl << 17) | (ttl >> 3);
    hash *= 2654435761u;
    hash ^= (hash >> 16);
    return hash;
}

std::tuple<uint32_t, double> ArbiterGSPriorityDeflection::GenerateDeflectionInfo(
        DeflectionType deflectionType, 
        Ptr<const Packet> pkt,
        Ipv4Header const &ipHeader,
        bool is_request_for_source_ip_so_no_next_header
) {
    std::srand(std::time(nullptr));
    uint32_t r_int = 0; // decides which routing option to deflect to
    float r = 1;        // determines if we should deflect

    if (deflectionType == PACKET) {         // we deflect on a per packet basis
        r_int = rand();
        r = ((double) r_int / (RAND_MAX));
        
    } else if (deflectionType == FLOW)  {   // we deflect on a per flow basis
        r_int = GenerateIpHash(pkt, ipHeader, is_request_for_source_ip_so_no_next_header);
        r = double(r_int % 65536) / 65535;
    } else if (deflectionType == ALWAYS) {  // if a checkpoint is passed, we deflect 
        r_int = rand();
    }
    return std::make_tuple(r_int, r);

}

int ArbiterGSPriorityDeflection::CountPassedCheckpoints(double filled, DeflectionFlags flags) {
    int num_passed = 0; 

    //we just go through each flag and see if the flag exists and if we passed this checkpoint yet
    for (size_t i = 0; i < 15; ++i) {
        const auto& cp = GetCheckpoint(i);
        if (flags & cp.flag) {
            if (filled >= cp.threshold) { 
                num_passed++;
            } else { // if we fail this checkpoint, we wont pass any others past this
                break;
            }
        }
    }
    return num_passed;
}   

double ArbiterGSPriorityDeflection::GetChanceDeflected(DeflectionFlags flags, double filled, int passed_checkpoints) {
    double highest_passed = 0.0;

    for (size_t i = 0; i < 15; ++i) {
        const auto& cp = GetCheckpoint(i);
        if (flags & cp.flag) { 
            if (filled >= cp.threshold) {
                highest_passed = std::max(highest_passed, cp.threshold);
            } else { // if we fail this checkpoint, we wont pass any others past this
                break;
            }
        }
    }
    return highest_passed;
}   


int ArbiterGSPriorityDeflection::hashFunc(
        Ptr<const Packet> pkt, 
        Ipv4Header const &ipHeader, 
        bool is_request_for_source_ip_so_no_next_header, 
        const std::vector<std::tuple<int32_t, int32_t, int32_t>> next_hop_options
) {
    //CONFIG VARIABLES
    // If static, the percent you have to beat is the checkpoint type
    // If not static, filled is what you have to beat (assuming a checkpoint has been passed), 
    // and each checkpoint will make it easier
    bool is_static_rate = false;   
    DeflectionType deflection_type = FLOW; 
    // At what ratio(s) of being filled should we make it easier to deflect
    DeflectionFlags flags = NEVER;//(DeflectionFlags)(OVER_60 | OVER_75 | OVER_90 | OVER_95 | CRITICAL);

    double deflected_chance = GetQueueRatio(std::get<1>(next_hop_options[0]));
    int passed_checkpoints = CountPassedCheckpoints(deflected_chance, flags);

    //print out debug info
    printf("P(0)/F(1): %d, is_static: %d, flags: %d, fullness: %.4f\n", deflection_type, is_static_rate, flags, deflected_chance);
    
    //if no checkpoints are passed or there are no deflection options, we will NEVER deflect anyways
    if (passed_checkpoints > 0 && next_hop_options.size() > 1) {
        uint32_t r_int; // decides which routing option to deflect to
        float r;        // determines if we should deflect
        std::tie(r_int, r) = GenerateDeflectionInfo(deflection_type, pkt, ipHeader, is_request_for_source_ip_so_no_next_header);

        //adjust rates
        if (is_static_rate) { // set to highest passed flag
            deflected_chance = GetChanceDeflected(flags, deflected_chance, passed_checkpoints);
        } else { // set to the percent chance we pass
            // for every checkpoint pass, raise the deflected_chance
            for (int i; i < passed_checkpoints - 1; i++) { 
                // increases the (0,1) value, increasing the chance we deflect
                deflected_chance = pow(deflected_chance, .8); 
            }
        }
        printf("We passed %d checkpoint(s). r: %.4f, deflection chance: %.4f\n", passed_checkpoints, r, deflected_chance);
        
        if (r <= deflected_chance) { //we now want to deflect the packet
            //select a random idx that is NOT 0, in range of options
            uint32_t options = next_hop_options.size() - 1;
            uint32_t idx = (r_int % options) + 1;
            printf("DEFLECTING TO %d OF OPTIONs %d\n", idx, options);
            return idx;
        }
    }
    return 0; // we did not deflect -> return zero
}

}
