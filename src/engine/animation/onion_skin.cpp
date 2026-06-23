#pragma once

#include "core/types.hpp"

#include <vector>
#include <functional>
#include <algorithm>
#include <cmath>

namespace fap {

struct OnionSkinSettings {
    int frames_before = 3;
    int frames_after = 1;
    float opacity = 0.35f;
    Color tint_before = {1.0f, 0.15f, 0.15f, 1.0f};
    Color tint_after = {0.15f, 0.4f, 1.0f, 1.0f};
};

void renderOnionSkin(
    std::vector<uint8_t>& output,
    int width,
    int height,
    const std::function<const uint8_t*(int frame)>& frameProvider,
    int currentFrame,
    int totalFrames,
    const OnionSkinSettings& settings)
{
    const size_t pixelCount = static_cast<size_t>(width) * height;
    const size_t byteCount = pixelCount * 4;

    if (output.size() != byteCount || totalFrames <= 0 || width <= 0 || height <= 0) {
        return;
    }

    const int maxDist = std::max(settings.frames_before, settings.frames_after);
    const float baseOpacity = settings.opacity;

    for (int dir = 0; dir < 2; ++dir) {
        const bool isBefore = (dir == 0);
        const int count = isBefore ? settings.frames_before : settings.frames_after;
        const int step = isBefore ? -1 : 1;

        for (int i = 0; i < count; ++i) {
            const int offset = (i + 1);
            const int frame = currentFrame + offset * step;

            if (frame < 0 || frame >= totalFrames || frame == currentFrame) {
                continue;
            }

            const uint8_t* src = frameProvider(frame);
            if (!src) {
                continue;
            }

            const float distFactor = 1.0f - static_cast<float>(offset) / static_cast<float>(maxDist + 1);
            const float alpha = std::clamp(baseOpacity * distFactor, 0.0f, 1.0f);
            if (alpha <= 0.0f) {
                continue;
            }

            const Color& tint = isBefore ? settings.tint_before : settings.tint_after;

            for (size_t px = 0; px < pixelCount; ++px) {
                const size_t i4 = px * 4;
                const float srcR = (src[i4 + 0] / 255.0f) * tint.r;
                const float srcG = (src[i4 + 1] / 255.0f) * tint.g;
                const float srcB = (src[i4 + 2] / 255.0f) * tint.b;
                const float srcA = (src[i4 + 3] / 255.0f) * alpha;

                const float dstR = output[i4 + 0] / 255.0f;
                const float dstG = output[i4 + 1] / 255.0f;
                const float dstB = output[i4 + 2] / 255.0f;
                const float dstA = output[i4 + 3] / 255.0f;

                const float outA = srcA + dstA * (1.0f - srcA);
                if (outA > 0.0f) {
                    const float invOutA = 1.0f / outA;
                    const float outR = (srcR * srcA + dstR * dstA * (1.0f - srcA)) * invOutA;
                    const float outG = (srcG * srcA + dstG * dstA * (1.0f - srcA)) * invOutA;
                    const float outB = (srcB * srcA + dstB * dstA * (1.0f - srcA)) * invOutA;
                    output[i4 + 0] = static_cast<uint8_t>(std::clamp(outR * 255.0f, 0.0f, 255.0f));
                    output[i4 + 1] = static_cast<uint8_t>(std::clamp(outG * 255.0f, 0.0f, 255.0f));
                    output[i4 + 2] = static_cast<uint8_t>(std::clamp(outB * 255.0f, 0.0f, 255.0f));
                    output[i4 + 3] = static_cast<uint8_t>(std::clamp(outA * 255.0f, 0.0f, 255.0f));
                } else {
                    output[i4 + 0] = 0;
                    output[i4 + 1] = 0;
                    output[i4 + 2] = 0;
                    output[i4 + 3] = 0;
                }
            }
        }
    }
}

} // namespace fap
