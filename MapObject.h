#pragma once
#include <SDL3/SDL.h>
#include <string>
#include "Map.h"

// forward
class GameObject;
class AnimationManager;

class MapObject {
public:
    MapObject(int tileX, int tileY, int tileIndex);
    virtual ~MapObject();

    virtual void update(GameObject& obj, Map& map);
    virtual void draw(SDL_Renderer* renderer, int camX, int camY, const Map& map) const;

    // Load an animated sprite for this map object. Returns true on success.
    bool loadAnimation(SDL_Renderer* renderer, const std::string& path,
                       int frameW, int frameH, int frames, int rowY,
                       int innerX = 0, int innerY = 0, int innerW = 0, int innerH = 0,
                       int speed = 12);

    // Manually set object's pixel size (defaults to TILE_SIZE)
    void setSize(int width, int height) { w = width; h = height; }

    // Offset object's position in pixels
    void offsetPosition(int dx, int dy);

    // Query pixel size
    int getWidth() const { return w; }
    int getHeight() const { return h; }

    SDL_FRect getRect();
    bool active = true;

    // If true, this object should be drawn behind the player even if it's taller than a tile.
    bool drawBehind = false;

    int getTileX() const { return tx; }
    int getTileY() const { return ty; }
    int getTileIndex() const { return tileIndex; }

protected:
    int tx, ty;
    int tileIndex;
    float x, y;
    int w, h;

    // Optional animation/texture for this map object
    SDL_Texture* animTexture = nullptr;
    AnimationManager* anim = nullptr;
    int animFrameW = 0;
    int animFrameH = 0;
};