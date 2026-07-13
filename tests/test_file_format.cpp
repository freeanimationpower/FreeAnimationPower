#include <gtest/gtest.h>
#include "core/document.hpp"
#include "core/layer.hpp"
#include "io/document_manager.hpp"
#include <QTemporaryDir>
#include <QDir>
#include <QTemporaryFile>
#include <QFileInfo>

namespace fap {
    bool saveFAP(const QString& path, const Document& doc);
    bool loadFAP(const QString& path, Document& doc);
}

TEST(FileFormatTest, SaveAndLoadBasic) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    QString projectDir = dir.path() + "/test_project.fap";

    fap::Document doc;
    doc.setCanvasSize(640, 480);
    doc.setFPS(30);
    doc.setTotalFrames(5);

    bool saved = fap::saveFAP(projectDir, doc);
    EXPECT_TRUE(saved);
    EXPECT_TRUE(QDir(projectDir).exists());
    EXPECT_TRUE(QFile::exists(projectDir + "/document.json"));

    fap::Document loaded;
    bool loaded_ok = fap::loadFAP(projectDir, loaded);
    EXPECT_TRUE(loaded_ok);
    EXPECT_EQ(loaded.width(), 640);
    EXPECT_EQ(loaded.height(), 480);
    EXPECT_EQ(loaded.fps(), 30);
    EXPECT_EQ(loaded.totalFrames(), 5);
}

TEST(FileFormatTest, SaveAndLoadPreservesLayerNames) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    QString projectDir = dir.path() + "/test_names.fap";

    fap::Document doc;
    doc.setCanvasSize(320, 240);
    doc.setTotalFrames(2);

    // Frame 0: Rename default layer and add a second named layer
    auto& root0 = doc.activeSequence().rootLayerForFrame(0);
    root0.layerAt(0)->setName("Background");
    auto layer2 = std::make_unique<fap::RasterLayer>("Foreground", 320, 240);
    root0.addLayer(std::move(layer2));

    // Frame 1: Different layer names
    auto& root1 = doc.activeSequence().rootLayerForFrame(1);
    root1.layerAt(0)->setName("Sketch");

    EXPECT_EQ(root0.layerAt(0)->name(), "Background");
    EXPECT_EQ(root0.layerAt(1)->name(), "Foreground");
    EXPECT_EQ(root1.layerAt(0)->name(), "Sketch");

    ASSERT_TRUE(fap::saveFAP(projectDir, doc));

    fap::Document loaded;
    ASSERT_TRUE(fap::loadFAP(projectDir, loaded));

    auto& lroot0 = loaded.activeSequence().rootLayerForFrame(0);
    ASSERT_GE(lroot0.layerCount(), 2u);
    EXPECT_EQ(lroot0.layerAt(0)->name(), "Background");
    EXPECT_EQ(lroot0.layerAt(1)->name(), "Foreground");

    auto& lroot1 = loaded.activeSequence().rootLayerForFrame(1);
    ASSERT_GE(lroot1.layerCount(), 1u);
    EXPECT_EQ(lroot1.layerAt(0)->name(), "Sketch");
}

TEST(FileFormatTest, BinaryZipSavesLayerNames) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    QString fapPath = dir.path() + "/test_binary_names.fap";

    fap::Document doc;
    doc.setCanvasSize(320, 240);
    doc.setTotalFrames(1);

    auto& root = doc.activeSequence().rootLayerForFrame(0);
    root.layerAt(0)->setName("MiCapa");

    EXPECT_EQ(root.layerAt(0)->name(), "MiCapa");

    fap::DocumentManager dm;
    fap::ViewState vs;
    ASSERT_TRUE(dm.save(doc, fapPath, vs));

    fap::Document loaded;
    ASSERT_TRUE(dm.load(loaded, fapPath));

    auto& lroot = loaded.activeSequence().rootLayerForFrame(0);
    ASSERT_GE(lroot.layerCount(), 1u);
    EXPECT_EQ(lroot.layerAt(0)->name(), "MiCapa");
}
