#define VKU_GLFW
#include <vku/vku_framework.hpp>
#include <vku/vku.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <algorithm>

// Compute-shader port of fdtd2dUpml.
// a compute-shader port of fdtd2dUpml using storage images 
// and a resizable fullscreen triangle display.
//
// Key differences from the render-pass version:
//  - H+B and E+D updates are compute dispatches; no FDTD render passes or framebuffers.
//  - Simulation images are eStorageImage (TextureImage2D has eStorage usage).
//  - Display is a fullscreen triangle with dynamic viewport/scissor — resizes freely.
//
// Ping-pong per frame (frame%2 == 0 shown; frame%2 == 1 swaps Ping/Pong):
//   pass0: reads ch0-3 Ping  →  writes ch0Pong (H'), ch2Pong (B')
//   pass1: reads ch0Pong, ch1Ping, ch2Pong, ch3Ping  →  writes ch1Pong (E'), ch3Pong (D')
//   display: samples ch1Pong (E')

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    const char *title = "fdtd2dUpmlCompute";
    auto glfwwindow = glfwCreateWindow(1024, 1024, title, nullptr, nullptr);

    {
    vku::Framework fw{title};
    if (!fw.ok()) { std::cout << "Framework creation failed\n"; exit(1); }
    fw.dumpCaps(std::cout);

    vk::Device device = fw.device();

    vku::Window window(
        fw.instance(), device, fw.physicalDevice(),
        fw.graphicsQueueFamilyIndex(), glfwwindow,
        { .desiredPresentMode = vk::PresentModeKHR::eImmediate, .tripleBuffering = true }
    );
    if (!window.ok()) { std::cout << "Window creation failed\n"; exit(1); }
    window.dumpCaps(std::cout, fw.physicalDevice());

    ////////////////////////////////////////////////////////////////////////
    // Simulation storage images — 8 total (4 channels × ping/pong).
    // TextureImage2D has eStorage|eSampled usage, which is exactly what we need.

    static constexpr uint32_t N = 1024;
    std::vector<uint8_t> zeros(N * N * 4 * sizeof(float), 0);

    vku::TextureImage2D ch0Ping{device, fw.memprops(), N, N, 1, vk::Format::eR32G32B32A32Sfloat};
    vku::TextureImage2D ch0Pong{device, fw.memprops(), N, N, 1, vk::Format::eR32G32B32A32Sfloat};
    vku::TextureImage2D ch1Ping{device, fw.memprops(), N, N, 1, vk::Format::eR32G32B32A32Sfloat};
    vku::TextureImage2D ch1Pong{device, fw.memprops(), N, N, 1, vk::Format::eR32G32B32A32Sfloat};
    vku::TextureImage2D ch2Ping{device, fw.memprops(), N, N, 1, vk::Format::eR32G32B32A32Sfloat};
    vku::TextureImage2D ch2Pong{device, fw.memprops(), N, N, 1, vk::Format::eR32G32B32A32Sfloat};
    vku::TextureImage2D ch3Ping{device, fw.memprops(), N, N, 1, vk::Format::eR32G32B32A32Sfloat};
    vku::TextureImage2D ch3Pong{device, fw.memprops(), N, N, 1, vk::Format::eR32G32B32A32Sfloat};

    // Upload zeroes and transition to eGeneral (required for eStorageImage).
    // eGeneral is also valid for combined-image-sampler reads in the display pass.
    for (auto* img : { &ch0Ping, &ch0Pong, &ch1Ping, &ch1Pong,
                       &ch2Ping, &ch2Pong, &ch3Ping, &ch3Pong }) {
        img->upload(device, zeros, window.commandPool(), fw.memprops(),
                    fw.graphicsQueue(), vk::ImageLayout::eGeneral);
    }

    ////////////////////////////////////////////////////////////////////////
    // UBO — iFrame for the time-varying source term.

    struct ComputeUniform { int iFrame; };
    vku::UniformBuffer ubo(device, fw.memprops(), sizeof(ComputeUniform));

    ////////////////////////////////////////////////////////////////////////
    // Descriptor set layouts.
    //
    // Compute: bindings 0-3 = eStorageImage inputs, 4-5 = eStorageImage outputs, 6 = UBO.
    // Display: binding 0   = eCombinedImageSampler.

    auto computeDSL = vku::DescriptorSetLayoutMaker{}
        .image(0, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute, 1)
        .image(1, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute, 1)
        .image(2, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute, 1)
        .image(3, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute, 1)
        .image(4, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute, 1)
        .image(5, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute, 1)
        .buffer(6, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eCompute, 1)
        .createUnique(device);

    auto displayDSL = vku::DescriptorSetLayoutMaker{}
        .image(0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1)
        .createUnique(device);

    auto computePL = vku::PipelineLayoutMaker{}.descriptorSetLayout(*computeDSL).createUnique(device);
    auto displayPL = vku::PipelineLayoutMaker{}.descriptorSetLayout(*displayDSL).createUnique(device);

    ////////////////////////////////////////////////////////////////////////
    // Own descriptor pool — the framework pool has no eStorageImage entries.

    // Exact counts: 4 compute sets × 6 storage images, 2 display sets × 1 sampler, 4 UBOs.
    std::array<vk::DescriptorPoolSize, 3> poolSizes{{
        {vk::DescriptorType::eStorageImage,         24},
        {vk::DescriptorType::eCombinedImageSampler,  2},
        {vk::DescriptorType::eUniformBuffer,         4}
    }};
    auto descPool = device.createDescriptorPoolUnique(
        vk::DescriptorPoolCreateInfo{
            vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            6,  // 4 compute + 2 display
            (uint32_t)poolSizes.size(), poolSizes.data()
        }
    );

    ////////////////////////////////////////////////////////////////////////
    // Descriptor sets — 4 compute (2 per pass) + 2 display.

    auto computeSets0 = vku::DescriptorSetMaker{}.layout(*computeDSL).layout(*computeDSL).create(device, *descPool);
    auto computeSets1 = vku::DescriptorSetMaker{}.layout(*computeDSL).layout(*computeDSL).create(device, *descPool);
    auto displaySets  = vku::DescriptorSetMaker{}.layout(*displayDSL).layout(*displayDSL).create(device, *descPool);

    vku::SamplerMaker sm{};
    auto linearSampler = sm
        .magFilter(vk::Filter::eLinear).minFilter(vk::Filter::eLinear)
        .mipmapMode(vk::SamplerMipmapMode::eNearest)
        .addressModeU(vk::SamplerAddressMode::eClampToEdge)
        .addressModeV(vk::SamplerAddressMode::eClampToEdge)
        .createUnique(device);

    // Helper: update one compute descriptor set.
    auto updateComputeSet = [&](vk::DescriptorSet ds,
                                vk::ImageView in0, vk::ImageView in1,
                                vk::ImageView in2, vk::ImageView in3,
                                vk::ImageView out0, vk::ImageView out1) {
        vku::DescriptorSetUpdater{}
            .beginDescriptorSet(ds)
            .beginImages(0, 0, vk::DescriptorType::eStorageImage)
            .image(vk::Sampler{}, in0, vk::ImageLayout::eGeneral)
            .beginImages(1, 0, vk::DescriptorType::eStorageImage)
            .image(vk::Sampler{}, in1, vk::ImageLayout::eGeneral)
            .beginImages(2, 0, vk::DescriptorType::eStorageImage)
            .image(vk::Sampler{}, in2, vk::ImageLayout::eGeneral)
            .beginImages(3, 0, vk::DescriptorType::eStorageImage)
            .image(vk::Sampler{}, in3, vk::ImageLayout::eGeneral)
            .beginImages(4, 0, vk::DescriptorType::eStorageImage)
            .image(vk::Sampler{}, out0, vk::ImageLayout::eGeneral)
            .beginImages(5, 0, vk::DescriptorType::eStorageImage)
            .image(vk::Sampler{}, out1, vk::ImageLayout::eGeneral)
            .beginBuffers(6, 0, vk::DescriptorType::eUniformBuffer)
            .buffer(ubo.buffer(), 0, sizeof(ComputeUniform))
            .update(device);
    };

    //                          in0      in1      in2      in3      out0      out1
    // pass0 frame%2==0: reads Ping set, writes H'=ch0Pong, B'=ch2Pong
    updateComputeSet(computeSets0[0],
        ch0Ping.imageView(), ch1Ping.imageView(), ch2Ping.imageView(), ch3Ping.imageView(),
        ch0Pong.imageView(), ch2Pong.imageView());
    // pass0 frame%2==1: reads Pong set, writes H'=ch0Ping, B'=ch2Ping
    updateComputeSet(computeSets0[1],
        ch0Pong.imageView(), ch1Pong.imageView(), ch2Pong.imageView(), ch3Pong.imageView(),
        ch0Ping.imageView(), ch2Ping.imageView());

    // pass1 frame%2==0: H' from ch0Pong, E=ch1Ping, B'=ch2Pong, D=ch3Ping → E'=ch1Pong, D'=ch3Pong
    updateComputeSet(computeSets1[0],
        ch0Pong.imageView(), ch1Ping.imageView(), ch2Pong.imageView(), ch3Ping.imageView(),
        ch1Pong.imageView(), ch3Pong.imageView());
    // pass1 frame%2==1: H' from ch0Ping, E=ch1Pong, B'=ch2Ping, D=ch3Pong → E'=ch1Ping, D'=ch3Ping
    updateComputeSet(computeSets1[1],
        ch0Ping.imageView(), ch1Pong.imageView(), ch2Ping.imageView(), ch3Pong.imageView(),
        ch1Ping.imageView(), ch3Ping.imageView());

    // display: sample the E' image for the current frame parity
    vku::DescriptorSetUpdater{}
        .beginDescriptorSet(displaySets[0]) // frame%2==0: E' = ch1Pong
        .beginImages(0, 0, vk::DescriptorType::eCombinedImageSampler)
        .image(*linearSampler, ch1Pong.imageView(), vk::ImageLayout::eGeneral)
        .beginDescriptorSet(displaySets[1]) // frame%2==1: E' = ch1Ping
        .beginImages(0, 0, vk::DescriptorType::eCombinedImageSampler)
        .image(*linearSampler, ch1Ping.imageView(), vk::ImageLayout::eGeneral)
        .update(device);

    ////////////////////////////////////////////////////////////////////////
    // Compute pipelines.

    vku::ShaderModule pass0Shader{device, BINARY_DIR "fdtd2dUpmlComputePass0.comp.spv"};
    vku::ShaderModule pass1Shader{device, BINARY_DIR "fdtd2dUpmlComputePass1.comp.spv"};

    auto pass0Pipeline = vku::ComputePipelineMaker{}
        .shader(vk::ShaderStageFlagBits::eCompute, pass0Shader)
        .createUnique(device, fw.pipelineCache(), *computePL);

    auto pass1Pipeline = vku::ComputePipelineMaker{}
        .shader(vk::ShaderStageFlagBits::eCompute, pass1Shader)
        .createUnique(device, fw.pipelineCache(), *computePL);

    ////////////////////////////////////////////////////////////////////////
    // Display pipeline — fullscreen triangle, dynamic viewport/scissor.

    vku::ShaderModule dispVert{device, BINARY_DIR "fdtd2dUpmlComputeDisplay.vert.spv"};
    vku::ShaderModule dispFrag{device, BINARY_DIR "fdtd2dUpmlComputeDisplay.frag.spv"};

    auto displayPipeline = vku::PipelineMaker{window.width(), window.height()}
        .shader(vk::ShaderStageFlagBits::eVertex,   dispVert)
        .shader(vk::ShaderStageFlagBits::eFragment, dispFrag)
        .dynamicState(vk::DynamicState::eViewport)
        .dynamicState(vk::DynamicState::eScissor)
        .depthTestEnable(VK_FALSE)
        .createUnique(device, fw.pipelineCache(), *displayPL, window.renderPass());

    ////////////////////////////////////////////////////////////////////////
    // Reusable image-memory barrier for compute→* ordering.
    auto imgBarrier = [](vk::Image img,
                         vk::AccessFlags srcAccess, vk::AccessFlags dstAccess) {
        vk::ImageMemoryBarrier b{};
        b.srcAccessMask       = srcAccess;
        b.dstAccessMask       = dstAccess;
        b.oldLayout           = vk::ImageLayout::eGeneral;
        b.newLayout           = vk::ImageLayout::eGeneral;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image               = img;
        b.subresourceRange    = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
        return b;
    };

    ////////////////////////////////////////////////////////////////////////
    // Main loop.

    int iFrame = 0;
    while (!glfwWindowShouldClose(glfwwindow) && glfwGetKey(glfwwindow, GLFW_KEY_ESCAPE) != GLFW_PRESS) {
        glfwPollEvents();

        int w, h;
        glfwGetWindowSize(glfwwindow, &w, &h);
        if (w == 0 || h == 0) { continue; }

        const int p = iFrame % 2;

        window.draw(device, fw.graphicsQueue(),
            [&](vk::CommandBuffer cb, int /*imageIndex*/, vk::RenderPassBeginInfo& rpbi) {

                ComputeUniform uniform{ .iFrame = iFrame };
                cb.begin(vk::CommandBufferBeginInfo{});

                // Cross-frame barrier: on a single queue a barrier's src scope covers all
                // previously-submitted work, so this serialises the shared ping-pong images
                // between consecutive in-flight frames (WAR and RAW cross-submission hazards).
                vk::MemoryBarrier crossFrame{
                    vk::AccessFlagBits::eShaderWrite,
                    vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead
                };
                cb.pipelineBarrier(
                    vk::PipelineStageFlagBits::eFragmentShader | vk::PipelineStageFlagBits::eComputeShader,
                    vk::PipelineStageFlagBits::eComputeShader,
                    {}, crossFrame, {}, {});

                // Update UBO
                ubo.barrier(cb,
                    vk::PipelineStageFlagBits::eComputeShader,
                    vk::PipelineStageFlagBits::eTransfer,
                    {}, {}, vk::AccessFlagBits::eTransferWrite,
                    fw.graphicsQueueFamilyIndex(), fw.graphicsQueueFamilyIndex());
                cb.updateBuffer(ubo.buffer(), 0, sizeof(ComputeUniform), &uniform);
                ubo.barrier(cb,
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::PipelineStageFlagBits::eComputeShader,
                    {}, vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eUniformRead,
                    fw.graphicsQueueFamilyIndex(), fw.graphicsQueueFamilyIndex());

                // ---- Pass 0: H + B update ----
                cb.bindPipeline(vk::PipelineBindPoint::eCompute, *pass0Pipeline);
                cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *computePL,
                                      0, computeSets0[p], nullptr);
                cb.dispatch(N/16, N/16, 1);

                // Barrier: pass0 writes → pass1 reads (the two images pass0 wrote to)
                vk::Image ch0Out = (p==0) ? ch0Pong.image() : ch0Ping.image();
                vk::Image ch2Out = (p==0) ? ch2Pong.image() : ch2Ping.image();
                std::array<vk::ImageMemoryBarrier, 2> b01{{
                    imgBarrier(ch0Out, vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
                    imgBarrier(ch2Out, vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead)
                }};
                cb.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                                   vk::PipelineStageFlagBits::eComputeShader,
                                   {}, 0, nullptr, 0, nullptr,
                                   (uint32_t)b01.size(), b01.data());

                // ---- Pass 1: E + D update ----
                cb.bindPipeline(vk::PipelineBindPoint::eCompute, *pass1Pipeline);
                cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *computePL,
                                      0, computeSets1[p], nullptr);
                cb.dispatch(N/16, N/16, 1);

                // Barrier: pass1 writes E' → fragment shader samples it
                vk::Image ch1Out = (p==0) ? ch1Pong.image() : ch1Ping.image();
                auto b1 = imgBarrier(ch1Out,
                    vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);
                cb.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                                   vk::PipelineStageFlagBits::eFragmentShader,
                                   {}, 0, nullptr, 0, nullptr, 1, &b1);

                // ---- Display pass ----
                cb.beginRenderPass(rpbi, vk::SubpassContents::eInline);

                vk::Viewport viewport{0.f, 0.f,
                    (float)window.width(), (float)window.height(), 0.f, 1.f};
                vk::Rect2D scissor{{0,0},{window.width(),window.height()}};
                cb.setViewport(0, viewport);
                cb.setScissor(0, scissor);

                cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *displayPipeline);
                cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *displayPL,
                                      0, displaySets[p], nullptr);
                cb.draw(3, 1, 0, 0); // fullscreen triangle
                cb.endRenderPass();

                cb.end();
            }
        );

        iFrame++;
    }

    device.waitIdle();
    } // Vulkan objects destroyed before GLFW teardown
    glfwDestroyWindow(glfwwindow);
    glfwTerminate();
    return 0;
}
