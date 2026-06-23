#include <gtest/gtest.h>
#include "core/undo_manager.hpp"
#include "core/types.hpp"
#include "core/layer.hpp"
#include "engine/vector/bezier_path.hpp"

using namespace fap;

class RasterCommand : public UndoCommand {
public:
    LayerUid layerUid = 0;
    int layerIndex = 0;
    int frame = 0;
    std::vector<uint32_t> beforeData;
    std::vector<uint32_t> afterData;
    std::vector<uint32_t> currentData;

    void undo() override { currentData = beforeData; }
    void redo() override { currentData = afterData; }
};

static std::unique_ptr<RasterCommand> makeRasterCmd(int layerIdx, int frame,
                                                     int w, int h,
                                                     uint32_t beforeFill,
                                                     uint32_t afterFill,
                                                     LayerUid uid = 0) {
    auto cmd = std::make_unique<RasterCommand>();
    cmd->layerUid = uid ? uid : static_cast<LayerUid>(layerIdx + 1);
    cmd->layerIndex = layerIdx;
    cmd->frame = frame;
    cmd->beforeData.assign(static_cast<size_t>(w) * h, beforeFill);
    cmd->afterData.assign(static_cast<size_t>(w) * h, afterFill);
    return cmd;
}

class VectorCommand : public UndoCommand {
public:
    LayerUid layerUid = 0;
    int layerIndex = 0;
    int frame = 0;
    std::vector<VectorStroke> beforeStrokes;
    std::vector<VectorStroke> afterStrokes;
    std::vector<VectorStroke> currentStrokes;

    void undo() override { currentStrokes = beforeStrokes; }
    void redo() override { currentStrokes = afterStrokes; }
};

static std::unique_ptr<VectorCommand> makeVectorCmd(int layerIdx, int frame) {
    auto cmd = std::make_unique<VectorCommand>();
    cmd->layerUid = static_cast<LayerUid>(layerIdx + 1);
    cmd->layerIndex = layerIdx;
    cmd->frame = frame;
    VectorStroke vsBefore;
    vsBefore.color = Color::fromRGBA(255, 0, 0, 255);
    vsBefore.width = 3.0f;
    cmd->beforeStrokes.push_back(vsBefore);
    VectorStroke vsAfter = vsBefore;
    vsAfter.width = 10.0f;
    cmd->afterStrokes.push_back(vsAfter);
    return cmd;
}

TEST(UndoManagerTest, EmptyStack) {
    UndoManager mgr;
    EXPECT_FALSE(mgr.canUndo());
    EXPECT_FALSE(mgr.canRedo());
    mgr.undo();
    mgr.redo();
}

TEST(UndoManagerTest, UndoRedoRaster) {
    UndoManager mgr;
    auto cmd = makeRasterCmd(0, 0, 4, 4, 0x00000000, 0xFFFF0000);
    auto* raw = cmd.get();
    mgr.pushCommand(std::move(cmd));

    EXPECT_EQ(raw->currentData[0], 0xFFFF0000u);
    EXPECT_TRUE(mgr.canUndo());
    EXPECT_FALSE(mgr.canRedo());

    mgr.undo();
    EXPECT_EQ(raw->currentData[0], 0x00000000u);
    EXPECT_EQ(raw->layerIndex, 0);
    EXPECT_EQ(raw->frame, 0);
    EXPECT_TRUE(mgr.canRedo());

    mgr.redo();
    EXPECT_EQ(raw->currentData[0], 0xFFFF0000u);
    EXPECT_TRUE(mgr.canUndo());
}

TEST(UndoManagerTest, UndoRedoVector) {
    UndoManager mgr;
    auto cmd = makeVectorCmd(0, 5);
    auto* raw = cmd.get();
    mgr.pushCommand(std::move(cmd));

    EXPECT_FLOAT_EQ(raw->currentStrokes[0].width, 10.0f);
    EXPECT_TRUE(mgr.canUndo());

    mgr.undo();
    EXPECT_FLOAT_EQ(raw->currentStrokes[0].width, 3.0f);

    mgr.redo();
    EXPECT_FLOAT_EQ(raw->currentStrokes[0].width, 10.0f);
}

TEST(UndoManagerTest, MultipleEntries) {
    UndoManager mgr;
    auto c0 = makeRasterCmd(0, 0, 2, 2, 0, 1);
    auto* r0 = c0.get();
    auto c1 = makeRasterCmd(0, 1, 2, 2, 2, 3);
    auto* r1 = c1.get();
    mgr.pushCommand(std::move(c0));
    mgr.pushCommand(std::move(c1));

    EXPECT_EQ(r1->currentData[0], 3u);
    mgr.undo();
    EXPECT_EQ(r1->currentData[0], 2u);
    mgr.undo();
    EXPECT_EQ(r0->currentData[0], 0u);
    EXPECT_FALSE(mgr.canUndo());

    mgr.redo();
    EXPECT_EQ(r0->currentData[0], 1u);
    mgr.redo();
    EXPECT_EQ(r1->currentData[0], 3u);
    EXPECT_TRUE(mgr.canUndo());
}

TEST(UndoManagerTest, NewPushClearsRedo) {
    UndoManager mgr;
    auto c0 = makeRasterCmd(0, 0, 2, 2, 0, 1);
    mgr.pushCommand(std::move(c0));
    mgr.undo();
    EXPECT_TRUE(mgr.canRedo());

    auto c1 = makeRasterCmd(0, 0, 2, 2, 2, 3);
    mgr.pushCommand(std::move(c1));
    EXPECT_FALSE(mgr.canRedo());
}

TEST(UndoManagerTest, Clear) {
    UndoManager mgr;
    mgr.pushCommand(makeRasterCmd(0, 0, 2, 2, 0, 1));
    mgr.pushCommand(makeRasterCmd(0, 0, 2, 2, 2, 3));
    mgr.clear();
    EXPECT_FALSE(mgr.canUndo());
    EXPECT_FALSE(mgr.canRedo());
}

TEST(UndoManagerTest, MaxEntries) {
    UndoManager mgr;
    for (int i = 0; i < 200; ++i) {
        mgr.pushCommand(makeRasterCmd(0, i, 1, 1, 0, 1));
    }
    EXPECT_TRUE(mgr.canUndo());
    EXPECT_FALSE(mgr.canRedo());
    int count = 0;
    while (mgr.canUndo()) {
        mgr.undo();
        ++count;
    }
    EXPECT_LE(count, 128);
    EXPECT_GE(count, 1);
}

TEST(UndoManagerTest, PreservesLayerUid) {
    UndoManager mgr;
    auto cmd = makeRasterCmd(0, 0, 2, 2, 0, 1, 42);
    auto* raw = cmd.get();
    mgr.pushCommand(std::move(cmd));

    mgr.undo();
    EXPECT_EQ(raw->layerUid, 42u);
    EXPECT_EQ(raw->frame, 0);
    EXPECT_EQ(raw->layerIndex, 0);
}
