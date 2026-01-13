#pragma once
#include "MapObject.h"
#include "Map.h"
#include <vector>

class FallingPlatform : public MapObject {
public:
    FallingPlatform(int leftTileX, int tileY, const std::vector<int>& tileIndices);
    ~FallingPlatform() override;

    void update(GameObject& obj, Map& map) override;
    void draw(SDL_Renderer* renderer, int camX, int camY, const Map& map) const override;

private:
    bool triggered = false;
    bool falling = false;
    int delayTicks = 30; // ~1 second at 60 FPS
    int delayTimer = 0;
    float velY = 0.0f;
    float gravity = 0.35f;

    // reset support
    int resetTicks = 180; // 3 seconds at 60 FPS
    int resetTimer = 0;
    int initTx = 0, initTy = 0, initTileIndex = -1;

    // multi-tile
    std::vector<int> tileIndices;
    int tileCount = 1;

    // collision helpers
    void ensureCollisionLayer(Map& map);
    void setCollision(Map& map, int val);
};
