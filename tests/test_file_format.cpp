#include <gtest/gtest.h>
#include "core/document.hpp"
#include <QTemporaryDir>
#include <QDir>

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
