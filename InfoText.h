#pragma once
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <string>
#include <vector>
#include <unordered_map>

class Camera;
class Player;
class Map;

// Draw informational text in world-space affected by camera.
// - 16px font by default
// - inline 16x16 icons referenced as {ID} in markup
// - fades in as player approaches trigger rect
class InfoText {
public:
    InfoText(SDL_Renderer* renderer, const std::string& fontPath, int fontSize = 16);
    ~InfoText();

    // trigger rect/fade removed: visibility is controlled by spawn tile proximity
    void setActive(bool a) { active = a; }

    // Load an icon (expected 16x16) and associate with id used in markup (e.g. "A")
    bool loadIcon(const std::string& id, const std::string& path);

    // Markup example: "press {A} to jump"
    void setMessage(const std::string& markup);
    // Lightweight helper that supports inline image tokens of the form
    // "press [img:Assets/Sprites/space.png] to jump". This will load the
    // referenced image automatically and embed it as a 16x16 icon.
    void setText(const std::string& markup);

    // Update fade based on player world position
    // New update API: supply the markup text, player's world X and map pointer.
    // InfoText will locate spawn tiles equal to triggerTileId and show the text
    // when the player's X is within `showRange` pixels of a trigger tile.
    void update(const std::string& markup, float playerWorldX, const Map* map, float showRange = 168.0f);
    // Convenience: update automatically using built-in per-level messages and input-device icons
    // supply current level id so InfoText can choose which hint to show
    void updateAuto(int currentLevelID, float playerWorldX, const Map* map);

    // Inform InfoText which input device was used last (true = controller, false = keyboard)
    void setLastInputIsController(bool v) { lastInputIsController = v; }

    // Set which spawn tile id acts as trigger (default 5)
    void setTriggerTileId(int id) { triggerTileId = id; }

    // Draw applying camera offset
    void draw(SDL_Renderer* renderer, const Camera& cam);

private:
    enum SegType { SEG_TEXT, SEG_ICON, SEG_NEWLINE };
    struct Segment {
        SegType type;
        std::string text; // used if SEG_TEXT
        SDL_Texture* icon = nullptr; // used if SEG_ICON
        std::string iconId; // id used to look up icon metadata
        SDL_Texture* tex = nullptr; // cached rendered text texture for SEG_TEXT
        int w = 0, h = 0;
    };

    void rebuildTextures();
    void clearSegments();

    SDL_Renderer* renderer = nullptr;
    TTF_Font* font = nullptr;
    SDL_Color color{ 212, 216, 220 ,255};
    int fontSize = 16;
    const size_t wrapLen = 24; // wrap after approx 14 chars
    float posX = 0.0f;
    float posY = 0.0f;
    bool active = true;

    std::vector<Segment> segments;
    std::unordered_map<std::string, SDL_Texture*> icons;
    std::unordered_map<std::string, std::pair<int,int>> iconSizes; // preferred or native sizes
    std::string currentMarkup;

    Uint8 currentAlpha = 0;
    bool initedTTF = false;
    int triggerTileId = 5; // default trigger spawn tile id
    float showRange = 64.0f; // how close (in pixels) player must be to show text
public:
    float getShowRange() const { return showRange; }
    float setShowRange(float t)  { showRange = t; }
private:
    bool lastInputIsController = false;

public:
    // Optionally set a preferred draw size for a loaded icon (width, height)
    void setIconPreferredSize(const std::string& id, int w, int h) { iconSizes[id] = {w,h}; }
};
