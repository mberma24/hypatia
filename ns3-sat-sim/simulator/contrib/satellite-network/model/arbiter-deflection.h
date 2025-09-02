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

#ifndef ARBITER_DEFLECTION_H
#define ARBITER_DEFLECTION_H

#include <tuple>
#include "ns3/arbiter-satnet.h"
#include "ns3/topology-satellite-network.h"
#include "ns3/hash.h"
#include "ns3/abort.h"
#include "ns3/ipv4-header.h"
#include "ns3/udp-header.h"
#include "ns3/tcp-header.h"

namespace ns3 {

class ArbiterDeflection : public ArbiterSatnet
{
public:
    static TypeId GetTypeId (void);

    // Constructor for single forward next-hop forwarding state
    ArbiterDeflection(
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
    // std::vector<std::tuple<int32_t, int32_t, int32_t>> m_next_hop_list;
    std::vector<std::vector<std::tuple<int32_t, int32_t, int32_t>>> m_next_hop_list;

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
     * Checks if some node is a ground station
     *
     * @param node_id       The id number of the node we are checking
     * 
     * @return              True if ground station, else false.
     */
    bool IsGroundStation(int32_t node_id);

    /**
     * Mark a packet as deflected by this node, and increase the deflection counter.
     *
     * @param pkt           The packet we are dealing with.  
     */
    void MarkPacket(Ptr<const Packet> pkt);

    
    Time m_path_time; 
    std::vector<int32_t> m_path_lengths; 

    // context for routing a packet
    struct PacketRoutingContext {
            ns3::Ptr<const ns3::Packet> pkt;                // the packet
            const ns3::Ipv4Header& ip_header;               // ip header for the packet
            bool is_request_for_source_ip_no_next_header;   // true if this is a request targeting the source ip
            std::vector<std::tuple<int32_t, int32_t, int32_t>> next_hop_options;    // list of options to forward the packet to
            const int32_t target_node_id;                   // destination node id for this packet
    }; 
    void log_paths(PacketRoutingContext  ctx);


};

}

#endif //ARBITER_SINGLE_FORWARD_H
