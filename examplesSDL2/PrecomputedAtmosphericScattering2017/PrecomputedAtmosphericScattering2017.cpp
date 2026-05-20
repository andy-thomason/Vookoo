// Vulkan/VKU/SDL2 port of the Bruneton 2017 precomputed atmospheric scattering demo.
//
// Original work:
//   Eric Bruneton, "A Scalable and Production Ready Sky and Atmosphere Rendering
//   Technique", Eurographics Symposium on Rendering 2017.
//   https://github.com/ebruneton/precomputed_atmospheric_scattering
//   Copyright (c) 2017 Eric Bruneton. BSD 3-Clause License.
//
//   Eric Bruneton, Fabrice Neyret, "Precomputed Atmospheric Scattering",
//   Eurographics Symposium on Rendering 2008.
//   Copyright (c) 2008 INRIA. BSD 3-Clause License.
//
// This file: Vulkan port using VKU/SDL2. Precomputation replaced by loading
// .dat textures produced by the original demo. AtmosphereParameters passed
// via std140 UBO rather than baked as GLSL constants.
//
// Controls:
//   Mouse drag          — orbit camera (zenith / azimuth)
//   Ctrl  + drag        — move sun
//   Scroll wheel        — change view distance
//   +/-                 — exposure
//   1–9                 — preset views
//   Escape / Q          — quit

#define VKU_SDL2
#include <vku/vku_framework.hpp>
#include <vku/vku.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

// ── Constants ─────────────────────────────────────────────────────────────────

static constexpr uint32_t WIN_W = 1024, WIN_H = 768;

static constexpr float kLengthUnitInMeters = 1000.0f;
static constexpr float kSunAngularRadius   = 0.00935f / 2.0f;
static constexpr float kFovY               = 50.0f * float(M_PI) / 180.0f;

// ── SceneUBO — must match layout in shaders (std140, binding=4) ───────────────

struct SceneUBO {
    glm::mat4 model_from_view;  // offset   0
    glm::mat4 view_from_clip;   // offset  64
    glm::vec4 camera_exposure;  // offset 128  xyz=camera, w=exposure
    glm::vec4 white_point;      // offset 144  xyz, w unused
    glm::vec4 earth_center;     // offset 160  xyz, w unused
    glm::vec4 sun_direction;    // offset 176  xyz, w unused
    glm::vec2 sun_size;         // offset 192  (tan(r), cos(r))
    glm::vec2 _pad;             // offset 200
};
static_assert(sizeof(SceneUBO) == 208);

// ── AtmosphereParameters UBO — std140 layout matching definitions.glsl ────────
//
// std140 rules applied to AtmosphereParameters:
//   vec3  → 12 bytes, aligned to 16 (next member aligns from after the 12 bytes)
//   float → 4 bytes, aligned to 4
//   DensityProfileLayer → 5 floats = 20 bytes, struct base align 16 → padded to 32
//   DensityProfile → 2×DensityProfileLayer = 64 bytes
//
// Total: 304 bytes (verified by static_assert below).

struct DensityProfileLayerGLSL {
    float width;
    float exp_term;
    float exp_scale;
    float linear_term;
    float constant_term;
    float _pad[3];
};
static_assert(sizeof(DensityProfileLayerGLSL) == 32);

struct DensityProfileGLSL {
    DensityProfileLayerGLSL layers[2];
};
static_assert(sizeof(DensityProfileGLSL) == 64);

struct AtmosphereParametersGLSL {
    float solar_irradiance[3];          // offset   0  (vec3, 12 bytes)
    float sun_angular_radius;           // offset  12
    float bottom_radius;                // offset  16
    float top_radius;                   // offset  20
    float _pad0[2];                     // offset  24  → pad to 32
    DensityProfileGLSL rayleigh_density;// offset  32
    float rayleigh_scattering[3];       // offset  96  (vec3, 12 bytes)
    float _pad1[1];                     // offset 108  → pad to 112
    DensityProfileGLSL mie_density;     // offset 112
    float mie_scattering[3];            // offset 176  (vec3, 12 bytes)
    float _pad2[1];                     // offset 188  → pad to 192
    float mie_extinction[3];            // offset 192  (vec3, 12 bytes)
    float mie_phase_function_g;         // offset 204
    // offset 208 is already 16-aligned; no pad needed before absorption_density
    DensityProfileGLSL absorption_density; // offset 208
    float absorption_extinction[3];     // offset 272  (vec3, 12 bytes)
    float _pad3[1];                     // offset 284  → pad to 288
    float ground_albedo[3];             // offset 288  (vec3, 12 bytes)
    float mu_s_min;                     // offset 300
};
static_assert(sizeof(AtmosphereParametersGLSL) == 304);

// Earth atmosphere constants (from Bruneton 2017 demo precomputation).
static AtmosphereParametersGLSL makeEarthAtmosphere() {
    AtmosphereParametersGLSL a{};

    a.solar_irradiance[0] = 1.474000f;
    a.solar_irradiance[1] = 1.850400f;
    a.solar_irradiance[2] = 1.911980f;
    a.sun_angular_radius  = 0.004675f;
    a.bottom_radius       = 6360.0f;
    a.top_radius          = 6420.0f;

    // Rayleigh: layer[0] = zeros, layer[1] = (0, 1, -0.125, 0, 0)
    a.rayleigh_density.layers[1].exp_term  = 1.0f;
    a.rayleigh_density.layers[1].exp_scale = -0.125f;

    a.rayleigh_scattering[0] = 0.005802f;
    a.rayleigh_scattering[1] = 0.013558f;
    a.rayleigh_scattering[2] = 0.033100f;

    // Mie: layer[0] = zeros, layer[1] = (0, 1, -0.833333, 0, 0)
    a.mie_density.layers[1].exp_term  = 1.0f;
    a.mie_density.layers[1].exp_scale = -0.833333f;

    a.mie_scattering[0] = 0.003996f;
    a.mie_scattering[1] = 0.003996f;
    a.mie_scattering[2] = 0.003996f;

    a.mie_extinction[0] = 0.004440f;
    a.mie_extinction[1] = 0.004440f;
    a.mie_extinction[2] = 0.004440f;

    a.mie_phase_function_g = 0.8f;

    // Absorption (ozone): layer[0] = (25, 0, 0, 0.066667, -0.666667)
    //                     layer[1] = (0, 0, 0, -0.066667, 2.666667)
    a.absorption_density.layers[0].width         = 25.0f;
    a.absorption_density.layers[0].linear_term   = 0.066667f;
    a.absorption_density.layers[0].constant_term = -0.666667f;
    a.absorption_density.layers[1].linear_term   = -0.066667f;
    a.absorption_density.layers[1].constant_term = 2.666667f;

    a.absorption_extinction[0] = 0.000650f;
    a.absorption_extinction[1] = 0.001881f;
    a.absorption_extinction[2] = 0.000085f;

    a.ground_albedo[0] = 0.1f;
    a.ground_albedo[1] = 0.1f;
    a.ground_albedo[2] = 0.1f;

    a.mu_s_min = -0.207912f;

    return a;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::vector<uint8_t> readFile(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open: " + path);
    return {std::istreambuf_iterator<char>(f), {}};
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1;
    }
    SDL_Window *sdlWin = SDL_CreateWindow(
        "Precomputed Atmospheric Scattering 2017",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_W, WIN_H, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!sdlWin) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); SDL_Quit(); return 1;
    }

    {
        vku::Framework fw{"PrecomputedAtmosphericScattering2017"};
        if (!fw.ok()) { fprintf(stderr, "Framework creation failed\n"); return 1; }
        vk::Device            dev    = fw.device();
        const auto           &mp     = fw.memprops();
        vk::PipelineCache     pCache = fw.pipelineCache();

        vku::Window window(fw.instance(), dev, fw.physicalDevice(),
                           fw.graphicsQueueFamilyIndex(), sdlWin);
        if (!window.ok()) { fprintf(stderr, "Window creation failed\n"); return 1; }

        vk::CommandPool cmdPool = window.commandPool();
        vk::Queue       gfxQ   = fw.graphicsQueue();

        // ── Load precomputed textures ─────────────────────────────────────────
        const std::string dataDir = BINARY_DIR;

        auto loadTex2D = [&](const std::string &name) {
            auto bytes = readFile(dataDir + name);
            vku::KTXFileLayout ktx(bytes.data(), bytes.data() + bytes.size());
            vku::TextureImage2D tex(dev, mp, ktx.width(0), ktx.height(0), ktx.mipLevels(), ktx.format());
            ktx.upload(dev, tex, bytes, cmdPool, mp, gfxQ);
            return tex;
        };
        auto loadTex3D = [&](const std::string &name) {
            auto bytes = readFile(dataDir + name);
            vku::KTXFileLayout ktx(bytes.data(), bytes.data() + bytes.size());
            vku::TextureImage3D tex(dev, mp, ktx.width(0), ktx.height(0), ktx.depth(0), ktx.mipLevels(), ktx.format());
            ktx.upload(dev, tex, bytes, cmdPool, mp, gfxQ);
            return tex;
        };

        auto txTrans = loadTex2D("transmittance.ktx");
        auto txScat  = loadTex3D("scattering.ktx");
        auto txIrr   = loadTex2D("irradiance.ktx");

        // ── Sampler ───────────────────────────────────────────────────────────
        auto sampler = vku::SamplerMaker{}
            .magFilter(vk::Filter::eLinear).minFilter(vk::Filter::eLinear)
            .mipmapMode(vk::SamplerMipmapMode::eNearest)
            .addressModeU(vk::SamplerAddressMode::eClampToEdge)
            .addressModeV(vk::SamplerAddressMode::eClampToEdge)
            .addressModeW(vk::SamplerAddressMode::eClampToEdge)
            .createUnique(dev);
        vk::Sampler samp = *sampler;

        // ── Load SPIR-V shaders ───────────────────────────────────────────────
        vku::ShaderModule vertMod(dev, BINARY_DIR "scene.vert.spv");
        vku::ShaderModule fragMod(dev, BINARY_DIR "scene.frag.spv");

        // ── Descriptor layout ─────────────────────────────────────────────────
        auto dsl = vku::DescriptorSetLayoutMaker{}
            .image(0, vk::DescriptorType::eCombinedImageSampler,
                   vk::ShaderStageFlagBits::eFragment, 1)
            .image(1, vk::DescriptorType::eCombinedImageSampler,
                   vk::ShaderStageFlagBits::eFragment, 1)
            .image(2, vk::DescriptorType::eCombinedImageSampler,
                   vk::ShaderStageFlagBits::eFragment, 1)
            .image(3, vk::DescriptorType::eCombinedImageSampler,
                   vk::ShaderStageFlagBits::eFragment, 1)
            .buffer(4, vk::DescriptorType::eUniformBuffer,
                    vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 1)
            .buffer(5, vk::DescriptorType::eUniformBuffer,
                    vk::ShaderStageFlagBits::eFragment, 1)
            .createUnique(dev);

        // ── Pipeline layout ───────────────────────────────────────────────────
        auto pl = vku::PipelineLayoutMaker{}
            .descriptorSetLayout(*dsl)
            .createUnique(dev);

        // ── Pipeline ──────────────────────────────────────────────────────────
        auto pipeline = [&]() {
            vku::PipelineMaker pm{WIN_W, WIN_H};
            return pm.shader(vk::ShaderStageFlagBits::eVertex,   vertMod)
                     .shader(vk::ShaderStageFlagBits::eFragment, fragMod)
                     .topology(vk::PrimitiveTopology::eTriangleList)
                     .depthTestEnable(VK_FALSE).depthWriteEnable(VK_FALSE)
                     .cullMode(vk::CullModeFlagBits::eNone)
                     .dynamicState(vk::DynamicState::eViewport)
                     .dynamicState(vk::DynamicState::eScissor)
                     .blendBegin(VK_FALSE)
                     .createUnique(dev, pCache, *pl, window.renderPass(), false);
        }();

        // ── Descriptor pool + set ─────────────────────────────────────────────
        std::array<vk::DescriptorPoolSize, 2> poolSizes{{
            {vk::DescriptorType::eCombinedImageSampler, 4},
            {vk::DescriptorType::eUniformBuffer,         2},
        }};
        auto descPool = dev.createDescriptorPoolUnique({
            vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            1u, uint32_t(poolSizes.size()), poolSizes.data()});

        auto dsets = vku::DescriptorSetMaker{}.layout(*dsl).create(dev, *descPool);

        // ── UBOs ──────────────────────────────────────────────────────────────
        vku::UniformBuffer sceneUbo(dev, mp, sizeof(SceneUBO));
        vku::UniformBuffer atmosUbo(dev, mp, sizeof(AtmosphereParametersGLSL));

        // ── Bind descriptors ──────────────────────────────────────────────────
        auto SRO = vk::ImageLayout::eShaderReadOnlyOptimal;
        vku::DescriptorSetUpdater{}
            .beginDescriptorSet(dsets[0])
            .beginImages(0, 0, vk::DescriptorType::eCombinedImageSampler)
            .image(samp, txTrans.imageView(), SRO)
            .beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler)
            .image(samp, txScat.imageView(), SRO)
            .beginImages(2, 0, vk::DescriptorType::eCombinedImageSampler)
            // COMBINED_SCATTERING_TEXTURES: single_mie is packed in scattering.w
            .image(samp, txScat.imageView(), SRO)
            .beginImages(3, 0, vk::DescriptorType::eCombinedImageSampler)
            .image(samp, txIrr.imageView(), SRO)
            .beginBuffers(4, 0, vk::DescriptorType::eUniformBuffer)
            .buffer(sceneUbo.buffer(), 0, sizeof(SceneUBO))
            .beginBuffers(5, 0, vk::DescriptorType::eUniformBuffer)
            .buffer(atmosUbo.buffer(), 0, sizeof(AtmosphereParametersGLSL))
            .update(dev);

        // ── Camera / view state ───────────────────────────────────────────────
        double viewDistMeters          = 9000.0;
        double viewZenithAngleRadians  = 1.47;
        double viewAzimuthAngleRadians = -0.1;
        double sunZenithAngleRadians   = 1.3;
        double sunAzimuthAngleRadians  = 2.9;
        float  exposure                = 10.0f;

        auto setView = [&](double vDist, double vZen, double vAz,
                           double sZen,  double sAz,  float exp_) {
            viewDistMeters          = vDist;
            viewZenithAngleRadians  = vZen;
            viewAzimuthAngleRadians = vAz;
            sunZenithAngleRadians   = sZen;
            sunAzimuthAngleRadians  = sAz;
            exposure                = exp_;
        };

        SceneUBO sceneData{};
        sceneData.earth_center = glm::vec4(0.0f, 0.0f, -6360.0f, 0.0f);
        sceneData.white_point  = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
        sceneData.sun_size     = glm::vec2(std::tan(kSunAngularRadius),
                                           std::cos(kSunAngularRadius));

        auto updateUBO = [&](uint32_t w, uint32_t h) {
            float aspect = float(w) / float(h);
            float tanFov = std::tan(kFovY / 2.0f);

            // view_from_clip (column-major). Y column negated vs WebGL
            // because Vulkan clip Y is inverted relative to OpenGL.
            sceneData.view_from_clip = glm::mat4(
                glm::vec4(tanFov * aspect, 0.f,      0.f, 0.f),
                glm::vec4(0.f,            -tanFov,   0.f, 0.f),
                glm::vec4(0.f,             0.f,      0.f, 1.f),
                glm::vec4(0.f,             0.f,     -1.f, 1.f));

            float cosZ = float(std::cos(viewZenithAngleRadians));
            float sinZ = float(std::sin(viewZenithAngleRadians));
            float cosA = float(std::cos(viewAzimuthAngleRadians));
            float sinA = float(std::sin(viewAzimuthAngleRadians));
            float d    = float(viewDistMeters / kLengthUnitInMeters);

            // model_from_view (column-major), from WebGL row-major via transpose:
            // JS row0 = [-sinA, -cosZ*cosA, sinZ*cosA, sinZ*cosA*d]
            // JS row1 = [ cosA, -cosZ*sinA, sinZ*sinA, sinZ*sinA*d]
            // JS row2 = [    0,       sinZ,      cosZ,      cosZ*d]
            // JS row3 = [    0,          0,         0,           1]
            sceneData.model_from_view = glm::mat4(
                glm::vec4(-sinA,        cosA,        0.f,    0.f),
                glm::vec4(-cosZ*cosA,  -cosZ*sinA,   sinZ,   0.f),
                glm::vec4( sinZ*cosA,   sinZ*sinA,   cosZ,   0.f),
                glm::vec4( sinZ*cosA*d, sinZ*sinA*d, cosZ*d, 1.f));

            sceneData.camera_exposure = glm::vec4(
                sinZ*cosA*d, sinZ*sinA*d, cosZ*d, exposure);

            float cosSZ = float(std::cos(sunZenithAngleRadians));
            float sinSZ = float(std::sin(sunZenithAngleRadians));
            float cosSA = float(std::cos(sunAzimuthAngleRadians));
            float sinSA = float(std::sin(sunAzimuthAngleRadians));
            sceneData.sun_direction = glm::vec4(
                cosSA * sinSZ, sinSA * sinSZ, cosSZ, 0.f);
        };

        uint32_t curW = WIN_W, curH = WIN_H;
        updateUBO(curW, curH);

        auto atmosData = makeEarthAtmosphere();

        // ── Main loop ─────────────────────────────────────────────────────────
        bool running   = true;
        bool minimized = false;
        bool dragging  = false;
        bool dragSun   = false;
        int  oldX = 0, oldY = 0;

        static constexpr double kDragScale = 500.0;

        while (running) {
            SDL_Event ev;
            while (SDL_PollEvent(&ev)) {
                switch (ev.type) {
                case SDL_QUIT:
                    running = false; break;

                case SDL_KEYDOWN:
                    switch (ev.key.keysym.sym) {
                    case SDLK_ESCAPE: case SDLK_q: running = false; break;
                    case SDLK_PLUS: case SDLK_EQUALS:
                        exposure *= 1.1f; updateUBO(curW, curH); break;
                    case SDLK_MINUS:
                        exposure /= 1.1f; updateUBO(curW, curH); break;
                    case SDLK_1: setView(9000,1.47,0,1.3,   3,     10); updateUBO(curW,curH); break;
                    case SDLK_2: setView(9000,1.47,0,1.564,-3,     10); updateUBO(curW,curH); break;
                    case SDLK_3: setView(7000,1.57,0,1.54, -2.96,  10); updateUBO(curW,curH); break;
                    case SDLK_4: setView(7000,1.57,0,1.328,-3.044, 10); updateUBO(curW,curH); break;
                    case SDLK_5: setView(9000,1.39,0,1.2,   0.7,   10); updateUBO(curW,curH); break;
                    case SDLK_6: setView(9000,1.5, 0,1.628, 1.05, 200); updateUBO(curW,curH); break;
                    case SDLK_7: setView(7000,1.43,0,1.57,  1.34,  40); updateUBO(curW,curH); break;
                    case SDLK_8: setView(2.7e6,0.81,0,1.57, 2,     10); updateUBO(curW,curH); break;
                    case SDLK_9: setView(1.2e7,0.0, 0,0.93,-2,     10); updateUBO(curW,curH); break;
                    }
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    dragging = true;
                    dragSun  = (SDL_GetModState() & KMOD_CTRL);
                    oldX = ev.button.x; oldY = ev.button.y;
                    break;

                case SDL_MOUSEBUTTONUP:
                    dragging = false; break;

                case SDL_MOUSEMOTION:
                    if (dragging) {
                        int dx = ev.motion.x - oldX;
                        int dy = ev.motion.y - oldY;
                        oldX = ev.motion.x; oldY = ev.motion.y;
                        if (dragSun) {
                            sunZenithAngleRadians  -= dy / kDragScale;
                            sunZenithAngleRadians   = std::max(0.0, std::min(M_PI, sunZenithAngleRadians));
                            sunAzimuthAngleRadians += dx / kDragScale;
                        } else {
                            viewZenithAngleRadians  += dy / kDragScale;
                            viewZenithAngleRadians   = std::max(0.0, std::min(M_PI/2.0, viewZenithAngleRadians));
                            viewAzimuthAngleRadians += dx / kDragScale;
                        }
                        updateUBO(curW, curH);
                    }
                    break;

                case SDL_MOUSEWHEEL:
                    viewDistMeters *= ev.wheel.y > 0 ? 1.05 : 1.0/1.05;
                    updateUBO(curW, curH);
                    break;

                case SDL_WINDOWEVENT:
                    if (ev.window.event == SDL_WINDOWEVENT_MINIMIZED) minimized = true;
                    if (ev.window.event == SDL_WINDOWEVENT_RESTORED)  minimized = false;
                    if (ev.window.event == SDL_WINDOWEVENT_RESIZED) {
                        curW = uint32_t(ev.window.data1);
                        curH = uint32_t(ev.window.data2);
                        updateUBO(curW, curH);
                    }
                    break;
                }
            }

            if (minimized) { SDL_Delay(16); continue; }

            window.draw(dev, gfxQ,
                [&](vk::CommandBuffer cb, int, vk::RenderPassBeginInfo &rpbi)
                {
                    cb.begin(vk::CommandBufferBeginInfo{});

                    // Upload scene UBO (changes each frame when camera moves).
                    sceneUbo.barrier(cb,
                        vk::PipelineStageFlagBits::eVertexShader |
                        vk::PipelineStageFlagBits::eFragmentShader,
                        vk::PipelineStageFlagBits::eTransfer,
                        {}, vk::AccessFlagBits::eUniformRead,
                        vk::AccessFlagBits::eTransferWrite,
                        fw.graphicsQueueFamilyIndex(), fw.graphicsQueueFamilyIndex());
                    cb.updateBuffer(sceneUbo.buffer(), 0, sizeof(SceneUBO), &sceneData);
                    sceneUbo.barrier(cb,
                        vk::PipelineStageFlagBits::eTransfer,
                        vk::PipelineStageFlagBits::eVertexShader |
                        vk::PipelineStageFlagBits::eFragmentShader,
                        {}, vk::AccessFlagBits::eTransferWrite,
                        vk::AccessFlagBits::eUniformRead,
                        fw.graphicsQueueFamilyIndex(), fw.graphicsQueueFamilyIndex());

                    // Upload atmosphere UBO (static; same data every frame).
                    atmosUbo.barrier(cb,
                        vk::PipelineStageFlagBits::eFragmentShader,
                        vk::PipelineStageFlagBits::eTransfer,
                        {}, vk::AccessFlagBits::eUniformRead,
                        vk::AccessFlagBits::eTransferWrite,
                        fw.graphicsQueueFamilyIndex(), fw.graphicsQueueFamilyIndex());
                    cb.updateBuffer(atmosUbo.buffer(), 0,
                        sizeof(AtmosphereParametersGLSL), &atmosData);
                    atmosUbo.barrier(cb,
                        vk::PipelineStageFlagBits::eTransfer,
                        vk::PipelineStageFlagBits::eFragmentShader,
                        {}, vk::AccessFlagBits::eTransferWrite,
                        vk::AccessFlagBits::eUniformRead,
                        fw.graphicsQueueFamilyIndex(), fw.graphicsQueueFamilyIndex());

                    cb.beginRenderPass(rpbi, vk::SubpassContents::eInline);
                    auto [w, h] = std::pair{window.width(), window.height()};
                    cb.setViewport(0, vk::Viewport{0,0,float(w),float(h),0,1});
                    cb.setScissor(0, vk::Rect2D{{0,0},{w,h}});
                    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
                    cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                        *pl, 0, dsets[0], {});
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
