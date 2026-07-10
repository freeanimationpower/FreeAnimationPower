#include "tracer.hpp"

#ifdef FAP_DIAGNOSTIC_TRACER

#include "tracer_types.hpp"
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QWidget>
#include <QString>
#include <sstream>
#include <iomanip>

namespace fap::diagnostic {

const char* categoryName(EventCategory cat) {
    switch (cat) {
    case EventCategory::Session:   return "session";
    case EventCategory::Mouse:     return "mouse";
    case EventCategory::Keyboard:  return "keyboard";
    case EventCategory::ToolState: return "tool";
    case EventCategory::Stroke:    return "stroke";
    case EventCategory::Buffer:    return "buffer";
    case EventCategory::UndoRedo:  return "undo";
    case EventCategory::Layer:     return "layer";
    case EventCategory::Frame:     return "frame";
    case EventCategory::Playback:  return "playback";
    case EventCategory::IO:        return "io";
    case EventCategory::Paint:     return "paint";
    case EventCategory::App:       return "app";
    }
    return "unknown";
}

static void writeString(std::ostringstream& ss, const std::string& s) {
    ss << '"';
    for (char c : s) {
        if (c == '"') ss << "\\\"";
        else if (c == '\\') ss << "\\\\";
        else if (c == '\n') ss << "\\n";
        else if (c == '\r') ss << "\\r";
        else if (c == '\t') ss << "\\t";
        else ss << c;
    }
    ss << '"';
}

void writeEventJson(std::ostringstream& ss, const TraceEvent& e) {
    ss << "{";
    ss << "\"seq\":" << e.sequence << ",";
    ss << std::fixed << std::setprecision(6) << "\"ts\":" << e.timestamp << ",";
    ss << "\"cat\":\"" << categoryName(e.category) << "\",";
    ss << "\"evt\":"; writeString(ss, e.event);

    if (!e.tool.empty())      { ss << ",\"tool\":"; writeString(ss, e.tool); }
    if (!e.color.empty())     { ss << ",\"color\":\"" << e.color << "\""; }
    if (e.toolSize > 0)       { ss << ",\"size\":" << e.toolSize; }
    if (e.pressure > 0.0)     { ss << ",\"pressure\":" << std::fixed << std::setprecision(3) << e.pressure; }
    if (e.button != 0)        { ss << ",\"btn\":" << e.button; }
    if (e.key != 0)           { ss << ",\"key\":" << e.key; }
    if (e.dabs > 0)           { ss << ",\"dabs\":" << e.dabs; }
    if (e.layerIndex >= 0)    { ss << ",\"layerIdx\":" << e.layerIndex; }
    if (e.layerUid > 0)       { ss << ",\"layerUid\":" << e.layerUid; }
    if (e.frameIndex >= 0)    { ss << ",\"frame\":" << e.frameIndex; }

    if (e.position.x != 0.0 || e.position.y != 0.0) {
        ss << std::fixed << std::setprecision(1)
           << ",\"pos\":{\"x\":" << e.position.x << ",\"y\":" << e.position.y << "}";
    }

    if (e.dirtyRect.w > 0 || e.dirtyRect.h > 0) {
        ss << ",\"rect\":{\"x\":" << e.dirtyRect.x
           << ",\"y\":" << e.dirtyRect.y
           << ",\"w\":" << e.dirtyRect.w
           << ",\"h\":" << e.dirtyRect.h << "}";
    }

    if (e.width > 0)          { ss << ",\"w\":" << e.width; }
    if (e.height > 0)         { ss << ",\"h\":" << e.height; }
    if (e.originX != 0)       { ss << ",\"ox\":" << e.originX; }
    if (e.originY != 0)       { ss << ",\"oy\":" << e.originY; }
    if (e.bufferSize > 0)     { ss << ",\"bufSz\":" << e.bufferSize; }
    if (e.undoStackSize > 0)  { ss << ",\"undoSz\":" << e.undoStackSize; }
    if (e.redoStackSize > 0)  { ss << ",\"redoSz\":" << e.redoStackSize; }
    if (e.durationMs > 0.0)   { ss << ",\"dur\":" << std::fixed << std::setprecision(2) << e.durationMs; }
    if (e.opacity != 100)     { ss << ",\"opacity\":" << e.opacity; }
    if (e.hardness != 80)     { ss << ",\"hardness\":" << e.hardness; }
    if (!e.brushShape.empty()){ ss << ",\"shape\":\"" << e.brushShape << "\""; }
    if (!e.fileName.empty())  { ss << ",\"file\":"; writeString(ss, e.fileName); }
    if (!e.layerName.empty()) { ss << ",\"layerName\":"; writeString(ss, e.layerName); }
    if (!e.modifiers.empty()) { ss << ",\"mods\":\"" << e.modifiers << "\""; }
    if (e.sessionId > 0)      { ss << ",\"sid\":" << e.sessionId; }

    ss << "}\n";
}

Tracer& Tracer::instance() {
    static Tracer inst;
    return inst;
}

Tracer& tracer() {
    return Tracer::instance();
}

Tracer::Tracer() {}

Tracer::~Tracer() {
    shutdown();
}

void Tracer::startup() {
    auto& t = instance();

    const char* env = qgetenv("FAP_TRACE");
    if (env && (qstrcmp(env, "1") == 0 || qstrcmp(env, "true") == 0 || qstrcmp(env, "on") == 0)) {
        t.enabled_ = true;
    }

    if (!t.enabled_) return;

    t.startSession();

    t.running_ = true;
    t.writerThread_ = std::make_unique<std::thread>(&Tracer::writeThreadFunc, &t);

    if (QApplication::instance()) {
        QApplication::instance()->installEventFilter(&t);
    }

    TraceEvent e;
    e.category = EventCategory::App;
    e.event = "session_start";
    e.sessionId = t.sessionId_;
    t.record(e);
}

void Tracer::shutdown() {
    auto& t = instance();
    if (!t.enabled_) return;

    TraceEvent e;
    e.category = EventCategory::App;
    e.event = "session_end";
    e.sessionId = t.sessionId_;
    t.record(e);

    t.running_ = false;
    t.queueCond_.wakeAll();

    if (t.writerThread_ && t.writerThread_->joinable()) {
        t.writerThread_->join();
    }

    if (QApplication::instance()) {
        QApplication::instance()->removeEventFilter(&t);
    }

    {
        QMutexLocker lock(&t.fileMutex_);
        if (t.traceFile_.isOpen()) {
            t.traceFile_.flush();
            t.traceFile_.close();
        }
    }
}

void Tracer::startSession() {
    static int globalSessionCounter = 0;
    sessionId_ = ++globalSessionCounter;

    QString tracesDir = QDir::currentPath() + "/fap_traces";
    QDir().mkpath(tracesDir);
    traceDir_ = tracesDir;

    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    QString fileName = QString("trace_%1_s%2.jsonl").arg(timestamp).arg(sessionId_);
    QString filePath = traceDir_ + "/" + fileName;

    QMutexLocker lock(&fileMutex_);
    traceFile_.setFileName(filePath);
    traceFile_.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate);

    qInfo() << "Diagnostic Tracer session" << sessionId_ << "->" << filePath;
}

QString Tracer::currentTracePath() const {
    QMutexLocker lock(const_cast<QMutex*>(&fileMutex_));
    return traceFile_.fileName();
}

void Tracer::record(EventCategory cat, const std::string& event) {
    if (!enabled_) return;
    TraceEvent e;
    e.category = cat;
    e.event = event;
    record(e);
}

void Tracer::record(TraceEvent evt) {
    if (!enabled_) return;

    evt.sequence = ++eventSequence_;
    evt.timestamp = static_cast<double>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    ) / 1'000'000.0;
    evt.sessionId = sessionId_;

    std::ostringstream ss;
    writeEventJson(ss, evt);

    {
        QMutexLocker lock(&queueMutex_);
        writeQueue_.push(ss.str());
    }
    queueCond_.wakeOne();
}

void Tracer::writeThreadFunc() {
    while (running_) {
        std::string line;
        {
            QMutexLocker lock(&queueMutex_);
            if (writeQueue_.empty()) {
                queueCond_.wait(&queueMutex_, 100);
                continue;
            }
            line = std::move(writeQueue_.front());
            writeQueue_.pop();
        }

        QMutexLocker lock(&fileMutex_);
        if (traceFile_.isOpen()) {
            traceFile_.write(line.data(), static_cast<qint64>(line.size()));
            traceFile_.flush();
        }
    }

    flushQueue();
}

void Tracer::flushQueue() {
    QMutexLocker lock(&fileMutex_);
    if (!traceFile_.isOpen()) return;

    QMutexLocker qlock(&queueMutex_);
    while (!writeQueue_.empty()) {
        const auto& line = writeQueue_.front();
        traceFile_.write(line.data(), static_cast<qint64>(line.size()));
        writeQueue_.pop();
    }
    traceFile_.flush();
}

bool Tracer::eventFilter(QObject* obj, QEvent* event) {
    if (!enabled_) return QObject::eventFilter(obj, event);

    TraceEvent e;

    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease: {
        auto* me = static_cast<QMouseEvent*>(event);
        e.category = EventCategory::Mouse;
        e.event = (event->type() == QEvent::MouseButtonPress) ? "press" : "release";
        e.position.x = static_cast<double>(me->position().x());
        e.position.y = static_cast<double>(me->position().y());
        e.button = static_cast<int>(me->button());
        record(e);
        break;
    }
    case QEvent::MouseMove: {
        auto* me = static_cast<QMouseEvent*>(event);
        if (!(me->buttons() & (Qt::LeftButton | Qt::RightButton | Qt::MiddleButton))) break;
        e.category = EventCategory::Mouse;
        e.event = "move";
        e.position.x = static_cast<double>(me->position().x());
        e.position.y = static_cast<double>(me->position().y());
        e.button = static_cast<int>(me->buttons());
        record(e);
        break;
    }
    case QEvent::Wheel: {
        auto* we = static_cast<QWheelEvent*>(event);
        e.category = EventCategory::Mouse;
        e.event = "wheel";
        e.position.x = static_cast<double>(we->position().x());
        e.position.y = static_cast<double>(we->position().y());
        e.button = we->angleDelta().y();
        record(e);
        break;
    }
    case QEvent::KeyPress: {
        auto* ke = static_cast<QKeyEvent*>(event);
        e.category = EventCategory::Keyboard;
        e.event = "key_press";
        e.key = ke->key();
        if (ke->modifiers() & Qt::ControlModifier)  e.modifiers += "Ctrl+";
        if (ke->modifiers() & Qt::ShiftModifier)    e.modifiers += "Shift+";
        if (ke->modifiers() & Qt::AltModifier)      e.modifiers += "Alt+";
        if (!e.modifiers.empty()) e.modifiers.pop_back();
        record(e);
        break;
    }
    default:
        break;
    }

    return QObject::eventFilter(obj, event);
}

} // namespace fap::diagnostic

#endif
