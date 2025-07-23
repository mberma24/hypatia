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
/* General Deflection Settings */
#define VERBOSE false        
#define FLAGS OVER_0            // BEST: 0
#define STATIC false
#define DEFLECTION_BOOST 0


/* Cache Settings */
#define USING_FLOW_CACHE true       // Is the cache being used
#define CACHE_REFRESH_RATE 0        // How long until you need to clear the entire cache (0 for never)                  BEST: 0   (ms)
#define ENTRY_EXPIRE_RATE 150       // How long in simulation time (ms) until a cache entry expires.                    BEST: 150 (ms)
#define THRESHOLD_SIZE .4           // Threshold for how much the queue needs to change (%) to recalculate the value
#define TTL_HOP_LIMIT 0             // How much lower does a ttl need to be to recalculate the value
#define MAX_DEFLECTIONS 8           // How many deflections can occur per packet.
#define DROP_DECAY .75              // If dropping in cache, multiply the threshold need by this value.
#define REPEATED_DEFLECTIONS true

#define WEIGHT_DECAY_RATE .001
#define WEIGHT_LEARN_RATE .01








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
        m_uniform_time = Simulator::Now();
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

        
        PacketRoutingContext context {
                pkt,
                ipHeader,
                is_request_for_source_ip_so_no_next_header,
                next_hops,
                std::vector<double>()
        };

        CheckDecayWeights(target_node_id);
    
        //if we are at the last hop before gs and have options
        bool fin = IsFinalHopToGS(next_node_id, my_if_id);

        if (fin && next_hops.size() > 1) { 
            
            
            HandleWeights(context, target_node_id);
            //the core logic is in HashFunc
            int32_t n_if = 0;
            n_if =  USING_FLOW_CACHE 
                    ? CacheHasheFunc(context)
                    : HashFunc(context);
            if (int32_t(n_if) == -1) {
                if (VERBOSE) { std::cout << "Dropped!\n"; }
                return std::make_tuple(-1, -1, -1);
            }
            to_return = next_hops[n_if];

            //check for errors
            int32_t next_node_id, my_if_id, next_if_id; 
            std::tie(next_node_id, my_if_id, next_if_id) = to_return;

            if (pkt && VERBOSE) {
                LastDeflectionTag tag;
                pkt->PeekPacketTag(tag);
                std::cout << "ID: " << pkt->GetUid() << " last: " << tag.GetLastNode() << " from: " << m_node_id << " to: " << next_node_id << " # def: " << int(tag.GetNumDeflections());
                std::cout << " ttl: "  << int(ipHeader.GetTtl()) << "\n";
            }
            
            // mark this packet if deflected
            if (pkt) {
                Ptr<Packet> nonConstPkt = const_cast<Packet*>(PeekPointer(pkt));
                LastDeflectionTag tag(m_node_id);
                if (nonConstPkt->PeekPacketTag(tag)) {
                    if (n_if > 0) {
                        tag.SetLastNode(m_node_id);
                        nonConstPkt->ReplacePacketTag(tag);
                    } else {
                        nonConstPkt->RemovePacketTag(tag);
                    }
                } else if (n_if > 0) {
                    nonConstPkt->AddPacketTag(LastDeflectionTag(m_node_id));
                } else {
                }
                
            }
        } 
        
        return to_return;
    }

    void ArbiterGSPriorityDeflection::SetDeflectionState(
            int32_t target_node_id, 
            const std::vector<std::tuple<int32_t, int32_t, int32_t>>& next_hops
    ) {
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

    void ArbiterGSPriorityDeflection::PrintFlowFromIpv4Header(const PacketRoutingContext& context) {
        Ptr<Packet> copy = context.pkt->Copy(); // so nothing is changed

        std::string protocol;
        uint16_t srcPort = 0, dstPort = 0;

        switch (context.ipHeader.GetProtocol()) {
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
                << context.ipHeader.GetSource() << " -> " << context.ipHeader.GetDestination()
                << " | Protocol: " << protocol
                << " | Ports: " << srcPort << " -> " << dstPort << std::endl;
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
    void ArbiterGSPriorityDeflection::CheckDecayWeights(int32_t target_node_id) {
        if (
                m_weights.find(target_node_id) != m_weights.end() 
                && Simulator::Now() >= m_uniform_time + MilliSeconds(1)
        ) { 
            DecayWeight(m_weights[target_node_id]); 
            m_uniform_time =  Simulator::Now();
        } 
    }

    void ArbiterGSPriorityDeflection::HandleWeights(PacketRoutingContext& context, int32_t target_node_id) {
        size_t n = context.nextHopOptions.size() - 1;
        if (m_weights.find(target_node_id) == m_weights.end()) { 
            // make new weights
            m_weights[target_node_id] = std::vector<double>(n, double(1) / (n)); 
 
            
            m_update_times[target_node_id] = Simulator::Now();
        } 
        
        if (Simulator::Now() <= m_update_times[target_node_id] + NanoSeconds(100)) {
            context.weights = m_weights[target_node_id];    
            return;
        }


        LastDeflectionTag tag;

        if (context.pkt->PeekPacketTag(tag) && tag.GetLastNode() > 0) {
            int32_t from_node_id = tag.GetLastNode(); // the node we received this pkt from
            for (size_t idx = 1; idx < context.nextHopOptions.size(); idx++) {
                auto hop = context.nextHopOptions[idx];
                int32_t next_hop_node_id = std::get<0>(hop); 
                if (from_node_id == next_hop_node_id) { // if we could resend, decrease that likelihood
                    BumpDownWeight(m_weights[target_node_id], idx - 1);
                    break;
                }
                
            }
            
        }


        context.weights = m_weights[target_node_id];         
    }

    ArbiterGSPriorityDeflection::Checkpoint 
    ArbiterGSPriorityDeflection::GetCheckpoint(int flag_index) {
        switch (flag_index){
            case  0: return { OVER_0,  0.0   };
            case  1: return { OVER_5,  0.05  };
            case  2: return { OVER_10, 0.1   };
            case  3: return { OVER_20, 0.2   };
            case  4: return { OVER_30, 0.3   };
            case  5: return { OVER_40, 0.4   };
            case  6: return { OVER_50, 0.5   };
            case  7: return { OVER_60, 0.6   };
            case  8: return { OVER_70, 0.6   };
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
        bool is_gsl = m_nodes.Get(m_node_id)->GetObject<Ipv4>()->GetNetDevice(my_if_id)->GetObject<GSLNetDevice>() != 0;
        
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
            to_return = isl->GetQueue();
            
        } 
        if (gsl != nullptr) {
            to_return = gsl->GetQueue(); 
        } 
        return to_return;
    }

    double ArbiterGSPriorityDeflection::GetQueueRatio(int32_t if_id) {
        Ptr<Queue<Packet>> dev_queue = GetQueue(if_id);
    
        uint32_t max_ND_queue_size = dev_queue->GetMaxSize().GetValue();
        uint32_t curr_ND_queue_size = dev_queue->GetNPackets();
        
        //calculate filled percentage  
        return double(curr_ND_queue_size) / (max_ND_queue_size); // how filled the queue is (0 <- empty, 1 <- full)
    }

    double ArbiterGSPriorityDeflection::NormalRNG(double avg = .5, double sd = .15) {
        static std::random_device rd;
        static std::mt19937 gen(rd());

        std::normal_distribution<> dist(avg, sd); 

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

    std::tuple<uint16_t, uint16_t> 
    ArbiterGSPriorityDeflection::GetPorts(const PacketRoutingContext& context) {
        uint8_t protocol = context.ipHeader.GetProtocol();
        if (!context.isRequestForSourceIpNoNextHeader) {
            if (protocol == 6) { // TCP
                TcpHeader tcpHeader;
                context.pkt->PeekHeader(tcpHeader);
                return std::make_tuple(tcpHeader.GetSourcePort(), tcpHeader.GetDestinationPort());
            } else if (protocol == 17) { // UDP
                UdpHeader udpHeader;
                context.pkt->PeekHeader(udpHeader);
                return std::make_tuple(udpHeader.GetSourcePort(), udpHeader.GetDestinationPort());
            }
        } 
        return std::make_tuple(0,0);
        
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

    double ArbiterGSPriorityDeflection::GetChanceDeflected(DeflectionFlags flags, double filled, int passed_cp) {
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
            double exponent = pow(0.9, passed_cp - 1);
            return pow(filled, exponent);
        }
    }   

    void ArbiterGSPriorityDeflection::Normalize(std::vector<double>& weights) {
        double sum = std::accumulate(weights.begin(), weights.end(), 0.0);
        if (sum > 0.0) {
            for (double& w : weights) {
                w /= sum;
            }
        } else { // uniform
            double uniform = 1.0 / weights.size();
            for (double& w : weights) {
                w = uniform;
            }
        }
    }

    void ArbiterGSPriorityDeflection::DecayWeight(std::vector<double>& weights) {
        double uniform_val = 1.0 / weights.size();
        for (auto& w : weights) {
            w = (1 - WEIGHT_DECAY_RATE) * w + WEIGHT_DECAY_RATE * uniform_val;
        }

        // if OVER 1, big problems
        if (std::accumulate(weights.begin(), weights.end(), 0.0) > 1) {
            Normalize(weights); // normalize for saftey
        }
    }

    void ArbiterGSPriorityDeflection::BumpWeight(std::vector<double>& weights, size_t i) {
        if (weights.empty() || i >= weights.size()) {
            return;
        }

        weights[i] += WEIGHT_LEARN_RATE;
    }

    void ArbiterGSPriorityDeflection::BumpDownWeight(std::vector<double>& weights, size_t i) {
        if (weights.empty() || i >= weights.size()) {
            return;
        }

        weights[i] -= WEIGHT_LEARN_RATE;
    }

    int32_t ArbiterGSPriorityDeflection::ChooseIDX(const PacketRoutingContext& context) { 
        std::vector<double> weights = context.weights;
        std::cout << "Weights: [";
        for (double w : weights) {
            std::cout << w << " ";
        }
        std::cout << "]" << std::endl;
        
        std::mt19937 gen(static_cast<uint32_t>(rand())); 
        std::discrete_distribution<> dist(weights.begin(), weights.end());
        return dist(gen) + 1;
        //return 1 + ( rand() % (context.nextHopOptions.size() - 1));
    }

    int32_t ArbiterGSPriorityDeflection::HashFunc(const PacketRoutingContext& context) {
        // total set flags
        DeflectionFlags flags = (DeflectionFlags)(FLAGS);

        double deflected_chance = GetQueueRatio(std::get<1>(context.nextHopOptions[0]));
        int passed_checkpoints = CountPassedCheckpoints(deflected_chance, flags);
        
        //if no checkpoints are passed or there are no deflection options, we will NEVER deflect anyways
        if (passed_checkpoints > 0 && context.nextHopOptions.size() > 1) {
            float r = NormalRNG();  // determines if we should deflect
            //adjust rates
            deflected_chance = GetChanceDeflected(flags, deflected_chance, passed_checkpoints);
            
            
            if (r < deflected_chance) { //we now want to deflect the packet
                //select a random idx that is NOT 0, in range of options
                int32_t idx = ChooseIDX(context);
                return idx;
            }
        }

        return 0; // we did not deflect -> return zero
    }

    int32_t ArbiterGSPriorityDeflection::GetNewCacheValue(
            const PacketRoutingContext& context,
            std::tuple<uint32_t, uint32_t, uint16_t, uint16_t> key
    ) {
        //PRE: value is may not be in the cache already and this is NOT urgent (needing to drop/fwd)
        double filled_ratio = GetQueueRatio(std::get<1>(context.nextHopOptions[0]));
        int32_t fwd = HashFunc(context);
        int min_ttl = std::min(3, context.ipHeader.GetTtl() - TTL_HOP_LIMIT);
        Time expire_time = Simulator::Now() + MilliSeconds(ENTRY_EXPIRE_RATE);
        m_cache[key] = std::make_tuple(fwd, min_ttl, filled_ratio, expire_time);
        return fwd;
    }

    int32_t ArbiterGSPriorityDeflection::GetUsedCacheValue(
            const PacketRoutingContext& context,
            std::tuple<uint32_t, uint32_t, uint16_t, uint16_t> key

    ) {
        uint8_t ttl = context.ipHeader.GetTtl();
        Time time = Simulator::Now();
        double filled_ratio = GetQueueRatio(std::get<1>(context.nextHopOptions[0]));
        
        auto cache_entry = m_cache[key];
        int32_t idx =  std::get<0>(cache_entry);
        bool deflecting = std::get<0>(cache_entry) != 0;

        uint8_t min_ttl = std::get<1>(cache_entry);
        double threshold = std::get<2>(cache_entry);
        double LThreshold = (idx == -1) ? THRESHOLD_SIZE : THRESHOLD_SIZE  * DROP_DECAY;
        Time expiraton_date = std::get<3>(cache_entry);
        

        bool recompute  =   time > expiraton_date 
                        ||  ttl < min_ttl
                        ||  (deflecting && 
                            (   filled_ratio <= 0
                            ||  filled_ratio < threshold - (LThreshold)
                            ))
                        ||  (!deflecting && 
                            (   filled_ratio >= 1
                            ||  filled_ratio > threshold + THRESHOLD_SIZE
                            ));

        if (recompute) {
            return GetNewCacheValue(context, key);
        } else {
            expiraton_date = time + MilliSeconds(ENTRY_EXPIRE_RATE);
            m_cache[key] = std::make_tuple(idx, min_ttl, threshold, expiraton_date);
            return idx;
        }
    }

    int32_t ArbiterGSPriorityDeflection::GetUrgentCacheValue(
            const PacketRoutingContext& context,
            std::tuple<uint32_t, uint32_t, uint16_t, uint16_t> key

    ) {
        double filled_ratio = GetQueueRatio(std::get<1>(context.nextHopOptions[0]));
        int32_t fwd =   NormalRNG(.6, .1) > GetQueueRatio(std::get<1>(context.nextHopOptions[0]))
                        ?  0    // forward
                        : -1;   // drop
        int min_ttl = std::min(3, context.ipHeader.GetTtl() - TTL_HOP_LIMIT);
        Time expire_time = Simulator::Now() + MilliSeconds(ENTRY_EXPIRE_RATE);
        m_cache[key] = std::make_tuple(fwd, min_ttl, filled_ratio, expire_time);
        return fwd;
    }

    std::pair<ArbiterGSPriorityDeflection::PacketRoutingContext, std::vector<size_t>> 
    ArbiterGSPriorityDeflection::RemoveUsedHops(const PacketRoutingContext& context) {
        LastDeflectionTag tag;
        context.pkt->PeekPacketTag(tag);
        const std::vector<int>& blacklist = { tag.GetLastNode() };  // for the lastdeflection tag, we 

        std::vector<std::tuple<int32_t, int32_t, int32_t>> filteredHops;
        std::vector<double> filteredWeights;
        std::vector<size_t> preservedIndices; // This tracks original indices that are preserved

        
        for (size_t i = 0; i < context.nextHopOptions.size(); ++i) {
            const auto& hop = context.nextHopOptions[i];
            int32_t nodeId = std::get<0>(hop);

            if (std::find(blacklist.begin(), blacklist.end(), nodeId) == blacklist.end()) {
                filteredHops.push_back(hop);
                preservedIndices.push_back(i);
                if (i > 0) {
                    filteredWeights.push_back(context.weights[i - 1]);
                }
                
            }
        }
        
        
        Normalize(filteredWeights);
        if (VERBOSE) {
            // Inline print Original
            std::cout << "Original: [";
            for (size_t i = 0; i < context.nextHopOptions.size(); ++i) {
                const auto& hop = context.nextHopOptions[i];
                std::cout << "(" << std::get<0>(hop) << ", "
                                << std::get<1>(hop) << ", "
                                << std::get<2>(hop) << ")";
                if (i + 1 < context.nextHopOptions.size()) std::cout << ", ";
            }
            std::cout << "]\n";

            // Inline print Filtered
            std::cout << "Filtered: [";
            for (size_t i = 0; i < filteredHops.size(); ++i) {
                const auto& hop = filteredHops[i];
                std::cout << "(" << std::get<0>(hop) << ", "
                                << std::get<1>(hop) << ", "
                                << std::get<2>(hop) << ")";
                if (i + 1 < filteredHops.size()) std::cout << ", ";
            }
            std::cout << "]\n";

            // Inline print Mapping
            std::cout << "Mapping: [";
            for (size_t i = 0; i < preservedIndices.size(); ++i) {
                std::cout << preservedIndices[i];
                if (i + 1 < preservedIndices.size()) std::cout << ", ";
            }
            std::cout << "]\n";
        }
   
        PacketRoutingContext newContext {
            context.pkt,
            context.ipHeader,
            context.isRequestForSourceIpNoNextHeader,
            filteredHops,
            filteredWeights,
        };
    

        return {newContext, preservedIndices};
    }


    int32_t ArbiterGSPriorityDeflection::GetCacheValue(
            const PacketRoutingContext& context,
            std::tuple<uint32_t, uint32_t, uint16_t, uint16_t> key
    ) {
        
        //if we need to forward or drop
        uint8_t ttl = context.ipHeader.GetTtl();
        LastDeflectionTag tag;
        bool urgent = ttl < 3 || (context.pkt->PeekPacketTag(tag) && tag.GetNumDeflections() > MAX_DEFLECTIONS);
        bool new_flow = m_cache.find(key) == m_cache.end();
    
        
        // If ttl is too low to deflect or too many deflection occured
        if (urgent) {
            return  GetUrgentCacheValue(context, key);   
        }
        
        // generate new context and a mapping to the old context's forwarding table
        auto result = RemoveUsedHops(context);
        int32_t idx;
        
        if (new_flow) {  // If flow is not in the cache
            idx = result.second[GetNewCacheValue(result.first, key)];
        } else { // If we have this flow in our cache
            idx = result.second[GetUsedCacheValue(result.first, key)];
        }

        //if error, drop
        if (idx < 0 || idx >= int(context.nextHopOptions.size())) { return -1; }

        return idx;
    }

    int32_t ArbiterGSPriorityDeflection::CacheHasheFunc(const PacketRoutingContext& context) {        
        // get key for cache
        uint32_t src_ip = context.ipHeader.GetSource().Get();
        uint32_t dst_ip = context.ipHeader.GetDestination().Get();
        uint16_t src_port; uint16_t dst_port;
        std::tie(src_port, dst_port) = GetPorts(context);
        std::tuple<uint32_t, uint32_t, uint16_t, uint16_t> key = std::make_tuple(src_ip, dst_ip, src_port, dst_port);

        //print cache info
        if (false && VERBOSE && context.pkt) {
            PrintCache();
        } 

        // check if we should erase whole cache
        if (CACHE_REFRESH_RATE != 0 && Simulator::Now() > m_refresh_time) {   
            m_refresh_time = Simulator::Now() + MilliSeconds(CACHE_REFRESH_RATE);
            m_cache.clear();
        } 
        // check if we should compute/recompute the entry
        return GetCacheValue(context, key);
    }

}