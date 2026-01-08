#include "Sound.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_stdinc.h>
#include <iostream>
#include <cstdint>
#include <algorithm>

Sound* gSound = nullptr;

static void mixAudioFormat(void* dst, const void* src, SDL_AudioFormat format, int len, int volume)
{
    if (len <= 0 || volume <= 0) return;
    float volf = (float)volume / 128.0f;

    // 16-bit signed
    if (SDL_AUDIO_BITSIZE(format) == 16 && SDL_AUDIO_ISINT(format) && SDL_AUDIO_ISSIGNED(format)) {
        int16_t* d = (int16_t*)dst;
        const int16_t* s = (const int16_t*)src;
        int samples = len / 2;
        for (int i = 0; i < samples; ++i) {
            int tmp = (int)std::lrint(s[i] * volf);
            if (tmp > INT16_MAX) tmp = INT16_MAX;
            if (tmp < INT16_MIN) tmp = INT16_MIN;
            d[i] = (int16_t)tmp;
        }
        return;
    }

    // 32-bit float
    if (SDL_AUDIO_BITSIZE(format) == 32 && SDL_AUDIO_ISFLOAT(format)) {
        float* d = (float*)dst;
        const float* s = (const float*)src;
        int samples = len / 4;
        for (int i = 0; i < samples; ++i) {
            d[i] = s[i] * volf;
        }
        return;
    }

    // 8-bit unsigned
    if (SDL_AUDIO_BITSIZE(format) == 8 && !SDL_AUDIO_ISSIGNED(format)) {
        uint8_t* d = (uint8_t*)dst;
        const uint8_t* s = (const uint8_t*)src;
        int samples = len;
        for (int i = 0; i < samples; ++i) {
            int signedSample = (int)s[i] - 128;
            int tmp = (int)std::lrint(signedSample * volf) + 128;
            if (tmp > 255) tmp = 255;
            if (tmp < 0) tmp = 0;
            d[i] = (uint8_t)tmp;
        }
        return;
    }

    // Fallback: copy source to dest if unknown format (no volume applied)
    SDL_memcpy(dst, src, len);
}

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
    for (auto &kv : playingSfx) {
        kv.second.clear();
    }
    playingSfx.clear();

    lastPlayTimeMs.clear();

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

void Sound::playSfx(const std::string& id, int volume, bool allowOverlap, int minIntervalMs)
{
    auto it = loaded.find(id);
    if (it == loaded.end()) {
        SDL_Log("Sound: sfx not loaded: %s", id.c_str());
        return;
    }

    Uint32 now = SDL_GetTicks();
    auto tIt = lastPlayTimeMs.find(id);
    if (minIntervalMs > 0 && tIt != lastPlayTimeMs.end()) {
        Uint32 last = tIt->second;
        if (now - last < (Uint32)minIntervalMs) {
            // Too soon to re-trigger
            return;
        }
    }

    // If overlap is not allowed and there's already an active instance, skip
    auto pit = playingSfx.find(id);
    if (!allowOverlap && pit != playingSfx.end() && !pit->second.empty()) {
        // update last play time even if skipped to prevent burst retriggers
        lastPlayTimeMs[id] = now;
        return;
    }

    SDL_AudioStream* stream = SDL_CreateAudioStream(&it->second.spec, &deviceSpec);
    if (!stream) {
        SDL_Log("Sound: SDL_CreateAudioStream failed: %s", SDL_GetError());
        return;
    }

    if (volume == 128) {
        if (!SDL_PutAudioStreamData(stream, it->second.buffer, (int)it->second.length)) {
            SDL_Log("Sound: SDL_PutAudioStreamData failed: %s", SDL_GetError());
            SDL_DestroyAudioStream(stream);
            return;
        }
    } else {
        Uint8* temp = (Uint8*)SDL_malloc(it->second.length);
        if (!temp) {
            SDL_Log("Sound: malloc failed for sfx volume mixing");
            SDL_DestroyAudioStream(stream);
            return;
        }
        mixAudioFormat(temp, it->second.buffer, deviceSpec.format, (int)it->second.length, volume);
        if (!SDL_PutAudioStreamData(stream, temp, (int)it->second.length)) {
            SDL_Log("Sound: SDL_PutAudioStreamData failed for sfx (mixed): %s", SDL_GetError());
            SDL_free(temp);
            SDL_DestroyAudioStream(stream);
            return;
        }
        SDL_free(temp);
    }

    if (!SDL_BindAudioStream(device, stream)) {
        SDL_Log("Sound: SDL_BindAudioStream failed: %s", SDL_GetError());
        SDL_DestroyAudioStream(stream);
        return;
    }

    activeStreams.push_back(stream);
    playingSfx[id].push_back(stream);
    lastPlayTimeMs[id] = now;
}

void Sound::stopSfx(const std::string& id)
{
    auto pit = playingSfx.find(id);
    if (pit == playingSfx.end()) return;

    for (SDL_AudioStream* s : pit->second) {
        SDL_UnbindAudioStream(s);
        SDL_DestroyAudioStream(s);
        for (auto it = activeStreams.begin(); it != activeStreams.end(); ++it) {
            if (*it == s) { activeStreams.erase(it); break; }
        }
    }

    playingSfx.erase(pit);
}

void Sound::stopAllSfx()
{
    for (auto& kv : playingSfx) {
        for (SDL_AudioStream* s : kv.second) {
            SDL_UnbindAudioStream(s);
            SDL_DestroyAudioStream(s);
        }
    }
    playingSfx.clear();

    for (auto s : activeStreams) SDL_DestroyAudioStream(s);
    activeStreams.clear();
}

void Sound::playMusic(const std::string& id, bool loop, int volume)
{
    if (musicMuted) return; // respect mute

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

    if (volume == 128) {
        if (!SDL_PutAudioStreamData(musicStream, it->second.buffer, (int)it->second.length)) {
            SDL_Log("Sound: SDL_PutAudioStreamData failed for music: %s", SDL_GetError());
            SDL_DestroyAudioStream(musicStream);
            musicStream = nullptr;
            return;
        }
    } else {
        Uint8* temp = (Uint8*)SDL_malloc(it->second.length);
        if (!temp) {
            SDL_Log("Sound: malloc failed for music volume mixing");
            SDL_DestroyAudioStream(musicStream);
            musicStream = nullptr;
            return;
        }
        mixAudioFormat(temp, it->second.buffer, deviceSpec.format, (int)it->second.length, volume);
        if (!SDL_PutAudioStreamData(musicStream, temp, (int)it->second.length)) {
            SDL_Log("Sound: SDL_PutAudioStreamData failed for music (mixed): %s", SDL_GetError());
            SDL_free(temp);
            SDL_DestroyAudioStream(musicStream);
            musicStream = nullptr;
            return;
        }
        SDL_free(temp);
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
}

void Sound::setMusicMuted(bool muted)
{
    musicMuted = muted;
    if (musicMuted) {
        // immediately stop music stream
        if (musicStream) {
            SDL_UnbindAudioStream(musicStream);
            SDL_DestroyAudioStream(musicStream);
            musicStream = nullptr;
        }
    } else {
        // if unmuting, re-start music if an id exists
        if (!musicId.empty()) {
            playMusic(musicId, musicLoop, musicVolume);
        }
    }
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
            for (auto &kv : playingSfx) {
                for (auto vit = kv.second.begin(); vit != kv.second.end();) {
                    if (*vit == s) vit = kv.second.erase(vit);
                    else ++vit;
                }
            }

            SDL_UnbindAudioStream(s);
            SDL_DestroyAudioStream(s);
            it = activeStreams.erase(it);
        } else {
            ++it;
        }
    }

    // Remove empty vectors from playingSfx
    for (auto pit = playingSfx.begin(); pit != playingSfx.end();) {
        if (pit->second.empty()) pit = playingSfx.erase(pit);
        else ++pit;
    }

    // Handle music looping: if musicStream drained and loop=true, requeue
    if (musicStream && musicLoop) {
        int avail = SDL_GetAudioStreamAvailable(musicStream);
        if (avail <= 0) {
            auto it2 = loaded.find(musicId);
            if (it2 != loaded.end()) {
                if (musicVolume == 128) {
                    SDL_PutAudioStreamData(musicStream, it2->second.buffer, (int)it2->second.length);
                } else {
                    Uint8* temp = (Uint8*)SDL_malloc(it2->second.length);
                    if (temp) {
                        mixAudioFormat(temp, it2->second.buffer, deviceSpec.format, (int)it2->second.length, musicVolume);
                        SDL_PutAudioStreamData(musicStream, temp, (int)it2->second.length);
                        SDL_free(temp);
                    }
                }
            }
        }
    }
}
