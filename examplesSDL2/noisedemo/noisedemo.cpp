// Vulkan port of the webgl-noise demo — modified from source at
// https://github.com/ashima/webgl-noise  (mirror: https://github.com/stegu/webgl-noise)
//
// Renders a unit sphere with animated 3D simplex noise as a procedural texture.
// The vertex position on the unit sphere doubles as the 3D noise coordinate,
// matching the original's `v_texCoord3D = gl_Vertex.xyz`.
//
// Noise: 6-octave fractal sum of snoise(vec3), perturbed by three noise offsets.
// Colormap: orange base + noise offset ("hot" colormap from the original).

#define VKU_SDL2
#include <vku/vku_framework.hpp>
#include <vku/vku.hpp>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <cmath>
#include <vector>

static constexpr int STACKS = 20;
static constexpr int SLICES = 20;

// Generate a UV sphere of radius 1, matching glutSolidSphere(1.0, SLICES, STACKS).
// Returns a flat triangle list; each vertex is a vec3 position on the unit sphere.
static std::vector<glm::vec3> generateSphere() {
    std::vector<glm::vec3> verts;
    verts.reserve(STACKS * SLICES * 6);
    for (int i = 0; i < STACKS; i++) {
        float phi0 = glm::pi<float>() * (-0.5f + (float)i       / STACKS);
        float phi1 = glm::pi<float>() * (-0.5f + (float)(i + 1) / STACKS);
        for (int j = 0; j < SLICES; j++) {
            float th0 = glm::two_pi<float>() * (float)j       / SLICES;
            float th1 = glm::two_pi<float>() * (float)(j + 1) / SLICES;

            glm::vec3 v00{cos(phi0)*cos(th0), cos(phi0)*sin(th0), sin(phi0)};
            glm::vec3 v10{cos(phi1)*cos(th0), cos(phi1)*sin(th0), sin(phi1)};
            glm::vec3 v01{cos(phi0)*cos(th1), cos(phi0)*sin(th1), sin(phi0)};
            glm::vec3 v11{cos(phi1)*cos(th1), cos(phi1)*sin(th1), sin(phi1)};

            verts.push_back(v00); verts.push_back(v10); verts.push_back(v11);
            verts.push_back(v00); verts.push_back(v11); verts.push_back(v01);
        }
    }
    return verts;
}

struct PushConstants {
    glm::mat4 mvp;   // bytes  0–63
    float     time;  // bytes 64–67
};
static_assert(sizeof(PushConstants) <= 128, "exceeds minimum push constant size");

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_Window *sdlWindow = SDL_CreateWindow(
        "GLSL Noise Demo: Press M to cycle modes",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        512, 512,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );
    if (!sdlWindow) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    {
        vku::Framework fw{"noisedemo"};
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
        // Sphere geometry.

        auto sphereVerts = generateSphere();
        vku::HostVertexBuffer vbo{device, fw.memprops(), sphereVerts};
        uint32_t vertexCount = (uint32_t)sphereVerts.size();

        ////////////////////////////////////////////////////////////////////////
        // Pipeline layout — push constants only.

        auto pipelineLayout = vku::PipelineLayoutMaker{}
            .pushConstantRange(
                vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
                0, sizeof(PushConstants))
            .createUnique(device);

        ////////////////////////////////////////////////////////////////////////
        // Pipeline — depth test on, matching the original's GL_DEPTH_TEST.

        vku::ShaderModule vert    {device, BINARY_DIR "noisedemo.vert.spv"};
        vku::ShaderModule frag_s3d{device, BINARY_DIR "noisedemo.frag.spv"};
        vku::ShaderModule frag_s2d{device, BINARY_DIR "noisedemo_s2d.frag.spv"};
        vku::ShaderModule frag_s4d{device, BINARY_DIR "noisedemo_s4d.frag.spv"};
        vku::ShaderModule frag_c2d{device, BINARY_DIR "noisedemo_c2d.frag.spv"};
        vku::ShaderModule frag_c3d{device, BINARY_DIR "noisedemo_c3d.frag.spv"};
        vku::ShaderModule frag_c4d{device, BINARY_DIR "noisedemo_c4d.frag.spv"};

        auto buildPipeline = [&](vku::ShaderModule& fragShader) {
            return vku::PipelineMaker{window.width(), window.height()}
                .shader(vk::ShaderStageFlagBits::eVertex,   vert)
                .shader(vk::ShaderStageFlagBits::eFragment, fragShader)
                .vertexBinding(0, sizeof(glm::vec3))
                .vertexAttribute(0, 0, vk::Format::eR32G32B32Sfloat, 0)
                .depthTestEnable(VK_TRUE)
                .depthWriteEnable(VK_TRUE)
                .dynamicState(vk::DynamicState::eViewport)
                .dynamicState(vk::DynamicState::eScissor)
                .createUnique(device, fw.pipelineCache(), *pipelineLayout, window.renderPass());
        };

        // Mode order matches modeNames[] below: 0=s2d 1=s3d 2=s4d 3=c2d 4=c3d 5=c4d
        static const char* modeNames[] = {
            "simplex 2D", "simplex 3D (hot)", "simplex 4D",
            "classic 2D", "classic 3D",       "classic 4D",
        };
        vk::UniquePipeline pipelines[6];
        pipelines[0] = buildPipeline(frag_s2d);
        pipelines[1] = buildPipeline(frag_s3d);
        pipelines[2] = buildPipeline(frag_s4d);
        pipelines[3] = buildPipeline(frag_c2d);
        pipelines[4] = buildPipeline(frag_c3d);
        pipelines[5] = buildPipeline(frag_c4d);

        int modeIdx = 1; // start on the original hot-coloured simplex 3D demo
        std::cout << "Noise mode: " << modeNames[modeIdx] << "  (press M = next mode)\n";

        ////////////////////////////////////////////////////////////////////////
        // Camera — matches gluLookAt(0,-3,0, 0,0,0, 0,0,1) + gluPerspective(45).
        // proj[1][1] *= -1 flips Vulkan's inverted Y clip axis.

        auto buildMVP = [&]() {
            float aspect = (float)window.width() / (float)window.height();
            glm::mat4 view = glm::lookAt(
                glm::vec3(0.0f, -3.0f, 0.0f),
                glm::vec3(0.0f,  0.0f, 0.0f),
                glm::vec3(0.0f,  0.0f, 1.0f));
            glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 1.0f, 100.0f);
            proj[1][1] *= -1.0f;
            return proj * view;
        };

        ////////////////////////////////////////////////////////////////////////
        // Main loop.

        auto startTime = std::chrono::high_resolution_clock::now();
        glm::mat4 mvp  = buildMVP();
        uint32_t cachedW = window.width(), cachedH = window.height();

        bool running   = true;
        bool minimized = false;
        while (running) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) { running = false; }
                if (event.type == SDL_KEYDOWN &&
                    event.key.keysym.sym == SDLK_ESCAPE) { running = false; }
                if (event.type == SDL_KEYDOWN &&
                    event.key.keysym.sym == SDLK_m) {
                    modeIdx = (modeIdx + 1) % 6;
                    std::cout << "Noise mode: " << modeNames[modeIdx] << "\n";
                }
                if (event.type == SDL_WINDOWEVENT) {
                    if (event.window.event == SDL_WINDOWEVENT_MINIMIZED) minimized = true;
                    if (event.window.event == SDL_WINDOWEVENT_RESTORED)  minimized = false;
                }
            }
            if (minimized) { SDL_Delay(16); continue; }

            window.draw(device, fw.graphicsQueue(),
                [&](vk::CommandBuffer cb, int /*imageIndex*/, vk::RenderPassBeginInfo& rpbi) {

                    if (window.width() != cachedW || window.height() != cachedH) {
                        cachedW = window.width();
                        cachedH = window.height();
                        mvp = buildMVP();
                    }

                    float t = std::chrono::duration<float>(
                        std::chrono::high_resolution_clock::now() - startTime).count();

                    PushConstants pc{ mvp, t };

                    cb.begin(vk::CommandBufferBeginInfo{});
                    cb.beginRenderPass(rpbi, vk::SubpassContents::eInline);
                    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipelines[modeIdx]);
                    cb.pushConstants(*pipelineLayout,
                        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
                        0, sizeof(PushConstants), &pc);
                    cb.bindVertexBuffers(0, vbo.buffer(), vk::DeviceSize(0));
                    vku::setViewportScissor(cb, window.width(), window.height());
                    cb.draw(vertexCount, 1, 0, 0);
                    cb.endRenderPass();
                    cb.end();
                });
        }

        device.waitIdle();
    }

    SDL_DestroyWindow(sdlWindow);
    SDL_Quit();
    return 0;
}
