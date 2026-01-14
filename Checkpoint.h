#pragma once
#include "MapObject.h"

class Checkpoint : public MapObject {
public:
    Checkpoint(int tileX, int tileY, int tileIndex);
    ~Checkpoint() override;

    void update(GameObject& obj, Map& map) override;
    void draw(SDL_Renderer* renderer, int camX, int camY, const Map& map) const override;

    void setLevel(int level) { levelID = level; }
    void markActivated();

private:
    bool activated = false;
    int animTimer = 0;
    int animInterval = 12; // ticks per frame when active
    int levelID = -1;
};
