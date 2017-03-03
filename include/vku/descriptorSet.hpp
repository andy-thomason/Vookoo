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
private:
  void destroy() {
  }
};

class descriptorSet : public resource<VkDescriptorSet, descriptorSet> {
public:
  VKU_RESOURCE_BOILERPLATE(VkDescriptorSet, descriptorSet)

  descriptorSet(descriptorPool &descPool, descriptorSetLayout &descLayout) {
    VkDescriptorSetLayout layout = descLayout.get();
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet value = VK_NULL_HANDLE;
    VkResult err = vkAllocateDescriptorSets(dev(), &allocInfo, &value);
    if (err) throw error(err, __FILE__, __LINE__);
    set(value, true);
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

private:
  void destroy() {
  }
};


} // vku

#endif
