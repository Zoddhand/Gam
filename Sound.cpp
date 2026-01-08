#include "Sound.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_stdinc.h>
#include <iostream>

Sound* gSound = nullptr;

Sound::Sound() {}
Sound::~Sound() { shutdown(); }

bool Sound::init()
{
    // Open default playback device with no specific hint; query preferred format via SDL_GetAudioDeviceFormat
    device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (device == 0) {
        SDL_Log("Sound: SDL_OpenAudioDevice failed: %s", SDL_GetError());
        return false;
    }

    if (!SDL_GetAudioDeviceFormat(device, &deviceSpec, &deviceSampleFrames)) {
        SDL_Log("Sound: SDL_GetAudioDeviceFormat failed: %s", SDL_GetError());
        SDL_CloseAudioDevice(device);
        device = 0;
        return false;
    }

    SDL_Log("Sound: device format freq=%d channels=%d format=%d frames=%d",
        deviceSpec.freq, deviceSpec.channels, deviceSpec.format, deviceSampleFrames);

    return true;
}

void Sound::shutdown()
{
    stopMusic();

    // destroy active streams
    for (auto s : activeStreams) {
        SDL_DestroyAudioStream(s);
    }
    activeStreams.clear();

    // clear playingSfx map
    playingSfx.clear();

    if (musicStream) {
        SDL_DestroyAudioStream(musicStream);
        musicStream = nullptr;
    }

    for (auto& kv : loaded) {
        if (kv.second.buffer) SDL_free(kv.second.buffer);
    }
    loaded.clear();

    if (device) {
        SDL_CloseAudioDevice(device);
        device = 0;
    }
}

bool Sound::loadWav(const std::string& id, const std::string& path)
{
    if (loaded.find(id) != loaded.end()) return true;

    SDL_AudioSpec spec;
    Uint8* audio_buf = nullptr;
    Uint32 audio_len = 0;

    if (!SDL_LoadWAV(path.c_str(), &spec, &audio_buf, &audio_len)) {
        SDL_Log("Sound: SDL_LoadWAV failed for %s: %s", path.c_str(), SDL_GetError());
        return false;
    }

    // Convert samples to device format if necessary
    if (spec.format != deviceSpec.format || spec.freq != deviceSpec.freq || spec.channels != deviceSpec.channels) {
        Uint8* converted = nullptr;
        int converted_len = 0;
        if (!SDL_ConvertAudioSamples(&spec, audio_buf, (int)audio_len, &deviceSpec, &converted, &converted_len)) {
            SDL_Log("Sound: SDL_ConvertAudioSamples failed: %s", SDL_GetError());
            SDL_free(audio_buf);
            return false;
        }
        SDL_free(audio_buf);
        audio_buf = converted;
        audio_len = (Uint32)converted_len;
        spec.format = deviceSpec.format;
        spec.freq = deviceSpec.freq;
        spec.channels = deviceSpec.channels;
    }

    AudioData d;
    d.buffer = audio_buf;
    d.length = audio_len;
    d.spec = spec;

    loaded[id] = d;
    SDL_Log("Sound: loaded %s (%u bytes)", id.c_str(), audio_len);
    return true;
}

void Sound::playSfx(const std::string& id, int volume)
{
    // If this SFX is already playing, ignore retriggers
    if (playingSfx.find(id) != playingSfx.end()) {
        //SDL_Log("Sound: sfx %s already playing, skipping retrigger", id.c_str());
        return;
    }

    auto it = loaded.find(id);
    if (it == loaded.end()) {
        SDL_Log("Sound: sfx not loaded: %s", id.c_str());
        return;
    }

    // Create a stream with input = file spec, output = deviceSpec
    SDL_AudioStream* stream = SDL_CreateAudioStream(&it->second.spec, &deviceSpec);
    if (!stream) {
        SDL_Log("Sound: SDL_CreateAudioStream failed: %s", SDL_GetError());
        return;
    }

    if (!SDL_PutAudioStreamData(stream, it->second.buffer, (int)it->second.length)) {
        SDL_Log("Sound: SDL_PutAudioStreamData failed: %s", SDL_GetError());
        SDL_DestroyAudioStream(stream);
        return;
    }

    // Bind stream to device and let SDL mix it
    if (!SDL_BindAudioStream(device, stream)) {
        SDL_Log("Sound: SDL_BindAudioStream failed: %s", SDL_GetError());
        SDL_DestroyAudioStream(stream);
        return;
    }

    activeStreams.push_back(stream);
    playingSfx[id] = stream;
}

void Sound::stopSfx(const std::string& id)
{
    auto pit = playingSfx.find(id);
    if (pit == playingSfx.end()) return;

    SDL_AudioStream* s = pit->second;
    // Unbind and destroy this stream
    SDL_UnbindAudioStream(s);
    SDL_DestroyAudioStream(s);

    // Remove from activeStreams list if present
    for (auto it = activeStreams.begin(); it != activeStreams.end(); ++it) {
        if (*it == s) { activeStreams.erase(it); break; }
    }

    playingSfx.erase(pit);
}

void Sound::stopAllSfx()
{
    for (auto& p : playingSfx) {
        SDL_AudioStream* s = p.second;
        SDL_UnbindAudioStream(s);
        SDL_DestroyAudioStream(s);
    }
    playingSfx.clear();

    // clear any active streams not in playingSfx
    for (auto s : activeStreams) SDL_DestroyAudioStream(s);
    activeStreams.clear();
}

void Sound::playMusic(const std::string& id, bool loop, int volume)
{
    auto it = loaded.find(id);
    if (it == loaded.end()) {
        SDL_Log("Sound: music not loaded: %s", id.c_str());
        return;
    }

    // stop existing music
    if (musicStream) {
        SDL_UnbindAudioStream(musicStream);
        SDL_DestroyAudioStream(musicStream);
        musicStream = nullptr;
    }

    musicStream = SDL_CreateAudioStream(&it->second.spec, &deviceSpec);
    if (!musicStream) {
        SDL_Log("Sound: SDL_CreateAudioStream failed for music: %s", SDL_GetError());
        return;
    }

    // For music we may want to loop: if loop, push the buffer multiple times or handle on update
    if (!SDL_PutAudioStreamData(musicStream, it->second.buffer, (int)it->second.length)) {
        SDL_Log("Sound: SDL_PutAudioStreamData failed for music: %s", SDL_GetError());
        SDL_DestroyAudioStream(musicStream);
        musicStream = nullptr;
        return;
    }

    if (!SDL_BindAudioStream(device, musicStream)) {
        SDL_Log("Sound: SDL_BindAudioStream failed for music: %s", SDL_GetError());
        SDL_DestroyAudioStream(musicStream);
        musicStream = nullptr;
        return;
    }

    musicId = id;
    musicLoop = loop;
    musicVolume = volume;
}

void Sound::stopMusic()
{
    if (!musicStream) return;
    SDL_UnbindAudioStream(musicStream);
    SDL_DestroyAudioStream(musicStream);
    musicStream = nullptr;
    musicId.clear();
}

void Sound::update()
{
    // Clean up any streams that are no longer bound or have no available data
    auto it = activeStreams.begin();
    while (it != activeStreams.end()) {
        SDL_AudioStream* s = *it;
        int available = SDL_GetAudioStreamAvailable(s);
        if (available <= 0) {
            // find any id in playingSfx mapped to this stream and remove it
            for (auto pit = playingSfx.begin(); pit != playingSfx.end();) {
                if (pit->second == s) pit = playingSfx.erase(pit);
                else ++pit;
            }

            SDL_UnbindAudioStream(s);
            SDL_DestroyAudioStream(s);
            it = activeStreams.erase(it);
        } else {
            ++it;
        }
    }

    // Handle music looping: if musicStream drained and loop=true, requeue
    if (musicStream && musicLoop) {
        int avail = SDL_GetAudioStreamAvailable(musicStream);
        if (avail <= 0) {
            auto it2 = loaded.find(musicId);
            if (it2 != loaded.end()) {
                SDL_PutAudioStreamData(musicStream, it2->second.buffer, (int)it2->second.length);
            }
        }
    }
}
