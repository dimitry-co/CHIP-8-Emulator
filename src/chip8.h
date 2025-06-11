#pragma once
#include <array>
#include <cstdint>

class Chip8 {
    public:
        Chip8();                                                // Constructor
        void init();                                            // Reset CPU, load fontset
        bool loadApplication(const std::string& filepath);      // load ROM at 0x200
        void emulateCycle();                                    // fetch-decode-execute one opcode
        void updateTimers();                                    // decrement delay & sound @60 Hz

        // public state consumed by main.cpp
        std::array<uint8_t, 64 * 32> gfx;           // Display buffer of 2048 pixels (0=off, 1=on)
        bool drawFlag = false;                      // set by 00E0 and DXYN
        std::array<uint8_t, 16> keypad;                // Hex Keypad state                

    private:
        // CHIP-8 core
        uint16_t pc = 0;            // Program counter
        uint16_t opcode = 0;        // Current opcode
        uint16_t I = 0;             // Index register
        uint8_t sp = 0;             // Stack pointer

        std::array<uint8_t, 16> V;          // V0-VF, 8 bit general purpose registers (Vx where x ranges from 0 to F (V0-VF))
        std::array<uint16_t, 16> stack;     // call stack of 16 return addresses
        std::array<uint8_t, 4096> memory;   // Memory (size = 4k)

        uint8_t delay_timer = 0;            // Delay timer (decrement at 60 Hz)
        uint8_t sound_timer = 0;            // Sound timer (decrement at 60 Hz)

        bool isBeeping = false;
        void startBeep();
        void stopBeep();

};
