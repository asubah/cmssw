#ifndef HeterogeneousCore_KernelConfigurations_interface_KernelConfigurations_h
#define HeterogeneousCore_KernelConfigurations_interface_KernelConfigurations_h

#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"

#include <iostream>
#include <regex>

namespace cms {

  struct LaunchConfig {
    std::string kernelName;
    std::array<uint32_t, 3> threads;
    std::array<uint32_t, 3> blocks;
  };

  class LaunchConfigs {
  public:
    void insertConfig(LaunchConfig config) { launchConfigs_.emplace_back(config); };

    LaunchConfig getConfig(std::string kernelName) const {
      for (const auto& config : launchConfigs_) {
        if (config.kernelName == kernelName) {
          LaunchConfig wantedConfig;
          wantedConfig.kernelName = config.kernelName;
          wantedConfig.threads = config.threads;
          wantedConfig.blocks = config.blocks;

          return wantedConfig;
        }
      }

      return {"", {{0, 0, 0}}, {{0, 0, 0}}};
    }

  private:
    std::vector<LaunchConfig> launchConfigs_;
  };

  class KernelConfigurations {
  public:
    KernelConfigurations(const edm::ParameterSet& iConfig) {
      configurations_ = iConfig.getParameter<edm::ParameterSet>("kernels");
    }
    
    void init(const edm::ParameterSet& iConfig) {
      configurations_ = iConfig.getParameter<edm::ParameterSet>("kernels");
    }

    LaunchConfigs getConfigsForDevice(std::string device) const {
      LaunchConfigs wantedConfigs;
      LaunchConfig config;
      std::vector<uint32_t> threadsVec;
      std::vector<uint32_t> blocksVec;

      std::vector<std::string> parametersNames = configurations_.getParameterNames();
      std::vector<edm::ParameterSet> kernel;

      std::vector<uint32_t> threads;
      std::vector<uint32_t> blocks;

      for (const auto& name : parametersNames) {
        kernel = configurations_.getParameter<std::vector<edm::ParameterSet>>(name);
        for (const auto& p : kernel) {
          std::string devicePattern = p.getParameter<std::string>("device");
          if (std::regex_search(device, std::regex(devicePattern))) {
            config.kernelName = name;
            threads = p.getParameter<std::vector<uint32_t>>("threads");
            blocks = p.getParameter<std::vector<uint32_t>>("blocks");

            std::copy(threads.begin(), threads.end(), config.threads.begin());
            std::copy(blocks.begin(), blocks.end(), config.blocks.begin());

            wantedConfigs.insertConfig(config);
            break;
          }
        }
      }

      return wantedConfigs;
    }

    static void fillKernelDescriptionsNames(edm::ParameterSetDescription& desc, std::vector<std::string> kernelsNames) {
      edm::ParameterSetDescription configsDesc;
      configsDesc.add<std::string>("device", "");
      configsDesc.add<std::vector<uint32_t>>("threads", {0});
      configsDesc.add<std::vector<uint32_t>>("blocks", {0});

      std::vector<edm::ParameterSet> defaultConfigsDescVector;

      edm::ParameterSet defaultConfigsDesc;
      defaultConfigsDesc.addParameter<std::string>("device", "");
      defaultConfigsDesc.addParameter<std::vector<uint32_t>>("threads", {0});
      defaultConfigsDesc.addParameter<std::vector<uint32_t>>("blocks", {0});

      defaultConfigsDescVector.push_back(defaultConfigsDesc);

      edm::ParameterSetDescription kernelsDesc;

      for (const auto& name : kernelsNames) {
        kernelsDesc.addVPSet(name, configsDesc, defaultConfigsDescVector);
      }

      desc.add<edm::ParameterSetDescription>("kernels", kernelsDesc);
    }

    static void fillKernelDescriptionsDetails(edm::ParameterSetDescription& desc,
                                       std::vector<std::tuple<std::string,            // kernel name
                                                              std::vector<uint32_t>,  // threads
                                                              std::vector<uint32_t>,  // blocks
                                                              std::vector<uint32_t>,  // minThreads
                                                              std::vector<uint32_t>,  // maxThredas
                                                              std::vector<uint32_t>,  // minBlocks
                                                              std::vector<uint32_t>   // maxBlocks
                                                              >> kernelsDescs) {
      edm::ParameterSetDescription defaults;

      for (const auto& kernelDesc : kernelsDescs) {
        auto& [kernelName, threads, blocks, minThreads, maxThreads, minBlocks, maxBlocks] = kernelDesc;
        edm::ParameterSetDescription configsDesc;
        configsDesc.add<std::string>("device", "");
        configsDesc.add<std::vector<uint32_t>>("threads", threads);
        configsDesc.add<std::vector<uint32_t>>("blocks", blocks);
        configsDesc.add<std::vector<uint32_t>>("minThreads", minThreads);
        configsDesc.add<std::vector<uint32_t>>("maxThreads", maxThreads);
        configsDesc.add<std::vector<uint32_t>>("minBlocks", minBlocks);
        configsDesc.add<std::vector<uint32_t>>("maxBlocks", maxBlocks);

        edm::ParameterSet defaultConfigsDesc;
        defaultConfigsDesc.addParameter<std::vector<uint32_t>>("threads", threads);
        defaultConfigsDesc.addParameter<std::vector<uint32_t>>("blocks", blocks);
        defaultConfigsDesc.addParameter<std::vector<uint32_t>>("minThreads", minThreads);
        defaultConfigsDesc.addParameter<std::vector<uint32_t>>("maxThreads", maxThreads);
        defaultConfigsDesc.addParameter<std::vector<uint32_t>>("minBlocks", minBlocks);
        defaultConfigsDesc.addParameter<std::vector<uint32_t>>("maxBlocks", maxBlocks);

        std::vector<edm::ParameterSet> defaultConfigsDescVector;
        defaultConfigsDescVector.push_back(defaultConfigsDesc);

        defaults.addVPSet(kernelName, configsDesc, defaultConfigsDescVector);
      }

      desc.add<edm::ParameterSetDescription>("kernels", defaults);
    }

  private:
    edm::ParameterSet configurations_;
  };

}  // namespace cms

#endif  // HeterogeneousCore_KernelConfigurations_interface_KernelConfigurations_h
