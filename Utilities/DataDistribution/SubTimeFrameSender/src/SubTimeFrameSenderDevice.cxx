// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#include "SubTimeFrameSender/SubTimeFrameSenderDevice.h"
#include "Common/SubTimeFrameDataModel.h"
#include "Common/SubTimeFrameVisitors.h"

#include <options/FairMQProgOptions.h>
#include <FairMQLogger.h>

#include <chrono>
#include <thread>

namespace o2 {
namespace DataDistribution {

StfSenderDevice::StfSenderDevice()
: O2Device{},
  mOutputHandler(*this)
{
}

StfSenderDevice::~StfSenderDevice()
{
}

void StfSenderDevice::InitTask()
{
  mInputChannelName = GetConfig()->GetValue<std::string>(OptionKeyInputChannelName);
  mOutputChannelName = GetConfig()->GetValue<std::string>(OptionKeyOutputChannelName);
  mEpnNodeCount = GetConfig()->GetValue<std::uint32_t>(OptionKeyEpnNodeCount);
}

void StfSenderDevice::PreRun()
{
  // Start output handler
  mOutputHandler.Start(mEpnNodeCount);
}

void StfSenderDevice::PostRun()
{
  // Start output handler
  mOutputHandler.Stop();

  LOG(INFO) << "PostRun done... ";
}

void StfSenderDevice::Run()
{
  auto &lInputChan = GetChannel(mInputChannelName, 0);

#if STF_SERIALIZATION == 1
  InterleavedHdrDataDeserializer lStfReceiver(lInputChan);
#elif STF_SERIALIZATION == 2
  HdrDataDeserializer lStfReceiver(lInputChan);
#else
#error "Unknown STF_SERIALIZATION type"
#endif

  while (CheckCurrentState(StfSenderDevice::RUNNING)) {

    static auto sStartTime = std::chrono::high_resolution_clock::now();
    SubTimeFrame lStf;

    if (!lStfReceiver.deserialize(lStf)) {
      LOG(WARN) << "Error while receiving a STF. Exiting...";
      return;
    }

    const TimeFrameIdType lStfId = lStf.Header().mId;

    { // is there a rate-limited LOG?
      static unsigned long floodgate = 0;
      if (++floodgate % 100 == 1)
        LOG(DEBUG) << "TF[" << lStfId << "] size: " << lStf.getDataSize();
    }

    // Send STF to one of the EPNs (round-robin on STF ID)
    if (mEpnNodeCount > 0) {
      const auto lTargetEpn = lStfId % mEpnNodeCount;

      mOutputHandler.PushStf(lTargetEpn, std::move(lStf));
    }
  }
}

}
} /* namespace o2::DataDistribution */
