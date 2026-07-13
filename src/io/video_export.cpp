#include "core/document.hpp"
#include "core/layer.hpp"
#include "core/types.hpp"
#include "video_export.hpp"

#include <QDir>
#include <QImage>
#include <QPainter>
#include <QProcess>
#include <QTemporaryDir>
#include <QString>

#include <cstdio>

namespace fap {

namespace {

static QPainter::CompositionMode toQtCompositionMode(BlendMode mode) {
    switch (mode) {
    case BlendMode::Normal:     return QPainter::CompositionMode_SourceOver;
    case BlendMode::Multiply:   return QPainter::CompositionMode_Multiply;
    case BlendMode::Screen:     return QPainter::CompositionMode_Screen;
    case BlendMode::Overlay:    return QPainter::CompositionMode_Overlay;
    case BlendMode::Add:        return QPainter::CompositionMode_Plus;
    case BlendMode::Subtract:   return QPainter::CompositionMode_Exclusion;
    case BlendMode::Darken:     return QPainter::CompositionMode_Darken;
    case BlendMode::Lighten:    return QPainter::CompositionMode_Lighten;
    case BlendMode::ColorBurn:  return QPainter::CompositionMode_ColorBurn;
    case BlendMode::ColorDodge: return QPainter::CompositionMode_ColorDodge;
    case BlendMode::SoftLight:  return QPainter::CompositionMode_SoftLight;
    case BlendMode::HardLight:  return QPainter::CompositionMode_HardLight;
    }
    return QPainter::CompositionMode_SourceOver;
}

} // anonymous namespace

QImage renderExportFrame(const Document& doc, int frameIndex, bool transparentBg) {
    int w = doc.width();
    int h = doc.height();
    QImage image(w, h, QImage::Format_ARGB32_Premultiplied);
    image.fill(transparentBg ? Qt::transparent : Qt::white);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

    for (size_t si = doc.sequenceCount(); si > 0; --si) {
        size_t idx = si - 1;
        if (!doc.sequenceAt(idx).visible()) continue;
        float seqOpacity = doc.sequenceAt(idx).opacity();
        const auto& root = doc.sequenceAt(idx).rootLayerForFrame(frameIndex);
        for (size_t li = root.layerCount(); li > 0; --li) {
            const Layer* layer = root.layerAt(li - 1);
            if (!layer || !layer->visible()) continue;
            if (layer->type() != LayerType::Raster) continue;

            const auto& rl = static_cast<const RasterLayer&>(*layer);
            QImage img(reinterpret_cast<const uchar*>(rl.pixelData()),
                       rl.width(), rl.height(),
                       rl.width() * static_cast<int>(sizeof(uint32_t)),
                       QImage::Format_ARGB32_Premultiplied);
            QImage display = img.convertToFormat(QImage::Format_ARGB32);

            painter.setOpacity(static_cast<qreal>(layer->opacity() * seqOpacity));
            painter.setCompositionMode(toQtCompositionMode(layer->blendMode()));
            painter.drawImage(QPoint(rl.originX(), rl.originY()), display);
        }
    }

    painter.end();
    return image;
}

namespace {

static bool ffmpegAvailable() {
    QProcess proc;
    proc.setProcessChannelMode(QProcess::ForwardedChannels);
    proc.start("ffmpeg", QStringList() << "-version");
    return proc.waitForFinished(3000) && proc.exitCode() == 0;
}

static bool executeFFmpeg(const QStringList& args) {
    QProcess proc;
    proc.start("ffmpeg", args);
    if (!proc.waitForStarted(5000)) {
        qWarning("exportVideo: failed to start ffmpeg");
        return false;
    }
    if (!proc.waitForFinished(120000)) {
        qWarning("exportVideo: ffmpeg timed out after 120s");
        proc.kill();
        return false;
    }
    if (proc.exitCode() != 0) {
        qWarning("exportVideo: ffmpeg exited with code %d", proc.exitCode());
        return false;
    }
    return true;
}

} // anonymous namespace

bool exportVideo(const Document& doc,
                 const QString& outputPath,
                 const QString& format,
                 int fps) {
    if (!ffmpegAvailable()) {
        qWarning("exportVideo: ffmpeg not found in PATH");
        return false;
    }

    int totalFrames = doc.totalFrames();
    if (totalFrames <= 0) {
        qWarning("exportVideo: document has no frames");
        return false;
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        qWarning("exportVideo: cannot create temp directory");
        return false;
    }

    QString pattern = tempDir.path() + "/frame_%04d.png";

    for (int f = 0; f < totalFrames; ++f) {
        QImage img = renderExportFrame(doc, f);
        QString framePath = tempDir.path() +
                            QString("/frame_%1.png")
                                .arg(f, 4, 10, QLatin1Char('0'));
        if (!img.save(framePath, "PNG")) {
            qWarning("exportVideo: failed to save frame %d", f);
            return false;
        }
    }

    QString ext = format;
    if (!ext.startsWith('.')) {
        ext.prepend('.');
    }

    QString outPath = outputPath;
    if (!outPath.endsWith(ext, Qt::CaseInsensitive)) {
        outPath += ext;
    }

    QStringList args;
    args << "-y"
         << "-framerate" << QString::number(fps)
         << "-i" << pattern
         << "-c:v" << "libx264"
         << "-pix_fmt" << "yuv420p"
         << "-vf" << "pad=ceil(iw/2)*2:ceil(ih/2)*2"
         << outPath;

    qInfo("exportVideo: running ffmpeg...");
    return executeFFmpeg(args);
}

bool exportGIF(const Document& doc,
               const QString& outputPath,
               int fps) {
    if (!ffmpegAvailable()) {
        qWarning("exportGIF: ffmpeg not found in PATH");
        return false;
    }

    int totalFrames = doc.totalFrames();
    if (totalFrames <= 0) {
        qWarning("exportGIF: document has no frames");
        return false;
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        qWarning("exportGIF: cannot create temp directory");
        return false;
    }

    QString pattern = tempDir.path() + "/frame_%04d.png";

    for (int f = 0; f < totalFrames; ++f) {
        QImage img = renderExportFrame(doc, f);
        QString framePath = tempDir.path() +
                            QString("/frame_%1.png")
                                .arg(f, 4, 10, QLatin1Char('0'));
        if (!img.save(framePath, "PNG")) {
            qWarning("exportGIF: failed to save frame %d", f);
            return false;
        }
    }

    QString outPath = outputPath;
    if (!outPath.endsWith(".gif", Qt::CaseInsensitive)) {
        outPath += ".gif";
    }

    QString paletteFile = tempDir.path() + "/palette.png";

    QStringList paletteArgs;
    paletteArgs << "-y"
                << "-framerate" << QString::number(fps)
                << "-i" << pattern
                << "-vf"
                << "fps=" + QString::number(fps) +
                       ",scale=320:-1:flags=lanczos,palettegen=stats_mode=diff"
                << paletteFile;

    if (!executeFFmpeg(paletteArgs)) {
        return false;
    }

    QStringList gifArgs;
    gifArgs << "-y"
            << "-framerate" << QString::number(fps)
            << "-i" << pattern
            << "-i" << paletteFile
            << "-lavfi"
            << "fps=" + QString::number(fps) +
                   ",scale=320:-1:flags=lanczos[x];[x][1:v]paletteuse=dither=bayer:bayer_scale=5"
            << outPath;

    qInfo("exportGIF: running ffmpeg...");
    return executeFFmpeg(gifArgs);
}

bool exportPNGSequence(const Document& doc, const QString& outputDir) {
    QDir dir(outputDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qWarning("exportPNGSequence: cannot create directory: %s",
                     qPrintable(outputDir));
            return false;
        }
    }

    int totalFrames = doc.totalFrames();
    if (totalFrames <= 0) {
        qWarning("exportPNGSequence: document has no frames");
        return false;
    }

    for (int f = 0; f < totalFrames; ++f) {
        QImage img = renderExportFrame(doc, f);
        QString framePath = dir.filePath(
            QString("frame_%1.png").arg(f, 4, 10, QLatin1Char('0')));
        if (!img.save(framePath, "PNG")) {
            qWarning("exportPNGSequence: failed to save frame %d to %s",
                     f, qPrintable(framePath));
            return false;
        }
    }

    qInfo("exportPNGSequence: exported %d frames to %s",
          totalFrames, qPrintable(outputDir));
    return true;
}

} // namespace fap
