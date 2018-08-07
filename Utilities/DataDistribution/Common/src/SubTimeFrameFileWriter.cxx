// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#include "Common/SubTimeFrameFile.h"
#include "Common/SubTimeFrameFileWriter.h"

#include <gsl/gsl_util>

namespace o2 {
namespace DataDistribution {

////////////////////////////////////////////////////////////////////////////////
/// SubTimeFrameFileWriter
////////////////////////////////////////////////////////////////////////////////

SubTimeFrameFileWriter::SubTimeFrameFileWriter(boost::filesystem::path pFileName)
{
  using ios = std::ios_base;

  // allocate and set the larger stream buffer
  mFileBuf = std::make_unique<char[]>(sBuffSize);
  mFile.rdbuf()->pubsetbuf(mFileBuf.get(), sBuffSize);
  mFile.clear();
  mFile.exceptions(std::fstream::failbit | std::fstream::badbit);

  try {
    mFile.open(pFileName.string(), ios::binary | ios::out | ios::app | ios::ate);
  } catch(std::ifstream::failure &eOpenErr) {
    LOG(ERROR) << "Failed to open/create TF file for writing. Error: " << eOpenErr.what();
    throw eOpenErr;
  }
}

SubTimeFrameFileWriter::~SubTimeFrameFileWriter()
{
  try {
    mFile.close();
  } catch(std::ifstream::failure &eCloseErr) {
    LOG(ERROR) << "Closing TF file failed. Error: " << eCloseErr.what();
  } catch(...) {
    LOG(ERROR) << "Closing TF file failed.";
  }
}

void SubTimeFrameFileWriter::visit(const EquipmentHBFrames& pHbFrames)
{
  for(const auto &lHbFrame : pHbFrames.mHBFrames) {
    // prepare the header
    mBlockHeaders.emplace_back(DataHeader{
      pHbFrames.Header().dataDescription,
      pHbFrames.Header().dataOrigin,
      pHbFrames.Header().subSpecification,
      lHbFrame->GetSize()
    });

    // add data block
    mBlocksToWrite.emplace_back(
      std::make_pair(
        lHbFrame->GetData(),
        lHbFrame->GetSize()
      )
    );
  }
}

void SubTimeFrameFileWriter::visit(const SubTimeFrame& pStf)
{
  assert(mBlocksToWrite.size() == 0 && mBlockHeaders.size() == 0);

  for (const auto& lDataSourceKey : pStf.mReadoutData) {
    auto& lDataSource = lDataSourceKey.second;
    lDataSource.accept(*this);
  }
}

template <typename pointer,
          typename = std::enable_if_t<std::is_pointer<pointer>::value>>
std::ofstream& SubTimeFrameFileWriter::buffered_write(const pointer p, std::streamsize pCount)
{
  const char *lPtr = reinterpret_cast<const char*>(p);

  // avoid the optimization if the write is large enough
  if (pCount >= sBuffSize) {
    mFile.write(lPtr, pCount);
  } else {

    // split the write to smaller chunks
    while (pCount > 0) {
      const auto lToWrite = std::min(pCount, sChunkSize);
      assert (lToWrite > 0 && lToWrite <= sChunkSize && lToWrite <= pCount);

      mFile.write(lPtr, lToWrite);
      lPtr += lToWrite;
      pCount -= lToWrite;
    }
  }

  return mFile;
}

const std::uint64_t SubTimeFrameFileWriter::stf_size_file() const
{
  std::uint64_t lStfSizeInFile = 0;

  // global hdr+meta
  lStfSizeInFile += sizeof(DataHeader) + sizeof(SubTimeFrameFileMeta);
  // add data headers size
  lStfSizeInFile += (mBlocksToWrite.size() * sizeof(DataHeader));
  // account all data chunks
  for(const auto &lBlk : mBlocksToWrite) {
    lStfSizeInFile += lBlk.second;
  }

  return lStfSizeInFile;
}

std::uint64_t SubTimeFrameFileWriter::write(const SubTimeFrame& pStf)
{
  if(!mFile.good()) {
    LOG(WARNING) << "Error while writing a TF to file. (bad stream state)";
    return std::uint64_t(0);
  }

  // cleanup
  auto lCleanup = gsl::finally([this] {
    // make sure headers and chunk pointers don't linger
    mBlockHeaders.clear();
    mBlocksToWrite.clear();
  });

  // collect all stf blocks
  pStf.accept(*this);
  assert(mBlocksToWrite.size() == mBlockHeaders.size());

  // start writing the blocks
  const std::uint64_t lPrevSize = size();
  const std::uint64_t lStfSizeInFile = stf_size_file();

  SubTimeFrameFileMeta lStfFileMeta(lStfSizeInFile);

  try {
    // Write DataHeader + SubTimeFrameFileMeta
    buffered_write(&gStfFileDataHeader, sizeof(DataHeader));
    buffered_write(&lStfFileMeta, sizeof(SubTimeFrameFileMeta));

    for(auto i = 0ULL; i < mBlocksToWrite.size(); i++) {
      buffered_write(&mBlockHeaders[i], sizeof(DataHeader));
      buffered_write(mBlocksToWrite[i].first, mBlocksToWrite[i].second);
    }

    // flush the buffer and check the state
    mFile.flush();

  } catch(const std::ios_base::failure& eFailExc) {
    LOG(ERROR) << "Writing to file failed. Error: " << eFailExc.what();
    return std::uint64_t(0);
  }

  assert((size() - lPrevSize == lStfSizeInFile) && "Calculated and written sizes differ");

  return std::uint64_t(size() - lPrevSize);
}

}
} /* o2::DataDistribution */
