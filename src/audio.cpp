#define _USE_MATH_DEFINES
#include "audio.h"
#include <iostream>
#include <cmath>

bool Audio::Initialize()
{
    SDL_AudioSpec desired{};
    SDL_AudioSpec obtained{};               // where SDL put the real format
    desired.freq = kFrequency;
    desired.format = AUDIO_F32;
    desired.channels = 1;
    desired.samples = 2048;
    desired.callback = AudioCallback;
    desired.userdata = this;                // this - pointer to the current Audio object

    device = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, SDL_AUDIO_ALLOW_ANY_CHANGE);

    if (!device)
    { // handle open-failure first
        std::cerr << "SDL Error: " << SDL_GetError() << '\n';
        return false;
    }

    wave_increment = static_cast<float>(kTone * 2.0 * M_PI / obtained.freq);        // “How far should I advance the phase for one sample so that my pitch comes out right?” so that, after the device has played exactly sample_rate samples, the phase has looped 440 times. Calculated as 2π × f / Fs.

    SDL_PauseAudioDevice(device, 0); // Start the audio thread
    return true;
}

// Helpers that set the atomic flag true/false. main CPU thread calls these; audio thread reads them.
void Audio::StartBeep()
{
    beep_on = true;
}

void Audio::StopBeep()
{
    beep_on = false;
}

void Audio::AudioCallback(void *userdata, Uint8 *stream, int len)
{
    Audio *audio = static_cast<Audio *>(userdata);              // Recovers the audio object that owns this callback
    float *fstream = reinterpret_cast<float *>(stream);
    int samples = len / sizeof(float);                          // Number of samples that SDL wants this time

    for (int i = 0; i < samples; ++i)
    {
        if (audio->beep_on)
        { // for each sample you look at wave position, output +0.25 or -0.25 to make a square-wave (that's amplitude)
            fstream[i] = (audio->wave_position < M_PI) ? kAmplitude : -kAmplitude;
        }
        else
        {
            fstream[i] = 0.0f;
        }
        audio->wave_position += audio->wave_increment;          // Advance phase by one sample
        if (audio->wave_position >= 2.0f * M_PI)
        { // Wrap back to 0 once you hit 2pie so the float never explodes. This keeps the angle in [0, 2pie]
            audio->wave_position -= 2.0f * M_PI;
        }
    }
}

Audio::~Audio()
{
    SDL_PauseAudioDevice(device, 1);            // Stop audio
    if (device)
    {
        SDL_CloseAudioDevice(device);           // Clean up SDL audio
    }
}



/*
____________________________________________________________________________________________________________________________________________________
Amplitude vs. frequency:
    Amplitude (here 0.25) is volume, not pitch.
    Doubling 0.25 → 0.50 makes it louder, not higher.

    Frequency (440 Hz) is entirely captured in wave_increment.
    Change kTone to 880 and wave_increment doubles—giving you the A one octave higher—while amplitude stays 0.25.
____________________________________________________________________________________________________________________________________________________

Wave Increment:
    Think of the wave as a circle that you walk around:

    One full lap = 2 π radians.

    You want to finish 440 laps every second.

    But you only take a step when the sound-card says “next sample!”

So the size of each step must be:
wave_increment= (2π×tone_freq) / sample_rate

That guarantees:

    After exactly sample_rate samples (one second of audio),
    you have stepped around the circle tone_freq times.

Sample-rate	Wave-increment (radians / sample)
44 100 Hz	2 π × 440 / 44 100 ≈ 0.0627
48 000 Hz	2 π × 440 / 48 000 ≈ 0.0576

Higher sample-rate ⇒ smaller step per sample
so that you still finish 440 laps in one second.
____________________________________________________________________________________________________________________________________________________

Quick summary:

    440 Hz = how fast the note vibrates (pitch).

    Sample-rate (44 100 Hz, 48 000 Hz…) = how often you tell the speaker where the wave is (time resolution).

    wave_increment = perfect mini-angle so that, no matter what sample-rate the hardware wants, the note still completes exactly 440 wobbles each second.
______________________________________________________________________________________________________________________________________________________________________
*/
