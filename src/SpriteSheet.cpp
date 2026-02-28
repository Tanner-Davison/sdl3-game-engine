#include "SpriteSheet.hpp"
#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <fstream>
#include <print>
#include <sstream>

SpriteSheet::SpriteSheet(const std::string& imageFile, const std::string& coordFile)
    : surface(nullptr) {
    // Load the sprite sheet image
    surface = IMG_Load(imageFile.c_str());
    if (!surface) {
        std::print("Failed to load sprite sheet: {}\n{}\n", imageFile, SDL_GetError());
        return;
    }

    SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);

    // Load the coordinate data
    LoadCoordinates(coordFile);
}

SpriteSheet::SpriteSheet(const std::string& directory, const std::string& prefix, int frameCount, int targetW, int targetH, int padDigits)
    : surface(nullptr) {
    std::string dir = directory;
    if (!dir.empty() && dir.back() != '/')
        dir += '/';

    std::vector<SDL_Surface*> frameSurfaces;
    int frameW = 0, frameH = 0;

    // Determine start index: padded sequences start at 0, unpadded at 1
    int startIdx = (padDigits > 0) ? 0 : 1;
    int endIdx   = startIdx + frameCount - 1;

    for (int i = startIdx; i <= endIdx; i++) {
        std::string numStr = std::to_string(i);
        if (padDigits > 0)
            numStr = std::string(std::max(0, padDigits - (int)numStr.size()), '0') + numStr;
        std::string  path = dir + prefix + numStr + ".png";
        SDL_Surface* s    = IMG_Load(path.c_str());
        if (!s) {
            std::print("Failed to load frame: {}\n{}\n", path, SDL_GetError());
            for (auto* f : frameSurfaces) SDL_DestroySurface(f);
            return;
        }
        // Scale down if target dimensions were provided
        if (targetW > 0 && targetH > 0) {
            SDL_Surface* scaled = SDL_CreateSurface(targetW, targetH, s->format);
            SDL_SetSurfaceBlendMode(scaled, SDL_BLENDMODE_BLEND);
            SDL_Rect src  = {0, 0, s->w, s->h};
            SDL_Rect dest = {0, 0, targetW, targetH};
            SDL_BlitSurfaceScaled(s, &src, scaled, &dest, SDL_SCALEMODE_LINEAR);
            SDL_DestroySurface(s);
            s = scaled;
        }
        if (i == startIdx) {
            frameW = s->w;
            frameH = s->h;
        }
        frameSurfaces.push_back(s);
    }

    if (frameSurfaces.empty()) return;

    surface = SDL_CreateSurface(frameW * frameCount, frameH, frameSurfaces[0]->format);
    if (!surface) {
        std::print("Failed to create stitched surface: {}\n", SDL_GetError());
        for (auto* f : frameSurfaces) SDL_DestroySurface(f);
        return;
    }
    SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);

    for (int i = 0; i < static_cast<int>(frameSurfaces.size()); i++) {
        SDL_Rect dest = {i * frameW, 0, frameW, frameH};
        SDL_SetSurfaceBlendMode(frameSurfaces[i], SDL_BLENDMODE_NONE);
        SDL_BlitSurface(frameSurfaces[i], nullptr, surface, &dest);
        int         frameIdx = startIdx + i;
        std::string frameKey = std::to_string(frameIdx);
        if (padDigits > 0)
            frameKey = std::string(std::max(0, padDigits - (int)frameKey.size()), '0') + frameKey;
        frames[prefix + frameKey] = {i * frameW, 0, frameW, frameH};
        SDL_DestroySurface(frameSurfaces[i]);
    }

    std::print("Loaded {} frames from directory: {}\n", frameCount, dir);
}

SpriteSheet::~SpriteSheet() {
    if (surface) {
        SDL_DestroySurface(surface);
    }
}

void SpriteSheet::LoadCoordinates(const std::string& coordFile) {
    // Detect file format by extension or content
    if (coordFile.find(".xml") != std::string::npos) {
        LoadXMLFormat(coordFile);
    } else {
        LoadTextFormat(coordFile);
    }
}

void SpriteSheet::LoadTextFormat(const std::string& coordFile) {
    std::ifstream file(coordFile);
    if (!file.is_open()) {
        std::print("Failed to open coordinate file: {}", coordFile);
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#')
            continue;

        // Parse format: "name = x y w h"
        std::istringstream iss(line);
        std::string        name, equals;
        int                x, y, w, h;

        if (iss >> name >> equals >> x >> y >> w >> h) {
            frames[name] = {x, y, w, h};
        }
    }

    std::print("Loaded {} frames from the sprite sheet\n", frames.size());
}

void SpriteSheet::LoadXMLFormat(const std::string& coordFile) {
    std::ifstream file(coordFile);
    if (!file.is_open()) {
        std::print("Failed to open coordinate file: {}", coordFile);
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Look for SubTexture tags
        if (line.find("<SubTexture") == std::string::npos)
            continue;

        // Extract attributes using simple string parsing
        std::string name;
        int         x = 0, y = 0, width = 0, height = 0;

        // Extract name
        size_t namePos = line.find("name=\"");
        if (namePos != std::string::npos) {
            namePos += 6; // Skip 'name="'
            size_t nameEnd = line.find("\"", namePos);
            name           = line.substr(namePos, nameEnd - namePos);

            // Remove .png extension if present
            if (name.size() > 4 && name.substr(name.size() - 4) == ".png") {
                name = name.substr(0, name.size() - 4);
            }
        }

        // Extract x
        size_t xPos = line.find("x=\"");
        if (xPos != std::string::npos) {
            xPos += 3;
            size_t xEnd = line.find("\"", xPos);
            x           = std::stoi(line.substr(xPos, xEnd - xPos));
        }

        // Extract y
        size_t yPos = line.find("y=\"");
        if (yPos != std::string::npos) {
            yPos += 3;
            size_t yEnd = line.find("\"", yPos);
            y           = std::stoi(line.substr(yPos, yEnd - yPos));
        }

        // Extract width
        size_t wPos = line.find("width=\"");
        if (wPos != std::string::npos) {
            wPos += 7;
            size_t wEnd = line.find("\"", wPos);
            width       = std::stoi(line.substr(wPos, wEnd - wPos));
        }

        // Extract height
        size_t hPos = line.find("height=\"");
        if (hPos != std::string::npos) {
            hPos += 8;
            size_t hEnd = line.find("\"", hPos);
            height      = std::stoi(line.substr(hPos, hEnd - hPos));
        }

        if (!name.empty()) {
            frames[name] = {x, y, width, height};
        }
    }

    // std::cout << "Loaded " << frames.size() << " frames from XML sprite
    // sheet"
    //           << std::endl;
}

SDL_Rect SpriteSheet::GetFrame(const std::string& name) const {
    auto it = frames.find(name);
    if (it != frames.end()) {
        return it->second;
    }
    std::print("Frame not found: {}\n", name);
    return {0, 0, 0, 0};
}

std::vector<SDL_Rect> SpriteSheet::GetAnimation(const std::string& baseName) const {
    std::vector<SDL_Rect> animation;

    // Collect all frames that start with baseName
    std::vector<std::pair<std::string, SDL_Rect>> matchingFrames;

    for (const auto& [name, rect] : frames) {
        if (name.find(baseName) == 0) {
            matchingFrames.push_back({name, rect});
        }
    }

    // Sort numerically by the number suffix to avoid "Gold_10" sorting before "Gold_2"
    std::sort(matchingFrames.begin(),
              matchingFrames.end(),
              [&baseName](const auto& a, const auto& b) {
                  auto numStr = [&](const std::string& s) {
                      std::string n = s.substr(baseName.size());
                      // strip any non-digit prefix left (e.g. walk01 vs walk1)
                      return n.empty() ? 0 : std::stoi(n);
                  };
                  return numStr(a.first) < numStr(b.first);
              });

    // Extract just the rects
    for (const auto& [name, rect] : matchingFrames) {
        animation.push_back(rect);
    }

    return animation;
}
