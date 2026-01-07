#pragma once
#include "MapObject.h"

class Spikes : public MapObject {
public:
    Spikes(int tileX, int tileY, int tileIndex);
    ~Spikes() override;

    void update(GameObject& obj, Map& map) override;
};