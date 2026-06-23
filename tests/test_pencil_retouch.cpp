#include <gtest/gtest.h>
#include "engine/brush/pencil_retouch.hpp"
#include "core/layer.hpp"

using namespace fap;

TEST(PencilRetouchEngineTest, DefaultConstruction) {
    PencilRetouchEngine engine;
    EXPECT_EQ(engine.mode(), RetouchMode::AdjustThickness);
    EXPECT_FLOAT_EQ(engine.strength(), 0.5f);
}

TEST(PencilRetouchEngineTest, SetMode) {
    PencilRetouchEngine engine;
    engine.setMode(RetouchMode::AdjustOpacity);
    EXPECT_EQ(engine.mode(), RetouchMode::AdjustOpacity);
    engine.setMode(RetouchMode::SmoothLines);
    EXPECT_EQ(engine.mode(), RetouchMode::SmoothLines);
    engine.setMode(RetouchMode::RepaintColor);
    EXPECT_EQ(engine.mode(), RetouchMode::RepaintColor);
}

TEST(PencilRetouchEngineTest, SetStrength) {
    PencilRetouchEngine engine;
    engine.setStrength(0.8f);
    EXPECT_FLOAT_EQ(engine.strength(), 0.8f);
    engine.setStrength(2.0f);
    EXPECT_FLOAT_EQ(engine.strength(), 1.0f);
    engine.setStrength(-1.0f);
    EXPECT_FLOAT_EQ(engine.strength(), 0.0f);
}

TEST(PencilRetouchEngineTest, SetBrush) {
    PencilRetouchEngine engine;
    RetouchBrush brush;
    brush.size = 25.0f;
    brush.opacity = 0.7f;
    brush.hardness = 0.3f;
    brush.color = { 1.0f, 0.0f, 0.0f, 1.0f };

    engine.setBrush(brush);
    EXPECT_FLOAT_EQ(engine.brush().size, 25.0f);
    EXPECT_FLOAT_EQ(engine.brush().opacity, 0.7f);
    EXPECT_FLOAT_EQ(engine.brush().color.r, 1.0f);
}

TEST(PencilRetouchEngineTest, RetouchOnEmptyLayer) {
    PencilRetouchEngine engine;
    RasterLayer layer("test", 100, 100);
    EXPECT_NO_THROW(engine.retouchAt(layer, { 50, 50 }, 1.0f));
}

TEST(PencilRetouchEngineTest, RetouchAdjustThickness) {
    PencilRetouchEngine engine;
    engine.setMode(RetouchMode::AdjustThickness);
    engine.setStrength(1.0f);

    RasterLayer layer("test", 100, 100);
    layer.setPixel(50, 50, { 0.0f, 0.0f, 0.0f, 0.5f });

    engine.retouchAt(layer, { 50, 50 }, 1.0f);

    Color c = layer.pixelAt(50, 50);
    EXPECT_GE(c.a, 0.4f);
}

TEST(PencilRetouchEngineTest, RetouchAdjustOpacity) {
    PencilRetouchEngine engine;
    engine.setMode(RetouchMode::AdjustOpacity);
    engine.setStrength(1.0f);

    RasterLayer layer("test", 100, 100);
    layer.setPixel(50, 50, { 0.0f, 0.0f, 0.0f, 0.5f });

    engine.retouchAt(layer, { 50, 50 }, 1.0f);

    Color c = layer.pixelAt(50, 50);
    EXPECT_GE(c.a, 0.0f);
}

TEST(PencilRetouchEngineTest, RetouchRepaintColor) {
    PencilRetouchEngine engine;
    engine.setMode(RetouchMode::RepaintColor);
    engine.setStrength(1.0f);

    RetouchBrush brush;
    brush.size = 20.0f;
    brush.color = { 1.0f, 0.0f, 0.0f, 1.0f };
    engine.setBrush(brush);

    RasterLayer layer("test", 100, 100);
    layer.setPixel(50, 50, { 0.0f, 0.0f, 0.0f, 0.5f });

    engine.retouchAt(layer, { 50, 50 }, 1.0f);

    Color c = layer.pixelAt(50, 50);
    EXPECT_GT(c.r, 0.0f);
}

TEST(PencilRetouchEngineTest, SmoothLines) {
    PencilRetouchEngine engine;
    engine.setMode(RetouchMode::SmoothLines);
    engine.setStrength(0.5f);

    RasterLayer layer("test", 100, 100);
    layer.setPixel(50, 50, { 0.0f, 0.0f, 0.0f, 1.0f });
    layer.setPixel(51, 50, { 1.0f, 1.0f, 1.0f, 0.0f });

    EXPECT_NO_THROW(engine.retouchAt(layer, { 50, 50 }, 1.0f));
}

TEST(RetouchBrushTest, DefaultValues) {
    RetouchBrush brush;
    EXPECT_FLOAT_EQ(brush.size, 10.0f);
    EXPECT_FLOAT_EQ(brush.opacity, 1.0f);
    EXPECT_FLOAT_EQ(brush.hardness, 0.5f);
    EXPECT_TRUE(brush.affect_opacity);
    EXPECT_TRUE(brush.affect_thickness);
}
