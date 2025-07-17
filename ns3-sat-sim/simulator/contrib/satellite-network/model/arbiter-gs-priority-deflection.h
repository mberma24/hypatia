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
#include "ns3/arbiter-satnet.h"
#include "ns3/topology-satellite-network.h"
#include "ns3/hash.h"
#include "ns3/abort.h"
#include "ns3/ipv4-header.h"
#include "ns3/udp-header.h"
#include "ns3/tcp-header.h"
#include "ns3/last-deflection-tag.h"
#include <unordered_set>

using TimePoint = std::chrono::steady_clock::time_point;

struct FlowKey {
    ns3::Ipv4Address srcIp;
    ns3::Ipv4Address dstIp;
    uint8_t protocol;
    uint16_t srcPort;
    uint16_t dstPort;

    // Define equality
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



    // Single forward next-hop implementation
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
    
        
    typedef enum DeflectionType {
        RNG,    // per packet
        HASH,   // per flow*
        ALWAYS  // (assuming checkpoints are passed) if you want to always deflect... for some reason
    } DeflectionType;

    typedef enum CacheRecomputingState {
        USE,        // use cache entry
        RECOMPUTE,  // recompute default 
        DEFLECT,    // deflect it
        FORWARD,    // send straight to GS
        DROP,       // drop packet
        GOTBACK     // we received a packet we sent out already
    } CacheRecomputingState;

    std::vector<std::vector<std::tuple<int32_t, int32_t, int32_t>>> m_next_hop_list; 

     // (src ip, dst ip, src port, dst port) : [next_if, ttl_limit, queue_fullness, experiation time] 
    std::unordered_map<std::tuple<uint32_t,uint32_t,uint16_t,uint16_t>, std::tuple<uint32_t, uint8_t, double, Time>, TupleHash> m_cache; // cache used for per flow deflection
    Time m_refresh_time;    // how long until the cache will clear itself

    /**
     * Prints out infomation about a flow given an IP header (src/dst ip+port, protocol) from some packet.
     *
     * @param ipv4Header    A reference to some IPV4 header.
     * @param packet        A pointer to the packet we are analyzing.
     */
    void PrintFlowFromIpv4Header(const Ipv4Header &ipv4Header, Ptr<const Packet> packet);

    /**
     * Prints out infomation about this nodes cache
     */
    void PrintCache();

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
     * @param node_id    The id number of the node we are checking
     * 
     * @return           True if ground station, else false.
     */
    bool IsGroundStation(int32_t node_id);

    /**
     * Checks if the current node is the final hop before arriving at a ground station.
     *
     * @param next_node_id      The id number of default next node to forward to.
     * @param my_if_id          The default interface we would forward through to get to the next node.
     * 
     * @return                  True if this is the final stop before a ground station.
     */
    bool IsFinalHopToGS(int32_t next_node_id, int32_t my_if_id);
    
    /**
     * Get the queue for some interface id.
     *
     * @param if_id     The interface id we are dealing with.
     * 
     * @return          The netdevice queue.
     */
    Ptr<Queue<Packet>> GetQueue(int32_t if_id);

    /**
     * Get the ratio that the netdevice queue is filled for some link.
     *
     * @param if_id     The interface id for the link we are dealing with.
     * 
     * @return          The ratio of how filled this link is.
     */
    double GetQueueRatio(int32_t if_id);

    /**
     * Generates a random number ~ N(.5,.15).
     *
     * @return  The random number.
     */
    double NormalRNG();

    /**
     * Generates a hash for some IP packet/header.
     *
     * @param pkt                                           The pkt we are basing this hash on.
     * @param ipHeader                                      The ipHeader of our packet.
     * @param is_request_for_source_ip_so_no_next_header    True if this is a request targeting the source IP,
     *                                                      and thus should not expect a next header.
     * 
     * @return                                              The hash.
     */
    uint32_t GenerateIpHash(
            Ptr<const Packet> pkt,
            Ipv4Header const &ipHeader,
            bool is_request_for_source_ip_so_no_next_header
    );
    
    /**
     * Gets the source and destination ports for some packet
     *
     * @param pkt                                           The pkt we are dealing with
     * @param proto                                         The protocol of our packet.
     * @param is_request_for_source_ip_so_no_next_header    True if this is a request targeting the source IP,
     *                                                      and thus should not expect a next header.
     * 
     * @return                                              A tuple representing (src port, dst port).
     */
    std::tuple<uint16_t, uint16_t> GetPorts(Ptr<const Packet> pkt, uint8_t proto, bool is_request_for_source_ip_so_no_next_header);

    /**
     * Generates some [0,1] value, representing the chances we will deflect.
     * (as r -> 0, our deflection chances rise)
     *
     * @param pkt                                           The pkt we are basing this on.
     * @param ipHeader                                      The ipHeader of our packet.
     * @param is_request_for_source_ip_so_no_next_header    True if this is a request targeting the source IP,
     *                                                      and thus should not expect a next header.
     * 
     * @return                                              A double, representing our 
     *                                                      chances of deflection.
     */
    double GenerateDeflectionInfo( 
            Ptr<const Packet> pkt,
            Ipv4Header const &ipHeader,
            bool is_request_for_source_ip_so_no_next_header
    );

    /**
     * Counts the number of checkpoints passed.
     *
     * @param filled    How filled the netdevice queue is.
     * @param flags     The flags set. (all possible checkpoints)
     * 
     * @return          The number of checkpoints passed.
     */
    uint32_t CountPassedCheckpoints(double filled, DeflectionFlags flags);

    /**
     * Generates the chance to deflect.
     *
     * @param flags                 The flags set. (all possible checkpoints)
     * @param filled                How filled the netdevice queue is.
     * @param passed_checkpoints    How many checkpoints have been passed.
     * 
     * @return                                              A [0,1] double representing our percent 
     *                                                      chance of deflection.
     */
    double GetChanceDeflected(DeflectionFlags flags, double filled, int passed_checkpoints);
   
    /**
     * Choose where to deflect to.
     *
     * @param next_hop_options      Our options of where we can forward to.
     * 
     * @return                      The index chosen from next_hop_options.
     */
    int32_t ChooseIDX(const std::vector<std::tuple<int32_t, int32_t, int32_t>> next_hop_options);

    /**
     * Given some packet and a list of options to forward to, determine where
     * to send the packet.
     *
     * @param pkt                                           The packet we are forwarding.
     * @param ipHeader                                      The ip header of our packet.
     * @param is_request_for_source_ip_so_no_next_header    True if this is a request targeting the source IP,
     *                                                      and thus should not expect a next header.
     * @param next_hop_options                              Our options of where we can forward to.
     * 
     * @return                                              The index to forward to, chosen from 
     *                                                      next_hop_options.
     */
    int32_t HashFunc(Ptr<const Packet> pkt, 
            Ipv4Header const &ipHeader, 
            bool is_request_for_source_ip_so_no_next_header, 
            const std::vector<std::tuple<int32_t, int32_t, int32_t>> next_hop_options
    );

    /**
     * Check to see if we should recompute a cache entry.
     *
     * @param ttl           The ttl of our packet.
     * @param filled_ratio  How filled the netdevice queue is for the GSL.
     * @param curr_time     The current simulation time.
     * @param key           The key of this packets flow in cache.
     * 
     * @return              If we should recompute true, else false.
     */
    CacheRecomputingState CheckRecompute(
            uint8_t ttl, 
            double filled_ratio, 
            Time curr_time, 
            std::tuple<uint32_t, uint32_t, uint16_t, uint16_t> key,
            LastDeflectionTag tag
    );

    /**
     * Check to ee if we should recompute a cache entry.
     *
     * @param is_recomputing                                True if we should recompute a cache entry.
     * @param pkt                                           The packet we are dealing with.
     * @param ipHeader                                      The ip header of our packet.
     * @param is_request_for_source_ip_so_no_next_header    True if this is a request targeting the source IP,
     *                                                      and thus should not expect a next header.
     * @param next_hop_options                              Our options of where we can forward to.
     * @param key                                           The key of this packets flow in cache.
     * 
     * @return                                              The index to forward to, chosen from 
     *                                                      next_hop_options.
     */
    int32_t GetCacheValue(
            CacheRecomputingState is_recomputing,
            Ptr<const Packet> pkt, 
            Ipv4Header const &ipHeader, 
            bool is_request_for_source_ip_so_no_next_header, 
            const std::vector<std::tuple<int32_t, int32_t, int32_t>> next_hop_options,
            std::tuple<uint32_t, uint32_t, uint16_t, uint16_t> key
    );
    
    /**
     * Given some packet and a list of options to forward to, determine where
     * to send the packet based on and while maintaining this nodes cache.
     *
     * @param pkt                                           The packet we are forwarding.
     * @param ipHeader                                      The ip header of our packet.
     * @param is_request_for_source_ip_so_no_next_header    True if this is a request targeting the source IP,
     *                                                      and thus should not expect a next header.
     * @param next_hop_options                              Our options of where we can forward to.
     * 
     * @return                                              The index to forward to, chosen from 
     *                                                      @p next_hop_options.
     */
    int32_t CacheHasheFunc(
            Ptr<const Packet> pkt, 
            Ipv4Header const &ipHeader, 
            bool is_request_for_source_ip_so_no_next_header, 
            const std::vector<std::tuple<int32_t, int32_t, int32_t>> next_hop_options
    );


    

};

}

#endif // ARBITER_GS_PRIORITY_DEFLECTION_H
