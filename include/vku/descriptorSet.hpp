////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016
//
// Vookoo: descriptorSet class: wraps VkDescriptorSet
// 

#ifndef VKU_DESCRIPTORSET_INCLUDED
#define VKU_DESCRIPTORSET_INCLUDED

namespace vku {

class descriptorSetLayout : public resource<VkDescriptorSetLayout, descriptorSetLayout> {
public:
  VKU_RESOURCE_BOILERPLATE(VkDescriptorSetLayout, descriptorSetLayout)

  descriptorSetLayout(pipelineCreateHelper &pipelineCreateHelper) {
    VkDescriptorSetLayout value = pipelineCreateHelper.createDescriptorSetLayout(dev());
    if (value) {
      set(value, true);
    }
  }

  void destroy() {
    vkDestroyDescriptorSetLayout(dev(), get(), nullptr);
  }
};

class descriptorSet : public resource<VkDescriptorSet, descriptorSet> {
public:
  VKU_RESOURCE_BOILERPLATE(VkDescriptorSet, descriptorSet)

  descriptorSet(const device &device, descriptorPool &descPool, descriptorSetLayout &descLayout) : resource(device) {
    VkDescriptorSetLayout layout = descLayout.get();
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descPool.get();
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet value = VK_NULL_HANDLE;
    VkResult err = vkAllocateDescriptorSets(dev(), &allocInfo, &value);
    if (err) throw error(err, __FILE__, __LINE__);
    set(value, true);
    pool_ = descPool.get();
  }

  // Update a uniform buffer binding
  void update(uint32_t binding, vku::buffer &buffer) {
    VkWriteDescriptorSet writeDescriptorSet = {};
    VkDescriptorBufferInfo desc = buffer.desc();
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.dstSet = get();
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.dstBinding = binding;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeDescriptorSet.pBufferInfo = &desc;
    vkUpdateDescriptorSets(dev(), 1, &writeDescriptorSet, 0, NULL);
  }

  // Update a uniform buffer binding
  void update(uint32_t binding, vku::sampler &smp, vku::texture &img) {
    VkWriteDescriptorSet writeDescriptorSet = {};
    VkDescriptorImageInfo desc = {};
    desc.sampler = smp.get();
    desc.imageView = img.gpuImage().view();
    desc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.dstSet = get();
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.dstBinding = binding;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writeDescriptorSet.pImageInfo = &desc;
    vkUpdateDescriptorSets(dev(), 1, &writeDescriptorSet, 0, NULL);
  }

  void destroy() {
    if (pool_ && get()) {
      vkFreeDescriptorSets(dev(), pool_, 1, ref());
    }
  }
private:
  VkDescriptorPool pool_ = VK_NULL_HANDLE;
};

} // vku

#endif
