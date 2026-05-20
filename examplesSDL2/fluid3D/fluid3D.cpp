////////////////////////////////////////////////////////////////////////////////
//
// fluid3D — 3D incompressible Euler smoke simulation via Vulkan compute
//             shaders, displayed by ray-marching through the volume.
//
// Simulation state — rgba16f 3D images (N×N×N), two ping-pong pairs:
//   stateA[2]  xyz = velocity,  w  = density
//   stateB[2]  x   = temperature
//   pres[2]    x   = pressure   (Jacobi solver ping-pong)
//   divg       x   = velocity divergence (computed once per frame)
//
// Per-frame pipeline (parity p ∈ {0,1}):
//   1. Advect   stateA[p], stateB[p]     → stateA[1-p], stateB[1-p]
//   2. Forces   stateA[1-p], stateB[1-p] → stateA[p],   stateB[p]
//   3. Divg     stateA[p]                → divg
//   4. Jacobi × JACOBI_ITERS  pres[j%2] → pres[(j+1)%2]  (result in pres[0])
//   5. GradSub  stateA[p]+stateB[p]+pres[0] → stateA[1-p]+stateB[1-p]
//   Display:    ray-march stateA[1-p] (density+temp) via push-constant camera
//   Flip p.
//
// Controls:
//   Right-drag  — orbit camera (yaw / pitch)
//   Scroll      — zoom (up = closer)
//   Left-drag   — move smoke injection point (XZ plane at grid bottom)
//   Escape      — quit
//   'M' key     - cycles currentKIdx through K ∈ {1, 2, 4, 6} and prints the new stats to stderr
//
// Jacobi pressure solver — multi-step shared-memory optimization:
//   A naive implementation issues one compute dispatch + one pipeline barrier per
//   Jacobi iteration.  On discrete GPUs each compToComp barrier drains the full
//   pipeline (~2 ms on RTX 3050), so 48 iterations cost ~96 ms in barrier overhead
//   alone — dwarfing the actual arithmetic.
//
//   The fix: K inner iterations are fused into a single dispatch using shared memory
//   (see fluid_jacobi.comp).  Each 8³ workgroup loads an (8+2K)³ halo tile once from
//   L2/DRAM, runs K Jacobi sweeps entirely in shared memory with cheap workgroup
//   barriers, then writes results back.  This replaces K global barriers with 1,
//   reducing total stalls by (K-1)/K.  K=4 cuts 48 barriers to 12; K=6 cuts to 8.
//   A specialization constant bakes K into each pipeline variant at creation time,
//   so the inner loop is fully unrolled and the tile array is a compile-time constant.
//   Press 'M' at runtime to cycle K ∈ {1, 2, 4, 6} and observe the throughput change.
//
////////////////////////////////////////////////////////////////////////////////

#define VKU_SDL2
#include <vku/vku_framework.hpp>
#include <vku/vku.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cstring>
#include <cmath>
#include <chrono>

// ─── Simulation constants ────────────────────────────────────────────────────

static constexpr uint32_t WIDTH        = 320;
static constexpr uint32_t HEIGHT       = 320;
static constexpr uint32_t N            = 96;    // grid edge (must be multiple of WG)
static constexpr uint32_t WG           = 8;     // compute local_size (8³)
static constexpr uint32_t JACOBI_ITERS     = 48;         // must be divisible by LCM(2*K for all K_VALUES) = 24
static constexpr int32_t  K_VALUES[]       = {1, 2, 4, 6}; // JACOBI_ITERS % 2*K == 0 must hold for any K
static constexpr uint32_t K_COUNT          = 4;
static_assert(N % WG == 0, "N must be a multiple of WG");
// JACOBI_ITERS / K must be even for every K so the result always lands in pres[0].
static_assert(JACOBI_ITERS % (2*K_VALUES[0]) == 0, "JACOBI_ITERS/1 must be even");
static_assert(JACOBI_ITERS % (2*K_VALUES[1]) == 0, "JACOBI_ITERS/2 must be even");
static_assert(JACOBI_ITERS % (2*K_VALUES[2]) == 0, "JACOBI_ITERS/4 must be even");
static_assert(JACOBI_ITERS % (2*K_VALUES[3]) == 0, "JACOBI_ITERS/6 must be even");

// ─── UBO (std140 — 20 × 4-byte scalars = 80 bytes) ──────────────────────────

struct FluidUBO {
    float   dt          = 0.25f;
    float   cellSize    = 1.25f * (96.0f / N);  // maintains same physical domain as original 96³ grid
    float   velDiss     = 0.999f;
    float   densDiss    = 0.9990f;
    float   tempDiss    = 0.995f;
    float   ambientTemp = 0.0f;
    float   sigma       = 0.5f;       // buoyancy: temperature lift coefficient
    float   kappa       = 0.02f;      // buoyancy: density sink coefficient
    float   gradScale   = 0.0f;       // filled per frame: 1.125 / cellSize
    float   halfInvCell = 0.0f;       // filled per frame: 0.5 / cellSize
    float   splatRadius = N / 8.0f;   // impulse radius in grid cells
    float   impulseTemp = 15.0f;
    float   impulseDens = 2.0f;
    float   impulsePosX = 0.0f;       // grid-space coordinates
    float   impulsePosY = 0.0f;
    float   impulsePosZ = 0.0f;
    int32_t addImpulse  = 1;
    int32_t gridW       = (int32_t)N;
    int32_t gridH       = (int32_t)N;
    int32_t gridD       = (int32_t)N;
};
static_assert(sizeof(FluidUBO) == 80, "FluidUBO size mismatch");

// ─── Display push constants ──────────────────────────────────────────────────

struct CameraPC {
    glm::vec4 eye;
    glm::mat4 invVP;
};
static_assert(sizeof(CameraPC) == 80, "CameraPC size mismatch");

// ─────────────────────────────────────────────────────────────────────────────

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *sdlWin = SDL_CreateWindow(
        "fluid3D: press M to cycle Jacobi K",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIDTH, HEIGHT,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );
    if (!sdlWin) { fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); SDL_Quit(); return 1; }

    {
        vku::FrameworkOptions fo{ .useCompute = true };
        vku::Framework fw{"fluid3D", fo};
        if (!fw.ok()) { fputs("Framework creation failed\n", stderr); return 1; }

        vk::Device device = fw.device();

        vku::Window window(
            fw.instance(), device, fw.physicalDevice(),
            fw.graphicsQueueFamilyIndex(),
            sdlWin,
            { .desiredPresentMode = vk::PresentModeKHR::eImmediate }
        );
        if (!window.ok()) { fputs("Window creation failed\n", stderr); return 1; }
        window.clearColorValue() = {0.0f, 0.125f, 0.25f, 0.0f};

        ////////////////////////////////////////////////////////////////////////
        // Simulation images — rgba16f for 4-channel, r16f for single-channel.

        const size_t volBytes4 = (size_t)N * N * N * 4 * sizeof(uint16_t);
        const size_t volBytes1 = (size_t)N * N * N * 1 * sizeof(uint16_t);
        std::vector<uint8_t> zeros4(volBytes4, 0);
        std::vector<uint8_t> zeros1(volBytes1, 0);

        auto mkVol = [&]() {
            vku::TextureImage3D img{device, fw.memprops(), N, N, N, 1,
                                   vk::Format::eR16G16B16A16Sfloat};
            img.upload(device, zeros4, window.commandPool(),
                       fw.memprops(), fw.graphicsQueue(),
                       vk::ImageLayout::eGeneral);
            return img;
        };
        auto mkVolR = [&]() {
            vku::TextureImage3D img{device, fw.memprops(), N, N, N, 1,
                                   vk::Format::eR16Sfloat};
            img.upload(device, zeros1, window.commandPool(),
                       fw.memprops(), fw.graphicsQueue(),
                       vk::ImageLayout::eGeneral);
            return img;
        };

        // Dummy 1³ rgba16f image fills unused descriptor slots.
        std::vector<uint8_t> dummyBytes(1 * 1 * 1 * 4 * sizeof(uint16_t), 0);
        vku::TextureImage3D dummy{device, fw.memprops(), 1, 1, 1, 1,
                                  vk::Format::eR16G16B16A16Sfloat};
        dummy.upload(device, dummyBytes, window.commandPool(),
                     fw.memprops(), fw.graphicsQueue(),
                     vk::ImageLayout::eGeneral);

        vku::TextureImage3D stateA[2] = { mkVol(),  mkVol()  };  // rgba16f: vel+dens
        vku::TextureImage3D stateB[2] = { mkVolR(), mkVolR() };  // r16f:   temp
        vku::TextureImage3D pres[2]   = { mkVolR(), mkVolR() };  // r16f:   pressure
        vku::TextureImage3D divg      = mkVolR();                 // r16f:   divergence

        ////////////////////////////////////////////////////////////////////////
        // UBO

        vku::UniformBuffer ubo(device, fw.memprops(), sizeof(FluidUBO));

        ////////////////////////////////////////////////////////////////////////
        // Descriptor set layouts.
        //
        // Compute DSL — 5 rgba32f storage images + 1 UBO:
        //   binding 0: in0   binding 1: in1   binding 2: in2
        //   binding 3: out0  binding 4: out1  binding 5: UBO
        //
        // Display DSL — 2 combined-image-samplers (stateA, stateB).
        // Display pipeline layout adds a push-constant range (CameraPC, 80 B).

        auto computeDSL = vku::DescriptorSetLayoutMaker{}
            .image(0, vk::DescriptorType::eStorageImage,
                   vk::ShaderStageFlagBits::eCompute, 1)
            .image(1, vk::DescriptorType::eStorageImage,
                   vk::ShaderStageFlagBits::eCompute, 1)
            .image(2, vk::DescriptorType::eStorageImage,
                   vk::ShaderStageFlagBits::eCompute, 1)
            .image(3, vk::DescriptorType::eStorageImage,
                   vk::ShaderStageFlagBits::eCompute, 1)
            .image(4, vk::DescriptorType::eStorageImage,
                   vk::ShaderStageFlagBits::eCompute, 1)
            .buffer(5, vk::DescriptorType::eUniformBuffer,
                    vk::ShaderStageFlagBits::eCompute, 1)
            .createUnique(device);

        auto displayDSL = vku::DescriptorSetLayoutMaker{}
            .image(0, vk::DescriptorType::eCombinedImageSampler,
                   vk::ShaderStageFlagBits::eFragment, 1)
            .image(1, vk::DescriptorType::eCombinedImageSampler,
                   vk::ShaderStageFlagBits::eFragment, 1)
            .createUnique(device);

        auto advectDSL = vku::DescriptorSetLayoutMaker{}
            .image(0, vk::DescriptorType::eCombinedImageSampler,
                   vk::ShaderStageFlagBits::eCompute, 1)
            .image(1, vk::DescriptorType::eCombinedImageSampler,
                   vk::ShaderStageFlagBits::eCompute, 1)
            .image(2, vk::DescriptorType::eStorageImage,
                   vk::ShaderStageFlagBits::eCompute, 1)
            .image(3, vk::DescriptorType::eStorageImage,
                   vk::ShaderStageFlagBits::eCompute, 1)
            .image(4, vk::DescriptorType::eStorageImage,
                   vk::ShaderStageFlagBits::eCompute, 1)
            .buffer(5, vk::DescriptorType::eUniformBuffer,
                    vk::ShaderStageFlagBits::eCompute, 1)
            .createUnique(device);

        // Jacobi DSL — bindings 0,1 are sampler3D (presIn, divgIn via texture cache).
        auto jacobiDSL = vku::DescriptorSetLayoutMaker{}
            .image(0, vk::DescriptorType::eCombinedImageSampler,
                   vk::ShaderStageFlagBits::eCompute, 1)
            .image(1, vk::DescriptorType::eCombinedImageSampler,
                   vk::ShaderStageFlagBits::eCompute, 1)
            .image(2, vk::DescriptorType::eStorageImage,
                   vk::ShaderStageFlagBits::eCompute, 1)
            .image(3, vk::DescriptorType::eStorageImage,
                   vk::ShaderStageFlagBits::eCompute, 1)
            .image(4, vk::DescriptorType::eStorageImage,
                   vk::ShaderStageFlagBits::eCompute, 1)
            .buffer(5, vk::DescriptorType::eUniformBuffer,
                    vk::ShaderStageFlagBits::eCompute, 1)
            .createUnique(device);

        auto computePL = vku::PipelineLayoutMaker{}
            .descriptorSetLayout(*computeDSL)
            .createUnique(device);

        auto advectPL = vku::PipelineLayoutMaker{}
            .descriptorSetLayout(*advectDSL)
            .createUnique(device);

        auto jacobiPL = vku::PipelineLayoutMaker{}
            .descriptorSetLayout(*jacobiDSL)
            .createUnique(device);

        auto displayPL = vku::PipelineLayoutMaker{}
            .descriptorSetLayout(*displayDSL)
            .pushConstantRange(vk::ShaderStageFlagBits::eFragment, 0,
                               sizeof(CameraPC))
            .createUnique(device);

        ////////////////////////////////////////////////////////////////////////
        // Own descriptor pool — the framework pool lacks eStorageImage slots.
        // 10 compute sets × 5 storage images = 50; 10 UBOs;
        // 2 advect + 2 jacobi + 2 display sets × 2 samplers = 12 combined samplers.

        std::array<vk::DescriptorPoolSize, 3> poolSizes{{
            { vk::DescriptorType::eStorageImage,         50 },
            { vk::DescriptorType::eUniformBuffer,        10 },
            { vk::DescriptorType::eCombinedImageSampler, 12 }
        }};
        auto descPool = device.createDescriptorPoolUnique(
            vk::DescriptorPoolCreateInfo{
                vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
                12u, (uint32_t)poolSizes.size(), poolSizes.data()
            }
        );

        ////////////////////////////////////////////////////////////////////////
        // Descriptor sets — two per compute pass (one per frame parity).
        //
        // Parity p=0 pipeline:
        //   Advect  stateA[0],stateB[0]           → stateA[1],stateB[1]
        //   Forces  stateA[1],stateB[1]            → stateA[0],stateB[0]
        //   Divg    stateA[0]                      → divg
        //   Jacobi  pres[j%2],divg                 → pres[(j+1)%2]  (result: pres[0])
        //   GradSub stateA[0],stateB[0],pres[0]   → stateA[1],stateB[1]
        //   Display stateA[1], stateB[1]

        auto sampler = vku::SamplerMaker{}
            .magFilter(vk::Filter::eLinear)
            .minFilter(vk::Filter::eLinear)
            .mipmapMode(vk::SamplerMipmapMode::eNearest)
            .addressModeU(vk::SamplerAddressMode::eClampToEdge)
            .addressModeV(vk::SamplerAddressMode::eClampToEdge)
            .addressModeW(vk::SamplerAddressMode::eClampToEdge)
            .createUnique(device);

        auto mk2comp = [&]() {
            return vku::DescriptorSetMaker{}
                .layout(*computeDSL).layout(*computeDSL)
                .create(device, *descPool);
        };

        auto advectSets  = vku::DescriptorSetMaker{}
            .layout(*advectDSL).layout(*advectDSL)
            .create(device, *descPool);
        auto forcesSets  = mk2comp();
        auto divgSets    = mk2comp();
        auto jacobiSets  = vku::DescriptorSetMaker{}
            .layout(*jacobiDSL).layout(*jacobiDSL)
            .create(device, *descPool);
        auto gradSets    = mk2comp();
        auto displaySets = vku::DescriptorSetMaker{}
            .layout(*displayDSL).layout(*displayDSL)
            .create(device, *descPool);

        // Helper: update one compute descriptor set (bindings 0-4 = images, 5 = UBO).
        auto dv = dummy.imageView();
        auto updCS = [&](vk::DescriptorSet ds,
                         vk::ImageView in0, vk::ImageView in1, vk::ImageView in2,
                         vk::ImageView out0, vk::ImageView out1)
        {
            vku::DescriptorSetUpdater{}
                .beginDescriptorSet(ds)
                .beginImages(0, 0, vk::DescriptorType::eStorageImage)
                .image({}, in0,  vk::ImageLayout::eGeneral)
                .beginImages(1, 0, vk::DescriptorType::eStorageImage)
                .image({}, in1,  vk::ImageLayout::eGeneral)
                .beginImages(2, 0, vk::DescriptorType::eStorageImage)
                .image({}, in2,  vk::ImageLayout::eGeneral)
                .beginImages(3, 0, vk::DescriptorType::eStorageImage)
                .image({}, out0, vk::ImageLayout::eGeneral)
                .beginImages(4, 0, vk::DescriptorType::eStorageImage)
                .image({}, out1, vk::ImageLayout::eGeneral)
                .beginBuffers(5, 0, vk::DescriptorType::eUniformBuffer)
                .buffer(ubo.buffer(), 0, sizeof(FluidUBO))
                .update(device);
        };

        auto updAdvect = [&](vk::DescriptorSet ds,
                             vk::ImageView in0, vk::ImageView in1, vk::ImageView in2,
                             vk::ImageView out0, vk::ImageView out1)
        {
            vku::DescriptorSetUpdater{}
                .beginDescriptorSet(ds)
                .beginImages(0, 0, vk::DescriptorType::eCombinedImageSampler)
                .image(*sampler, in0,  vk::ImageLayout::eGeneral)
                .beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler)
                .image(*sampler, in1,  vk::ImageLayout::eGeneral)
                .beginImages(2, 0, vk::DescriptorType::eStorageImage)
                .image({}, in2,  vk::ImageLayout::eGeneral)
                .beginImages(3, 0, vk::DescriptorType::eStorageImage)
                .image({}, out0, vk::ImageLayout::eGeneral)
                .beginImages(4, 0, vk::DescriptorType::eStorageImage)
                .image({}, out1, vk::ImageLayout::eGeneral)
                .beginBuffers(5, 0, vk::DescriptorType::eUniformBuffer)
                .buffer(ubo.buffer(), 0, sizeof(FluidUBO))
                .update(device);
        };

        auto sA0 = stateA[0].imageView(), sA1 = stateA[1].imageView();
        auto sB0 = stateB[0].imageView(), sB1 = stateB[1].imageView();
        auto p0  = pres[0].imageView(),   p1  = pres[1].imageView();
        auto dg  = divg.imageView();

        // Advect[p]: reads stateA[p],stateB[p] → writes stateA[1-p],stateB[1-p]
        updAdvect(advectSets[0], sA0, sB0, dv, sA1, sB1);
        updAdvect(advectSets[1], sA1, sB1, dv, sA0, sB0);

        // Forces[p]: reads stateA[1-p],stateB[1-p] → writes stateA[p],stateB[p]
        updCS(forcesSets[0], sA1, sB1, dv, sA0, sB0);
        updCS(forcesSets[1], sA0, sB0, dv, sA1, sB1);

        // Divg[p]: reads stateA[p] → writes divg
        updCS(divgSets[0], sA0, dv, dv, dg, dv);
        updCS(divgSets[1], sA1, dv, dv, dg, dv);

        // Jacobi[j%2]: pres[j%2],divg → pres[(j+1)%2]  (sampler bindings for L1 cache)
        auto updJacobi = [&](vk::DescriptorSet ds,
                             vk::ImageView presIn, vk::ImageView presOut)
        {
            vku::DescriptorSetUpdater{}
                .beginDescriptorSet(ds)
                .beginImages(0, 0, vk::DescriptorType::eCombinedImageSampler)
                .image(*sampler, presIn, vk::ImageLayout::eGeneral)
                .beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler)
                .image(*sampler, dg,     vk::ImageLayout::eGeneral)
                .beginImages(2, 0, vk::DescriptorType::eStorageImage)
                .image({},       dv,     vk::ImageLayout::eGeneral)
                .beginImages(3, 0, vk::DescriptorType::eStorageImage)
                .image({},       presOut,vk::ImageLayout::eGeneral)
                .beginImages(4, 0, vk::DescriptorType::eStorageImage)
                .image({},       dv,     vk::ImageLayout::eGeneral)
                .beginBuffers(5, 0, vk::DescriptorType::eUniformBuffer)
                .buffer(ubo.buffer(), 0, sizeof(FluidUBO))
                .update(device);
        };
        updJacobi(jacobiSets[0], p0, p1);
        updJacobi(jacobiSets[1], p1, p0);

        // GradSub[p]: stateA[p],stateB[p],pres[0] → stateA[1-p],stateB[1-p]
        updCS(gradSets[0], sA0, sB0, p0, sA1, sB1);
        updCS(gradSets[1], sA1, sB1, p0, sA0, sB0);

        // Display[p]: sample the grad output = stateA[1-p], stateB[1-p]
        vku::DescriptorSetUpdater{}
            .beginDescriptorSet(displaySets[0])   // p=0 → display stateA[1],stateB[1]
            .beginImages(0, 0, vk::DescriptorType::eCombinedImageSampler)
            .image(*sampler, sA1, vk::ImageLayout::eGeneral)
            .beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler)
            .image(*sampler, sB1, vk::ImageLayout::eGeneral)
            .beginDescriptorSet(displaySets[1])   // p=1 → display stateA[0],stateB[0]
            .beginImages(0, 0, vk::DescriptorType::eCombinedImageSampler)
            .image(*sampler, sA0, vk::ImageLayout::eGeneral)
            .beginImages(1, 0, vk::DescriptorType::eCombinedImageSampler)
            .image(*sampler, sB0, vk::ImageLayout::eGeneral)
            .update(device);

        ////////////////////////////////////////////////////////////////////////
        // Compute pipelines.

        vku::ShaderModule shAdvect{device, BINARY_DIR "fluid_advect.comp.spv"};
        vku::ShaderModule shForces{device, BINARY_DIR "fluid_forces.comp.spv"};
        vku::ShaderModule shDivg  {device, BINARY_DIR "fluid_divg.comp.spv"};
        vku::ShaderModule shJacobi{device, BINARY_DIR "fluid_jacobi.comp.spv"};
        vku::ShaderModule shGrad  {device, BINARY_DIR "fluid_grad.comp.spv"};

        auto mkComp = [&](vku::ShaderModule &sh) {
            return vku::ComputePipelineMaker{}
                .shader(vk::ShaderStageFlagBits::eCompute, sh)
                .createUnique(device, fw.pipelineCache(), *computePL);
        };
        auto advectPipeline  = vku::ComputePipelineMaker{}
            .shader(vk::ShaderStageFlagBits::eCompute, shAdvect)
            .createUnique(device, fw.pipelineCache(), *advectPL);
        auto forcesPipeline  = mkComp(shForces);
        auto divgPipeline    = mkComp(shDivg);
        auto gradPipeline    = mkComp(shGrad);

        // One Jacobi pipeline per K value (specialization constant 0 = K).
        std::array<vk::UniquePipeline, K_COUNT> jacobiPipelines;
        for (uint32_t i = 0; i < K_COUNT; ++i) {
            vku::PipelineMaker::SpecData specData(
                std::vector<vku::SpecConst>{ vku::SpecConst(0, K_VALUES[i]) });
            jacobiPipelines[i] = vku::ComputePipelineMaker{}
                .shader(vk::ShaderStageFlagBits::eCompute, shJacobi, std::move(specData))
                .createUnique(device, fw.pipelineCache(), *jacobiPL);
        }

        ////////////////////////////////////////////////////////////////////////
        // Display pipeline — fullscreen triangle, dynamic viewport/scissor.

        vku::ShaderModule shDispVert{device, BINARY_DIR "fluid_display.vert.spv"};
        vku::ShaderModule shDispFrag{device, BINARY_DIR "fluid_display.frag.spv"};

        auto displayPipeline = vku::PipelineMaker{window.width(), window.height()}
            .shader(vk::ShaderStageFlagBits::eVertex,   shDispVert)
            .shader(vk::ShaderStageFlagBits::eFragment, shDispFrag)
            .dynamicState(vk::DynamicState::eViewport)
            .dynamicState(vk::DynamicState::eScissor)
            .depthTestEnable(VK_FALSE)
            .depthWriteEnable(VK_FALSE)
            .createUnique(device, fw.pipelineCache(), *displayPL, window.renderPass());

        ////////////////////////////////////////////////////////////////////////
        // Barrier helpers.

        auto compToComp = [&](vk::CommandBuffer cb) {
            vk::MemoryBarrier mb{
                vk::AccessFlagBits::eShaderWrite,
                vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite
            };
            cb.pipelineBarrier(
                vk::PipelineStageFlagBits::eComputeShader,
                vk::PipelineStageFlagBits::eComputeShader,
                {}, mb, {}, {});
        };

        auto compToFrag = [&](vk::CommandBuffer cb) {
            vk::MemoryBarrier mb{
                vk::AccessFlagBits::eShaderWrite,
                vk::AccessFlagBits::eShaderRead
            };
            cb.pipelineBarrier(
                vk::PipelineStageFlagBits::eComputeShader,
                vk::PipelineStageFlagBits::eFragmentShader,
                {}, mb, {}, {});
        };

        ////////////////////////////////////////////////////////////////////////
        // Camera state.

        float camYaw   = 0.0f;    // radians, around world Y
        float camPitch = 0.3f;    // radians from horizontal
        float camDist  = 2.0f;    // distance from origin

        ////////////////////////////////////////////////////////////////////////
        // Main loop.

        int      p            = 0;
        uint32_t currentKIdx  = 3;   // default K=K_VALUES[currentKIdx]=6
        fprintf(stderr, "K = %d (%u outer dispatches)  [press M to cycle]\n",
                K_VALUES[currentKIdx], JACOBI_ITERS / (uint32_t)K_VALUES[currentKIdx]);

        float impX      = N / 2.0f;
        float impY      = N * 0.85f;    // near top (grid Y≈N); smoke falls toward Y=0
        float impZ      = N / 2.0f;

        bool leftDown  = false;
        bool rightDown = false;
        int  lastMX    = 0, lastMY = 0;

        bool running   = true;
        bool minimized = false;

        static constexpr int BENCH_FRAMES = 100;
        int  benchFrame = 0;
        auto tBench = std::chrono::steady_clock::now();

        while (running) {
            SDL_Event ev;
            while (SDL_PollEvent(&ev)) {
                switch (ev.type) {
                case SDL_QUIT:
                    running = false; break;
                case SDL_KEYDOWN:
                    if (ev.key.keysym.sym == SDLK_ESCAPE) running = false;
                    if (ev.key.keysym.sym == SDLK_m) {
                        currentKIdx = (currentKIdx + 1) % K_COUNT;
                        fprintf(stderr, "K = %d (%u outer dispatches)\n",
                                K_VALUES[currentKIdx],
                                JACOBI_ITERS / (uint32_t)K_VALUES[currentKIdx]);
                    }
                    break;
                case SDL_WINDOWEVENT:
                    if (ev.window.event == SDL_WINDOWEVENT_MINIMIZED) minimized = true;
                    if (ev.window.event == SDL_WINDOWEVENT_RESTORED)  minimized = false;
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (ev.button.button == SDL_BUTTON_LEFT)  leftDown  = true;
                    if (ev.button.button == SDL_BUTTON_RIGHT) rightDown = true;
                    lastMX = ev.button.x;
                    lastMY = ev.button.y;
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (ev.button.button == SDL_BUTTON_LEFT)  leftDown  = false;
                    if (ev.button.button == SDL_BUTTON_RIGHT) rightDown = false;
                    break;
                case SDL_MOUSEMOTION:
                    if (rightDown) {
                        float dx = (ev.motion.x - lastMX) * 0.005f;
                        float dy = (ev.motion.y - lastMY) * 0.005f;
                        camYaw   -= dx;
                        camPitch  = std::clamp(camPitch + dy, -1.4f, 1.4f);
                    }
                    if (leftDown) {
                        impX = ev.motion.x * float(N) / float(WIDTH);
                        impZ = ev.motion.y * float(N) / float(HEIGHT);
                    }
                    lastMX = ev.motion.x;
                    lastMY = ev.motion.y;
                    break;
                case SDL_MOUSEWHEEL:
                    camDist = std::clamp(camDist + ev.wheel.y * 0.1f, 0.5f, 6.0f);
                    break;
                default: break;
                }
            }

            if (minimized) { SDL_Delay(16); continue; }

            if (!leftDown) { impX = N / 2.0f; impZ = N / 2.0f; }

            glm::vec3 eye{
                camDist * std::cos(camPitch) * std::sin(camYaw),
                camDist * std::sin(camPitch),
                camDist * std::cos(camPitch) * std::cos(camYaw)
            };
            float aspect = float(window.width()) / float(window.height());
            glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.01f, 100.0f);
            proj[1][1] *= -1.0f;
            glm::mat4 view = glm::lookAt(eye, glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f));
            CameraPC camPC;
            camPC.eye   = glm::vec4(eye, 0.0f);
            camPC.invVP = glm::inverse(proj * view);

            window.draw(device, fw.graphicsQueue(),
                [&](vk::CommandBuffer cb, int /*imageIndex*/,
                    vk::RenderPassBeginInfo &rpbi)
                {
                    cb.begin(vk::CommandBufferBeginInfo{});

                    vk::MemoryBarrier crossFrame{
                        vk::AccessFlagBits::eShaderWrite,
                        vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead
                    };
                    cb.pipelineBarrier(
                        vk::PipelineStageFlagBits::eFragmentShader |
                        vk::PipelineStageFlagBits::eComputeShader,
                        vk::PipelineStageFlagBits::eComputeShader,
                        {}, crossFrame, {}, {});

                    FluidUBO uboData;
                    uboData.gradScale   = 1.125f / uboData.cellSize;
                    uboData.halfInvCell = 0.5f   / uboData.cellSize;
                    uboData.impulsePosX = impX;
                    uboData.impulsePosY = impY;
                    uboData.impulsePosZ = impZ;
                    uboData.addImpulse  = 1;

                    ubo.barrier(cb,
                        vk::PipelineStageFlagBits::eComputeShader,
                        vk::PipelineStageFlagBits::eTransfer,
                        {}, {}, vk::AccessFlagBits::eTransferWrite,
                        fw.graphicsQueueFamilyIndex(),
                        fw.graphicsQueueFamilyIndex());
                    cb.updateBuffer(ubo.buffer(), 0, sizeof(FluidUBO), &uboData);
                    ubo.barrier(cb,
                        vk::PipelineStageFlagBits::eTransfer,
                        vk::PipelineStageFlagBits::eComputeShader,
                        {}, vk::AccessFlagBits::eTransferWrite,
                        vk::AccessFlagBits::eUniformRead,
                        fw.graphicsQueueFamilyIndex(),
                        fw.graphicsQueueFamilyIndex());

                    auto dispatch = [&](vk::Pipeline pl, vk::PipelineLayout plo, vk::DescriptorSet ds) {
                        cb.bindPipeline(vk::PipelineBindPoint::eCompute, pl);
                        cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                              plo, 0, ds, nullptr);
                        cb.dispatch(N / WG, N / WG, N / WG);
                    };

                    dispatch(*advectPipeline, *advectPL,  advectSets[p]);  compToComp(cb);
                    dispatch(*forcesPipeline, *computePL, forcesSets[p]);  compToComp(cb);
                    dispatch(*divgPipeline,   *computePL, divgSets[p]);    compToComp(cb);

                    // Cold-start pressure from zero each frame (matches OpenGL behaviour,
                    // prevents fp16 asymmetry accumulation that causes turbulence).
                    {
                        vk::MemoryBarrier toTransfer{
                            vk::AccessFlagBits::eShaderWrite,
                            vk::AccessFlagBits::eTransferWrite
                        };
                        cb.pipelineBarrier(
                            vk::PipelineStageFlagBits::eComputeShader,
                            vk::PipelineStageFlagBits::eTransfer,
                            {}, toTransfer, {}, {});

                        vk::ClearColorValue zero{};
                        vk::ImageSubresourceRange range{
                            vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
                        cb.clearColorImage(pres[0].image(),
                            vk::ImageLayout::eGeneral, zero, range);

                        vk::MemoryBarrier fromTransfer{
                            vk::AccessFlagBits::eTransferWrite,
                            vk::AccessFlagBits::eShaderRead
                        };
                        cb.pipelineBarrier(
                            vk::PipelineStageFlagBits::eTransfer,
                            vk::PipelineStageFlagBits::eComputeShader,
                            {}, fromTransfer, {}, {});
                    }

                    {
                        uint32_t outerIters = JACOBI_ITERS / (uint32_t)K_VALUES[currentKIdx];
                        vk::Pipeline jpl = *jacobiPipelines[currentKIdx];
                        for (uint32_t j = 0; j < outerIters; ++j) {
                            dispatch(jpl, *jacobiPL, jacobiSets[j % 2]); compToComp(cb);
                        }
                    }
                    dispatch(*gradPipeline, *computePL, gradSets[p]);
                    compToFrag(cb);

                    cb.beginRenderPass(rpbi, vk::SubpassContents::eInline);
                    vk::Viewport vp{0.f, 0.f,
                        float(window.width()), float(window.height()), 0.f, 1.f};
                    vk::Rect2D sc{{0, 0}, {window.width(), window.height()}};
                    cb.setViewport(0, vp);
                    cb.setScissor(0, sc);
                    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *displayPipeline);
                    cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                          *displayPL, 0, displaySets[p], nullptr);
                    cb.pushConstants(*displayPL, vk::ShaderStageFlagBits::eFragment,
                                     0, sizeof(CameraPC), &camPC);
                    cb.draw(3, 1, 0, 0);
                    cb.endRenderPass();
                    cb.end();
                }
            );

            p = 1 - p;

            if (++benchFrame == BENCH_FRAMES) {
                auto ms = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - tBench).count();
                fprintf(stderr, "%d frames: %.2f ms/frame  (%.1f fps)\n",
                        BENCH_FRAMES, ms / BENCH_FRAMES, 1000.0 * BENCH_FRAMES / ms);
                benchFrame = 0;
                tBench = std::chrono::steady_clock::now();
            }
        }

        device.waitIdle();
    } // all Vulkan objects destroyed before SDL teardown

    SDL_DestroyWindow(sdlWin);
    SDL_Quit();
    return 0;
}
