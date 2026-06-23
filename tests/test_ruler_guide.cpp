#include <gtest/gtest.h>
#include "engine/brush/ruler_guide.hpp"

using namespace fap;

TEST(RulerToolTest, DefaultConstruction) {
    RulerTool ruler;
    EXPECT_TRUE(ruler.snapEnabled());
    EXPECT_FLOAT_EQ(ruler.snapThreshold(), 8.0f);
    EXPECT_EQ(ruler.guideCount(), 0);
}

TEST(RulerToolTest, AddRemoveGuides) {
    RulerTool ruler;
    ruler.addGuide(std::make_unique<LinearGuide>());
    ruler.addGuide(std::make_unique<RadialGuide>());

    EXPECT_EQ(ruler.guideCount(), 2);
    EXPECT_NE(ruler.guideAt(0), nullptr);
    EXPECT_EQ(ruler.guideAt(10), nullptr);

    ruler.removeGuide(0);
    EXPECT_EQ(ruler.guideCount(), 1);
}

TEST(RulerToolTest, SnapDisabled) {
    RulerTool ruler;
    ruler.setSnapEnabled(false);
    ruler.addGuide(std::make_unique<LinearGuide>());

    Vec2 pos = { 50, 50 };
    Vec2 snapped = ruler.snap(pos);
    EXPECT_FLOAT_EQ(snapped.x, pos.x);
    EXPECT_FLOAT_EQ(snapped.y, pos.y);
}

TEST(LinearGuideTest, SnapToLine) {
    RulerTool ruler;
    auto guide = std::make_unique<LinearGuide>();
    guide->setPoints({
        {{0, 0}, true},
        {{100, 0}, true}
    });
    ruler.addGuide(std::move(guide));
    ruler.setSnapThreshold(10.0f);

    Vec2 snapped = ruler.snap({ 50, 5 });
    EXPECT_NEAR(snapped.x, 50.0f, 1.0f);
    EXPECT_NEAR(snapped.y, 0.0f, 1.0f);
}

TEST(LinearGuideTest, NoSnapFarAway) {
    RulerTool ruler;
    auto guide = std::make_unique<LinearGuide>();
    guide->setPoints({
        {{0, 0}, true},
        {{100, 0}, true}
    });
    ruler.addGuide(std::move(guide));
    ruler.setSnapThreshold(5.0f);

    Vec2 pos = { 50, 100 };
    Vec2 snapped = ruler.snap(pos);
    EXPECT_FLOAT_EQ(snapped.x, pos.x);
    EXPECT_FLOAT_EQ(snapped.y, pos.y);
}

TEST(RadialGuideTest, SetCenter) {
    RadialGuide guide;
    guide.setCenter({ 100, 100 });

    auto pts = guide.points();
    ASSERT_FALSE(pts.empty());
    EXPECT_FLOAT_EQ(pts[0].position.x, 100.0f);
    EXPECT_FLOAT_EQ(pts[0].position.y, 100.0f);
}

TEST(MirrorGuideTest, MirrorPoint) {
    MirrorGuide guide;
    guide.setAxisPoint({ 0, 0 }, { 100, 0 });

    Vec2 mirrored = guide.mirror({ 50, 30 });
    EXPECT_NEAR(mirrored.x, 50.0f, 1.0f);
    EXPECT_NEAR(mirrored.y, -30.0f, 1.0f);
}

TEST(MirrorGuideTest, MirrorOnAxis) {
    MirrorGuide guide;
    guide.setAxisPoint({ 0, 0 }, { 100, 0 });

    Vec2 mirrored = guide.mirror({ 50, 0 });
    EXPECT_NEAR(mirrored.x, 50.0f, 0.1f);
    EXPECT_NEAR(mirrored.y, 0.0f, 0.1f);
}

TEST(PerspectiveGuideTest, VanishingPoints) {
    PerspectiveGuide guide;

    guide.setVanishingPoint(0, { 100, 200 });
    EXPECT_FLOAT_EQ(guide.vanishingPoint(0).x, 100.0f);
    EXPECT_FLOAT_EQ(guide.vanishingPoint(0).y, 200.0f);
}

TEST(PerspectiveGuideTest, InvalidIndex) {
    PerspectiveGuide guide;

    guide.setVanishingPoint(5, { 100, 200 });
    EXPECT_FLOAT_EQ(guide.vanishingPoint(5).x, 0.0f);
}

TEST(GuideTest, EnableDisable) {
    LinearGuide guide;
    EXPECT_TRUE(guide.enabled());
    guide.setEnabled(false);
    EXPECT_FALSE(guide.enabled());
}

TEST(RulerToolTest, SnapThreshold) {
    RulerTool ruler;
    ruler.setSnapThreshold(15.0f);
    EXPECT_FLOAT_EQ(ruler.snapThreshold(), 15.0f);
}

TEST(RulerToolTest, DisabledGuideNotSnapped) {
    RulerTool ruler;
    auto guide = std::make_unique<LinearGuide>();
    guide->setPoints({
        {{0, 0}, true},
        {{100, 0}, true}
    });
    guide->setEnabled(false);
    ruler.addGuide(std::move(guide));
    ruler.setSnapThreshold(20.0f);

    Vec2 snapped = ruler.snap({ 50, 5 });
    EXPECT_FLOAT_EQ(snapped.x, 50.0f);
    EXPECT_FLOAT_EQ(snapped.y, 5.0f);
}
