#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <print>
#include <vector>
#include "stb_image.h"
#include "stb_image_resize.h"

#define WIDTH 960
#define HEIGHT 720
#define RED 0xFF0000
#define GREEN 0x00FF00
#define BLUE 0x0000FF
#define WHITE 0xFFFFFF
#define BLACK 0x000000
#define MAGENTA 0xFF00FF
#define CYAN 0x00FFFF
#define YELLOW 0xFFFF00

typedef enum
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
}
color_t;

typedef struct
{
    float x;
    float y;
    float angle;
    uint32_t color;
}
agent_t;

static SDL_Window* window;
static SDL_GPUDevice* device;
static SDL_GPUComputePipeline* update_pipeline;
static SDL_GPUGraphicsPipeline* diffuse_pipeline;
static SDL_GPUGraphicsPipeline* draw_pipeline;
static SDL_GPUBuffer* agent_buffer;
static SDL_GPUTexture* trail_texture;
static std::vector<agent_t> agents;

static bool load_image(const char* path)
{
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
    if (!stbir_resize_uint8(
        src,
        width,
        height,
        0,
        dst,
        WIDTH,
        HEIGHT,
        0,
        channels))
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
        agents.emplace_back(
            static_cast<float>(x),
            static_cast<float>(y),
            0.0f, result);
    }
    free(dst);
    return true;
}

static bool init()
{
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
    SDL_SetAppMetadata("slime_molds", NULL, NULL);
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::println("Failed to initialize SDL: {}", SDL_GetError());
        return false;
    }
    window = SDL_CreateWindow("slime_molds", 960, 540, SDL_WINDOW_RESIZABLE);
    if (!window)
    {
        std::println("Failed to create window: {}", SDL_GetError());
        return false;
    }
    device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, NULL);
    if (!device)
    {
        std::println("Failed to create device: {}", SDL_GetError());
        return false;
    }
    if (!SDL_ClaimWindowForGPUDevice(device, window))
    {
        std::println("Failed to create swapchain: {}", SDL_GetError());
        return false;
    }
    return true;
}

static void draw()
{
    SDL_WaitForGPUSwapchain(device, window);
    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(device);
    if (!command_buffer)
    {
        std::println("Failed to acquire command buffer: {}", SDL_GetError());
        return;
    }
    SDL_GPUTexture* texture;
    if (!SDL_AcquireGPUSwapchainTexture(command_buffer, window, &texture, NULL, NULL))
    {
        std::println("Failed to acquire swapchain texture: {}", SDL_GetError());
        SDL_CancelGPUCommandBuffer(command_buffer);
        return;
    }
    SDL_SubmitGPUCommandBuffer(command_buffer);
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::println("No image provided");
        return 1;
    }
    if (!load_image(argv[1]))
    {
        std::println("Failed to load image");
        return 1;
    }
    if (!init())
    {
        std::println("Failed to init");
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
        draw();
    }
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}