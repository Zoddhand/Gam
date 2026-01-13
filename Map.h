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
    std::vector<int> tiles2;  // optional foreground tile layer (Tile Layer 2)
    std::vector<int> spawn;   // optional second layer
    std::vector<int> collision; // collision layer CSV

    bool loadCSV(const std::string& path);
    bool loadLayer2CSV(const std::string& path); // load Tile Layer 2
    bool loadSpawnCSV(const std::string& path);
    bool loadColCSV(const std::string& path); // collision csv loader
    int getTile(int x, int y) const;

    // Collision semantics:
    // -1 = empty / passable
    //  0..n = solid (engine treats any non -1 as solid)
    //  95 = one-way platform (special behavior)
    static constexpr int COLL_ONEWAY = 2;

    // Return the raw collision value for a tile, or -1 if out of bounds / no collision.
    // This allows callers to implement special behaviors (one-way platforms).
    int getCollision(int tx, int ty) const;

    // Spawn / object tile ids (map uses these tile values to indicate spawns)
    static constexpr int SPAWN_PLAYER = 15;        // player spawn
    static constexpr int SPAWN_ORC = 16;           // orc enemy spawn
    static constexpr int SPAWN_SPIKES = 17;        // spikes (MapObject) spawn
    static constexpr int SPAWN_FALLINGTRAP = 18;   // falling trap spawn
    static constexpr int SPAWN_SKELETON = 19;      // skeleton spawn
    static constexpr int SPAWN_ARCHER = 20;        // archer spawn
    static constexpr int SPAWN_ARROWTRAP_LEFT = 21; // arrow trap that shoots left
    static constexpr int SPAWN_ARROWTRAP_RIGHT = 22; // arrow trap that shoots right
    static constexpr int SPAWN_SLIME = 23; // arrow trap that shoots right
    static constexpr int SPAWN_WATER = 11; // water spawn (tile id 11)
    static constexpr int SPAWN_WATERFALL_DAY = 360; // water spawn (tile id 11)
    static constexpr int SPAWN_DOOR = 423; // door spawn (custom)
    static constexpr int SPAWN_KEY = 10; // door spawn (custom)

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
    // Draw foreground tile layer (Tile Layer 2) on top of entities
    void drawForeground(SDL_Renderer* renderer, int camX, int camY);

    // Check if a tile is solid (-1 is passable; object tiles are passable)
    bool isSolid(int tx, int ty);

    // Load a tileset from an image (16x16 tiles)
    bool loadTileset(SDL_Renderer* renderer, const std::string& path);

    // Return object spawn tiles (tile value, tile coordinates)
    std::vector<ObjectSpawn> getObjectSpawns() const;
};
