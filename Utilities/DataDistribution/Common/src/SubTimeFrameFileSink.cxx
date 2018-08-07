// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#include "Common/SubTimeFrameFileSink.h"
#include "Common/FilePathUtils.h"

#include <boost/program_options/options_description.hpp>
#include <boost/filesystem.hpp>

#include <chrono>
#include <ctime>
#include <iostream>
#include <iomanip>

namespace o2 {
namespace DataDistribution {

namespace bpo = boost::program_options;

////////////////////////////////////////////////////////////////////////////////
/// SubTimeFrameFileSink
////////////////////////////////////////////////////////////////////////////////


SubTimeFrameFileSink::SubTimeFrameFileSink(O2Device *pDevice,
                                           ConcurrentFifo<SubTimeFrame> &pInStfQueue,
                                           ConcurrentFifo<SubTimeFrame> &pOutStfQueue)
  : mDevice(pDevice),
    mInStfQueue(pInStfQueue),
    mOutStfQueue(pOutStfQueue)
{
}

SubTimeFrameFileSink::~SubTimeFrameFileSink()
{
  mOutStfQueue.stop();
  if (mSinkThread.joinable())
    mSinkThread.join();

  LOG(INFO) << "(Sub)TimeFrame Sink terminated...";
}

void SubTimeFrameFileSink::start()
{
  // NOTE: the thread must be started when Device is in RUNNING state
  mSinkThread = std::thread(&SubTimeFrameFileSink::DataHandlerThread, this, 0);
}

void SubTimeFrameFileSink::stop()
{
  mOutStfQueue.stop();
  if (mSinkThread.joinable())
    mSinkThread.join();
}


bpo::options_description SubTimeFrameFileSink::getProgramOptions()
{

  bpo::options_description lSinkDesc("(Sub)TimeFrame file sink options");

  lSinkDesc.add_options()
  (
    OptionKeyStfSinkEnable,
    bpo::value<bool>()->default_value(false),
    "Enable writing of (Sub)TimeFrames to disk"
  )
  (
    OptionKeyStfSinkDir,
    bpo::value<std::string>()->default_value(""),
    "Specifies a destination directory where (Sub)TimeFrames are to be written"
  )
  (
    OptionKeyStfSinkFileName,
    bpo::value<std::string>()->default_value("%n"),
    "Specifies file name pattern: %n - file index, %D - date, %T - time"
  )
  (
    OptionKeyStfSinkStfsPerFile,
    bpo::value<std::uint64_t>()->default_value(1),
    "Specifies number of (Sub)TimeFrames per file"
  )
  (
    OptionKeyStfSinkFileSize,
    bpo::value<std::uint64_t>()->default_value(std::uint64_t(4) << 30),  /* 4GiB */
    "Specifies target size for (Sub)TimeFrame files"
  )
  ;

  return lSinkDesc;
}

bool SubTimeFrameFileSink::loadVerifyConfig(const FairMQProgOptions &pFMQProgOpt)
{
  mEnabled = pFMQProgOpt.GetValue<bool>(OptionKeyStfSinkEnable);

  LOG(INFO) << "(Sub)TimeFrame file sink " << (mEnabled ? "enabled" : "disabled");

  if (!mEnabled)
    return true;

  mRootDir = pFMQProgOpt.GetValue<std::string>(OptionKeyStfSinkDir);
  if (mRootDir.length() == 0) {
    LOG(ERROR) << "(Sub)TimeFrame file sink directory must be specified";
    return false;
  }

  mFileNamePattern = pFMQProgOpt.GetValue<std::string>(OptionKeyStfSinkFileName);
  mStfsPerFile = std::max(std::uint64_t(1), pFMQProgOpt.GetValue<std::uint64_t>(OptionKeyStfSinkStfsPerFile));
  mFileSize = std::max(std::uint64_t(1), pFMQProgOpt.GetValue<std::uint64_t>(OptionKeyStfSinkFileSize));

  // print options
  LOG(INFO) << "(Sub)TimeFrame Sink :: enabled = " << mEnabled;
  LOG(INFO) << "(Sub)TimeFrame Sink :: root directory = " << mRootDir;
  LOG(INFO) << "(Sub)TimeFrame Sink :: file pattern = " << mFileNamePattern;
  LOG(INFO) << "(Sub)TimeFrame Sink :: stfs per file = " << mStfsPerFile;
  LOG(INFO) << "(Sub)TimeFrame Sink :: max file size = " << mFileSize;

  // make sure directory exists and it is writable
  namespace bfs =  boost::filesystem;
  bfs::path lDirPath(mRootDir);
  if(! bfs::is_directory(lDirPath)) {
    LOG(ERROR) << "(Sub)TimeFrame file sink directory does not exist";
    return false;
  }

  // make a session directory
  mCurrentDir = (bfs::path(mRootDir) / FilePathUtils::getNextSeqName(mRootDir)).string();
  if (!bfs::create_directory(mCurrentDir)) {
    LOG(ERROR) << "Directory '" << mCurrentDir << "' for (Sub)TimeFrame file sink cannot be created";
    return false;
  }

  LOG(INFO) << "(Sub)TimeFrame Sink :: write directory'" << mCurrentDir;

  // initialize the file writer
  std::string lFileName = newStfFileName();

  mStfWriter = std::make_unique<SubTimeFrameFileWriter>(bfs::path(mCurrentDir) / bfs::path(lFileName));

  return true;
}

std::string SubTimeFrameFileSink::newStfFileName()
{
  time_t lNow;
  time(&lNow);
  char lTimeBuf[32];

  std::string lFileName = mFileNamePattern;
  std::stringstream lIdxString;
  lIdxString << std::dec << std::setw(6) << std::setfill('0')  << mCurrentFileIdx;
  boost::replace_all(lFileName, "%n", lIdxString.str());

  strftime(lTimeBuf, sizeof(lTimeBuf), "%F", localtime(&lNow));
  boost::replace_all(lFileName, "%D", lTimeBuf);

  strftime(lTimeBuf, sizeof(lTimeBuf), "%%H_%M_%S", localtime(&lNow));
  boost::replace_all(lFileName, "%T", lTimeBuf);

  mCurrentFileIdx++;
  return lFileName;
}

/// File writing thread
void SubTimeFrameFileSink::DataHandlerThread(const unsigned pIdx)
{
  std::uint64_t lCurrentFileSize = 0;
  std::uint64_t lCurrentFileStfs = 0;

  while (mDevice->CheckCurrentState(O2Device::RUNNING)) {
    SubTimeFrame lStf;

    // Get the next STF
    if (!mInStfQueue.pop(lStf)) {
      // input queue is stopped, bail out
      break;
    }

    if (!is_enabled()) {
      // push the stf to output
      mOutStfQueue.push(std::move(lStf));
      continue;
    }

    // check if we should start a new file
    if ((lCurrentFileStfs >= mStfsPerFile) || (lCurrentFileSize >= mFileSize)) {
      lCurrentFileStfs = 0;
      lCurrentFileSize = 0;
      namespace bfs =  boost::filesystem;
      mStfWriter = std::make_unique<SubTimeFrameFileWriter>(bfs::path(mCurrentDir) / bfs::path(newStfFileName()));
    }

    if (mStfWriter->write(lStf)) {
      lCurrentFileStfs++;
      lCurrentFileSize = mStfWriter->size();
    } else {
      mStfWriter.reset();
      mEnabled = false;
      LOG(ERROR) << "(Sub)TimeFrame file sink: error while writing a file";
      LOG(ERROR) << "(Sub)TimeFrame file sink: disabling writing";
    }
  }
  LOG(INFO) << "Exiting file sink thread[" << pIdx << "]...";
}


}
} /* o2::DataDistribution */
