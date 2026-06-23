#pragma once

#include "core/types.hpp"
#include <cstdint>
#include <algorithm>
#include <cmath>

namespace fap {

struct FloatPixel { float r, g, b, a; };

inline FloatPixel unpackPixel(uint32_t p) {
    return {
        static_cast<float>(p & 0xFF) / 255.0f,
        static_cast<float>((p >> 8) & 0xFF) / 255.0f,
        static_cast<float>((p >> 16) & 0xFF) / 255.0f,
        static_cast<float>((p >> 24) & 0xFF) / 255.0f
    };
}

inline uint32_t packPixel(const FloatPixel& fp) {
    auto clampRound = [](float v) -> uint8_t {
        return static_cast<uint8_t>(std::clamp(std::round(v * 255.0f), 0.0f, 255.0f));
    };
    return (static_cast<uint32_t>(clampRound(fp.r)) << 0)
         | (static_cast<uint32_t>(clampRound(fp.g)) << 8)
         | (static_cast<uint32_t>(clampRound(fp.b)) << 16)
         | (static_cast<uint32_t>(clampRound(fp.a)) << 24);
}

inline float blendChannel(float src, float dst, BlendMode mode) {
    switch (mode) {
    case BlendMode::Normal:     return src;
    case BlendMode::Multiply:   return src * dst;
    case BlendMode::Screen:     return 1.0f - (1.0f - src) * (1.0f - dst);
    case BlendMode::Overlay:    return (dst < 0.5f) ? 2.0f * src * dst : 1.0f - 2.0f * (1.0f - src) * (1.0f - dst);
    case BlendMode::Add:        return std::min(1.0f, src + dst);
    case BlendMode::Subtract:   return std::max(0.0f, dst - src);
    case BlendMode::Darken:     return std::min(src, dst);
    case BlendMode::Lighten:    return std::max(src, dst);
    case BlendMode::ColorBurn:  return (src > 0.0f) ? 1.0f - std::min(1.0f, (1.0f - dst) / src) : 0.0f;
    case BlendMode::ColorDodge: return (src < 1.0f) ? std::min(1.0f, dst / (1.0f - src)) : 1.0f;
    case BlendMode::SoftLight: {
        if (src <= 0.5f) return dst - (1.0f - 2.0f * src) * dst * (1.0f - dst);
        if (dst <= 0.25f) return dst + (2.0f * src - 1.0f) * (((16.0f * dst - 12.0f) * dst + 4.0f) * dst - dst);
        return dst + (2.0f * src - 1.0f) * (std::sqrt(dst) - dst);
    }
    case BlendMode::HardLight:  return (src < 0.5f) ? 2.0f * src * dst : 1.0f - 2.0f * (1.0f - src) * (1.0f - dst);
    default:                    return src;
    }
}

inline void blendPixel(uint32_t* pixels, int width, int height, int x, int y,
                       float r, float g, float b, float a, BlendMode mode) {
    if (x < 0 || x >= width || y < 0 || y >= height || a <= 0.0f) return;

    size_t idx = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
    FloatPixel dst = unpackPixel(pixels[idx]);
    FloatPixel src = {r, g, b, a};

    float blendedR = blendChannel(src.r, dst.r, mode);
    float blendedG = blendChannel(src.g, dst.g, mode);
    float blendedB = blendChannel(src.b, dst.b, mode);

    float finalA = src.a + dst.a * (1.0f - src.a);
    float invA = 1.0f - src.a;

    FloatPixel result;
    result.r = src.a * blendedR + invA * dst.r;
    result.g = src.a * blendedG + invA * dst.g;
    result.b = src.a * blendedB + invA * dst.b;
    result.a = finalA;

    pixels[idx] = packPixel(result);
}

} // namespace fap
