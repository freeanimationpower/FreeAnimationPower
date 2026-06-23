#pragma once

#include "core/types.hpp"
#include "engine/vector/bezier_path.hpp"
#include <vector>
#include <functional>

namespace fap {

enum class EasingType {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
    Elastic,
    Bounce,
    Back,
    Cubic
};

float applyEasing(float t, EasingType type);

enum class TweenableProperty {
    Position,
    Scale,
    Rotation,
    Opacity,
    Color,
    StrokeWidth
};

struct TweenKeyframe {
    int frame = 0;
    Vec2 position{0, 0};
    Vec2 scale{1, 1};
    float rotation = 0.0f;
    float opacity = 1.0f;
    Color color{0, 0, 0, 1};
    float strokeWidth = 3.0f;
    EasingType easing = EasingType::Linear;

    TweenKeyframe lerp(const TweenKeyframe& other, float t,
                       const std::vector<TweenableProperty>& props) const;
};

class TweenEngine {
public:
    TweenEngine();

    void setKeyframes(const std::vector<TweenKeyframe>& kfs);
    const std::vector<TweenKeyframe>& keyframes() const { return keyframes_; }

    void addKeyframe(const TweenKeyframe& kf);
    void removeKeyframe(int index);
    void updateKeyframe(int index, const TweenKeyframe& kf);

    void setEnabledProperties(const std::vector<TweenableProperty>& props);
    const std::vector<TweenableProperty>& enabledProperties() const { return enabled_props_; }

    TweenKeyframe evaluateAtFrame(int frame) const;

    BezierPath interpolatePaths(const BezierPath& from, const BezierPath& to,
                                int fromFrame, int toFrame, int currentFrame) const;

private:
    std::vector<TweenKeyframe> keyframes_;
    std::vector<TweenableProperty> enabled_props_;
};

class BreakdownPoseEngine {
public:
    BreakdownPoseEngine();

    void setStartPose(const TweenKeyframe& pose) { start_pose_ = pose; }
    void setEndPose(const TweenKeyframe& pose) { end_pose_ = pose; }
    void setStartFrame(int frame) { start_frame_ = frame; }
    void setEndFrame(int frame) { end_frame_ = frame; }

    TweenKeyframe computeBreakdown(int currentFrame, float bias = 0.5f) const;

    float bias() const { return bias_; }
    void setBias(float b) { bias_ = std::clamp(b, 0.0f, 1.0f); }

    EasingType easing() const { return easing_; }
    void setEasing(EasingType type) { easing_ = type; }

private:
    TweenKeyframe start_pose_;
    TweenKeyframe end_pose_;
    int start_frame_ = 0;
    int end_frame_ = 1;
    float bias_ = 0.5f;
    EasingType easing_ = EasingType::Linear;
};

} // namespace fap
