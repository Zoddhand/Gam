#include "InfoText.h"
#include "Camera.h"
#include "Player.h"
#include "Map.h"

#include <sstream>
#include <iostream>

InfoText::InfoText(SDL_Renderer* ren, const std::string& fontPath, int fsize)
    : renderer(ren), fontSize(fsize)
{
    if (!renderer) return;

    if (TTF_WasInit() == 0) {
        if (TTF_Init() != 0) {
            SDL_Log("InfoText: Failed to init TTF: %s", SDL_GetError());
            initedTTF = false;
        } else {
            initedTTF = true;
        }
    } else {
        initedTTF = true;
    }

    if (initedTTF) {
        font = TTF_OpenFont(fontPath.c_str(), fontSize);
        if (!font) {
            SDL_Log("InfoText: Failed to open font '%s': %s", fontPath.c_str(), SDL_GetError());
        }
    }
}

// Convenience: automatically choose per-level hint text and icons.
// This hardcodes a small set of level messages and uses keyboard vs controller
// icon paths depending on the last input device.
void InfoText::updateAuto(int currentLevelID, float playerWorldX, const Map* map)
{
    if (!map) return;

    // Hardcoded icon paths (keyboard vs controller)
    const std::string KB_JUMP = ("Assets/Icons/jump_kb.png");
    const std::string CONT_JUMP = "Assets/Icons/jump_cont.png";
    const std::string KB_ATTACK = "Assets/Icons/attack_kb.png";
    const std::string CONT_ATTACK = "Assets/Icons/attack_cont.png";
    const std::string KB_SPECIAL = "Assets/Icons/special_kb.png";
    const std::string CONT_SPECIAL = "Assets/Icons/special_cont.png";
    const std::string KB_DODGE = "Assets/Icons/dodge_kb.png";
    const std::string CONT_DODGE = "Assets/Icons/dodge_cont.png";
    const std::string KB_UP = "Assets/Icons/up_kb.png";
    const std::string CONT_UP = "Assets/Icons/up_cont.png";
    const std::string KB_DOWN = "Assets/Icons/down_kb.png";
    const std::string CONT_DOWN = "Assets/Icons/down_cont.png";
    const std::string KB_LEFT = "Assets/Icons/left_kb.png";
    const std::string CONT_LEFT = "Assets/Icons/left_cont.png";
    const std::string KB_RIGHT = "Assets/Icons/right_kb.png";
    const std::string CONT_RIGHT = "Assets/Icons/right_cont.png";

    // Choose which set to use depending on last used input device
    bool cont = lastInputIsController;

    // Build per-level markup. Add cases here for any levels that require hints.
    std::string markup;
    // Ensure icons are loaded and set preferred sizes when necessary.
    auto ensureLoad = [&](const std::string &path, int prefW = -1, int prefH = -1) {
        if (icons.find(path) == icons.end()) {
            loadIcon(path, path);
        }
        if (prefW > 0 && prefH > 0) {
            setIconPreferredSize(path, prefW, prefH);
        }
    };

    // Example: jump_kb is a 32x16 image and should be drawn at that size
    ensureLoad(KB_JUMP, 32, 16);
    // load controller jump icon at default size (defaults to 16x16)
    ensureLoad(CONT_JUMP);
    switch (currentLevelID) {
    case 22:
        markup = std::string("Press [img:") + (cont ? CONT_JUMP : KB_JUMP ) + "] to Jump";
        break;
    case 23:
        markup = std::string("Press [img:") + (cont ? CONT_JUMP : KB_JUMP ) + "] Twice to Double Jump";
        break;
    case 24:
        // example: attack hint (two icons inline)
        markup = std::string("Press [img:") + (cont ? CONT_DOWN : KB_DOWN) + "]" + " + " + std::string("[img:") + (cont ? CONT_JUMP : KB_JUMP) + "] to Drop Down";
        break;
    case 25:
        // directional hint
        markup = std::string("Press [img:") + (cont ? CONT_ATTACK : KB_ATTACK) + "] to Attack";
        break;
    case 26:
        // directional hint
        markup = std::string("Watch out for Spikes!");
        break;
    case 27:
        // directional hint
        markup = std::string("Find Lamp Post to Save your Progress");
        break;
    case 28:
        // directional hint
        markup = std::string("Better Hurry!");
        break;
    case 32:
        // directional hint
        markup = std::string("[img:") + (cont ? CONT_UP : KB_UP) + "] To Enter";
        break;
    default:
        // no automatic hint for this level
        markup.clear();
        break;
    }

    // Delegate to the general update which finds trigger tiles and positions the text
    update(markup, playerWorldX, map, showRange);
}

InfoText::~InfoText()
{
    // free segment text textures
    for (auto &s : segments) {
        if (s.tex) SDL_DestroyTexture(s.tex);
    }
    for (auto &p : icons) {
        if (p.second) SDL_DestroyTexture(p.second);
    }
    //if (font) TTF_CloseFont(font);
    // Do not call TTF_Quit here; other code may use it
}

bool InfoText::loadIcon(const std::string& id, const std::string& path)
{
    if (!renderer) return false;
    SDL_Surface* surf = IMG_Load(path.c_str());
    if (!surf) {
        SDL_Log("InfoText: failed to load icon '%s' (%s)", path.c_str(), SDL_GetError());
        return false;
    }
    SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_free(surf);
    if (!t) {
        SDL_Log("InfoText: failed to create texture from '%s'", path.c_str());
        return false;
    }
    // Use nearest (integer-style) scaling to avoid blurriness for small icons
    SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureAlphaMod(t, 255);
    icons[id] = t;
    // Default icon draw size: 16x16 unless a preferred size is set via setIconPreferredSize
    if (iconSizes.find(id) == iconSizes.end()) {
        iconSizes[id] = { 16, 16 };
    }
    return true;
}

void InfoText::clearSegments()
{
    for (auto &s : segments) {
        if (s.tex) SDL_DestroyTexture(s.tex);
    }
    segments.clear();
}

void InfoText::setMessage(const std::string& markup)
{
    clearSegments();
    // parse markup: text and {ID} tokens
    std::string cur;
    size_t curCount = 0;
    for (size_t i = 0; i < markup.size(); ++i) {
        if (markup[i] == '{') {
            // flush cur
            if (!cur.empty()) {
                Segment seg; seg.type = SEG_TEXT; seg.text = cur; cur.clear(); segments.push_back(seg);
                curCount += seg.text.size();
            }
            // find closing
            size_t j = markup.find('}', i+1);
            if (j == std::string::npos) break;
            std::string id = markup.substr(i+1, j - (i+1));
            Segment seg; seg.type = SEG_ICON;
            auto it = icons.find(id);
            if (it != icons.end()) seg.icon = it->second;
            else seg.icon = nullptr;
            seg.iconId = id;
            segments.push_back(seg);
            // treat icon as a single char for wrapping
            curCount += 1;
            i = j;
        } else {
            cur.push_back(markup[i]);
            curCount++;
            // insert newline segment when exceeding wrap length at a space or when forced
            if (curCount >= wrapLen) {
                // try to break at last space in cur
                size_t lastSpace = cur.find_last_of(' ');
                if (lastSpace != std::string::npos) {
                    // split at lastSpace
                    std::string left = cur.substr(0, lastSpace);
                    std::string right = cur.substr(lastSpace + 1);
                    if (!left.empty()) { Segment seg; seg.type = SEG_TEXT; seg.text = left; segments.push_back(seg); }
                    // push newline
                    Segment nl; nl.type = SEG_NEWLINE; segments.push_back(nl);
                    // continue with right part
                    cur = right;
                    curCount = cur.size();
                } else {
                    // no space found, force break
                    if (!cur.empty()) { Segment seg; seg.type = SEG_TEXT; seg.text = cur; segments.push_back(seg); }
                    Segment nl; nl.type = SEG_NEWLINE; segments.push_back(nl);
                    cur.clear(); curCount = 0;
                }
            }
        }
    }
    if (!cur.empty()) {
        Segment seg; seg.type = SEG_TEXT; seg.text = cur; segments.push_back(seg);
    }
    rebuildTextures();
}

void InfoText::setText(const std::string& markup)
{
    // Avoid repeated work if message unchanged
    if (markup == currentMarkup) return;
    currentMarkup = markup;
    // Simple convenience: support token [img:PATH]
    std::string out = markup;
    size_t p = 0;
    while ((p = out.find("[img:", p)) != std::string::npos) {
        size_t q = out.find(']', p+5);
        if (q == std::string::npos) break;
        std::string path = out.substr(p+5, q - (p+5));
        // generate id from path
        std::string id = path; // use full path as id
        // load if not loaded
        if (icons.find(id) == icons.end()) {
            loadIcon(id, path);
        }
        // replace [img:PATH] with {ID}
        out.replace(p, q - p + 1, std::string("{") + id + "}");
        p += 1;
    }
    setMessage(out);
}

void InfoText::rebuildTextures()
{
    if (!renderer || !initedTTF || !font) return;
    for (auto &s : segments) {
            if (s.type == SEG_TEXT) {
            if (s.tex) { SDL_DestroyTexture(s.tex); s.tex = nullptr; }
            if (s.text.empty()) continue;
            // render text to surface (use same API pattern as the rest of the project)
            SDL_Surface* surf = TTF_RenderText_Solid(font, s.text.c_str(), 0, color);
            if (!surf) continue;
            s.w = surf->w;
            s.h = surf->h;
            s.tex = SDL_CreateTextureFromSurface(renderer, surf);
            SDL_free(surf);
            if (s.tex) {
                SDL_SetTextureBlendMode(s.tex, SDL_BLENDMODE_BLEND);
                SDL_SetTextureAlphaMod(s.tex, 255);
                // ensure text textures are nearest scaled to keep pixel-art crisp
                SDL_SetTextureScaleMode(s.tex, SDL_SCALEMODE_NEAREST);
            }
        } else if (s.type == SEG_ICON) {
            if (s.icon) {
                // query icon size (SDL_GetTextureSize uses floats in this codebase)
                float fw = 0.0f, fh = 0.0f;
                SDL_GetTextureSize(s.icon, &fw, &fh);
                s.w = int(fw); s.h = int(fh);
                // if a preferred size was set for this icon id, use it
                if (!s.iconId.empty()) {
                    auto it = iconSizes.find(s.iconId);
                    if (it != iconSizes.end()) {
                        s.w = it->second.first;
                        s.h = it->second.second;
                    }
                }
            } else {
                // placeholder size 16x16
                s.w = 16; s.h = 16;
            }
        } else if (s.type == SEG_NEWLINE) {
            // newline has zero size and forces line break during layout
            s.w = 0; s.h = 0;
        }
    }
}

// Note: old fade/update-by-distance removed. Visibility is controlled by active flag
// which is set in update(markup, player, map).

// New API: show/hide based on horizontal proximity to any trigger tile
void InfoText::update(const std::string& markup, float playerWorldX, const Map* map, float showRange)
{
    // simple hide when empty
    // Hide when empty or no map provided
    if (markup.empty() || !map) {
        currentMarkup.clear();
        clearSegments();
        active = false;
        currentAlpha = 0;
        return;
    }

    // Find a trigger tile and compute its world position (first occurrence)
    int foundIdx = -1;
    for (size_t i = 0; i < map->spawn.size(); ++i) {
        if (map->spawn[i] == triggerTileId) { foundIdx = int(i); break; }
    }
    if (foundIdx < 0) {
        // no trigger present
        currentMarkup.clear();
        clearSegments();
        active = false; currentAlpha = 0; return;
    }

    int ttx = foundIdx % map->width;
    int tty = foundIdx / map->width;
    float tileWorldX = float(ttx * Map::TILE_SIZE);

    // Visibility is a simple horizontal proximity check (no fade)
    if (fabs(playerWorldX - tileWorldX) <= showRange) {
        // Set the text (this builds textures and sets segment widths)
        setText(markup);
        // compute total width of rendered segments
        int totalW = 0;
        for (auto &s : segments) totalW += s.w;
        // Center the text horizontally on the spawn tile center
        float tileCenterX = tileWorldX + float(Map::TILE_SIZE) * 0.5f;
        posX = tileCenterX - float(totalW) * 0.5f;
        posY = float(tty * Map::TILE_SIZE) - float(fontSize) - 2.0f;
        setActive(true);
        currentAlpha = 255;
    } else {
        setText("");
        setActive(false);
        currentAlpha = 0;
    }
}

void InfoText::draw(SDL_Renderer* ren, const Camera& cam)
{
    if (!ren) return;
    if (currentAlpha == 0) return;

    // compute world -> screen
    float sx = posX - cam.x;
    float sy = posY - cam.y;

    // draw segments left to right
    float x = sx;
    float y = sy;
    float lineStartX = x;
    for (auto &s : segments) {
        if (s.type == SEG_TEXT) {
            if (!s.tex) continue;
            // render drop shadow first (offset by 1px)
            SDL_SetTextureColorMod(s.tex, 0x00, 0x00, 0x00);
            SDL_SetTextureAlphaMod(s.tex, (Uint8)(currentAlpha * 0.6f));
            SDL_FRect sdst{ x + 1.0f, y + 1.0f, float(s.w), float(s.h) };
            SDL_RenderTexture(ren, s.tex, nullptr, &sdst);
            // render main text
            SDL_SetTextureColorMod(s.tex, 0xFF, 0xFF, 0xFF);
            SDL_SetTextureAlphaMod(s.tex, currentAlpha);
            SDL_FRect dst{ x, y, float(s.w), float(s.h) };
            SDL_RenderTexture(ren, s.tex, nullptr, &dst);
            x += s.w;
        } else if (s.type == SEG_ICON) {
            if (s.icon) {
                SDL_SetTextureColorMod(s.icon, 0xFF, 0xFF, 0xFF);
                SDL_SetTextureAlphaMod(s.icon, currentAlpha);
                // draw using the segment's computed width/height (respects preferred sizes)
                float dw = float(s.w > 0 ? s.w : 16);
                float dh = float(s.h > 0 ? s.h : 16);
                // align icon vertically to text baseline (approx)
                float iy = y; // keep top-aligned for simplicity
                SDL_FRect dst{ x, iy, dw, dh };
                SDL_RenderTexture(ren, s.icon, nullptr, &dst);
            } else {
                // draw placeholder
                SDL_SetRenderDrawColor(ren, 255, 200, 0, currentAlpha);
                SDL_FRect dst{ x, y, 16.0f, 16.0f };
                SDL_RenderFillRect(ren, &dst);
            }
            // advance by segment width (or default 16)
            x += float(s.w > 0 ? s.w : 16);
        } else if (s.type == SEG_NEWLINE) {
            // move to next line
            x = lineStartX;
            y += float(fontSize) + 2.0f; // line height
        }
    }
    // No fade behavior; alpha is managed by update()
}
