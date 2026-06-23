#include <gtest/gtest.h>
#include "core/layer.hpp"
#include "engine/vector/bezier_path.hpp"

using namespace fap;

TEST(RasterLayerTest, BufferCreation) {
    RasterLayer layer("Test", 64, 48);
    EXPECT_EQ(layer.width(), 64);
    EXPECT_EQ(layer.height(), 48);
    EXPECT_EQ(layer.pixelCount(), 64u * 48u);
    ASSERT_NE(layer.pixelData(), nullptr);
    const uint32_t* data = layer.pixelData();
    for (size_t i = 0; i < layer.pixelCount(); ++i) {
        EXPECT_EQ(data[i], 0u) << "Pixel " << i << " not transparent black";
    }
}

TEST(RasterLayerTest, SetPixelAndReadBack) {
    RasterLayer layer("Test", 4, 4);
    Color red = Color::fromRGBA(255, 0, 0, 255);
    layer.setPixel(1, 2, red);
    Color got = layer.pixelAt(1, 2);
    EXPECT_EQ(got.r, red.r);
    EXPECT_EQ(got.g, red.g);
    EXPECT_EQ(got.b, red.b);
    EXPECT_EQ(got.a, red.a);
}

TEST(RasterLayerTest, PixelOutOfBoundsClamped) {
    RasterLayer layer("Test", 4, 4);
    Color c = Color::fromRGBA(128, 64, 32, 255);
    layer.setPixel(-1, 5, c);
    layer.setPixel(4, 0, c);
    layer.setPixel(0, 4, c);
    for (size_t i = 0; i < layer.pixelCount(); ++i) {
        EXPECT_EQ(layer.pixelData()[i], 0u);
    }
}

TEST(RasterLayerTest, ClearFillsBuffer) {
    RasterLayer layer("Test", 4, 4);
    layer.setPixel(0, 0, Color::fromRGBA(255, 0, 0, 255));
    layer.setPixel(3, 3, Color::fromRGBA(0, 255, 0, 255));
    layer.clear();
    for (size_t i = 0; i < layer.pixelCount(); ++i) {
        EXPECT_EQ(layer.pixelData()[i], 0u);
    }
}

TEST(RasterLayerTest, ClearWithColor) {
    RasterLayer layer("Test", 2, 2);
    Color bg = Color::fromRGBA(64, 128, 192, 255);
    layer.clear(bg);
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            Color got = layer.pixelAt(x, y);
            EXPECT_EQ(got, bg);
        }
    }
    layer.clear();
    for (size_t i = 0; i < layer.pixelCount(); ++i) {
        EXPECT_EQ(layer.pixelData()[i], 0u);
    }
}

TEST(RasterLayerTest, ResizePreservesIntersection) {
    RasterLayer layer("Test", 4, 4);
    Color c = Color::fromRGBA(255, 255, 0, 255);
    layer.setPixel(0, 0, c);
    layer.setPixel(3, 2, c);
    layer.resize(2, 2);
    EXPECT_EQ(layer.width(), 2);
    EXPECT_EQ(layer.height(), 2);
    EXPECT_EQ(layer.pixelCount(), 4u);
    EXPECT_EQ(layer.pixelAt(0, 0), c);
    Color transparent = Color::fromRGBA(0, 0, 0, 0);
    EXPECT_EQ(layer.pixelAt(1, 1), transparent);
}

TEST(RasterLayerTest, ResizeGrowFillsNewArea) {
    RasterLayer layer("Test", 2, 2);
    Color red = Color::fromRGBA(255, 0, 0, 255);
    layer.setPixel(0, 0, red);
    layer.resize(4, 4);
    Color transparent = Color::fromRGBA(0, 0, 0, 0);
    EXPECT_EQ(layer.pixelAt(0, 0), red);
    EXPECT_EQ(layer.pixelAt(3, 3), transparent);
    EXPECT_EQ(layer.pixelAt(2, 0), transparent);
}

TEST(VectorLayerTest, AddStrokeAndAccess) {
    VectorLayer layer("Vectors");
    EXPECT_EQ(layer.strokeCount(), 0u);

    VectorStroke s1;
    s1.color = Color::fromRGBA(255, 0, 0, 255);
    s1.width = 5.0f;
    layer.addStroke(s1);

    EXPECT_EQ(layer.strokeCount(), 1u);
    EXPECT_EQ(layer.strokeAt(0).color, s1.color);
    EXPECT_FLOAT_EQ(layer.strokeAt(0).width, 5.0f);
}

TEST(VectorLayerTest, RemoveStroke) {
    VectorLayer layer("Vectors");
    VectorStroke s1, s2;
    s1.width = 1.0f;
    s2.width = 2.0f;
    layer.addStroke(s1);
    layer.addStroke(s2);
    EXPECT_EQ(layer.strokeCount(), 2u);

    layer.removeStroke(0);
    EXPECT_EQ(layer.strokeCount(), 1u);
    EXPECT_FLOAT_EQ(layer.strokeAt(0).width, 2.0f);

    layer.removeStroke(-1);
    EXPECT_EQ(layer.strokeCount(), 1u);
    layer.removeStroke(100);
    EXPECT_EQ(layer.strokeCount(), 1u);
}

TEST(VectorLayerTest, ClearStrokes) {
    VectorLayer layer("Vectors");
    layer.addStroke(VectorStroke{});
    layer.addStroke(VectorStroke{});
    layer.addStroke(VectorStroke{});
    EXPECT_EQ(layer.strokeCount(), 3u);

    layer.clearStrokes();
    EXPECT_EQ(layer.strokeCount(), 0u);
}

TEST(VectorLayerTest, StrokesConstAccess) {
    VectorLayer layer("Vectors");
    layer.addStroke(VectorStroke{});
    const VectorLayer& clayer = layer;
    EXPECT_EQ(clayer.strokes().size(), 1u);
    EXPECT_EQ(clayer.strokeCount(), 1u);
}
