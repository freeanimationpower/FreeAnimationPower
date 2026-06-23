#include <gtest/gtest.h>
#include "core/timeline.hpp"
using namespace fap;

TEST(TimelineTest, InitialStatePlayback) {
    Timeline t;
    EXPECT_EQ(t.currentFrame(), 0);
    EXPECT_EQ(t.totalFrames(), 1);
    EXPECT_EQ(t.fps(), 24);
    EXPECT_FALSE(t.playing());
}

TEST(TimelineTest, SetCurrentFrameClamp) {
    Timeline t;
    t.setTotalFrames(10);
    t.setCurrentFrame(5);
    EXPECT_EQ(t.currentFrame(), 5);
}

TEST(TimelineTest, NextPrevFrameNav) {
    Timeline t;
    t.setTotalFrames(5);
    t.nextFrame(); EXPECT_EQ(t.currentFrame(), 1);
    t.nextFrame(); EXPECT_EQ(t.currentFrame(), 2);
    t.prevFrame(); EXPECT_EQ(t.currentFrame(), 1);
    t.goToStart(); EXPECT_EQ(t.currentFrame(), 0);
    t.goToEnd(); EXPECT_EQ(t.currentFrame(), 4);
}

TEST(TimelineTest, KeyframesAddRemove) {
    Timeline t;
    t.setTotalFrames(10);
    t.addKeyframe(0, 3);
    EXPECT_TRUE(t.hasKeyframe(0, 3));
    EXPECT_FALSE(t.hasKeyframe(0, 5));
    t.removeKeyframe(0, 3);
    EXPECT_FALSE(t.hasKeyframe(0, 3));
}

TEST(TimelineTest, LoopingToggle) {
    Timeline t;
    t.setTotalFrames(3);
    t.setLooping(true);
    EXPECT_TRUE(t.looping());
}
