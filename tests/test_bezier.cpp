#include <gtest/gtest.h>
#include "engine/vector/bezier_path.hpp"
using namespace fap;

TEST(BezierTest, EmptyPath) {
    BezierPath path;
    EXPECT_TRUE(path.empty());
    EXPECT_EQ(path.totalLength(), 0.0f);
}

TEST(BezierTest, LinePath) {
    BezierPath path;
    path.moveTo(0, 0);
    path.lineTo(100, 0);
    EXPECT_FALSE(path.empty());
    EXPECT_GT(path.totalLength(), 99.0f);
}

TEST(BezierTest, EvaluateAt) {
    BezierPath path;
    path.moveTo(0, 0);
    path.lineTo(100, 0);
    Vec2 mid = path.evaluateAt(0.5f);
    EXPECT_NEAR(mid.x, 50.0f, 1.0f);
    EXPECT_NEAR(mid.y, 0.0f, 0.1f);
}

TEST(BezierTest, FlattenPoints) {
    BezierPath path;
    path.moveTo(0, 0);
    path.lineTo(100, 0);
    path.lineTo(100, 100);
    auto pts = path.flattenPoints(1.0f);
    EXPECT_GT(pts.size(), 1);
    EXPECT_NEAR(pts[0].x, 0.0f, 0.1f);
    EXPECT_NEAR(pts.back().x, 100.0f, 0.1f);
    EXPECT_NEAR(pts.back().y, 100.0f, 0.1f);
}

TEST(BezierTest, CurveTo) {
    BezierPath path;
    path.moveTo(0, 0);
    path.curveTo(33, 0, 66, 100, 100, 100);
    Vec2 mid = path.evaluateAt(0.5f);
    EXPECT_GT(mid.y, 20.0f);
}
