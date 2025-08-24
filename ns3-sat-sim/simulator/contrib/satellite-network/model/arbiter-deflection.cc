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

#include "arbiter-deflection.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (ArbiterDeflection);
TypeId ArbiterDeflection::GetTypeId (void)
{
    static TypeId tid = TypeId ("ns3::ArbiterDeflection")
            .SetParent<ArbiterSatnet> ()
            .SetGroupName("BasicSim")
    ;
    return tid;
}

ArbiterDeflection::ArbiterDeflection(
        Ptr<Node> this_node,
        NodeContainer nodes,
        std::vector<std::vector<std::tuple<int32_t, int32_t, int32_t>>> next_hop_list
) : ArbiterSatnet(this_node, nodes)
{
    m_next_hop_list = next_hop_list;
}

// Custom tag to mark deflected packets
class DeflectionMarkTag : public Tag {
public:
    static TypeId GetTypeId (void) {
        static TypeId tid = TypeId ("DeflectionMarkTag")
            .SetParent<Tag> ()
            .AddConstructor<DeflectionMarkTag> ();
        return tid;
    }
    TypeId GetInstanceTypeId (void) const override {
        return GetTypeId();
    }
    uint32_t GetSerializedSize (void) const override { return 0; }
    void Serialize (TagBuffer buf) const override {}
    void Deserialize (TagBuffer buf) override {}
    void Print (std::ostream &os) const override { os << "DeflectionMark"; }
};

// std::tuple<int32_t, int32_t, int32_t> ArbiterDeflection::TopologySatelliteNetworkDecide(
//         int32_t source_node_id,
//         int32_t target_node_id,
//         Ptr<const Packet> pkt,
//         Ipv4Header const &ipHeader,
//         bool is_request_for_source_ip_so_no_next_header
// ) {
//     // Hash-based selection of next hop
//     const auto& next_hops = m_next_hop_list[target_node_id];
//     if (next_hops.empty()) {
//         // Return an invalid entry if no next hops are available
//         return std::make_tuple(-2, -2, -2);
//     }

//     // Check if packet is already marked
//     bool isMarked = false;
//     if (pkt) {
//         DeflectionMarkTag tag;
//         isMarked = pkt->PeekPacketTag(tag);
//     }

//     uint32_t idx = 0;
//     if (!isMarked) {
//         // Per-flow selection: hash the 5-tuple (src IP, dst IP, protocol, src port, dst port)
//         uint32_t src_ip = ipHeader.GetSource().Get();
//         uint32_t dst_ip = ipHeader.GetDestination().Get();
//         uint8_t proto = ipHeader.GetProtocol();
//         uint16_t src_port = 0, dst_port = 0;
//         if (!is_request_for_source_ip_so_no_next_header) {
//             if (proto == 6) { // TCP
//                 TcpHeader tcpHeader;
//                 pkt->PeekHeader(tcpHeader);
//                 src_port = tcpHeader.GetSourcePort();
//                 dst_port = tcpHeader.GetDestinationPort();
//             } else if (proto == 17) { // UDP
//                 UdpHeader udpHeader;
//                 pkt->PeekHeader(udpHeader);
//                 src_port = udpHeader.GetSourcePort();
//                 dst_port = udpHeader.GetDestinationPort();
//             }
//         }
//         uint32_t hash = src_ip ^ (dst_ip << 1) ^ (proto << 2) ^ (src_port << 3) ^ (dst_port << 4);
//         idx = hash % next_hops.size();
//         if (idx != 0 && pkt) {
//             // Mark the packet
//             Ptr<Packet> nonConstPkt = const_cast<Packet*>(PeekPointer(pkt));
//             DeflectionMarkTag tag;
//             nonConstPkt->AddPacketTag(tag);
//         }
//     } // else idx remains 0 if marked

    
//     return next_hops[idx];
// }

std::tuple<int32_t, int32_t, int32_t> ArbiterDeflection::TopologySatelliteNetworkDecide(
        int32_t source_node_id,
        int32_t target_node_id,
        Ptr<const Packet> pkt,
        Ipv4Header const &ipHeader,
        bool is_request_for_source_ip_so_no_next_header
) {
    // Hash-based selection of next hop
    const auto& next_hops = m_next_hop_list[target_node_id];
    if (next_hops.empty()) {
        // Return an invalid entry if no next hops are available
        return std::make_tuple(-2, -2, -2);
    }

    uint32_t src_ip = ipHeader.GetSource().Get();
    uint32_t dst_ip = ipHeader.GetDestination().Get();
    uint8_t proto = ipHeader.GetProtocol();
    uint8_t ttl = ipHeader.GetTtl();
    uint16_t src_port = 0;
    uint16_t dst_port = 0;


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


    uint32_t hash = src_ip;
    hash ^= (dst_ip << 7) | (dst_ip >> 25);
    hash ^= (proto << 13);
    hash ^= (src_port << 3) | (src_port >> 13);
    hash ^= (dst_port << 11) | (dst_port >> 5);
    hash ^= (ttl << 17) | (ttl >> 3);
    // Mix with a large prime
    hash *= 2654435761u;
    hash ^= (hash >> 16);
    uint32_t idx = hash % next_hops.size();

    
    return next_hops[idx];
}

// std::tuple<int32_t, int32_t, int32_t> ArbiterDeflection::TopologySatelliteNetworkDecide(
//         int32_t source_node_id,
//         int32_t target_node_id,
//         Ptr<const Packet> pkt,
//         Ipv4Header const &ipHeader,
//         bool is_request_for_source_ip_so_no_next_header
// ) {

//     const auto& next_hops = m_next_hop_list[target_node_id];
//     // if (next_hops.empty()) {
//     //     // Return an invalid entry if no next hops are available
//     //     return std::make_tuple(-2, -2, -2);
//     // }
//         // Use ns-3's random variable generator
//     std::cout << "Current Source node_id " << source_node_id << ": ";
//     std::cout << "next_hops for target_node_id " << target_node_id << ": ";
//     for (size_t i = 0; i < next_hops.size(); ++i) {
//         std::cout << "(" << std::get<0>(next_hops[i]) << ", "
//                   << std::get<1>(next_hops[i]) << ", "
//                   << std::get<2>(next_hops[i]) << ")";
//         if (i != next_hops.size() - 1) std::cout << ", ";
//     }
//     std::cout << std::endl;
//     // Print packet info
//     if (pkt) {
//         if (ipHeader.GetProtocol() == 6) { // TCP
//             TcpHeader tcpHeader;
//             pkt->PeekHeader(tcpHeader);
//             std::cout << "    [TCP Seq: " << tcpHeader.GetSequenceNumber() << "]" << std::endl;
//         }
//     } else {
//         std::cout << "  [Packet: nullptr]" << std::endl;
//     }

//     return next_hops[0];
// }

// void ArbiterDeflection::SetDeflectionState(int32_t target_node_id, int32_t next_node_id, int32_t own_if_id, int32_t next_if_id) {
//     NS_ABORT_MSG_IF(next_node_id == -2 || own_if_id == -2 || next_if_id == -2, "Not permitted to set invalid (-2).");
//     m_next_hop_list[target_node_id] = std::make_tuple(next_node_id, own_if_id, next_if_id);
// }

void ArbiterDeflection::SetDeflectionState(int32_t target_node_id, const std::vector<std::tuple<int32_t, int32_t, int32_t>>& next_hops) {
    // Check that all next_hops are valid
    for (const auto& next_hop : next_hops) {
        int32_t next_node_id = std::get<0>(next_hop);
        int32_t own_if_id = std::get<1>(next_hop);
        int32_t next_if_id = std::get<2>(next_hop);
        NS_ABORT_MSG_IF(next_node_id == -2 || own_if_id == -2 || next_if_id == -2, "Not permitted to set invalid (-2).");
    }
    m_next_hop_list[target_node_id] = next_hops;
}

std::string ArbiterDeflection::StringReprOfForwardingState() {
    std::ostringstream res;
    res << "Deflection-forward state of node " << m_node_id << std::endl;
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

}
