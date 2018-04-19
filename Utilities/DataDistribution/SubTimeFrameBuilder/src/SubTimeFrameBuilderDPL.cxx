// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#include "SubTimeFrameBuilder/SubTimeFrameBuilderDPL.h"
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

    StfToO2Adapter lModelAdapter;

    std::vector<o2::header::Stack> lDplHdr;
    std::vector<FairMQMessagePtr> lDplData;

    lModelAdapter.adapt(std::move(lStf), lDplHdr, lDplData);

    assert(lDplHdr.size() == lDplData.size());

    for (size_t i = 0; i < lDplHdr.size(); i++) {
      dpl::broadcastMessage(device, std::move(lDplHdr[i]), std::move(lDplData[i]), index);
    }
  };
}

}
} /* namespace o2::DataDistribution */
