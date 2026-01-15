#pragma once
#include "MapObject.h"

class PressurePlate : public MapObject {
public:
    PressurePlate(int tileX, int tileY, int tileIndex);
    ~PressurePlate();

    void update(GameObject& obj, Map& map) override;
    void draw(SDL_Renderer* renderer, int camX, int camY, const Map& map) const override;

    bool isTriggered() const { return triggered; }

private:
    bool triggered = false;
    bool prevTriggered = false;
    int animTimer = 0;
    int animInterval = 12;
};
