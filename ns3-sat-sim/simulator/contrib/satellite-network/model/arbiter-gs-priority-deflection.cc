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
#include <random>
#include <algorithm>
#include <numeric>


// costumizable deflection features
#define VERBOSE false        
#define FLAGS OVER_0            // BEST: 0
#define DEFLECTION_TYPE RNG     //
#define DEFLECTION_BOOST 0
#define STATIC false


#define USING_FLOW_CACHE true
#define CACHE_REFRESH_RATE 0    // BEST: 0   (ms)
#define ENTRY_EXPIRE_RATE 150   // BEST: 150 (ms)
#define THRESHOLD_SIZE .4
#define TTL_HOP_LIMIT 0
#define MAX_DEFLECTIONS 8







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
        m_refresh_time = Simulator::Now() + MilliSeconds(CACHE_REFRESH_RATE);
        m_next_hop_list = next_hop_list;
    }

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
            return std::make_tuple(-2, -2, -2);
        }
        
        //get default (straightforward) routing option + its info
        std::tuple<int32_t, int32_t, int32_t> to_return = next_hops[0];
        int32_t next_node_id, my_if_id, next_if_id;
        std::tie(next_node_id, my_if_id, next_if_id) = to_return;
        
        //if something went wrong, we will simply return here (dropped, no path found)
        if (next_node_id < 0 || my_if_id < 0 ||  next_if_id < 0) {
            printf("Err occured\n");
            for (auto hop : next_hops) {
                std::tie( next_node_id, my_if_id, next_if_id) = hop;
                std::cout << "next_node_id: " << next_node_id << " my_if_id: " << my_if_id << " next_if_id" << next_if_id << std::endl;
            }
            return to_return; // err
        }
    
        //if we are at the last hop before gs and have options
        bool fin = IsFinalHopToGS(next_node_id, my_if_id);

        if (fin && next_hops.size() > 1) { 
            //the core logic is in HashFunc
            int32_t n_if = 0;
            n_if =  USING_FLOW_CACHE 
                    ? CacheHasheFunc(pkt, ipHeader, is_request_for_source_ip_so_no_next_header, next_hops)
                    : HashFunc(pkt, ipHeader, is_request_for_source_ip_so_no_next_header, next_hops);
            if (int32_t(n_if) == -1) {
                if (pkt) {
                    LastDeflectionTag tag;
                    pkt->PeekPacketTag(tag);
                    std::cout << "ID: " << pkt->GetUid() << " from " << m_node_id << " Path: ";
                    tag.Print(std::cout);
                    std::cout << " ttl: "  << int(ipHeader.GetTtl());
                    
                }
                std::cout << "Dropped!\n";
                return std::make_tuple(-1, -1, -1);
            }
            to_return = next_hops[n_if];
            
            if (VERBOSE) {
                printf("SENT TO %d\n\n", n_if);
            }
            

            //check for errors
            int32_t next_node_id, my_if_id, next_if_id; 
            std::tie(next_node_id, my_if_id, next_if_id) = to_return;

            if (pkt) {
                LastDeflectionTag tag;
                pkt->PeekPacketTag(tag);
                std::cout << "ID: " << pkt->GetUid() << " from " << m_node_id << " to " << next_node_id << " Path: ";
                tag.Print(std::cout);
                std::cout << " ttl: "  << int(ipHeader.GetTtl()) << " |-|\n";
                
            }
            
            if (next_node_id < 0 || my_if_id < 0 ||  next_if_id < 0) {
                return next_hops[0];
            }

            // mark this packet if deflected
            if (pkt && n_if > 0) {
                Ptr<Packet> nonConstPkt = const_cast<Packet*>(PeekPointer(pkt));
                LastDeflectionTag tag;
                if (nonConstPkt->RemovePacketTag(tag)) {
                    tag.AddNodeId(m_node_id); 
                    nonConstPkt->AddPacketTag(tag);
                } else {
                    std::vector<int32_t> path = { m_node_id };
                    tag = LastDeflectionTag(path);
                    nonConstPkt->AddPacketTag(tag);
                }   
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

    void ArbiterGSPriorityDeflection::PrintFlowFromIpv4Header(const Ipv4Header &ipv4Header, Ptr<const Packet> packet) {
        Ptr<Packet> copy = packet->Copy(); // so nothing is changed

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
                    protocol = "No TCP Header Found)";
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
                    protocol = "No UDP Header Found)";
                }
                break;
            }
            default:
                protocol = "???";
        }

        // now print all the info
        std::cout << "Flow ID: "
                << ipv4Header.GetSource() << " -> " << ipv4Header.GetDestination()
                << " | Protocol: " << protocol
                << " | Ports: " << srcPort << " -> " << dstPort << std::endl;
    }

    ArbiterGSPriorityDeflection::Checkpoint 
    ArbiterGSPriorityDeflection::GetCheckpoint(int flag_index) {
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
            default: return { CRITICAL, 1.0 }; 
        }
    }

    bool ArbiterGSPriorityDeflection::IsGroundStation(int32_t node_id) {
        Ptr<Node> node = m_nodes.Get(node_id);
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        for (uint32_t i = 0; i < ipv4->GetNInterfaces(); ++i) { // for all interfaces/devices
            if (ipv4->GetNetDevice(i)->GetObject<PointToPointLaserNetDevice>() != nullptr) { // there is no isl
                return false;
            }
        }
        return true;
    }

    bool ArbiterGSPriorityDeflection::IsFinalHopToGS(int32_t next_node_id, int32_t my_if_id) {
        //we simply check if this is a GSL, and if so we need to ensure THIS node is a satelight and the next node is NOT
        auto node = m_nodes.Get(m_node_id);
        if (node == nullptr) {
            printf(" N ");
            return 0;
        }
        
        auto ipv4 = node->GetObject<Ipv4>();
        if (ipv4 == nullptr || ipv4->GetNInterfaces() == 0 || uint32_t(my_if_id) > ipv4->GetNInterfaces()) {
            printf(" I ");
            return 0;
        }
    
        auto dev = ipv4->GetNetDevice(my_if_id);
        if (dev == nullptr) {
            printf(" D ");
            return 0;
        }
        bool is_gsl = dev->GetObject<GSLNetDevice>() != 0;
        
        bool this_is_gs = IsGroundStation(m_node_id);
        bool next_is_gs = IsGroundStation(next_node_id);
        return is_gsl && next_is_gs && !this_is_gs;
    }

    Ptr<Queue<Packet>> ArbiterGSPriorityDeflection::GetQueue(int32_t if_id) {
        Ptr<Node> node = m_nodes.Get(m_node_id);
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        Ptr<NetDevice> dev = ipv4->GetNetDevice(if_id);
        Ptr<PointToPointLaserNetDevice> isl = dev->GetObject<PointToPointLaserNetDevice>();
        Ptr<GSLNetDevice> gsl =  dev->GetObject<GSLNetDevice>();
        
        Ptr<Queue<Packet>> to_return = nullptr;
        
        if (isl != nullptr) {
            if (VERBOSE&& false) {
                printf("ISL Link\t");
            }
            to_return = isl->GetQueue();
            
        } 
        if (gsl != nullptr) {
            if (VERBOSE&& false) {
                printf("GSL Link\t");
            }
            to_return = gsl->GetQueue(); 
        } 
        if (isl == nullptr && gsl == nullptr && VERBOSE&& false) {
            printf("Unknown queue\t");
            
        }
        return to_return;
    }

    // Gets the ratio the netdevice if filled given the if id for the link
    double ArbiterGSPriorityDeflection::GetQueueRatio(int32_t if_id) {
        Ptr<Queue<Packet>> dev_queue = GetQueue(if_id);
    
        uint32_t max_ND_queue_size = dev_queue->GetMaxSize().GetValue();
        uint32_t curr_ND_queue_size = dev_queue->GetNPackets();
        
        //calculate filled percentage  
        return double(curr_ND_queue_size) / (max_ND_queue_size); // how filled the queue is (0 <- empty, 1 <- full)
    }

    double ArbiterGSPriorityDeflection::NormalRNG() {
        static std::random_device rd;
        static std::mt19937 gen(rd());

        std::normal_distribution<> dist(0.5, 0.15); 

        double value = dist(gen);

        //clip value at [0,1]
        if (value < 0.0) {
            return 0.0;
        } else if (value > 1.0) {
            return 1.0;
        } else {
            return value;
        }
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
        uint16_t src_port;
        uint16_t dst_port;
        std::tie(src_port, dst_port) = GetPorts(pkt, proto, is_request_for_source_ip_so_no_next_header);
        uint8_t ttl = ipHeader.GetTtl();
        
        //calculate hash
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

    std::tuple<uint16_t, uint16_t> 
    ArbiterGSPriorityDeflection::GetPorts(Ptr<const Packet> pkt, uint8_t proto, bool is_request_for_source_ip_so_no_next_header) {
        if (!is_request_for_source_ip_so_no_next_header) {
            if (proto == 6) { // TCP
                TcpHeader tcpHeader;
                pkt->PeekHeader(tcpHeader);
                return std::make_tuple(tcpHeader.GetSourcePort(), tcpHeader.GetDestinationPort());
            } else if (proto == 17) { // UDP
                UdpHeader udpHeader;
                pkt->PeekHeader(udpHeader);
                return std::make_tuple(udpHeader.GetSourcePort(), udpHeader.GetDestinationPort());
            }
        } 
        return std::make_tuple(0,0);
        
    }

    double ArbiterGSPriorityDeflection::GenerateDeflectionInfo(
            Ptr<const Packet> pkt,
            Ipv4Header const &ipHeader,
            bool is_request_for_source_ip_so_no_next_header
    ) {
        float r = 1;        // determines if we should deflect
                            // as r -> 0, our chances of deflection go up

        if (DEFLECTION_TYPE == RNG) {         // we deflect on a per packet basis
            r = NormalRNG();
            
        } else if (DEFLECTION_TYPE == HASH)  {   // we deflect on a per flow basis
            uint32_t r_int = GenerateIpHash(pkt, ipHeader, is_request_for_source_ip_so_no_next_header);
            r = double(r_int % 65536) / 65535;
        } else if (DEFLECTION_TYPE == ALWAYS) {  // if a checkpoint is passed, we deflect 
            r = 1;
        }
        return r;

    }

    uint32_t ArbiterGSPriorityDeflection::CountPassedCheckpoints(double filled, DeflectionFlags flags) {
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
        if (STATIC) { // set to highest passed flag false
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
        } else { // set to the percent chance we pass
            // for every checkpoint pass, raise the deflected_chance
            double exponent = pow(0.9, passed_checkpoints - 1);
            return pow(filled, exponent);
        }
    }   

    int32_t ArbiterGSPriorityDeflection::ChooseIDX(const std::vector<std::tuple<int32_t, int32_t, int32_t>> next_hop_options) {
        std::vector<double> weights;
        for (size_t i = 1; i < next_hop_options.size(); ++i) {
            auto& option = next_hop_options[i];
            int32_t next_if = std::get<1>(option);
            auto queue = GetQueue(next_if);
            weights.push_back(1 - double(queue->GetNPackets()) / queue->GetMaxSize().GetValue());
        }
        double total_weight = std::accumulate(weights.begin(), weights.end(), 0.0); 

        // if all are equal, choose randomly
        if (total_weight == 0.0) {
            return 1 + (rand() % (next_hop_options.size() - 1));
        }

        // else choose based on a distribution
        std::mt19937 gen(static_cast<uint32_t>(rand())); 
        std::discrete_distribution<> dist(weights.begin(), weights.end());
        return 1;//dist(gen) + 1;
    }

    int32_t ArbiterGSPriorityDeflection::HashFunc(
            Ptr<const Packet> pkt, 
            Ipv4Header const &ipHeader, 
            bool is_request_for_source_ip_so_no_next_header, 
            const std::vector<std::tuple<int32_t, int32_t, int32_t>> next_hop_options
    ) {
        // total set flags
        DeflectionFlags flags = (DeflectionFlags)(FLAGS);

        double deflected_chance = GetQueueRatio(std::get<1>(next_hop_options[0]));
        int passed_checkpoints = CountPassedCheckpoints(deflected_chance, flags);
        
        //if no checkpoints are passed or there are no deflection options, we will NEVER deflect anyways
        if (passed_checkpoints > 0 && next_hop_options.size() > 1) {
            float r;        // determines if we should deflect
            r = GenerateDeflectionInfo(pkt, ipHeader, is_request_for_source_ip_so_no_next_header);
            //adjust rates
            deflected_chance = GetChanceDeflected(flags, deflected_chance, passed_checkpoints);
            
            
            if (r < deflected_chance + DEFLECTION_BOOST) { //we now want to deflect the packet
                //select a random idx that is NOT 0, in range of options
                int32_t idx = ChooseIDX(next_hop_options);
                if (VERBOSE && false) {
                    printf("DEFLECTING TO %d OF OPTIONs %ld to %d\n", idx, next_hop_options.size() - 1, std::get<0>(next_hop_options[idx]));
            
                }
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

            uint32_t next_if = std::get<0>(value);
            uint8_t ttl_min = std::get<1>(value);
            double fullness = std::get<2>(value);
            Time timestamp = std::get<3>(value);
            

            auto ms = timestamp.GetMilliSeconds();
            ms = ms < 0 ? 0 : ms;

            std::cout << "("
                    << src_ip << ", "
                    << dst_ip << ", "
                    << src_port << ", "
                    << dst_port << ") -> "
                    << "(n_if: "
                    << next_if << ", ttl_min: "
                    << static_cast<int>(ttl_min) << ", Fullness:"
                    << fullness << ", "
                    << "ms till expiry: " << ms
                    << ")\n";
        }
    }

    ArbiterGSPriorityDeflection::CacheRecomputingState 
    ArbiterGSPriorityDeflection::CheckRecompute(
            uint8_t ttl, 
            double filled_ratio, 
            Time curr_time, 
            std::tuple<uint32_t, uint32_t, uint16_t, uint16_t> key,
            LastDeflectionTag tag
    ) {
        if (CACHE_REFRESH_RATE != 0 && curr_time > m_refresh_time) {    // check if we should erase whole cache
            if(VERBOSE && false) { printf("Clearing cache\n"); }
            m_refresh_time = curr_time + MilliSeconds(CACHE_REFRESH_RATE);
            m_cache.clear();
            return RECOMPUTE;
        } 
        if (m_cache.find(key) == m_cache.end()) {                // check if flow is found in the cache
            if(VERBOSE && false) { printf("Not found\n"); }
            return RECOMPUTE;
        } 
        
        //get cache info
        auto cache_entry = m_cache[key];
        uint8_t min_ttl = std::get<1>(cache_entry);
        bool deflecting = std::get<0>(cache_entry) != 0;
        double threshold = std::get<2>(cache_entry);
        Time expiraton_date = std::get<3>(cache_entry);
        if (ttl <= min_ttl) {
            if (ttl <= 2) {
                return NormalRNG() < filled_ratio ? FORWARD : DROP;
            } else {
                return GOTBACK;
            }
        } else if (tag.GetPath().size() >= MAX_DEFLECTIONS) { // if too many deflections
            return NormalRNG() < filled_ratio ? FORWARD : DROP;
        } else if (curr_time >= expiraton_date) {                          // if entry is expired
            return RECOMPUTE;
        }
        else if(deflecting) {                                     // if we are deflecting and...
            if (filled_ratio == 0) {                                // GSL queue is empty
                return FORWARD;                                     
            } else if (filled_ratio < threshold - THRESHOLD_SIZE) { // We went below the threshold (too empty)
                return RECOMPUTE;
            }
        } else {                                                    // if we are not deflecting and...              
            if (filled_ratio == 1) {                                // GSL queue is full
                return DEFLECT; 
            } else if (filled_ratio > threshold + THRESHOLD_SIZE) { // We went above the threshold (too full)
                return RECOMPUTE;
            }
        } 
        return USE;
    }

    int32_t ArbiterGSPriorityDeflection::GetCacheValue(
            CacheRecomputingState is_recomputing,
            Ptr<const Packet> pkt, 
            Ipv4Header const &ipHeader, 
            bool is_request_for_source_ip_so_no_next_header, 
            const std::vector<std::tuple<int32_t, int32_t, int32_t>> next_hop_options,
            std::tuple<uint32_t, uint32_t, uint16_t, uint16_t> key

    ) {
        auto new_exp_time = Simulator::Now() + MilliSeconds(ENTRY_EXPIRE_RATE); // this is the new expiration time, if this flow becomes inactive
       
        if (is_recomputing == RECOMPUTE || is_recomputing == GOTBACK ) { // we need to recompute/compute the cache entry
            int flow_fid = HashFunc(
                            pkt, 
                            ipHeader, 
                            is_request_for_source_ip_so_no_next_header,
                            next_hop_options
                    );
            int min_ttl = std::min(3, ipHeader.GetTtl() - TTL_HOP_LIMIT);
            double filled_ratio = GetQueueRatio(std::get<1>(next_hop_options[0]));
            m_cache[key] = std::make_tuple(flow_fid, min_ttl, filled_ratio, new_exp_time);
            return flow_fid;
        } else if (is_recomputing == FORWARD) {
            int min_ttl = std::min(3, ipHeader.GetTtl() - TTL_HOP_LIMIT);
            double filled_ratio = GetQueueRatio(std::get<1>(next_hop_options[0]));
            m_cache[key] = std::make_tuple(0, min_ttl, filled_ratio, new_exp_time);
            return 0;
        } else if (is_recomputing == DROP) {
            // we should forward in the table for the future
            int min_ttl = std::min(3, ipHeader.GetTtl() - TTL_HOP_LIMIT);
            double filled_ratio = GetQueueRatio(std::get<1>(next_hop_options[0]));
            m_cache[key] = std::make_tuple(-1, min_ttl, filled_ratio, new_exp_time); // we want to forward
            return -1; //but we drop
        } else { // we can just use the cache value
            auto cache_entry = m_cache[key];
            int flow_fid = std::get<0>(cache_entry);
            m_cache[key] = std::make_tuple(flow_fid, std::get<1>(cache_entry), std::get<2>(cache_entry), new_exp_time);
            return flow_fid;
        }
    }

    int32_t ArbiterGSPriorityDeflection::CacheHasheFunc(
            Ptr<const Packet> pkt, 
            Ipv4Header const &ipHeader, 
            bool is_request_for_source_ip_so_no_next_header, 
            const std::vector<std::tuple<int32_t, int32_t, int32_t>> next_hop_options
    ) {        
        // get key for cache
        uint32_t src_ip = ipHeader.GetSource().Get();
        uint32_t dst_ip = ipHeader.GetDestination().Get();
        uint16_t src_port; uint16_t dst_port;
        std::tie(src_port, dst_port) = GetPorts(pkt, ipHeader.GetProtocol(), is_request_for_source_ip_so_no_next_header);
        std::tuple<uint32_t, uint32_t, uint16_t, uint16_t> key = std::make_tuple(src_ip, dst_ip, src_port, dst_port);
        
        double filled_ratio = GetQueueRatio(std::get<1>(next_hop_options[0]));
        

        if (false && VERBOSE && pkt) {
            PrintFlowFromIpv4Header(ipHeader, pkt);
            std::cout << "is_static: " << STATIC << " flags: " << (FLAGS) << " fullness: " << filled_ratio << " Time (ms): " << Simulator::Now().GetMilliSeconds() << std::endl;
            PrintCache();
        } 
        
        // check if we should compute/recompute the entry
        LastDeflectionTag tag;
        CacheRecomputingState recomp = CheckRecompute(ipHeader.GetTtl(), filled_ratio, Simulator::Now(), key, tag);
        
        
        
        return GetCacheValue(recomp, pkt, ipHeader, is_request_for_source_ip_so_no_next_header, next_hop_options, key);
    }
    
}