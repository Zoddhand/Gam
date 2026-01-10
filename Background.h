#pragma once
#include <SDL3/SDL.h>
#include <SDL3_IMAGE/SDL_image.h>
#include <vector>
#include <string>

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
    void draw(SDL_Renderer* renderer, int camX, int camY, int screenW, int screenH);

    // Update autonomous scrolling (dt seconds)
    void update(float dt);

    // Associate specific level IDs with this background
    void addLevel(int levelID);
    bool matchesLevel(int levelID) const;

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
