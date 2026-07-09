#include <QtWidgets/QApplication>
#include <QtCore/QCommandLineParser>
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtGui/QFontDatabase>
#include <QMutex>
#include "ui_v2/main_window_v2.hpp"
#include "core/app_state.hpp"

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#endif

static QFile g_logFile;

void logHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg) {
    if (!g_logFile.isOpen()) return;
    QTextStream ts(&g_logFile);
    QString level;
    switch (type) {
    case QtDebugMsg:    level = "DEBUG"; break;
    case QtInfoMsg:     level = "INFO "; break;
    case QtWarningMsg:  level = "WARN "; break;
    case QtCriticalMsg: level = "CRIT "; break;
    case QtFatalMsg:    level = "FATAL"; break;
    }
    QString tsStr = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString location;
    if (ctx.file) location = QString("%1:%2").arg(ctx.file).arg(ctx.line);
    ts << "[" << tsStr << "] [" << level << "] " << msg;
    if (!location.isEmpty()) ts << "  (" << location << ")";
    ts << "\n";
    ts.flush();
}

void globalLogHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg) {
    static QMutex mutex;
    QMutexLocker locker(&mutex);

    QString timeStr = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    QString levelStr;
    switch (type) {
        case QtDebugMsg:    levelStr = "[DEBUG]"; break;
        case QtInfoMsg:     levelStr = "[INFO ]"; break;
        case QtWarningMsg:  levelStr = "[WARN ]"; break;
        case QtCriticalMsg: levelStr = "[CRIT ]"; break;
        case QtFatalMsg:    levelStr = "[FATAL]"; break;
    }

    QString contextStr = QString("%1:%2").arg(context.file ? context.file : "unknown").arg(context.line);
    QString logLine = QString("[%1] %2 <%3> %4\n").arg(timeStr, levelStr, contextStr, msg);

    fprintf(stderr, "%s", logLine.toLocal8Bit().constData());

    QFile outFile(QDir::currentPath() + "/debug.log");
    if (outFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&outFile);
        ts << logLine;
    }
}

#ifdef _WIN32
static LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS* info) {
    QString crashPath = QDir::currentPath() + "/crash.log";
    QFile f(crashPath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream ts(&f);
        ts << "CRASH: Exception 0x" << Qt::hex << info->ExceptionRecord->ExceptionCode << "\n";
        ts << "Address: 0x" << Qt::hex << reinterpret_cast<uintptr_t>(info->ExceptionRecord->ExceptionAddress) << "\n";
        ts << Qt::dec;
        void* stack[64];
        USHORT frames = CaptureStackBackTrace(0, 64, stack, nullptr);
        ts << "Stack frames: " << frames << "\n";
        HANDLE process = GetCurrentProcess();
        SymInitialize(process, nullptr, TRUE);
        SYMBOL_INFO* sym = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + 256, 1);
        if (sym) {
            sym->SizeOfStruct = sizeof(SYMBOL_INFO);
            sym->MaxNameLen = 255;
            for (USHORT i = 0; i < frames; ++i) {
                DWORD64 addr = reinterpret_cast<DWORD64>(stack[i]);
                DWORD64 disp = 0;
                if (SymFromAddr(process, addr, &disp, sym)) {
                    ts << "  [" << i << "] " << sym->Name << " + 0x" << Qt::hex << disp << Qt::dec << "\n";
                } else {
                    ts << "  [" << i << "] 0x" << Qt::hex << addr << Qt::dec << "\n";
                }
            }
            free(sym);
        }
        SymCleanup(process);
        f.close();
    }
    if (g_logFile.isOpen()) g_logFile.close();
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

int main(int argc, char* argv[]) {
    qInstallMessageHandler(globalLogHandler);

    QApplication app(argc, argv);

    int fontId1 = QFontDatabase::addApplicationFont(":/fonts/Avenir-Roman.ttf");
    int fontId2 = QFontDatabase::addApplicationFont(":/fonts/Avenir-Heavy.ttf");

    if (fontId1 < 0 || fontId2 < 0) {
        qWarning() << "Failed to load bundled Avenir fonts";
    }

    app.setApplicationName("Free Animation Power");
    app.setApplicationVersion("2.0.0");
    app.setOrganizationName("FAP");

#ifdef _WIN32
    SetUnhandledExceptionFilter(unhandledExceptionFilter);
#endif

    QString logPath = QDir::currentPath() + "/debug.log";
    g_logFile.setFileName(logPath);
    g_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);

    qInfo() << "==============================";
    qInfo() << "Free Animation Power v2.0.0 started";
    qInfo() << "Log:" << logPath;

    QCommandLineParser parser;
    parser.setApplicationDescription("2D Animation Studio — Hybrid Vector + Raster Engine");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"project", "Open project file", "file.fap"});
    parser.process(app);

    auto appState = std::make_shared<fap::AppState>();
    fap::MainWindowV2 window(appState);
    window.resize(1600, 900);
    window.show();
    qInfo() << "MainWindow shown";

    if (parser.isSet("project")) {
        window.openProject(parser.value("project"));
    }

    int result = app.exec();
    qInfo() << "Application exiting with code" << result;
    g_logFile.close();
    return result;
}
