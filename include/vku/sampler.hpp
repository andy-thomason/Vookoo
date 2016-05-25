////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016
//
// Vookoo: sampler class
// 

#ifndef VKU_SAMPLER_INCLUDED
#define VKU_SAMPLER_INCLUDED

#include <vku/resource.hpp>

namespace vku {

class samplerLayout {
public:
  samplerLayout(int mipLevels = 0) {
    // reasonable defaults.
    info_.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		info_.magFilter = VK_FILTER_LINEAR;
		info_.minFilter = VK_FILTER_LINEAR;
		info_.mipmapMode = mipLevels ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR;
		info_.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		info_.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		info_.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info_.mipLodBias = 0;
    info_.anisotropyEnable = VK_FALSE;
    info_.maxAnisotropy = 0;
    info_.compareEnable = VK_FALSE;
		info_.compareOp = VK_COMPARE_OP_NEVER;
    info_.minLod = 0;
    info_.maxLod = (float)mipLevels;
    info_.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    info_.unnormalizedCoordinates = VK_FALSE;
  }

  VkSamplerCreateFlags flags() const { return info_.flags; }
  VkFilter magFilter() const { return info_.magFilter; }
  VkFilter minFilter() const { return info_.minFilter; }
  VkSamplerMipmapMode mipmapMode() const { return info_.mipmapMode; }
  VkSamplerAddressMode addressModeU() const { return info_.addressModeU; }
  VkSamplerAddressMode addressModeV() const { return info_.addressModeV; }
  VkSamplerAddressMode addressModeW() const { return info_.addressModeW; }
  float mipLodBias() const { return info_.mipLodBias; }
  VkBool32 anisotropyEnable() const { return info_.anisotropyEnable; }
  float maxAnisotropy() const { return info_.maxAnisotropy; }
  VkBool32 compareEnable() const { return info_.compareEnable; }
  VkCompareOp compareOp() const { return info_.compareOp; }
  float minLod() const { return info_.minLod; }
  float maxLod() const { return info_.maxLod; }
  VkBorderColor borderColor() const { return info_.borderColor; }
  VkBool32 unnormalizedCoordinates() const { return info_.unnormalizedCoordinates; }

  samplerLayout &flags(VkSamplerCreateFlags value) { info_.flags = value; return *this; }
  samplerLayout &magFilter(VkFilter value) { info_.magFilter = value; return *this; }
  samplerLayout &minFilter(VkFilter value) { info_.minFilter = value; return *this; }
  samplerLayout &mipmapMode(VkSamplerMipmapMode value) { info_.mipmapMode = value; return *this; }
  samplerLayout &addressModeU(VkSamplerAddressMode value) { info_.addressModeU = value; return *this; }
  samplerLayout &addressModeV(VkSamplerAddressMode value) { info_.addressModeV = value; return *this; }
  samplerLayout &addressModeW(VkSamplerAddressMode value) { info_.addressModeW = value; return *this; }
  samplerLayout &mipLodBias(float value) { info_.mipLodBias = value; return *this; }
  samplerLayout &anisotropyEnable(VkBool32 value) { info_.anisotropyEnable = value; return *this; }
  samplerLayout &maxAnisotropy(float value) { info_.maxAnisotropy = value; return *this; }
  samplerLayout &compareEnable(VkBool32 value) { info_.compareEnable = value; return *this; }
  samplerLayout &compareOp(VkCompareOp value) { info_.compareOp = value; return *this; }
  samplerLayout &minLod(float value) { info_.minLod = value; return *this; }
  samplerLayout &maxLod(float value) { info_.maxLod = value; return *this; }
  samplerLayout &borderColor(VkBorderColor value) { info_.borderColor = value; return *this; }
  samplerLayout &unnormalizedCoordinates(VkBool32 value) { info_.unnormalizedCoordinates = value; return *this; }

  VkSampler createSampler(const vku::device &device) const {
    VkSampler res = 0;
    VkResult err = vkCreateSampler(device, &info_, nullptr, &res);
    if (err) throw error(err, __FILE__, __LINE__);
    return res;
  }
private:
  VkSamplerCreateInfo info_ = {};
};

class sampler : public resource<VkSampler, sampler> {
public:
  sampler() : resource(VK_NULL_HANDLE, VK_NULL_HANDLE) {
  }

  /// Sampler that does not own the handle
  sampler(VkSampler value, VkDevice dev) : resource(value, dev) {
  }

  /// Sampler that owns the handle
  sampler(const vku::device &device, const samplerLayout &layout) : resource(device) {
    set(layout.createSampler(device), true);
  }

  /// move constructor
  sampler(sampler &&rhs) {
    move(std::move(rhs));
  }

  /// move operator
  sampler &operator=(sampler &&rhs) {
    move(std::move(rhs));
    return *this;
  }

  /// copy constructor
  sampler(const sampler &rhs) {
    copy(rhs);
  }

  /// copy operator
  sampler &operator=(const sampler &rhs) {
    copy(rhs);
    return *this;
  }

  void destroy() {
    if (get()) vkDestroySampler(dev(), get(), nullptr);
  }
private:
  void copy(const sampler &rhs) {
    (resource&)*this = (const resource&)rhs;
  }

  void move(sampler &&rhs) {
    (resource&)*this = (resource&&)rhs;
  }
};


} // vku

#endif
