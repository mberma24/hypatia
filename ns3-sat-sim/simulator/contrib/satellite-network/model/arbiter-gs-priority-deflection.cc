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


// costumizable deflection features
#define FLAGS OVER_0
#define DEFLECTION_TYPE FLOW
#define DEFLECTION_BOOST 0
#define STATIC false


#define USING_FLOW_CACHE true
#define CACHE_REFRESH_RATE 1000 // ms
#define ENTRY_EXPIRE_RATE 100   // ms
#define THRESHOLD_SIZE .5 
#define TTL_HOP_LIMIT 0





namespace ns3 {

void
ArbiterGSPriorityDeflection::TrackFlowFromPacket(const ns3::Ipv4Header &ipHeader, ns3::Ptr<const ns3::Packet> pkt)
{
    Ptr<Packet> copy = pkt->Copy();
    uint8_t protocol = ipHeader.GetProtocol();

    uint16_t srcPort = 0;
    uint16_t dstPort = 0;

    if (protocol == 6) { // TCP
        ns3::TcpHeader tcpHeader;
        if (copy->PeekHeader(tcpHeader)) {
            srcPort = tcpHeader.GetSourcePort();
            dstPort = tcpHeader.GetDestinationPort();
        }
    } else if (protocol == 17) { // UDP
        ns3::UdpHeader udpHeader;
        if (copy->PeekHeader(udpHeader)) {
            srcPort = udpHeader.GetSourcePort();
            dstPort = udpHeader.GetDestinationPort();
        }
    }

    FlowKey key = {
        ipHeader.GetSource(),
        ipHeader.GetDestination(),
        protocol,
        srcPort,
        dstPort
    };

    m_uniqueFlows.insert(key); // Insert if not already tracked
}

size_t
ArbiterGSPriorityDeflection::GetUniqueFlowCount() const {
    return m_uniqueFlows.size();
}


void PrintFlowFromIpv4Header(const Ipv4Header &ipv4Header, Ptr<const Packet> packet) {
    Ptr<Packet> copy = packet->Copy(); // Don't modify the original

    std::string protocol;
    uint16_t srcPort = 0, dstPort = 0;

    switch (ipv4Header.GetProtocol()) {
        case 6: { // TCP
            TcpHeader tcpHeader;
            if (copy->PeekHeader(tcpHeader)) {
                srcPort = tcpHeader.GetSourcePort();
                dstPort = tcpHeader.GetDestinationPort();
                protocol = "TCP";
            } else {
                protocol = "TCP (no header found)";
            }
            break;
        }
        case 17: { // UDP
            UdpHeader udpHeader;
            if (copy->PeekHeader(udpHeader)) {
                srcPort = udpHeader.GetSourcePort();
                dstPort = udpHeader.GetDestinationPort();
                protocol = "UDP";
            } else {
                protocol = "UDP (no header found)";
            }
            break;
        }
        default:
            protocol = "Unknown";
    }

    std::cout << "Flow ID: "
              << ipv4Header.GetSource() << " -> " << ipv4Header.GetDestination()
              << " | Protocol: " << protocol
              << " | Ports: " << srcPort << " -> " << dstPort << std::endl;
}

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
    m_start_time = Simulator::Now();
    m_refresh_time = Simulator::Now() + MilliSeconds(CACHE_REFRESH_RATE);
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

    if (true) { //IsFinalHopToGS(m_node_id, next_node_id, my_if_id) && next_hops.size() > 1
        //DEBUG PRINT TCP SEQ #
        auto Q = getQueue(my_if_id, next_node_id);

        printf("cnid: %d nnid: %d my_if_id: %d next_if_id: %d Queue len: %.4f\t", m_node_id, next_node_id, my_if_id, next_if_id, double(Q->GetNPackets()) / Q->GetMaxSize().GetValue());
        if (pkt) {
            printf("PKT_ID: %ld, size: %d\t", pkt->GetUid(), pkt->GetSize());

        } else {
            std::cout << "no packet";
        }
        if (!is_request_for_source_ip_so_no_next_header) {
            
            uint8_t proto = ipHeader.GetProtocol();
            if (proto == 6) { // TCP
                TcpHeader tcpHeader;
                pkt->PeekHeader(tcpHeader);
                std::cout << "Seq:" << tcpHeader.GetSequenceNumber().GetValue() << std::endl;
            } else if (proto == 17) { // UDP
                
                std::cout << "UDP" << std::endl;

            } else {
                std::cout << "???" << std::endl;
            }
        } else {
            std::cout << "IRFSISNNH" << std::endl;
        }
    }
    
   
    //if we are at the last hop before gs and have options
    if (IsFinalHopToGS(m_node_id, next_node_id, my_if_id) && next_hops.size() > 1) { 
        //the core logic is in hashFunc
        auto n_if = 0;
        if (USING_FLOW_CACHE) {
            n_if = cacheHasheFunc(pkt, ipHeader, is_request_for_source_ip_so_no_next_header, next_hops);
        } else {
            n_if = hashFunc(pkt, ipHeader, is_request_for_source_ip_so_no_next_header, next_hops);
        }
        to_return = next_hops[n_if];
        getQueue(std::get<1>(to_return), std::get<0>(to_return));
        printf("SENT TO %d\n\n", n_if);

       
        
        //check for errors
        uint32_t next_node_id, my_if_id, next_if_id; 
        std::tie(next_node_id, my_if_id, next_if_id) = to_return;
        if (next_node_id < 0 || my_if_id < 0 ||  next_if_id < 0) {
            std::cout << "ERR: SOMETHING WENT WRONG" << std::endl;
            return next_hops[0];
        }
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
        case  1: return { OVER_5,  0.05   };
        case  2: return { OVER_10, 0.1   };
        case  3: return { OVER_20, 0.2   };
        case  4: return { OVER_30, 0.3   };
        case  5: return { OVER_40, 0.4   };
        case  6: return { OVER_50, 0.5  };
        case  7: return { OVER_60, 0.6   };
        case  8: return { OVER_70, 0.6  };
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
    for (uint32_t i = 0; i < ipv4->GetNInterfaces(); ++i) { // for all devices
        if (ipv4->GetNetDevice(i)->GetObject<PointToPointLaserNetDevice>() != nullptr) { // there is no isl
            return false;
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
    // if (dev_queue) {
    //     uint32_t max_dev_size = dev_queue->GetMaxSize().GetValue();
    //     uint32_t curr_dev_size = dev_queue->GetNPackets();
    //     printf("NetQueue (ND) max: %d curr: %d\n", max_dev_size, curr_dev_size);
    //     printf("name: %s\n", dev_queue->GetInstanceTypeId().GetName().c_str());
    // } else {
    //     printf("NetQueue (ND): not found\n");
    // }
    
    // //DEBUG INFO on traffic control queue
    // Ptr<TrafficControlLayer> tc = m_nodes.Get(m_node_id)->GetObject<TrafficControlLayer>();
    // if (tc) {
    //     Ptr<QueueDisc> qdisc = tc->GetRootQueueDiscOnDevice(m_nodes.Get(m_node_id)->GetObject<Ipv4>()->GetNetDevice(if_id));
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
        Ptr<const Packet> pkt,
        Ipv4Header const &ipHeader,
        bool is_request_for_source_ip_so_no_next_header
) {
    uint32_t r_int = 0; // decides which routing option to deflect to
    float r = 1;        // determines if we should deflect

    if (DEFLECTION_TYPE == PACKET) {         // we deflect on a per packet basis
        r_int = rand();
        r = ((double) r_int / (RAND_MAX));
        printf("GOT R: %.4f\n", r);
        
    } else if (DEFLECTION_TYPE == FLOW)  {   // we deflect on a per flow basis
        r_int = GenerateIpHash(pkt, ipHeader, is_request_for_source_ip_so_no_next_header);
        r = double(r_int % 65536) / 65535;
    } else if (DEFLECTION_TYPE == ALWAYS) {  // if a checkpoint is passed, we deflect 
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


Ptr<Queue<Packet>> ArbiterGSPriorityDeflection::getQueue(uint32_t if_id, uint32_t target_node) {
    Ptr<Node> node = m_nodes.Get(m_node_id);
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    Ptr<NetDevice> dev = ipv4->GetNetDevice(if_id);
    Ptr<PointToPointLaserNetDevice> isl = dev->GetObject<PointToPointLaserNetDevice>();
    Ptr<GSLNetDevice> gsl =  dev->GetObject<GSLNetDevice>();
    
    Ptr<Queue<Packet>> to_return = nullptr;
    
    if (isl != nullptr) {
        printf("ISL Link: %d to %d via %d\t", m_node_id, target_node, if_id);
        to_return = isl->GetQueue();
        
    } 
    if (gsl != nullptr) {
        printf("GSL Link: %d to %d via %d\t", m_node_id, target_node, if_id);
        to_return = gsl->GetQueue(); 
    } 
    if (isl == nullptr && gsl == nullptr) {
        printf("Unknown queue: %d to %d via %d\t", m_node_id, target_node, if_id);
    }
    return to_return;

    

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
    // At what ratio(s) of being filled should we make it easier to deflect
    DeflectionFlags flags = (DeflectionFlags)(FLAGS);


    double deflected_chance = GetQueueRatio(std::get<1>(next_hop_options[0]));
    int passed_checkpoints = CountPassedCheckpoints(deflected_chance, flags);

    //print out debug info
    //printf("P(0)/F(1): %d, is_static: %d, flags: %d, fullness: %.4f\n", DEFLECTION_TYPE, STATIC, flags, deflected_chance);
    
    //if no checkpoints are passed or there are no deflection options, we will NEVER deflect anyways
    if (passed_checkpoints > 0 && next_hop_options.size() > 1) {
        uint32_t r_int; // decides which routing option to deflect to
        float r;        // determines if we should deflect
        std::tie(r_int, r) = GenerateDeflectionInfo(pkt, ipHeader, is_request_for_source_ip_so_no_next_header);
        //adjust rates
        if (STATIC) { // set to highest passed flag false
            deflected_chance = GetChanceDeflected(flags, deflected_chance, passed_checkpoints);
        } else { // set to the percent chance we pass
            // for every checkpoint pass, raise the deflected_chance
            double exponent = pow(0.9, passed_checkpoints - 1);
            deflected_chance = pow(deflected_chance, exponent);
        }
        //printf("We passed %d checkpoint(s). r: %.4f, deflection chance: %.4f\n", passed_checkpoints, r, deflected_chance);
        
        if (r < deflected_chance + DEFLECTION_BOOST) { //we now want to deflect the packet
            //select a random idx that is NOT 0, in range of options
            uint32_t options = next_hop_options.size() - 1;
            uint32_t idx = (r_int % options) + 1;
            //printf("DEFLECTING TO %d OF OPTIONs %d to %d\n", idx, options, std::get<0>(next_hop_options[idx]));
            return idx;
        }
    }

    return 0; // we did not deflect -> return zero
}

void ArbiterGSPriorityDeflection::PrintCache() {
    std::cout << "Time till refresh: " << m_refresh_time.GetMilliSeconds() - Simulator::Now().GetMilliSeconds() << std::endl;
    for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
        const std::tuple<uint32_t, uint32_t, uint16_t, uint16_t>& key = it->first;
        const std::tuple<uint32_t, uint8_t, double, Time>& value = it->second;

        uint32_t src_ip = std::get<0>(key);
        uint32_t dst_ip = std::get<1>(key);
        uint16_t src_port = std::get<2>(key);
        uint16_t dst_port = std::get<3>(key);

        uint32_t some_id = std::get<0>(value);
        uint8_t ttl_min = std::get<1>(value);
        double fullness = std::get<2>(value);
        Time timestamp = std::get<3>(value);
        

        auto ms = timestamp.GetMilliSeconds();
        ms = ms < 0 ? 0 : ms;

        std::cout << "Key: ("
                  << src_ip << ", "
                  << dst_ip << ", "
                  << src_port << ", "
                  << dst_port << ") -> "
                  << "Value: ("
                  << some_id << ", "
                  << static_cast<int>(ttl_min) << ", "
                  << fullness << ", "
                  << "ms till expiry: " << ms
                  << ")\n";
    }
}

int ArbiterGSPriorityDeflection::cacheHasheFunc(Ptr<const Packet> pkt, 
    Ipv4Header const &ipHeader, 
    bool is_request_for_source_ip_so_no_next_header, 
    const std::vector<std::tuple<int32_t, int32_t, int32_t>> next_hop_options
) {
    // I was always told to never meddle with forces I dont understand
    if (is_request_for_source_ip_so_no_next_header) {
        printf("idk what to do here\n");
        return 0;
    }

    //basic IP info
    uint8_t proto = ipHeader.GetProtocol();
    uint32_t src_ip = ipHeader.GetSource().Get();
    uint32_t dst_ip = ipHeader.GetDestination().Get();
    uint16_t src_port = 0;
    uint16_t dst_port = 0;
    uint8_t ttl = ipHeader.GetTtl();

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
    // get more info abt this packet
    Time curr_time = Simulator::Now();
    double filled_ratio = GetQueueRatio(std::get<1>(next_hop_options[0]));
    std::tuple<uint32_t, uint32_t, uint16_t, uint16_t> key = std::make_tuple(src_ip, dst_ip, src_port, dst_port);
    this->TrackFlowFromPacket(ipHeader, pkt);
    this->TrackFlowFromPacket(ipHeader, pkt);
    
    

    auto elapsed_ms = curr_time.GetMilliSeconds();
    if (pkt) {
        PrintFlowFromIpv4Header(ipHeader, pkt);
    } else {
        printf("No packet found\t");
    }
    std::cout << "is_static: " << STATIC << " flags: " << (FLAGS) << " fullness: " << filled_ratio << " Time (ms): " << elapsed_ms << "Num flows so far: " << this->GetUniqueFlowCount() << std::endl;
    PrintCache();
    
    // check if we should compute/recompute the entry
    bool recomp = false;

    
    if (curr_time > m_refresh_time) { // check if we should erase whole cache
        printf("Clearing cache\n");
        m_refresh_time = curr_time + MilliSeconds(CACHE_REFRESH_RATE);
        m_cache.clear();
        recomp = true;
    } else if (m_cache.find(key) == m_cache.end()) { // if not found in cache
        printf("Not found\n");
        recomp = true;
    } else {   
        auto cache_entry = m_cache[key];
        bool deflecting = std::get<0>(cache_entry) != 0;
        uint8_t min_ttl =  std::get<1>(cache_entry);
        double threshold = std::get<2>(cache_entry);
        Time expiraton_date = std::get<3>(cache_entry);
        
        if (
                curr_time >= expiraton_date                                     // if expired
                || ttl < min_ttl                                                // if ttl is too low
                || (filled_ratio < threshold - THRESHOLD_SIZE && deflecting)    // if under threshold (too empty)
                || (filled_ratio > threshold + THRESHOLD_SIZE && !deflecting)   // if passed threshold (too full)
        ) {
            printf("Recomputing\t time: %dttl: %d | < threshhold: %d | > threshhold %d | deflecting: %d\t", curr_time >= expiraton_date , ttl < min_ttl, (filled_ratio < threshold - THRESHOLD_SIZE), (filled_ratio > threshold + THRESHOLD_SIZE), deflecting);
            recomp = true;
        }
    }


    if (recomp) { // we need to recompute/compute the cache entry
        int flow_if = hashFunc(
                pkt, 
                ipHeader, 
                is_request_for_source_ip_so_no_next_header,
                next_hop_options);
        auto new_exp_time = curr_time + MilliSeconds(ENTRY_EXPIRE_RATE); 
        int min_ttl = std::min(1, ttl - TTL_HOP_LIMIT);
        m_cache[key] = std::make_tuple(flow_if, min_ttl, filled_ratio, new_exp_time);
        return flow_if;
    } else { // we can just use the cache value
        auto cache_entry = m_cache[key];
        int flow_if = std::get<0>(cache_entry);
        auto new_time = curr_time + MilliSeconds(ENTRY_EXPIRE_RATE); // this is the new expiration time, if this flow becomes inactive
        m_cache[key] = std::make_tuple(flow_if, std::get<1>(cache_entry), std::get<2>(cache_entry), new_time);
        return flow_if;
    }
}
}



/*
For the cache, timeout after x seconds

Redo deflections

Cache:
Flow, deflect to, expire_time
Flow, deflect to, expire_time
*/