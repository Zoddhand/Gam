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
    // allowOverlap=false prevents starting a new instance while one is playing
    // minIntervalMs specifies minimum milliseconds between plays of the same id (0 = no limit)
    void playSfx(const std::string& id, int volume = 128, bool allowOverlap = false, int minIntervalMs = 0);
    void stopSfx(const std::string& id);
    void stopAllSfx();
    void playMusic(const std::string& id, bool loop = true, int volume = 128);
    void stopMusic();

    // music mute control
    void setMusicMuted(bool muted);
    bool isMusicMuted() const { return musicMuted; }

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
    // Track which SFX ids are currently playing (allow multiple streams per id)
    std::unordered_map<std::string, std::vector<SDL_AudioStream*>> playingSfx;

    // track last play time (ms) for each sfx id to enforce min interval
    std::unordered_map<std::string, Uint32> lastPlayTimeMs;

    // music-specific
    SDL_AudioStream* musicStream = nullptr;
    std::string musicId;
    bool musicLoop = false;
    int musicVolume = 128;
    bool musicMuted = false;
};

// Global pointer set by Engine to allow easy SFX calls from gameplay code
extern Sound* gSound;
