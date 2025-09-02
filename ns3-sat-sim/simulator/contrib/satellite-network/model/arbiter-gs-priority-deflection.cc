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
#include <fstream>
#include <limits.h>
#include <string>

// ************************************
// ====== Customizable Variables ======
// ************************************

/* General Deflection Settings */
#define VERBOSE false               // Turn on logging statements
#define GET_PATH_LENS false         // Logs the time and number of deflections before reaching to target N to m_paths_N.txt 
#define FLAGS OVER_0                // When should we start/ramp up deflecting
#define STATIC false                // Should the chance to deflect increase per flag or smoothly 
                                    // BEST: false (it is not worth considering this parameter)
#define DEFLECTION_BOOST 0          // Make it easier to deflect

/* Cache Settings */
#define USING_CACHE true            // Is the cache being used
#define CACHE_REFRESH_RATE 0        // How long until you need to clear the entire cache (0 for never)                      
                                    // BEST: 0   (ms)

#define ENTRY_EXPIRE_RATE 150       // How long in simulation time (ms) until a cache entry expires.                        
                                    // BEST: 150 (ms) 

#define THRESHOLD_SIZE .4           // Threshold for how much the queue needs to change (%) to recalculate the value
#define TTL_HOP_LIMIT 0             // How much lower does a ttl need to be to recalculate the value
#define MAX_DEFLECTIONS 7           // How many deflections can occur per packet.
#define DROP_DECAY .75              // If dropping in cache, multiply the threshold need by this value.                     
                                    // BEST: UNUSED (We do not drop packets)
#define REPEATED_DEFLECTIONS false  // Should we allow deflections to send packets back to the interface it received from.
                                    // BEST: false

/* Weight Settings */
#define USE_WEIGHTS true            // Pick where to deflect via from weighted distribution
#define WEIGHT_DECAY_RATE .018      // How fast a node will go back to being uniform
                                    // BEST: .018 ms (goes to uniform in ~1.5s)
#define WEIGHT_LEARN_RATE 1.25      // On a sendback, how much do we penalize the weight for this interface(per packet)
                                    // BEST: ??? (Yet to find satisfying topology to test on)



namespace ns3 {

    // ******************************
    // ====== Public Functions ======
    // ******************************
    
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
            std::vector<std::vector<NextHopOption>> next_hop_list
    ) : ArbiterSatnet(this_node, nodes)
    {
        m_refresh_time = Simulator::Now() + MilliSeconds(CACHE_REFRESH_RATE);
        m_next_hop_list = next_hop_list;
    }

    NextHopOption ArbiterGSPriorityDeflection::TopologySatelliteNetworkDecide(
            int32_t source_node_id,
            int32_t target_node_id,
            Ptr<const Packet> pkt,
            Ipv4Header const &ip_header,
            bool is_request_for_source_ip_so_no_next_header
    ) {
        const auto& next_hops = m_next_hop_list[target_node_id];
        int32_t next_node_id, my_if_id, next_if_id;

        

        PacketRoutingContext context {
                pkt,
                ip_header,
                is_request_for_source_ip_so_no_next_header,
                next_hops,
                target_node_id,
        };
        

        // if the next hop list is invalid
        if (HandleBasicHopErrors(next_hops, next_node_id, my_if_id, next_if_id)) { 
            if (next_hops.empty()) {
                return std::make_tuple(-2, -2, -2);
            } else {
                return next_hops[0];
            }
        }


        // if we CAN deflect (NOT guaranteed)
        if (IsFinalHopToGS(next_node_id, my_if_id) && next_hops.size() > 1) { 
            if (USE_WEIGHTS) { 
                HandleWeights(context); 
                CheckBumpWeights(context); 
            }
     
            // the main logic for deflection is in CacheDecideDeflection and DecideDeflection
            int32_t n_if =  USING_CACHE ? CacheDecideDeflection(context) : DecideDeflection(context);
    
            if (n_if < 0) { 
                return HandleSpecialIDX(context, n_if); // unused
            } 
            else if (pkt && n_if > 0) { MarkPacket(pkt); } 

            if (n_if == 0 && GET_PATH_LENS) {
                log_paths(context);
            }

            return next_hops[n_if];
        } 
        
       
        return next_hops[0];
    }

    void ArbiterGSPriorityDeflection::log_paths(PacketRoutingContext ctx) {
        Time t = Simulator::Now();
        //std::cout << std::to_string(ctx.target_node_id) << " TIME: " <<  t.GetSeconds() << std::endl;
        Ptr<Packet> nonConstPkt = const_cast<Packet*>(PeekPointer(ctx.pkt));

        // Handle PathDeflectionTag
        PathDeflectionTag path_tag;
        size_t len = 0;
        if (nonConstPkt->PeekPacketTag(path_tag)) {
            len = path_tag.GetNumDeflections();
        }
            

        if (t > m_path_time) {
            double avg = 0.0;
            if (!m_path_lengths.empty()) {
                avg = static_cast<double>(
                        std::accumulate(m_path_lengths.begin(),
                                        m_path_lengths.end(),
                                        0ULL))
                    / m_path_lengths.size();
            }


            std::string baseDir = "/home/sat/hypatia/paper/ns3_experiments/two_compete/"
                                "runs/run_two_kuiper_isls_moving/logs_ns3/";


            std::string filename = baseDir + "m_paths_" + std::to_string(ctx.target_node_id) + ".txt";

            // fill out file with times and avrgs
            std::ofstream outfile(filename, std::ios::app);
            if (outfile.is_open()) {
                outfile << t.GetNanoSeconds() << " " << avg << "\n";
                outfile.close();
            }

            // reset for next interval
            m_path_time = t;
            m_path_lengths.clear();
        
        } 
        m_path_lengths.push_back(len);
            
        
    }

    void ArbiterGSPriorityDeflection::SetDeflectionState(
            int32_t target_node_id, 
            const std::vector<NextHopOption>& next_hops
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
    
    // ******************************
    // ====== Cache Management ======
    // ******************************

    int32_t ArbiterGSPriorityDeflection::CacheDecideDeflection(const PacketRoutingContext& context) {        
        // get key for cache
        uint32_t src_ip = context.ip_header.GetSource().Get();
        uint32_t dst_ip = context.ip_header.GetDestination().Get();
        uint16_t src_port; uint16_t dst_port;
        std::tie(src_port, dst_port) = GetPorts(context);
        CacheKey key = std::make_tuple(src_ip, dst_ip, src_port, dst_port);

        //print cache info
        if (VERBOSE && context.pkt) {
            PrintCache();
        } 

        // check if we should erase whole cache
        if (CACHE_REFRESH_RATE != 0 && Simulator::Now() > m_refresh_time) {   
            m_refresh_time = Simulator::Now() + MilliSeconds(CACHE_REFRESH_RATE);
            m_weights.clear();
            HandleWeights(context);
            m_cache.clear();
        } 
        // check if we should compute/recompute the entry
        return GetCacheValue(context, key);
    }

    int32_t ArbiterGSPriorityDeflection::GetCacheValue(const PacketRoutingContext& context, CacheKey key) {
        //if we need to forward or drop
        uint8_t ttl = context.ip_header.GetTtl();
        PathDeflectionTag tag;
        bool urgent = ttl < 3 || (context.pkt->PeekPacketTag(tag) && tag.GetNumDeflections() > MAX_DEFLECTIONS);
        bool new_flow = m_cache.find(key) == m_cache.end();
    
        // If ttl is too low to deflect or too many deflection occured
        if (urgent) {
            return  GetUrgentCacheValue(context, key);   
        }
        
        // generate new context and a mapping to the old context's forwarding table
        int32_t idx;
        
        if (new_flow) {  // If flow is not in the cache
            idx = GetNewCacheValue(context, key);
        } else { // If we have this flow in our cache
            idx = GetUsedCacheValue(context, key);
        }

        //if error, forward directly
        if (idx < 0 || idx >= int(context.next_hop_options.size())) { 
            return 0; 
        }

        return idx;
    }

    int32_t ArbiterGSPriorityDeflection::GetUrgentCacheValue(const PacketRoutingContext& context, CacheKey key) {
        //get info for new "urgent" cache entry
        double filled_ratio = GetQueueRatio(std::get<1>(context.next_hop_options[0]));
        double random_number = NormalRNG(2, .1);
        auto ttl = context.ip_header.GetTtl();

        int32_t fwd;
        if (ttl <= 1) {
            fwd = -1; // drop
        } else {
            fwd =   random_number > GetQueueRatio(std::get<1>(context.next_hop_options[0]))
                        ?   0    // forward
                        :  -1;   // drop (unused)
        }

        int min_ttl = std::min(3, context.ip_header.GetTtl() - TTL_HOP_LIMIT);
        Time expire_time = Simulator::Now() + MilliSeconds(ENTRY_EXPIRE_RATE);

        m_cache[key] = std::make_tuple(fwd, min_ttl, filled_ratio, expire_time);
        return fwd;
    }

    int32_t ArbiterGSPriorityDeflection::GetNewCacheValue(const PacketRoutingContext& context, CacheKey key) {    
        //info for new cache entry
        double filled_ratio = GetQueueRatio(std::get<1>(context.next_hop_options[0]));
        int32_t fwd = DecideDeflection(context); 
        int min_ttl = std::min(3, context.ip_header.GetTtl() - TTL_HOP_LIMIT);
        Time expire_time = Simulator::Now() + MilliSeconds(ENTRY_EXPIRE_RATE);

        //assign entry
        m_cache[key] = std::make_tuple(fwd, min_ttl, filled_ratio, expire_time);
        return fwd;
    }

    int32_t ArbiterGSPriorityDeflection::GetUsedCacheValue(const PacketRoutingContext& context, CacheKey key) {
        // get basic info from this context
        Time time = Simulator::Now();
        double filled_ratio = GetQueueRatio(std::get<1>(context.next_hop_options[0]));
        
        //get cache info for this flow
        auto cache_entry = m_cache[key];
        int32_t idx =  std::get<0>(cache_entry);
        bool deflecting = idx != 0;
        uint8_t min_ttl = std::get<1>(cache_entry);
        double threshold = std::get<2>(cache_entry);
        double lower_threshold_size = (idx == -1) ? THRESHOLD_SIZE : THRESHOLD_SIZE  * DROP_DECAY;
        Time expiraton_date = std::get<3>(cache_entry);
        
        bool recompute_cache_entry =
                time > expiraton_date 
                ||  context.ip_header.GetTtl() < min_ttl
                ||  (deflecting && (filled_ratio <= 0 || filled_ratio < threshold - (lower_threshold_size)))
                ||  (!deflecting && (filled_ratio >= 1 || filled_ratio > threshold + THRESHOLD_SIZE));

        if (recompute_cache_entry) {
            idx = GetNewCacheValue(context, key);
        } else { 
            expiraton_date = time + MilliSeconds(ENTRY_EXPIRE_RATE);
            m_cache[key] = std::make_tuple(idx, min_ttl, threshold, expiraton_date);
        }
        return idx;
    }

    // ***********************************
    // ====== Deflection Core Logic ======
    // ***********************************

    int32_t ArbiterGSPriorityDeflection::DecideDeflection(const PacketRoutingContext& context) {
        // as the deflection_chance -> 1, it becomes MORE likely to deflect
        double deflected_chance = GetQueueRatio(std::get<1>(context.next_hop_options[0]));
        int passed_checkpoints = CountPassedCheckpoints(deflected_chance);
        
        //if no checkpoints are passed or there are no deflection options, we will NEVER deflect anyways
        if (passed_checkpoints > 0 && context.next_hop_options.size() > 1) {
            deflected_chance = GetChanceDeflected(deflected_chance, passed_checkpoints);
            
            const auto filtered = RemoveLastHop(context);
            auto new_context = std::get<0>(filtered);
            const auto mapping = std::get<1>(filtered);
            auto weights = std::get<2>(filtered);

            if (USE_WEIGHTS) {
                double weight_bias_factor = std::accumulate(weights.begin(), weights.end(), 0.0);
                deflected_chance = 0.75 * deflected_chance + 0.25 * weight_bias_factor;
                Normalize(weights);
            }

            if (NormalRNG() < deflected_chance) { 
                // deflect to some non-zero index
                return mapping[Deflect(context, weights)];
            }
        }
        return 0; // no deflection
    }

    double ArbiterGSPriorityDeflection::GetChanceDeflected(double filled, int passed_cp) {
        if (STATIC) { // set to highest passed flag false
            double highest_passed = 0.0;

            for (size_t i = 0; i < 15; ++i) {
                const auto& cp = GetCheckpoint(i);
                if (((DeflectionFlags)FLAGS) & cp.flag) { 
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

    std::tuple<ArbiterGSPriorityDeflection::PacketRoutingContext, std::vector<size_t>, std::vector<double>> 
    ArbiterGSPriorityDeflection::RemoveLastHop(const PacketRoutingContext& context) {
        PathDeflectionTag tag;
        context.pkt->PeekPacketTag(tag);
        int32_t deflected_from_node_id = tag.GetLastNode();
        
        // make new context
        PacketRoutingContext new_context = context;
        new_context.next_hop_options.clear();

        // maps indices from new hops to old hops 
        std::vector<size_t> preserved_indices; 
        auto weights = m_weights[context.target_node_id];

        //vector of weights for new next hop list
        std::vector<double> filtered_weights;
        
        for (size_t i = 0; i < context.next_hop_options.size(); ++i) {
            const auto& hop = context.next_hop_options[i];
            int32_t node_id = std::get<0>(hop);
            
            // if this node was not the last hop (in a deflection)
            if (REPEATED_DEFLECTIONS || deflected_from_node_id != node_id) {
                new_context.next_hop_options.push_back(hop);
                preserved_indices.push_back(i);
                if (USE_WEIGHTS && i > 0) {
                    filtered_weights.push_back(weights[node_id]);
                }
            } 
        }

        return { new_context, preserved_indices, filtered_weights };
    }

    int32_t ArbiterGSPriorityDeflection::Deflect(const PacketRoutingContext& context, std::vector<double> weights) { 
        // if size 1, that means the one possible hop is no longer possible
        if (context.next_hop_options.size() == 1) { 
            return 0; 
        }

        int32_t idx; // index for the next hop entry we want to forward to

        if (USE_WEIGHTS) {
            std::mt19937 gen(static_cast<uint32_t>(rand())); 
            std::discrete_distribution<> dist(weights.begin(), weights.end());
            idx = dist(gen) + 1;
        } else {
            idx = 1 + ( rand() % (context.next_hop_options.size() - 1));
        }

        // map back to real hops
        return idx; 
    }

    double ArbiterGSPriorityDeflection::NormalRNG(double avg, double sd) {
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

    NextHopOption ArbiterGSPriorityDeflection::HandleSpecialIDX(const PacketRoutingContext&context, int32_t n_if) {
        return std::make_tuple(-1, -1, -1); // unused for now, so drop
    }

    // *******************************
    // ====== Weight Management ======
    // *******************************

    void ArbiterGSPriorityDeflection::HandleWeights(const PacketRoutingContext& context) {
        Time now = Simulator::Now();

        //if weights do not exist
        if (m_weights.find(context.target_node_id) == m_weights.end()) { 
            // create uniform weights
            CreateWeights(context);
            //create a decay timer
            m_weight_decay_timer[context.target_node_id] = Simulator::Now();
            //create a timer to limit how often we bump the weights
            m_weight_bump_timer[context.target_node_id] = Simulator::Now();

        } else {
            // first we must ensure all weights reflect nodes in the hop list (and we haven no extra weights!)
            MaintainWeights(context);

            if (now >= m_weight_decay_timer[context.target_node_id] + MilliSeconds(1)) {

                //since a lot of time could have passed, we Flatten multiple times if needed
                int64_t elapsed = ( now - m_weight_decay_timer[context.target_node_id] ).GetMilliSeconds();
                for (int i = 0; i < elapsed; i++) {
                    Flatten(m_weights[context.target_node_id]);
                }
            
                m_weight_decay_timer[context.target_node_id] = now;
            }
        }
    }

    void ArbiterGSPriorityDeflection::CreateWeights(const PacketRoutingContext& context) {
        std::unordered_map<int32_t, double> weights;
        size_t n = context.next_hop_options.size() - 1;
        
        for (auto hop : context.next_hop_options) {
            auto next_hop_node = std::get<0>(hop);
            if (next_hop_node != context.target_node_id) {
                weights[next_hop_node] = 1.0 / n;
            }

        }
        m_weights[context.target_node_id] = weights;
    }

    void ArbiterGSPriorityDeflection::MaintainWeights(const PacketRoutingContext& context) {
        if (context.next_hop_options.size() < 2) { 
            return; 
        }
        auto& weights = m_weights[context.target_node_id];
        std::unordered_set<int32_t> valid_hops;
        double original_sum = Sum(weights);

        // add new hops with temp weights
        for (const auto& hop : context.next_hop_options) {
            auto next_hop_node = std::get<0>(hop);
            if (next_hop_node != context.target_node_id) {
                valid_hops.insert(next_hop_node);
                weights.insert({next_hop_node, 0.0});
            }
        }

        // remove old hops not in current list
        for (auto it = weights.begin(); it != weights.end(); ) {
            it = valid_hops.count(it->first) ? ++it : weights.erase(it);
        }

        // normalize or distribute weights
        double current_sum = Sum(weights); 
        if (current_sum == 0.0) { // if all zeros
            double equal_weight = (weights.empty() ? 0.0 : original_sum / weights.size());
            for (auto& kv : weights) {
                kv.second = equal_weight;
            }
        } else {
            double scale = original_sum / current_sum;
            for (auto& kv : weights) {
                kv.second *= scale;
            }
        }
    }

    void ArbiterGSPriorityDeflection::Flatten(std::unordered_map<int32_t, double>& map) {
        double uniform_val = 1.0 / map.size();
        for (auto& kv : map) {
            kv.second = (1 - WEIGHT_DECAY_RATE) * kv.second + WEIGHT_DECAY_RATE * uniform_val;
        }
    }

    void ArbiterGSPriorityDeflection::CheckBumpWeights(const PacketRoutingContext& context) {
        // if we were not deflected to, we shouldnt bump down weights
        PathDeflectionTag tag;
        if (!context.pkt->PeekPacketTag(tag)) {
            return; 
        } 
        auto now = Simulator::Now();

        // TODO: Add timer to stablize bumping the weights (T^ -> W^)
        if (now < m_weight_bump_timer[context.target_node_id] + MilliSeconds(0)) { // 0: not used
            m_weight_bump_timer[context.target_node_id] = now;
            return;
        }
        
        int32_t node_deflected_from_id = tag.GetLastNode();
        if (m_weights[context.target_node_id].find(node_deflected_from_id) != m_weights[context.target_node_id].end()) {
            BumpDown(m_weights[context.target_node_id], node_deflected_from_id);
        }  
    }

    void ArbiterGSPriorityDeflection::BumpDown(std::unordered_map<int32_t, double>& map, int32_t node_id) {
        auto it = map.find(node_id);

        if (it != map.end()) {
            it->second /= WEIGHT_LEARN_RATE; 
        }
    }

    void ArbiterGSPriorityDeflection::Normalize(std::unordered_map<int32_t, double>& map) {
        double sum = 0.0;

        for (const auto& kv : map) {
            sum += kv.second;
        }

        if (sum > 0.0) {
            for (auto& kv : map) {
                kv.second /= sum;
            }
        } else { // uniform
            double uniform = 1.0 / map.size();
            for (auto& kv : map) {
                kv.second = uniform;
            }
        }
    } 

    void ArbiterGSPriorityDeflection::Normalize(std::vector<double>& vector) {
        double sum = 0.0;

        for (const auto& v : vector) {
            sum += v;
        }

        if (sum > 0.0) {
            for (auto& v : vector) {
                v /= sum;
            }
        } else { // uniform
            double uniform = 1.0 / vector.size();
            for (auto& v : vector) {
                v = uniform;
            }
        }
    }

    double ArbiterGSPriorityDeflection::Sum(const std::unordered_map<int32_t, double>& map) {
        double sum = 0.0;
        for (const auto kv : map) {
            sum += kv.second;
        }
        return sum;
    }
    
    // ********************************
    // ======= Queue Ultilities =======
    // ********************************

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

    // ********************************
    // ====== Topology Utilities ======
    // ********************************

    bool ArbiterGSPriorityDeflection::IsFinalHopToGS(int32_t next_node_id, int32_t my_if_id) {
        //we simply check if this is a GSL, and if so we need to ensure THIS node is a satelight and the next node is NOT
        bool is_gsl 
                = m_nodes.Get(m_node_id)
                ->GetObject<Ipv4>()
                ->GetNetDevice(my_if_id)
                ->GetObject<GSLNetDevice>() != 0;
        
        bool this_is_gs = IsGroundStation(m_node_id);
        bool next_is_gs = IsGroundStation(next_node_id);
        return is_gsl && next_is_gs && !this_is_gs;
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

    // ***********************************
    // ====== Checkpoint Management ======
    // ***********************************

    ArbiterGSPriorityDeflection::Checkpoint ArbiterGSPriorityDeflection::GetCheckpoint(int flag_index) {
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

    uint32_t ArbiterGSPriorityDeflection::CountPassedCheckpoints(double filled) {
        int num_passed = 0; 

        //we just go through each flag and see if the flag exists and if we passed this checkpoint yet
        for (size_t i = 0; i < 15; ++i) {
            const auto& cp = GetCheckpoint(i);
            if (((DeflectionFlags)FLAGS) & cp.flag) {
                if (filled >= cp.threshold) { 
                    num_passed++;
                } else { // if we fail this checkpoint, we wont pass any others past this
                    break;
                }
            }
        }
        return num_passed;
    }

    // ***********************************
    // ======= Packet/Flow Helpers =======
    // ***********************************

    bool ArbiterGSPriorityDeflection::HandleBasicHopErrors(
            const std::vector<NextHopOption> &next_hops,
            int32_t& next_node_id, 
            int32_t& my_if_id,
            int32_t& next_if_id
    ) {
         // ERROR: No next hops available 
        if (next_hops.empty()) { 
            return true; 
        }
        
        // assign values
        std::tie(next_node_id, my_if_id, next_if_id) = next_hops[0];

        // ERROR: invalid next hop list (bad ttl?)
        if (next_node_id < 0 || my_if_id < 0 ||  next_if_id < 0) {
            for (auto hop : next_hops) { 
                std::cout << "E: next_node_id: " << std::get<0>(hop) << " my_if_id: " 
                << std::get<1>(hop)  << " next_if_id" << std::get<2>(hop)  << std::endl; }
            return true;
        }

        return false;
    }

    std::tuple<uint16_t, uint16_t> ArbiterGSPriorityDeflection::GetPorts(const PacketRoutingContext& context) {
        uint8_t protocol = context.ip_header.GetProtocol();
        if (!context.is_request_for_source_ip_no_next_header) {
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


   void ArbiterGSPriorityDeflection::MarkPacket(Ptr<const Packet> pkt) {
        Ptr<Packet> nonConstPkt = const_cast<Packet*>(PeekPointer(pkt));

        // // Handle LastDeflectionTag
        // LastDeflectionTag tag(m_node_id);
        // if (nonConstPkt->PeekPacketTag(tag)) {
        //     tag.SetLastNode(m_node_id);
        //     nonConstPkt->ReplacePacketTag(tag);
        // } else {
        //     nonConstPkt->AddPacketTag(LastDeflectionTag(m_node_id));
        // }

        // Handle PathDeflectionTag
        PathDeflectionTag path_tag;
        if (nonConstPkt->PeekPacketTag(path_tag)) {
            path_tag.SetLastNode(m_node_id);
            nonConstPkt->ReplacePacketTag(path_tag);
        } else {
            PathDeflectionTag new_tag;
            new_tag.SetLastNode(m_node_id);
            nonConstPkt->AddPacketTag(new_tag);
        }
    }

    /**********************
        DEBUG FUNCTIONS
     **********************/

    void ArbiterGSPriorityDeflection::PrintCache() {
        std::cout << "Time till refresh: " << m_refresh_time.GetMilliSeconds() - Simulator::Now().GetMilliSeconds() << std::endl;
        for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {            
            uint32_t src_ip, dst_ip;
            uint16_t src_port, dst_port;
            std::tie(src_ip, dst_ip, src_port, dst_port) = it->first;

            uint32_t next_if = std::get<0>(it->second);
            uint8_t ttl_min = std::get<1>(it->second);
            double fullness = std::get<2>(it->second);
            Time timestamp = std::get<3>(it->second);

            auto ms = timestamp.GetMilliSeconds() - ENTRY_EXPIRE_RATE;
            ms = ms < 0 ? 0 : ms;

            std::cout << "("
                    << src_ip << ", " << dst_ip << ", " << src_port << ", " << dst_port << ") -> "
                    << "(n_if: " << next_if << ", ttl_min: " << static_cast<int>(ttl_min) 
                    << ", Fullness:" << fullness << ", " << "ms set: " << ms << ")\n";
        }
    }

}

/*
**********************
*   TASKS            *
**********************
*   BUILD GRAPH
    *   MEASURE GS CONNECTIONS OVER TIME
    *   MEASURE THROUGHPUT OVER TIME
    *   CHECK CORRELATION
*   GRAPH RATIO CDF
    *   HASH VS CACHE?
    *   DEGREE NONSENSE?
    
*   TEST TOP (IMPROVE)
    *   PICK WORST TOP
    *   TRY TO FIND WORST PAIR? -> PUT IN AB OR COMP

*   IMPROVEMENT IDEAS:
    *   FINE TOON PARAMS?
        *   NEED STRESS TEST SINCE MOST VARS ARE RARE TO BE USED

*/