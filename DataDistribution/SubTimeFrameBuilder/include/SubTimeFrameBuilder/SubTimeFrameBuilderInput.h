// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#ifndef ALICEO2_STFBUILDER_INPUT_H_
#define ALICEO2_STFBUILDER_INPUT_H_

#include "Common/SubTimeFrameDataModel.h"
#include "Common/ConcurrentQueue.h"

#include <thread>
#include <vector>

namespace o2
{
namespace DataDistribution
{

class StfBuilderDevice;

class StfInputInterface
{
 public:
  StfInputInterface() = delete;
  StfInputInterface(StfBuilderDevice& pStfBuilderDev)
    : mDevice(pStfBuilderDev)
  {
  }

  void Start(unsigned pCnt);
  void Stop();

  void DataHandlerThread(const unsigned pInputChannelIdx);

 private:
  /// Main SubTimeBuilder O2 device
  StfBuilderDevice& mDevice;

  /// Threads for input channels
  std::vector<std::thread> mInputThreads;
};
}
} /* namespace o2::DataDistribution */

#endif /* ALICEO2_STFBUILDER_INPUT_H_ */
