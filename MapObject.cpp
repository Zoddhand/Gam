#include "MapObject.h"
#include "GameObject.h"

MapObject::MapObject(int tileX, int tileY, int tileIndex)
    : tx(tileX), ty(tileY), tileIndex(tileIndex)
{
    x = float(tx * Map::TILE_SIZE);
    y = float(ty * Map::TILE_SIZE);
    w = Map::TILE_SIZE;
    h = Map::TILE_SIZE;
}

MapObject::~MapObject() = default;

void MapObject::update(GameObject& /*player*/, Map& /*map*/)
{
    // default: no logic
}

void MapObject::draw(SDL_Renderer* renderer, int camX, int camY, const Map& map) const
{
    if (!active) return;

    if (map.tilesetTexture) {
        int txIdx = tileIndex % map.tileCols;
        int tyIdx = tileIndex / map.tileCols;
        SDL_FRect src{ float(txIdx * Map::TILE_SIZE), float(tyIdx * Map::TILE_SIZE), float(Map::TILE_SIZE), float(Map::TILE_SIZE) };
        SDL_FRect dest{ float(x - camX), float(y - camY), float(Map::TILE_SIZE), float(Map::TILE_SIZE) };
        SDL_RenderTexture(renderer, map.tilesetTexture, &src, &dest);
    }
    else {
        SDL_FRect dest{ float(x - camX), float(y - camY), float(Map::TILE_SIZE), float(Map::TILE_SIZE) };
        SDL_SetRenderDrawColor(renderer, 200, 30, 30, 255);
        SDL_RenderFillRect(renderer, &dest);
    }
}

SDL_FRect MapObject::getRect() const
{
    return { x, y, float(w), float(h) };
}