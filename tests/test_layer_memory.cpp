#include <gtest/gtest.h>
#include "core/layer.hpp"

using namespace fap;

TEST(LayerMemoryTest, UidIsImmutableAndMonotonic) {
    RasterLayer layer1("Layer 1", 100, 100);
    RasterLayer layer2("Layer 2", 100, 100);

    LayerUid uid1 = layer1.uid();
    LayerUid uid2 = layer2.uid();

    EXPECT_NE(uid1, uid2);
    EXPECT_GT(uid2, uid1);

    EXPECT_EQ(layer1.uid(), uid1);
    layer1.setName("Renamed");
    EXPECT_EQ(layer1.uid(), uid1);
}

TEST(LayerMemoryTest, BufferEpochIncrementsOnResize) {
    RasterLayer layer("Test", 4, 4);
    uint64_t e0 = layer.bufferEpoch();

    layer.resize(4, 4);
    uint64_t e1 = layer.bufferEpoch();
    EXPECT_NE(e1, e0);

    layer.resize(8, 8);
    EXPECT_GT(layer.bufferEpoch(), e1);
}

TEST(LayerMemoryTest, PixelBufferAddressStableWithoutResize) {
    RasterLayer layer("Test", 100, 100);
    const uint32_t* addr1 = layer.pixelData();

    layer.setPixel(0, 0, Color::fromRGBA(255, 0, 0, 255));
    EXPECT_EQ(layer.pixelData(), addr1);

    layer.clear();
    EXPECT_EQ(layer.pixelData(), addr1);
}

TEST(LayerMemoryTest, LayerByUidFindsCorrectLayer) {
    GroupLayer group("Root");
    auto l1 = std::make_unique<RasterLayer>("Layer 1", 64, 64);
    auto l2 = std::make_unique<RasterLayer>("Layer 2", 64, 64);
    LayerUid uid1 = l1->uid();
    LayerUid uid2 = l2->uid();

    group.addLayer(std::move(l1));
    group.addLayer(std::move(l2));

    Layer* found1 = group.layerByUid(uid1);
    Layer* found2 = group.layerByUid(uid2);
    ASSERT_NE(found1, nullptr);
    ASSERT_NE(found2, nullptr);
    EXPECT_EQ(found1->name(), "Layer 1");
    EXPECT_EQ(found2->name(), "Layer 2");
    EXPECT_EQ(group.layerByUid(999999), nullptr);
}

TEST(LayerMemoryTest, IndexOfLayerReturnsCorrectPosition) {
    GroupLayer group("Root");
    auto l1 = std::make_unique<RasterLayer>("Layer 1", 64, 64);
    auto l2 = std::make_unique<RasterLayer>("Layer 2", 64, 64);
    Layer* p1 = l1.get();
    Layer* p2 = l2.get();

    group.addLayer(std::move(l1));
    group.addLayer(std::move(l2));

    EXPECT_EQ(group.indexOfLayer(p1), 0);
    EXPECT_EQ(group.indexOfLayer(p2), 1);
}

TEST(LayerMemoryTest, PointerStabilityAcrossAddLayer) {
    GroupLayer group("Root");
    auto l1 = std::make_unique<RasterLayer>("Layer 1", 1920, 1080);
    const uint32_t* pixelAddr1 = l1->pixelData();
    LayerUid uid1 = l1->uid();
    group.addLayer(std::move(l1));

    RasterLayer* r1 = static_cast<RasterLayer*>(group.layerByUid(uid1));
    ASSERT_NE(r1, nullptr);
    EXPECT_EQ(r1->pixelData(), pixelAddr1);

    auto l2 = std::make_unique<RasterLayer>("Layer 2", 1920, 1080);
    const uint32_t* pixelAddr2 = l2->pixelData();
    LayerUid uid2 = l2->uid();
    group.addLayer(std::move(l2));

    RasterLayer* r1b = static_cast<RasterLayer*>(group.layerByUid(uid1));
    RasterLayer* r2 = static_cast<RasterLayer*>(group.layerByUid(uid2));
    ASSERT_NE(r1b, nullptr);
    ASSERT_NE(r2, nullptr);
    EXPECT_EQ(r1b->pixelData(), pixelAddr1);
    EXPECT_EQ(r2->pixelData(), pixelAddr2);
}

TEST(LayerMemoryTest, PointerStabilityAcrossInsertAndRemove) {
    GroupLayer group("Root");

    for (int i = 0; i < 32; ++i) {
        group.addLayer(std::make_unique<RasterLayer>(
            "Layer " + std::to_string(i), 100, 100));
    }

    LayerUid firstUid = group.layerAt(0)->uid();
    const uint32_t* firstData = static_cast<RasterLayer*>(group.layerAt(0))->pixelData();

    group.addLayer(std::make_unique<RasterLayer>("Middle", 100, 100));

    auto inserted = std::make_unique<RasterLayer>("Inserted", 100, 100);
    LayerUid insUid = inserted->uid();
    const uint32_t* insData = inserted->pixelData();
    group.addLayer(std::move(inserted));

    RasterLayer* rfirst = static_cast<RasterLayer*>(group.layerByUid(firstUid));
    RasterLayer* rins = static_cast<RasterLayer*>(group.layerByUid(insUid));
    ASSERT_NE(rfirst, nullptr);
    ASSERT_NE(rins, nullptr);
    EXPECT_EQ(rfirst->pixelData(), firstData);
    EXPECT_EQ(rins->pixelData(), insData);

    group.removeLayer(0);
    EXPECT_EQ(group.layerByUid(insUid)->name(), "Inserted");
}

TEST(LayerMemoryTest, CrashReproductionSequence_LockCreateSelectDraw) {
    GroupLayer group("Root");

    auto l1 = std::make_unique<RasterLayer>("Layer 1", 1920, 1080);
    LayerUid uid1 = l1->uid();
    const uint32_t* data1 = l1->pixelData();
    group.addLayer(std::move(l1));

    RasterLayer* layer1 = static_cast<RasterLayer*>(group.layerByUid(uid1));
    ASSERT_NE(layer1, nullptr);
    EXPECT_FALSE(layer1->locked());

    layer1->setPixel(100, 200, Color::fromRGBA(255, 128, 64, 255));
    EXPECT_EQ(layer1->pixelData(), data1);

    layer1->setLocked(true);
    EXPECT_TRUE(layer1->locked());

    auto l2 = std::make_unique<RasterLayer>("Layer 2", 1920, 1080);
    LayerUid uid2 = l2->uid();
    const uint32_t* data2 = l2->pixelData();
    group.addLayer(std::move(l2));

    RasterLayer* layer2 = static_cast<RasterLayer*>(group.layerByUid(uid2));
    ASSERT_NE(layer2, nullptr);
    EXPECT_FALSE(layer2->locked());
    EXPECT_EQ(layer2->name(), "Layer 2");

    RasterLayer* layer1b = static_cast<RasterLayer*>(group.layerByUid(uid1));
    ASSERT_NE(layer1b, nullptr);
    EXPECT_EQ(layer1b->pixelData(), data1);
    EXPECT_EQ(layer2->pixelData(), data2);

    EXPECT_TRUE(layer1b->locked());
    EXPECT_FALSE(layer2->locked());

    layer2->setPixel(500, 300, Color::fromRGBA(0, 255, 0, 255));
    EXPECT_EQ(layer2->pixelData(), data2);

    int idx2 = group.indexOfLayer(layer2);
    EXPECT_GE(idx2, 0);
    EXPECT_EQ(group.layerAt(idx2)->uid(), uid2);
}

TEST(LayerMemoryTest, DuplicateLayerPreservesBufferStability) {
    GroupLayer group("Root");
    auto l1 = std::make_unique<RasterLayer>("Original", 100, 100);
    LayerUid uid1 = l1->uid();
    const uint32_t* data1 = l1->pixelData();
    l1->setPixel(50, 50, Color::fromRGBA(255, 0, 0, 255));
    group.addLayer(std::move(l1));

    Layer* clone = group.duplicateLayer(0);
    ASSERT_NE(clone, nullptr);

    RasterLayer* rclone = static_cast<RasterLayer*>(clone);
    EXPECT_EQ(rclone->pixelAt(50, 50), Color::fromRGBA(255, 0, 0, 255));

    RasterLayer* rorig = static_cast<RasterLayer*>(group.layerByUid(uid1));
    ASSERT_NE(rorig, nullptr);
    EXPECT_EQ(rorig->pixelData(), data1);
    EXPECT_EQ(rorig->name(), "Original");
    EXPECT_EQ(group.layerCount(), 2u);
}

TEST(LayerMemoryTest, CowShallowCloneDoesNotDuplicatePixelBytes) {
    RasterLayer original("Original", 1920, 1080);
    original.setPixel(50, 50, Color::fromRGBA(255, 128, 64, 255));

    const uint32_t* origData = original.pixelData();
    size_t origPixels = original.pixelCount();
    EXPECT_FALSE(original.isBufferShared());

    auto cloned = original.clone();
    auto* rclone = static_cast<RasterLayer*>(cloned.get());
    ASSERT_NE(rclone, nullptr);

    EXPECT_EQ(rclone->pixelData(), origData);
    EXPECT_TRUE(original.isBufferShared());
    EXPECT_TRUE(rclone->isBufferShared());
    EXPECT_EQ(rclone->pixelCount(), origPixels);
    EXPECT_EQ(rclone->pixelAt(50, 50), Color::fromRGBA(255, 128, 64, 255));

    rclone->setPixel(10, 10, Color::fromRGBA(0, 255, 0, 255));

    EXPECT_NE(rclone->pixelData(), origData);
    EXPECT_FALSE(original.isBufferShared());
    EXPECT_FALSE(rclone->isBufferShared());
    EXPECT_EQ(original.pixelAt(10, 10), Color::fromRGBA(0, 0, 0, 0));
    EXPECT_EQ(rclone->pixelAt(10, 10), Color::fromRGBA(0, 255, 0, 255));
    EXPECT_EQ(original.pixelAt(50, 50), Color::fromRGBA(255, 128, 64, 255));
    EXPECT_EQ(rclone->pixelAt(50, 50), Color::fromRGBA(255, 128, 64, 255));
}
