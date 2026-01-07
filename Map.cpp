#include "Map.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <iostream>

Map::Map()
{
   
}

bool Map::loadTileset(SDL_Renderer* renderer, const std::string& path)
{
    SDL_Surface* surf = IMG_Load(path.c_str());
    if (!surf) return false;
    tilesetTexture = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_DestroySurface(surf);   
    if (!tilesetTexture) return false;

    // compute tileset dimensions
    float w = 0, h = 0;
    SDL_GetTextureSize(tilesetTexture, &w, &h);
    tilesetWidth = (int)w;
    tilesetHeight = (int)h;
    tileCols = tilesetWidth / TILE_SIZE;
    tileRows = tilesetHeight / TILE_SIZE;
    return true;
}

void Map::draw(SDL_Renderer* renderer, int camX, int camY)
{
    if (!tilesetTexture) {
        // No tileset; draw simple colored grid so level is visible
        for (int y = 0; y < height; y++)
            for (int x = 0; x < width; x++)
                if (tiles[y][x] >= 0) // now 0 is first tile
                {
                    SDL_FRect r{ float(x * TILE_SIZE - camX), float(y * TILE_SIZE), TILE_SIZE, TILE_SIZE };
                    SDL_SetRenderDrawColor(renderer, 100, 120, 255, 255);
                    SDL_RenderFillRect(renderer, &r);
                }
        return;
    }

    SDL_FRect dest{ 0,0,float(TILE_SIZE), float(TILE_SIZE) };
    SDL_FRect src{ 0,0,float(TILE_SIZE), float(TILE_SIZE) };

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int tileIndex = tiles[y][x];
            if (tileIndex < 0) continue; // -1 is walkable

            int tx = tileIndex % tileCols;
            int ty = tileIndex / tileCols;

            src.x = float(tx * TILE_SIZE);
            src.y = float(ty * TILE_SIZE);

            dest.x = float(x * TILE_SIZE - camX);
            dest.y = float(y * TILE_SIZE - camY);

            SDL_RenderTexture(renderer, tilesetTexture, &src, &dest);
        }
    }
}

std::vector<Map::ObjectSpawn> Map::getObjectSpawns() const
{
    std::vector<ObjectSpawn> out;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int t = spawn[y][x];
            // include any spawn types you want the engine to instantiate
            if (t == SPAWN_PLAYER ||
                t == SPAWN_ORC ||
                t == SPAWN_FALLINGTRAP ||
                t == SPAWN_SPIKES)
            {
                out.push_back({ x, y, t });
            }
        }
    }
    return out;
}

bool Map::isSolid(int tx, int ty)
{
    if (tx < 0 || ty < 0 || tx >= width || ty >= height) return true;
    // trap and chest tiles are treated as non-solid so actors can step on them
    int t = tiles[ty][tx];
    if (t == TRAP_TILE || t == FALLINGTRAP_TILE) return false;
    return tiles[ty][tx] != -1; // only -1 is walkable
}
