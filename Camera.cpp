#include "Camera.h"
#include <cmath>

void Camera::update(float playerX, float playerY, int mapWidth, int screenWidth, int mapHeight, int screenHeight, int tileSize, float scale) {
    if (scale < 0.0001f) scale = 1.0f;

    // Visible area in world (unscaled) pixels = screen pixels / scale
    float halfVisibleW = (float)screenWidth / (2.0f * scale);
    float halfVisibleH = (float)screenHeight / (2.0f * scale);

    // Keep your small offsets (6,8) in world pixels so centering still feels right
    x = int(std::round(playerX + 6.0f - halfVisibleW));
    y = int(std::round(playerY + 8.0f - halfVisibleH));

    // Compute visible size in world pixels and clamp
    int visibleW_world = int(std::round((float)screenWidth / scale));
    int visibleH_world = int(std::round((float)screenHeight / scale));

    int maxX = mapWidth * tileSize - visibleW_world;
    int maxY = mapHeight * tileSize - visibleH_world;
    if (maxX < 0) maxX = 0;
    if (maxY < 0) maxY = 0;

    if (x < 0) x = 0;
    if (x > maxX) x = maxX;
    if (y < 0) y = 0;
    if (y > maxY) y = maxY;
}
