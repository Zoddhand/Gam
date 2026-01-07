#pragma once
#include <SDL3/SDL.h>
#include <cstring>
#include <string>
#include <vector>

class Map {
public:
    static constexpr int TILE_SIZE = 16;

    int width = 0;
    int height = 0;

    std::vector<int> tiles;   // size = width * height
    std::vector<int> spawn;   // optional second layer

    bool loadCSV(const std::string& path);
    bool loadSpawnCSV(const std::string& path);
    int getTile(int x, int y) const;

    // Spawn / object tile ids (map uses these tile values to indicate spawns)
    static constexpr int SPAWN_PLAYER = 80;        // player spawn
    static constexpr int SPAWN_ORC = 81;           // orc enemy spawn
    static constexpr int SPAWN_SPIKES = 82;        // spikes (MapObject) spawn
    static constexpr int SPAWN_FALLINGTRAP = 83;   // falling trap spawn

    // Backwards-compatible aliases used elsewhere in the codebase
    static constexpr int TRAP_TILE = SPAWN_SPIKES;
    static constexpr int FALLINGTRAP_TILE = SPAWN_FALLINGTRAP;

    struct ObjectSpawn {
        int x;           // tile X
        int y;           // tile Y
        int tileIndex;   // the value found in tiles[y][x]
        std::string targetLevel;
    };

    // Tileset info
    SDL_Texture* tilesetTexture = nullptr;
    int tilesetWidth = 0;
    int tilesetHeight = 0;
    int tileCols = 0;
    int tileRows = 0;

    // Constructor
    Map();

    // Draw map to the screen, using camera X offset
    void draw(SDL_Renderer* renderer, int camX, int camY);

    // Check if a tile is solid (-1 is passable; object tiles are passable)
    bool isSolid(int tx, int ty);

    // Load a tileset from an image (16x16 tiles)
    bool loadTileset(SDL_Renderer* renderer, const std::string& path);

    // Return object spawn tiles (tile value, tile coordinates)
    std::vector<ObjectSpawn> getObjectSpawns() const;
};
