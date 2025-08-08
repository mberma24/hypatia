// my-deflection-tag.h
#ifndef PATH_DEFLECTION_TAG_H
#define PATH_DEFLECTION_TAG_H

#include "ns3/tag.h"
#include "ns3/uinteger.h"
#include "ns3/nstime.h"
#include "ns3/node.h"
#include "ns3/address.h"

namespace ns3 {

class PathDeflectionTag : public Tag
{
public:
  PathDeflectionTag();
  PathDeflectionTag(const int32_t node_id);
  PathDeflectionTag(const std::vector<int32_t>& path);

  PathDeflectionTag(const Ptr<const Packet>);

  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const override;
  virtual void Serialize (TagBuffer i) const override;
  virtual void Deserialize (TagBuffer i) override;
  virtual uint32_t GetSerializedSize (void) const override;
  virtual void Print (std::ostream &os) const override;

  void SetLastNode(int32_t nodeId);
  void SetPath(const std::vector<int32_t>& path);
  std::vector<int32_t> GetPath() const;
  int32_t GetLastNode() const;
  uint8_t GetNumDeflections() const;

private:
  std::vector<int32_t> m_path;
};

} // namespace ns3

#endif // PATH_DEFLECTION_TAG_H