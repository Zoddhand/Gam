#pragma once
#include <SDL3/SDL.h>
#include <SDL3_IMAGE/SDL_image.h>
#include <vector>
#include <string>
#include <array>

// Simple reusable parallax background manager
// It loads N layers named "1.png", "2.png", ... from a directory
// and draws them with configurable parallax factors.
class Background {
public:
    Background();
    ~Background();

    // Load layers from a directory (expects files 1.png .. layers.png)
    bool load(SDL_Renderer* renderer, const std::string& dir, int layers = 7);

    // Unload textures
    void unload();

    // Render background using camera position (world coordinates)
    // mapPixelHeight: total map height in world pixels (map.height * TILE_SIZE)
    void draw(SDL_Renderer* renderer, int camX, int camY, int screenW, int screenH, int mapPixelHeight);

    // Update autonomous scrolling (dt seconds)
    void update(float dt);

    // Associate specific level IDs with this background
    void addLevel(int levelID);
    bool matchesLevel(int levelID) const;

    // Per-layer configuration (public so game/level code can tweak easily)
    // Multipliers are applied to the internal base horizontal/vertical factors.
    std::vector<float> layerHMultiplier; // multiply base horizontal parallax factor
    std::vector<float> layerVMultiplier; // multiply base vertical parallax factor
    std::vector<float> layerAutoScrollMultiplier; // multiplier applied to autonomous scroll speed

    // Hardcoded defaults you can tweak here. Array length is the maximum expected layers
    // Default auto-scroll speeds (pixels/sec). Positive = right, negative = left.
    inline static const std::array<float, 7> DEFAULT_AUTO_SCROLL_SPEEDS = { 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, -1.4f, 0.0f };
    // Default per-layer horizontal multipliers (multiplies computed hFactor)
    inline static const std::array<float, 7> DEFAULT_LAYER_H_MULT = { 0.6f, 0.4f, 0.2f, 0.1f, 0.1f, 0.1f, 0.1f };
    // Default per-layer vertical multipliers (multiplies computed vFactor)
    inline static const std::array<float, 7> DEFAULT_LAYER_V_MULT = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };

private:
    std::vector<SDL_Texture*> m_textures;
    std::vector<float> m_factors; // vertical parallax factor per layer (0..1)
    std::vector<float> m_hFactors; // horizontal parallax factor per layer (0..1)
    std::vector<float> m_scrollOffsets; // autonomous horizontal scroll offsets (pixels)
    std::vector<float> m_scrollSpeeds;  // autonomous horizontal scroll speeds (pixels/sec)
    std::vector<char>  m_autoScroll;    // flags: whether this layer scrolls by itself
    std::vector<int> m_levels;    // level IDs this background applies to
    int m_layers = 0;
    std::string m_dir;
};
