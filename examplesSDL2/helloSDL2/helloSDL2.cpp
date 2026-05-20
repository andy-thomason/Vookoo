////////////////////////////////////////////////////////////////////////////////
//
// Vookoo compute particles example — SDL2 variant
//
// Translates the Sascha Willems compute particle sample into the vookoo
// style using vku_framework_sdl2.hpp.
//
//
////////////////////////////////////////////////////////////////////////////////

#define VKU_SDL2
#include <vku/vku_framework.hpp>
#include <vku/vku.hpp>
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <chrono>
#include <cstring>
#include <random>

static constexpr uint32_t WIDTH          = 800;
static constexpr uint32_t HEIGHT         = 600;
static constexpr uint32_t PARTICLE_COUNT = 8192;

struct Particle {
    glm::vec2 position;
    glm::vec2 velocity;
    glm::vec4 color;
};

struct UBO {
    float deltaTime = 1.0f;
};

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_Window *sdlWindow = SDL_CreateWindow(
        "helloSDL2",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIDTH, HEIGHT,
        SDL_WINDOW_VULKAN
    );
    if (!sdlWindow) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    {
        vku::FrameworkOptions fo{ .useCompute = true };
        vku::Framework fw{"helloSDL2", fo};
        if (!fw.ok()) {
            std::cerr << "Framework creation failed\n";
            return 1;
        }

        vk::Device device = fw.device();

        vku::Window window(
            fw.instance(), device, fw.physicalDevice(),
            fw.graphicsQueueFamilyIndex(),
            sdlWindow,
            { .desiredPresentMode = vk::PresentModeKHR::eImmediate }
        );
        if (!window.ok()) {
            std::cerr << "Window creation failed\n";
            return 1;
        }
        window.clearColorValue() = {0.0f, 0.0f, 0.0f, 1.0f};

        ////////////////////////////////////////////////////////////////////////
        //
        // Particles — initialised on a circle, uploaded to two device-local
        // SSBOs that are ping-ponged each frame.

        std::default_random_engine rng{(unsigned)time(nullptr)};
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        std::vector<Particle> particles(PARTICLE_COUNT);
        for (auto &p : particles) {
            float r    = 0.25f * std::sqrt(dist(rng));
            float theta = dist(rng) * 2.0f * 3.14159265f;
            float x    = r * std::cos(theta) * HEIGHT / float(WIDTH);
            float y    = r * std::sin(theta);
            p.position = glm::vec2(x, y);
            p.velocity = glm::normalize(glm::vec2(x, y)) * 0.00050f;
            p.color    = glm::vec4(dist(rng), dist(rng), dist(rng), 1.0f);
        }

        const vk::DeviceSize ssboSize = sizeof(Particle) * PARTICLE_COUNT;

        // Temporary staging buffer for upload.
        vku::GenericBuffer staging(device, fw.memprops(),
            vk::BufferUsageFlagBits::eTransferSrc,
            ssboSize,
            vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent);
        std::memcpy(staging.map(device), particles.data(), (size_t)ssboSize);
        staging.unmap(device);

        // Two device-local SSBOs.
        //   ssbo[0] ←→ ssbo[1] alternate as input / output each frame.
        vku::GenericBuffer ssbo[2];
        for (int i = 0; i < 2; ++i) {
            ssbo[i] = vku::GenericBuffer{device, fw.memprops(),
                vk::BufferUsageFlagBits::eStorageBuffer |
                vk::BufferUsageFlagBits::eVertexBuffer  |
                vk::BufferUsageFlagBits::eTransferDst,
                ssboSize,
                vk::MemoryPropertyFlagBits::eDeviceLocal};
        }

        // Copy initial particle data into both SSBOs.
        vku::executeImmediately(device, window.commandPool(), fw.graphicsQueue(),
            [&](vk::CommandBuffer cb) {
                vk::BufferCopy region{0, 0, ssboSize};
                cb.copyBuffer(staging.buffer(), ssbo[0].buffer(), region);
                cb.copyBuffer(staging.buffer(), ssbo[1].buffer(), region);
            });

        ////////////////////////////////////////////////////////////////////////
        //
        // Uniform buffer — device-local, updated every frame via updateBuffer.

        vku::UniformBuffer ubo(device, fw.memprops(), sizeof(UBO));

        ////////////////////////////////////////////////////////////////////////
        //
        // Compute descriptor set layout:
        //   binding 0 — UBO    (compute)
        //   binding 1 — SSBOIn (compute, read-only)
        //   binding 2 — SSBOOut(compute, read-write)

        auto computeDSL = vku::DescriptorSetLayoutMaker{}
            .buffer(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eCompute, 1)
            .buffer(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute, 1)
            .buffer(2, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute, 1)
            .createUnique(device);

        auto computePL = vku::PipelineLayoutMaker{}
            .descriptorSetLayout(*computeDSL)
            .createUnique(device);

        // Two descriptor sets — one per ping-pong direction:
        //   set[0] (even frames): SSBOIn=ssbo[0], SSBOOut=ssbo[1]
        //   set[1]  (odd frames): SSBOIn=ssbo[1], SSBOOut=ssbo[0]
        auto computeSets = vku::DescriptorSetMaker{}
            .layout(*computeDSL)
            .layout(*computeDSL)
            .create(device, fw.descriptorPool());

        vku::DescriptorSetUpdater{}
            .beginDescriptorSet(computeSets[0])
            .beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer)
            .buffer(ubo.buffer(), 0, sizeof(UBO))
            .beginBuffers(1, 0, vk::DescriptorType::eStorageBuffer)
            .buffer(ssbo[0].buffer(), 0, ssboSize)
            .beginBuffers(2, 0, vk::DescriptorType::eStorageBuffer)
            .buffer(ssbo[1].buffer(), 0, ssboSize)

            .beginDescriptorSet(computeSets[1])
            .beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer)
            .buffer(ubo.buffer(), 0, sizeof(UBO))
            .beginBuffers(1, 0, vk::DescriptorType::eStorageBuffer)
            .buffer(ssbo[1].buffer(), 0, ssboSize)
            .beginBuffers(2, 0, vk::DescriptorType::eStorageBuffer)
            .buffer(ssbo[0].buffer(), 0, ssboSize)

            .update(device);

        ////////////////////////////////////////////////////////////////////////
        //
        // Compute pipeline.

        vku::ShaderModule comp{device, BINARY_DIR "helloSDL2.comp.spv"};

        auto computePipeline = vku::ComputePipelineMaker{}
            .shader(vk::ShaderStageFlagBits::eCompute, comp)
            .createUnique(device, fw.pipelineCache(), *computePL);

        ////////////////////////////////////////////////////////////////////////
        //
        // Graphics pipeline — point list, additive-ish alpha blend, no depth.

        vku::ShaderModule vert{device, BINARY_DIR "helloSDL2.vert.spv"};
        vku::ShaderModule frag{device, BINARY_DIR "helloSDL2.frag.spv"};

        auto graphicsPL = vku::PipelineLayoutMaker{}.createUnique(device);

        auto buildGraphicsPipeline = [&]() {
            vku::PipelineMaker pm{window.width(), window.height()};
            return pm
                .shader(vk::ShaderStageFlagBits::eVertex,   vert)
                .shader(vk::ShaderStageFlagBits::eFragment, frag)
                .vertexBinding(0, sizeof(Particle), vk::VertexInputRate::eVertex)
                .vertexAttribute(0, 0, vk::Format::eR32G32Sfloat,       offsetof(Particle, position))
                .vertexAttribute(1, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(Particle, color))
                .topology(vk::PrimitiveTopology::ePointList)
                .depthWriteEnable(VK_FALSE)
                .cullMode(vk::CullModeFlagBits::eNone)
                .blendBegin(VK_TRUE)
                .blendSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
                .blendDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
                .blendColorBlendOp(vk::BlendOp::eAdd)
                .blendSrcAlphaBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
                .blendDstAlphaBlendFactor(vk::BlendFactor::eZero)
                .blendAlphaBlendOp(vk::BlendOp::eAdd)
                .createUnique(device, fw.pipelineCache(), *graphicsPL, window.renderPass(), false);
        };

        auto graphicsPipeline = buildGraphicsPipeline();

        ////////////////////////////////////////////////////////////////////////
        //
        // Main loop.

        int  iFrame   = 0;
        auto prevTime = std::chrono::high_resolution_clock::now();
        UBO uboCpu{};

        bool running   = true;
        bool minimized = false;
        while (running) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) { running = false; }
                if (event.type == SDL_KEYDOWN &&
                    event.key.keysym.sym == SDLK_ESCAPE) { running = false; }
                if (event.type == SDL_WINDOWEVENT) {
                    if (event.window.event == SDL_WINDOWEVENT_MINIMIZED) { minimized = true;  }
                    if (event.window.event == SDL_WINDOWEVENT_RESTORED)  { minimized = false; }
                }
            }

            if (minimized) { prevTime = std::chrono::high_resolution_clock::now(); SDL_Delay(16); continue; }

            window.draw(device, fw.graphicsQueue(),
                [&](vk::CommandBuffer cb, int /*imageIndex*/, vk::RenderPassBeginInfo &rpbi) {

                    auto now         = std::chrono::high_resolution_clock::now();
                    uboCpu.deltaTime = std::chrono::duration<float, std::milli>(now - prevTime).count();
                    prevTime         = now;

                    cb.begin(vk::CommandBufferBeginInfo{});

                    // --- update UBO ---
                    ubo.barrier(cb,
                        vk::PipelineStageFlagBits::eComputeShader,
                        vk::PipelineStageFlagBits::eTransfer,
                        {}, vk::AccessFlagBits::eUniformRead, vk::AccessFlagBits::eTransferWrite,
                        fw.graphicsQueueFamilyIndex(), fw.graphicsQueueFamilyIndex());
                    cb.updateBuffer(ubo.buffer(), 0, sizeof(UBO), &uboCpu);
                    ubo.barrier(cb,
                        vk::PipelineStageFlagBits::eTransfer,
                        vk::PipelineStageFlagBits::eComputeShader,
                        {}, vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eUniformRead,
                        fw.graphicsQueueFamilyIndex(), fw.graphicsQueueFamilyIndex());

                    // --- compute dispatch ---
                    // set[iFrame%2]: reads ssbo[iFrame%2], writes ssbo[(iFrame+1)%2]
                    int setIdx = iFrame % 2;
                    int outIdx = (iFrame + 1) % 2;

                    cb.bindPipeline(vk::PipelineBindPoint::eCompute, *computePipeline);
                    cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                        *computePL, 0, computeSets[setIdx], nullptr);
                    cb.dispatch(PARTICLE_COUNT / 256, 1, 1);

                    // Barrier: compute shader write → vertex attribute read.
                    ssbo[outIdx].barrier(cb,
                        vk::PipelineStageFlagBits::eComputeShader,
                        vk::PipelineStageFlagBits::eVertexInput,
                        {}, vk::AccessFlagBits::eShaderWrite,
                        vk::AccessFlagBits::eVertexAttributeRead,
                        fw.graphicsQueueFamilyIndex(), fw.graphicsQueueFamilyIndex());

                    // --- graphics pass ---
                    cb.beginRenderPass(rpbi, vk::SubpassContents::eInline);
                    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
                    cb.bindVertexBuffers(0, ssbo[outIdx].buffer(), vk::DeviceSize(0));
                    cb.draw(PARTICLE_COUNT, 1, 0, 0);
                    cb.endRenderPass();

                    cb.end();
                });

            iFrame++;
        }

        device.waitIdle();
    } // all Vulkan objects destroyed before SDL teardown

    SDL_DestroyWindow(sdlWindow);
    SDL_Quit();

    return 0;
}
