//
// RT64
//

#include "rhi/rt64_render_interface.h"

#include <cassert>
#include <cstring>
#include <chrono>
#include <functional>
#include <SDL.h>
#include <SDL_syswm.h>
#include <thread>
#include <random>

#ifdef _WIN64
#include "shaders/RenderInterfaceTestPS.hlsl.dxil.h"
#include "shaders/RenderInterfaceTestRT.hlsl.dxil.h"
#include "shaders/RenderInterfaceTestVS.hlsl.dxil.h"
#include "shaders/RenderInterfaceTestCS.hlsl.dxil.h"
#include "shaders/RenderInterfaceTestPostPS.hlsl.dxil.h"
#include "shaders/RenderInterfaceTestPostVS.hlsl.dxil.h"
#endif
#include "shaders/RenderInterfaceTestPS.hlsl.spirv.h"
#ifndef __APPLE__
#include "shaders/RenderInterfaceTestRT.hlsl.spirv.h"
#endif
#include "shaders/RenderInterfaceTestVS.hlsl.spirv.h"
#include "shaders/RenderInterfaceTestCS.hlsl.spirv.h"
#include "shaders/RenderInterfaceTestPostPS.hlsl.spirv.h"
#include "shaders/RenderInterfaceTestPostVS.hlsl.spirv.h"
#ifdef __APPLE__
#include "shaders/RenderInterfaceTestPS.hlsl.metal.h"
// TODO: Enable when RT is added to Metal.
//#include "shaders/RenderInterfaceTestRT.hlsl.metal.h"
#include "shaders/RenderInterfaceTestVS.hlsl.metal.h"
#include "shaders/RenderInterfaceTestCS.hlsl.metal.h"
#include "shaders/RenderInterfaceTestPostPS.hlsl.metal.h"
#include "shaders/RenderInterfaceTestPostVS.hlsl.metal.h"
#endif

namespace RT64 {
    struct CheckeredTextureGenerator {
        static std::vector<uint8_t> generateCheckeredData(uint32_t width, uint32_t height) {
            std::vector<uint8_t> textureData(width * height * 4);
            const uint32_t squareSize = 32; // Size of each checker square
            
            for (uint32_t y = 0; y < height; y++) {
                for (uint32_t x = 0; x < width; x++) {
                    uint32_t index = (y * width + x) * 4;
                    bool isWhite = ((x / squareSize) + (y / squareSize)) % 2 == 0;
                    uint8_t pixelValue = isWhite ? 255 : 0;
                    
                    textureData[index + 0] = pixelValue;  // R
                    textureData[index + 1] = pixelValue;  // G
                    textureData[index + 2] = pixelValue;  // B
                    textureData[index + 3] = 255;         // A
                }
            }
            
            return textureData;
        }
    };
    
    struct TestBase {
        static const uint32_t BufferCount = 2;
        static const RenderFormat SwapchainFormat = RenderFormat::B8G8R8A8_UNORM;
        static const uint32_t MSAACount = 4;
        static const RenderFormat ColorFormat = RenderFormat::R8G8B8A8_UNORM;
        static const RenderFormat DepthFormat = RenderFormat::D32_FLOAT;
        
        const RenderInterface* renderInterface = nullptr;
        std::unique_ptr<RenderDevice> device;
        std::unique_ptr<RenderCommandQueue> commandQueue;
        std::unique_ptr<RenderCommandList> commandList;
        std::unique_ptr<RenderCommandSemaphore> acquireSemaphore;
        std::unique_ptr<RenderCommandSemaphore> drawSemaphore;
        std::unique_ptr<RenderCommandFence> commandFence;
        std::unique_ptr<RenderSwapChain> swapChain;
        std::vector<std::unique_ptr<RenderFramebuffer>> swapFramebuffers;

        virtual void initialize(RenderInterface *renderInterface, RenderWindow window) {
            this->renderInterface = renderInterface;
            device = renderInterface->createDevice();
            commandQueue = device->createCommandQueue(RenderCommandListType::DIRECT);
            commandList = commandQueue->createCommandList(RenderCommandListType::DIRECT);
            acquireSemaphore = device->createCommandSemaphore();
            drawSemaphore = device->createCommandSemaphore();
            commandFence = device->createCommandFence();
            swapChain = commandQueue->createSwapChain(window, BufferCount, SwapchainFormat);
            resize();
        }

        virtual void resize() {
            if (!swapChain) return;
            swapFramebuffers.clear();
            swapChain->resize();
            swapFramebuffers.resize(swapChain->getTextureCount());
            for (uint32_t i = 0; i < swapChain->getTextureCount(); i++) {
                const RenderTexture* curTex = swapChain->getTexture(i);
                swapFramebuffers[i] = device->createFramebuffer(RenderFramebufferDesc{&curTex, 1});
            }
        }

        virtual void draw() = 0;
        
        virtual void shutdown() {
            swapFramebuffers.clear();
            commandList.reset();
            drawSemaphore.reset();
            acquireSemaphore.reset();
            commandFence.reset();
            swapChain.reset();
            commandQueue.reset();
            device.reset();
        }
    };

    struct ClearTest : TestBase {
        void draw() override {
            // Get swap chain texture index
            uint32_t swapChainTextureIndex = 0;
            swapChain->acquireTexture(acquireSemaphore.get(), &swapChainTextureIndex);
            RenderTexture* swapChainTexture = swapChain->getTexture(swapChainTextureIndex);
            RenderFramebuffer* swapFramebuffer = swapFramebuffers[swapChainTextureIndex].get();

            // Begin command list
            commandList->begin();

            // Set viewport and scissor
            const uint32_t width = swapChain->getWidth();
            const uint32_t height = swapChain->getHeight();
            const RenderViewport viewport(0.0f, 0.0f, float(width), float(height));
            const RenderRect scissor(0, 0, width, height);
            commandList->setViewports(viewport);
            commandList->setScissors(scissor);

            // Transition texture to color write layout and clear
            commandList->barriers(RenderBarrierStage::GRAPHICS, 
                RenderTextureBarrier(swapChainTexture, RenderTextureLayout::COLOR_WRITE));
            commandList->setFramebuffer(swapFramebuffer);
            
            // Clear full screen
            commandList->clearColor(0, RenderColor(0.0f, 0.0f, 0.5f)); // Clear to blue
            
            // Clear with rects
            std::vector<RenderRect> clearRects = {
                {0, 0, 100, 100},
                {200, 200, 100, 100},
                {400, 400, 100, 100}
            };
            commandList->clearColor(0, RenderColor(0.0f, 1.0f, 0.5f), clearRects.data(), clearRects.size()); // Clear to green

            // Transition texture to present layout
            commandList->barriers(RenderBarrierStage::NONE, 
                RenderTextureBarrier(swapChainTexture, RenderTextureLayout::PRESENT));

            // End and submit command list
            commandList->end();

            const RenderCommandList* cmdList = commandList.get();
            RenderCommandSemaphore* waitSemaphore = acquireSemaphore.get();
            RenderCommandSemaphore* signalSemaphore = drawSemaphore.get();
            
            commandQueue->executeCommandLists(&cmdList, 1, &waitSemaphore, 1, &signalSemaphore, 1, commandFence.get());
            swapChain->present(swapChainTextureIndex, &signalSemaphore, 1);
            commandQueue->waitForCommandFence(commandFence.get());
        }
    };

     struct RasterTest : TestBase {
         void draw() override {
             // TODO: Build this
         }
     };

     struct ComputeTest : RasterTest {
         void draw() override {
             // TODO: Build this
         }
     };

     struct RaytracingTest : TestBase {
         void draw() override {
             // TODO: Build this
         }
     };

    // Test registration and management
    using TestSetupFunc = std::function<std::unique_ptr<TestBase>()>;
    static std::vector<TestSetupFunc> g_Tests;
    static std::unique_ptr<TestBase> g_CurrentTest;
    static uint32_t g_CurrentTestIndex = 0;
    
    void RegisterTests() {
        g_Tests.push_back([]() { return std::make_unique<ClearTest>(); });
        g_Tests.push_back([]() { return std::make_unique<RasterTest>(); });
        g_Tests.push_back([]() { return std::make_unique<ComputeTest>(); });
#ifndef __APPLE__
        g_Tests.push_back([]() { return std::make_unique<RaytracingTest>(); });
#endif
    }

    // Update platform specific code to use the new test framework
#if defined(_WIN64)
    static LRESULT CALLBACK TestWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
        switch (message) {
        case WM_CLOSE:
            PostQuitMessage(0);
            break;
        case WM_SIZE:
            if (g_CurrentTest) g_CurrentTest->resize();
            return 0;
        case WM_PAINT:
            if (g_CurrentTest) g_CurrentTest->draw();
            return 0;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        return 0;
    }

    static HWND TestCreateWindow() {
        // Register window class.
        WNDCLASS wc;
        memset(&wc, 0, sizeof(WNDCLASS));
        wc.lpfnWndProc = TestWndProc;
        wc.hInstance = GetModuleHandle(0);
        wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND);
        wc.lpszClassName = "RenderInterfaceTest";
        RegisterClass(&wc);

        // Create window.
        const int Width = 1280;
        const int Height = 720;
        RECT rect;
        UINT dwStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
        rect.left = (GetSystemMetrics(SM_CXSCREEN) - Width) / 2;
        rect.top = (GetSystemMetrics(SM_CYSCREEN) - Height) / 2;
        rect.right = rect.left + Width;
        rect.bottom = rect.top + Height;
        AdjustWindowRectEx(&rect, dwStyle, 0, 0);

        return CreateWindow(wc.lpszClassName, "Render Interface Test", dwStyle, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, 0, 0, wc.hInstance, NULL);
    }

    void RenderInterfaceTest(RenderInterface *renderInterface) {
        RegisterTests();
        HWND hwnd = TestCreateWindow();
        
        g_CurrentTest = g_Tests[g_CurrentTestIndex]();
        g_CurrentTest->initialize(renderInterface, hwnd);

        MSG msg = {};
        while (msg.message != WM_QUIT) {
            if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

        g_CurrentTest->shutdown();
        DestroyWindow(hwnd);
    }
#elif defined(__ANDROID__)
    void RenderInterfaceTest(RenderInterface* renderInterface) {
        assert(false);
    }
#elif defined(__linux__)
    void RenderInterfaceTest(RenderInterface* renderInterface) {
        RegisterTests();
        Display* display = XOpenDisplay(nullptr);
        int blackColor = BlackPixel(display, DefaultScreen(display));
        Window window = XCreateSimpleWindow(display, DefaultRootWindow(display), 
            0, 0, 1280, 720, 0, blackColor, blackColor);
        

          XSelectInput(display, window, StructureNotifyMask);
        // Map the window and wait for the notify event to come in.
        XMapWindow(display, window);
        while (true) {
            XEvent event;
            XNextEvent(display, &event);
            if (event.type == MapNotify) {
                break;
            }
        }

        // Set up the delete window protocol.
        Atom wmDeleteMessage = XInternAtom(display, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(display, window, &wmDeleteMessage, 1);

        g_CurrentTest = g_Tests[g_CurrentTestIndex]();
        g_CurrentTest->initialize(renderInterface, {display, window});
        g_CurrentTest->resize();
        g_CurrentTest->draw();

        // Loop until the window is closed.
        std::chrono::system_clock::time_point prev_frame = std::chrono::system_clock::now();
        bool running = true;
        while (running) {
            if (XPending(display) > 0) {
                XEvent event;
                XNextEvent(display, &event);

                switch (event.type) {
                    case Expose:
                        g_CurrentTest->draw();
                        break;

                    case ClientMessage:
                        if (event.xclient.data.l[0] == wmDeleteMessage)
                            running = false;
                        break;

                    default:
                        break;
                }
            }

            using namespace std::chrono_literals;
            std::this_thread::sleep_for(1ms);
            auto now_time = std::chrono::system_clock::now();
            if (now_time - prev_frame > 16666us) {
                prev_frame = now_time;
                g_CurrentTest->draw();
            }
        }

        g_CurrentTest->shutdown();
        XDestroyWindow(display, window);
    }
#elif defined(__APPLE__)
    void RenderInterfaceTest(RenderInterface* renderInterface) {
        RegisterTests();
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
            return;
        }

        SDL_Window *window = SDL_CreateWindow("Render Interface Test", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_METAL);
        if (window == nullptr) {
            fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
            SDL_Quit();
            return;
        }

        // Setup Metal view.
        SDL_MetalView view = SDL_Metal_CreateView(window);

        // SDL_Window's handle can be used directly if needed
        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        SDL_GetWindowWMInfo(window, &wmInfo);

        g_CurrentTest = g_Tests[g_CurrentTestIndex]();
        g_CurrentTest->initialize(renderInterface, { wmInfo.info.cocoa.window, SDL_Metal_GetLayer(view) });

        std::chrono::system_clock::time_point prev_frame = std::chrono::system_clock::now();
        bool running = true;
        while (running) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                switch (event.type) {
                    case SDL_QUIT:
                        running = false;
                        break;
                    case SDL_WINDOWEVENT:
                        if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                            g_CurrentTest->resize();
                        }
                        break;
                }
            }

            using namespace std::chrono_literals;
            std::this_thread::sleep_for(1ms);
            auto now_time = std::chrono::system_clock::now();
            if (now_time - prev_frame > 16666us) {
                prev_frame = now_time;
                g_CurrentTest->draw();
            }
        }

        g_CurrentTest->shutdown();
        SDL_Metal_DestroyView(view);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }
#endif
};
