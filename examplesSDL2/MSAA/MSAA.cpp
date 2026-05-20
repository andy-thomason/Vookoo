// Demonstrates 4× MSAA using a render pass with an explicit resolve attachment.
//
// Press M to toggle MSAA on/off at runtime to compare triangle edge quality.
//
// MSAA render pass — two colour attachments, no depth:
//   att 0 : 4× MSAA colour (transient — storeOp=eDontCare, never read back)
//   att 1 : swapchain image (1× resolve target → present)
//
// Non-MSAA render pass — one colour attachment:
//   att 0 : swapchain image (1× sample, written directly)
//
// Both render passes and pipelines are built once at startup; toggling just
// switches which set is active for the current frame.

#define VKU_SDL2
#include <vku/vku_framework.hpp>
#include <vku/vku.hpp>
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

static constexpr vk::SampleCountFlagBits SAMPLESx4 = vk::SampleCountFlagBits::e4;
static constexpr vk::SampleCountFlagBits SAMPLESx1 = vk::SampleCountFlagBits::e1;

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_Window *sdlWindow = SDL_CreateWindow(
        "MSAA press M to toggle",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );
    if (!sdlWindow) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    {
        vku::Framework fw{"MSAA"};
        if (!fw.ok()) { std::cerr << "Framework creation failed\n"; return 1; }

        vk::Device device = fw.device();

        vku::Window window(
            fw.instance(), device, fw.physicalDevice(),
            fw.graphicsQueueFamilyIndex(),
            sdlWindow,
            { .desiredPresentMode = vk::PresentModeKHR::eFifo }
        );
        if (!window.ok()) { std::cerr << "Window creation failed\n"; return 1; }

        ////////////////////////////////////////////////////////////////////////
        // Vertex data — RGB triangle in NDC.

        struct Vertex { glm::vec2 pos; glm::vec3 color; };
        const std::vector<Vertex> vertices = {
            {{ 0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
            {{ 0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},
            {{-0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
        };
        vku::HostVertexBuffer vbo{device, fw.memprops(), vertices};

        ////////////////////////////////////////////////////////////////////////
        // Two render passes — one MSAA, one non-MSAA.

        auto buildRenderPass = [&](bool msaa) {
            vku::RenderpassMaker rpm;

            // Attachment 0: colour target.
            // MSAA: 4× samples, storeOp=eDontCare (resolved away, never stored).
            // No MSAA: 1× sample, storeOp=eStore (written directly to swapchain).
            rpm.attachmentBegin(window.swapchainImageFormat());
            rpm.attachmentSamples(msaa ? SAMPLESx4 : SAMPLESx1);
            rpm.attachmentLoadOp(vk::AttachmentLoadOp::eClear);
            rpm.attachmentStoreOp(msaa ? vk::AttachmentStoreOp::eDontCare
                                       : vk::AttachmentStoreOp::eStore);
            rpm.attachmentFinalLayout(msaa ? vk::ImageLayout::eColorAttachmentOptimal
                                           : vk::ImageLayout::ePresentSrcKHR);

            if (msaa) {
                // Attachment 1: swapchain resolve target, 1× sample.
                // loadOp=eDontCare: the resolve write overwrites every pixel unconditionally.
                rpm.attachmentBegin(window.swapchainImageFormat());
                rpm.attachmentLoadOp(vk::AttachmentLoadOp::eDontCare);
                rpm.attachmentStoreOp(vk::AttachmentStoreOp::eStore);
                rpm.attachmentFinalLayout(vk::ImageLayout::ePresentSrcKHR);
            }

            // Subpass: draw to att 0; when MSAA, resolve to att 1 automatically.
            rpm.subpassBegin(vk::PipelineBindPoint::eGraphics);
            rpm.subpassColorAttachment(vk::ImageLayout::eColorAttachmentOptimal, 0);
            if (msaa) {
                rpm.subpassResolveAttachment(vk::ImageLayout::eColorAttachmentOptimal, 1);
            }

            // EXTERNAL→0: wait for imageAvailableSemaphore before writing colour.
            rpm.dependencyBegin(VK_SUBPASS_EXTERNAL, 0);
            rpm.dependencySrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
            rpm.dependencyDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
            rpm.dependencyDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

            return rpm.createUnique(device);
        };

        auto msaaRenderPass   = buildRenderPass(true);
        auto noMsaaRenderPass = buildRenderPass(false);

        ////////////////////////////////////////////////////////////////////////
        // Two pipelines — one with rasterizationSamples=e4, one with e1.

        vku::ShaderModule vert{device, BINARY_DIR "MSAA.vert.spv"};
        vku::ShaderModule frag{device, BINARY_DIR "MSAA.frag.spv"};
        auto pipelineLayout = vku::PipelineLayoutMaker{}.createUnique(device);

        auto buildPipeline = [&](vk::SampleCountFlagBits samples, vk::RenderPass rp) {
            return vku::PipelineMaker{window.width(), window.height()}
                .shader(vk::ShaderStageFlagBits::eVertex,   vert)
                .shader(vk::ShaderStageFlagBits::eFragment, frag)
                .vertexBinding(0, sizeof(Vertex))
                .vertexAttribute(0, 0, vk::Format::eR32G32Sfloat,    offsetof(Vertex, pos))
                .vertexAttribute(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color))
                .rasterizationSamples(samples)
                .depthTestEnable(VK_FALSE)
                .dynamicState(vk::DynamicState::eViewport)
                .dynamicState(vk::DynamicState::eScissor)
                .createUnique(device, fw.pipelineCache(), *pipelineLayout, rp);
        };

        auto msaaPipeline   = buildPipeline(SAMPLESx4, *msaaRenderPass);
        auto noMsaaPipeline = buildPipeline(SAMPLESx1, *noMsaaRenderPass);

        ////////////////////////////////////////////////////////////////////////
        // MSAA image + two framebuffer sets (rebuilt on resize).
        //
        // MSAA framebuffer:    [msaaImage.imageView(), swapchain[i]]
        // Non-MSAA framebuffer:[swapchain[i]]

        vku::MsaaImage msaaImage;
        std::vector<vk::UniqueFramebuffer> msaaFBs, noMsaaFBs;

        auto buildResources = [&]() {
            msaaImage = vku::MsaaImage{device, fw.memprops(),
                                       window.width(), window.height(),
                                       window.swapchainImageFormat(), SAMPLESx4};
            msaaFBs.clear();
            noMsaaFBs.clear();
            for (auto view : window.imageViews()) {
                {
                    std::array<vk::ImageView, 2> atts{msaaImage.imageView(), view};
                    msaaFBs.push_back(device.createFramebufferUnique(
                        {{}, *msaaRenderPass, (uint32_t)atts.size(), atts.data(),
                         window.width(), window.height(), 1}));
                }
                {
                    std::array<vk::ImageView, 1> atts{view};
                    noMsaaFBs.push_back(device.createFramebufferUnique(
                        {{}, *noMsaaRenderPass, (uint32_t)atts.size(), atts.data(),
                         window.width(), window.height(), 1}));
                }
            }
        };
        buildResources();

        ////////////////////////////////////////////////////////////////////////
        // Main loop.

        bool msaaEnabled = true;
        uint32_t cachedW = window.width(), cachedH = window.height();

        bool running   = true;
        bool minimized = false;
        while (running) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) { running = false; }
                if (event.type == SDL_KEYDOWN) {
                    if (event.key.keysym.sym == SDLK_ESCAPE) { running = false; }
                    if (event.key.keysym.sym == SDLK_m) {
                        msaaEnabled = !msaaEnabled;
                        std::cout << "MSAA " << (msaaEnabled ? "ON" : "OFF") << "\n";
                    }
                }
                if (event.type == SDL_WINDOWEVENT) {
                    if (event.window.event == SDL_WINDOWEVENT_MINIMIZED) minimized = true;
                    if (event.window.event == SDL_WINDOWEVENT_RESTORED)  minimized = false;
                }
            }
            if (minimized) { SDL_Delay(16); continue; }

            window.draw(device, fw.graphicsQueue(),
                [&](vk::CommandBuffer cb, int imageIndex, vk::RenderPassBeginInfo& rpbi) {

                    // Rebuild both framebuffer sets after a swapchain resize.
                    if (window.width() != cachedW || window.height() != cachedH) {
                        cachedW = window.width();
                        cachedH = window.height();
                        device.waitIdle();
                        buildResources();
                    }

                    // Select active render pass, pipeline, and framebuffers.
                    std::array<vk::ClearValue, 2> clearVals{};
                    clearVals[0].color = vk::ClearColorValue{std::array{0.1f, 0.1f, 0.1f, 1.0f}};
                    // clearVals[1]: MSAA resolve has loadOp=eDontCare — value unused but
                    // clearValueCount must cover all attachment indices with eClear.

                    rpbi.renderPass      = msaaEnabled ? *msaaRenderPass   : *noMsaaRenderPass;
                    rpbi.framebuffer     = msaaEnabled ? *msaaFBs[imageIndex] : *noMsaaFBs[imageIndex];
                    rpbi.clearValueCount = msaaEnabled ? 2u : 1u;
                    rpbi.pClearValues    = clearVals.data();

                    vk::Pipeline pipeline = msaaEnabled ? *msaaPipeline : *noMsaaPipeline;

                    cb.begin(vk::CommandBufferBeginInfo{});
                    cb.beginRenderPass(rpbi, vk::SubpassContents::eInline);
                    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
                    cb.bindVertexBuffers(0, vbo.buffer(), vk::DeviceSize(0));
                    vku::setViewportScissor(cb, window.width(), window.height());
                    cb.draw(3, 1, 0, 0);
                    cb.endRenderPass();
                    cb.end();
                });
        }

        device.waitIdle();
    } // all Vulkan objects destroyed before SDL teardown

    SDL_DestroyWindow(sdlWindow);
    SDL_Quit();
    return 0;
}
