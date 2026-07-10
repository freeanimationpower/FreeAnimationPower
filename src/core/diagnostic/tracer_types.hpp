#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <sstream>

namespace fap::diagnostic {

enum class EventCategory : uint8_t {
    Session,
    Mouse,
    Keyboard,
    ToolState,
    Stroke,
    Buffer,
    UndoRedo,
    Layer,
    Frame,
    Playback,
    IO,
    Paint,
    App
};

const char* categoryName(EventCategory cat);

struct Vec2 {
    double x = 0.0;
    double y = 0.0;
};

struct Rect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

struct TraceEvent {
    uint64_t sequence = 0;
    double timestamp = 0.0;
    EventCategory category;
    std::string event;
    std::string tool;
    std::string color;
    int toolSize = 0;
    double pressure = 0.0;
    int button = 0;
    int key = 0;
    int dabs = 0;
    int layerIndex = 0;
    uint64_t layerUid = 0;
    int frameIndex = 0;
    Vec2 position;
    Rect dirtyRect;
    int width = 0;
    int height = 0;
    int originX = 0;
    int originY = 0;
    size_t bufferSize = 0;
    size_t undoStackSize = 0;
    size_t redoStackSize = 0;
    double durationMs = 0.0;
    int opacity = 100;
    int hardness = 80;
    std::string brushShape;
    std::string fileName;
    std::string layerName;
    std::string modifiers;
    int sessionId = 0;
};

void writeEventJson(std::ostringstream& ss, const TraceEvent& evt);

} // namespace fap::diagnostic
