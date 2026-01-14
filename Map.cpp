#include "Map.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>

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

    // Ensure nearest-neighbor sampling so tiles don't bleed into each other when drawn scaled
    SDL_SetTextureScaleMode(tilesetTexture, SDL_SCALEMODE_NEAREST);
    // Ensure alpha blending is enabled for tiles with transparency
    SDL_SetTextureBlendMode(tilesetTexture, SDL_BLENDMODE_BLEND);

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
    // --------------------------------------------------
    // Fallback: draw simple colored tiles if no tileset
    // --------------------------------------------------
    if (!tilesetTexture)
    {
        SDL_SetRenderDrawColor(renderer, 100, 120, 255, 255);

        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                int tileIndex = tiles[y * width + x];
                if (tileIndex < 0)
                    continue;

                SDL_FRect r{
                    float(x * TILE_SIZE - camX),
                    float(y * TILE_SIZE - camY),
                    float(TILE_SIZE),
                    float(TILE_SIZE)
                };

                SDL_RenderFillRect(renderer, &r);
            }
        }
        return;
    }

    // --------------------------------------------------
    // Normal tileset rendering
    // --------------------------------------------------
    SDL_FRect src{
        0.0f,
        0.0f,
        float(TILE_SIZE),
        float(TILE_SIZE)
    };

    SDL_FRect dest{
        0.0f,
        0.0f,
        float(TILE_SIZE),
        float(TILE_SIZE)
    };

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            int tileIndex = tiles[y * width + x];
            if (tileIndex < 0 || tileIndex == 3)
                continue; // walkable / empty

            int tx = tileIndex % tileCols;
            int ty = tileIndex / tileCols;

            // Use integer-aligned source rect to avoid sampling neighboring texels
            src.x = float(tx * TILE_SIZE);
            src.y = float(ty * TILE_SIZE);
            src.w = float(TILE_SIZE);
            src.h = float(TILE_SIZE);

            // Round destination position to integer pixels to avoid sub-pixel interpolation
            float rawX = float(x * TILE_SIZE - camX);
            float rawY = float(y * TILE_SIZE - camY);
            dest.x = std::round(rawX);
            dest.y = std::round(rawY);
            dest.w = float(TILE_SIZE);
            dest.h = float(TILE_SIZE);

            SDL_RenderTexture(renderer, tilesetTexture, &src, &dest);
        }
    }
}



std::vector<Map::ObjectSpawn> Map::getObjectSpawns() const
{
    std::vector<ObjectSpawn> out;

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            int t = spawn[y * width + x];
            if (t < 0)
                continue;

            // Include any spawn types you want the engine to instantiate
            if (t == SPAWN_PLAYER ||
                t == SPAWN_ORC ||
                t == SPAWN_WATER ||
                t == SPAWN_WATERFALL ||
                t == SPAWN_WATERFALL_LONG ||
                t == SPAWN_DUNGEON_WATER ||
                t == SPAWN_WATERFALL_DAY ||
                t == SPAWN_FALLINGTRAP ||
                t == SPAWN_SPIKES ||    
                t == SPAWN_SKELETON ||
                t == SPAWN_SLIME ||
                t == SPAWN_ARCHER ||
                t == SPAWN_KEY ||
                t == SPAWN_CRATE ||
                t == SPAWN_ARROWTRAP_LEFT ||
                t == SPAWN_ARROWTRAP_RIGHT ||
                t == SPAWN_DOOR ||
                t == SPAWN_CHECKPOINT ||
                // falling platform spawn tiles (use numeric ids in spawn CSV)
                (t >= 363 && t <= 366))
            {
                out.push_back({
                    x,   // tile X
                    y,   // tile Y
                    t    // spawn tile value
                    });
            }
        }
    }

    return out;
}


bool Map::isSolid(int tx, int ty)
{
    if (tx < 0 || ty < 0 || tx >= width || ty >= height)
        return true;

    // Treat falling-platform spawn tiles as solid so players can stand on them
    if (!spawn.empty()) {
        int s = spawn[ty * width + tx];
        if (s >= 363 && s <= 366) return true;
    }

    // Use getCollision for consistent semantics
    int c = getCollision(tx, ty);
    return c != -1;
}

int Map::getCollision(int tx, int ty) const
{
    if (tx < 0 || ty < 0 || tx >= width || ty >= height)
        return -1;

    // If a spawn layer exists, treat falling-platform spawn tiles as solid regardless of collision layer
    if (!spawn.empty()) {
        int s = spawn[ty * width + tx];
        if (s >= 363 && s <= 366) return 1; // non -1 = solid
    }

    // If a collision layer exists, return that raw value.
    if (!collision.empty()) {
        return collision[ty * width + tx];
    }

    // Fallback: return tile value (old behaviour)
    return tiles[ty * width + tx];
}



int Map::getTile(int x, int y) const {
    if (x < 0 || y < 0 || x >= width || y >= height)
        return -1;
    return tiles[y * width + x];
}

bool Map::loadCSV(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        SDL_Log("Failed to open map CSV: %s", path.c_str());
        return false;
    }
        
    tiles.clear();
    tiles2.clear();
    width = 0;
    height = 0;

    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string cell;
        int rowWidth = 0;

        while (std::getline(ss, cell, ',')) {
            tiles.push_back(std::stoi(cell));
            rowWidth++;
        }

        if (width == 0)
            width = rowWidth;
        else if (rowWidth != width) {
            SDL_Log("CSV row width mismatch!");
            return false;
        }

        height++;
    }

    return true;
}

bool Map::loadLayer2CSV(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        SDL_Log("Failed to open map Layer2 CSV: %s", path.c_str());
        return false;
    }

    tiles2.clear();
    tiles2.reserve(width * height);

    std::string line;
    int row = 0;

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string cell;
        int col = 0;

        while (std::getline(ss, cell, ',')) {
            tiles2.push_back(std::stoi(cell));
            col++;
        }

        if (col != width) {
            SDL_Log("Layer2 CSV width mismatch at row %d", row);
            return false;
        }

        row++;
    }

    if (row != height) {
        SDL_Log("Layer2 CSV height mismatch: expected %d got %d", height, row);
        return false;
    }

    SDL_Log("Loaded Layer2 CSV: %dx%d", width, height);
    return true;
}

bool Map::loadSpawnCSV(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        SDL_Log("Failed to open spawn CSV: %s", path.c_str());
        return false;
    }

    spawn.clear();
    spawn.reserve(width * height);

    std::string line;
    int row = 0;

    while (std::getline(file, line))
    {
        std::stringstream ss(line);
        std::string cell;
        int col = 0;

        while (std::getline(ss, cell, ','))
        {
            spawn.push_back(std::stoi(cell));
            col++;
        }

        if (col != width) {
            SDL_Log("Spawn CSV width mismatch at row %d", row);
            return false;
        }

        row++;
    }

    if (row != height) {
        SDL_Log("Spawn CSV height mismatch");
        return false;
    }

    SDL_Log("Loaded spawn CSV: %dx%d", width, height);
    return true;
}

bool Map::loadColCSV(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        SDL_Log("Failed to open Collision CSV: %s", path.c_str());
        return false;
    }

    collision.clear();
    collision.reserve(width * height);

    std::string line;
    int row = 0;

    while (std::getline(file, line))
    {
        std::stringstream ss(line);
        std::string cell;
        int col = 0;

        while (std::getline(ss, cell, ','))
        {
            collision.push_back(std::stoi(cell));
            col++;
        }

        if (col != width) {
            SDL_Log("Collision CSV width mismatch at row %d", row);
            return false;
        }

        row++;
    }

    if (row != height) {
        SDL_Log("Loaded Collision CSV: %dx%d", width, height);
        return false;
    }

    SDL_Log("Loaded Collision CSV: %dx%d", width, height);
    return true;
}

// Draw the optional foreground layer (Tile Layer 2) on top of entities
void Map::drawForeground(SDL_Renderer* renderer, int camX, int camY)
{
    if (tiles2.empty()) return;

    // If no tileset, draw fallback rects with a different color so they stand out
    if (!tilesetTexture)
    {
        SDL_SetRenderDrawColor(renderer, 180, 200, 255, 200);
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                int tileIndex = tiles2[y * width + x];
                if (tileIndex < 0) continue;
                SDL_FRect r{ float(x * TILE_SIZE - camX), float(y * TILE_SIZE - camY), float(TILE_SIZE), float(TILE_SIZE) };
                SDL_RenderFillRect(renderer, &r);
            }
        }
        return;
    }

    SDL_FRect src{ 0.0f, 0.0f, float(TILE_SIZE), float(TILE_SIZE) };
    SDL_FRect dest{ 0.0f, 0.0f, float(TILE_SIZE), float(TILE_SIZE) };

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            int tileIndex = tiles2[y * width + x];
            if (tileIndex < 0 || tileIndex == 3) continue;

            int tx = tileIndex % tileCols;
            int ty = tileIndex / tileCols;

            src.x = float(tx * TILE_SIZE);
            src.y = float(ty * TILE_SIZE);
            src.w = float(TILE_SIZE);
            src.h = float(TILE_SIZE);

            float rawX = float(x * TILE_SIZE - camX);
            float rawY = float(y * TILE_SIZE - camY);
            dest.x = std::round(rawX);
            dest.y = std::round(rawY);
            dest.w = float(TILE_SIZE);
            dest.h = float(TILE_SIZE);

            SDL_RenderTexture(renderer, tilesetTexture, &src, &dest);
        }
    }
}