#ifndef HeterogeneousCore_KernelConfigurations_interface_KernelConfigurations_h
#define HeterogeneousCore_KernelConfigurations_interface_KernelConfigurations_h

#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"

#include <array>
#include <regex>
#include <unordered_map>
#include <vector>

namespace cms {

  struct LaunchConfig {
    std::string kernelName;
    std::array<uint32_t, 3> threads = {{1, 1, 1}};
    std::array<uint32_t, 3> blocks = {{1, 1, 1}};
  };

  class LaunchConfigs {
  public:
    void insertConfig(LaunchConfig config) {
      configs_.try_emplace(config.kernelName, std::move(config));
    }

    LaunchConfig getConfig(const std::string& kernelName) const {
      const auto it = configs_.find(kernelName);
      return it != configs_.end() ? it->second : LaunchConfig{};
    }

  private:
    std::unordered_map<std::string, LaunchConfig> configs_;
  };

  class KernelConfigurations {
  public:
    struct KernelDescription {
      std::string name;
      std::vector<uint32_t> threads;
      std::vector<uint32_t> blocks;
      std::vector<uint32_t> minThreads;
      std::vector<uint32_t> maxThreads;
      std::vector<uint32_t> minBlocks;
      std::vector<uint32_t> maxBlocks;
    };

    explicit KernelConfigurations(const edm::ParameterSet& config)
        : configurations_(config.getParameter<edm::ParameterSet>("kernels")) {}

    LaunchConfigs getConfigsForDevice(const std::string& device) const {
      LaunchConfigs configs;
      for (const auto& name : configurations_.getParameterNames()) {
        const auto& params = configurations_.getParameter<std::vector<edm::ParameterSet>>(name);
        for (const auto& pset : params) {
          if (deviceMatches(device, pset.getParameter<std::string>("device"))) {
            configs.insertConfig(parseLaunchConfig(name, pset));
            break;
          }
        }
      }
      return configs;
    }

    static void fillBasicDescriptions(edm::ParameterSetDescription& desc,
                                      const std::vector<std::string>& kernelNames) {
      edm::ParameterSetDescription kernelsDesc;
      const auto configDesc = createBasicConfigDescription();
      
      for (const auto& name : kernelNames) {
        kernelsDesc.addVPSet(name, configDesc, {createBasicDefaultConfig()});
      }
      
      desc.add<edm::ParameterSetDescription>("kernels", kernelsDesc);
    }

    static void fillDetailedDescriptions(edm::ParameterSetDescription& desc,
                                         const std::vector<KernelDescription>& kernels) {
      edm::ParameterSetDescription kernelsDesc;
      
      for (const auto& kernel : kernels) {
        kernelsDesc.addVPSet(kernel.name,
                            createDetailedConfigDescription(kernel),
                            {createDetailedDefaultConfig(kernel)});
      }
      
      desc.add<edm::ParameterSetDescription>("kernels", kernelsDesc);
    }

  private:
    edm::ParameterSet configurations_;

    static bool deviceMatches(const std::string& device, const std::string& pattern) {
      try {
        return std::regex_search(device, std::regex(pattern));
      } catch (const std::regex_error&) {
        return false;
      }
    }

    static LaunchConfig parseLaunchConfig(const std::string& name, const edm::ParameterSet& pset) {
      LaunchConfig config;
      config.kernelName = name;
      config.threads = convertToDimensions(pset.getParameter<std::vector<uint32_t>>("threads"));
      config.blocks = convertToDimensions(pset.getParameter<std::vector<uint32_t>>("blocks"));
      return config;
    }

    static std::array<uint32_t, 3> convertToDimensions(const std::vector<uint32_t>& input) {
      std::array<uint32_t, 3> output{{1, 1, 1}};
      for (size_t i = 0; i < input.size() && i < 3; ++i) {
        output[i] = input[i];
      }
      return output;
    }

    static edm::ParameterSetDescription createBasicConfigDescription() {
      edm::ParameterSetDescription desc;
      desc.add<std::string>("device", "");
      desc.add<std::vector<uint32_t>>("threads", {1});
      desc.add<std::vector<uint32_t>>("blocks", {1});
      return desc;
    }

    static edm::ParameterSet createBasicDefaultConfig() {
      edm::ParameterSet pset;
      pset.addParameter<std::string>("device", "");
      pset.addParameter<std::vector<uint32_t>>("threads", {1});
      pset.addParameter<std::vector<uint32_t>>("blocks", {1});
      return pset;
    }

    static edm::ParameterSetDescription createDetailedConfigDescription(const KernelDescription& desc) {
      edm::ParameterSetDescription configDesc;
      configDesc.add<std::string>("device", "");
      configDesc.add<std::vector<uint32_t>>("threads", desc.threads);
      configDesc.add<std::vector<uint32_t>>("blocks", desc.blocks);
      configDesc.add<std::vector<uint32_t>>("minThreads", desc.minThreads);
      configDesc.add<std::vector<uint32_t>>("maxThreads", desc.maxThreads);
      configDesc.add<std::vector<uint32_t>>("minBlocks", desc.minBlocks);
      configDesc.add<std::vector<uint32_t>>("maxBlocks", desc.maxBlocks);
      return configDesc;
    }

    static edm::ParameterSet createDetailedDefaultConfig(const KernelDescription& desc) {
      edm::ParameterSet pset;
      pset.addParameter<std::vector<uint32_t>>("threads", desc.threads);
      pset.addParameter<std::vector<uint32_t>>("blocks", desc.blocks);
      pset.addParameter<std::vector<uint32_t>>("minThreads", desc.minThreads);
      pset.addParameter<std::vector<uint32_t>>("maxThreads", desc.maxThreads);
      pset.addParameter<std::vector<uint32_t>>("minBlocks", desc.minBlocks);
      pset.addParameter<std::vector<uint32_t>>("maxBlocks", desc.maxBlocks);
      return pset;
    }
  };

}  // namespace cms

#endif  // HeterogeneousCore_KernelConfigurations_interface_KernelConfigurations_h
