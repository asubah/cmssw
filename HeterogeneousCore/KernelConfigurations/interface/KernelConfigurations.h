/*
 * TODO: create a kernel configuration helper class to read and manage all the configs
 * create another class to maange single device configs with methods such as:
 * getKernelConfig(string kernelName);
 *
 */
#ifndef HeterogeneousCore_KernelConfigurationsi_interface_KernelkConfigurations_h
#define HeterogeneousCore_KernelConfigurationsi_interface_KernelkConfigurations_h

#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"

#include <iostream>

namespace cms {
  struct KernelParameters {
    std::string device_;
    std::vector<uint32_t> threads_;
    std::vector<uint32_t> blocks_;
  };

  struct LaunchConfigs {
    std::string kernelName_;
    std::vector<uint32_t> threads_;
    std::vector<uint32_t> blocks_;
  };

  using KernelParametersVector = std::vector<KernelParameters>;
  using LaunchConfigsVector = std::vector<LaunchConfigs>;

  class KernelConfigurations {
    public:
      void init(const edm::ParameterSet& iConfig){
        // TODO read all the KernelParameters from the configuration files
        auto kernels = iConfig.getParameter<edm::ParameterSet>("kernels");

        std::vector<std::string> parametersNames = kernels.getParameterNames();
        KernelParameters param;
        KernelParametersVector deviceParams;
        std::vector<edm::ParameterSet> kernel;
        for (auto name : parametersNames) {
          kernel = kernels.getParameter<std::vector<edm::ParameterSet>>(name);
          for (auto p : kernel) {
            param.device_ = p.getParameter<std::string>("device");
            param.threads_ = p.getParameter<std::vector<uint32_t>>("threads");
            param.blocks_ = p.getParameter<std::vector<uint32_t>>("blocks");

            deviceParams.emplace_back(param);
          }

          kernelParameters_.emplace_back(std::make_pair(name, deviceParams));
        }

        for (auto k : kernelParameters_) {
          std::cout << "kernel: " << k.first << '\n';
          for (auto p : k.second) {
            std::cout << "device: " << p.device_ << '\n';
            std::cout << "threads: " << p.threads_[0] << '\n';
            std::cout << "blocks: " << p.blocks_[0] << '\n';
          }
        }
      }

      LaunchConfigsVector getConfigsForDevice(std::string device) const {
        LaunchConfigsVector wantedConfigs;
        LaunchConfigs config;

        for (auto kernel : kernelParameters_) {
          for(auto c : kernel.second) {
            if (c.device_ == device) {
              // config.kernelName_ = kernelName
              config = {kernel.first, c.threads_, c.blocks_};
              wantedConfigs.emplace_back(config);
              break;
            }
          }
        }

        std::cout << "Wanted Configs: \n";
        for (auto c : wantedConfigs) {
          std::cout << "kernel: " << c.kernelName_ << '\n';
          std::cout << "threads: " << c.threads_[0] << '\n';
          std::cout << "blocks: " << c.blocks_[0] << '\n';
        }
        // todo: raise error when kerel not found

        return wantedConfigs;
      }

      static void fillKernelDescriptions(edm::ParameterSetDescription& desc, std::vector<std::string> kernelsNames){
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
      std::vector<std::pair<std::string, KernelParametersVector>> kernelParameters_;
  };

}  // namespace cms

#endif  // HeterogeneousCore_AlpakaInterface_interface_CachingAllocator_h
