#include "engine/raster/raster_engine.hpp"
#include "engine/brush/brush_engine.hpp"
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace fap {
namespace {

float smoothstep(float edge0, float edge1, float x) {
    float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float distance(const Vec2& a, const Vec2& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

Vec2 lerp(const Vec2& a, const Vec2& b, float t) {
    return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t };
}

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

inline uint32_t packRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return (static_cast<uint32_t>(r)) |
           (static_cast<uint32_t>(g) << 8) |
           (static_cast<uint32_t>(b) << 16) |
           (static_cast<uint32_t>(a) << 24);
}

inline void unpackRGBA(uint32_t p, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a) {
    r = static_cast<uint8_t>(p & 0xFF);
    g = static_cast<uint8_t>((p >> 8) & 0xFF);
    b = static_cast<uint8_t>((p >> 16) & 0xFF);
    a = static_cast<uint8_t>((p >> 24) & 0xFF);
}

void alphaBlendPixel(uint32_t* dst, float cr, float cg, float cb, float ca) {
    if (ca <= 0.0f) return;
    uint8_t dr, dg, db, da;
    unpackRGBA(*dst, dr, dg, db, da);

    float da_f = da / 255.0f;
    float dr_f = dr / 255.0f;
    float dg_f = dg / 255.0f;
    float db_f = db / 255.0f;

    float inv_ca = 1.0f - ca;
    float out_a = ca + da_f * inv_ca;
    if (out_a <= 0.0f) { *dst = 0; return; }

    float out_r = (cr * ca + dr_f * da_f * inv_ca) / out_a;
    float out_g = (cg * ca + dg_f * da_f * inv_ca) / out_a;
    float out_b = (cb * ca + db_f * da_f * inv_ca) / out_a;

    *dst = packRGBA(
        static_cast<uint8_t>(std::clamp(out_r * 255.0f, 0.0f, 255.0f)),
        static_cast<uint8_t>(std::clamp(out_g * 255.0f, 0.0f, 255.0f)),
        static_cast<uint8_t>(std::clamp(out_b * 255.0f, 0.0f, 255.0f)),
        static_cast<uint8_t>(std::clamp(out_a * 255.0f, 0.0f, 255.0f))
    );
}

void stampBrush(RasterEngine& target, int cx, int cy, float radius,
                float hardness, float r, float g, float b, float opacity) {
    if (radius < 0.5f || opacity <= 0.0f) return;

    int ir = static_cast<int>(std::ceil(radius));
    int x0 = std::max(0, cx - ir);
    int y0 = std::max(0, cy - ir);
    int x1 = std::min(target.width() - 1, cx + ir);
    int y1 = std::min(target.height() - 1, cy + ir);

    uint32_t* data = target.pixels();
    int tw = target.width();

    for (int py = y0; py <= y1; ++py) {
        for (int px = x0; px <= x1; ++px) {
            float dx = static_cast<float>(px - cx);
            float dy = static_cast<float>(py - cy);
            float dist = std::sqrt(dx * dx + dy * dy) / radius;
            if (dist >= 1.0f) continue;

            float alpha = 1.0f - smoothstep(hardness, 1.0f, dist);
            alpha *= opacity;
            if (alpha <= 0.0f) continue;

            alphaBlendPixel(&data[static_cast<size_t>(py) * tw + px], r, g, b, alpha);
        }
    }
}

} // anonymous namespace

void renderRasterStroke(RasterEngine& target,
                        const std::vector<StrokePoint>& points,
                        const BrushPreset& preset)
{
    if (points.empty()) return;

    if (points.size() == 1) {
        const auto& p = points[0];
        float size = preset.size;
        if (preset.dynamics.pressure_size) {
            size *= lerp(preset.dynamics.min_size, preset.dynamics.max_size, p.pressure);
        }
        float opacity = preset.opacity;
        if (preset.dynamics.pressure_opacity) {
            opacity *= lerp(preset.dynamics.min_opacity, preset.dynamics.max_opacity, p.pressure);
        }
        stampBrush(target,
                   static_cast<int>(p.position.x),
                   static_cast<int>(p.position.y),
                   size * 0.5f,
                   preset.tip.hardness,
                   preset.color.r, preset.color.g, preset.color.b,
                   opacity);
        return;
    }

    for (size_t i = 0; i + 1 < points.size(); ++i) {
        const auto& p0 = points[i];
        const auto& p1 = points[i + 1];

        float seg_dist = distance(p0.position, p1.position);
        float spacing = preset.tip.spacing * preset.size;
        spacing = std::max(spacing, 0.5f);

        if (seg_dist < spacing) {
            float size = preset.size;
            float opacity = preset.opacity;
            if (preset.dynamics.pressure_size) {
                size *= lerp(preset.dynamics.min_size, preset.dynamics.max_size, p0.pressure);
            }
            if (preset.dynamics.pressure_opacity) {
                opacity *= lerp(preset.dynamics.min_opacity, preset.dynamics.max_opacity, p0.pressure);
            }
            stampBrush(target,
                       static_cast<int>(p0.position.x),
                       static_cast<int>(p0.position.y),
                       size * 0.5f,
                       preset.tip.hardness,
                       preset.color.r, preset.color.g, preset.color.b,
                       opacity);
            continue;
        }

        int num_stamps = static_cast<int>(std::floor(seg_dist / spacing));
        for (int j = 0; j < num_stamps; ++j) {
            float d = (static_cast<float>(j) + 0.5f) * spacing;
            float t = d / seg_dist;

            Vec2 pos = lerp(p0.position, p1.position, t);
            float pressure = lerp(p0.pressure, p1.pressure, t);

            float size = preset.size;
            if (preset.dynamics.pressure_size) {
                size *= lerp(preset.dynamics.min_size, preset.dynamics.max_size, pressure);
            }

            float opacity = preset.opacity;
            if (preset.dynamics.pressure_opacity) {
                opacity *= lerp(preset.dynamics.min_opacity, preset.dynamics.max_opacity, pressure);
            }

            stampBrush(target,
                       static_cast<int>(pos.x),
                       static_cast<int>(pos.y),
                       size * 0.5f,
                       preset.tip.hardness,
                       preset.color.r, preset.color.g, preset.color.b,
                       opacity);
        }
    }
}

} // namespace fap
