// my-deflection-tag.cc
#include "last-deflection-tag.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("LastDeflectionTag");
NS_OBJECT_ENSURE_REGISTERED (LastDeflectionTag);

LastDeflectionTag::LastDeflectionTag() {}

LastDeflectionTag::LastDeflectionTag(const std::vector<int32_t>& path)
  : m_path(path) {}

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
  uint32_t size = m_path.size();
  i.WriteU32(size);
  for (uint32_t idx = 0; idx < size; ++idx)
    {
      i.WriteU32(static_cast<uint32_t>(m_path[idx]));
    }
}

void
LastDeflectionTag::Deserialize (TagBuffer i)
{
  uint32_t size = i.ReadU32();
  m_path.clear();
  for (uint32_t idx = 0; idx < size; ++idx)
    {
      m_path.push_back(static_cast<int32_t>(i.ReadU32()));
    }
}

uint32_t
LastDeflectionTag::GetSerializedSize (void) const
{
  // 4 bytes for size + 4 bytes per int32_t entry
  return 4 + 4 * m_path.size();
}

void
LastDeflectionTag::Print (std::ostream &os) const
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
LastDeflectionTag::AddNodeId (int32_t nodeId)
{
  m_path.push_back(nodeId);
}

void
LastDeflectionTag::SetPath (const std::vector<int32_t>& path)
{
  m_path = path;
}

std::vector<int32_t>
LastDeflectionTag::GetPath () const
{
  return m_path;
}

} // namespace ns3
