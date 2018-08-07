// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#ifndef ALICEO2_SUBTIMEFRAME_FILE_SINK_H_
#define ALICEO2_SUBTIMEFRAME_FILE_SINK_H_

#include "Common/SubTimeFrameDataModel.h"
#include "Common/SubTimeFrameFileWriter.h"
#include "Common/ConcurrentQueue.h"

#include <Headers/DataHeader.h>

#include <boost/program_options/options_description.hpp>
#include <boost/filesystem.hpp>
#include <fstream>
#include <vector>

class O2Device;

namespace o2 {
namespace DataDistribution {

////////////////////////////////////////////////////////////////////////////////
/// SubTimeFrameFileSink
////////////////////////////////////////////////////////////////////////////////

class SubTimeFrameFileSink {
public:
  static constexpr const char* OptionKeyStfSinkEnable = "stf-sink-enable";
  static constexpr const char* OptionKeyStfSinkDir = "stf-sink-dir";
  static constexpr const char* OptionKeyStfSinkFileName = "stf-sink-file-name";
  static constexpr const char* OptionKeyStfSinkStfsPerFile = "stf-sink-max-stfs-per-file";
  static constexpr const char* OptionKeyStfSinkFileSize = "stf-sink-max-file-size";

  static boost::program_options::options_description getProgramOptions();

  SubTimeFrameFileSink() = delete;
  SubTimeFrameFileSink(O2Device *pDevice, ConcurrentFifo<SubTimeFrame> &pInStfQueue, ConcurrentFifo<SubTimeFrame> &pOutStfQueue);
  ~SubTimeFrameFileSink();

  bool loadVerifyConfig(const FairMQProgOptions &pFMQProgOpt);
  bool is_enabled() const { return mEnabled; }
  void start();
  void stop();

  // TODO: bound the queue
  void queue(SubTimeFrame &&pStf) { mInStfQueue.push(std::move(pStf)); }
  ConcurrentFifo<SubTimeFrame>& out_queue() { return mOutStfQueue; }

  void DataHandlerThread(const unsigned pIdx);

  std::string newStfFileName();

private:
  O2Device *mDevice;
  std::unique_ptr<SubTimeFrameFileWriter> mStfWriter = nullptr;

  /// Configuration
  bool mEnabled;
  std::string mRootDir;
  std::string mCurrentDir;
  std::string mFileNamePattern;
  std::uint64_t mStfsPerFile;
  std::uint64_t mFileSize;

  /// Thread for file writing
  std::thread mSinkThread;
  ConcurrentFifo<SubTimeFrame> &mInStfQueue;
  ConcurrentFifo<SubTimeFrame> &mOutStfQueue;

  /// variables
  unsigned mCurrentFileIdx = 0;
};

}
} /* o2::DataDistribution */

#endif /* ALICEO2_SUBTIMEFRAME_FILE_SINK_H_ */
