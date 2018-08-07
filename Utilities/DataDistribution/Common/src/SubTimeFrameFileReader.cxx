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
#include "Common/SubTimeFrameFileReader.h"

namespace o2 {
namespace DataDistribution {

////////////////////////////////////////////////////////////////////////////////
/// SubTimeFrameFileReader
////////////////////////////////////////////////////////////////////////////////

SubTimeFrameFileReader::SubTimeFrameFileReader(boost::filesystem::path &pFileName)
{
  using ios = std::ios_base;

  try {
    mFile.open(pFileName.string(), ios::binary | ios::in);
    mFile.exceptions(std::fstream::failbit | std::fstream::badbit);

    // get the file size
    mFile.seekg(0,std::ios_base::end);
    mFileSize = mFile.tellg();
    mFile.seekg(0,std::ios_base::beg);

  } catch(std::ifstream::failure &eOpenErr) {
    LOG(ERROR) << "Failed to open TF file for reading. Error: " << eOpenErr.what();
  }
}

SubTimeFrameFileReader::~SubTimeFrameFileReader()
{
  try {
    if (mFile.is_open())
      mFile.close();
  } catch(std::ifstream::failure &eCloseErr) {
    LOG(ERROR) << "Closing TF file failed. Error: " << eCloseErr.what();
  } catch(...) {
    LOG(ERROR) << "Closing TF file failed.";
  }
}

void SubTimeFrameFileReader::visit(EquipmentHBFrames& pHbFrames)
{
  /* blank */
}

void SubTimeFrameFileReader::visit(SubTimeFrame& pStf)
{
  for (auto &lHdrBlkPair : mBlocks) {
    EquipmentIdentifier lEquipId(lHdrBlkPair.first);
    pStf.addHBFrame(lEquipId, std::move(lHdrBlkPair.second));
  }
}

bool SubTimeFrameFileReader::read(SubTimeFrame& pStf, std::uint64_t pStfId, const FairMQChannel& pDstChan)
{
  // If mFile is good, we're positioned to read a TF
  if (!mFile || mFile.eof()) {
    return false;
  }

  if(!mFile.good()) {
    LOG(WARNING) << "Error while reading a TF from file. (bad stream state)";
    return false;
  }

  // record current position
  const auto lTfStartPosition = this->position();

  DataHeader lStfMetaDataHdr;
  SubTimeFrameFileMeta lStfFileMeta;

  try {
  // Write DataHeader + SubTimeFrameFileMeta
    mFile.read(reinterpret_cast<char*>(&lStfMetaDataHdr), sizeof(DataHeader));
    mFile.read(reinterpret_cast<char*>(&lStfFileMeta), sizeof(SubTimeFrameFileMeta));

  } catch(const std::ios_base::failure& eFailExc) {
    LOG(ERROR) << "Reading from file failed. Error: " << eFailExc.what();
    return false;
  }

  // verify we're actually reading the correct data in
  if (!(gStfFileDataHeader == lStfMetaDataHdr)) {
    LOG(WARNING) << "Reading bad data";
    mFile.close();
    return false;
  }

  // prepare to read the TF data
  const auto lStfSizeInFile = lStfFileMeta.mStfSizeInFile;
  if (lStfSizeInFile == (sizeof(DataHeader) + sizeof(SubTimeFrameFileMeta))) {
    LOG(WARNING) << "Reading an empty TF from file. Only meta information present";
    return false;
  }

  const auto lStfDataSize = lStfSizeInFile - (sizeof(DataHeader) + sizeof(SubTimeFrameFileMeta));

  // check there's enough data in the file
  if (lTfStartPosition + lStfSizeInFile > this->size()) {
    LOG(WARNING) << "Not enough data in file for TF. Required: " << lStfSizeInFile
                 << ", available: " << (this->size() - lTfStartPosition);
    return false;
  }

  // read all data blocks and headers
  mBlocks.clear();
  try {
    DataHeader lBlkDataHdr;
    mFile.read(reinterpret_cast<char*>(&lBlkDataHdr), sizeof(DataHeader));

    auto lDataBlk = pDstChan.NewMessage(lBlkDataHdr.payloadSize);
    mFile.read(reinterpret_cast<char*>(lDataBlk->GetData()), lDataBlk->GetSize());

    mBlocks.emplace_back(
      std::make_pair(
        lBlkDataHdr,
        std::move(lDataBlk)
      )
    );
  } catch(const std::ios_base::failure& eFailExc) {
    LOG(ERROR) << "Reading from file failed. Error: " << eFailExc.what();
    mBlocks.clear();
    return false;
  }

  // build the SubtimeFrame
  pStf.accept(*this);

  mBlocks.clear();

  return true;
}


}
} /* o2::DataDistribution */
