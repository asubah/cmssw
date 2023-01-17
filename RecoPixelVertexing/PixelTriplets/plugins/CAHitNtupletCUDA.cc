#include <cuda_runtime.h>

#include "CUDADataFormats/Common/interface/Product.h"
#include "CUDADataFormats/Track/interface/TrackSoAHeterogeneousDevice.h"
#include "CUDADataFormats/Track/interface/TrackSoAHeterogeneousHost.h"
#include "DataFormats/Common/interface/Handle.h"
#include "FWCore/Framework/interface/ConsumesCollector.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/global/EDProducer.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/PluginManager/interface/ModuleDef.h"
#include "FWCore/Utilities/interface/EDGetToken.h"
#include "FWCore/Utilities/interface/InputTag.h"
#include "FWCore/Utilities/interface/RunningAverage.h"
#include "HeterogeneousCore/CUDACore/interface/ScopedContext.h"
#include "HeterogeneousCore/KernelConfigurations/interface/KernelConfigurations.h"
#include "MagneticField/Records/interface/IdealMagneticFieldRecord.h"
#include "RecoTracker/TkMSParametrization/interface/PixelRecoUtilities.h"

#include "FWCore/ServiceRegistry/interface/Service.h"
#include "HeterogeneousCore/CUDACore/src/chooseDevice.h"
#include "HeterogeneousCore/CUDAServices/interface/CUDAService.h"

#include "CAHitNtupletGeneratorOnGPU.h"

template <typename TrackerTraits>
class CAHitNtupletCUDAT : public edm::global::EDProducer<> {
  using HitsConstView = TrackingRecHitSoAConstView<TrackerTraits>;
  using HitsOnDevice = TrackingRecHitSoADevice<TrackerTraits>;
  using HitsOnHost = TrackingRecHitSoAHost<TrackerTraits>;

  using TrackSoAHost = TrackSoAHeterogeneousHost<TrackerTraits>;
  using TrackSoADevice = TrackSoAHeterogeneousDevice<TrackerTraits>;

  using GPUAlgo = CAHitNtupletGeneratorOnGPU<TrackerTraits>;

public:
  explicit CAHitNtupletCUDAT(const edm::ParameterSet& iConfig);
  ~CAHitNtupletCUDAT() override = default;

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);

private:
  void beginJob() override;
  void endJob() override;

  void produce(edm::StreamID streamID, edm::Event& iEvent, const edm::EventSetup& iSetup) const override;

  bool onGPU_;

  edm::ESGetToken<MagneticField, IdealMagneticFieldRecord> tokenField_;
  edm::EDGetTokenT<cms::cuda::Product<HitsOnDevice>> tokenHitGPU_;
  edm::EDPutTokenT<cms::cuda::Product<TrackSoADevice>> tokenTrackGPU_;
  edm::EDGetTokenT<HitsOnHost> tokenHitCPU_;
  edm::EDPutTokenT<TrackSoAHost> tokenTrackCPU_;

  GPUAlgo gpuAlgo_;

  cms::KernelConfigurations kernelConfigs_;
};

template <typename TrackerTraits>
CAHitNtupletCUDAT<TrackerTraits>::CAHitNtupletCUDAT(const edm::ParameterSet& iConfig)
    : onGPU_(iConfig.getParameter<bool>("onGPU")), tokenField_(esConsumes()), gpuAlgo_(iConfig, consumesCollector()) {
  if (onGPU_) {
    kernelConfigs_.init(iConfig);
    tokenHitGPU_ = consumes(iConfig.getParameter<edm::InputTag>("pixelRecHitSrc"));
    tokenTrackGPU_ = produces<cms::cuda::Product<TrackSoADevice>>();
  } else {
    tokenHitCPU_ = consumes(iConfig.getParameter<edm::InputTag>("pixelRecHitSrc"));
    tokenTrackCPU_ = produces<TrackSoAHost>();
  }


}

template <typename TrackerTraits>
void CAHitNtupletCUDAT<TrackerTraits>::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;

  desc.add<bool>("onGPU", true);
  desc.add<edm::InputTag>("pixelRecHitSrc", edm::InputTag("siPixelRecHitsPreSplittingCUDA"));

  cms::KernelConfigurations::fillKernelDescriptions(desc, {"findClus", "kernel_connect"});
  CAHitNtupletGeneratorOnGPU::fillDescriptions(desc);

  GPUAlgo::fillDescriptions(desc);
  descriptions.addWithDefaultLabel(desc);
}

template <typename TrackerTraits>
void CAHitNtupletCUDAT<TrackerTraits>::beginJob() {
  gpuAlgo_.beginJob();
}

template <typename TrackerTraits>
void CAHitNtupletCUDAT<TrackerTraits>::endJob() {
  gpuAlgo_.endJob();
}

template <typename TrackerTraits>
void CAHitNtupletCUDAT<TrackerTraits>::produce(edm::StreamID streamID,
                                               edm::Event& iEvent,
                                               const edm::EventSetup& es) const {
  auto bf = 1. / es.getData(tokenField_).inverseBzAtOriginInGeV();

  if (onGPU_) {
    auto const& hits = iEvent.get(tokenHitGPU_);

    cms::cuda::ScopedContextProduce ctx{hits};
    auto& hits_d = ctx.get(hits);

    edm::Service<CUDAService> cudaService;
    int cudaDevice = cms::cuda::chooseDevice(streamID);
    std::cout << "cudaDevice: " << cudaDevice << '\n';
    std::pair<int, int> capability = cudaService->computeCapability(cudaDevice);
    cms::LaunchConfigsVector filteredKernelConfigs = kernelConfigs_.getConfigsForDevice("cuda/sm_" + std::to_string(capability.first) + std::to_string(capability.second) + "/T4");

    ctx.emplace(iEvent, tokenTrackGPU_, gpuAlgo_.makeTuplesAsync(hits, bf, ctx.stream(), filteredKernelConfigs));
  } else {
    auto& hits_h = iEvent.get(tokenHitCPU_);
    iEvent.emplace(tokenTrackCPU_, gpuAlgo_.makeTuples(hits_h, bf));
  }
}

using CAHitNtupletCUDAPhase1 = CAHitNtupletCUDAT<pixelTopology::Phase1>;
DEFINE_FWK_MODULE(CAHitNtupletCUDAPhase1);

using CAHitNtupletCUDAPhase2 = CAHitNtupletCUDAT<pixelTopology::Phase2>;
DEFINE_FWK_MODULE(CAHitNtupletCUDAPhase2);
