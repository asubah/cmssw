#include "BrokenLineFitOnGPU.h"
#include "HeterogeneousCore/CUDAUtilities/interface/device_unique_ptr.h"

template <typename TrackerTraits>
void HelixFitOnGPU<TrackerTraits>::launchBrokenLineKernels(const TrackingRecHitSoAConstView<TrackerTraits>& hv,
                                                           uint32_t hitsInFit,
                                                           uint32_t maxNumberOfTuples,
                                                           cms::LaunchConfigs const &kernelConfigs,
                                                           cudaStream_t stream) {
  assert(tuples_);

  cms::LaunchConfig config = kernelConfigs.getConfig("kernel_BLFastFit");
  auto blockSize_BLFastFit = config.threads[0] > 0 ? config.threads[0] : 64;
  auto numberOfBlocks_BLFastFit = config.blocks[0] > 0 ? config.blocks[0] : ((maxNumberOfConcurrentFits_ + blockSize_BLFastFit - 1) / blockSize_BLFastFit);

  config = kernelConfigs.getConfig("kernel_BLFit");
  auto blockSize_BLFit = config.threads[0] > 0 ? config.threads[0] : 64;
  auto numberOfBlocks_BLFit = config.blocks[0] > 0 ? config.blocks[0] : ((maxNumberOfConcurrentFits_ + blockSize_BLFit - 1) / blockSize_BLFit);

  //  Fit internals
  auto tkidGPU =
      cms::cuda::make_device_unique<typename TrackerTraits::tindex_type[]>(maxNumberOfConcurrentFits_, stream);
  auto hitsGPU = cms::cuda::make_device_unique<double[]>(
      maxNumberOfConcurrentFits_ * sizeof(riemannFit::Matrix3xNd<6>) / sizeof(double), stream);
  auto hits_geGPU = cms::cuda::make_device_unique<float[]>(
      maxNumberOfConcurrentFits_ * sizeof(riemannFit::Matrix6xNf<6>) / sizeof(float), stream);
  auto fast_fit_resultsGPU = cms::cuda::make_device_unique<double[]>(
      maxNumberOfConcurrentFits_ * sizeof(riemannFit::Vector4d) / sizeof(double), stream);

  for (uint32_t offset = 0; offset < maxNumberOfTuples; offset += maxNumberOfConcurrentFits_) {
    // fit triplets

    kernel_BLFastFit<3, TrackerTraits><<<numberOfBlocks_BLFastFit, blockSize_BLFastFit, 0, stream>>>(tuples_,
                                                                                 tupleMultiplicity_,
                                                                                 hv,
                                                                                 tkidGPU.get(),
                                                                                 hitsGPU.get(),
                                                                                 hits_geGPU.get(),
                                                                                 fast_fit_resultsGPU.get(),
                                                                                 3,
                                                                                 3,
                                                                                 offset);
    cudaCheck(cudaGetLastError());

    kernel_BLFit<3, TrackerTraits><<<numberOfBlocks_BLFit, blockSize_BLFit, 0, stream>>>(tupleMultiplicity_,
                                                                             bField_,
                                                                             outputSoa_,
                                                                             tkidGPU.get(),
                                                                             hitsGPU.get(),
                                                                             hits_geGPU.get(),
                                                                             fast_fit_resultsGPU.get());
    cudaCheck(cudaGetLastError());

    if (fitNas4_) {
      // fit all as 4
      riemannFit::rolling_fits<4, TrackerTraits::maxHitsOnTrack, 1>([this,
                                                                     &hv,
                                                                     &tkidGPU,
                                                                     &hitsGPU,
                                                                     &hits_geGPU,
                                                                     &fast_fit_resultsGPU,
                                                                     &offset,
                                                                     &numberOfBlocks_BLFastFit,
                                                                     &numberOfBlocks_BLFit,
                                                                     &blockSize_BLFastFit,
                                                                     &blockSize_BLFit,
                                                                     &stream](auto i) {
        kernel_BLFastFit<4, TrackerTraits><<<numberOfBlocks_BLFastFit / 4, blockSize_BLFastFit, 0, stream>>>(tuples_,
                                                                                         tupleMultiplicity_,
                                                                                         hv,
                                                                                         tkidGPU.get(),
                                                                                         hitsGPU.get(),
                                                                                         hits_geGPU.get(),
                                                                                         fast_fit_resultsGPU.get(),
                                                                                         4,
                                                                                         4,
                                                                                         offset);

        cudaCheck(cudaGetLastError());

        kernel_BLFit<4, TrackerTraits><<<numberOfBlocks_BLFit / 4, blockSize_BLFit, 0, stream>>>(tupleMultiplicity_,
                                                                                     bField_,
                                                                                     outputSoa_,
                                                                                     tkidGPU.get(),
                                                                                     hitsGPU.get(),
                                                                                     hits_geGPU.get(),
                                                                                     fast_fit_resultsGPU.get());

        cudaCheck(cudaGetLastError());
      });

    } else {
      riemannFit::rolling_fits<4, TrackerTraits::maxHitsOnTrackForFullFit, 1>([this,
                                                                               &hv,
                                                                               &tkidGPU,
                                                                               &hitsGPU,
                                                                               &hits_geGPU,
                                                                               &fast_fit_resultsGPU,
                                                                               &offset,
                                                                               &numberOfBlocks_BLFastFit,
                                                                               &numberOfBlocks_BLFit,
                                                                               &blockSize_BLFastFit,
                                                                               &blockSize_BLFit,
                                                                               &stream](auto i) {
        kernel_BLFastFit<i, TrackerTraits><<<numberOfBlocks_BLFastFit / 4, blockSize_BLFastFit, 0, stream>>>(tuples_,
                                                                                         tupleMultiplicity_,
                                                                                         hv,
                                                                                         tkidGPU.get(),
                                                                                         hitsGPU.get(),
                                                                                         hits_geGPU.get(),
                                                                                         fast_fit_resultsGPU.get(),
                                                                                         i,
                                                                                         i,
                                                                                         offset);

        kernel_BLFit<i, TrackerTraits><<<8, blockSize_BLFit, 0, stream>>>(tupleMultiplicity_,
                                                                    bField_,
                                                                    outputSoa_,
                                                                    tkidGPU.get(),
                                                                    hitsGPU.get(),
                                                                    hits_geGPU.get(),
                                                                    fast_fit_resultsGPU.get());
      });

      static_assert(TrackerTraits::maxHitsOnTrackForFullFit < TrackerTraits::maxHitsOnTrack);

      //Fit all the rest using the maximum from previous call
      kernel_BLFastFit<TrackerTraits::maxHitsOnTrackForFullFit, TrackerTraits>
          <<<numberOfBlocks_BLFastFit/ 4, blockSize_BLFastFit, 0, stream>>>(tuples_,
                                                         tupleMultiplicity_,
                                                         hv,
                                                         tkidGPU.get(),
                                                         hitsGPU.get(),
                                                         hits_geGPU.get(),
                                                         fast_fit_resultsGPU.get(),
                                                         TrackerTraits::maxHitsOnTrackForFullFit,
                                                         TrackerTraits::maxHitsOnTrack - 1,
                                                         offset);

      kernel_BLFit<TrackerTraits::maxHitsOnTrackForFullFit, TrackerTraits>
          <<<8, blockSize_BLFit, 0, stream>>>(tupleMultiplicity_,
                                        bField_,
                                        outputSoa_,
                                        tkidGPU.get(),
                                        hitsGPU.get(),
                                        hits_geGPU.get(),
                                        fast_fit_resultsGPU.get());
    }

  }  // loop on concurrent fits
}

template class HelixFitOnGPU<pixelTopology::Phase1>;
template class HelixFitOnGPU<pixelTopology::Phase2>;
