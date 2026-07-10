#pragma once

#include "tracer_types.hpp"
#include <QObject>
#include <QString>
#include <QFile>
#include <QMutex>
#include <QWaitCondition>
#include <atomic>
#include <thread>
#include <queue>
#include <memory>

#ifdef FAP_DIAGNOSTIC_TRACER

namespace fap::diagnostic {

class Tracer : public QObject {
    Q_OBJECT

public:
    static Tracer& instance();
    static void startup();
    static void shutdown();

    bool enabled() const { return enabled_; }
    void setEnabled(bool e) { enabled_ = e; }

    int sessionId() const { return sessionId_; }

    void record(EventCategory cat, const std::string& event);
    void record(TraceEvent evt);

    QString currentTracePath() const;

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    Tracer();
    ~Tracer() override;

    void startSession();
    void writeThreadFunc();
    void flushQueue();

    std::atomic<bool> enabled_{false};
    std::atomic<bool> running_{false};
    int sessionId_ = 0;

    QString traceDir_;
    QFile traceFile_;
    QMutex queueMutex_;
    std::queue<std::string> writeQueue_;
    std::unique_ptr<std::thread> writerThread_;
    QWaitCondition queueCond_;

    uint64_t eventSequence_{0};
    QMutex fileMutex_;
};

Tracer& tracer();

} // namespace fap::diagnostic

#endif
