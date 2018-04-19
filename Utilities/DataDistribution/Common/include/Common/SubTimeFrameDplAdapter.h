// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#ifndef SUB_TIME_FRAME_DPL_ADAPTER_H_
#define SUB_TIME_FRAME_DPL_ADAPTER_H_

#include "Common/SubTimeFrameDataModel.h"
#include "Common/DataModelUtils.h"

#include <Framework/DataProcessingHeader.h>
#include <Framework/OutputSpec.h>
#include <Framework/ExternalFairMQDeviceProxy.h>

#include <vector>

namespace o2 {
namespace DataDistribution {

////////////////////////////////////////////////////////////////////////////////
/// StfToDplAdapter
////////////////////////////////////////////////////////////////////////////////

class StfToDplAdapter : public ISubTimeFrameVisitor {
public:
  StfToDplAdapter() = default;

  void adapt(SubTimeFrame &&pStf, std::vector<o2::header::Stack> &pDplHdr, std::vector<FairMQMessagePtr> &pDplData);

protected:
  void visit(EquipmentHBFrames& pStf) override;
  void visit(SubTimeFrame& pStf) override;

private:

  // cursors
  std::vector<o2::header::Stack> *mDplHdrVector;
  std::vector<FairMQMessagePtr> *mDplDataVector;

  EquipmentIdentifier mEquipment;
  o2::framework::DataProcessingHeader mDplHdr;
};




o2::framework::InjectorFunction SubTimeFrameModelDplAdaptor(o2::framework::OutputSpec const &spec,
  uint64_t startTime, uint64_t step);

}
} /* namespace o2::DataDistribution */

#endif /* SUB_TIME_FRAME_DPL_ADAPTER_H_ */
