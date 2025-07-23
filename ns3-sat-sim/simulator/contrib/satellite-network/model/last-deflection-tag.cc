// my-deflection-tag.cc
#include "last-deflection-tag.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("LastDeflectionTag");
NS_OBJECT_ENSURE_REGISTERED (LastDeflectionTag);

LastDeflectionTag::LastDeflectionTag() : m_last_node(-1), m_num_def(0) {}

LastDeflectionTag::LastDeflectionTag(int32_t node_id)
  : m_last_node(node_id), m_num_def(1) {}


TypeId
LastDeflectionTag::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::LastDeflectionTag")
    .SetParent<Tag> ()
    .AddConstructor<LastDeflectionTag> ();
  return tid;
}

TypeId
LastDeflectionTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

void
LastDeflectionTag::Serialize (TagBuffer i) const
{
  i.WriteU32(static_cast<uint32_t>(m_last_node));
  i.WriteU8(m_num_def);
}

void
LastDeflectionTag::Deserialize (TagBuffer i)
{
  m_last_node = static_cast<int32_t>(i.ReadU32());
  m_num_def = i.ReadU8();
}

uint32_t
LastDeflectionTag::GetSerializedSize (void) const
{
  return 4 + 1;
}

void
LastDeflectionTag::Print (std::ostream &os) const
{
  os << "LastNode=" << m_last_node << ", Num=" << static_cast<uint32_t>(m_num_def);

}

void
LastDeflectionTag::SetLastNode(int32_t nodeId)
{
  m_last_node = nodeId;
  m_num_def += 1;
}

int32_t
LastDeflectionTag::GetLastNode() const
{
  return m_last_node;
}

uint8_t
LastDeflectionTag::GetNumDeflections() const
{
  return m_num_def;
}

} // namespace ns3
