// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#include "Common/ReadoutDataModel.h"
#include "Common/SubTimeFrameDataModel.h"

#include <map>
#include <iterator>
#include <algorithm>

namespace o2 {
namespace DataDistribution {

const o2::header::HeaderType HBFrameHeader::sHeaderType = "HBFrame";

////////////////////////////////////////////////////////////////////////////////
/// EquipmentHBFrames
////////////////////////////////////////////////////////////////////////////////

EquipmentHBFrames::EquipmentHBFrames(int pFMQChannelId, const EquipmentIdentifier &pHdr)
{
  mHeader = make_channel_ptr<EquipmentHeader>(pFMQChannelId);

  mHeader->dataDescription = pHdr.mDataDescription;
  mHeader->dataOrigin = pHdr.mDataOrigin;
  mHeader->subSpecification = pHdr.mSubSpecification;
  mHeader->headerSize = sizeof(EquipmentHBFrames);
}

const EquipmentIdentifier EquipmentHBFrames::getEquipmentIdentifier() const
{
  return EquipmentIdentifier (
    mHeader->dataDescription,
    mHeader->dataOrigin,
    mHeader->subSpecification
  );
}

void EquipmentHBFrames::addHBFrame(FairMQMessagePtr &&pHBFrame)
{
  mHBFrames.emplace_back(std::move(pHBFrame));

  // Update payload count
  mHeader->payloadSize = mHBFrames.size();
}

void EquipmentHBFrames::addHBFrames(std::vector<FairMQMessagePtr> &&pHBFrames)
{
  std::move(
    pHBFrames.begin(),
    pHBFrames.end(),
    std::back_inserter(mHBFrames)
  );

  pHBFrames.clear();

  // Update payload count
  mHeader->payloadSize = mHBFrames.size();
}

std::uint64_t EquipmentHBFrames::getDataSize() const
{
  std::uint64_t lDataSize = 0;
  for (const auto& lHBFrame : mHBFrames) {
    lDataSize += lHBFrame->GetSize();
    lHBFrame->GetData();
  }
  return lDataSize;
}

////////////////////////////////////////////////////////////////////////////////
/// SubTimeFrame
////////////////////////////////////////////////////////////////////////////////
SubTimeFrame::SubTimeFrame(int pFMQChannelId, uint64_t pStfId)
{
  mFMQChannelId = pFMQChannelId;

  mHeader = make_channel_ptr<SubTimeFrameHeader>(mFMQChannelId);

  mHeader->mId = pStfId;
  mHeader->headerSize = sizeof(SubTimeFrameHeader);
  mHeader->dataDescription = o2::header::gDataDescriptionSubTimeFrame;
  mHeader->dataOrigin = o2::header::gDataOriginFLP;
  mHeader->payloadSerializationMethod = o2::header::gSerializationMethodNone; // Stf serialization?
  mHeader->payloadSize = 0;                                                   // to hold # of CRUs in the FLP
}

void SubTimeFrame::addHBFrame(const EquipmentIdentifier &pEqId, FairMQMessagePtr &&pHBFrame)
{
  // make the equipment id if needed
  if (mReadoutData.find(pEqId) == mReadoutData.end()) {
    /* add a HBFrame collection for the new equipment */
    mReadoutData.emplace(
      std::piecewise_construct,
      std::make_tuple(pEqId),
      std::make_tuple(mFMQChannelId, pEqId)
    );
  }

  mReadoutData.at(pEqId).addHBFrame(std::move(pHBFrame));

  // update the count
  mHeader->payloadSize = mReadoutData.size();
}

void SubTimeFrame::addHBFrames(const ReadoutSubTimeframeHeader &pHdr, std::vector<FairMQMessagePtr> &&pHBFrames)
{
  // FIXME: proper equipment specification
  EquipmentIdentifier lEquipId(
    o2::header::gDataDescriptionCruData,
    o2::header::gDataOriginCRU,
    pHdr.linkId
  );

  if (mReadoutData.find(lEquipId) == mReadoutData.end()) {
    /* add a HBFrame collection for the new equipment */
    mReadoutData.emplace(
      std::piecewise_construct,
      std::make_tuple(lEquipId),
      std::make_tuple(mFMQChannelId, lEquipId)
    );
  }

  mReadoutData.at(lEquipId).addHBFrames(std::move(pHBFrames));

  // update the count
  mHeader->payloadSize = mReadoutData.size();
}

std::uint64_t SubTimeFrame::getDataSize() const
{
  std::uint64_t lDataSize = 0;

  for (auto& lReadoutDataKey : mReadoutData) {
    auto& lHBFrameData = lReadoutDataKey.second;
    lDataSize += lHBFrameData.getDataSize();
  }

  return lDataSize;
}

std::vector<EquipmentIdentifier> SubTimeFrame::getEquipmentIdentifiers() const
{
  std::vector<EquipmentIdentifier> lKeys;

  transform(std::begin(mReadoutData), std::end(mReadoutData), std::back_inserter(lKeys),
    [](decltype(mReadoutData)::value_type const& pair) {
      return pair.first;
  });

  return lKeys;
}


// TODO: make sure to report miss-configured equipment specs
static bool fixme__EqupId = true;

SubTimeFrame& SubTimeFrame::operator+=(SubTimeFrame&& pStf)
{

  assert(pStf.Header().mId == Header().mId);

  // iterate over all incoming HBFrame data sources
  for (auto &lEquipHBFrame : pStf.mReadoutData) {
    const EquipmentIdentifier &lEquipId = lEquipHBFrame.first;

    // equipment must not repeat
    if (fixme__EqupId && mReadoutData.count(lEquipId) != 0) {
      LOG(WARNING) << "Equipment already present" << lEquipId.info();
      fixme__EqupId = false;
      // throw std::invalid_argument("Cannot add Equipment: already present!");
    }
    // mReadoutData[lEquipId] = std::move(lEquipHBFrame.second);
    mReadoutData.insert(std::make_pair(lEquipId, std::move(lEquipHBFrame.second)));
  }

  // update the count
  mHeader->payloadSize = mReadoutData.size();

  return *this;
}

}
} /* o2::DataDistribution */
