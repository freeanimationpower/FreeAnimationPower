#include "tween_engine.hpp"
#include <algorithm>
#include <cmath>

namespace fap {

float applyEasing(float t, EasingType type)
{
    t = std::clamp(t, 0.0f, 1.0f);

    switch (type) {
    case EasingType::Linear:
        return t;
    case EasingType::EaseIn:
        return t * t;
    case EasingType::EaseOut:
        return 1.0f - (1.0f - t) * (1.0f - t);
    case EasingType::EaseInOut:
        return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
    case EasingType::Elastic: {
        if (t == 0.0f || t == 1.0f) return t;
        const float c4 = (2.0f * 3.14159265358979323846f) / 3.0f;
        return -std::pow(2.0f, 10.0f * t - 10.0f) * std::sin((t * 10.0f - 10.75f) * c4);
    }
    case EasingType::Bounce: {
        const float n1 = 7.5625f;
        const float d1 = 2.75f;
        if (t < 1.0f / d1) {
            return n1 * t * t;
        } else if (t < 2.0f / d1) {
            t -= 1.5f / d1;
            return n1 * t * t + 0.75f;
        } else if (t < 2.5f / d1) {
            t -= 2.25f / d1;
            return n1 * t * t + 0.9375f;
        } else {
            t -= 2.625f / d1;
            return n1 * t * t + 0.984375f;
        }
    }
    case EasingType::Back: {
        const float c1 = 1.70158f;
        const float c3 = c1 + 1.0f;
        return 1.0f + c3 * std::pow(t - 1.0f, 3.0f) + c1 * std::pow(t - 1.0f, 2.0f);
    }
    case EasingType::Cubic:
        return t < 0.5f
            ? 4.0f * t * t * t
            : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
    default:
        return t;
    }
}

TweenKeyframe TweenKeyframe::lerp(const TweenKeyframe& other, float t,
                                   const std::vector<TweenableProperty>& props) const
{
    t = std::clamp(t, 0.0f, 1.0f);
    float et = applyEasing(t, easing);
    TweenKeyframe result = *this;

    for (auto prop : props) {
        switch (prop) {
        case TweenableProperty::Position:
            result.position = Vec2::lerp(position, other.position, et);
            break;
        case TweenableProperty::Scale:
            result.scale = Vec2::lerp(scale, other.scale, et);
            break;
        case TweenableProperty::Rotation:
            result.rotation = lerpf(rotation, other.rotation, et);
            break;
        case TweenableProperty::Opacity:
            result.opacity = lerpf(opacity, other.opacity, et);
            break;
        case TweenableProperty::Color: {
            auto c = color.premultiplied();
            auto oc = other.color.premultiplied();
            result.color.r = lerpf(c.r, oc.r, et);
            result.color.g = lerpf(c.g, oc.g, et);
            result.color.b = lerpf(c.b, oc.b, et);
            result.color.a = lerpf(c.a, oc.a, et);
            break;
        }
        case TweenableProperty::StrokeWidth:
            result.strokeWidth = lerpf(strokeWidth, other.strokeWidth, et);
            break;
        }
    }

    return result;
}

TweenEngine::TweenEngine()
{
    enabled_props_ = {
        TweenableProperty::Position,
        TweenableProperty::Scale,
        TweenableProperty::Rotation,
        TweenableProperty::Opacity
    };
}

void TweenEngine::setKeyframes(const std::vector<TweenKeyframe>& kfs)
{
    keyframes_ = kfs;
    std::sort(keyframes_.begin(), keyframes_.end(),
        [](const TweenKeyframe& a, const TweenKeyframe& b) { return a.frame < b.frame; });
}

void TweenEngine::addKeyframe(const TweenKeyframe& kf)
{
    auto it = std::lower_bound(keyframes_.begin(), keyframes_.end(), kf.frame,
        [](const TweenKeyframe& a, int frame) { return a.frame < frame; });
    if (it != keyframes_.end() && it->frame == kf.frame) {
        *it = kf;
    } else {
        keyframes_.insert(it, kf);
    }
}

void TweenEngine::removeKeyframe(int index)
{
    if (index >= 0 && static_cast<size_t>(index) < keyframes_.size()) {
        keyframes_.erase(keyframes_.begin() + index);
    }
}

void TweenEngine::updateKeyframe(int index, const TweenKeyframe& kf)
{
    if (index >= 0 && static_cast<size_t>(index) < keyframes_.size()) {
        keyframes_[index] = kf;
    }
}

void TweenEngine::setEnabledProperties(const std::vector<TweenableProperty>& props)
{
    enabled_props_ = props;
}

TweenKeyframe TweenEngine::evaluateAtFrame(int frame) const
{
    if (keyframes_.empty()) return {};
    if (keyframes_.size() == 1) return keyframes_[0];
    if (frame <= keyframes_.front().frame) return keyframes_.front();
    if (frame >= keyframes_.back().frame) return keyframes_.back();

    for (size_t i = 0; i < keyframes_.size() - 1; ++i) {
        const auto& a = keyframes_[i];
        const auto& b = keyframes_[i + 1];
        if (frame >= a.frame && frame <= b.frame) {
            float t = (b.frame != a.frame)
                ? static_cast<float>(frame - a.frame) / (b.frame - a.frame)
                : 0.0f;
            return a.lerp(b, t, enabled_props_);
        }
    }

    return keyframes_.front();
}

BezierPath TweenEngine::interpolatePaths(const BezierPath& from, const BezierPath& to,
                                          int fromFrame, int toFrame, int currentFrame) const
{
    if (currentFrame <= fromFrame) return from;
    if (currentFrame >= toFrame) return to;

    float t = (toFrame != fromFrame)
        ? static_cast<float>(currentFrame - fromFrame) / (toFrame - fromFrame)
        : 0.0f;
    t = applyEasing(t, EasingType::EaseInOut);

    BezierPath result;
    size_t maxSegs = std::max(from.segments().size(), to.segments().size());
    if (maxSegs == 0) return result;
    result.segments().resize(maxSegs);

    const CubicBezier defaultSeg{};

    for (size_t i = 0; i < maxSegs; ++i) {
        const auto& sf = (i < from.segments().size()) ? from.segments()[i]
            : (from.segments().empty() ? defaultSeg : from.segments().back());
        const auto& st = (i < to.segments().size()) ? to.segments()[i]
            : (to.segments().empty() ? defaultSeg : to.segments().back());

        result.segments()[i].p0 = Vec2::lerp(sf.p0, st.p0, t);
        result.segments()[i].p1 = Vec2::lerp(sf.p1, st.p1, t);
        result.segments()[i].p2 = Vec2::lerp(sf.p2, st.p2, t);
        result.segments()[i].p3 = Vec2::lerp(sf.p3, st.p3, t);
    }

    return result;
}

BreakdownPoseEngine::BreakdownPoseEngine() = default;

TweenKeyframe BreakdownPoseEngine::computeBreakdown(int currentFrame, float bias) const
{
    float t = (end_frame_ != start_frame_)
        ? static_cast<float>(currentFrame - start_frame_) / (end_frame_ - start_frame_)
        : 0.0f;
    t = std::clamp(t, 0.0f, 1.0f);

    float adjustedT = applyEasing(t, easing_);
    adjustedT = lerpf(t, adjustedT, bias_);

    std::vector<TweenableProperty> allProps = {
        TweenableProperty::Position,
        TweenableProperty::Scale,
        TweenableProperty::Rotation,
        TweenableProperty::Opacity,
        TweenableProperty::Color,
        TweenableProperty::StrokeWidth
    };

    return start_pose_.lerp(end_pose_, adjustedT, allProps);
}

} // namespace fap
