#include <gtest/gtest.h>
#include "engine/brush/brush_engine.hpp"
using namespace fap;

TEST(BrushEngineTest, DefaultPreset) {
    BrushEngine engine;
    EXPECT_EQ(engine.preset().name, "Round Soft");
    EXPECT_EQ(engine.preset().mode, BrushMode::Raster);
}

TEST(BrushEngineTest, BeginAddEndStroke) {
    BrushEngine engine;
    Color c = {0, 0, 0, 1};
    engine.beginStroke(c, 10.0f);
    StrokePoint pt;
    pt.position = {100, 100}; pt.pressure = 0.5f;
    engine.addPoint(pt);
    engine.addPoint({{200, 200}, 1.0f});
    EXPECT_EQ(engine.currentStroke().size(), 2);
    engine.endStroke();
}

TEST(BrushEngineTest, PresetsManagement) {
    BrushEngine engine;
    EXPECT_GT(engine.presets().size(), 0);
    BrushPreset custom;
    custom.name = "Custom";
    engine.setPreset(custom);
    EXPECT_EQ(engine.preset().name, "Custom");
}
