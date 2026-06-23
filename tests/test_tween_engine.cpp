#include <gtest/gtest.h>
#include "engine/animation/tween_engine.hpp"

using namespace fap;

TEST(EasingTest, Linear) {
    EXPECT_FLOAT_EQ(applyEasing(0.0f, EasingType::Linear), 0.0f);
    EXPECT_FLOAT_EQ(applyEasing(0.5f, EasingType::Linear), 0.5f);
    EXPECT_FLOAT_EQ(applyEasing(1.0f, EasingType::Linear), 1.0f);
}

TEST(EasingTest, EaseIn) {
    float t = applyEasing(0.5f, EasingType::EaseIn);
    EXPECT_LT(t, 0.5f);
    EXPECT_FLOAT_EQ(applyEasing(0.0f, EasingType::EaseIn), 0.0f);
    EXPECT_FLOAT_EQ(applyEasing(1.0f, EasingType::EaseIn), 1.0f);
}

TEST(EasingTest, EaseOut) {
    float t = applyEasing(0.5f, EasingType::EaseOut);
    EXPECT_GT(t, 0.5f);
}

TEST(EasingTest, EaseInOut) {
    EXPECT_FLOAT_EQ(applyEasing(0.0f, EasingType::EaseInOut), 0.0f);
    EXPECT_FLOAT_EQ(applyEasing(0.5f, EasingType::EaseInOut), 0.5f);
    EXPECT_FLOAT_EQ(applyEasing(1.0f, EasingType::EaseInOut), 1.0f);
}

TEST(EasingTest, ClampOutOfRange) {
    EXPECT_FLOAT_EQ(applyEasing(-1.0f, EasingType::Linear), 0.0f);
    EXPECT_FLOAT_EQ(applyEasing(2.0f, EasingType::Linear), 1.0f);
}

TEST(TweenEngineTest, EmptyKeyframes) {
    TweenEngine engine;
    auto result = engine.evaluateAtFrame(5);
    EXPECT_EQ(result.frame, 0);
}

TEST(TweenEngineTest, SingleKeyframe) {
    TweenEngine engine;
    TweenKeyframe kf;
    kf.frame = 0;
    kf.position = { 10, 20 };
    engine.setKeyframes({ kf });

    auto result = engine.evaluateAtFrame(5);
    EXPECT_FLOAT_EQ(result.position.x, 10.0f);
    EXPECT_FLOAT_EQ(result.position.y, 20.0f);
}

TEST(TweenEngineTest, InterpolatePosition) {
    TweenEngine engine;
    TweenKeyframe kf1, kf2;
    kf1.frame = 0;
    kf1.position = { 0, 0 };
    kf1.easing = EasingType::Linear;
    kf2.frame = 10;
    kf2.position = { 100, 100 };
    kf2.easing = EasingType::Linear;

    engine.setKeyframes({ kf1, kf2 });

    auto result = engine.evaluateAtFrame(5);
    EXPECT_NEAR(result.position.x, 50.0f, 1.0f);
    EXPECT_NEAR(result.position.y, 50.0f, 1.0f);
}

TEST(TweenEngineTest, InterpolateOpacity) {
    TweenEngine engine;
    TweenKeyframe kf1, kf2;
    kf1.frame = 0;
    kf1.opacity = 1.0f;
    kf1.easing = EasingType::Linear;
    kf2.frame = 10;
    kf2.opacity = 0.0f;
    kf2.easing = EasingType::Linear;

    engine.setKeyframes({ kf1, kf2 });

    auto result = engine.evaluateAtFrame(5);
    EXPECT_NEAR(result.opacity, 0.5f, 0.1f);
}

TEST(TweenEngineTest, BeforeFirstKeyframe) {
    TweenEngine engine;
    TweenKeyframe kf;
    kf.frame = 10;
    kf.position = { 100, 100 };
    engine.addKeyframe(kf);

    auto result = engine.evaluateAtFrame(5);
    EXPECT_FLOAT_EQ(result.position.x, 100.0f);
}

TEST(TweenEngineTest, AfterLastKeyframe) {
    TweenEngine engine;
    TweenKeyframe kf;
    kf.frame = 10;
    kf.position = { 100, 100 };
    engine.addKeyframe(kf);

    auto result = engine.evaluateAtFrame(20);
    EXPECT_FLOAT_EQ(result.position.x, 100.0f);
}

TEST(TweenEngineTest, AddUpdateRemoveKeyframe) {
    TweenEngine engine;
    TweenKeyframe kf1{0, {0, 0}, {1, 1}, 0, 1, {0, 0, 0, 1}, 3, EasingType::Linear};
    TweenKeyframe kf2{10, {100, 100}, {2, 2}, 45, 0.5f, {1, 0, 0, 1}, 5, EasingType::EaseIn};

    engine.addKeyframe(kf1);
    engine.addKeyframe(kf2);
    EXPECT_EQ(engine.keyframes().size(), 2);

    TweenKeyframe updated{10, {200, 200}, {3, 3}, 90, 0.3f, {0, 1, 0, 1}, 8, EasingType::EaseOut};
    engine.updateKeyframe(1, updated);
    EXPECT_FLOAT_EQ(engine.keyframes()[1].position.x, 200.0f);

    engine.removeKeyframe(0);
    EXPECT_EQ(engine.keyframes().size(), 1);
}

TEST(TweenEngineTest, EnabledProperties) {
    TweenEngine engine;
    engine.setEnabledProperties({ TweenableProperty::Opacity });

    TweenKeyframe kf1{0, {0, 0}, {1, 1}, 0, 1.0f, {0, 0, 0, 1}, 3, EasingType::Linear};
    TweenKeyframe kf2{10, {100, 100}, {2, 2}, 90, 0.0f, {1, 0, 0, 1}, 5, EasingType::Linear};
    engine.setKeyframes({ kf1, kf2 });

    auto result = engine.evaluateAtFrame(5);
    EXPECT_NEAR(result.opacity, 0.5f, 0.1f);
}

TEST(BreakdownPoseEngineTest, BasicBreakdown) {
    BreakdownPoseEngine engine;
    TweenKeyframe start{0, {0, 0}, {1, 1}, 0, 1.0f, {0, 0, 0, 1}, 3, EasingType::Linear};
    TweenKeyframe end{10, {100, 0}, {1, 1}, 0, 1.0f, {0, 0, 0, 1}, 3, EasingType::Linear};

    engine.setStartPose(start);
    engine.setEndPose(end);
    engine.setStartFrame(0);
    engine.setEndFrame(10);

    auto result = engine.computeBreakdown(5);
    EXPECT_NEAR(result.position.x, 50.0f, 1.0f);
}

TEST(BreakdownPoseEngineTest, Bias) {
    BreakdownPoseEngine engine;
    TweenKeyframe start{0, {0, 0}, {1, 1}, 0, 1.0f, {0, 0, 0, 1}, 3, EasingType::Linear};
    TweenKeyframe end{10, {100, 0}, {1, 1}, 0, 1.0f, {0, 0, 0, 1}, 3, EasingType::Linear};

    engine.setStartPose(start);
    engine.setEndPose(end);
    engine.setStartFrame(0);
    engine.setEndFrame(10);
    engine.setBias(0.0f);

    auto result = engine.computeBreakdown(5);
    EXPECT_NEAR(result.position.x, 50.0f, 1.0f);
}

TEST(TweenKeyframeTest, LerpStatic) {
    TweenKeyframe a{0, {0, 0}, {1, 1}, 0, 1.0f, {0, 0, 0, 1}, 3, EasingType::Linear};
    TweenKeyframe b{0, {100, 100}, {2, 2}, 90, 0.0f, {1, 1, 1, 1}, 10, EasingType::Linear};

    std::vector<TweenableProperty> props = {
        TweenableProperty::Position,
        TweenableProperty::Scale,
        TweenableProperty::Opacity,
        TweenableProperty::StrokeWidth
    };

    auto result = a.lerp(b, 0.5f, props);
    EXPECT_NEAR(result.position.x, 50.0f, 1.0f);
    EXPECT_NEAR(result.scale.x, 1.5f, 0.1f);
    EXPECT_NEAR(result.opacity, 0.5f, 0.1f);
    EXPECT_NEAR(result.strokeWidth, 6.5f, 0.1f);
}
