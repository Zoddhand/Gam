#pragma once

class Camera {
public:
    int x = 0;
    int y = 0;
    // scale is float now so non-integer zoom (1.5, 2.0, etc.) works
    void update(float playerX, float playerY, int mapWidth, int screenWidth, int mapHeight, int screenHeight, int tileSize, float scale = 1.0f);
};  
