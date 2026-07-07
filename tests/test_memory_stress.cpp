#include <gtest/gtest.h>
#include "core/layer.hpp"
#include "core/document.hpp"
#include "core/timeline.hpp"
#include "core/undo_manager.hpp"
#include "engine/raster/raster_engine.hpp"
#include "engine/vector/bezier_path.hpp"
#include "engine/compositor/compositor.hpp"
#include "engine/compositor/node_graph.hpp"
#include <chrono>
#include <iostream>
#include <sstream>
#include <vector>
#include <set>
#include <cstdlib>
#include <windows.h>
#include <psapi.h>

using namespace fap;
using namespace fap::comp;

namespace {

int randInt(int lo, int hi) {
    return lo + (std::rand() % (hi - lo + 1));
}

struct MemSnapshot {
    SIZE_T workingSetMB = 0;
    SIZE_T privateBytesMB = 0;
    SIZE_T pageFaults = 0;
    double elapsedSec = 0.0;
};

MemSnapshot captureMemSnapshot(double elapsed) {
    HANDLE hProc = GetCurrentProcess();
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    pmc.cb = sizeof(pmc);
    MemSnapshot snap;
    snap.elapsedSec = elapsed;
    if (GetProcessMemoryInfo(hProc, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        snap.workingSetMB = pmc.WorkingSetSize / (1024ULL * 1024ULL);
        snap.privateBytesMB = pmc.PrivateUsage / (1024ULL * 1024ULL);
        snap.pageFaults = pmc.PageFaultCount;
    }
    return snap;
}

std::string fmt(const MemSnapshot& s) {
    std::ostringstream oss;
    oss << "[t=" << s.elapsedSec << "s] WS=" << s.workingSetMB
        << "MB Priv=" << s.privateBytesMB
        << "MB PF=" << s.pageFaults;
    return oss.str();
}

} // anon namespace

TEST(MemoryStressTest, SystemInfo) {
    MEMORYSTATUSEX mem{};
    mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);
    std::cout << "\n=== SYSTEM INFO ===" << std::endl;
    std::cout << "  Total Physical: " << (mem.ullTotalPhys / (1024*1024)) << " MB" << std::endl;
    std::cout << "  Avail Physical: " << (mem.ullAvailPhys / (1024*1024)) << " MB" << std::endl;
    std::cout << "  Total Virtual:  " << (mem.ullTotalVirtual / (1024*1024)) << " MB" << std::endl;
    std::cout << "  Avail Virtual:  " << (mem.ullAvailVirtual / (1024*1024)) << " MB" << std::endl;
    std::cout << "  Memory Load:    " << mem.dwMemoryLoad << "%" << std::endl;
    SUCCEED();
}

TEST(MemoryStressTest, Churn_AllocDealloc_1000Cycles) {
    auto t0 = std::chrono::steady_clock::now();

    for (int cycle = 0; cycle < 1000; ++cycle) {
        int n = 5 + (cycle % 20);
        GroupLayer root("ChurnRoot");
        for (int i = 0; i < n; ++i) {
            int w = 32 + randInt(1, 32) * 32;
            int h = 32 + randInt(1, 24) * 32;
            auto layer = std::make_unique<RasterLayer>("L" + std::to_string(i), w, h);
            for (int p = 0; p < 16; ++p) {
                int px = randInt(0, w - 1);
                int py = randInt(0, h - 1);
                layer->setPixel(px, py, Color::fromRGBA(128, 64, 32, 255));
            }
            root.addLayer(std::move(layer));
        }
    }

    auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    auto snap = captureMemSnapshot(elapsed);
    std::cout << "  Churn 1000 cycles: " << fmt(snap) << " rate=" << (1000.0/elapsed) << " cyc/s" << std::endl;
    SUCCEED();
}

TEST(MemoryStressTest, MassiveLayerCount_2000Layers) {
    auto t0 = std::chrono::steady_clock::now();
    GroupLayer root("Massive");

    for (int i = 0; i < 2000; ++i) {
        int w = 32 + (i % 16) * 16;
        int h = 32 + (i % 12) * 16;
        root.addLayer(std::make_unique<RasterLayer>("Layer " + std::to_string(i), w, h));
    }

    double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    auto snap = captureMemSnapshot(elapsed);
    std::cout << "  2000 layers created: " << fmt(snap) << " count=" << root.layerCount() << std::endl;

    EXPECT_EQ(root.layerCount(), 2000u);

    EXPECT_EQ(root.indexOfLayer(root.layerAt(0)), 0);
    EXPECT_EQ(root.indexOfLayer(root.layerAt(1999)), 1999);
    EXPECT_EQ(root.layerByUid(99999999), nullptr);
    EXPECT_NE(root.layerByUid(root.layerAt(500)->uid()), nullptr);

    std::vector<LayerUid> uids;
    std::vector<const uint32_t*> addrs;
    for (size_t i = 0; i < root.layerCount(); ++i) {
        auto* l = static_cast<RasterLayer*>(root.layerAt(static_cast<int>(i)));
        uids.push_back(l->uid());
        addrs.push_back(l->pixelData());
    }

    for (size_t i = 0; i < uids.size(); ++i) {
        auto* l = static_cast<RasterLayer*>(root.layerByUid(uids[i]));
        ASSERT_NE(l, nullptr) << "UID " << uids[i] << " at index " << i;
        EXPECT_EQ(l->pixelData(), addrs[i]) << "Buffer moved at index " << i;
    }

    double elapsed2 = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    auto snap2 = captureMemSnapshot(elapsed2);
    std::cout << "  Verified all: " << fmt(snap2) << std::endl;
}

TEST(MemoryStressTest, Single4KLayer_FullFill) {
    auto t0 = std::chrono::steady_clock::now();
    constexpr int kW = 3840;
    constexpr int kH = 2160;
    constexpr size_t kPixels = static_cast<size_t>(kW) * static_cast<size_t>(kH);

    auto layer = std::make_unique<RasterLayer>("4K", kW, kH);
    EXPECT_EQ(layer->pixelCount(), kPixels);

    for (int y = 0; y < kH; y += 2) {
        for (int x = 0; x < kW; x += 2) {
            layer->setPixel(x, y, Color::fromRGBA(
                (x + y) % 256,
                (x * 3) % 256,
                (y * 5) % 256,
                255));
        }
    }

    double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    auto snap = captureMemSnapshot(elapsed);
    std::cout << "  4K fill: " << fmt(snap) << " pixels=" << kPixels << std::endl;

    EXPECT_EQ(layer->pixelAt(0, 0), Color::fromRGBA(0, 0, 0, 255));

    auto clone = layer->clone();
    auto* rclone = static_cast<RasterLayer*>(clone.get());
    EXPECT_EQ(rclone->pixelCount(), kPixels);
    EXPECT_TRUE(layer->isBufferShared());
    EXPECT_TRUE(rclone->isBufferShared());

    double elapsed2 = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    auto snap2 = captureMemSnapshot(elapsed2);
    std::cout << "  COW cloned: " << fmt(snap2) << " shared=" << layer->isBufferShared() << std::endl;
}

TEST(MemoryStressTest, Ten_4KLayers_Simultaneous) {
    auto t0 = std::chrono::steady_clock::now();
    GroupLayer root("Ten4K");

    for (int i = 0; i < 10; ++i) {
        auto layer = std::make_unique<RasterLayer>("4K." + std::to_string(i), 3840, 2160);
        for (int p = 0; p < 50000; ++p) {
            int x = (p * 137 + i * 17) % 3840;
            int y = (p * 251 + i * 53) % 2160;
            layer->setPixel(x, y, Color::fromRGBA(p % 256, (p*2) % 256, (p*3) % 256, 255));
        }
        root.addLayer(std::move(layer));
    }

    double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    auto snap = captureMemSnapshot(elapsed);
    std::cout << "  Ten 4K layers: " << fmt(snap) << " count=" << root.layerCount() << std::endl;

    EXPECT_EQ(root.layerCount(), 10u);

    for (size_t i = 0; i < root.layerCount(); ++i) {
        auto* l = static_cast<RasterLayer*>(root.layerAt(static_cast<int>(i)));
        ASSERT_NE(l, nullptr);
        EXPECT_EQ(l->pixelCount(), 3840u * 2160u);
    }

    std::set<LayerUid> seen;
    for (size_t i = 0; i < root.layerCount(); ++i) {
        LayerUid uid = root.layerAt(static_cast<int>(i))->uid();
        EXPECT_TRUE(seen.insert(uid).second) << "Duplicate UID: " << uid;
    }
}

TEST(MemoryStressTest, Document_ManyFrames) {
    auto t0 = std::chrono::steady_clock::now();
    Document doc;
    doc.setCanvasSize(1920, 1080);
    doc.setFPS(24);
    doc.setTotalFrames(240);

    auto& root = doc.rootLayer();
    for (int i = 0; i < 5; ++i) {
        root.addLayer(std::make_unique<RasterLayer>("Frame0.L" + std::to_string(i), 960, 540));
    }

    for (int f = 0; f < 240; f += 10) {
        auto& frameRoot = doc.rootLayerForFrame(f);
        EXPECT_GE(frameRoot.layerCount(), 0u);
    }

    double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    auto snap = captureMemSnapshot(elapsed);
    std::cout << "  Document 240 frames: " << fmt(snap) << std::endl;
    SUCCEED();
}

TEST(MemoryStressTest, UndoStack_Full128_WithLargeBuffers) {
    auto t0 = std::chrono::steady_clock::now();
    UndoManager undo;

    struct TestCmd : UndoCommand {
        std::vector<uint32_t> data;
        explicit TestCmd(size_t bytes) : data(bytes / sizeof(uint32_t)) {
            for (size_t i = 0; i < data.size() && i < 1000; ++i) {
                data[i] = static_cast<uint32_t>(i);
            }
        }
        void undo() override { (void)data.size(); }
        void redo() override { (void)data.size(); }
    };

    for (size_t i = 0; i < UndoManager::kMaxEntries; ++i) {
        size_t bytes = 64 * 1024 + (i * 16 * 1024);
        if (bytes > 8 * 1024 * 1024) bytes = 8 * 1024 * 1024;
        undo.pushCommand(std::make_unique<TestCmd>(bytes));
    }

    double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    auto snap = captureMemSnapshot(elapsed);
    std::cout << "  Undo stack 128: " << fmt(snap)
              << " canUndo=" << undo.canUndo() << " canRedo=" << undo.canRedo() << std::endl;

    EXPECT_TRUE(undo.canUndo());
    EXPECT_FALSE(undo.canRedo());

    for (size_t i = 0; i < UndoManager::kMaxEntries; ++i) {
        undo.undo();
    }
    EXPECT_FALSE(undo.canUndo());
    EXPECT_TRUE(undo.canRedo());

    auto snap2 = captureMemSnapshot(elapsed);
    std::cout << "  After undo all: " << fmt(snap2) << std::endl;
}

TEST(MemoryStressTest, CowStress_DeepCloneChain) {
    auto t0 = std::chrono::steady_clock::now();
    auto original = std::make_unique<RasterLayer>("Original", 2048, 2048);

    for (int y = 0; y < 2048; y += 4) {
        for (int x = 0; x < 2048; x += 4) {
            original->setPixel(x, y, Color::fromRGBA(x % 256, y % 256, 128, 255));
        }
    }

    const int kClones = 20;
    std::vector<std::unique_ptr<Layer>> clones;
    clones.reserve(kClones);
    for (int i = 0; i < kClones; ++i) {
        clones.push_back(original->clone());
    }

    for (int i = 0; i < kClones; ++i) {
        auto* r = static_cast<RasterLayer*>(clones[i].get());
        r->ensureUnique();
        for (int p = 0; p < 1000; ++p) {
            int x = (p * 71 + i * 31) % 2048;
            int y = (p * 113 + i * 67) % 2048;
            r->setPixel(x, y, Color::fromRGBA(i * 10, 255 - i * 10, i * 5, 255));
        }
    }

    for (int i = 0; i < kClones; ++i) {
        auto* r = static_cast<RasterLayer*>(clones[i].get());
        EXPECT_FALSE(r->isBufferShared()) << "Clone " << i << " still sharing";
    }

    double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    auto snap = captureMemSnapshot(elapsed);
    std::cout << "  COW stress 20 clones: " << fmt(snap) << std::endl;
    SUCCEED();
}

TEST(MemoryStressTest, Compositor_FullTarget) {
    auto t0 = std::chrono::steady_clock::now();
    Compositor comp;
    RasterEngine target(1280, 720);

    GroupLayer root("Root");
    for (int i = 0; i < 100; ++i) {
        auto layer = std::make_unique<RasterLayer>("L" + std::to_string(i), 1280, 720);
        for (int p = 0; p < 500; ++p) {
            int x = (p * 13 + i * 7) % 1280;
            int y = (p * 17 + i * 11) % 720;
            layer->setPixel(x, y, Color::fromRGBA((i*20) % 256, (i*10) % 256, 128, 255 - (i * 2) % 256));
        }
        root.addLayer(std::move(layer));
    }

    for (int pass = 0; pass < 3; ++pass) {
        target.clear();
        comp.composite(root, target);
        EXPECT_EQ(target.width(), 1280);
        EXPECT_EQ(target.height(), 720);
    }

    double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    auto snap = captureMemSnapshot(elapsed);
    std::cout << "  Compositor 100 layers: " << fmt(snap) << " passes=3" << std::endl;
    SUCCEED();
}

TEST(MemoryStressTest, NodeGraph_LargeScale) {
    auto t0 = std::chrono::steady_clock::now();
    NodeGraph graph;

    // Create 80 input nodes
    std::vector<NodeId> inputIds;
    for (int i = 0; i < 80; ++i) {
        auto* n = graph.addNode(std::make_unique<InputNode>("Input." + std::to_string(i)));
        inputIds.push_back(n->id());
    }

    // Create 30 filter nodes
    std::vector<NodeId> filterIds;
    for (int i = 0; i < 30; ++i) {
        auto* f = graph.addNode(std::make_unique<FilterNode>(FilterNode::FilterType::Blur, "Filter." + std::to_string(i)));
        filterIds.push_back(f->id());
    }

    // Create 15 transform nodes
    std::vector<NodeId> xformIds;
    for (int i = 0; i < 15; ++i) {
        auto* t = graph.addNode(std::make_unique<TransformNode>("Xform." + std::to_string(i)));
        xformIds.push_back(t->id());
    }

    // Connect each filter to 2 inputs
    for (size_t i = 0; i < filterIds.size(); ++i) {
        int srcA = static_cast<int>(i * 2) % static_cast<int>(inputIds.size());
        int srcB = static_cast<int>(i * 2 + 1) % static_cast<int>(inputIds.size());
        graph.connect(inputIds[srcA], 0, filterIds[i], 0);
        graph.connect(inputIds[srcB], 0, filterIds[i], 1);
    }

    // Connect each transform to a filter
    for (size_t i = 0; i < xformIds.size(); ++i) {
        int src = static_cast<int>(i) % static_cast<int>(filterIds.size());
        graph.connect(filterIds[src], 0, xformIds[i], 0);
    }

    EXPECT_NE(graph.nodeById(inputIds[0]), nullptr);
    EXPECT_NE(graph.nodeById(xformIds.back()), nullptr);

    double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    auto snap = captureMemSnapshot(elapsed);
    std::cout << "  NodeGraph " << graph.nodes().size() << " nodes: " << fmt(snap) << std::endl;
    SUCCEED();
}

TEST(MemoryStressTest, PointerStability_Extreme_InsertRemoveChurn) {
    auto t0 = std::chrono::steady_clock::now();
    GroupLayer root("Stability");

    for (int i = 0; i < 500; ++i) {
        root.addLayer(std::make_unique<RasterLayer>("Pre." + std::to_string(i), 128, 128));
    }

    std::vector<LayerUid> uids;
    std::vector<const uint32_t*> addrs;
    for (int i = 0; i < 500; ++i) {
        auto* l = static_cast<RasterLayer*>(root.layerAt(i));
        uids.push_back(l->uid());
        addrs.push_back(l->pixelData());
    }

    for (int iter = 0; iter < 200; ++iter) {
        int insPos = randInt(0, static_cast<int>(root.layerCount()));
        auto inserted = std::make_unique<RasterLayer>("Ins." + std::to_string(iter), 64, 64);
        root.addLayer(std::move(inserted));

        if (root.layerCount() > 1) {
            int delPos = randInt(0, static_cast<int>(root.layerCount()) - 1);
            root.removeLayer(delPos);
        }
    }

    int lostCount = 0;
    for (size_t i = 0; i < uids.size(); ++i) {
        auto* l = root.layerByUid(uids[i]);
        if (l) {
            auto* rl = static_cast<RasterLayer*>(l);
            EXPECT_EQ(rl->pixelData(), addrs[i]) << "Buffer moved for UID " << uids[i];
        } else {
            ++lostCount;
        }
    }

    double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    auto snap = captureMemSnapshot(elapsed);
    std::cout << "  Stability churn: " << fmt(snap) << " lost=" << lostCount
              << " finalCount=" << root.layerCount() << std::endl;
    SUCCEED();
}

TEST(MemoryStressTest, Bezier_MassivePathGeneration) {
    auto t0 = std::chrono::steady_clock::now();

    for (int i = 0; i < 5000; ++i) {
        BezierPath path;
        double x = (i * 0.7);
        double y = (i * 0.3);
        path.moveTo(static_cast<float>(x), static_cast<float>(y));
        path.curveTo(static_cast<float>(x + 50), static_cast<float>(y - 20),
                     static_cast<float>(x + 80), static_cast<float>(y + 30),
                     static_cast<float>(x + 100), static_cast<float>(y + 10));
        path.curveTo(static_cast<float>(x + 120), static_cast<float>(y - 10),
                     static_cast<float>(x + 150), static_cast<float>(y + 40),
                     static_cast<float>(x + 160), static_cast<float>(y));
        auto pts = path.flattenPoints(2.0f);
        EXPECT_GT(pts.size(), 0u);
    }

    double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    auto snap = captureMemSnapshot(elapsed);
    std::cout << "  Bezier 5K paths: " << fmt(snap) << std::endl;
    SUCCEED();
}

TEST(MemoryStressTest, Combined_MixedStress) {
    auto t0 = std::chrono::steady_clock::now();

    Document doc;
    doc.setCanvasSize(1920, 1080);

    auto& root = doc.rootLayer();
    for (int i = 0; i < 50; ++i) {
        int w = 128 + (i % 8) * 128;
        int h = 128 + (i % 6) * 128;
        auto layer = std::make_unique<RasterLayer>("Mix.L" + std::to_string(i), w, h);
        for (int p = 0; p < 200; ++p) {
            int x = (p * 31 + i) % w;
            int y = (p * 47 + i * 2) % h;
            layer->setPixel(x, y, Color::fromRGBA(i * 5, p % 256, 128, 255));
        }
        root.addLayer(std::move(layer));
    }

    auto vec = std::make_unique<VectorLayer>("Vectors");
    for (int s = 0; s < 100; ++s) {
        VectorStroke stroke;
        stroke.color = Color::fromRGBA(s % 256, (s*3) % 256, (s*7) % 256, 255);
        stroke.width = 1.0f + (s % 20);
        BezierPath bp;
        bp.moveTo(static_cast<float>(s * 10), static_cast<float>(s * 5));
        bp.curveTo(static_cast<float>(s * 10 + 30), static_cast<float>(s * 5 - 15),
                   static_cast<float>(s * 10 + 50), static_cast<float>(s * 5 + 20),
                   static_cast<float>(s * 10 + 60), static_cast<float>(s * 5));
        stroke.path = bp;
        vec->addStroke(stroke);
    }
    root.addLayer(std::move(vec));

    auto group = std::make_unique<GroupLayer>("Nested");
    for (int i = 0; i < 20; ++i) {
        group->addLayer(std::make_unique<RasterLayer>("Nested.L" + std::to_string(i), 256, 256));
    }
    root.addLayer(std::move(group));

    auto clone = root.clone();
    auto* cloneGroup = static_cast<GroupLayer*>(clone.get());
    EXPECT_GE(cloneGroup->layerCount(), root.layerCount());

    double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    auto snap = captureMemSnapshot(elapsed);
    std::cout << "  Combined stress: " << fmt(snap) << " layers=" << root.layerCount() << std::endl;
    SUCCEED();
}

TEST(MemoryStressTest, GrowUntilNearLimit) {
    auto t0 = std::chrono::steady_clock::now();
    std::cout << "\n=== GROW UNTIL MEMORY PRESSURE ===" << std::endl;

    MEMORYSTATUSEX mem{};
    mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);
    size_t availMB = mem.ullAvailPhys / (1024ULL * 1024ULL);
    std::cout << "  Available physical: " << availMB << " MB" << std::endl;

    size_t targetMB = availMB * 60 / 100;
    size_t bytesPerLayer = 3840ULL * 2160ULL * sizeof(uint32_t);
    size_t targetLayers = targetMB * 1024 * 1024 / bytesPerLayer;
    if (targetLayers > 50) targetLayers = 50;
    if (targetLayers < 1) targetLayers = 1;

    std::cout << "  Target: " << targetMB << " MB, attempting " << targetLayers << " 4K layers" << std::endl;

    GroupLayer root("GrowTest");
    std::vector<const uint32_t*> addrs;
    size_t allocated = 0;

    for (size_t i = 0; i < targetLayers; ++i) {
        try {
            auto layer = std::make_unique<RasterLayer>("Heavy." + std::to_string(i), 3840, 2160);
            addrs.push_back(layer->pixelData());

            for (int p = 0; p < 10000; ++p) {
                int x = (p * 97 + static_cast<int>(i) * 13) % 3840;
                int y = (p * 151 + static_cast<int>(i) * 29) % 2160;
                layer->setPixel(x, y, Color::fromRGBA(128, 128, 128, 255));
            }

            root.addLayer(std::move(layer));
            ++allocated;

            if (i % 5 == 0) {
                auto snap = captureMemSnapshot(0);
                std::cout << "  Layer " << i << "/" << targetLayers
                          << " WS=" << snap.workingSetMB << "MB" << std::endl;
            }
        } catch (const std::bad_alloc& e) {
            std::cout << "  *** BAD_ALLOC at layer " << i << ": " << e.what() << " ***" << std::endl;
            break;
        } catch (const std::exception& e) {
            std::cout << "  *** Exception at layer " << i << ": " << e.what() << " ***" << std::endl;
            break;
        }
    }

    double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    auto finalSnap = captureMemSnapshot(elapsed);
    std::cout << "  Final: " << fmt(finalSnap) << " allocated=" << allocated << std::endl;

    GlobalMemoryStatusEx(&mem);
    size_t remainingMB = mem.ullAvailPhys / (1024ULL * 1024ULL);
    std::cout << "  Remaining physical: " << remainingMB << " MB" << std::endl;

    for (size_t i = 0; i < allocated; ++i) {
        auto* l = static_cast<RasterLayer*>(root.layerAt(static_cast<int>(i)));
        ASSERT_NE(l, nullptr);
        EXPECT_EQ(l->pixelData(), addrs[i]);
    }

    SUCCEED();
}

TEST(MemoryStressTest, FinalSummary) {
    auto snap = captureMemSnapshot(0);
    std::cout << "\n========================================" << std::endl;
    std::cout << "  MEMORY STRESS TEST SUMMARY" << std::endl;
    std::cout << "  Date: " << __DATE__ << " " << __TIME__ << std::endl;
    std::cout << "  Final Working Set: " << snap.workingSetMB << " MB" << std::endl;
    std::cout << "  Final Private Bytes: " << snap.privateBytesMB << " MB" << std::endl;
    std::cout << "  Total Page Faults: " << snap.pageFaults << std::endl;
    std::cout << "========================================" << std::endl;
    SUCCEED();
}
