#ifndef CHIP8_AUDIO_H                       // Says "If CHIP8_AUDIO_H is not defined yet ..."
#define CHIP8_AUDIO_H                       // "Then define it and include this code"

#include <SDL.h>
#include <atomic>

class Audio {
public:
    ~Audio();                               // Destructor (cleanup when Audio object is destroyed)

    bool Initialize();                      // Sets up SDL audio - returns true if successful
    void StartBeep();                       // Turns on beep sound
    void StopBeep();                        // Turns off beep sound

private:
    // Audio Settings (constants):
    static constexpr double kFrequency = 44100;     // Sample rate (44.1 kHz) - How many audio samples per second SDL needs (standard CD quality)
    static constexpr double kTone = 440;            // Chip8 Beep frequency (440 Hz = A note)
    static constexpr float kAmplitude = 0.25f;      // Volume (0.25 = 25% volume)

    SDL_AudioDeviceID device = 0;                   // SDL's audio device handle - Needed to pause/unpause and close audio.
    std::atomic<bool> beep_on{false};               // Whether the beep is currently playinh (atomic = thread-safe. safe to change from the main thread while SDL’s audio thread reads it)

    // Wave generation:
    float wave_position = 0.0f;                     // Current position in the sound wave cycle in radians 0 to 2π (0 to 360 degrees)
    float wave_increment = 0.0f;                    // How much to advance wave position for each audio sample, based on the obtained frequency

    static void AudioCallback(void* userdata, Uint8* stream, int len);  // SDL calls this to get audio samples
};

#endif  // CHIP8_AUDIO_H



// #ifndef, #define, and #endif is used to prevent compilation errors if you accidentally include audio.h twice.