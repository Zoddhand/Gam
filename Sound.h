#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <SDL3/SDL.h>

class Sound {
public:
    Sound();
    ~Sound();

    bool init();
    void shutdown();

    bool loadWav(const std::string& id, const std::string& path);
    void playSfx(const std::string& id, int volume = 128);
    void stopSfx(const std::string& id);
    void stopAllSfx();
    void playMusic(const std::string& id, bool loop = true, int volume = 128);
    void stopMusic();

    // Call regularly from the game's update loop to clean up finished streams
    void update();

    static int randomInt(int min, int max) {
        return rand() % (max - min + 1) + min;
    }
private:
    struct AudioData {
        Uint8* buffer = nullptr; // converted to device format
        Uint32 length = 0;
        SDL_AudioSpec spec{};    // format of buffer (should match deviceSpec)
    };

    std::unordered_map<std::string, AudioData> loaded;

    SDL_AudioDeviceID device = 0;
    SDL_AudioSpec deviceSpec{};
    int deviceSampleFrames = 0;

    std::vector<SDL_AudioStream*> activeStreams;
    // Track which SFX id is currently playing (no overlap per id)
    std::unordered_map<std::string, SDL_AudioStream*> playingSfx;

    // music-specific
    SDL_AudioStream* musicStream = nullptr;
    std::string musicId;
    bool musicLoop = false;
    int musicVolume = 128;
};

// Global pointer set by Engine to allow easy SFX calls from gameplay code
extern Sound* gSound;
