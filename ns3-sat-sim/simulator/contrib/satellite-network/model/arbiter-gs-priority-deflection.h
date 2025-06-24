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

namespace ns3 {

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
        OVER_20    = 1 << 1,     // 0b000000000000010
        OVER_30    = 1 << 2,     // 0b000000000000100
        OVER_40    = 1 << 3,     // 0b000000000001000
        OVER_50    = 1 << 4,     // 0b000000000010000
        OVER_55    = 1 << 5,     // 0b000000000100000
        OVER_60    = 1 << 6,     // 0b000000001000000
        OVER_65    = 1 << 7,     // 0b000000010000000
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
        PACKET, // per packet
        FLOW,   // per flow
        ALWAYS  // (assuming checkpoints are passed) if you want to always deflect... for some reason
    } DeflectionType;

    

    // std::vector<std::tuple<int32_t, int32_t, int32_t>> m_next_hop_list;
    std::vector<std::vector<std::tuple<int32_t, int32_t, int32_t>>> m_next_hop_list;


    Checkpoint GetCheckpoint(int flag_index);
    // Figure out if some node is a ground station
    bool IsGroundStation(uint32_t node_id);
    // Figure out if some node is the final hop to a ground station
    bool IsFinalHopToGS(uint32_t node_id, uint32_t next_node_id, uint32_t my_if_id);
    // Get the ratio that the netdevice queue is filled for some link
    double GetQueueRatio(uint32_t if_id);
    //Generates the hash given some IP packet
    uint32_t GenerateIpHash(
        Ptr<const Packet> pkt,
        Ipv4Header const &ipHeader,
        bool is_request_for_source_ip_so_no_next_header
    );
    //Generate the chance we deflect, and were we would deflect to
    std::tuple<uint32_t, double> GenerateDeflectionInfo(
        DeflectionType deflectionType, 
        Ptr<const Packet> pkt,
        Ipv4Header const &ipHeader,
        bool is_request_for_source_ip_so_no_next_header
    );
    int CountPassedCheckpoints(double filled, DeflectionFlags flags);
    double GetChanceDeflected(DeflectionFlags flags, double filled, int passed_checkpoints);
    bool ShouldDeflect(double filled, double generated_chance, int passed_checkpoints, bool is_static_rate, DeflectionFlags flags);
    

    int hashFunc(Ptr<const Packet> pkt, 
        Ipv4Header const &ipHeader, 
        bool is_request_for_source_ip_so_no_next_header, 
        const std::vector<std::tuple<int32_t, int32_t, int32_t>> next_hop_options
    );

};

}

#endif // ARBITER_GS_PRIORITY_DEFLECTION_H
