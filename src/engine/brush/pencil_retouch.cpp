#include "pencil_retouch.hpp"
#include <algorithm>
#include <cmath>

namespace fap {

PencilRetouchEngine::PencilRetouchEngine() = default;

void PencilRetouchEngine::retouchAt(RasterLayer& layer,
                                     const Vec2& pos,
                                     float pressure) const
{
    int w = layer.width();
    int h = layer.height();
    int ox = layer.originX();
    int oy = layer.originY();

    int cx = static_cast<int>(pos.x) - ox;
    int cy = static_cast<int>(pos.y) - oy;

    float brushSize = brush_.size * 0.5f;
    int minX = std::max(0, static_cast<int>(cx - brushSize - 1));
    int maxX = std::min(w - 1, static_cast<int>(cx + brushSize + 1));
    int minY = std::max(0, static_cast<int>(cy - brushSize - 1));
    int maxY = std::min(h - 1, static_cast<int>(cy + brushSize + 1));

    layer.ensureUnique();
    auto* pixels = layer.pixelData();

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            float alpha = calcBrushAlpha(static_cast<float>(x - cx),
                                          static_cast<float>(y - cy), brushSize);
            if (alpha <= 0.0f) continue;

            float intensity = alpha * strength_ * pressure;
            size_t idx = static_cast<size_t>(y) * w + x;
            uint32_t& px = pixels[idx];

            switch (mode_) {
            case RetouchMode::AdjustThickness:
                adjustPixelThickness(px, intensity);
                break;
            case RetouchMode::AdjustOpacity:
                adjustPixelOpacity(px, intensity);
                break;
            case RetouchMode::SmoothLines:
                smoothPixel(layer, x, y);
                break;
            case RetouchMode::RepaintColor:
                repaintPixel(px, intensity);
                break;
            }
        }
    }

    layer.bufferEpochTick();
}

float PencilRetouchEngine::calcBrushAlpha(float dx, float dy, float brushSize) const
{
    float dist = std::sqrt(dx * dx + dy * dy);
    if (dist > brushSize) return 0.0f;

    float core = brushSize * brush_.hardness;
    if (dist <= core) return 1.0f;

    float t = (dist - core) / (brushSize - core + 1e-6f);
    return 1.0f - smoothstepf(0.0f, 1.0f, t);
}

void PencilRetouchEngine::adjustPixelThickness(uint32_t& pixel, float intensity) const
{
    int a = (pixel >> 24) & 0xFF;
    if (a == 0) return;

    float alpha = a / 255.0f;
    float factor = 1.0f + (intensity * 2.0f - intensity) * 0.5f;
    float newAlpha = std::clamp(alpha * factor, 0.0f, 1.0f);
    int newA = static_cast<int>(newAlpha * 255.0f);
    float scale = newAlpha / alpha;
    int newR = static_cast<int>(std::clamp(((pixel >> 16) & 0xFF) * scale, 0.0f, 255.0f));
    int newG = static_cast<int>(std::clamp(((pixel >> 8) & 0xFF) * scale, 0.0f, 255.0f));
    int newB = static_cast<int>(std::clamp((pixel & 0xFF) * scale, 0.0f, 255.0f));
    pixel = (newA << 24) | (newR << 16) | (newG << 8) | newB;
}

void PencilRetouchEngine::adjustPixelOpacity(uint32_t& pixel, float intensity) const
{
    int a = (pixel >> 24) & 0xFF;
    if (a == 0) return;

    float alpha = a / 255.0f;
    float factor = 1.0f + (intensity - 0.5f) * 0.3f;
    float newAlpha = std::clamp(alpha * factor, 0.0f, 1.0f);
    int newA = static_cast<int>(newAlpha * 255.0f);
    float scale = newAlpha / alpha;
    int newR = static_cast<int>(std::clamp(((pixel >> 16) & 0xFF) * scale, 0.0f, 255.0f));
    int newG = static_cast<int>(std::clamp(((pixel >> 8) & 0xFF) * scale, 0.0f, 255.0f));
    int newB = static_cast<int>(std::clamp((pixel & 0xFF) * scale, 0.0f, 255.0f));
    pixel = (newA << 24) | (newR << 16) | (newG << 8) | newB;
}

void PencilRetouchEngine::smoothPixel(RasterLayer& layer, int x, int y) const
{
    int w = layer.width();
    int h = layer.height();
    auto* pixels = layer.pixelData();

    size_t idx = static_cast<size_t>(y) * w + x;
    uint32_t orig = pixels[idx];
    if (((orig >> 24) & 0xFF) == 0) return;

    int sumR = 0, sumG = 0, sumB = 0, sumA = 0, count = 0;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int nx = x + dx, ny = y + dy;
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
            size_t ni = static_cast<size_t>(ny) * w + nx;
            uint32_t np = pixels[ni];
            if (((np >> 24) & 0xFF) == 0) continue;
            sumR += (np >> 16) & 0xFF;
            sumG += (np >> 8) & 0xFF;
            sumB += np & 0xFF;
            sumA += (np >> 24) & 0xFF;
            ++count;
        }
    }

    if (count > 1) {
        float s = strength_;
        int r = static_cast<int>(lerpf((orig >> 16) & 0xFF, sumR / static_cast<float>(count), s));
        int g = static_cast<int>(lerpf((orig >> 8) & 0xFF, sumG / static_cast<float>(count), s));
        int b = static_cast<int>(lerpf(orig & 0xFF, sumB / static_cast<float>(count), s));
        int a = static_cast<int>(lerpf((orig >> 24) & 0xFF, sumA / static_cast<float>(count), s));
        pixels[idx] = (std::clamp(a, 0, 255) << 24)
                    | (std::clamp(r, 0, 255) << 16)
                    | (std::clamp(g, 0, 255) << 8)
                    |  std::clamp(b, 0, 255);
    }
}

void PencilRetouchEngine::repaintPixel(uint32_t& pixel, float intensity) const
{
    int a = (pixel >> 24) & 0xFF;
    if (a == 0) return;

    auto pc = brush_.color.premultiplied();
    int r = static_cast<int>(lerpf((pixel >> 16) & 0xFF, pc.r * 255.0f, intensity));
    int g = static_cast<int>(lerpf((pixel >> 8) & 0xFF, pc.g * 255.0f, intensity));
    int b = static_cast<int>(lerpf(pixel & 0xFF, pc.b * 255.0f, intensity));

    pixel = (std::clamp(a, 0, 255) << 24)
          | (std::clamp(r, 0, 255) << 16)
          | (std::clamp(g, 0, 255) << 8)
          |  std::clamp(b, 0, 255);
}

} // namespace fap
