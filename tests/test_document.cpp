#include <gtest/gtest.h>
#include "core/document.hpp"
#include "core/timeline.hpp"
#include "core/layer.hpp"

using namespace fap;

TEST(DocumentTest, DefaultValues) {
    Document doc;
    EXPECT_EQ(doc.width(), 1920);
    EXPECT_EQ(doc.height(), 1080);
    EXPECT_EQ(doc.fps(), 24);
    EXPECT_FALSE(doc.modified());
}

TEST(DocumentTest, SetCanvasSize) {
    Document doc;
    doc.setCanvasSize(1280, 720);
    EXPECT_EQ(doc.width(), 1280);
    EXPECT_EQ(doc.height(), 720);
    EXPECT_TRUE(doc.modified());
}

TEST(TimelineTest, CurrentFrame) {
    Timeline timeline;
    EXPECT_EQ(timeline.currentFrame(), 0);
    timeline.setTotalFrames(24);
    timeline.setCurrentFrame(10);
    EXPECT_EQ(timeline.currentFrame(), 10);
}

TEST(TimelineTest, FrameClamping) {
    Timeline timeline;
    timeline.setTotalFrames(10);
    timeline.setCurrentFrame(100);
    EXPECT_EQ(timeline.currentFrame(), 9);
    timeline.setCurrentFrame(-5);
    EXPECT_EQ(timeline.currentFrame(), 0);
}

TEST(TimelineTest, Keyframes) {
    Timeline timeline;
    timeline.setTotalFrames(24);
    timeline.addKeyframe(0, 5);
    EXPECT_TRUE(timeline.hasKeyframe(0, 5));
    EXPECT_FALSE(timeline.hasKeyframe(0, 6));
    timeline.removeKeyframe(0, 5);
    EXPECT_FALSE(timeline.hasKeyframe(0, 5));
}

TEST(LayerTest, GroupLayer) {
    GroupLayer group("TestGroup");
    group.addLayer(std::make_unique<RasterLayer>("Layer1", 100, 100));
    group.addLayer(std::make_unique<VectorLayer>("Layer2"));
    EXPECT_EQ(group.layerCount(), 2);
    EXPECT_EQ(group.layerAt(0)->name(), "Layer1");
    group.removeLayer(0);
    EXPECT_EQ(group.layerCount(), 1);
}

TEST(LayerTest, LayerProperties) {
    RasterLayer layer("Test", 800, 600);
    EXPECT_EQ(layer.name(), "Test");
    EXPECT_EQ(layer.type(), LayerType::Raster);
    EXPECT_TRUE(layer.visible());
    layer.setVisible(false);
    EXPECT_FALSE(layer.visible());
    layer.setOpacity(0.5f);
    EXPECT_FLOAT_EQ(layer.opacity(), 0.5f);
}
