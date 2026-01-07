#pragma once
#include <SDL3/SDL.h>
#include "Map.h"

// forward
class GameObject;

class MapObject {
public:
    MapObject(int tileX, int tileY, int tileIndex);
    virtual ~MapObject();

    virtual void update(GameObject& obj, Map& map);
    virtual void draw(SDL_Renderer* renderer, int camX, int camY, const Map& map) const;

    SDL_FRect getRect();
    bool active = true;

    int getTileX() const { return tx; }
    int getTileY() const { return ty; }
    int getTileIndex() const { return tileIndex; }

protected:
    int tx, ty;
    int tileIndex;
    float x, y;
    int w, h;
};