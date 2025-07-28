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

#ifndef ARBITER_GS_PRIORITY_DEFLECTION_H
#define ARBITER_GS_PRIORITY_DEFLECTION_H

#include <tuple>
#include <unordered_set>
#include "ns3/arbiter-satnet.h"
#include "ns3/topology-satellite-network.h"
#include "ns3/hash.h"
#include "ns3/abort.h"
#include "ns3/ipv4-header.h"
#include "ns3/udp-header.h"
#include "ns3/tcp-header.h"
#include "ns3/last-deflection-tag.h"


using CacheKey = std::tuple<uint32_t, uint32_t, uint16_t, uint16_t>;
using CacheValue = std::tuple<uint32_t, uint8_t, double, ns3::Time>;

struct FlowKey {
    ns3::Ipv4Address srcIp;
    ns3::Ipv4Address dstIp;
    uint8_t protocol;
    uint16_t srcPort;
    uint16_t dstPort;

    bool operator==(const FlowKey& other) const {
        return srcIp == other.srcIp &&
               dstIp == other.dstIp &&
               protocol == other.protocol &&
               srcPort == other.srcPort &&
               dstPort == other.dstPort;
    }
};
namespace std {
    template <>
    struct hash<FlowKey> {
        std::size_t operator()(const FlowKey& k) const {
            using std::hash;
            return ((hash<uint32_t>()(k.srcIp.Get()) ^
                    (hash<uint32_t>()(k.dstIp.Get()) << 1)) >> 1) ^
                   (hash<uint8_t>()(k.protocol) << 1) ^
                   (hash<uint16_t>()(k.srcPort) << 1) ^
                   (hash<uint16_t>()(k.dstPort) << 2);
        }
    };
}


namespace ns3 {


// hash for the 4 tuple (src ip, dst ip, src port, dst port) for per flow caching 
struct TupleHash {
    std::size_t operator()(const std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>& t) const {
        std::size_t h1 = std::hash<uint32_t>{}(std::get<0>(t));
        std::size_t h2 = std::hash<uint32_t>{}(std::get<1>(t));
        std::size_t h3 = std::hash<uint32_t>{}(std::get<2>(t));
        std::size_t h4 = std::hash<uint32_t>{}(std::get<3>(t));

        std::size_t hash = h1;
        hash ^= h2 + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= h3 + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= h4 + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        return hash;
    }
};

class ArbiterGSPriorityDeflection : public ArbiterSatnet
{
public:
    static TypeId GetTypeId (void);
    // Constructor for single forward next-hop forwarding state
    ArbiterGSPriorityDeflection(
            Ptr<Node> this_node,
            NodeContainer nodes,
            // std::vector<std::tuple<int32_t, int32_t, int32_t>> next_hop_list
            std::vector<std::vector<std::tuple<int32_t, int32_t, int32_t>>> next_hop_list
    );



    // 
    std::tuple<int32_t, int32_t, int32_t> TopologySatelliteNetworkDecide(
            int32_t source_node_id,
            int32_t target_node_id,
            ns3::Ptr<const ns3::Packet> pkt,
            ns3::Ipv4Header const &ipHeader,
            bool is_socket_request_for_source_ip
    );

    // Updating of forward state
    // void SetDeflectionState(int32_t target_node_id, int32_t next_node_id, int32_t own_if_id, int32_t next_if_id);
    void SetDeflectionState(int32_t target_node_id, const std::vector<std::tuple<int32_t, int32_t, int32_t>>& next_hops);

    // Static routing table
    std::string StringReprOfForwardingState();

private:
    typedef enum DeflectionFlags : uint16_t {
        NEVER      = 0,          // 0b000000000000000
        OVER_0     = 1 << 0,     // 0b000000000000001
        OVER_5     = 1 << 1,     // 0b000000000000010
        OVER_10    = 1 << 2,     // 0b000000000000100
        OVER_20    = 1 << 3,     // 0b000000000001000
        OVER_30    = 1 << 4,     // 0b000000000010000
        OVER_40    = 1 << 5,     // 0b000000000100000
        OVER_50    = 1 << 6,     // 0b000000001000000
        OVER_60    = 1 << 7,     // 0b000000010000000
        OVER_70    = 1 << 8,     // 0b000000100000000
        OVER_75    = 1 << 9,     // 0b000001000000000
        OVER_80    = 1 << 10,    // 0b000010000000000
        OVER_85    = 1 << 11,    // 0b000100000000000
        OVER_90    = 1 << 12,    // 0b001000000000000
        OVER_95    = 1 << 13,    // 0b010000000000000
        CRITICAL   = 1 << 14     // 0b100000000000000
    } DeflectionFlags;

    struct Checkpoint {
        DeflectionFlags flag;
        double threshold;
    };


    // context for routing a packet
    struct PacketRoutingContext {
            ns3::Ptr<const ns3::Packet> pkt;                                    // the packet
            const ns3::Ipv4Header& ip_header;                                    // ip header for the packet
            bool is_request_for_source_ip_no_next_header;                              // true if this is a request targeting the source ip
            std::vector<std::tuple<int32_t, int32_t, int32_t>> next_hop_options;  // list of options to forward the packet to
            const int32_t target_node_id;
    }; 

    // map of target ground station to weights for nodes to deflection to
    std::unordered_map<int32_t, std::unordered_map<int32_t, double>> m_weights;
    
    // list of lists of next hops, per node id (forwarding table)
    std::vector<std::vector<std::tuple<int32_t, int32_t, int32_t>>> 
    m_next_hop_list; 

    // cache used for per flow deflection
    // (src ip, dst ip, src port, dst port) : [next_if, ttl_limit, queue_fullness, experiation time] 
    std::unordered_map<CacheKey, CacheValue, TupleHash> m_cache; 
        
    // how long until the cache will clear itself
    Time m_refresh_time;    

    // each maps a target ground station to some timer
    std::unordered_map<int32_t, Time> m_weight_decay_timer; 
    std::unordered_map<int32_t, Time> m_weight_update_timer; 


    /*********************
        CACHE FUNCTIONS 
     *********************/

    /**
     * Given some packet and a list of options to forward to, determine where
     * to send the packet based on and while maintaining this nodes cache.
     *
     * @param context       The packet/routing context for this forwarding decision.
     * 
     * @return              The index to forward to, chosen from the forwarding options
     *                      inside of @p context.
     */
    int32_t CacheDecideDeflection(const PacketRoutingContext& context);

    /**
     * Gets the value from a cache, possibly generating a new/recomputing the cache entry.
     *
     * @param context       The packet/routing context for this forwarding decision. 
     * @param key           The key of this packets flow in cache.
     * 
     * @return              The index to forward to, chosen from the forwarding options
     *                      inside of @p context
     */
    int32_t GetCacheValue(
            const PacketRoutingContext& context,
            std::tuple<uint32_t, uint32_t, uint16_t, uint16_t> key
    );

    /**
     * If too many deflections have occured or the TTL is now 1, this packet is marked urgent.
     * When urgent, the arbiter decides wether to forward directly to its ground station or to drop
     * the packet.
     *
     * @param context       The packet/routing context for this forwarding decision.  
     * @param key           The key of this packets flow in cache.
     * 
     * @return              The index to forward to, chosen from the forwarding options
     *                      inside of @p context.
     */
    int32_t GetUrgentCacheValue(const PacketRoutingContext& context, CacheKey key);

    /**
     * Generates the cache entry given no previous entry exists for this key in our cache.
     *
     * @param context       The packet/routing context for this forwarding decision.  
     * @param key           The key of this packets flow in cache.
     * 
     * @return              The index to forward to, chosen from the forwarding options
     *                      inside of @p context.
     */
    int32_t GetNewCacheValue(const PacketRoutingContext& context, CacheKey key);

    /**
     * Generates the cache entry given some entry exists for this key in our cache.
     *
     * @param context       The packet/routing context for this forwarding decision.  
     * @param key           The key of this packets flow in cache.
     * 
     * @return              The index to forward to, chosen from the forwarding options
     *                      inside of @p context.
     */
    int32_t GetUsedCacheValue(const PacketRoutingContext& context, CacheKey key);

    /*********************
        next FUNCTIONS 
     *********************/


    bool HandleBasicHopErrors(
            const std::vector<std::tuple<int32_t, int32_t, int32_t>> &next_hops,
            int32_t& next_node_id, 
            int32_t& my_if_id,
            int32_t& next_if_id
    );


    



    /**
     * Given the index of the checkpoint flag we want, returns said flag.
     *
     * @param flag_index    The index of the checkpoint flag we want.
     * 
     * @return              The checkpoint.
     */
    Checkpoint GetCheckpoint(int flag_index);

    /**
     * Checks if some node is a ground station
     *
     * @param node_id       The id number of the node we are checking
     * 
     * @return              True if ground station, else false.
     */
    bool IsGroundStation(int32_t node_id);

    /**
     * Checks if the current node is the final hop before arriving at a ground station.
     *
     * @param next_node_id  The id number of default next node to forward to.
     * @param my_if_id      The default interface we would forward through to get to the next node.
     * 
     * @return              True if this is the final stop before a ground station.
     */
    bool IsFinalHopToGS(int32_t next_node_id, int32_t my_if_id);
    
    /**
     * Get the queue for some interface id.
     *
     * @param if_id         The interface id we are dealing with.
     * 
     * @return              The netdevice queue.
     */
    Ptr<Queue<Packet>> GetQueue(int32_t if_id);

    /**
     * Get the ratio that the netdevice queue is filled for some link.
     *
     * @param if_id         The interface id for the link we are dealing with.
     * 
     * @return              The ratio of how filled this link is.
     */
    double GetQueueRatio(int32_t if_id);

    /**
     * Generates a random number ~ N(avrg, sd).
     *
     * @return              The random number.
     */
    double NormalRNG(double avg, double sd);

    
    /**
     * Gets the source and destination ports for some packet
     *
     * @param context       The packet/routing context for this forwarding decision.
     * 
     * @return              A tuple representing (src port, dst port).
     */
    std::tuple<uint16_t, uint16_t> GetPorts(const PacketRoutingContext& context);

    /**
     * Counts the number of checkpoints passed.
     *
     * @param filled        How filled the netdevice queue is.
     * @param flags         The flags set. (all possible checkpoints)
     * 
     * @return              The number of checkpoints passed.
     */
    uint32_t CountPassedCheckpoints(double filled, DeflectionFlags flags);

    /**
     * Generates the chance to deflect.
     *
     * @param flags         The flags set. (all possible checkpoints)
     * @param filled        How filled the netdevice queue is.
     * @param passed_cp     How many checkpoints have been passed.
     * 
     * @return              A [0,1] double representing our percent 
     *                      chance of deflection.
     */
    double GetChanceDeflected(DeflectionFlags flags, double filled, int passed_cp);

    /**
     * Normalizes the weights of the given vector.
     *
     * @param weights       The weights, representing our likeliehood to deflect out of each forwarding
     *                      table entry.
     */
    void Normalize(std::unordered_map<int32_t, double>& map);

    void Normalize(std::vector<double>& vector);

    /**
     * Decays each weight in the vector, moving them all towards being uniform.
     *
     * @param weights       The weights, representing our likeliehood to deflect out of each forwarding
     *                      table entry.
     */
    void Decay(std::unordered_map<int32_t, double>& map);

    /**
     * Increases a specific weight in the given vector, while decreasing other weights.
     *
     * @param weights       The weights, representing our likeliehood to deflect out of each forwarding
     *                      table entry.
     * @param i             The index of the weight we are bumping.
     */
    void BumpUp(std::unordered_map<int32_t, double>& map, int32_t node_id);

    void BumpDown(std::unordered_map<int32_t, double>& map, int32_t node_id);

    void CreateWeights(const PacketRoutingContext& context);

    void MaintainWeights(const PacketRoutingContext& context);
    
    void HandleWeights(const PacketRoutingContext& context);

    void CheckBumpWeights(const PacketRoutingContext& context);

    /**
     * Creates a new context object the forwarding table is updates to not include deflections we have 
     * already done.
     *
     * @param context       The packet/routing context for this forwarding decision.  
     * 
     * @return              A tuple which contrains:
     *                          The new context which contains the new forwarding table,
     *                          A vector mapping indices of new forwarding table to the old one. 
     *                          A vector represeting normalized weights for the new hop list
     */
    std::tuple<PacketRoutingContext, std::vector<size_t>, std::vector<double>>  
    RemoveUsedHops(const PacketRoutingContext& context);

    /**
     * Choose where to deflect to.
     *
     * @param context       The packet/routing context for this forwarding decision.
     * 
     * @return              The index to forward to, chosen from the forwarding options
     *                      inside of @p context.
     */
    int32_t Deflect(const PacketRoutingContext& context, std::vector<double> weights);

    /**
     * Given some packet and a list of options to forward to, determine where
     * to send the packet.
     *
     * @param context       The packet/routing context for this forwarding decision.  
     * 
     * @return              The index to forward to, chosen from the forwarding options
     *                      inside of @p context.
     */
    int32_t DecideDeflection(const PacketRoutingContext& context);


    std::tuple<int32_t, int32_t, int32_t> HandleSpecialIDX(const PacketRoutingContext&context, int32_t n_if);

    void MarkPacket(Ptr<const Packet> pkt);

    /*********************
        HASHING FUNCTIONS 
     *********************/


    

    /********************
        DEBUG FUNCTIONS 
     ********************/

    /**
     * Prints out infomation about this nodes cache
     */
    void PrintCache();
    
};

}

#endif // ARBITER_GS_PRIORITY_DEFLECTION_H
