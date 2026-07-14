#include <gtest/gtest.h>
#include "engine/compositor/compositor.hpp"
#include "engine/raster/raster_engine.hpp"
#include "core/layer.hpp"

using namespace fap;

TEST(CompositorTest, SingleRasterLayer) {
    GroupLayer root("Root");
    root.addLayer(std::make_unique<RasterLayer>("bg", 4, 4));
    root.addLayer(std::make_unique<RasterLayer>("fg", 4, 4));
    auto& fg = static_cast<RasterLayer&>(*root.layerAt(1));
    fg.setPixel(1, 1, Color::fromRGBA(255, 0, 0, 255));
    fg.setPixel(2, 2, Color::fromRGBA(0, 255, 0, 255));

    RasterEngine target(4, 4);
    Compositor comp;
    comp.composite(root, target);

    Color c11 = Color::fromRGBA(
        static_cast<uint8_t>((target.pixels()[1 * 4 + 1] >> 16) & 0xFF),
        static_cast<uint8_t>((target.pixels()[1 * 4 + 1] >> 8) & 0xFF),
        static_cast<uint8_t>(target.pixels()[1 * 4 + 1] & 0xFF),
        static_cast<uint8_t>((target.pixels()[1 * 4 + 1] >> 24) & 0xFF)
    );
    EXPECT_EQ(c11, Color::fromRGBA(255, 0, 0, 255));

    Color c22 = Color::fromRGBA(
        static_cast<uint8_t>((target.pixels()[2 * 4 + 2] >> 16) & 0xFF),
        static_cast<uint8_t>((target.pixels()[2 * 4 + 2] >> 8) & 0xFF),
        static_cast<uint8_t>(target.pixels()[2 * 4 + 2] & 0xFF),
        static_cast<uint8_t>((target.pixels()[2 * 4 + 2] >> 24) & 0xFF)
    );
    EXPECT_EQ(c22, Color::fromRGBA(0, 255, 0, 255));
}

TEST(CompositorTest, LayerOpacity) {
    GroupLayer root("Root");
    root.addLayer(std::make_unique<RasterLayer>("fg", 2, 2));
    auto& fg = static_cast<RasterLayer&>(*root.layerAt(0));
    fg.setPixel(0, 0, Color::fromRGBA(255, 255, 255, 255));
    fg.setOpacity(0.5f);

    RasterEngine target(2, 2);
    target.fill(Color::fromRGBA(0, 0, 0, 0));
    Compositor comp;
    comp.composite(root, target);

    uint32_t p = target.pixels()[0];
    uint8_t a = static_cast<uint8_t>((p >> 24) & 0xFF);
    EXPECT_GT(a, 0u);
    EXPECT_LT(a, 255u);
}

TEST(CompositorTest, InvisibleLayerSkipped) {
    GroupLayer root("Root");
    root.addLayer(std::make_unique<RasterLayer>("fg", 2, 2));
    auto& fg = static_cast<RasterLayer&>(*root.layerAt(0));
    fg.setPixel(0, 0, Color::fromRGBA(255, 0, 0, 255));
    fg.setVisible(false);

    RasterEngine target(2, 2);
    Compositor comp;
    comp.composite(root, target);

    EXPECT_EQ(target.pixels()[0], 0u);
    EXPECT_EQ(target.pixels()[1], 0u);
}

TEST(CompositorTest, GroupLayerRecursive) {
    auto inner = std::make_unique<RasterLayer>("inner", 2, 2);
    inner->setPixel(0, 0, Color::fromRGBA(0, 0, 255, 255));

    auto mid = std::make_unique<GroupLayer>("mid");
    mid->addLayer(std::move(inner));

    GroupLayer root("Root");
    root.addLayer(std::move(mid));

    RasterEngine target(2, 2);
    Compositor comp;
    comp.composite(root, target);

    uint32_t p = target.pixels()[0];
    Color got = Color::fromRGBA(
        static_cast<uint8_t>((p >> 16) & 0xFF),
        static_cast<uint8_t>((p >> 8) & 0xFF),
        static_cast<uint8_t>(p & 0xFF),
        static_cast<uint8_t>((p >> 24) & 0xFF)
    );
    EXPECT_EQ(got, Color::fromRGBA(0, 0, 255, 255));
}

TEST(CompositorTest, MoveLayerChangesOrder) {
    GroupLayer root("Root");
    auto bottom = std::make_unique<RasterLayer>("bottom", 2, 2);
    bottom->setPixel(0, 0, Color::fromRGBA(255, 0, 0, 255));
    auto top = std::make_unique<RasterLayer>("top", 2, 2);
    top->setPixel(0, 0, Color::fromRGBA(0, 0, 255, 255));
    root.addLayer(std::move(bottom));
    root.addLayer(std::move(top));

    RasterEngine target(2, 2);
    Compositor comp;

    comp.composite(root, target);
    uint32_t p1 = target.pixels()[0];
    EXPECT_EQ(static_cast<uint8_t>((p1 >> 16) & 0xFF), 0u);
    EXPECT_GT(static_cast<uint8_t>(p1 & 0xFF), 0u);

    root.moveLayer(0, 1);
    target.clear();
    comp.composite(root, target);
    uint32_t p2 = target.pixels()[0];
    EXPECT_GT(static_cast<uint8_t>((p2 >> 16) & 0xFF), 0u);
    EXPECT_EQ(static_cast<uint8_t>(p2 & 0xFF), 0u);
}

TEST(CompositorTest, VisibilityToggleReflected) {
    GroupLayer root("Root");
    root.addLayer(std::make_unique<RasterLayer>("fg", 2, 2));
    auto& fg = static_cast<RasterLayer&>(*root.layerAt(0));
    fg.setPixel(0, 0, Color::fromRGBA(255, 0, 0, 255));

    RasterEngine target(2, 2);
    Compositor comp;
    comp.composite(root, target);
    EXPECT_NE(target.pixels()[0], 0u);

    fg.setVisible(false);
    target.clear();
    comp.composite(root, target);
    EXPECT_EQ(target.pixels()[0], 0u);

    fg.setVisible(true);
    target.clear();
    comp.composite(root, target);
    EXPECT_NE(target.pixels()[0], 0u);
}
