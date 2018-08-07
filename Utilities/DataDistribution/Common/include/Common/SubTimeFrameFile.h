// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#ifndef ALICEO2_SUBTIMEFRAME_FILE_H_
#define ALICEO2_SUBTIMEFRAME_FILE_H_

#include <chrono>

#include <Headers/DataHeader.h>

// class O2Device;

namespace o2 {
namespace DataDistribution {

using namespace std::chrono;

struct SubTimeFrameFileMeta
{
  ///
  /// Version of STF file format
  ///
  const std::uint64_t mStfFileVersion = 1;

  ///
  /// Size of the Stf in file, including this header.
  ///
  std::uint64_t mStfSizeInFile;

  ///
  /// Time when Stf was written (in ms)
  ///
  std::uint64_t mWriteTimeMs;

  auto getTime() {
    return std::chrono::time_point<std::chrono::system_clock, milliseconds>{milliseconds{mWriteTimeMs}};
  }

  SubTimeFrameFileMeta()
  : mStfSizeInFile{0},
    mWriteTimeMs(time_point_cast<milliseconds>(system_clock::now()).time_since_epoch().count())
  { }

  SubTimeFrameFileMeta(const std::uint64_t pStfSize)
  : SubTimeFrameFileMeta()
  {
    mStfSizeInFile = pStfSize;
  }
};

static constexpr o2::header::DataDescription gDataDescFileSubTimeFrame{"FILESUBTIMEFRAME"};
static const o2::header::DataHeader gStfFileDataHeader(
  gDataDescFileSubTimeFrame,
  o2::header::gDataOriginFLP,
  sizeof(SubTimeFrameFileMeta)
);

}
} /* o2::DataDistribution */

#endif /* ALICEO2_SUBTIMEFRAME_FILE_H_ */
