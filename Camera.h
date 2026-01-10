#pragma once

class Camera {
public:
    int x = 0;
    int y = 0;
    // scale is float now so non-integer zoom (1.5, 2.0, etc.) works
    void update(float playerX, float playerY, int mapWidth, int screenWidth, int mapHeight, int screenHeight, int tileSize, float scale = 1.0f);

    // Trigger a screen shake: duration measured in update ticks, magnitude in pixels
    void shake(int durationTicks, int magnitude);

private:
    int shakeTicksLeft = 0;
    int shakeDuration = 0;
    int shakeMag = 0;
};  
