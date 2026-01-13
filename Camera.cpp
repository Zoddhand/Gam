#include "Camera.h"
#include <cmath>
#include <cstdlib>

void Camera::shake(int durationTicks, int magnitude) {
    if (durationTicks <= 0 || magnitude <= 0) return;
    shakeDuration = durationTicks;
    shakeTicksLeft = durationTicks;
    shakeMag = magnitude;
}

void Camera::update(float playerX, float playerY, int mapWidth, int screenWidth, int mapHeight, int screenHeight, int tileSize, float scale) {
    if (scale < 0.0001f) scale = 1.0f;

    // Visible area in world (unscaled) pixels = screen pixels / scale
    float halfVisibleW = (float)screenWidth / (2.0f * scale);
    float halfVisibleH = (float)screenHeight / (2.0f * scale);

    // Keep your small offsets (6,8) in world pixels so centering still feels right
    int baseX = int(std::round(playerX + 6.0f - halfVisibleW));
    int baseY = int(std::round(playerY + 8.0f - halfVisibleH));

    // Compute visible size in world pixels and clamp
    int visibleW_world = int(std::round((float)screenWidth / scale));
    int visibleH_world = int(std::round((float)screenHeight / scale));

    // Original maximum allowed X when clamping to map bounds
    int defaultMaxX = mapWidth * tileSize - visibleW_world;
    if (defaultMaxX < 0) defaultMaxX = 0;

    // We want to prevent the camera from showing the first and last tile columns
    // when the map is wide enough to allow it. If the map is too small (<=2 tiles)
    // then fall back to the normal clamping behavior.
    int minX = 0;
    int maxX = defaultMaxX;
    if (mapWidth > 2) {
        minX = tileSize; // don't allow camera to show column 0
        maxX = mapWidth * tileSize - visibleW_world - tileSize; // don't allow camera to show last column
        if (maxX < 0) maxX = 0;
    }

    // Vertical clamping remains unchanged
    int maxY = mapHeight * tileSize - visibleH_world;
    if (maxY < 0) maxY = 0;

    if (baseX < minX) baseX = minX;
    if (baseX > maxX) baseX = maxX;
    if (baseY < 0) baseY = 0;
    if (baseY > maxY) baseY = maxY;

    // If shaking, compute a random offset scaled by remaining shake progress
    int offsetX = 0;
    int offsetY = 0;
    if (shakeTicksLeft > 0) {
        float progress = (float)shakeTicksLeft / (float)shakeDuration; // 1.0 -> 0.0
        int curMag = int(std::round(shakeMag * progress));
        if (curMag < 1) curMag = 1;
        // random offset in [-curMag, curMag]
        offsetX = (std::rand() % (curMag * 2 + 1)) - curMag;
        offsetY = (std::rand() % (curMag * 2 + 1)) - curMag;

        --shakeTicksLeft;
    }

    x = baseX + offsetX;
    y = baseY + offsetY;

    // clamp final
    if (x < minX) x = minX;
    if (x > maxX) x = maxX;
    if (y < 0) y = 0;
    if (y > maxY) y = maxY;
}
