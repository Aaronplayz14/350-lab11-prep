#ifndef TEXTURE_ATLAS_H
#define TEXTURE_ATLAS_H

#include <algorithm>
#include <cassert>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

#include "vector_util.h"
#include <SFML/Graphics.hpp>

// RectPacker maintains two offsets into an overall bin size:
//  - A line y-offset, representing the current horizontal line on which to place new rectangles
//      horizontally along (the line corresponding to the rectangle tops)
//  - An x-offset into the line, representing the x-coordinate to align the next rectangle left-side
// Furthermore it maintains the maximum texture height encountered along the current line.
// The RectPacker places rectangles left to right along this line until it cannot fit the next
//  rectangle. Once this happens, the line is shifted down to meet the bottom of the tallest texture
//  placed on the line, and the x-offset is reset. This continues until the next rectangle
//  packed cannot be fit on the line within the bin space.
class RectPacker {
public:
    RectPacker(const sf::Vector2f& size) : mSize{size} {};

    // pack returns { succeeded, rect id }. If not succeeded, id is irrelevant (but ideally should
    // be set to the maximum possible value).
    std::pair<bool, size_t> pack(const sf::Vector2f& rectSize) {
        if (rectSize.x <= 0.f || rectSize.y <= 0.f || rectSize.x > mSize.x ||
            rectSize.y > mSize.y) {
            return {false, std::numeric_limits<size_t>::max()};
        }

        sf::Vector2f position = mOffset;
        float lineHeight = mLineHeight;
        if (position.x + rectSize.x > mSize.x) {
            position.x = 0.f;
            position.y += lineHeight;
            lineHeight = 0.f;
        }
        if (position.y + rectSize.y > mSize.y) {
            return {false, std::numeric_limits<size_t>::max()};
        }

        size_t id = mBounds.size();
        mBounds.push_back(sf::FloatRect(position, rectSize));
        mOffset = {position.x + rectSize.x, position.y};
        mLineHeight = std::max(lineHeight, rectSize.y);
        return {true, id};
    }

    // Returns the coordinates and size of a previously returned rectangle id.
    // Note that in this coordinate system, y points downwards from the origin at the top-left.
    sf::FloatRect getBounds(size_t rectID) const { return mBounds.at(rectID); }

private:
    sf::Vector2f mSize;
    sf::Vector2f mOffset{0.f, 0.f};
    float mLineHeight = 0.f;
    std::vector<sf::FloatRect> mBounds;
};

// Returns a new image which pages the given img with *pixels* pixels on each side.
// The colour of new border pixels are taken by sampling the closest valid pixel in the original
// image.
//
// NOTE: a production implementation might also support other edge wrapping behaviors, like cycling
// around to the other side of the texture. See https://learnopengl.com/Getting-started/Textures
// Our implementation corresponds to GL_CLAMP_TO_EDGE
inline sf::Image padImage(const sf::Image& img, uint pixels) {
    sf::Vector2u size = img.getSize();
    if (size.x == 0 || size.y == 0) {
        throw std::runtime_error("Cannot pad an empty image");
    }
    if (pixels > (std::numeric_limits<unsigned int>::max() - size.x) / 2 ||
        pixels > (std::numeric_limits<unsigned int>::max() - size.y) / 2) {
        throw std::runtime_error("Padded image is too large");
    }

    sf::Image padded(sf::Vector2u{size.x + 2 * pixels, size.y + 2 * pixels});
    for (unsigned int y = 0; y < padded.getSize().y; ++y) {
        for (unsigned int x = 0; x < padded.getSize().x; ++x) {
            unsigned int sourceX = x < pixels ? 0 : std::min(x - pixels, size.x - 1);
            unsigned int sourceY = y < pixels ? 0 : std::min(y - pixels, size.y - 1);
            padded.setPixel({x, y}, img.getPixel({sourceX, sourceY}));
        }
    }
    return padded;
};

using TextureID = size_t;

class TextureAtlas {
public:
    TextureAtlas(sf::Vector2u binSize, size_t padding)
        : mTexture(binSize), mPacker(sf::Vector2f(mTexture.getSize())), mPadding{padding} {}

    TextureID registerTexture(const std::string& filepath) {
        sf::Image img;
        if (!img.loadFromFile(filepath)) {
            throw std::runtime_error("Failed to load texture: " + filepath);
        }
        if (mPadding > std::numeric_limits<unsigned int>::max()) {
            throw std::runtime_error("Texture padding is too large");
        }
        sf::Image padded = padImage(img, static_cast<unsigned int>(mPadding));
        auto [succeeded, id] = mPacker.pack(sf::Vector2f(padded.getSize()));
        if (!succeeded) {
            throw std::runtime_error("Texture atlas is full: " + filepath);
        }

        sf::FloatRect bounds = mPacker.getBounds(id);
        mTexture.update(padded, sf::Vector2u(bounds.position));
        return id;
    }

    sf::FloatRect getBoundsUV(TextureID id) const {
        sf::FloatRect bounds = mPacker.getBounds(id);
        sf::Vector2f atlasSize(mTexture.getSize());
        float padding = static_cast<float>(mPadding);
        sf::Vector2f position = bounds.position + sf::Vector2f{padding, padding};
        sf::Vector2f size = bounds.size - sf::Vector2f{2.f * padding, 2.f * padding};
        return {{position.x / atlasSize.x, 1.f - (position.y + size.y) / atlasSize.y},
                {size.x / atlasSize.x, size.y / atlasSize.y}};
    }

    sf::Vector2u getSizePixels(TextureID id) const {
        sf::FloatRect bounds = mPacker.getBounds(id);
        float padding = static_cast<float>(mPadding);
        return sf::Vector2u(bounds.size - sf::Vector2f{2.f * padding, 2.f * padding});
    }

    const sf::Texture& getTexture() const { return mTexture; }

private:
    sf::Texture mTexture;
    RectPacker mPacker;
    size_t mPadding;
};

#endif  // TEXTURE_ATLAS_H
