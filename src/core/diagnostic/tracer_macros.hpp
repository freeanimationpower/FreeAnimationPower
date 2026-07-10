#pragma once

#include "tracer_types.hpp"

#ifdef FAP_DIAGNOSTIC_TRACER

#include "tracer.hpp"

namespace fap::diagnostic {

inline void traceMouseEvent(const char* evt, int px, int py, int btn, double press) {
    TraceEvent e;
    e.category = EventCategory::Mouse;
    e.event = evt;
    e.position.x = static_cast<double>(px);
    e.position.y = static_cast<double>(py);
    e.button = btn;
    e.pressure = press;
    tracer().record(e);
}

inline void traceKeyEvent(int key, const std::string& mods) {
    TraceEvent e;
    e.category = EventCategory::Keyboard;
    e.event = "key";
    e.key = key;
    e.modifiers = mods;
    tracer().record(e);
}

inline void traceToolEvent(const std::string& toolName, int size, int opac, int hard,
                            const std::string& shape, const std::string& col) {
    TraceEvent e;
    e.category = EventCategory::ToolState;
    e.event = "change";
    e.tool = toolName;
    e.toolSize = size;
    e.opacity = opac;
    e.hardness = hard;
    e.brushShape = shape;
    e.color = col;
    tracer().record(e);
}

inline void traceStrokeEvent(const char* evt, int dabs) {
    TraceEvent e;
    e.category = EventCategory::Stroke;
    e.event = evt;
    e.dabs = dabs;
    tracer().record(e);
}

inline void traceBufferEvent(const char* evt, int w, int h, int ox, int oy, size_t sz) {
    TraceEvent e;
    e.category = EventCategory::Buffer;
    e.event = evt;
    e.width = w;
    e.height = h;
    e.originX = ox;
    e.originY = oy;
    e.bufferSize = sz;
    tracer().record(e);
}

inline void traceBufferCommitEvent(uint64_t luid, int rx, int ry, int rw, int rh, int frame) {
    TraceEvent e;
    e.category = EventCategory::Buffer;
    e.event = "commit";
    e.layerUid = luid;
    e.dirtyRect.x = rx;
    e.dirtyRect.y = ry;
    e.dirtyRect.w = rw;
    e.dirtyRect.h = rh;
    e.frameIndex = frame;
    tracer().record(e);
}

inline void traceUndoEvent(const char* op, size_t unSize, size_t reSize) {
    TraceEvent e;
    e.category = EventCategory::UndoRedo;
    e.event = op;
    e.undoStackSize = unSize;
    e.redoStackSize = reSize;
    tracer().record(e);
}

inline void traceLayerEvent(const char* evt, int idx, uint64_t luid, const std::string& name) {
    TraceEvent e;
    e.category = EventCategory::Layer;
    e.event = evt;
    e.layerIndex = idx;
    e.layerUid = luid;
    e.layerName = name;
    tracer().record(e);
}

inline void traceFrameEvent(const char* evt, int frame) {
    TraceEvent e;
    e.category = EventCategory::Frame;
    e.event = evt;
    e.frameIndex = frame;
    tracer().record(e);
}

inline void tracePlaybackEvent(const char* evt, int frame, int fps) {
    TraceEvent e;
    e.category = EventCategory::Playback;
    e.event = evt;
    e.frameIndex = frame;
    e.durationMs = static_cast<double>(fps);
    tracer().record(e);
}

inline void traceIOEvent(const char* evt, const std::string& filename, double ms) {
    TraceEvent e;
    e.category = EventCategory::IO;
    e.event = evt;
    e.fileName = filename;
    e.durationMs = ms;
    tracer().record(e);
}

inline void tracePaintEvent(double ms) {
    TraceEvent e;
    e.category = EventCategory::Paint;
    e.event = "paint";
    e.durationMs = ms;
    tracer().record(e);
}

inline void traceAppEvent(const char* evt) {
    tracer().record(EventCategory::App, evt);
}

inline void traceCatEvent(EventCategory cat, const char* evt) {
    tracer().record(cat, evt);
}

} // namespace fap::diagnostic

#define FAP_TRACE_CAT(cat, evt)     fap::diagnostic::traceCatEvent(cat, evt)
#define FAP_TRACE_MOUSE(evt, px, py, btn, press) fap::diagnostic::traceMouseEvent(evt, px, py, btn, press)
#define FAP_TRACE_KEY(key, mods)    fap::diagnostic::traceKeyEvent(key, mods)
#define FAP_TRACE_TOOL(toolName, size, opac, hard, shape, col) \
    fap::diagnostic::traceToolEvent(toolName, size, opac, hard, shape, col)
#define FAP_TRACE_STROKE(evt, dabs) fap::diagnostic::traceStrokeEvent(evt, dabs)
#define FAP_TRACE_BUFFER(evt, w, h, ox, oy, sz) fap::diagnostic::traceBufferEvent(evt, w, h, ox, oy, sz)
#define FAP_TRACE_BUFFER_COMMIT(luid, rx, ry, rw, rh, frame) \
    fap::diagnostic::traceBufferCommitEvent(luid, rx, ry, rw, rh, frame)
#define FAP_TRACE_UNDO(op, unSize, reSize) fap::diagnostic::traceUndoEvent(op, unSize, reSize)
#define FAP_TRACE_LAYER(evt, idx, luid, name) fap::diagnostic::traceLayerEvent(evt, idx, luid, name)
#define FAP_TRACE_FRAME(evt, frame)  fap::diagnostic::traceFrameEvent(evt, frame)
#define FAP_TRACE_PLAYBACK(evt, frame, fps) fap::diagnostic::tracePlaybackEvent(evt, frame, fps)
#define FAP_TRACE_IO(evt, filename, ms) fap::diagnostic::traceIOEvent(evt, filename, ms)
#define FAP_TRACE_PAINT(ms)          fap::diagnostic::tracePaintEvent(ms)
#define FAP_TRACE_APP(evt)           fap::diagnostic::traceAppEvent(evt)

#else

#define FAP_TRACE_CAT(cat, evt)        ((void)0)
#define FAP_TRACE_MOUSE(...)           ((void)0)
#define FAP_TRACE_KEY(...)             ((void)0)
#define FAP_TRACE_TOOL(...)            ((void)0)
#define FAP_TRACE_STROKE(...)          ((void)0)
#define FAP_TRACE_BUFFER(...)          ((void)0)
#define FAP_TRACE_BUFFER_COMMIT(...)   ((void)0)
#define FAP_TRACE_UNDO(...)            ((void)0)
#define FAP_TRACE_LAYER(...)           ((void)0)
#define FAP_TRACE_FRAME(...)           ((void)0)
#define FAP_TRACE_PLAYBACK(...)        ((void)0)
#define FAP_TRACE_IO(...)              ((void)0)
#define FAP_TRACE_PAINT(...)           ((void)0)
#define FAP_TRACE_APP(...)             ((void)0)

#endif
