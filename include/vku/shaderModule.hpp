////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016
//
// Vookoo: command pool wraps VkCommandPool
// 

#ifndef VKU_SHADER_MODULE_INCLUDED
#define VKU_SHADER_MODULE_INCLUDED

#include <vku/resource.hpp>
#include <fstream>

namespace vku {

class shaderModule : public resource<VkShaderModule, shaderModule> {
public:
  VKU_RESOURCE_BOILERPLATE(VkShaderModule, shaderModule)

  shaderModule(VkDevice dev, const std::string &filename, VkShaderStageFlagBits stage) : resource(dev) {
    std::ifstream input(filename, std::ios::binary | std::ios::ate);
    std::streampos length = input.tellg();
    
    if (length <= 0) throw(std::runtime_error("shaderModule: file not found or empty"));

    std::vector<uint8_t> buf((size_t)length);
    input.seekg(0);
    input.read((char*)buf.data(), buf.size());
    if (buf.size() == 0) throw(std::runtime_error("shaderModule(): shader file empty or not found"));

    create(buf.data(), buf.data() + buf.size(), stage);
  }

  shaderModule(VkDevice dev, const uint8_t *b, const uint8_t *e, VkShaderStageFlagBits stage) : resource(dev) {
    create(b, e, stage);
  }

  /// descriptor pool that owns (and creates) its pointer
  void create(const uint8_t *b, const uint8_t *e, VkShaderStageFlagBits stage) {
    std::vector<uint8_t> buf;
    if (*b != 0x03) {
      buf.resize(12);
      ((uint32_t *)buf.data())[0] = 0x07230203; 
      ((uint32_t *)buf.data())[1] = 0;
      ((uint32_t *)buf.data())[2] = stage;
      while (b != e) buf.push_back(*b++);
      buf.push_back(0);
      b = buf.data();
      e = b + buf.size();
    }

    VkShaderModuleCreateInfo moduleCreateInfo = {};
    moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleCreateInfo.codeSize = e - b;
    moduleCreateInfo.pCode = (uint32_t*)b;
    moduleCreateInfo.flags = 0;

    VkShaderModule shaderModule;
    VkResult err = vkCreateShaderModule(dev(), &moduleCreateInfo, NULL, &shaderModule);
    if (err) throw error(err, __FILE__, __LINE__);
    set(shaderModule, true);
  }

  void destroy() {
    if (get()) vkDestroyShaderModule(dev(), get(), nullptr);
  }
};


} // vku

#endif
