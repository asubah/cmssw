/*
 * TODO: create a kernel configuration helper class to read and manage all the configs
 * create another class to maange single device configs with methods such as:
 * getKernelConfig(string kernelName);
 *
 */
#ifndef HeterogeneousCore_KernelConfigurations_interface_KernelkConfigurations_h
#define HeterogeneousCore_KernelConfigurations_interface_KernelkConfigurations_h

#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"

#include <iostream>

namespace cms {

  struct LaunchConfig {
    std::string kernelName;
    std::vector<uint32_t> threads;
    std::vector<uint32_t> blocks;
  };

  class LaunchConfigs {
    public:
      void insertConfig(LaunchConfig config) { launchConfigs_.emplace_back(config); };

      LaunchConfig getConfig(std::string kernelName) const {
        for (auto config : launchConfigs_) {
          if (config.kernelName == kernelName) {
            LaunchConfig wantedConfig;
            wantedConfig.kernelName = config.kernelName;
            wantedConfig.threads = config.threads;
            wantedConfig.blocks = config.blocks;

            // std::cout << "kernelName: " << wantedConfig.kernelName << '\n';
            // std::cout << "threads: " << wantedConfig.threads[0] << '\n';
            // std::cout << "blocks: " << wantedConfig.blocks[0] << '\n';

            return wantedConfig;
          }
        }

        std::cout << "WARN: kernel \"" + kernelName + "\" not found\n";
        return {"", {0, 0, 0}, {0, 0, 0}};
      }

    private:
      std::vector<LaunchConfig> launchConfigs_;
  };

  class KernelConfigurations {
    public:
      void init(const edm::ParameterSet& iConfig) {
        configurations_ = iConfig.getParameter<edm::ParameterSet>("kernels");
      }

      LaunchConfigs getConfigsForDevice(std::string device) const {
        // TODO: raise error when kerenl not found
        LaunchConfigs wantedConfigs;
        LaunchConfig config;

        std::vector<std::string> parametersNames = configurations_.getParameterNames();
        std::vector<edm::ParameterSet> kernel;
        for (auto name : parametersNames) {
          kernel = configurations_.getParameter<std::vector<edm::ParameterSet>>(name);
          for (auto p : kernel) {
            if (device == p.getParameter<std::string>("device")) {
              config.kernelName = name; 
              config.threads = p.getParameter<std::vector<uint32_t>>("threads");
              config.blocks = p.getParameter<std::vector<uint32_t>>("blocks");

              wantedConfigs.insertConfig(config);
              break;
            }
          }
        }

        return wantedConfigs;
      }

      static void fillKernelDescriptions(edm::ParameterSetDescription& desc, std::vector<std::string> kernelsNames) {
        edm::ParameterSetDescription configsDesc;
        configsDesc.add<std::string>("device", "");
        configsDesc.add<std::vector<unsigned int>>("threads", {0});
        configsDesc.add<std::vector<unsigned int>>("blocks", {0});

        std::vector<edm::ParameterSet> defaultConfigsDescVector;
        
        edm::ParameterSet defaultConfigsDesc;
        defaultConfigsDesc.addParameter<std::string>("device", "");
        defaultConfigsDesc.addParameter<std::vector<unsigned int>>("threads", {0});
        defaultConfigsDesc.addParameter<std::vector<unsigned int>>("blocks", {0});

        defaultConfigsDescVector.push_back(defaultConfigsDesc);

        edm::ParameterSetDescription kernelsDesc;

        for (auto name : kernelsNames) {
          kernelsDesc.addVPSet(name, configsDesc, defaultConfigsDescVector);
        }

        desc.add<edm::ParameterSetDescription>("kernels", kernelsDesc);
      }  

    private:
      edm::ParameterSet configurations_;
  };

}  // namespace cms

#endif  // HeterogeneousCore_KernelConfigurations_interface_KernelkConfigurations_h
