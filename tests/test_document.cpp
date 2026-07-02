#include <gtest/gtest.h>
#include "core/document.hpp"
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

TEST(DocumentTest, SequenceDefaultCreated) {
    Document doc;
    EXPECT_EQ(doc.sequenceCount(), 1u);
    EXPECT_EQ(doc.activeSequenceIndex(), 0u);
    EXPECT_EQ(doc.activeSequence().name(), "Sequence 1");
    EXPECT_EQ(doc.activeSequence().fps(), 24);
    EXPECT_EQ(doc.activeSequence().totalFrames(), 24);
}

TEST(DocumentTest, AddAndManageSequences) {
    Document doc;
    doc.addSequence("Sequence 2");
    EXPECT_EQ(doc.sequenceCount(), 2u);
    EXPECT_EQ(doc.activeSequenceIndex(), 0u);
    EXPECT_EQ(doc.sequenceAt(1).name(), "Sequence 2");
    EXPECT_TRUE(doc.modified());
}

TEST(DocumentTest, SetActiveSequence) {
    Document doc;
    doc.addSequence("Second");
    doc.setActiveSequence(1);
    EXPECT_EQ(doc.activeSequenceIndex(), 1u);
    EXPECT_EQ(doc.activeSequence().name(), "Second");
}

TEST(DocumentTest, DuplicateSequence) {
    Document doc;
    doc.addSequence("Original");
    doc.setActiveSequence(1);
    doc.activeSequence().setFPS(30);
    doc.activeSequence().setTotalFrames(48);

    doc.duplicateSequence(1);
    EXPECT_EQ(doc.sequenceCount(), 3u);
    EXPECT_EQ(doc.sequenceAt(2).name(), "Original copy");
    EXPECT_EQ(doc.sequenceAt(2).fps(), 30);
    EXPECT_EQ(doc.sequenceAt(2).totalFrames(), 48);
}

TEST(DocumentTest, CannotRemoveLastSequence) {
    Document doc;
    doc.removeSequence(0);
    EXPECT_EQ(doc.sequenceCount(), 1u);
}

TEST(DocumentTest, RemoveSequence) {
    Document doc;
    doc.addSequence("A");
    doc.addSequence("B");
    EXPECT_EQ(doc.sequenceCount(), 3u);
    doc.removeSequence(1);
    EXPECT_EQ(doc.sequenceCount(), 2u);
    EXPECT_EQ(doc.sequenceAt(1).name(), "B");
}

TEST(DocumentTest, ActiveIndexClampedAfterRemove) {
    Document doc;
    doc.addSequence("Extra");
    doc.setActiveSequence(1);
    doc.removeSequence(1);
    EXPECT_EQ(doc.activeSequenceIndex(), 0u);
}

TEST(DocumentTest, DelegatesToActiveSequence) {
    Document doc;
    EXPECT_NO_THROW(doc.rootLayerForFrame(0));
    EXPECT_NO_THROW(doc.peekRootLayerForFrame(0));
    EXPECT_NO_THROW(doc.rootLayer());
    EXPECT_NO_THROW(doc.undoManager());
}

TEST(SequenceTest, DefaultState) {
    Sequence seq("Test", 1920, 1080);
    EXPECT_EQ(seq.name(), "Test");
    EXPECT_EQ(seq.currentFrame(), 0);
    EXPECT_EQ(seq.totalFrames(), 24);
    EXPECT_EQ(seq.fps(), 24);
    EXPECT_FALSE(seq.playing());
    EXPECT_FALSE(seq.looping());
}

TEST(SequenceTest, FrameNavigation) {
    Sequence seq("Test", 1920, 1080, 24, 24);
    seq.setCurrentFrame(10);
    EXPECT_EQ(seq.currentFrame(), 10);
    seq.nextFrame();
    EXPECT_EQ(seq.currentFrame(), 11);
    seq.prevFrame();
    EXPECT_EQ(seq.currentFrame(), 10);
    seq.goToEnd();
    EXPECT_EQ(seq.currentFrame(), 23);
    seq.goToStart();
    EXPECT_EQ(seq.currentFrame(), 0);
}

TEST(SequenceTest, FrameClamping) {
    Sequence seq("Test", 1920, 1080, 24, 10);
    seq.setCurrentFrame(100);
    EXPECT_EQ(seq.currentFrame(), 9);
    seq.setCurrentFrame(-5);
    EXPECT_EQ(seq.currentFrame(), 0);
}

TEST(SequenceTest, Keyframes) {
    Sequence seq("Test", 1920, 1080, 24, 24);
    seq.addKeyframe(0, 5);
    EXPECT_TRUE(seq.hasKeyframe(0, 5));
    EXPECT_FALSE(seq.hasKeyframe(0, 6));
    seq.removeKeyframe(0, 5);
    EXPECT_FALSE(seq.hasKeyframe(0, 5));
}

TEST(SequenceTest, HasDefaultLayer) {
    Sequence seq("Test", 1920, 1080);
    auto& root = seq.rootLayerForFrame(0);
    EXPECT_EQ(root.layerCount(), 1u);
    EXPECT_EQ(root.layerAt(0)->name(), "Layer 1");
    EXPECT_EQ(root.layerAt(0)->type(), LayerType::Raster);
}

TEST(SequenceTest, PeekNonExistentFrame) {
    Sequence seq("Test", 1920, 1080);
    EXPECT_EQ(seq.peekRootLayerForFrame(99), nullptr);
}

TEST(SequenceTest, ShiftFrameData) {
    Sequence seq("Test", 1920, 1080, 24, 10);
    auto& frame3 = seq.rootLayerForFrame(3);
    auto& frame5 = seq.rootLayerForFrame(5);
    seq.shiftFrameData(3, 2);
    EXPECT_EQ(seq.peekRootLayerForFrame(3), nullptr);
    EXPECT_NE(seq.peekRootLayerForFrame(5), nullptr);
    EXPECT_NE(seq.peekRootLayerForFrame(7), nullptr);
}

TEST(SequenceTest, RemoveFrameData) {
    Sequence seq("Test", 1920, 1080, 24, 10);
    seq.rootLayerForFrame(3);
    EXPECT_NE(seq.peekRootLayerForFrame(3), nullptr);
    seq.removeFrameData(3);
    EXPECT_EQ(seq.peekRootLayerForFrame(3), nullptr);
}

TEST(SequenceTest, IsolatedUndoManager) {
    Sequence seq1("A", 1920, 1080);
    Sequence seq2("B", 1920, 1080);
    EXPECT_FALSE(seq1.undoManager().canUndo());
    EXPECT_FALSE(seq2.undoManager().canUndo());
    EXPECT_NE(&seq1.undoManager(), &seq2.undoManager());
}

TEST(SequenceTest, Clone) {
    Sequence seq("Original", 1920, 1080, 30, 48);
    seq.setCurrentFrame(5);
    seq.setLooping(true);

    auto copy = seq.clone();
    EXPECT_EQ(copy->name(), "Original copy");
    EXPECT_EQ(copy->fps(), 30);
    EXPECT_EQ(copy->totalFrames(), 48);
    EXPECT_EQ(copy->currentFrame(), 5);
    EXPECT_TRUE(copy->looping());
    EXPECT_FALSE(copy->playing());
}

TEST(SequenceTest, CloneCustomName) {
    Sequence seq("Original", 1920, 1080);
    auto copy = seq.clone("Custom Copy");
    EXPECT_EQ(copy->name(), "Custom Copy");
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

TEST(SequenceTest, OpacityDefaults) {
    Sequence seq("Test", 1920, 1080);
    EXPECT_FLOAT_EQ(seq.opacity(), 1.0f);
}

TEST(SequenceTest, SetOpacity) {
    Sequence seq("Test", 1920, 1080);
    seq.setOpacity(0.75f);
    EXPECT_FLOAT_EQ(seq.opacity(), 0.75f);
    seq.setOpacity(-0.5f);
    EXPECT_FLOAT_EQ(seq.opacity(), 0.0f);
    seq.setOpacity(2.0f);
    EXPECT_FLOAT_EQ(seq.opacity(), 1.0f);
}

TEST(SequenceTest, ClonePreservesOpacity) {
    Sequence seq("Original", 1920, 1080);
    seq.setOpacity(0.6f);
    auto copy = seq.clone();
    EXPECT_FLOAT_EQ(copy->opacity(), 0.6f);
}

TEST(DocumentTest, SetSequenceOpacity) {
    Document doc;
    doc.addSequence("S2");
    doc.setActiveSequence(1);
    doc.activeSequence().setOpacity(0.3f);
    EXPECT_FLOAT_EQ(doc.activeSequence().opacity(), 0.3f);
    // Sequence 0 still at default
    EXPECT_FLOAT_EQ(doc.sequenceAt(0).opacity(), 1.0f);
}
