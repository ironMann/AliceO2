// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
#include <Common/SubTimeFrameDplAdapter.h>

#include <Framework/runDataProcessing.h>
#include <Framework/ExternalFairMQDeviceProxy.h>
#include <FairMQLogger.h>
#include <Headers/HeartbeatFrame.h>


using namespace o2::framework;

using DataHeader = o2::header::DataHeader;
using DataOrigin = o2::header::DataOrigin;


// A simple workflow which takes heartbeats from
// a raw FairMQ device as input and uses them as
// part of the DPL.
void defineDataProcessing(WorkflowSpec &specs) {

  auto outspec = OutputSpec{o2::header::gDataOriginTPC,
                            o2::header::gDataDescriptionRawData};

  auto inspec = InputSpec{"heatbeat",
                          o2::header::gDataOriginTPC,
                          o2::header::gDataDescriptionRawData};
  WorkflowSpec workflow;

  workflow.emplace_back(
    specifyExternalFairMQDeviceProxy(
      "SubTimeFrameBuilderDplAdapter",
      {outspec},
      "",
      o2::DataDistribution::SubTimeFrameModelDplAdaptor(outspec, 0, 1)
    )
  );

  workflow.emplace_back(
    DataProcessorSpec{
      "foreign-consumer",
      Inputs{inspec},
      {},
      AlgorithmSpec{
        [](ProcessingContext &ctx) {

          LOG(DEBUG) << ctx.inputs().size();
        }
      }
    }
  );

  specs.swap(workflow);
}
