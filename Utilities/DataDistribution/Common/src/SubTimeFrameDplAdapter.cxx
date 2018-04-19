// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#include "Common/SubTimeFrameDplAdapter.h"
#include "Common/SubTimeFrameDataModel.h"
#include "Common/SubTimeFrameVisitors.h"


// #include "Framework/runDataProcessing.h"
#include <Framework/WorkflowSpec.h>
#include <Framework/DataProcessingHeader.h>
#include <Framework/ExternalFairMQDeviceProxy.h>
#include <FairMQLogger.h>

namespace dpl = o2::framework;

namespace o2 {
namespace DataDistribution {

////////////////////////////////////////////////////////////////////////////////
/// StfToDplAdapter
////////////////////////////////////////////////////////////////////////////////

namespace dpl = o2::framework;

void StfToDplAdapter::visit(EquipmentHBFrames& pHBFrames)
{
  DataHeader lDataHdr;
  lDataHdr.dataOrigin = mEquipment.mDataOrigin;
  lDataHdr.dataDescription = mEquipment.mDataDescription;
  lDataHdr.subSpecification = mEquipment.mSubSpecification;


  // TODO: handle missing HBFs
  for (auto i = 0; i < pHBFrames.mHBFrames.size(); i++) {

    auto &lHbfMsg = pHBFrames.mHBFrames[i];

    HBFrameHeader lHbfHdr(i);

    // update the size
    lDataHdr.payloadSize = lHbfMsg->GetSize();

    // header stack for HBFrames + DPL (TODO: step param? )
    mDplHdrVector->emplace_back(std::move(o2::header::Stack{lDataHdr, lHbfHdr, mDplHdr}));
    mDplDataVector->emplace_back(std::move(lHbfMsg));
  }

  pHBFrames.mHeader.release();
  pHBFrames.mHBFrames.clear();
}

void StfToDplAdapter::visit(SubTimeFrame& pStf)
{

  mDplHdr = dpl::DataProcessingHeader(pStf.mHeader->mId, 0 /* ? */);

  for (auto& lDataSourceKey : pStf.mReadoutData) {
    auto& lDataSource = lDataSourceKey.second;
    mEquipment = lDataSourceKey.first;

    lDataSource.accept(*this);
  }

  pStf.mHeader.release();
  pStf.mReadoutData.clear();
}

void StfToDplAdapter::adapt(SubTimeFrame &&pStf, std::vector<o2::header::Stack> &pDplHdr, std::vector<FairMQMessagePtr> &pDplData)
{
  mDplHdrVector = &pDplHdr;
  mDplDataVector = &pDplData;

  pStf.accept(*this);

  mDplHdrVector = nullptr;
  mDplDataVector = nullptr;
}


////////////////////////////////////////////////////////////////////////////////
/// DPL injector
////////////////////////////////////////////////////////////////////////////////

dpl::InjectorFunction SubTimeFrameModelDplAdaptor(dpl::OutputSpec const &spec, uint64_t startTime, uint64_t step) {

  auto timesliceId = std::make_shared<size_t>(startTime);

  return [timesliceId, step](FairMQDevice &device, FairMQParts &parts, int index) {

    SubTimeFrame lStf;

#if STF_SERIALIZATION == 1
    InterleavedHdrDataDeserializer lStfReceiver;
#else
  #error "Unsupported serialization";
#endif

    lStfReceiver.deserialize(lStf, parts);

    LOG(DEBUG) << "STFB: received STF size: " << lStf.getDataSize();

    StfToDplAdapter lModelAdapter;

    std::vector<o2::header::Stack> lDplHdrVec;
    std::vector<FairMQMessagePtr> lDplDataVec;

    lModelAdapter.adapt(std::move(lStf), lDplHdrVec, lDplDataVec);

    assert(lDplHdrVec.size() == lDplDataVec.size());

    for (size_t i = 0; i < lDplHdrVec.size(); i++) {

      // TODO: what is the step parameter?
      auto lDplHdr = const_cast<dpl::DataProcessingHeader*>(o2::header::get<dpl::DataProcessingHeader*>(lDplHdrVec[i].data()));
      lDplHdr->startTime = *timesliceId;
      *timesliceId += step;

      dpl::broadcastMessage(device, std::move(lDplHdrVec[i]), std::move(lDplDataVec[i]), index);
    }
  };
}


}
} /* namespace o2::DataDistribution */
