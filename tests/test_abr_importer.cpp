#include <gtest/gtest.h>
#include "engine/brush/abr_importer.hpp"

using namespace fap;

TEST(AbrImporterTest, IsAbrFileFalse) {
    EXPECT_FALSE(AbrImporter::isAbrFile("nonexistent.abr"));
    EXPECT_FALSE(AbrImporter::isAbrFile(""));
}

TEST(AbrImporterTest, LoadInvalidFile) {
    AbrImporter importer;
    EXPECT_FALSE(importer.loadFile("nonexistent.abr"));
    EXPECT_FALSE(importer.loadFile(""));
}

TEST(AbrImporterTest, EmptyBrushes) {
    AbrImporter importer;
    EXPECT_EQ(importer.brushCount(), 0);
}

TEST(AbrImporterTest, ConvertToBrushTipInvalidIndex) {
    AbrImporter importer;
    BrushTip tip = importer.convertToBrushTip(-1);
    EXPECT_FLOAT_EQ(tip.spacing, 0.15f);
    EXPECT_FLOAT_EQ(tip.hardness, 0.8f);

    tip = importer.convertToBrushTip(100);
    EXPECT_TRUE(tip.name.empty());
}

TEST(AbrImporterTest, CreatePresetInvalid) {
    AbrImporter importer;
    BrushPreset preset = importer.createPreset(-1);
    EXPECT_FLOAT_EQ(preset.size, 10.0f);

    preset = importer.createPreset(0, "MyBrush");
    EXPECT_TRUE(preset.name.empty());
}

TEST(AbrImporterTest, MaxValues) {
    EXPECT_EQ(AbrImporter::kMaxBrushes, 512);
    EXPECT_EQ(AbrImporter::kMaxBrushSize, 2500);
}
