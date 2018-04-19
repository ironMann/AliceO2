// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#include "Common/SubTimeFrameVisitors.h"
#include "Common/SubTimeFrameDataModel.h"
#include "Common/DataModelUtils.h"



#include <O2Device/O2Device.h>

#include <stdexcept>

#include <vector>
#include <deque>

namespace o2 {
namespace DataDistribution {

////////////////////////////////////////////////////////////////////////////////
/// InterleavedHdrDataSerializer
////////////////////////////////////////////////////////////////////////////////

void InterleavedHdrDataSerializer::visit(EquipmentHBFrames& pHBFrames)
{
  // header
  mMessages.emplace_back(std::move(pHBFrames.mHeader.getMessage()));

  // iterate all Hbfs
  std::move(
    std::begin(pHBFrames.mHBFrames),
    std::end(pHBFrames.mHBFrames),
    std::back_inserter(mMessages)
  );

  // clean the object
  assert(!pHBFrames.mHeader);
  pHBFrames.mHBFrames.clear();
}

void InterleavedHdrDataSerializer::visit(SubTimeFrame& pStf)
{
  // header
  mMessages.emplace_back(std::move(pStf.mHeader.getMessage()));

  for (auto& lDataSourceKey : pStf.mReadoutData) {
    auto& lDataSource = lDataSourceKey.second;
    lDataSource.accept(*this);
  }
}

void InterleavedHdrDataSerializer::serialize(SubTimeFrame&& pStf)
{
  mMessages.clear();

  pStf.accept(*this);

  mChan.Send(mMessages);
  mMessages.clear();
}

////////////////////////////////////////////////////////////////////////////////
/// InterleavedHdrDataDeserializer
////////////////////////////////////////////////////////////////////////////////

void InterleavedHdrDataDeserializer::visit(EquipmentHBFrames& pHBFrames)
{
  int ret;
  // header
  pHBFrames.mHeader = std::move(*mMsgIter++);

  const auto lHBFramesCnt = pHBFrames.mHeader->payloadSize;

  // move data messages
  std::move(mMsgIter, mMsgIter + lHBFramesCnt, std::back_inserter(pHBFrames.mHBFrames));

  // update the data iterator
  mMsgIter += lHBFramesCnt;
}

void InterleavedHdrDataDeserializer::visit(SubTimeFrame& pStf)
{
  int ret;
  // header
  pStf.mHeader = std::move(*mMsgIter++);

  // iterate over all incoming HBFrame data sources
  for (size_t i = 0; i < pStf.mHeader->payloadSize; i++) {
    EquipmentHBFrames lDataSource;
    lDataSource.accept(*this);

    pStf.mReadoutData.emplace(lDataSource.getEquipmentIdentifier(), std::move(lDataSource));
  }
}

bool InterleavedHdrDataDeserializer::deserialize(SubTimeFrame& pStf, const FairMQChannel& pChan)
{
  int ret;
  mMessages.clear();

  if ((ret = pChan.Receive(mMessages)) < 0)
    throw std::runtime_error("STF receive failed (err = " + std::to_string(ret) + ")");

  mMsgIter = mMessages.begin();

  return deserialize_impl(pStf);
}

bool InterleavedHdrDataDeserializer::deserialize(SubTimeFrame& pStf, FairMQParts &pMsgs)
{
  mMessages = std::move(pMsgs.fParts);
  pMsgs.fParts.clear();
  mMsgIter = mMessages.begin();

  return deserialize_impl(pStf);
}



bool InterleavedHdrDataDeserializer::deserialize_impl(SubTimeFrame& pStf)
{
  try
  {
    pStf.accept(*this);
  }
  catch (std::runtime_error& e)
  {
    // LOG(ERROR) << "SubTimeFrame deserialization failed. Reason: " << e.what();
    return false; // TODO: what? O2Device.Receive() does not throw...?
  }
  catch (std::exception& e)
  {
    LOG(ERROR) << "SubTimeFrame deserialization failed. Reason: " << e.what();
    return false;
  }

  assert(mMsgIter == mMessages.end());

  mMessages.clear();

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// HdrDataSerializer
////////////////////////////////////////////////////////////////////////////////

void HdrDataSerializer::visit(EquipmentHBFrames& pHBFrames)
{

  assert(pHBFrames.mHeader->payloadSize == pHBFrames.mHBFrames.size());

  // header
  mHeaderMessages.emplace_back(std::move(pHBFrames.mHeader.getMessage()));

  // iterate all Hbfs
  std::move(std::begin(pHBFrames.mHBFrames), std::end(pHBFrames.mHBFrames),
            std::back_inserter(mDataMessages));

  assert(!pHBFrames.mHeader);
  pHBFrames.mHBFrames.clear();
}

void HdrDataSerializer::visit(SubTimeFrame& pStf)
{
  assert(pStf.mHeader->payloadSize == pStf.mReadoutData.size());

  mHeaderMessages.emplace_back(std::move(pStf.mHeader.getMessage()));

  for (auto& lDataSourceKey : pStf.mReadoutData) {
    auto& lDataSource = lDataSourceKey.second;
    lDataSource.accept(*this);
  }
}

void HdrDataSerializer::serialize(SubTimeFrame&& pStf)
{
  mHeaderMessages.clear();
  mDataMessages.clear();

  // get messages
  pStf.accept(*this);

  auto lSendData = mHeaderMessages.size() > 1;

  // send headers
  assert(!mHeaderMessages.empty());
  mChan.Send(mHeaderMessages);
  mHeaderMessages.clear();

  if (lSendData) {
    // send data
    assert(!mDataMessages.empty());
    mChan.Send(mDataMessages);
    mDataMessages.clear();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// HdrDataDeserializer
////////////////////////////////////////////////////////////////////////////////

void HdrDataDeserializer::visit(EquipmentHBFrames& pHBFrames)
{
  assert(mHeaderIter < mHeaderMessages.end());
  pHBFrames.mHeader = std::move(*mHeaderIter++);

  const auto lHBFramesCnt = pHBFrames.mHeader->payloadSize;

  // move data messages
  std::move(mDataIter, mDataIter + lHBFramesCnt, std::back_inserter(pHBFrames.mHBFrames));

  // update the data iterator
  mDataIter += lHBFramesCnt;
}

void HdrDataDeserializer::visit(SubTimeFrame& pStf)
{
  assert(mHeaderIter < mHeaderMessages.end());
  pStf.mHeader = std::move(*mHeaderIter++);

  // iterate over all incoming HBFrame data sources
  for (size_t i = 0; i < pStf.mHeader->payloadSize; i++) {
    EquipmentHBFrames lDataSource;
    lDataSource.accept(*this);

    pStf.mReadoutData.emplace(lDataSource.getEquipmentIdentifier(), std::move(lDataSource));
  }
}

bool HdrDataDeserializer::deserialize(SubTimeFrame& pStf)
{
  int ret;

  mHeaderMessages.clear();

  if ((ret = mChan.Receive(mHeaderMessages)) < 0)
    return false;
  mHeaderIter = mHeaderMessages.begin();

  // only receive data messages IF the data exists (TODO: should these NULL STFs be legal?)
  mDataMessages.clear();
  if (mHeaderMessages.size() > 1) {
    if ((ret = mChan.Receive(mDataMessages)) < 0)
      return false;
  }
  mDataIter = mDataMessages.begin();

  try
  {
    // build the SubtimeFrame
    pStf.accept(*this);
  }
  catch (std::runtime_error& e)
  {
    LOG(ERROR) << "SubTimeFrame deserialization failed. Reason: " << e.what();
    return false; // TODO: what? O2Device.Receive() does not throw...?
  }
  catch (std::exception& e)
  {
    LOG(ERROR) << "SubTimeFrame deserialization failed. Reason: " << e.what();
    return false;
  }

  assert(mHeaderIter == mHeaderMessages.end());
  assert(mDataIter == mDataMessages.end());

  // cleanup
  mHeaderMessages.clear();
  mDataMessages.clear();

  return true;
}



}
} /* o2::DataDistribution */
