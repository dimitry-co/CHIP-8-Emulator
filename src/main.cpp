#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "chip8.h"
#include <iostream>

// logical chip 8 screen size (resolution)
constexpr int SCREEN_W = 64;
constexpr int SCREEN_H = 32;
// how much to scale each CHIP-8 pixel on your desktop
constexpr int SCALE = 10;

// helper to map host keys -> CHIP-8 keypad (0x0-0xf)
int mapSDLKeyToChip8(SDL_Keycode key) {
    switch (key) {
        case SDLK_1: return 0x1;
        case SDLK_2: return 0x2;
        case SDLK_3: return 0x3;
        case SDLK_4: return 0xC;

        case SDLK_q: return 0x4;
        case SDLK_w: return 0x5;
        case SDLK_e: return 0x6;
        case SDLK_r: return 0xD;

        case SDLK_a: return 0x7;
        case SDLK_s: return 0x8;
        case SDLK_d: return 0x9;
        case SDLK_f: return 0xE;

        case SDLK_z: return 0xA;
        case SDLK_x: return 0x0;
        case SDLK_c: return 0xB;
        case SDLK_v: return 0xF;

        default: return -1;
    }
}

int main(int argc, char** argv) {
    // 1) Handle command-line: must be a filename to load 
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " path/to/game.ch8\n";
        return 1;
    }

    // 2) Initialize CHIP-8 core, load the game into CHIP-8 memory
    Chip8 chip8;
    chip8.init();                         // clear memory, regs, load fontset
    if (!chip8.loadApplication(argv[1])) {      // read file into memory[0x200...]
        std::cerr << "Failed to load game\n";
        return 1;
    }

    // 3) Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << "\n";
        return 1;
    }

    // 4) Create window (640×320 window, scaled 10×)
    SDL_Window* window = SDL_CreateWindow(
        "CHIP-8 Emulator",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_W * SCALE, SCREEN_H * SCALE,
        SDL_WINDOW_SHOWN
    );
    if (!window) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }


    // 5) Create renderer
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // 6) Create a streaming texture of size 64x32
    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STREAMING,
        SCREEN_W, SCREEN_H
    );
    if (!texture) {
        std::cerr << "SDL_CreateTexture Error: " << SDL_GetError() << "\n";
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // 7) Main emulation loop
    bool quit = false;
    SDL_Event event;
    while(!quit) {
        Uint32 frameStart = SDL_GetTicks(); // Get start time in ms.

        // 7a) Handle input events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                quit = true;
            }
            else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {    // handle key presses and releases
                int key = mapSDLKeyToChip8(event.key.keysym.sym);
                if (key >= 0) {
                    chip8.keypad[key] = (event.type == SDL_KEYDOWN);
                    std::cout << "Key " << key << " is " << (event.type == SDL_KEYDOWN ? "pressed" : "released") << std::endl;
                }
                // optional: ESC to quit
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    quit = true;
                }
            }
        }

        // 7b) Emulate multiple cycles (fetch-decode-execute)
        for (int i = 0; i < 10; ++i) {
            chip8.emulateCycle();
        }

        // 7c) If a draw was requested, update the texture & renderer
        if (chip8.drawFlag) {
            // copy 64x32 monochrome framebuffer into RGBA pixels
            uint32_t* pixels;
            int pitch;
            SDL_LockTexture(texture, nullptr, (void**)&pixels, &pitch);
            for (int y = 0; y < SCREEN_H; ++y) {
                for (int x = 0; x < SCREEN_W; ++x) {
                    // look up your emulator mon framebuffer:
                    bool on = chip8.gfx[y * SCREEN_W + x];
                    // calculate the index into pixels[]
                    int rowStart = y * (pitch / 4);     // y * 64: each row is 64 pixels (256 bytes/4 bytes per pixel)
                    int idx = rowStart + x;             // column x in that row
                    pixels[idx] = on ? 0xFFFFFFFF : 0xFF000000; // white if on, black if off
                }
            }
            SDL_UnlockTexture(texture);

            // draw the texture to the window (it will be auto-scaled)
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, nullptr, nullptr);
            SDL_RenderPresent(renderer);
            
            chip8.drawFlag = false; // reset for next frame
        }

        // 7d) update timers (decrement at 60 Hz)
        chip8.updateTimers();

        // 7e) Cap speed to ~60 Hz
        Uint32 frameTime = SDL_GetTicks() - frameStart; // How long the cycle took in ms.
        if (frameTime < 16) { // 16ms = 1/60th of a second
            SDL_Delay(16 - frameTime); // Delay the remaining time
        }  
    }

    // 8) Clean up SDL resources : texture, renderer, then window
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

/*
 What’s going on, step by step:

 1. Command‑line handling:
        We require a .ch8 ROM path as argv[1]; if missing, we print usage and exit.

 2. CHIP‑8 core setup:

        chip8.initialize() zeroes out memory, registers, gfx buffer, loads the built‑in font sprites at 0x050.

        chip8.loadApplication(path) opens the file and reads its bytes into memory[0x200…].

 3. SDL initialization:
        SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) brings up video (window, GPU) and audio (for the buzzer).

 4. Window:
        We create a 640×320 window (64×10 by 32×10).
        
 5. Renderer:
        SDL_CreateRenderer gives us a hardware‑accelerated 2D renderer.

 6. Streaming texture:
        We allocate an off-screen 64×32 RGBA texture. On each draw-frame we'll memcpy our mono-pixel data into this and let SDL scale it for us.

 7. Main emulation loop:
    a. Input: pump the SDL event queue. Map physical keys (1,2,3,4,Q,W... etc.) into the CHIP-8's 16-key keypad array.
    b. Emulate multiple cycles: fetch the next 2-byte opcode from pc, decode and execute it—this may alter registers, memory, PC, and set drawFlag if it's a 00E0 or DXYN.
    c. Draw: when drawFlag is true, we lock our texture, write white or black pixels based on chip8.gfx[], unlock it, then clear & copy the texture to the render target.
        c1. in the Draw loop:
            - chip8.gfx[] → is a 1D array of 64×32 entries, each 0 or 1 (contains the updated screen buffer) 
            - pixels → a pointer to the first pixel's memory, typed here as uint32_t* since each pixel is 4 bytes (RGBA8888).
            - pitch → the number of bytes per row of the texture (64 pixels × 4 bytes = 256, but SDL may pad rows to align).
            - pitch/4 = number of pixels per row = 256 bytes / 4 bytes_per_pixel = 64.
            - (Converts to 0 → false → black, 1 → true → white)
    d. Timers: decrement delay_timer and sound_timer if they're above zero. If sound_timer > 0, you'd also yank out an SDL audio callback to play a square-wave beep.
    e. Frame cap: measure how long this loop took and delay the remainder of ~16 ms so the entire loop runs at ≈60 Hz.

 8. Cleanup:
        Destroy the SDL texture, renderer, window, then call SDL_Quit() to release all subsystems before exiting.
 */