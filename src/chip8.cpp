#include <cstdlib>   // for std::srand(), std::rand()
#include <ctime>     // for std::time()
#include <cstring>   // for std::memset()
#include <fstream>   // for std::ifstream
#include <iostream>  // for std::cerr
#include "chip8.h"
#include <SDL.h>
#include <atomic>

static constexpr int SCREEN_W = 64;
static constexpr int SCREEN_H = 32;

// 4x5 pixels font sprites for 0-F
static const uint8_t fontset[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, //0
    0x20, 0x60, 0x20, 0x20, 0x70, //1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, //2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, //3
    0x90, 0x90, 0xF0, 0x10, 0x10, //4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, //5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, //6
    0xF0, 0x10, 0x20, 0x40, 0x40, //7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, //8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, //9
    0xF0, 0x90, 0xF0, 0x90, 0x90, //A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, //B
    0xF0, 0x80, 0x80, 0x80, 0xF0, //C
    0xE0, 0x90, 0x90, 0x90, 0xE0, //D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, //E
    0xF0, 0x80, 0xF0, 0x80, 0x80  //F
};


// Constructor
Chip8::Chip8() { 
    std::srand(static_cast<unsigned>(std::time(nullptr)));  // seed RNG so CXNN yields varied random values
}

void Chip8::init() {
    pc = 0x200;         // Program counter starts at 0x200 (start address program)
    opcode = 0;         // Reset current opcode
    I = 0;              // Reset index register
    sp = 0;             // Reset stack pointer

    // Clear display
    gfx.fill(0);

    // Clear registers
    V.fill(0);

    // Clear stack
    stack.fill(0);

    // Clear keys state
    keypad.fill(0);

    // Clear memory
    memory.fill(0);

    // Load fontset at memory location 0x50
    for (int i = 0; i < 80; ++i) 
        memory[0x050 + i] = fontset[i];
    
    // Reset timers
    delay_timer = 0;
    sound_timer = 0;

    // Clear screen once
    drawFlag = true;
    
}

bool Chip8::loadApplication(const std::string& filepath) {
    init(); // Clear everything, reset state, load fontset, etc.
    
    // 1) Open file path in binary mode + go to end to get size
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return false;
    }

    // 2) Measure, validate size of ROM
    std::streamsize romSize = file.tellg(); // Get size of ROM

    // Maximum space from 0x200 to 0xFFF (512 to 4095 bytes)
    const std::size_t MAX_ROM_SIZE = memory.size() - 0x200; // 3584 bytes
    // Check if ROM size is valid
    if (romSize <= 0 || romSize > MAX_ROM_SIZE) {
        return false;
    }

    // 3) Go back to start and read ROM into memory starting at 0x200
    file.seekg(0, std::ios::beg);
    auto dst = memory.data() + 0x200;             // uint8_t*
    auto ptr = reinterpret_cast<char*>(dst);      // tell read() "here's a char*"
    file.read(ptr, romSize);                     
    if (file.fail()) {
        return false;
    }

    return true;
}

void Chip8::updateTimers() {
    // Each call = 1/60 s "tick"

    // 0) if we ended last tick still beeping but timer now 0, turn it off 
    if (isBeeping && sound_timer == 0) {
        stopBeep();
        isBeeping = false;
    }

    // 1) tick the delay timer
    if (delay_timer > 0) {
        --delay_timer;
    }

    // 2) if sound_timer > 0 start the beep (once) if it hasn't started already, then decrement the timer
    if (sound_timer > 0) {
        // first tick of a new beep
        if (!isBeeping) {
            startBeep();
            isBeeping = true;
        }
        --sound_timer; // tick the sound timer
    }

}

void Chip8::startBeep() {
    audio.StartBeep();
}
void Chip8::stopBeep()  {
    audio.StopBeep();
}

void Chip8::emulateCycle() {
    // 1) Fetch next opcode (big-indian two bytes)
    opcode = (memory[pc] << 8) | memory[pc + 1];
    pc += 2;
    uint16_t nnn = opcode & 0x0FFF;         // address
    uint8_t regX = (opcode & 0x0F00) >> 8;  // Extract V-reg X
    uint8_t regY = (opcode & 0x00F0) >> 4;  // Extract V-reg Y
    uint8_t val = opcode & 0x00FF;          // Extract kk

    // Decode + Execute
    switch(opcode & 0xF000) { // read first 4 bits of opcode (most-significant nibble)
        case 0x000:
            switch(opcode & 0x0FF) {
                case 0x00E0: // CLS: Clears the display
                    gfx.fill(0);
                    drawFlag = true;
                    break;

                case 0x00EE: // RET: Return from subroutine
                    --sp;
                    pc = stack[sp];
                    break;
                
                default: // 0NNN SYS addr (ignored)
                    break;
            }
            break;
        
        case 0x1000: // 1NNN: JP addr, Set PC to nnn
            pc = nnn;
            break;

        case 0x2000: // 2NNN: CALL addr, push current pc on top of stack then set pc to nnn
            stack[sp] = pc; // push the address you just moved to (i.e. return‑address)
            sp++;
            pc = nnn; // jump into the subroutine
            break;
        
        case 0x3000: // 3XKK: Skip next instruction if Vx = kk
            if (V[regX] == val) {
                pc += 2;    // Skip instruction
            }
            break;
        
        case 0x4000: // 4XKK: Skip next instruction if Vx != kk
            if (V[regX] != val) {
                pc += 2;
            }
            break;
        
        case 0x5000: // 5XY0: Skip next instruction if Vx = Vy
            if (V[regX] == V[regY]) {
                pc += 2;
            }
            break; 
        
        case 0x6000: // 6XKK: set Vx to kk (put val kk into reg Vx)
            V[regX] = val;
            break;
        
        case 0x7000: // 7XKK: Set Vx = Vx + kk
            V[regX] += val;
            break;
        
        case 0x8000: // Multiple instructions
            switch(opcode & 0x000F) {
                case 0x0: // 8XY0: Set Vx to Vy
                    V[regX] = V[regY];
                    break;
                
                case 0x1: // 8XY1: Set Vx to Vx or Vy (Bitwise OR operation)
                    V[regX] |= V[regY];
                    break;
                
                case 0x2: // 8XY2: Set Vx to Vx and Vy (Bitwise AND operation)
                    V[regX] &= V[regY];
                    break;
                
                case 0x3: // 8XY3: Set Vx to Vx xor Vy
                    V[regX] ^= V[regY];
                    break;
                
                case 0x4: { // 8XY4: Adds Vy to Vx. VF is set to 1 when there's a carry, and to 0 when there isn't
                    uint16_t sum = V[regX] + V[regY];
                    V[0xF] = (sum > 0xFF) ? 1 : 0; // if 
                    V[regX] = sum & 0xFF;
                    break;
                }
                    
                
                case 0x5: // 8XY5: Vy is subtracted from Vx. VF is set to 1 if Vx > Vy (no borrow needed), else set to 0 (there's a borrow)
                    V[0xF] = (V[regX] > V[regY]) ? 1 : 0;
                    V[regX] = V[regX] - V[regY];
                    break;
                
                case 0x6: // 8XY6: Shifts Vx right by one(same as divide by 2). VF is set to the value of the least significant bit of Vx before the shift
                    V[0xF] = V[regX] & 0x1; 
                    V[regX] >>= 1;
                    break;
                
                case 0x7: // 8XY7: Sets Vx to Vy - Vx. VF is set to 1 if Vy > Vx(no borrow), else set to 0 (theres a borrow)
                    V[0xF] = (V[regY] > V[regX]) ? 1 : 0;
                    V[regX] = V[regY] - V[regX];
                    break;
                
                case 0xE: // 8XYE: Shifts Vx left by 1(multiplied by 2). VF is set to the value of the most significant bit of Vx before the shift.
                    V[0xF] = V[regX] >> 7;
                    V[regX] <<= 1;
                    break;
                
                default:
                    // unkown 
                    break;
            }
            break; 
        
        case 0x9000: // 9XY0: Skips next instruction if Vx != Vy
            if (V[regX] != V[regY]) {
                pc += 2;
            }
            break;
        
        case 0xA000: // ANNN: Sets I to the address NNN.
            I = nnn;
            break;
        
        case 0xB000: // BNNN: Jumps to the address NNN plus V0
            pc = nnn + V[0];
            break;
               
        case 0xC000: // CXKK: Sets VX to the result of a bitwise and operation on a random number (0 to 255) and KK.
            V[regX] = (std::rand() & 0xFF) & val;
            break;
        
        case 0xD000: { // DXYN: draw sprite at (Vx,Vy), height=N, XOR, wrap, VF=collision
            // Draws a sprite at coordinate (VX, VY) that has a width of 8 pixels and a height of N pixels.
            // Each row of 8 pixels is read as bit-coded starting from memory location I
            // I value doesn’t change after the execution of this instruction.
            // VF is set to 1 if any screen pixels are flipped from set to unset when the sprite is drawn, and to 0 if that doesn’t happen
            // If the sprite is positioned so part of it is outside the coordinates of the display, it wraps around to the opposite side of the screen
            auto xStart = V[regX];
            auto yStart = V[regY];
            auto height = opcode & 0x000F;
            uint8_t sprite;
            V[0xF] = 0;

            for (int row = 0; row < height; row++) 
            {
                sprite = memory[I + row];
                for (int col = 0; col < 8; col++) 
                {
                    if ((sprite & (0x80 >> col)) != 0) // checks the pixel value of sprite. if pixel is 1 check for collision and XOR with current pixel on display
                    { 
                        // use modulo (%) to wrap around row and column when out of bounds
                        auto x = (xStart + col) % SCREEN_W;
                        auto y = (yStart + row) % SCREEN_H;
                        auto idx = y * SCREEN_W + x;
                        if (gfx[idx] == 1)
                        {
                            V[0xF] = 1;
                        }
                        gfx[idx] ^= 1;
                    }
                }
            }
            drawFlag = true;
            break;
        }
        
        case 0xE000: // Two instructions possible
            switch(val) {
                case 0x9E: // EX9E: Skips the next instruction if key stored in Vx is pressed.
                    if (keypad[V[regX]] != 0) {
                        pc += 2;
                    }
                    break;
                
                case 0xA1: // EXA1: Skips the next instruction if key stored in Vx is NOT pressed.
                    if (keypad[V[regX]] == 0) {
                        pc += 2;
                    }
                    break;
                
                default:
                    // unknown
                    break;
            }
            break;
        
        case 0xF000: // multiple instructions possible
            switch(val) {
                case 0x07: // FX07: Sets Vx to the value of the delay timer.
                    V[regX] = delay_timer;
                    break;
                
                case 0x0A: {// FX0A: Waits for a key press, then stores the key name in Vx
                    bool key_pressed = false;
                    for (int i = 0; i < 16; ++i) {
                        if (keypad[i]) {
                            V[regX] = i;
                            key_pressed = true;
                        }
                    }
                    // If no key pressed, decrement PC by 2 to try again.
					if(!key_pressed) {
                        pc -= 2;
                    }								
                }
                    break;
                
                case 0x15: // FX15: Sets the delay timer to Vx
                    delay_timer = V[regX];
                    break;
                
                case 0x18: // FX18: Sets the sound timer to Vx
                    sound_timer = V[regX];
                    break;
                
                case 0x1E: // FX1E: Adds Vx to I.
                    I += V[regX];
                    break;
                
                case 0x29: // FX29: Sets I to the location of the sprite for the character in Vx. Characters 0-F (in hexadecimal) are represented by a 4×5 font.
                    I = 0x50 + (V[regX] * 5); // Each sprite is 5 bytes long, and 0x050 is bases address for the fontset in memory.
                    break;
                
                case 0x33: // FX33: Stores the binary-coded decimal representation of Vx, with the hundreds digit in memory location I, the tens digit in I+1, and the ones digit in I+2.
                    memory[I] = V[regX] / 100; // Integer division (/) truncates towards zero when both operands are integers
                    memory[I + 1] = (V[regX] / 10) % 10;
                    memory[I + 2] =  V[regX] % 10;
                    break;
                
                case 0x55: // FX55: Stores V0 to VX (inclusive) in memory starting at address stored in I.
                    for (int i = 0; i <= regX; ++i) {
                        memory[I + i] = V[i];
                    }
                    break;
                
                case 0x65: //FX65: Read V0 to Vx (inclusive) from memory starting at address stored in I.
                    for (int i = 0; i <= regX; ++i) {
                        V[i] = memory[I + i];
                    }
                    break;
                
                default:
                    // unknown
                    break;
            }

            break;
        
        default:
        // Log the full 16-bit opcode in hex so you can see what was missed
        std::cerr
          << "Unknown opcode: 0x"
          << std::hex << opcode << std::dec
          << "\n";
        break;
    }
}