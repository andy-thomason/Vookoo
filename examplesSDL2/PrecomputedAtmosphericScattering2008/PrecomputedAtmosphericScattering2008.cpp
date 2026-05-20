// Precomputed Atmospheric Scattering — Bruneton & Neyret 2008
// Vulkan port using the vku/SDL2 framework.
//
// Reference:
//   Eric Bruneton, Fabrice Neyret. Precomputed Atmospheric Scattering.
//   Computer Graphics Forum, 2008, Special Issue: Proceedings of the 19th
//   Eurographics Symposium on Rendering 2008, 27(4), pp.1079-1086.
//   https://doi.org/10.1111/j.1467-8659.2008.01245.x
//   https://inria.hal.science/inria-00288758
//
// Controls:
//   Mouse drag          — rotate sun direction
//   Ctrl  + drag        — rotate camera (phi/theta)
//   Shift + drag        — pan surface position (lon/lat)
//   PageUp / PageDown   — change altitude
//   +/-                 — exposure
//   Escape              — quit

#define VKU_SDL2
#include <vku/vku_framework.hpp>
#include <vku/vku.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <cmath>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <array>

// ── Constants (must match common.glsl) ────────────────────────────────────────

static constexpr float   Rg = 6360.0f;
static constexpr float   Rt = 6420.0f;
static constexpr int     TRANSMITTANCE_W = 256, TRANSMITTANCE_H = 64;
static constexpr int     SKY_W = 64,            SKY_H = 16;
static constexpr int     RES_R = 32, RES_MU = 128, RES_MU_S = 32, RES_NU = 8;
static constexpr uint32_t INS_W = uint32_t(RES_MU_S * RES_NU); // 256
static constexpr uint32_t INS_H = uint32_t(RES_MU);            // 128
static constexpr uint32_t INS_D = uint32_t(RES_R);             // 32

static constexpr uint32_t WIN_W = 1024, WIN_H = 768;

// ── Structs ───────────────────────────────────────────────────────────────────

// Matches push_constant block in every precomp frag shader (std430, 32 bytes).
struct PC {
    float     r     = 0.0f;  // offset  0
    float     k     = 0.0f;  // offset  4
    float     first = 0.0f;  // offset  8
    int       layer = 0;     // offset 12
    glm::vec4 dhdH{};        // offset 16
};
static_assert(sizeof(PC) == 32);

// Matches DrawUBO in earth.vert / earth.frag (std140, 160 bytes).
struct DrawUBO {
    glm::vec3 c;        float pad0;     // offset   0
    glm::vec3 s;        float exposure; // offset  16
    glm::mat4 projInverse;              // offset  32
    glm::mat4 viewInverse;              // offset  96
};
static_assert(sizeof(DrawUBO) == 160);

// ── Image helpers ─────────────────────────────────────────────────────────────

struct RT2D {
    vk::UniqueImage        image;
    vk::UniqueDeviceMemory mem;
    vk::UniqueImageView    view;
};

struct RT3D {
    vk::UniqueImage                  image;
    vk::UniqueDeviceMemory           mem;
    vk::UniqueImageView              fullView;
    std::vector<vk::UniqueImageView> layerViews;
};

static uint32_t findMem(const vk::PhysicalDeviceMemoryProperties &mp,
                        uint32_t bits, vk::MemoryPropertyFlags flags) {
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
        if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & flags) == flags)
            return i;
    throw std::runtime_error("no suitable memory type");
}

static RT2D make2DRT(vk::Device dev, const vk::PhysicalDeviceMemoryProperties &mp,
                     uint32_t w, uint32_t h, vk::Format fmt,
                     vk::ImageUsageFlags extraUsage = {}) {
    vk::ImageCreateInfo ci;
    ci.imageType   = vk::ImageType::e2D;
    ci.format      = fmt;
    ci.extent      = vk::Extent3D{w, h, 1};
    ci.mipLevels   = 1;  ci.arrayLayers = 1;
    ci.samples     = vk::SampleCountFlagBits::e1;
    ci.tiling      = vk::ImageTiling::eOptimal;
    ci.usage       = vk::ImageUsageFlagBits::eSampled
                   | vk::ImageUsageFlagBits::eColorAttachment | extraUsage;
    RT2D r;
    r.image = dev.createImageUnique(ci);
    auto req = dev.getImageMemoryRequirements(*r.image);
    r.mem   = dev.allocateMemoryUnique({req.size, findMem(mp, req.memoryTypeBits, {})});
    dev.bindImageMemory(*r.image, *r.mem, 0);
    r.view  = dev.createImageViewUnique({
        {}, *r.image, vk::ImageViewType::e2D, fmt, {},
        {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}});
    return r;
}

static RT3D make3DRT(vk::Device dev, const vk::PhysicalDeviceMemoryProperties &mp,
                     uint32_t w, uint32_t h, uint32_t d, vk::Format fmt) {
    vk::ImageCreateInfo ci;
    ci.flags       = vk::ImageCreateFlagBits::e2DArrayCompatible;
    ci.imageType   = vk::ImageType::e3D;
    ci.format      = fmt;
    ci.extent      = vk::Extent3D{w, h, d};
    ci.mipLevels   = 1;  ci.arrayLayers = 1;
    ci.samples     = vk::SampleCountFlagBits::e1;
    ci.tiling      = vk::ImageTiling::eOptimal;
    ci.usage       = vk::ImageUsageFlagBits::eSampled
                   | vk::ImageUsageFlagBits::eColorAttachment;
    RT3D r;
    r.image = dev.createImageUnique(ci);
    auto req = dev.getImageMemoryRequirements(*r.image);
    r.mem   = dev.allocateMemoryUnique({req.size, findMem(mp, req.memoryTypeBits, {})});
    dev.bindImageMemory(*r.image, *r.mem, 0);
    r.fullView = dev.createImageViewUnique({
        {}, *r.image, vk::ImageViewType::e3D, fmt, {},
        {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}});
    r.layerViews.resize(d);
    for (uint32_t z = 0; z < d; ++z)
        r.layerViews[z] = dev.createImageViewUnique({
            {}, *r.image, vk::ImageViewType::e2D, fmt, {},
            {vk::ImageAspectFlagBits::eColor, 0, 1, z, 1}});
    return r;
}

// ── Image layout transition ───────────────────────────────────────────────────

static void imgTransition(vk::CommandBuffer cb, vk::Image img,
                          vk::ImageLayout from, vk::ImageLayout to) {
    using IL = vk::ImageLayout;
    using AF = vk::AccessFlagBits;
    using PS = vk::PipelineStageFlagBits;

    vk::AccessFlags        srcA, dstA;
    vk::PipelineStageFlags srcS, dstS;

    if      (from == IL::eUndefined)
        { srcA = {};                           srcS = PS::eTopOfPipe; }
    else if (from == IL::eColorAttachmentOptimal)
        { srcA = AF::eColorAttachmentWrite;    srcS = PS::eColorAttachmentOutput; }
    else if (from == IL::eShaderReadOnlyOptimal)
        { srcA = AF::eShaderRead;              srcS = PS::eFragmentShader; }
    else if (from == IL::eTransferDstOptimal)
        { srcA = AF::eTransferWrite;           srcS = PS::eTransfer; }
    else
        { srcA = {};                           srcS = PS::eAllCommands; }

    if      (to == IL::eColorAttachmentOptimal)
        { dstA = AF::eColorAttachmentWrite|AF::eColorAttachmentRead; dstS = PS::eColorAttachmentOutput; }
    else if (to == IL::eShaderReadOnlyOptimal)
        { dstA = AF::eShaderRead;              dstS = PS::eFragmentShader; }
    else if (to == IL::eTransferDstOptimal)
        { dstA = AF::eTransferWrite;           dstS = PS::eTransfer; }
    else
        { dstA = {};                           dstS = PS::eAllCommands; }

    vk::ImageMemoryBarrier b;
    b.srcAccessMask = srcA;  b.dstAccessMask = dstA;
    b.oldLayout     = from;  b.newLayout     = to;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image               = img;
    b.subresourceRange    = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, VK_REMAINING_ARRAY_LAYERS};
    cb.pipelineBarrier(srcS, dstS, {}, {}, {}, b);
}

// ── setLayer ──────────────────────────────────────────────────────────────────

static void setLayer(PC &pc, int layer) {
    double r = layer / (RES_R - 1.0);
    r = r * r;
    r = std::sqrt(double(Rg)*Rg + r*(double(Rt)*Rt - double(Rg)*Rg));
    if      (layer == 0)       r += 0.01;
    else if (layer == RES_R-1) r -= 0.001;
    double dmin  = Rt - r;
    double dmax  = std::sqrt(r*r - double(Rg)*Rg) + std::sqrt(double(Rt)*Rt - double(Rg)*Rg);
    double dminp = r - Rg;
    double dmaxp = std::sqrt(r*r - double(Rg)*Rg);
    pc.r     = float(r);
    pc.dhdH  = {float(dmin), float(dmax), float(dminp), float(dmaxp)};
    pc.layer = layer;
}

// ── Render pass builders ──────────────────────────────────────────────────────

static vk::UniqueRenderPass makeSingleRP(vk::Device dev, vk::Format fmt, bool load) {
    vk::AttachmentDescription att;
    att.format = fmt;  att.samples = vk::SampleCountFlagBits::e1;
    att.loadOp         = load ? vk::AttachmentLoadOp::eLoad : vk::AttachmentLoadOp::eDontCare;
    att.storeOp        = vk::AttachmentStoreOp::eStore;
    att.stencilLoadOp  = vk::AttachmentLoadOp::eDontCare;
    att.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    att.initialLayout  = vk::ImageLayout::eColorAttachmentOptimal;
    att.finalLayout    = vk::ImageLayout::eColorAttachmentOptimal;

    vk::AttachmentReference ref{0, vk::ImageLayout::eColorAttachmentOptimal};
    vk::SubpassDescription  sub;
    sub.pipelineBindPoint    = vk::PipelineBindPoint::eGraphics;
    sub.colorAttachmentCount = 1;  sub.pColorAttachments = &ref;

    vk::RenderPassCreateInfo ci;
    ci.attachmentCount = 1; ci.pAttachments = &att;
    ci.subpassCount    = 1; ci.pSubpasses   = &sub;
    return dev.createRenderPassUnique(ci);
}

static vk::UniqueRenderPass makeDualRP(vk::Device dev, vk::Format fmt) {
    vk::AttachmentDescription att[2];
    for (auto &a : att) {
        a.format = fmt;  a.samples = vk::SampleCountFlagBits::e1;
        a.loadOp         = vk::AttachmentLoadOp::eDontCare;
        a.storeOp        = vk::AttachmentStoreOp::eStore;
        a.stencilLoadOp  = vk::AttachmentLoadOp::eDontCare;
        a.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        a.initialLayout  = vk::ImageLayout::eColorAttachmentOptimal;
        a.finalLayout    = vk::ImageLayout::eColorAttachmentOptimal;
    }
    vk::AttachmentReference refs[2] = {
        {0, vk::ImageLayout::eColorAttachmentOptimal},
        {1, vk::ImageLayout::eColorAttachmentOptimal}};
    vk::SubpassDescription sub;
    sub.pipelineBindPoint    = vk::PipelineBindPoint::eGraphics;
    sub.colorAttachmentCount = 2;  sub.pColorAttachments = refs;

    vk::RenderPassCreateInfo ci;
    ci.attachmentCount = 2; ci.pAttachments = att;
    ci.subpassCount    = 1; ci.pSubpasses   = &sub;
    return dev.createRenderPassUnique(ci);
}

// ── Framebuffer builders ──────────────────────────────────────────────────────

static vk::UniqueFramebuffer makeFB(vk::Device dev, vk::RenderPass rp,
                                    vk::ImageView view, uint32_t w, uint32_t h) {
    return dev.createFramebufferUnique({{}, rp, 1, &view, w, h, 1});
}

static vk::UniqueFramebuffer makeDualFB(vk::Device dev, vk::RenderPass rp,
                                        vk::ImageView v0, vk::ImageView v1,
                                        uint32_t w, uint32_t h) {
    vk::ImageView views[2] = {v0, v1};
    return dev.createFramebufferUnique({{}, rp, 2, views, w, h, 1});
}

// ── Precomp pipeline builder ──────────────────────────────────────────────────

static vk::UniquePipeline makePrecompPipeline(
    vk::Device dev, vk::PipelineCache cache,
    vk::PipelineLayout layout, vk::RenderPass rp,
    const vku::ShaderModule &vert, const vku::ShaderModule &frag,
    bool additive = false, int numAttachments = 1)
{
    vku::PipelineMaker pm{1, 1};
    pm.shader(vk::ShaderStageFlagBits::eVertex,   vert)
      .shader(vk::ShaderStageFlagBits::eFragment, frag)
      .topology(vk::PrimitiveTopology::eTriangleList)
      .depthTestEnable(VK_FALSE)
      .depthWriteEnable(VK_FALSE)
      .cullMode(vk::CullModeFlagBits::eNone)
      .dynamicState(vk::DynamicState::eViewport)
      .dynamicState(vk::DynamicState::eScissor);

    auto bf = vk::BlendFactor::eOne;
    if (additive) {
        pm.blendBegin(VK_TRUE)
          .blendSrcColorBlendFactor(bf).blendDstColorBlendFactor(bf)
          .blendColorBlendOp(vk::BlendOp::eAdd)
          .blendSrcAlphaBlendFactor(bf).blendDstAlphaBlendFactor(bf)
          .blendAlphaBlendOp(vk::BlendOp::eAdd);
    } else {
        pm.blendBegin(VK_FALSE);
    }
    if (numAttachments == 2)
        pm.blendBegin(VK_FALSE);  // second attachment for dual-output (inscatter1)

    return pm.createUnique(dev, cache, layout, rp, false);
}

// ── Staging upload helper ─────────────────────────────────────────────────────

static void stageUpload(vk::Device dev, const vk::PhysicalDeviceMemoryProperties &mp,
                        vk::CommandPool pool, vk::Queue queue,
                        vk::Image dst, uint32_t w, uint32_t h,
                        const void *pixels, vk::DeviceSize bytes) {
    vk::BufferCreateInfo bci{{}, bytes, vk::BufferUsageFlagBits::eTransferSrc};
    auto stageBuf = dev.createBufferUnique(bci);
    auto req      = dev.getBufferMemoryRequirements(*stageBuf);
    auto stageMem = dev.allocateMemoryUnique({req.size,
        findMem(mp, req.memoryTypeBits,
                vk::MemoryPropertyFlagBits::eHostVisible |
                vk::MemoryPropertyFlagBits::eHostCoherent)});
    dev.bindBufferMemory(*stageBuf, *stageMem, 0);
    std::memcpy(dev.mapMemory(*stageMem, 0, bytes), pixels, bytes);
    dev.unmapMemory(*stageMem);

    vku::executeImmediately(dev, pool, queue, [&](vk::CommandBuffer cb) {
        imgTransition(cb, dst, vk::ImageLayout::eUndefined,
                      vk::ImageLayout::eTransferDstOptimal);
        vk::BufferImageCopy region;
        region.imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1};
        region.imageExtent      = vk::Extent3D{w, h, 1};
        cb.copyBufferToImage(*stageBuf, dst,
                             vk::ImageLayout::eTransferDstOptimal, region);
        imgTransition(cb, dst, vk::ImageLayout::eTransferDstOptimal,
                      vk::ImageLayout::eShaderReadOnlyOptimal);
    });
}

// ─────────────────────────────────────────────────────────────────────────────

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1;
    }
    SDL_Window *sdlWin = SDL_CreateWindow(
        "Precomputed Atmospheric Scattering 2008",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_W, WIN_H, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!sdlWin) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); SDL_Quit(); return 1;
    }

    {
        vku::Framework fw{"PrecomputedAtmosphericScattering2008"};
        if (!fw.ok()) { fputs("Framework creation failed\n", stderr); return 1; }
        vk::Device dev    = fw.device();
        const auto &mp    = fw.memprops();
        auto pCache       = fw.pipelineCache();

        vku::Window window(fw.instance(), dev, fw.physicalDevice(),
                           fw.graphicsQueueFamilyIndex(),
                           sdlWin);
        if (!window.ok()) { fputs("Window creation failed\n", stderr); return 1; }
        window.clearColorValue() = {0.0f, 0.0f, 0.0f, 1.0f};

        vk::CommandPool cmdPool = window.commandPool();
        vk::Queue       gfxQ   = fw.graphicsQueue();

        // ── Load earth reflectance texture ────────────────────────────────────
        int tw = 0, th = 0;
        uint8_t *earthPx = stbi_load(
            BINARY_DIR "earth.png",
            &tw, &th, nullptr, STBI_rgb_alpha);
        if (!earthPx) {
            fprintf(stderr, "Failed to load earth.png: %s\n", stbi_failure_reason());
            return 1;
        }
        auto reflRT = make2DRT(dev, mp, uint32_t(tw), uint32_t(th),
                               vk::Format::eR8G8B8A8Unorm,
                               vk::ImageUsageFlagBits::eTransferDst);
        stageUpload(dev, mp, cmdPool, gfxQ, *reflRT.image,
                    uint32_t(tw), uint32_t(th), earthPx,
                    vk::DeviceSize(tw) * th * 4);
        stbi_image_free(earthPx);

        // ── Precomp textures ──────────────────────────────────────────────────
        static constexpr auto F = vk::Format::eR16G16B16A16Sfloat;

        auto txT  = make2DRT(dev, mp, TRANSMITTANCE_W, TRANSMITTANCE_H, F);
        auto txE  = make2DRT(dev, mp, SKY_W, SKY_H, F);   // irradiance E
        auto txDE = make2DRT(dev, mp, SKY_W, SKY_H, F);   // deltaE
        auto txS  = make3DRT(dev, mp, INS_W, INS_H, INS_D, F);  // inscatter S
        auto txSR = make3DRT(dev, mp, INS_W, INS_H, INS_D, F);  // deltaSR
        auto txSM = make3DRT(dev, mp, INS_W, INS_H, INS_D, F);  // deltaSM
        auto txJ  = make3DRT(dev, mp, INS_W, INS_H, INS_D, F);  // deltaJ

        // Dummy textures to fill unused descriptor bindings.
        auto dummy2D = make2DRT(dev, mp, 1, 1, F);
        auto dummy3D = make3DRT(dev, mp, 1, 1, 1, F);

        // ── Samplers ──────────────────────────────────────────────────────────
        // Atmosphere LUT sampler: clamp all axes (no wrap needed for lookup tables).
        auto sampler = vku::SamplerMaker{}
            .magFilter(vk::Filter::eLinear).minFilter(vk::Filter::eLinear)
            .mipmapMode(vk::SamplerMipmapMode::eNearest)
            .addressModeU(vk::SamplerAddressMode::eClampToEdge)
            .addressModeV(vk::SamplerAddressMode::eClampToEdge)
            .addressModeW(vk::SamplerAddressMode::eClampToEdge)
            .createUnique(dev);
        vk::Sampler samp = *sampler;

        // Earth texture sampler: U = eRepeat so the longitude seam (atan wrap at
        // ±π → UV 0/1) bilinearly blends correctly across the wrap boundary instead
        // of clamping to the edge texel and producing tile-shaped artifacts.
        auto earthSampler = vku::SamplerMaker{}
            .magFilter(vk::Filter::eLinear).minFilter(vk::Filter::eLinear)
            .mipmapMode(vk::SamplerMipmapMode::eNearest)
            .addressModeU(vk::SamplerAddressMode::eRepeat)
            .addressModeV(vk::SamplerAddressMode::eClampToEdge)
            .addressModeW(vk::SamplerAddressMode::eClampToEdge)
            .createUnique(dev);
        vk::Sampler eSamp = *earthSampler;

        // ── Render passes ─────────────────────────────────────────────────────
        auto rpSingle  = makeSingleRP(dev, F, false);
        auto rpSingleL = makeSingleRP(dev, F, true);  // loadOp=eLoad (additive blend)
        auto rpDual    = makeDualRP(dev, F);

        // ── Descriptor set layouts ────────────────────────────────────────────
        auto precompDSL = vku::DescriptorSetLayoutMaker{}
            .image(0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1)
            .image(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1)
            .image(2, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1)
            .image(3, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1)
            .image(4, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1)
            .image(5, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1)
            .createUnique(dev);

        auto drawDSL = vku::DescriptorSetLayoutMaker{}
            .buffer(0, vk::DescriptorType::eUniformBuffer,
                    vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 1)
            .image(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1)
            .image(2, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1)
            .image(3, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1)
            .image(4, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1)
            .createUnique(dev);

        // ── Pipeline layouts ──────────────────────────────────────────────────
        auto precompPL = vku::PipelineLayoutMaker{}
            .descriptorSetLayout(*precompDSL)
            .pushConstantRange(vk::ShaderStageFlagBits::eFragment, 0, sizeof(PC))
            .createUnique(dev);

        auto drawPL = vku::PipelineLayoutMaker{}
            .descriptorSetLayout(*drawDSL)
            .createUnique(dev);

        // ── Shaders ───────────────────────────────────────────────────────────
        vku::ShaderModule quadVert   {dev, BINARY_DIR "quad.vert.spv"};
        vku::ShaderModule earthVert  {dev, BINARY_DIR "earth.vert.spv"};
        vku::ShaderModule transF     {dev, BINARY_DIR "transmittance.frag.spv"};
        vku::ShaderModule irrad1F    {dev, BINARY_DIR "irradiance1.frag.spv"};
        vku::ShaderModule insc1F     {dev, BINARY_DIR "inscatter1.frag.spv"};
        vku::ShaderModule copyIrrF   {dev, BINARY_DIR "copyIrradiance.frag.spv"};
        vku::ShaderModule copyInsc1F {dev, BINARY_DIR "copyInscatter1.frag.spv"};
        vku::ShaderModule inscSF     {dev, BINARY_DIR "inscatterS.frag.spv"};
        vku::ShaderModule irradNF    {dev, BINARY_DIR "irradianceN.frag.spv"};
        vku::ShaderModule inscNF     {dev, BINARY_DIR "inscatterN.frag.spv"};
        vku::ShaderModule copyInscNF {dev, BINARY_DIR "copyInscatterN.frag.spv"};
        vku::ShaderModule earthFrag  {dev, BINARY_DIR "earth.frag.spv"};

        // ── Precomp pipelines ─────────────────────────────────────────────────
        auto pTrans      = makePrecompPipeline(dev, pCache, *precompPL, *rpSingle,  quadVert, transF);
        auto pIrrad1     = makePrecompPipeline(dev, pCache, *precompPL, *rpSingle,  quadVert, irrad1F);
        auto pInsc1      = makePrecompPipeline(dev, pCache, *precompPL, *rpDual,    quadVert, insc1F,    false, 2);
        auto pCopyIrr    = makePrecompPipeline(dev, pCache, *precompPL, *rpSingle,  quadVert, copyIrrF);
        auto pCopyIrrAdd = makePrecompPipeline(dev, pCache, *precompPL, *rpSingleL, quadVert, copyIrrF,  true);
        auto pCopyInsc1  = makePrecompPipeline(dev, pCache, *precompPL, *rpSingle,  quadVert, copyInsc1F);
        auto pInscS      = makePrecompPipeline(dev, pCache, *precompPL, *rpSingle,  quadVert, inscSF);
        auto pIrradN     = makePrecompPipeline(dev, pCache, *precompPL, *rpSingle,  quadVert, irradNF);
        auto pInscN      = makePrecompPipeline(dev, pCache, *precompPL, *rpSingle,  quadVert, inscNF);
        auto pCopyInscN  = makePrecompPipeline(dev, pCache, *precompPL, *rpSingleL, quadVert, copyInscNF, true);

        // ── Draw pipeline ─────────────────────────────────────────────────────
        auto pDraw = [&]() {
            vku::PipelineMaker pm{WIN_W, WIN_H};
            return pm.shader(vk::ShaderStageFlagBits::eVertex,   earthVert)
                     .shader(vk::ShaderStageFlagBits::eFragment, earthFrag)
                     .topology(vk::PrimitiveTopology::eTriangleList)
                     .depthTestEnable(VK_FALSE).depthWriteEnable(VK_FALSE)
                     .cullMode(vk::CullModeFlagBits::eNone)
                     .dynamicState(vk::DynamicState::eViewport)
                     .dynamicState(vk::DynamicState::eScissor)
                     .blendBegin(VK_FALSE)
                     .createUnique(dev, pCache, *drawPL, window.renderPass(), false);
        }();

        // ── Descriptor pool ───────────────────────────────────────────────────
        std::array<vk::DescriptorPoolSize, 2> poolSizes{{
            {vk::DescriptorType::eCombinedImageSampler, 80},
            {vk::DescriptorType::eUniformBuffer,         2},
        }};
        auto descPool = dev.createDescriptorPoolUnique({
            vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            16u, uint32_t(poolSizes.size()), poolSizes.data()});

        // ── UBO ───────────────────────────────────────────────────────────────
        vku::UniformBuffer ubo(dev, mp, sizeof(DrawUBO));

        // ── Descriptor sets ───────────────────────────────────────────────────
        // Indices: 0=transmittance 1=irradiance1 2=inscatter1 3=copyIrradiance
        //          4=copyInscatter1 5=inscatterS 6=irradianceN 7=inscatterN
        //          8=copyInscatterN 9=draw
        auto dsets = vku::DescriptorSetMaker{}
            .layout(*precompDSL).layout(*precompDSL).layout(*precompDSL)
            .layout(*precompDSL).layout(*precompDSL).layout(*precompDSL)
            .layout(*precompDSL).layout(*precompDSL).layout(*precompDSL)
            .layout(*drawDSL)
            .create(dev, *descPool);

        // Convenience: bind 6 combined-image-sampler slots for a precomp DS.
        auto IL_SRO = vk::ImageLayout::eShaderReadOnlyOptimal;
        auto bindPrecomp = [&](int idx,
            vk::ImageView b0, vk::ImageView b1, vk::ImageView b2,
            vk::ImageView b3, vk::ImageView b4, vk::ImageView b5)
        {
            vku::DescriptorSetUpdater{}
                .beginDescriptorSet(dsets[idx])
                .beginImages(0, 0, vk::DescriptorType::eCombinedImageSampler)
                .image(samp, b0, IL_SRO)
                .beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler)
                .image(samp, b1, IL_SRO)
                .beginImages(2, 0, vk::DescriptorType::eCombinedImageSampler)
                .image(samp, b2, IL_SRO)
                .beginImages(3, 0, vk::DescriptorType::eCombinedImageSampler)
                .image(samp, b3, IL_SRO)
                .beginImages(4, 0, vk::DescriptorType::eCombinedImageSampler)
                .image(samp, b4, IL_SRO)
                .beginImages(5, 0, vk::DescriptorType::eCombinedImageSampler)
                .image(samp, b5, IL_SRO)
                .update(dev);
        };

        vk::ImageView d2  = *dummy2D.view,    d3  = *dummy3D.fullView;
        vk::ImageView vT  = *txT.view,        vE  = *txE.view;
        vk::ImageView vDE = *txDE.view;
        vk::ImageView vS  = *txS.fullView,    vSR = *txSR.fullView;
        vk::ImageView vSM = *txSM.fullView,   vJ  = *txJ.fullView;

        //         idx   b0   b1   b2   b3   b4   b5
        bindPrecomp(0,   d2,  d2,  d3,  d3,  d3,  d3); // transmittance
        bindPrecomp(1,   vT,  d2,  d3,  d3,  d3,  d3); // irradiance1
        bindPrecomp(2,   vT,  d2,  d3,  d3,  d3,  d3); // inscatter1
        bindPrecomp(3,   d2, vDE,  d3,  d3,  d3,  d3); // copyIrradiance
        bindPrecomp(4,   d2,  d2, vSR, vSM,  d3,  d3); // copyInscatter1
        bindPrecomp(5,   vT, vDE, vSR, vSM,  d3,  d3); // inscatterS (jProg)
        bindPrecomp(6,   vT,  d2, vSR, vSM,  d3,  d3); // irradianceN
        bindPrecomp(7,   vT,  d2,  d3,  d3,  vJ,  d3); // inscatterN
        bindPrecomp(8,   d2,  d2,  d3,  d3,  d3, vSR); // copyInscatterN

        // Draw DS (binding 0=UBO, 1=transmittance, 2=irradiance, 3=inscatter, 4=reflectance)
        {
            vku::DescriptorSetUpdater upd(10, 10);
            upd.beginDescriptorSet(dsets[9])
               .beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer)
               .buffer(ubo.buffer(), 0, sizeof(DrawUBO))
               .beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler)
               .image(samp, vT, IL_SRO)
               .beginImages(2, 0, vk::DescriptorType::eCombinedImageSampler)
               .image(samp, vE, IL_SRO)
               .beginImages(3, 0, vk::DescriptorType::eCombinedImageSampler)
               .image(samp, vS, IL_SRO)
               .beginImages(4, 0, vk::DescriptorType::eCombinedImageSampler)
               .image(eSamp, *reflRT.view, IL_SRO)
               .update(dev);
        }

        // ── Framebuffers ──────────────────────────────────────────────────────
        auto fbTrans  = makeFB(dev, *rpSingle, *txT.view,  TRANSMITTANCE_W, TRANSMITTANCE_H);
        auto fbDeltaE = makeFB(dev, *rpSingle, *txDE.view, SKY_W, SKY_H);
        auto fbIrrad  = makeFB(dev, *rpSingle, *txE.view,  SKY_W, SKY_H);

        std::vector<vk::UniqueFramebuffer> fbInsc1(INS_D);   // dual: deltaSR+deltaSM
        std::vector<vk::UniqueFramebuffer> fbS(INS_D);       // inscatterTexture S
        std::vector<vk::UniqueFramebuffer> fbJ(INS_D);       // deltaJ
        std::vector<vk::UniqueFramebuffer> fbSR(INS_D);      // deltaSR (inscatterN writes)

        for (uint32_t z = 0; z < INS_D; ++z) {
            fbInsc1[z] = makeDualFB(dev, *rpDual,
                *txSR.layerViews[z], *txSM.layerViews[z], INS_W, INS_H);
            fbS[z]  = makeFB(dev, *rpSingle, *txS.layerViews[z],  INS_W, INS_H);
            fbJ[z]  = makeFB(dev, *rpSingle, *txJ.layerViews[z],  INS_W, INS_H);
            fbSR[z] = makeFB(dev, *rpSingle, *txSR.layerViews[z], INS_W, INS_H);
        }

        // ── Precomputation ────────────────────────────────────────────────────
        fprintf(stderr, "Precomputing atmosphere tables...\n");

        using IL = vk::ImageLayout;

        vku::executeImmediately(dev, cmdPool, gfxQ, [&](vk::CommandBuffer cb) {

            // Transition all precomp render targets to eColorAttachmentOptimal.
            for (vk::Image img : {*txT.image, *txE.image, *txDE.image})
                imgTransition(cb, img, IL::eUndefined, IL::eColorAttachmentOptimal);
            for (vk::Image img : {*txS.image, *txSR.image, *txSM.image, *txJ.image})
                imgTransition(cb, img, IL::eUndefined, IL::eColorAttachmentOptimal);

            // Dummies: transition to ShaderReadOnly (never written to).
            imgTransition(cb, *dummy2D.image, IL::eUndefined, IL::eShaderReadOnlyOptimal);
            imgTransition(cb, *dummy3D.image, IL::eUndefined, IL::eShaderReadOnlyOptimal);

            auto setVP = [&](uint32_t w, uint32_t h) {
                cb.setViewport(0, vk::Viewport{0, 0, float(w), float(h), 0, 1});
                cb.setScissor(0, vk::Rect2D{{0, 0}, {w, h}});
            };
            auto beginRP = [&](vk::RenderPass rp, vk::Framebuffer fb,
                               uint32_t w, uint32_t h) {
                cb.beginRenderPass(
                    vk::RenderPassBeginInfo{rp, fb, {{0,0},{w,h}}, 0, nullptr},
                    vk::SubpassContents::eInline);
            };

            PC pushC{};

            // ── Step 1: transmittance T ───────────────────────────────────────
            cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pTrans);
            cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                *precompPL, 0, dsets[0], {});
            beginRP(*rpSingle, *fbTrans, TRANSMITTANCE_W, TRANSMITTANCE_H);
            setVP(TRANSMITTANCE_W, TRANSMITTANCE_H);
            cb.draw(3, 1, 0, 0);
            cb.endRenderPass();
            imgTransition(cb, *txT.image, IL::eColorAttachmentOptimal, IL::eShaderReadOnlyOptimal);

            // ── Step 2: deltaE (irradiance1) ──────────────────────────────────
            cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pIrrad1);
            cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                *precompPL, 0, dsets[1], {});
            beginRP(*rpSingle, *fbDeltaE, SKY_W, SKY_H);
            setVP(SKY_W, SKY_H);
            cb.draw(3, 1, 0, 0);
            cb.endRenderPass();
            imgTransition(cb, *txDE.image, IL::eColorAttachmentOptimal, IL::eShaderReadOnlyOptimal);

            // ── Step 3: inscatter1 → deltaSR + deltaSM (32 layers, dual) ─────
            cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pInsc1);
            cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                *precompPL, 0, dsets[2], {});
            setVP(INS_W, INS_H);
            for (int layer = 0; layer < RES_R; ++layer) {
                setLayer(pushC, layer);
                cb.pushConstants(*precompPL, vk::ShaderStageFlagBits::eFragment,
                                 0, sizeof(PC), &pushC);
                beginRP(*rpDual, *fbInsc1[layer], INS_W, INS_H);
                cb.draw(3, 1, 0, 0);
                cb.endRenderPass();
            }
            imgTransition(cb, *txSR.image, IL::eColorAttachmentOptimal, IL::eShaderReadOnlyOptimal);
            imgTransition(cb, *txSM.image, IL::eColorAttachmentOptimal, IL::eShaderReadOnlyOptimal);

            // ── Step 4: copyIrradiance → irradiance E (initial write, k=1) ───
            pushC = {};  pushC.k = 1.0f;
            cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pCopyIrr);
            cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                *precompPL, 0, dsets[3], {});
            cb.pushConstants(*precompPL, vk::ShaderStageFlagBits::eFragment,
                             0, sizeof(PC), &pushC);
            beginRP(*rpSingle, *fbIrrad, SKY_W, SKY_H);
            setVP(SKY_W, SKY_H);
            cb.draw(3, 1, 0, 0);
            cb.endRenderPass();
            // txE left in eColorAttachmentOptimal for additive accumulation.

            // ── Step 5: copyInscatter1 → inscatterTexture S (32 layers) ──────
            cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pCopyInsc1);
            cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                *precompPL, 0, dsets[4], {});
            setVP(INS_W, INS_H);
            for (int layer = 0; layer < RES_R; ++layer) {
                setLayer(pushC, layer);
                cb.pushConstants(*precompPL, vk::ShaderStageFlagBits::eFragment,
                                 0, sizeof(PC), &pushC);
                beginRP(*rpSingle, *fbS[layer], INS_W, INS_H);
                cb.draw(3, 1, 0, 0);
                cb.endRenderPass();
            }
            // txS left in eColorAttachmentOptimal for additive accumulation.

            // ── Loop: scattering orders 2..4 ─────────────────────────────────
            for (int order = 2; order <= 4; ++order) {

                // Step 6: inscatterS → deltaJ (32 layers)
                pushC = {};  pushC.first = (order == 2) ? 1.0f : 0.0f;
                cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pInscS);
                cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                    *precompPL, 0, dsets[5], {});
                setVP(INS_W, INS_H);
                for (int layer = 0; layer < RES_R; ++layer) {
                    setLayer(pushC, layer);
                    cb.pushConstants(*precompPL, vk::ShaderStageFlagBits::eFragment,
                                     0, sizeof(PC), &pushC);
                    beginRP(*rpSingle, *fbJ[layer], INS_W, INS_H);
                    cb.draw(3, 1, 0, 0);
                    cb.endRenderPass();
                }
                imgTransition(cb, *txJ.image, IL::eColorAttachmentOptimal, IL::eShaderReadOnlyOptimal);

                // Step 7: irradianceN → deltaE (overwrites deltaE)
                imgTransition(cb, *txDE.image, IL::eShaderReadOnlyOptimal, IL::eColorAttachmentOptimal);
                pushC = {};  pushC.first = (order == 2) ? 1.0f : 0.0f;
                cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pIrradN);
                cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                    *precompPL, 0, dsets[6], {});
                cb.pushConstants(*precompPL, vk::ShaderStageFlagBits::eFragment,
                                 0, sizeof(PC), &pushC);
                beginRP(*rpSingle, *fbDeltaE, SKY_W, SKY_H);
                setVP(SKY_W, SKY_H);
                cb.draw(3, 1, 0, 0);
                cb.endRenderPass();
                imgTransition(cb, *txDE.image, IL::eColorAttachmentOptimal, IL::eShaderReadOnlyOptimal);

                // Step 8: inscatterN → deltaSR (overwrites deltaSR, 32 layers)
                imgTransition(cb, *txSR.image, IL::eShaderReadOnlyOptimal, IL::eColorAttachmentOptimal);
                pushC = {};  pushC.first = (order == 2) ? 1.0f : 0.0f;
                cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pInscN);
                cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                    *precompPL, 0, dsets[7], {});
                setVP(INS_W, INS_H);
                for (int layer = 0; layer < RES_R; ++layer) {
                    setLayer(pushC, layer);
                    cb.pushConstants(*precompPL, vk::ShaderStageFlagBits::eFragment,
                                     0, sizeof(PC), &pushC);
                    beginRP(*rpSingle, *fbSR[layer], INS_W, INS_H);
                    cb.draw(3, 1, 0, 0);
                    cb.endRenderPass();
                }
                imgTransition(cb, *txSR.image, IL::eColorAttachmentOptimal, IL::eShaderReadOnlyOptimal);

                // Step 9: copyIrradiance (additive) → irradiance E
                pushC = {};  pushC.k = 1.0f;
                cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pCopyIrrAdd);
                cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                    *precompPL, 0, dsets[3], {});
                cb.pushConstants(*precompPL, vk::ShaderStageFlagBits::eFragment,
                                 0, sizeof(PC), &pushC);
                beginRP(*rpSingleL, *fbIrrad, SKY_W, SKY_H);
                setVP(SKY_W, SKY_H);
                cb.draw(3, 1, 0, 0);
                cb.endRenderPass();

                // Step 10: copyInscatterN (additive) → inscatterTexture S (32 layers)
                cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pCopyInscN);
                cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                    *precompPL, 0, dsets[8], {});
                setVP(INS_W, INS_H);
                for (int layer = 0; layer < RES_R; ++layer) {
                    setLayer(pushC, layer);
                    cb.pushConstants(*precompPL, vk::ShaderStageFlagBits::eFragment,
                                     0, sizeof(PC), &pushC);
                    beginRP(*rpSingleL, *fbS[layer], INS_W, INS_H);
                    cb.draw(3, 1, 0, 0);
                    cb.endRenderPass();
                }

                // Transition deltaJ back to CAO for next loop iteration.
                imgTransition(cb, *txJ.image, IL::eShaderReadOnlyOptimal, IL::eColorAttachmentOptimal);
            } // end scattering loop

            // Final transitions: irradiance E and inscatter S to ShaderReadOnly.
            imgTransition(cb, *txE.image, IL::eColorAttachmentOptimal, IL::eShaderReadOnlyOptimal);
            imgTransition(cb, *txS.image, IL::eColorAttachmentOptimal, IL::eShaderReadOnlyOptimal);
            // txSR is already ShaderReadOnly after last step 8.
        });

        fprintf(stderr, "Precomputation done.\n");

        // ── Camera state ──────────────────────────────────────────────────────
        double lon = 0.0, lat = 0.0;
        double theta = 0.0, phi = 0.0;
        double d = Rg;
        glm::dvec3 sun(0.0, -1.0, 0.0);
        float exposure = 0.4f;
        int oldx = 0, oldy = 0, movement = -1;

        DrawUBO uboData{};

        auto updateView = [&](uint32_t w, uint32_t h) {
            double co = cos(lon), so = sin(lon);
            double ca = cos(lat), sa = sin(lat);
            glm::dvec3 po = glm::dvec3(co*ca, so*ca, sa) * double(Rg);
            glm::dvec3 px(-so, co, 0.0);
            glm::dvec3 py(-co*sa, -so*sa, ca);
            glm::dvec3 pz(co*ca, so*ca, sa);

            double ct = cos(theta), st = sin(theta);
            double cp = cos(phi),   sp = sin(phi);
            glm::dvec3 cx = px*cp + py*sp;
            glm::dvec3 cy = -px*sp*ct + py*cp*ct + pz*st;
            glm::dvec3 cz =  px*sp*st - py*cp*st + pz*ct;
            glm::dvec3 pos = po + cz * d;
            if (glm::length(pos) < Rg + 0.01)
                pos = glm::normalize(pos) * (Rg + 0.01);

            double hh = glm::length(pos) - Rg;
            float vfov = 2.0f * float(std::atan(
                float(h) / float(w) * std::tan(glm::radians(80.0f) / 2.0f)));
            glm::mat4 proj = glm::perspective(vfov, float(w)/float(h),
                                              float(0.1*hh), float(1e5*hh));
            proj[1][1] *= -1.0f;  // Vulkan y-flip

            // View matrix (rows = cx, cy, cz; translated by -pos)
            glm::dmat4 view(
                glm::dvec4(cx.x, cy.x, cz.x, 0.0),
                glm::dvec4(cx.y, cy.y, cz.y, 0.0),
                glm::dvec4(cx.z, cy.z, cz.z, 0.0),
                glm::dvec4(-glm::dot(cx,pos), -glm::dot(cy,pos), -glm::dot(cz,pos), 1.0));

            uboData.c           = glm::vec3(pos);
            uboData.s           = glm::vec3(sun);
            uboData.exposure    = exposure;
            uboData.projInverse = glm::inverse(proj);
            uboData.viewInverse = glm::mat4(glm::inverse(view));
        };

        uint32_t curW = WIN_W, curH = WIN_H;
        updateView(curW, curH);

        // ── Main loop ─────────────────────────────────────────────────────────
        bool running = true, minimized = false;

        while (running) {
            SDL_Event ev;
            while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_QUIT) running = false;
                if (ev.type == SDL_KEYDOWN) {
                    switch (ev.key.keysym.sym) {
                    case SDLK_ESCAPE:   running = false; break;
                    case SDLK_PAGEUP:   d *= 1.05; updateView(curW, curH); break;
                    case SDLK_PAGEDOWN: d /= 1.05; updateView(curW, curH); break;
                    case SDLK_PLUS: case SDLK_EQUALS:
                        exposure *= 1.1f; updateView(curW, curH); break;
                    case SDLK_MINUS:
                        exposure /= 1.1f; updateView(curW, curH); break;
                    }
                }
                if (ev.type == SDL_MOUSEBUTTONDOWN) {
                    oldx = ev.button.x;  oldy = ev.button.y;
                    SDL_Keymod mod = SDL_GetModState();
                    if      (mod & KMOD_CTRL)  movement = 0;
                    else if (mod & KMOD_SHIFT) movement = 1;
                    else                        movement = 2;
                }
                if (ev.type == SDL_MOUSEBUTTONUP) movement = -1;
                if (ev.type == SDL_MOUSEMOTION && movement >= 0) {
                    int x = ev.motion.x, y = ev.motion.y;
                    if (movement == 0) {                   // ctrl: rotate camera
                        phi   += (oldx - x) / 500.0;
                        theta += (oldy - y) / 500.0;
                        theta  = std::max(0.0, std::min(M_PI, theta));
                    } else if (movement == 1) {            // shift: pan surface
                        double factor = (glm::length(glm::dvec3(uboData.c)) - Rg) / Rg;
                        lon += (oldx - x) / 400.0 * factor;
                        lat -= (oldy - y) / 400.0 * factor;
                        lat  = std::max(-M_PI/2.0, std::min(M_PI/2.0, lat));
                    } else {                               // plain: rotate sun
                        float va = std::asin(sun.z);
                        float ha = std::atan2(sun.y, sun.x);
                        va += (oldy - y) / 180.0f * float(M_PI) / 4.0f;
                        ha += (oldx - x) / 180.0f * float(M_PI) / 4.0f;
                        sun.x = std::cos(va) * std::cos(ha);
                        sun.y = std::cos(va) * std::sin(ha);
                        sun.z = std::sin(va);
                    }
                    oldx = x;  oldy = y;
                    updateView(curW, curH);
                }
                if (ev.type == SDL_WINDOWEVENT) {
                    if (ev.window.event == SDL_WINDOWEVENT_MINIMIZED) { minimized = true; }
                    if (ev.window.event == SDL_WINDOWEVENT_RESTORED)  { minimized = false; }
                    if (ev.window.event == SDL_WINDOWEVENT_RESIZED) {
                        curW = uint32_t(ev.window.data1);
                        curH = uint32_t(ev.window.data2);
                        updateView(curW, curH);
                    }
                }
            }

            if (minimized) { SDL_Delay(16); continue; }

            window.draw(dev, gfxQ,
                [&](vk::CommandBuffer cb, int /*imageIndex*/,
                    vk::RenderPassBeginInfo &rpbi)
                {
                    cb.begin(vk::CommandBufferBeginInfo{});

                    // Update UBO.
                    ubo.barrier(cb,
                        vk::PipelineStageFlagBits::eFragmentShader,
                        vk::PipelineStageFlagBits::eTransfer,
                        {}, vk::AccessFlagBits::eUniformRead,
                        vk::AccessFlagBits::eTransferWrite,
                        fw.graphicsQueueFamilyIndex(), fw.graphicsQueueFamilyIndex());
                    cb.updateBuffer(ubo.buffer(), 0, sizeof(DrawUBO), &uboData);
                    ubo.barrier(cb,
                        vk::PipelineStageFlagBits::eTransfer,
                        vk::PipelineStageFlagBits::eVertexShader |
                        vk::PipelineStageFlagBits::eFragmentShader,
                        {}, vk::AccessFlagBits::eTransferWrite,
                        vk::AccessFlagBits::eUniformRead,
                        fw.graphicsQueueFamilyIndex(), fw.graphicsQueueFamilyIndex());

                    cb.beginRenderPass(rpbi, vk::SubpassContents::eInline);

                    auto [w, h] = std::pair{window.width(), window.height()};
                    cb.setViewport(0, vk::Viewport{0,0,float(w),float(h),0,1});
                    cb.setScissor(0, vk::Rect2D{{0,0},{w,h}});

                    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pDraw);
                    cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                        *drawPL, 0, dsets[9], {});
                    cb.draw(3, 1, 0, 0);

                    cb.endRenderPass();
                    cb.end();
                });
        }

        dev.waitIdle();
    }

    SDL_DestroyWindow(sdlWin);
    SDL_Quit();
    return 0;
}
