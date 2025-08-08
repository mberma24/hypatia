// my-deflection-tag.cc
#include "path-deflection-tag.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("PathDeflectionTag");
NS_OBJECT_ENSURE_REGISTERED (PathDeflectionTag);

PathDeflectionTag::PathDeflectionTag() {}

PathDeflectionTag::PathDeflectionTag(const std::vector<int32_t>& path)
  : m_path(path) {}

PathDeflectionTag::PathDeflectionTag(const int32_t node_id)
  : m_path(node_id) {}

PathDeflectionTag::PathDeflectionTag(const Ptr<const Packet> pkt) {
    if (!pkt->PeekPacketTag(*this)) {
        NS_LOG_WARN("PathDeflectionTag: Packet does not contain tag!");
        // Optionally, throw or set default state here
        m_path.clear();
    }
}

TypeId
PathDeflectionTag::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PathDeflectionTag")
    .SetParent<Tag> ()
    .AddConstructor<PathDeflectionTag> ();
  return tid;
}

TypeId
PathDeflectionTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

void
PathDeflectionTag::Serialize (TagBuffer i) const
{
  uint32_t size = m_path.size();
  i.WriteU32(size);
  for (uint32_t idx = 0; idx < size; ++idx)
    {
      i.WriteU32(static_cast<uint32_t>(m_path[idx]));
    }
}

void
PathDeflectionTag::Deserialize (TagBuffer i)
{
  uint32_t size = i.ReadU32();
  m_path.clear();
  for (uint32_t idx = 0; idx < size; ++idx)
    {
      m_path.push_back(static_cast<int32_t>(i.ReadU32()));
    }
}

uint32_t
PathDeflectionTag::GetSerializedSize (void) const
{
  // 4 bytes for size + 4 bytes per int32_t entry
  return 4 + 4 * m_path.size();
}

void
PathDeflectionTag::Print (std::ostream &os) const
{
  os << "Path=[";
  for (size_t i = 0; i < m_path.size(); ++i)
    {
      os << m_path[i];
      if (i != m_path.size() - 1)
        {
          os << " -> ";
        }
    }
  os << "]";
}

void
PathDeflectionTag::SetLastNode (int32_t nodeId)
{
  m_path.push_back(nodeId);
}

void
PathDeflectionTag::SetPath(const std::vector<int32_t>& path)
{
  m_path = path;
}

std::vector<int32_t>
PathDeflectionTag::GetPath () const
{
  return m_path;
}

int32_t
PathDeflectionTag::GetLastNode() const
{
  if (!m_path.empty())
  {
    return m_path.back();
  }
  return -1; // or throw an error/log it
}

uint8_t
PathDeflectionTag::GetNumDeflections() const
{
  return static_cast<uint8_t>(m_path.size());
}

} // namespace ns3