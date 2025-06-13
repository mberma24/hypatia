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

std::tuple<int32_t, int32_t, int32_t> ArbiterDeflection::TopologySatelliteNetworkDecide(
        int32_t source_node_id,
        int32_t target_node_id,
        Ptr<const Packet> pkt,
        Ipv4Header const &ipHeader,
        bool is_request_for_source_ip_so_no_next_header
) {
    // Randomly select one of the elements in m_next_hop_list[target_node_id] as the next hop.
    const auto& next_hops = m_next_hop_list[target_node_id];
    if (next_hops.empty()) {
        // Return an invalid entry if no next hops are available
        return std::make_tuple(-2, -2, -2);
    }
    // Use ns-3's random variable generator
    Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();
    uint32_t idx = rand->GetInteger(0, next_hops.size() - 1);
    return next_hops[idx];
}

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
