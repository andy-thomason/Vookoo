////////////////////////////////////////////////////////////////////////////////
// gamedemo — minimal Asteroids-lite demo (multi-layer refactor)
//
// DrawLayer / InstancedLayer pattern:
//   - DrawLayer: interface — begin_frame(), dispatch(cb, slot), record(cb, slot), shutdown(dev)
//   - InstancedLayer: owns its own pipeline + RING host-visible instance bufs +
//     device-local mesh VBO; queues instances each frame, records its own draws
//   - GpuResources: framework + window + shared proj UBO + vector<DrawLayer*>;
//     end_frame() binds proj UBO once, then iterates layers calling record()
//
// Adding a new draw type (e.g. an exhaust particle system):
//   1. Reuse ShapeInstance or define a new per-instance struct with matching
//      vertex attributes in your SPIR-V shaders
//   2. Declare an InstancedLayer (global or local in main), call init(), add_layer()
//   3. Queue instances from your render system each frame
//
// Controls: Left/Right = rotate  Up = thrust  Space = fire  R = restart
////////////////////////////////////////////////////////////////////////////////

#define VKU_SDL2
#include <vku/vku_framework.hpp>
#include <vku/vku.hpp>
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <optional>
#include <vector>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <random>
#include <algorithm>
#include <unordered_map>
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#include "ecs.hpp"

// ============================================================================
// DrawLayer — interface for a self-contained pipeline + ring buffer pair.
//
// GpuResources calls begin_frame() once before queuing, then record() inside
// the win->draw() lambda once per swapchain image.  Each implementation owns
// its VkPipeline, instance buffers, and mesh VBO independently.
// ============================================================================
struct DrawLayer {
    virtual void begin_frame()                                    = 0;
    virtual void dispatch(vk::CommandBuffer /*cb*/, int /*slot*/) {}
    virtual void record(vk::CommandBuffer cb, int slot)           = 0;
    virtual void shutdown(vk::Device dev)                         = 0;
    virtual ~DrawLayer() = default;
};

// ============================================================================
// GpuResources — framework, window, shared proj UBO, and layer registry.
//
// Pipeline / mesh / instance-buffer state has moved into DrawLayer subclasses.
// GpuResources is now responsible only for:
//   - Vulkan device + swapchain (via vku::Framework / vku::Window)
//   - The shared proj-UBO descriptor set bound once per frame
//   - Iterating registered layers in begin_frame() and end_frame()
// ============================================================================
struct GpuResources {
    static constexpr int RING = 3;

    std::optional<vku::Framework> fw;
    std::optional<vku::Window>    win;

    vku::GenericBuffer            proj_buf;
    vk::UniqueDescriptorSetLayout dsl;
    vk::UniquePipelineLayout      pl;
    vk::DescriptorSet             ds;

    std::vector<DrawLayer*> layers;

    vku::ColorAttachmentImage     offscreen;
    vk::UniqueSampler             offscreen_sampler;
    vk::UniqueRenderPass          offscreen_rp;
    vk::UniqueFramebuffer         offscreen_fb;
    vk::UniqueDescriptorSetLayout post_dsl;
    vk::UniquePipelineLayout      post_pl;
    vk::DescriptorSet             post_ds;
    vk::UniquePipeline            post_pipeline;

    // Register a layer to participate in begin_frame() / end_frame().
    // The caller retains ownership; layers must outlive GpuResources::shutdown().
    void add_layer(DrawLayer* layer) { layers.push_back(layer); }

    // -------------------------------------------------------------------------
    void init(SDL_Window* sdl_win) {
        fw.emplace("gamedemo");
        if (!fw->ok()) { throw std::runtime_error("vku::Framework failed"); }

        vku::WindowOptions wopts;
        wopts.desiredPresentMode = vk::PresentModeKHR::eFifo;
        win.emplace(fw->instance(), fw->device(), fw->physicalDevice(),
                    fw->graphicsQueueFamilyIndex(), sdl_win, wopts);
        if (!win->ok()) { throw std::runtime_error("vku::Window failed"); }

        win->clearColorValue() = {0.02f, 0.02f, 0.05f, 1.0f};

        // Proj UBO: one mat4, host-visible so reshape() can write it directly.
        proj_buf = vku::GenericBuffer(
            fw->device(), fw->memprops(),
            vk::BufferUsageFlagBits::eUniformBuffer,
            sizeof(glm::mat4),
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        // Shared descriptor set layout (set 0, binding 0 = proj UBO).
        // All layer pipelines are created with this layout so the single
        // bindDescriptorSets call in end_frame() covers every layer.
        dsl = vku::DescriptorSetLayoutMaker{}
            .buffer(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex, 1)
            .createUnique(fw->device());

        pl = vku::PipelineLayoutMaker{}
            .descriptorSetLayout(*dsl)
            .createUnique(fw->device());

        ds = vku::DescriptorSetMaker{}
            .layout(*dsl)
            .create(fw->device(), fw->descriptorPool())[0];

        vku::DescriptorSetUpdater{}
            .beginDescriptorSet(ds)
            .beginBuffers(0, 0, vk::DescriptorType::eUniformBuffer)
            .buffer(proj_buf.buffer(), 0, sizeof(glm::mat4))
            .update(fw->device());

        // Offscreen render target — game geometry draws here each frame.
        offscreen = vku::ColorAttachmentImage(fw->device(), fw->memprops(),
            win->width(), win->height(), vk::Format::eR16G16B16A16Sfloat);

        offscreen_sampler = fw->device().createSamplerUnique(
            vk::SamplerCreateInfo{}
                .setMagFilter(vk::Filter::eLinear)
                .setMinFilter(vk::Filter::eLinear)
                .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeW(vk::SamplerAddressMode::eClampToEdge));

        // Render pass: clear to black, transition to eShaderReadOnlyOptimal when done.
        {
            vk::AttachmentDescription att;
            att.format         = vk::Format::eR16G16B16A16Sfloat;
            att.samples        = vk::SampleCountFlagBits::e1;
            att.loadOp         = vk::AttachmentLoadOp::eClear;
            att.storeOp        = vk::AttachmentStoreOp::eStore;
            att.stencilLoadOp  = vk::AttachmentLoadOp::eDontCare;
            att.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
            att.initialLayout  = vk::ImageLayout::eUndefined;
            att.finalLayout    = vk::ImageLayout::eShaderReadOnlyOptimal;

            vk::AttachmentReference ref{0, vk::ImageLayout::eColorAttachmentOptimal};

            vk::SubpassDescription subpass;
            subpass.pipelineBindPoint    = vk::PipelineBindPoint::eGraphics;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments    = &ref;

            std::array<vk::SubpassDependency, 2> deps;
            deps[0].srcSubpass      = VK_SUBPASS_EXTERNAL;
            deps[0].dstSubpass      = 0;
            deps[0].srcStageMask    = vk::PipelineStageFlagBits::eFragmentShader;
            deps[0].dstStageMask    = vk::PipelineStageFlagBits::eColorAttachmentOutput;
            deps[0].srcAccessMask   = vk::AccessFlagBits::eShaderRead;
            deps[0].dstAccessMask   = vk::AccessFlagBits::eColorAttachmentWrite;
            deps[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;
            deps[1].srcSubpass      = 0;
            deps[1].dstSubpass      = VK_SUBPASS_EXTERNAL;
            deps[1].srcStageMask    = vk::PipelineStageFlagBits::eColorAttachmentOutput;
            deps[1].dstStageMask    = vk::PipelineStageFlagBits::eFragmentShader;
            deps[1].srcAccessMask   = vk::AccessFlagBits::eColorAttachmentWrite;
            deps[1].dstAccessMask   = vk::AccessFlagBits::eShaderRead;
            deps[1].dependencyFlags = vk::DependencyFlagBits::eByRegion;

            vk::RenderPassCreateInfo rpci;
            rpci.attachmentCount = 1;
            rpci.pAttachments    = &att;
            rpci.subpassCount    = 1;
            rpci.pSubpasses      = &subpass;
            rpci.dependencyCount = (uint32_t)deps.size();
            rpci.pDependencies   = deps.data();
            offscreen_rp = fw->device().createRenderPassUnique(rpci);
        }

        {
            vk::ImageView views[] = {offscreen.imageView()};
            vk::FramebufferCreateInfo fci;
            fci.renderPass      = *offscreen_rp;
            fci.attachmentCount = 1;
            fci.pAttachments    = views;
            fci.width           = win->width();
            fci.height          = win->height();
            fci.layers          = 1;
            offscreen_fb = fw->device().createFramebufferUnique(fci);
        }

        // Post descriptor set: set 0, binding 0 = offscreen sampler.
        post_dsl = vku::DescriptorSetLayoutMaker{}
            .image(0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1)
            .createUnique(fw->device());

        post_pl = vku::PipelineLayoutMaker{}
            .descriptorSetLayout(*post_dsl)
            .pushConstantRange(vk::ShaderStageFlagBits::eFragment, 0, sizeof(float))
            .createUnique(fw->device());

        post_ds = vku::DescriptorSetMaker{}
            .layout(*post_dsl)
            .create(fw->device(), fw->descriptorPool())[0];

        vku::DescriptorSetUpdater{}
            .beginDescriptorSet(post_ds)
            .beginImages(0, 0, vk::DescriptorType::eCombinedImageSampler)
            .image(*offscreen_sampler, offscreen.imageView(), vk::ImageLayout::eShaderReadOnlyOptimal)
            .update(fw->device());

        // Post pipeline: fullscreen triangle, no vertex input, targets swapchain render pass.
        {
            vku::ShaderModule vert{fw->device(), BINARY_DIR "gamedemo_post.vert.spv"};
            vku::ShaderModule frag{fw->device(), BINARY_DIR "gamedemo_post.frag.spv"};
            post_pipeline = vku::PipelineMaker{win->width(), win->height()}
                .shader(vk::ShaderStageFlagBits::eVertex,   vert)
                .shader(vk::ShaderStageFlagBits::eFragment, frag)
                .topology(vk::PrimitiveTopology::eTriangleList)
                .blendBegin(false)
                .dynamicState(vk::DynamicState::eViewport)
                .dynamicState(vk::DynamicState::eScissor)
                .createUnique(fw->device(), fw->pipelineCache(), *post_pl, win->renderPass());
        }
    }

    // -------------------------------------------------------------------------
    // Y-flipped ortho for Vulkan NDC (+Y = up in world space).
    void reshape(int w, int h) {
        float hw = w * 0.5f, hh = h * 0.5f;
        glm::mat4 proj = glm::ortho(-hw, hw, hh, -hh, -1.f, 1.f);
        proj_buf.updateLocal(fw->device(), &proj, sizeof(proj));
    }

    // -------------------------------------------------------------------------
    void begin_frame() {
        for (auto* layer : layers) { layer->begin_frame(); }
    }

    // -------------------------------------------------------------------------
    void end_frame() {
        win->draw(fw->device(), fw->graphicsQueue(),
            [&](vk::CommandBuffer cb, int fi, vk::RenderPassBeginInfo& rpbi) {
                int slot = fi % RING;
                vk::Viewport vp{0, 0, (float)win->width(), (float)win->height(), 0, 1};
                vk::Rect2D   sc{{0, 0}, {win->width(), win->height()}};

                cb.begin(vk::CommandBufferBeginInfo{});

                // Compute dispatches — must run before any render pass.
                for (auto* layer : layers) { layer->dispatch(cb, slot); }

                // Pass 1: game geometry → offscreen RGBA16F image.
                vk::ClearValue black; black.color = {0.f, 0.f, 0.f, 1.f};
                vk::RenderPassBeginInfo off_rpbi;
                off_rpbi.renderPass      = *offscreen_rp;
                off_rpbi.framebuffer     = *offscreen_fb;
                off_rpbi.renderArea      = sc;
                off_rpbi.clearValueCount = 1;
                off_rpbi.pClearValues    = &black;

                cb.beginRenderPass(off_rpbi, vk::SubpassContents::eInline);
                cb.setViewport(0, vp);
                cb.setScissor(0, sc);
                cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pl, 0, ds, {});
                for (auto* layer : layers) { layer->record(cb, slot); }
                cb.endRenderPass();
                // offscreen image is now in eShaderReadOnlyOptimal (render pass finalLayout).

                // Pass 2: post-process → swapchain.
                cb.beginRenderPass(rpbi, vk::SubpassContents::eInline);
                cb.setViewport(0, vp);
                cb.setScissor(0, sc);
                cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *post_pipeline);
                cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *post_pl, 0, post_ds, {});
                float t = SDL_GetTicks() / 1000.0f;
                cb.pushConstants(*post_pl, vk::ShaderStageFlagBits::eFragment, 0, sizeof(float), &t);
                cb.draw(3, 1, 0, 0);  // fullscreen triangle, no VBO
                cb.endRenderPass();

                cb.end();
            });
    }

    // -------------------------------------------------------------------------
    void shutdown() {
        fw->device().waitIdle();
        for (auto* layer : layers) { layer->shutdown(fw->device()); }
        post_pipeline.reset();
        post_pl.reset();
        post_dsl.reset();
        offscreen_fb.reset();
        offscreen_rp.reset();
        offscreen_sampler.reset();
        offscreen = {};
        proj_buf = {};
        pl.reset();
        dsl.reset();
        win.reset();
        fw.reset();
    }
};

// ============================================================================
// ShapeInstance — per-instance data for line-list geometry.
// Matches the vertex attribute layout declared in InstancedLayer::init().
// ============================================================================
struct ShapeInstance { float x, y, angle, scale, color; };

// ============================================================================
// InstancedLayer — one pipeline + ring instance buffers for a single draw type.
//
// Usage:
//   InstancedLayer my_layer;
//   my_layer.init(gpu, "my.vert.spv", "my.frag.spv", pts, n);
//   gpu.add_layer(&my_layer);
//   // each frame:
//   my_layer.queue(x, y, angle, scale, color);
//
// To use different per-instance data, add a new struct alongside ShapeInstance
// and a new layer type that stores that struct and declares matching vertex
// attributes in its init() call.
// ============================================================================
struct InstancedLayer : DrawLayer {
    static constexpr int MAX_INSTANCES = 512;

    vku::GenericBuffer                  mesh_vbo;
    int                                 n_verts = 0;
    vk::UniquePipeline                  pipeline;
    std::array<vku::GenericBuffer, 3>   inst_bufs;
    std::array<ShapeInstance*, 3>       inst_ptrs{};
    std::vector<ShapeInstance>          pending;

    // Upload mesh geometry and build the pipeline.
    // pts/n: line-list vertex pairs (v0,v1, v2,v3, ...).
    // vert_spv/frag_spv: file paths to compiled SPIR-V modules.
    // Uses gpu.pl as the pipeline layout so the shared proj UBO descriptor
    // set bound in GpuResources::end_frame() is valid for this pipeline.
    void init(GpuResources& gpu, const char* vert_spv, const char* frag_spv,
              const glm::vec2* pts, int n) {
        auto  dev      = gpu.fw->device();
        auto& memprops = gpu.fw->memprops();

        // Device-local mesh VBO uploaded once via a short-lived staging buffer.
        size_t bytes = n * sizeof(glm::vec2);
        vku::GenericBuffer staging(dev, memprops,
            vk::BufferUsageFlagBits::eTransferSrc, bytes,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        staging.updateLocal(dev, pts, bytes);

        mesh_vbo = vku::GenericBuffer(dev, memprops,
            vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
            bytes, vk::MemoryPropertyFlagBits::eDeviceLocal);
        vku::executeImmediately(dev, gpu.win->commandPool(), gpu.fw->graphicsQueue(),
            [&](vk::CommandBuffer cb) {
                cb.copyBuffer(staging.buffer(), mesh_vbo.buffer(), vk::BufferCopy{0, 0, bytes});
            });
        n_verts = n;

        // One host-visible instance buffer per swapchain image (RING=3).
        // Written by the CPU each frame; never read by the GPU until the
        // matching fence fires, so no explicit sync is needed here.
        size_t inst_bytes = MAX_INSTANCES * sizeof(ShapeInstance);
        for (int i = 0; i < GpuResources::RING; ++i) {
            inst_bufs[i] = vku::GenericBuffer(dev, memprops,
                vk::BufferUsageFlagBits::eVertexBuffer, inst_bytes,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
            inst_ptrs[i] = static_cast<ShapeInstance*>(inst_bufs[i].map(dev));
        }

        // Pipeline: binding 0 = mesh geometry (eVertex), binding 1 = ShapeInstance (eInstance).
        // eLineList avoids cross-instance connections that eLineStrip would produce.
        // To use different shaders or a different ShapeInstance layout, swap vert_spv/frag_spv
        // and adjust the vertexAttribute declarations to match your new struct.
        vku::ShaderModule vert{dev, vert_spv};
        vku::ShaderModule frag{dev, frag_spv};
        pipeline = vku::PipelineMaker{gpu.win->width(), gpu.win->height()}
            .shader(vk::ShaderStageFlagBits::eVertex,   vert)
            .shader(vk::ShaderStageFlagBits::eFragment, frag)
            .vertexBinding(0, sizeof(glm::vec2),    vk::VertexInputRate::eVertex)
            .vertexAttribute(0, 0, vk::Format::eR32G32Sfloat,       0)
            .vertexBinding(1, sizeof(ShapeInstance), vk::VertexInputRate::eInstance)
            .vertexAttribute(1, 1, vk::Format::eR32G32B32A32Sfloat, 0)                // x,y,angle,scale
            .vertexAttribute(2, 1, vk::Format::eR32Sfloat,          4 * sizeof(float)) // color
            .topology(vk::PrimitiveTopology::eLineList)
            .blendBegin(false)
            .dynamicState(vk::DynamicState::eViewport)
            .dynamicState(vk::DynamicState::eScissor)
            .createUnique(dev, gpu.fw->pipelineCache(), *gpu.pl, *gpu.offscreen_rp);
    }

    void queue(float x, float y, float angle, float scale, float color) {
        if ((int)pending.size() < MAX_INSTANCES) {
            pending.push_back({x, y, angle, scale, color});
        }
    }

    void begin_frame() override {
        pending.clear();
    }

    // Pack pending instances into this image's ring slot, then bind this
    // layer's pipeline and issue one draw call.
    void record(vk::CommandBuffer cb, int slot) override {
        if (pending.empty()) { return; }
        std::memcpy(inst_ptrs[slot], pending.data(), pending.size() * sizeof(ShapeInstance));
        vk::DeviceSize zero = 0;
        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
        cb.bindVertexBuffers(0, mesh_vbo.buffer(), zero);
        cb.bindVertexBuffers(1, inst_bufs[slot].buffer(), zero);
        cb.draw(n_verts, (uint32_t)pending.size(), 0, 0);
    }

    void shutdown(vk::Device dev) override {
        for (int i = 0; i < GpuResources::RING; ++i) {
            if (inst_ptrs[i]) {
                inst_bufs[i].unmap(dev);
                inst_ptrs[i] = nullptr;
            }
            inst_bufs[i] = {};
        }
        mesh_vbo = {};
        pipeline.reset();
    }
};

// ============================================================================
// RockLayer — filled quads with Perlin noise shaping; no mesh VBO (positions
// generated in the vertex shader from gl_VertexIndex).  Per-instance data has
// the same memory layout as ShapeInstance so the same attribute declarations
// are reused, with scale→radius and color→seed.
// ============================================================================
struct RockInstance { float x, y, angle, radius, seed; };

struct RockLayer : DrawLayer {
    static constexpr int MAX_INSTANCES = 512;

    vk::UniquePipeline                 pipeline;
    std::array<vku::GenericBuffer, 3>  inst_bufs;
    std::array<RockInstance*, 3>       inst_ptrs{};
    std::vector<RockInstance>          pending;

    void init(GpuResources& gpu) {
        auto  dev      = gpu.fw->device();
        auto& memprops = gpu.fw->memprops();

        size_t inst_bytes = MAX_INSTANCES * sizeof(RockInstance);
        for (int i = 0; i < GpuResources::RING; ++i) {
            inst_bufs[i] = vku::GenericBuffer(dev, memprops,
                vk::BufferUsageFlagBits::eVertexBuffer, inst_bytes,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
            inst_ptrs[i] = static_cast<RockInstance*>(inst_bufs[i].map(dev));
        }

        vku::ShaderModule vert{dev, BINARY_DIR "gamedemo_rock.vert.spv"};
        vku::ShaderModule frag{dev, BINARY_DIR "gamedemo_rock.frag.spv"};
        pipeline = vku::PipelineMaker{gpu.win->width(), gpu.win->height()}
            .shader(vk::ShaderStageFlagBits::eVertex,   vert)
            .shader(vk::ShaderStageFlagBits::eFragment, frag)
            .vertexBinding(1, sizeof(RockInstance), vk::VertexInputRate::eInstance)
            .vertexAttribute(1, 1, vk::Format::eR32G32B32A32Sfloat, 0)                // x,y,angle,radius
            .vertexAttribute(2, 1, vk::Format::eR32Sfloat,          4 * sizeof(float)) // seed
            .topology(vk::PrimitiveTopology::eTriangleList)
            .blendBegin(false)
            .dynamicState(vk::DynamicState::eViewport)
            .dynamicState(vk::DynamicState::eScissor)
            .createUnique(dev, gpu.fw->pipelineCache(), *gpu.pl, *gpu.offscreen_rp);
    }

    void queue(float x, float y, float angle, float radius, float seed) {
        if ((int)pending.size() < MAX_INSTANCES) {
            pending.push_back({x, y, angle, radius, seed});
        }
    }

    void begin_frame() override { pending.clear(); }

    void record(vk::CommandBuffer cb, int slot) override {
        if (pending.empty()) { return; }
        std::memcpy(inst_ptrs[slot], pending.data(), pending.size() * sizeof(RockInstance));
        vk::DeviceSize zero = 0;
        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
        cb.bindVertexBuffers(1, inst_bufs[slot].buffer(), zero);
        cb.draw(6, (uint32_t)pending.size(), 0, 0);  // 6 verts (2 triangles) per rock
    }

    void shutdown(vk::Device dev) override {
        for (int i = 0; i < GpuResources::RING; ++i) {
            if (inst_ptrs[i]) { inst_bufs[i].unmap(dev); inst_ptrs[i] = nullptr; }
            inst_bufs[i] = {};
        }
        pipeline.reset();
    }
};

// ============================================================================
// HudLayer — SDF text rendered from a pre-baked BMFont atlas.
//
// yoffset in the .txt file is the distance from the baseline to the TOP of
// the glyph, positive = above the baseline (world y-up).  Cursor x/y is the
// baseline position in world space.
// ============================================================================
struct HudVertex { float x, y, u, v; };

struct GlyphInfo {
    float ax, ay;    // atlas pixel origin (top-left)
    float aw, ah;    // atlas pixel dimensions
    float xoff;      // cursor → glyph left (can be negative)
    float yoff;      // cursor/baseline → glyph TOP, positive = above baseline
    float xadv;      // cursor advance after this glyph
};

struct HudLayer : DrawLayer {
    static constexpr int MAX_VERTS = 6 * 512;

    vk::UniqueImage        atlas_img;
    vk::UniqueDeviceMemory atlas_mem;
    vk::UniqueImageView    atlas_view;
    vk::UniqueSampler      atlas_sampler;
    int                    atlas_w = 512, atlas_h = 512;

    std::unordered_map<int, GlyphInfo> glyphs;

    vk::UniqueDescriptorSetLayout hud_dsl;
    vk::UniquePipelineLayout      hud_pl;
    vk::DescriptorSet             hud_ds{};

    vk::UniquePipeline                pipeline;
    std::array<vku::GenericBuffer, 3> vert_bufs;
    std::array<HudVertex*, 3>         vert_ptrs{};
    std::vector<HudVertex>            pending;

    void load_glyphs(const char* path) {
        FILE* f = fopen(path, "r");
        if (!f) throw std::runtime_error(std::string("cannot open font metrics: ") + path);
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            int id, ax, ay, aw, ah, page, chnl;
            float xoff, yoff, xadv;
            if (sscanf(line,
                "char id=%d x=%d y=%d width=%d height=%d "
                "xoffset=%f yoffset=%f xadvance=%f page=%d chnl=%d",
                &id, &ax, &ay, &aw, &ah, &xoff, &yoff, &xadv, &page, &chnl) == 10) {
                glyphs[id] = { (float)ax, (float)ay, (float)aw, (float)ah,
                               xoff, yoff, xadv };
            }
        }
        fclose(f);
    }

    void init(GpuResources& gpu) {
        auto  dev      = gpu.fw->device();
        auto& memprops = gpu.fw->memprops();

        load_glyphs(BINARY_DIR "FINALNEW.TTF_sdf.txt");

        // Load atlas PNG as single-channel (SDF values in the red/grey channel).
        int w, h, ch;
        unsigned char* px = stbi_load(BINARY_DIR "FINALNEW.TTF_sdf.png", &w, &h, &ch, 1);
        if (!px) throw std::runtime_error("cannot load HUD font atlas PNG");
        atlas_w = w;  atlas_h = h;
        size_t nbytes = (size_t)w * h;

        vku::GenericBuffer staging(dev, memprops,
            vk::BufferUsageFlagBits::eTransferSrc, nbytes,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        staging.updateLocal(dev, px, nbytes);
        stbi_image_free(px);

        // Device-local R8_UNORM image.
        {
            vk::ImageCreateInfo ici;
            ici.imageType     = vk::ImageType::e2D;
            ici.format        = vk::Format::eR8Unorm;
            ici.extent        = vk::Extent3D{ (uint32_t)w, (uint32_t)h, 1 };
            ici.mipLevels     = 1;
            ici.arrayLayers   = 1;
            ici.samples       = vk::SampleCountFlagBits::e1;
            ici.tiling        = vk::ImageTiling::eOptimal;
            ici.usage         = vk::ImageUsageFlagBits::eSampled
                              | vk::ImageUsageFlagBits::eTransferDst;
            ici.initialLayout = vk::ImageLayout::eUndefined;
            atlas_img = dev.createImageUnique(ici);

            auto req = dev.getImageMemoryRequirements(*atlas_img);
            vk::MemoryAllocateInfo mai;
            mai.allocationSize = req.size;
            for (uint32_t i = 0; i < memprops.memoryTypeCount; ++i) {
                if ((req.memoryTypeBits & (1u << i)) &&
                    (memprops.memoryTypes[i].propertyFlags &
                     vk::MemoryPropertyFlagBits::eDeviceLocal)) {
                    mai.memoryTypeIndex = i;  break;
                }
            }
            atlas_mem = dev.allocateMemoryUnique(mai);
            dev.bindImageMemory(*atlas_img, *atlas_mem, 0);

            vk::ImageViewCreateInfo ivci;
            ivci.image            = *atlas_img;
            ivci.viewType         = vk::ImageViewType::e2D;
            ivci.format           = vk::Format::eR8Unorm;
            ivci.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
            atlas_view = dev.createImageViewUnique(ivci);
        }

        // Upload: undefined → transfer-dst, copy, transfer-dst → shader-read.
        vku::executeImmediately(dev, gpu.win->commandPool(), gpu.fw->graphicsQueue(),
            [&](vk::CommandBuffer cb) {
                vk::ImageSubresourceRange sr{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
                vk::ImageMemoryBarrier b;
                b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                b.image               = *atlas_img;
                b.subresourceRange    = sr;

                b.oldLayout     = vk::ImageLayout::eUndefined;
                b.newLayout     = vk::ImageLayout::eTransferDstOptimal;
                b.srcAccessMask = {};
                b.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
                cb.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                                   vk::PipelineStageFlagBits::eTransfer,
                                   {}, {}, {}, b);

                vk::BufferImageCopy region;
                region.imageSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
                region.imageExtent      = vk::Extent3D{ (uint32_t)w, (uint32_t)h, 1 };
                cb.copyBufferToImage(staging.buffer(), *atlas_img,
                                     vk::ImageLayout::eTransferDstOptimal, region);

                b.oldLayout     = vk::ImageLayout::eTransferDstOptimal;
                b.newLayout     = vk::ImageLayout::eShaderReadOnlyOptimal;
                b.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
                b.dstAccessMask = vk::AccessFlagBits::eShaderRead;
                cb.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                   vk::PipelineStageFlagBits::eFragmentShader,
                                   {}, {}, {}, b);
            });

        atlas_sampler = dev.createSamplerUnique(
            vk::SamplerCreateInfo{}
                .setMagFilter(vk::Filter::eLinear)
                .setMinFilter(vk::Filter::eLinear)
                .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeW(vk::SamplerAddressMode::eClampToEdge));

        // Descriptor: set 0 = proj UBO (from gpu.dsl, bound by end_frame),
        //             set 1 = atlas combined-image-sampler.
        hud_dsl = vku::DescriptorSetLayoutMaker{}
            .image(0, vk::DescriptorType::eCombinedImageSampler,
                   vk::ShaderStageFlagBits::eFragment, 1)
            .createUnique(dev);

        hud_pl = vku::PipelineLayoutMaker{}
            .descriptorSetLayout(*gpu.dsl)   // set 0: proj UBO
            .descriptorSetLayout(*hud_dsl)   // set 1: atlas
            .createUnique(dev);

        hud_ds = vku::DescriptorSetMaker{}
            .layout(*hud_dsl)
            .create(dev, gpu.fw->descriptorPool())[0];

        vku::DescriptorSetUpdater{}
            .beginDescriptorSet(hud_ds)
            .beginImages(0, 0, vk::DescriptorType::eCombinedImageSampler)
            .image(*atlas_sampler, *atlas_view, vk::ImageLayout::eShaderReadOnlyOptimal)
            .update(dev);

        // Ring vertex buffers.
        size_t vbytes = MAX_VERTS * sizeof(HudVertex);
        for (int i = 0; i < GpuResources::RING; ++i) {
            vert_bufs[i] = vku::GenericBuffer(dev, memprops,
                vk::BufferUsageFlagBits::eVertexBuffer, vbytes,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
            vert_ptrs[i] = static_cast<HudVertex*>(vert_bufs[i].map(dev));
        }

        vku::ShaderModule vert{dev, BINARY_DIR "gamedemo_hud.vert.spv"};
        vku::ShaderModule frag{dev, BINARY_DIR "gamedemo_hud.frag.spv"};
        pipeline = vku::PipelineMaker{gpu.win->width(), gpu.win->height()}
            .shader(vk::ShaderStageFlagBits::eVertex,   vert)
            .shader(vk::ShaderStageFlagBits::eFragment, frag)
            .vertexBinding(0, sizeof(HudVertex), vk::VertexInputRate::eVertex)
            .vertexAttribute(0, 0, vk::Format::eR32G32Sfloat, 0)                   // pos
            .vertexAttribute(1, 0, vk::Format::eR32G32Sfloat, 2 * sizeof(float))   // uv
            .topology(vk::PrimitiveTopology::eTriangleList)
            .blendBegin(true)
            .blendSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
            .blendDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
            .blendSrcAlphaBlendFactor(vk::BlendFactor::eOne)
            .blendDstAlphaBlendFactor(vk::BlendFactor::eZero)
            .dynamicState(vk::DynamicState::eViewport)
            .dynamicState(vk::DynamicState::eScissor)
            .createUnique(dev, gpu.fw->pipelineCache(), *hud_pl, *gpu.offscreen_rp);
    }

    // Print text at world position (x = left edge, y = baseline).
    // scale: world units per atlas pixel.
    void print(float x, float y, float scale, const char* text) {
        float cx = x;
        for (const char* p = text; *p; ++p) {
            auto it = glyphs.find((unsigned char)*p);
            if (it == glyphs.end()) continue;
            const GlyphInfo& g = it->second;

            float wx0 = cx + g.xoff * scale;
            float wx1 = cx + (g.xoff + g.aw) * scale;
            float wy_top = y + g.yoff * scale;           // above baseline
            float wy_bot = y + (g.yoff - g.ah) * scale;  // may cross below baseline

            float u0 = g.ax / atlas_w,          v0 = g.ay / atlas_h;
            float u1 = (g.ax + g.aw) / atlas_w, v1 = (g.ay + g.ah) / atlas_h;

            if ((int)pending.size() + 6 <= MAX_VERTS) {
                pending.push_back({wx0, wy_top, u0, v0});
                pending.push_back({wx1, wy_top, u1, v0});
                pending.push_back({wx1, wy_bot, u1, v1});
                pending.push_back({wx0, wy_top, u0, v0});
                pending.push_back({wx1, wy_bot, u1, v1});
                pending.push_back({wx0, wy_bot, u0, v1});
            }
            cx += g.xadv * scale;
        }
    }

    void begin_frame() override { pending.clear(); }

    void record(vk::CommandBuffer cb, int slot) override {
        if (pending.empty()) { return; }
        std::memcpy(vert_ptrs[slot], pending.data(), pending.size() * sizeof(HudVertex));
        vk::DeviceSize zero = 0;
        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
        cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *hud_pl, 1, hud_ds, {});
        cb.bindVertexBuffers(0, vert_bufs[slot].buffer(), zero);
        cb.draw((uint32_t)pending.size(), 1, 0, 0);
    }

    void shutdown(vk::Device dev) override {
        pipeline.reset();
        hud_pl.reset();
        hud_dsl.reset();
        atlas_sampler.reset();
        atlas_view.reset();
        atlas_img.reset();
        atlas_mem.reset();
        for (int i = 0; i < GpuResources::RING; ++i) {
            if (vert_ptrs[i]) { vert_bufs[i].unmap(dev); vert_ptrs[i] = nullptr; }
            vert_bufs[i] = {};
        }
    }
};

// ============================================================================
// Utilities
// ============================================================================
static constexpr float HW = 400.f, HH = 300.f;  // world half-extents (= pixel half-extents at 800x600)

static std::mt19937 rng{ std::random_device{}() };

static float randf(float lo, float hi) {
    return std::uniform_real_distribution<float>{lo, hi}(rng);
}

static float wrapf(float v, float lo, float hi) {
    float range = hi - lo;
    while (v <  lo) { v += range; }
    while (v >= hi) { v -= range; }
    return v;
}

static float dist2(glm::vec2 a, glm::vec2 b) {
    glm::vec2 d = a - b;
    return d.x*d.x + d.y*d.y;
}

// ============================================================================
// DustLayer — GPU particle dust driven by a compute shader.
//
// Each frame has two phases:
//   dispatch() [before render pass]: vkCmdDispatch updates all particles,
//              then a pipeline barrier ensures the SSBO write is visible to
//              the vertex shader before the draw — this is where an explicit
//              barrier is required and correct (no render pass context to exploit).
//   record()   [inside render pass]:  each particle drawn as a 2-triangle quad
//              (no VBO — vertex data pulled from SSBO via gl_VertexIndex)
//              with additive blending for a glowing-dust look.
//
// The RING=3 slots are the ping-pong: compute for slot i reads (i-1+RING)%RING
// and writes i; vertex reads i.  The frame fence guarantees slot i is idle
// before it is reused RING frames later, so compute and vertex never race.
// ============================================================================

// Must match std140 Emitter in gamedemo_dust.comp.
struct GpuEmitter {
    float pos[2];
    float vel[2];
    float strength;
    float pad[3];
};
static_assert(sizeof(GpuEmitter) == 32);

struct EmitterUBO {
    GpuEmitter ship;
    GpuEmitter rocks[32];
    GpuEmitter bullets[16];
    int32_t    n_rocks;
    int32_t    n_bullets;
    float      dt;
    float      time;
};

// Must match std430 Particle in gamedemo_dust.comp/vert.
struct GpuParticle {
    float pos[2];
    float vel[2];
    float life;
    float size;
    float pad[2];
};
static_assert(sizeof(GpuParticle) == 32);

struct DustLayer : DrawLayer {
    static constexpr int N          = 2048;
    static constexpr int GROUP_SIZE = 64;
    static constexpr int RING       = GpuResources::RING;

    // One particle buffer per ring slot.  Compute for slot i reads from slot
    // (i-1+RING)%RING and writes to slot i.  The existing frame fence ensures
    // slot i is idle before we reuse it RING frames later, so compute and vertex
    // of different frames never touch the same buffer.
    std::array<vku::GenericBuffer, RING> particle_ssbo;

    std::array<vku::GenericBuffer, RING> emitter_ubo;
    std::array<EmitterUBO*,        RING> emitter_ptrs{};

    // One compute descriptor set per slot: binds prev SSBO, curr SSBO, emitter UBO.
    vk::UniqueDescriptorSetLayout              compute_dsl;
    vk::UniquePipelineLayout                   compute_pl;
    std::array<vk::DescriptorSet, RING>        compute_ds;
    vk::UniquePipeline                         compute_pipeline;

    // One graphics descriptor set per slot: binds that slot's particle SSBO.
    vk::UniqueDescriptorSetLayout          gfx_dsl;
    vk::UniquePipelineLayout               gfx_pl;
    std::array<vk::DescriptorSet, RING>    gfx_ds;
    vk::UniquePipeline                     gfx_pipeline;

    EmitterUBO pending{};

    void set_emitters(const EmitterUBO& e) { pending = e; }

    void init(GpuResources& gpu) {
        auto  dev      = gpu.fw->device();
        auto& memprops = gpu.fw->memprops();
        const size_t bytes = N * sizeof(GpuParticle);

        // Seed all RING particle buffers from the same random data.
        std::vector<GpuParticle> seed(N);
        for (auto& p : seed) {
            p.pos[0] = randf(-HW, HW);  p.pos[1] = randf(-HH, HH);
            p.vel[0] = randf(-8.f, 8.f); p.vel[1] = randf(-8.f, 8.f);
            p.life   = randf(0.1f, 1.0f);
            p.size   = randf(1.f,  2.5f);
        }
        vku::GenericBuffer staging(dev, memprops,
            vk::BufferUsageFlagBits::eTransferSrc, bytes,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        staging.updateLocal(dev, seed.data(), bytes);

        for (int i = 0; i < RING; ++i) {
            particle_ssbo[i] = vku::GenericBuffer(dev, memprops,
                vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
                bytes, vk::MemoryPropertyFlagBits::eDeviceLocal);
        }
        vku::executeImmediately(dev, gpu.win->commandPool(), gpu.fw->graphicsQueue(),
            [&](vk::CommandBuffer cb) {
                for (int i = 0; i < RING; ++i) {
                    cb.copyBuffer(staging.buffer(), particle_ssbo[i].buffer(),
                                  vk::BufferCopy{0, 0, bytes});
                }
            });

        // Ring-buffered emitter UBOs — written from CPU each frame.
        for (int i = 0; i < RING; ++i) {
            emitter_ubo[i] = vku::GenericBuffer(dev, memprops,
                vk::BufferUsageFlagBits::eUniformBuffer, sizeof(EmitterUBO),
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
            emitter_ptrs[i] = static_cast<EmitterUBO*>(emitter_ubo[i].map(dev));
        }

        // Compute DSL: binding 0 = prev SSBO, binding 1 = curr SSBO, binding 2 = emitter UBO.
        compute_dsl = vku::DescriptorSetLayoutMaker{}
            .buffer(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute, 1)
            .buffer(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute, 1)
            .buffer(2, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eCompute, 1)
            .createUnique(dev);

        compute_pl = vku::PipelineLayoutMaker{}
            .descriptorSetLayout(*compute_dsl)
            .createUnique(dev);

        {
            vku::DescriptorSetMaker dsm;
            for (int i = 0; i < RING; ++i) { dsm.layout(*compute_dsl); }
            auto sets = dsm.create(dev, gpu.fw->descriptorPool());
            for (int i = 0; i < RING; ++i) {
                int prev = (i - 1 + RING) % RING;
                compute_ds[i] = sets[i];
                vku::DescriptorSetUpdater{}
                    .beginDescriptorSet(compute_ds[i])
                    .beginBuffers(0, 0, vk::DescriptorType::eStorageBuffer)
                    .buffer(particle_ssbo[prev].buffer(), 0, bytes)   // prev slot → p_in
                    .beginBuffers(1, 0, vk::DescriptorType::eStorageBuffer)
                    .buffer(particle_ssbo[i].buffer(),    0, bytes)   // curr slot → p_out
                    .beginBuffers(2, 0, vk::DescriptorType::eUniformBuffer)
                    .buffer(emitter_ubo[i].buffer(), 0, sizeof(EmitterUBO))
                    .update(dev);
            }
        }

        {
            vku::ShaderModule comp{dev, BINARY_DIR "gamedemo_dust.comp.spv"};
            compute_pipeline = vku::ComputePipelineMaker{}
                .shader(vk::ShaderStageFlagBits::eCompute, comp)
                .createUnique(dev, gpu.fw->pipelineCache(), *compute_pl);
        }

        // Graphics DSL (set 1): one SSBO binding — the slot written by this frame's compute.
        gfx_dsl = vku::DescriptorSetLayoutMaker{}
            .buffer(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eVertex, 1)
            .createUnique(dev);

        gfx_pl = vku::PipelineLayoutMaker{}
            .descriptorSetLayout(*gpu.dsl)   // set 0: proj UBO (bound by end_frame)
            .descriptorSetLayout(*gfx_dsl)   // set 1: particle SSBO
            .createUnique(dev);

        {
            vku::DescriptorSetMaker dsm;
            for (int i = 0; i < RING; ++i) { dsm.layout(*gfx_dsl); }
            auto sets = dsm.create(dev, gpu.fw->descriptorPool());
            for (int i = 0; i < RING; ++i) {
                gfx_ds[i] = sets[i];
                vku::DescriptorSetUpdater{}
                    .beginDescriptorSet(gfx_ds[i])
                    .beginBuffers(0, 0, vk::DescriptorType::eStorageBuffer)
                    .buffer(particle_ssbo[i].buffer(), 0, bytes)
                    .update(dev);
            }
        }

        {
            vku::ShaderModule vert{dev, BINARY_DIR "gamedemo_dust.vert.spv"};
            vku::ShaderModule frag{dev, BINARY_DIR "gamedemo_dust.frag.spv"};
            gfx_pipeline = vku::PipelineMaker{gpu.win->width(), gpu.win->height()}
                .shader(vk::ShaderStageFlagBits::eVertex,   vert)
                .shader(vk::ShaderStageFlagBits::eFragment, frag)
                .topology(vk::PrimitiveTopology::eTriangleList)
                .blendBegin(true)
                .blendDstColorBlendFactor(vk::BlendFactor::eOne)
                .blendDstAlphaBlendFactor(vk::BlendFactor::eOne)
                .dynamicState(vk::DynamicState::eViewport)
                .dynamicState(vk::DynamicState::eScissor)
                .createUnique(dev, gpu.fw->pipelineCache(), *gfx_pl, *gpu.offscreen_rp);
        }
    }

    void begin_frame() override { pending = {}; }

    void dispatch(vk::CommandBuffer cb, int slot) override {
        std::memcpy(emitter_ptrs[slot], &pending, sizeof(EmitterUBO));

        cb.bindPipeline(vk::PipelineBindPoint::eCompute, *compute_pipeline);
        cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *compute_pl, 0, compute_ds[slot], {});
        cb.dispatch(N / GROUP_SIZE, 1, 1);

        // Barrier: compute write to particle_ssbo[slot] must complete before the
        // vertex shader reads it in the same frame.  Scoped to particle_ssbo[slot]
        // only — no other slot is touched by this frame's compute or vertex.
        vk::BufferMemoryBarrier b;
        b.srcAccessMask       = vk::AccessFlagBits::eShaderWrite;
        b.dstAccessMask       = vk::AccessFlagBits::eShaderRead;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.buffer              = particle_ssbo[slot].buffer();
        b.offset              = 0;
        b.size                = N * sizeof(GpuParticle);
        cb.pipelineBarrier(
            vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eVertexShader,
            {}, {}, b, {});
    }

    void record(vk::CommandBuffer cb, int slot) override {
        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, *gfx_pipeline);
        cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *gfx_pl, 1, gfx_ds[slot], {});
        cb.draw(N * 6, 1, 0, 0);
    }

    void shutdown(vk::Device dev) override {
        gfx_pipeline.reset();
        gfx_pl.reset();
        gfx_dsl.reset();
        compute_pipeline.reset();
        compute_pl.reset();
        compute_dsl.reset();
        for (int i = 0; i < RING; ++i) {
            if (emitter_ptrs[i]) { emitter_ubo[i].unmap(dev); emitter_ptrs[i] = nullptr; }
            emitter_ubo[i] = {};
            particle_ssbo[i] = {};
        }
    }
};

// ============================================================================
// ECS core — world, singleton accessors, events
// ============================================================================
static ecs_hpp::registry   world;

static ecs_hpp::entity_id  ship_id{};
static ecs_hpp::entity ship()  noexcept { return {world, ship_id};  }

static ecs_hpp::entity_id  state_id{};
static ecs_hpp::entity state() noexcept { return {world, state_id}; }

struct EvSpawnBullet { glm::vec2 pos, vel; };
struct EvSpawnRock   { glm::vec2 pos; float radius; glm::vec2 parent_vel; };
struct EvDestroy     { ecs_hpp::entity e; };

// ============================================================================
// Ship — components + input system
// ============================================================================
struct Kinematics {
    glm::vec2 pos{}, vel{};
};

struct Orientation {
    float angle = 0;       // radians; 0 = pointing up (+Y)
};

struct Invulnerability {
    float timer = 3.f;     // seconds of spawn protection remaining
    bool is_visible() const { return timer <= 0 || std::fmod(timer, 0.1f) > 0.05f; }
};

struct FireControl {
    float cooldown = 0;    // seconds until next shot allowed
};

struct Alive {};

static void system_ship_input(float dt, const Uint8* keys) {
    if (!ship().exists_component<Alive>()) { return; }

    static constexpr float ROT    = 4.5f;
    static constexpr float THRUST = 420.f;
    static constexpr float DRAG   = 0.985f;

    auto& ori = ship().get_component<Orientation>();
    auto& fc  = ship().get_component<FireControl>();
    auto& inv = ship().get_component<Invulnerability>();
    auto& k   = ship().get_component<Kinematics>();

    // Y-flip in ortho (world +Y = up) inverts apparent rotation direction vs
    // screen-space convention.  Negate so RIGHT key = CW = ship turns right.
    if (keys[SDL_SCANCODE_LEFT])  { ori.angle += ROT * dt; }
    if (keys[SDL_SCANCODE_RIGHT]) { ori.angle -= ROT * dt; }
    if (keys[SDL_SCANCODE_UP]) {
        glm::vec2 fwd{ -sinf(ori.angle), cosf(ori.angle) };
        k.vel += fwd * (THRUST * dt);
    }
    std::optional<EvSpawnBullet> bullet;
    if (keys[SDL_SCANCODE_SPACE] && fc.cooldown <= 0) {
        glm::vec2 dir{ -sinf(ori.angle), cosf(ori.angle) };
        bullet = EvSpawnBullet{k.pos + dir * 20.f, k.vel + dir * 350.f};
        fc.cooldown = 0.18f;
    }

    k.vel   *= std::pow(DRAG, dt * 60.f);
    k.pos   += k.vel * dt;
    k.pos.x  = wrapf(k.pos.x, -HW, HW);
    k.pos.y  = wrapf(k.pos.y, -HH, HH);
    fc.cooldown = std::max(0.f, fc.cooldown - dt);
    inv.timer   = std::max(0.f, inv.timer   - dt);

    if (bullet) { world.process_event(*bullet); }
}

// ============================================================================
// Rocks — components + spawning + physics
// ============================================================================
struct AngularKinematics {
    float angle = 0, spin = 0;
};

struct Size {
    float radius = 0;
};

struct RockSeed {
    float value = 0;
};

static void spawn_rock(glm::vec2 pos, float radius, glm::vec2 parent_vel = {}) {
    auto e = world.create_entity();
    float dir = randf(0.f, 6.2832f), spd = randf(1500.f, 3000.f) / radius;
    glm::vec2 vel = parent_vel + glm::vec2{cosf(dir)*spd, sinf(dir)*spd};
    e.assign_component<Kinematics>() = { .pos = pos, .vel = vel };
    e.assign_component<AngularKinematics>() = {
        .angle = randf(0.f, 6.2832f),
        .spin = randf(-1.5f, 1.5f) };
    e.assign_component<Size>().radius = radius;
    e.assign_component<RockSeed>().value = randf(0.f, 100.f);
}

static void system_rock_physics(float dt) {
    world.for_joined_components<Kinematics, AngularKinematics>([&](auto, Kinematics& k, AngularKinematics& ak) {
        ak.angle += ak.spin * dt;
        k.pos    += k.vel  * dt;
        k.pos.x   = wrapf(k.pos.x, -HW, HW);
        k.pos.y   = wrapf(k.pos.y, -HH, HH);
    });
}

// ============================================================================
// Bullets — components + physics
// ============================================================================
struct Lifetime {
    float life = 0;
};

static void system_bullet_physics(float dt) {
    static std::vector<ecs_hpp::entity> expired;
    expired.clear();
    world.for_joined_components<Kinematics, Lifetime>([&](ecs_hpp::entity e, Kinematics& k, Lifetime& lt) {
        k.pos  += k.vel * dt;
        k.pos.x = wrapf(k.pos.x, -HW, HW);
        k.pos.y = wrapf(k.pos.y, -HH, HH);
        lt.life -= dt;
        if (lt.life <= 0) { expired.push_back(e); }
    });
    for (auto e : expired) { world.process_event(EvDestroy{e}); }
}

// ============================================================================
// Game meta — components + wave spawning + respawn
// ============================================================================
struct GameState {
    int score = 0, lives = 3, wave = 0;
};

struct RespawnTimer {
    float remaining = 0;
};

static void spawn_wave() {
    auto& gs = state().get_component<GameState>();
    ++gs.wave;
    int n = std::min(2 + gs.wave, 6);
    for (int i = 0; i < n; ++i) {
        float ang  = randf(0.f, 6.2832f);
        float dist = randf(150.f, 260.f);
        world.process_event(EvSpawnRock{{ cosf(ang)*dist, sinf(ang)*dist }, 40.f, {}});
    }
}

static void system_respawn(float dt) {
    if (ship().exists_component<Alive>()) { return; }
    auto& rt = state().get_component<RespawnTimer>();
    rt.remaining -= dt;
    if (rt.remaining <= 0 && state().get_component<GameState>().lives > 0) {
        ship().get_component<Kinematics>()      = {};
        ship().get_component<Orientation>()     = {};
        ship().get_component<Invulnerability>() = {};
        ship().get_component<FireControl>()     = {};
        ship().assign_component<Alive>();
    }
}

static void system_wave_check() {
    bool any = false;
    world.for_each_component<Size>([&](auto, auto&){ any = true; });
    if (!any) { spawn_wave(); }
}

// ============================================================================
// Collision — crosses ship/rock/bullet; needs all component types + game state
// ============================================================================
static void system_bullet_rock_collision() {
    using EP = std::pair<ecs_hpp::entity, ecs_hpp::entity>;
    static std::vector<EP>              hits;
    static std::vector<ecs_hpp::entity> killed;
    hits.clear();
    world.for_joined_components<Kinematics, Lifetime>([&](ecs_hpp::entity be, Kinematics& bk, Lifetime&) {
        world.for_joined_components<Kinematics, Size>([&](ecs_hpp::entity re, Kinematics& rk, Size& s) {
            if (dist2(bk.pos, rk.pos) < s.radius * s.radius) {
                hits.push_back({be, re});
            }
        });
    });
    killed.clear();
    for (auto& [be, re] : hits) {
        auto dead = [&](ecs_hpp::entity e) {
            return std::find(killed.begin(), killed.end(), e) != killed.end();
        };
        if (dead(be) || dead(re)) { continue; }
        killed.push_back(be);  killed.push_back(re);
        const auto&     rk     = re.get_component<Kinematics>();
        const float     radius = re.get_component<Size>().radius;
        const glm::vec2 pos    = rk.pos;
        const glm::vec2 vel    = rk.vel;
        state().get_component<GameState>().score += radius > 20.f ? 100 : 200;
        if (radius > 20.f) {
            world.process_event(EvSpawnRock{pos, radius * 0.5f, vel});
            world.process_event(EvSpawnRock{pos, radius * 0.5f, vel});
        }
        world.process_event(EvDestroy{be});
        world.process_event(EvDestroy{re});
    }
}

static void system_ship_rock_collision() {
    if (!ship().exists_component<Alive>()) { return; }
    if (ship().get_component<Invulnerability>().timer > 0) { return; }
    const glm::vec2 ship_pos = ship().get_component<Kinematics>().pos;
    bool hit = false;
    world.for_joined_components<Kinematics, Size>([&](auto, const Kinematics& k, const Size& s) {
        if (!hit && dist2(ship_pos, k.pos) < (s.radius + 14.f) * (s.radius + 14.f)) {
            hit = true;
        }
    });
    if (hit) {
        ship().remove_component<Alive>();
        state().get_component<RespawnTimer>().remaining = 2.f;
        --state().get_component<GameState>().lives;
    }
}

// ============================================================================
// Event systems — registered at world-reset; SpawnRock calls spawn_rock above
// ============================================================================
struct DestroySystem : ecs_hpp::system<EvDestroy> {
    void process(ecs_hpp::registry&, const EvDestroy& ev) override {
        auto e = ev.e;
        e.destroy();
    }
};
struct SpawnBulletSystem : ecs_hpp::system<EvSpawnBullet> {
    void process(ecs_hpp::registry&, const EvSpawnBullet& ev) override {
        auto e = world.create_entity();
        e.assign_component<Kinematics>() = {.pos=ev.pos, .vel=ev.vel};
        e.assign_component<Lifetime>().life = 1.5f;
    }
};
struct SpawnRockSystem : ecs_hpp::system<EvSpawnRock> {
    void process(ecs_hpp::registry&, const EvSpawnRock& ev) override {
        spawn_rock(ev.pos, ev.radius, ev.parent_vel);
    }
};

static void register_event_handlers() {
    world.assign_feature<DestroySystem>().add_system<DestroySystem>();
    world.assign_feature<SpawnBulletSystem>().add_system<SpawnBulletSystem>();
    world.assign_feature<SpawnRockSystem>().add_system<SpawnRockSystem>();
}

// ============================================================================
// Layers — one per object type; mesh + pipeline owned here, not in ECS
// ============================================================================
static InstancedLayer ship_layer, bullet_layer;
static RockLayer      rock_layer;
static DustLayer      dust_layer;
static HudLayer       hud_layer;

// ============================================================================
// Render systems — queue instances into their layer; must follow layer defs
// ============================================================================
static void system_render_ship() {
    if (!ship().exists_component<Alive>()) { return; }
    if (!ship().get_component<Invulnerability>().is_visible()) { return; }
    const auto& k   = ship().get_component<Kinematics>();
    const float ang = ship().get_component<Orientation>().angle;
    ship_layer.queue(k.pos.x, k.pos.y, ang, 1.f, 0.f);
}

static void system_render_rocks() {
    world.for_joined_components<Kinematics, AngularKinematics, Size, RockSeed>([](auto, const Kinematics& k, const AngularKinematics& ak, const Size& s, const RockSeed& rs) {
        rock_layer.queue(k.pos.x, k.pos.y, ak.angle, s.radius, rs.value);
    });
}

static void system_render_bullets() {
    world.for_joined_components<Kinematics, Lifetime>([](auto, const Kinematics& k, const Lifetime&) {
        bullet_layer.queue(k.pos.x, k.pos.y, 0.f, 1.f, 2.f);
    });
}

static void system_fill_emitters(float dt) {
    EmitterUBO em{};
    em.dt   = dt;
    em.time = SDL_GetTicks() / 1000.0f;

    if (ship().exists_component<Alive>()) {
        const auto& k    = ship().get_component<Kinematics>();
        em.ship.pos[0]   = k.pos.x;  em.ship.pos[1]   = k.pos.y;
        em.ship.vel[0]   = k.vel.x;  em.ship.vel[1]   = k.vel.y;
        em.ship.strength = glm::length(k.vel) * 1.0f + 40.0f;
    }

    world.for_joined_components<Kinematics, AngularKinematics, Size, RockSeed>(
        [&](auto, const Kinematics& k, const AngularKinematics&, const Size& s, const RockSeed&) {
            if (em.n_rocks < 32) {
                GpuEmitter& e = em.rocks[em.n_rocks++];
                e.pos[0] = k.pos.x;  e.pos[1] = k.pos.y;
                e.vel[0] = k.vel.x;  e.vel[1] = k.vel.y;
                e.strength = s.radius * 1.0f + 20.0f;
            }
        });

    world.for_joined_components<Kinematics, Lifetime>(
        [&](auto, const Kinematics& k, const Lifetime&) {
            if (em.n_bullets < 16) {
                GpuEmitter& e = em.bullets[em.n_bullets++];
                e.pos[0] = k.pos.x;  e.pos[1] = k.pos.y;
                e.vel[0] = k.vel.x;  e.vel[1] = k.vel.y;
                e.strength = 70.0f;
            }
        });

    dust_layer.set_emitters(em);
}

static void system_render_hud() {
    const auto& gs = state().get_component<GameState>();
    char buf[80];
    if (gs.lives > 0)
        std::snprintf(buf, sizeof(buf), "SCORE: %d   LIVES: %d   WAVE: %d", gs.score, gs.lives, gs.wave);
    else
        std::snprintf(buf, sizeof(buf), "SCORE: %d   GAME OVER   PRESS [R] TO RESPAWN", gs.score);
    hud_layer.print(-390.f, 268.f, 0.38f, buf);
}

static void update_graphics(float dt) {
    system_render_ship();
    system_render_rocks();
    system_render_bullets();
    system_fill_emitters(dt);
    system_render_hud();
}

// ============================================================================
// Game initialization + main loop functions
// ============================================================================
static void reset_game() {
    world = {};
    register_event_handlers();

    auto se = world.create_entity();
    ship_id = se.id();
    se.assign_component<Kinematics>();
    se.assign_component<Orientation>();
    se.assign_component<Invulnerability>();
    se.assign_component<FireControl>();
    se.assign_component<Alive>();

    auto st = world.create_entity();
    state_id = st.id();
    st.assign_component<GameState>();
    st.assign_component<RespawnTimer>();

    spawn_wave();
}

static void update(float dt, const Uint8* keys) {
    system_respawn(dt);
    system_ship_input(dt, keys);
    system_bullet_physics(dt);
    system_bullet_rock_collision();
    system_ship_rock_collision();
    system_wave_check();
    system_rock_physics(dt);
}

// ============================================================================
// main
// ============================================================================
int main() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_Window* sdlWindow = SDL_CreateWindow(
        "gamedemo (Vookoo)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_VULKAN
    );
    if (!sdlWindow) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    {
        GpuResources gpu;
        gpu.init(sdlWindow);
        gpu.reshape(800, 600);

        // Layer registration order = draw order (no depth buffer).
        // Dust is first so solid objects (ship, rock, bullet) paint over it.

        dust_layer.init(gpu);
        gpu.add_layer(&dust_layer);

        // Ship: pointed arrowhead outline — 4 segments
        {
            glm::vec2 pts[] = {
                { 0, 20}, {11,-12},    // nose -> right wing
                {11,-12}, { 0, -5},    // right wing -> center notch
                { 0, -5}, {-11,-12},   // center notch -> left wing
                {-11,-12}, { 0, 20},   // left wing -> nose
            };
            ship_layer.init(gpu, BINARY_DIR "gamedemo.vert.spv", BINARY_DIR "gamedemo.frag.spv", pts, 8);
            gpu.add_layer(&ship_layer);
        }
        rock_layer.init(gpu);
        gpu.add_layer(&rock_layer);
        // Bullet: small diamond — 4 segments
        {
            glm::vec2 pts[] = {
                { 0, 5}, { 3, 0},
                { 3, 0}, { 0,-5},
                { 0,-5}, {-3, 0},
                {-3, 0}, { 0, 5},
            };
            bullet_layer.init(gpu, BINARY_DIR "gamedemo.vert.spv", BINARY_DIR "gamedemo.frag.spv", pts, 8);
            gpu.add_layer(&bullet_layer);
        }

        hud_layer.init(gpu);
        gpu.add_layer(&hud_layer);

        reset_game();

        auto prev      = std::chrono::high_resolution_clock::now();
        bool running   = true;
        bool minimized = false;

        while (running) {
            SDL_Event ev;
            while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_QUIT) { running = false; }
                if (ev.type == SDL_KEYDOWN) {
                    if (ev.key.keysym.sym == SDLK_ESCAPE) { running = false; }
                    if (ev.key.keysym.sym == SDLK_r && state().get_component<GameState>().lives <= 0) { reset_game(); }
                }
                if (ev.type == SDL_WINDOWEVENT) {
                    if (ev.window.event == SDL_WINDOWEVENT_MINIMIZED) { minimized = true;  }
                    if (ev.window.event == SDL_WINDOWEVENT_RESTORED)  { minimized = false; }
                }
            }

            if (minimized) {
                prev = std::chrono::high_resolution_clock::now();
                SDL_Delay(16);
                continue;
            }

            auto now = std::chrono::high_resolution_clock::now();
            float dt = std::min(std::chrono::duration<float>(now - prev).count(), 1.0f / 30.0f);
            prev = now;

            update(dt, SDL_GetKeyboardState(nullptr));

            gpu.begin_frame();
            update_graphics(dt);
            gpu.end_frame();
        }

        gpu.shutdown();
    }

    SDL_DestroyWindow(sdlWindow);
    SDL_Quit();
    return 0;
}
