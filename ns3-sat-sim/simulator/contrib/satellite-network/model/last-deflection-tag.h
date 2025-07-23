// my-deflection-tag.h
#ifndef MY_DEFLECTION_TAG_H
#define MY_DEFLECTION_TAG_H

#include "ns3/tag.h"
#include "ns3/uinteger.h"
#include "ns3/nstime.h"
#include "ns3/node.h"
#include "ns3/address.h"

namespace ns3 {

class LastDeflectionTag : public Tag
{
public:
  LastDeflectionTag();
  LastDeflectionTag(int32_t node_id);

  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const override;
  virtual void Serialize (TagBuffer i) const override;
  virtual void Deserialize (TagBuffer i) override;
  virtual uint32_t GetSerializedSize (void) const override;
  virtual void Print (std::ostream &os) const override;

  void SetLastNode(int32_t nodeId);
  int32_t GetLastNode() const;
  uint8_t GetNumDeflections() const;

private:
  int32_t m_last_node;
  uint8_t m_num_def;
};

} // namespace ns3

#endif // MY_DEFLECTION_TAG_H