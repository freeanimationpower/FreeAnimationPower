#include <gtest/gtest.h>
#include "engine/deformation/deformation_mesh.hpp"
#include "engine/deformation/deformation_engine.hpp"
#include "core/layer.hpp"

using namespace fap;

TEST(DeformationMeshTest, DefaultConstruction) {
    DeformationMesh mesh(4, 4);
    EXPECT_EQ(mesh.cols(), 4);
    EXPECT_EQ(mesh.rows(), 4);
    EXPECT_EQ(mesh.pointCount(), 16);
}

TEST(DeformationMeshTest, MinimumDimensions) {
    DeformationMesh mesh(1, 1);
    EXPECT_GE(mesh.cols(), 2);
    EXPECT_GE(mesh.rows(), 2);
}

TEST(DeformationMeshTest, BoundsSet) {
    DeformationMesh mesh(3, 3);
    mesh.setBounds({ 0, 0, 100, 100 });

    auto orig = mesh.originalPosition(0, 0);
    EXPECT_NEAR(orig.x, 0.0f, 0.1f);
    EXPECT_NEAR(orig.y, 0.0f, 0.1f);

    auto corner = mesh.originalPosition(2, 2);
    EXPECT_NEAR(corner.x, 100.0f, 0.1f);
    EXPECT_NEAR(corner.y, 100.0f, 0.1f);
}

TEST(DeformationMeshTest, PointOffset) {
    DeformationMesh mesh(3, 3);
    mesh.setBounds({ 0, 0, 100, 100 });

    mesh.setPointOffset(1, 1, { 10.0f, 5.0f });
    auto deformed = mesh.deformedPosition(1, 1);
    auto orig = mesh.originalPosition(1, 1);
    EXPECT_NEAR(deformed.x, orig.x + 10.0f, 0.1f);
    EXPECT_NEAR(deformed.y, orig.y + 5.0f, 0.1f);
}

TEST(DeformationMeshTest, LockedPointUnchanged) {
    DeformationMesh mesh(3, 3);
    mesh.setBounds({ 0, 0, 100, 100 });
    mesh.lockPoint(1, 1, true);
    mesh.setPointOffset(1, 1, { 10.0f, 5.0f });

    auto deformed = mesh.deformedPosition(1, 1);
    auto orig = mesh.originalPosition(1, 1);
    EXPECT_NEAR(deformed.x, orig.x, 0.1f);
    EXPECT_NEAR(deformed.y, orig.y, 0.1f);
}

TEST(DeformationMeshTest, MapPointIdentity) {
    DeformationMesh mesh(4, 4);
    mesh.setBounds({ 0, 0, 200, 200 });

    Vec2 result = mesh.mapPoint({ 50, 50 });
    EXPECT_NEAR(result.x, 50.0f, 1.0f);
    EXPECT_NEAR(result.y, 50.0f, 1.0f);
}

TEST(DeformationMeshTest, MapPointWithOffset) {
    DeformationMesh mesh(4, 4);
    mesh.setBounds({ 0, 0, 200, 200 });
    mesh.setPointOffset(3, 3, { 20.0f, 20.0f });

    Vec2 result = mesh.mapPoint({ 180, 180 });
    EXPECT_GT(result.x, 180.0f);
    EXPECT_GT(result.y, 180.0f);
}

TEST(DeformationMeshTest, ResetClearsOffsets) {
    DeformationMesh mesh(3, 3);
    mesh.setBounds({ 0, 0, 100, 100 });
    mesh.setPointOffset(1, 1, { 10.0f, 10.0f });
    mesh.reset();

    auto deformed = mesh.deformedPosition(1, 1);
    auto orig = mesh.originalPosition(1, 1);
    EXPECT_NEAR(deformed.x, orig.x, 0.1f);
    EXPECT_NEAR(deformed.y, orig.y, 0.1f);
}

TEST(DeformationEngineTest, CreateDefaultMesh) {
    DeformationEngine engine;
    engine.createDefaultMesh({ 0, 0, 100, 100 }, 5, 5);
    ASSERT_NE(engine.mesh(), nullptr);
    EXPECT_EQ(engine.mesh()->cols(), 5);
    EXPECT_EQ(engine.mesh()->rows(), 5);
}

TEST(DeformationEngineTest, DeformRasterPreservesType) {
    DeformationEngine engine;
    engine.createDefaultMesh({ -10, -10, 60, 60 }, 3, 3);

    RasterLayer layer("test", 50, 50);
    layer.setPixel(10, 10, { 1.0f, 0.0f, 0.0f, 1.0f });

    engine.deformRasterLayer(layer);
    EXPECT_EQ(layer.width(), 50);
    EXPECT_EQ(layer.height(), 50);
}
