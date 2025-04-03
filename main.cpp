#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <print>
#include <vector>
#include "config.h"
#include "stb_image.h"
#include "stb_image_resize.h"

enum Color : uint32_t
{
    COLOR_RED,
    COLOR_GREEN,
    COLOR_BLUE,
    COLOR_WHITE,
    COLOR_BLACK,
    COLOR_MAGENTA,
    COLOR_CYAN,
    COLOR_YELLOW,
    COLOR_COUNT,
};

struct Agent
{
    float x;
    float y;
    float angle;
    Color color;
};

static SDL_Window* window;
static SDL_GPUDevice* device;
static SDL_GPUComputePipeline* update_pipeline;
static SDL_GPUGraphicsPipeline* blur_pipeline;
static SDL_GPUGraphicsPipeline* draw_pipeline;
static SDL_GPUBuffer* agent_buffer;
static SDL_GPUTexture* trail_textures[2];
static SDL_GPUSampler* sampler;
static int frame;

static bool reload(const char* path)
{
    SDL_ReleaseGPUBuffer(device, agent_buffer);
    for (int i = 0; i < FRAMES; i++)
    {
        SDL_ReleaseGPUTexture(device, trail_textures[i]);
        trail_textures[i] = nullptr;
    }
    agent_buffer = nullptr;
    int channels;
    int width;
    int height;
    stbi_uc* src = stbi_load(path, &width, &height, &channels, 3);
    if (!src)
    {
        std::println("Failed to load image: {}", path);
        return false;
    }
    channels = 3;
    stbi_uc* dst = static_cast<stbi_uc*>(malloc(WIDTH * HEIGHT * channels));
    if (!dst)
    {
        std::println("Failed to allocate image");
        return false;
    }
    if (!stbir_resize_uint8(src, width, height, 0, dst, WIDTH, HEIGHT, 0, channels))
    {
        std::println("Failed to resize image");
        return false;
    }
    stbi_image_free(src);
    constexpr uint32_t colors[COLOR_COUNT] =
    {
        0xFF0000, /* RED */
        0x00FF00, /* GREEN */
        0x0000FF, /* BLUE */
        0xFFFFFF, /* WHITE */
        0x000000, /* BLACK */
        0xFF00FF, /* MAGENTA */
        0x00FFFF, /* CYAN */
        0xFFFF00, /* YELLOW */
    };
    std::vector<Agent> agents;
    for (uint32_t x = 0; x < WIDTH; x++)
    for (uint32_t y = 0; y < HEIGHT; y++)
    {
        const uint32_t index = (y * WIDTH + x) * channels;
        uint32_t color1 = 0;
        color1 |= dst[index + 0] << 0;
        color1 |= dst[index + 1] << 8;
        color1 |= dst[index + 2] << 16;
        double distance1 = DBL_MAX;
        uint32_t result = UINT32_MAX;
        for (uint32_t i = 0; i < COLOR_COUNT; i++)
        {
            const uint32_t color2 = colors[i];
            const int r1 = (color1 >> 0) & 0xFF;
            const int g1 = (color1 >> 8) & 0xFF;
            const int b1 = (color1 >> 16) & 0xFF;
            const int r2 = (color2 >> 0) & 0xFF;
            const int g2 = (color2 >> 8) & 0xFF;
            const int b2 = (color2 >> 16) & 0xFF;
            const double distance2 =
                (r1 - r2) * (r1 - r2) + 
                (g1 - g2) * (g1 - g2) + 
                (b1 - b2) * (b1 - b2);
            if (distance2 < distance1)
            {
                distance1 = distance2;
                result = i;
            }
        }
        agents.emplace_back(static_cast<float>(x), static_cast<float>(y), 0.0f, Color(result));
    }
    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(device);
    if (!command_buffer)
    {
        std::println("Failed to acquire command buffer: {}", SDL_GetError());
        return false;
    }
    SDL_GPUBufferCreateInfo buffer_info{};
    buffer_info.size = agents.size() * sizeof(Agent);
    buffer_info.usage =
        SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ |
        SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
    agent_buffer = SDL_CreateGPUBuffer(device, &buffer_info);
    if (!agent_buffer)
    {
        std::println("Failed to create buffer: {}", SDL_GetError());
        return false;
    }
    SDL_GPUTransferBufferCreateInfo transfer_buffer_info{};
    transfer_buffer_info.size = buffer_info.size;
    transfer_buffer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    SDL_GPUTransferBuffer* transfer_buffer = SDL_CreateGPUTransferBuffer(device, &transfer_buffer_info);
    if (!transfer_buffer)
    {
        std::println("Failed to create transfer buffer: {}", SDL_GetError());
        return false;
    }
    void* data = SDL_MapGPUTransferBuffer(device, transfer_buffer, false);
    if (!data)
    {
        std::println("Failed to map transfer buffer: {}", SDL_GetError());
        return false;
    }
    std::memcpy(data, agents.data(), buffer_info.size);
    SDL_UnmapGPUTransferBuffer(device, transfer_buffer);
    SDL_GPUTransferBufferLocation location{};
    SDL_GPUBufferRegion region{};
    location.transfer_buffer = transfer_buffer;
    region.buffer = agent_buffer;
    region.size = buffer_info.size;
    SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(command_buffer);
    if (!copy_pass)
    {
        std::println("Failed to begin copy pass: {}", SDL_GetError());
        return false;
    }
    SDL_UploadToGPUBuffer(copy_pass, &location, &region, false);
    SDL_EndGPUCopyPass(copy_pass);
    SDL_ReleaseGPUTransferBuffer(device, transfer_buffer);
    SDL_GPUTextureCreateInfo texture_info{};
    texture_info.type = SDL_GPU_TEXTURETYPE_2D;
    texture_info.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_UINT;
    texture_info.usage =
        SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ |
        SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE |
        SDL_GPU_TEXTUREUSAGE_SAMPLER |
        SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    texture_info.width = WIDTH;
    texture_info.height = HEIGHT;
    texture_info.layer_count_or_depth = 1;
    texture_info.num_levels = 1;
    for (int i = 0; i < FRAMES; i++)
    {
        trail_textures[i] = SDL_CreateGPUTexture(device, &texture_info);
        if (!trail_textures[i])
        {
            std::println("Failed to create texture: {}", SDL_GetError());
            return false;
        }
        SDL_GPUColorTargetInfo target_info{};
        target_info.texture = trail_textures[i];
        target_info.load_op = SDL_GPU_LOADOP_CLEAR;
        target_info.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(command_buffer, &target_info, 1, nullptr);
        if (!render_pass)
        {
            std::println("Failed to begin render pass: {}", SDL_GetError());
            return false;
        }
        SDL_EndGPURenderPass(render_pass);
    }
    SDL_SubmitGPUCommandBuffer(command_buffer);
    free(dst);
    return true;
}

int main(int argc, char** argv)
{
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
    SDL_SetAppMetadata("png2slime", nullptr, nullptr);
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::println("Failed to initialize SDL: {}", SDL_GetError());
        return 1;
    }
    window = SDL_CreateWindow("png2slime", 960, 540, SDL_WINDOW_RESIZABLE);
    if (!window)
    {
        std::println("Failed to create window: {}", SDL_GetError());
        return 1;
    }
    device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, nullptr);
    if (!device)
    {
        std::println("Failed to create device: {}", SDL_GetError());
        return 1;
    }
    if (!SDL_ClaimWindowForGPUDevice(device, window))
    {
        std::println("Failed to create swapchain: {}", SDL_GetError());
        return 1;
    }
    const auto load = [](
        const char* path,
        const int uniforms,
        const int samplers,
        const int buffers,
        const int textures)
    {
        SDL_GPUShader* shader = nullptr;
        SDL_GPUShaderCreateInfo info{};
        void* code = SDL_LoadFile(path, &info.code_size);
        if (!code)
        {
            std::println("Failed to load shader: {}, {}", path, SDL_GetError());
            return shader;
        }
        info.code = static_cast<uint8_t*>(code);
        info.num_uniform_buffers = uniforms;
        info.num_samplers = samplers;
        info.num_storage_buffers = buffers;
        info.num_storage_textures = textures;
        if (strstr(path, ".vert"))
        {
            info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
        }
        else
        {
            info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
        }
        info.format = SDL_GPU_SHADERFORMAT_SPIRV;
        info.entrypoint = "main";
        shader = SDL_CreateGPUShader(device, &info);
        SDL_free(code);
        if (!shader)
        {
            std::println("Failed to create shader: {}, {}", path, SDL_GetError());
        }
        return shader;
    };
    SDL_GPUShader* draw_shader = load("draw.frag", 0, 1, 0, 0);
    SDL_GPUShader* blur_shader = load("blur.frag", 0, 0, 0, 0);
    SDL_GPUShader* quad_shader = load("quad.vert", 0, 0, 0, 0);
    if (!blur_shader || !draw_shader || !quad_shader)
    {
        std::println("Failed to load shader(s)");
        return false;
    }
    SDL_GPUColorTargetDescription draw_target =
    {
        .format = SDL_GetGPUSwapchainTextureFormat(device, window),
        .blend_state =
        {
            .src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
            .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            .color_blend_op = SDL_GPU_BLENDOP_ADD,
            .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
            .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
            .enable_blend = true,
        }
    };
    SDL_GPUColorTargetDescription blur_target =
    {
        .format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_UINT
    };
    SDL_GPUGraphicsPipelineCreateInfo draw_pipeline_info =
    {
        .vertex_shader = quad_shader,
        .fragment_shader = draw_shader,
        .target_info =
        {
            .color_target_descriptions = &draw_target,
            .num_color_targets = 1,
        },
    };
    SDL_GPUGraphicsPipelineCreateInfo blur_pipeline_info =
    {
        .vertex_shader = quad_shader,
        .fragment_shader = blur_shader,
        .target_info =
        {
            .color_target_descriptions = &blur_target,
            .num_color_targets = 1,
        },
    };
    draw_pipeline = SDL_CreateGPUGraphicsPipeline(device, &draw_pipeline_info);
    if (!draw_pipeline)
    {
        std::println("Failed to create draw pipeline: {}", SDL_GetError());
        return 1;
    }
    blur_pipeline = SDL_CreateGPUGraphicsPipeline(device, &blur_pipeline_info);
    if (!blur_pipeline)
    {
        std::println("Failed to create blur pipeline: {}", SDL_GetError());
        return 1;
    }
    SDL_ReleaseGPUShader(device, blur_shader);
    SDL_ReleaseGPUShader(device, draw_shader);
    SDL_ReleaseGPUShader(device, quad_shader);
    SDL_GPUComputePipelineCreateInfo update_pipeline_info{};
    void* code = SDL_LoadFile("update.comp", &update_pipeline_info.code_size);
    if (!code)
    {
        std::println("Failed to load shader: {}, {}", "update.comp", SDL_GetError());
        return 1;
    }
    update_pipeline_info.code = static_cast<uint8_t*>(code);
    update_pipeline_info.threadcount_x = THREADS_X;
    update_pipeline_info.threadcount_y = THREADS_Y;
    update_pipeline_info.threadcount_z = THREADS_Z;
    update_pipeline_info.num_readonly_storage_buffers = 0;
    update_pipeline_info.num_readwrite_storage_buffers = 1;
    update_pipeline_info.num_readonly_storage_textures = 1;
    update_pipeline_info.num_readwrite_storage_textures = 1;
    update_pipeline_info.num_samplers = 0;
    update_pipeline_info.num_uniform_buffers = 0;
    update_pipeline_info.entrypoint = "main";
    update_pipeline_info.format = SDL_GPU_SHADERFORMAT_SPIRV;
    update_pipeline = SDL_CreateGPUComputePipeline(device, &update_pipeline_info);
    if (!update_pipeline)
    {
        std::println("Failed to create update pipeline: {}", SDL_GetError());
        return 1;
    }
    SDL_GPUSamplerCreateInfo sampler_info{};
    sampler_info.min_filter = SDL_GPU_FILTER_NEAREST;
    sampler_info.mag_filter = SDL_GPU_FILTER_NEAREST;
    sampler_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler = SDL_CreateGPUSampler(device, &sampler_info);
    if (!sampler)
    {
        std::println("Failed to create sampler: {}", SDL_GetError());
        return 1;
    }
    if (argc >= 2 && !reload(argv[1]))
    {
        std::println("Failed to load image");
        return 1;
    }
    bool running = true;
    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            }
        }
        SDL_WaitForGPUSwapchain(device, window);
        SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(device);
        if (!command_buffer)
        {
            std::println("Failed to acquire command buffer: {}", SDL_GetError());
            continue;
        }
        SDL_GPUTexture* texture;
        if (!SDL_AcquireGPUSwapchainTexture(command_buffer, window, &texture, nullptr, nullptr))
        {
            std::println("Failed to acquire swapchain texture: {}", SDL_GetError());
            SDL_CancelGPUCommandBuffer(command_buffer);
            continue;
        }
        const int read_frame = frame;
        const int write_frame = (frame + 1) % FRAMES;
        {
            SDL_PushGPUDebugGroup(command_buffer, "update");
            SDL_GPUStorageBufferReadWriteBinding buffer_binding{};
            buffer_binding.buffer = agent_buffer;
            SDL_GPUStorageTextureReadWriteBinding texture_binding{};
            texture_binding.texture = trail_textures[write_frame];
            SDL_GPUComputePass* compute_pass = SDL_BeginGPUComputePass(command_buffer, &texture_binding, 1, &buffer_binding, 1);
            if (!compute_pass)
            {
                SDL_PopGPUDebugGroup(command_buffer);
                SDL_CancelGPUCommandBuffer(command_buffer);
                std::println("Failed to begin update pass: {}", SDL_GetError());
                continue;
            }
            SDL_BindGPUComputePipeline(compute_pass, update_pipeline);
            SDL_BindGPUComputeStorageTextures(compute_pass, 0, &trail_textures[read_frame], 1);
            SDL_DispatchGPUCompute(compute_pass, (WIDTH + THREADS_X - 1) / THREADS_X, (HEIGHT + THREADS_Y - 1) / THREADS_Y, 1);
            SDL_EndGPUComputePass(compute_pass);
            SDL_PopGPUDebugGroup(command_buffer);
        }
        {
            SDL_PushGPUDebugGroup(command_buffer, "draw");
            SDL_GPUColorTargetInfo info{};
            info.texture = texture;
            info.load_op = SDL_GPU_LOADOP_CLEAR;
            info.store_op = SDL_GPU_STOREOP_STORE;
            SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(command_buffer, &info, 1, nullptr);
            if (!render_pass)
            {
                SDL_PopGPUDebugGroup(command_buffer);
                SDL_CancelGPUCommandBuffer(command_buffer);
                std::println("Failed to begin draw pass: {}", SDL_GetError());
                continue;
            }
            SDL_GPUTextureSamplerBinding binding{};
            binding.texture = trail_textures[write_frame];
            binding.sampler = sampler;
            SDL_BindGPUGraphicsPipeline(render_pass, draw_pipeline);
            SDL_BindGPUFragmentSamplers(render_pass, 0, &binding, 1);
            SDL_DrawGPUPrimitives(render_pass, 4, 1, 0, 0);
            SDL_EndGPURenderPass(render_pass);
            SDL_PopGPUDebugGroup(command_buffer);
        }
        SDL_SubmitGPUCommandBuffer(command_buffer);
        frame = write_frame;
    }
    SDL_ReleaseGPUBuffer(device, agent_buffer);
    for (int i = 0; i < FRAMES; i++)
    {
        SDL_ReleaseGPUTexture(device, trail_textures[i]);
    }
    SDL_ReleaseGPUSampler(device, sampler);
    SDL_ReleaseGPUComputePipeline(device, update_pipeline);
    SDL_ReleaseGPUGraphicsPipeline(device, draw_pipeline);
    SDL_ReleaseGPUGraphicsPipeline(device, blur_pipeline);
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}