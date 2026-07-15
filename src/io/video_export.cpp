#include "core/document.hpp"
#include "core/layer.hpp"
#include "core/types.hpp"
#include "video_export.hpp"

#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QProcess>
#include <QTemporaryDir>
#include <QString>

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

QImage renderExportFrame(const Document& doc, int frameIndex, const ExportOptions& opts) {
    int w = doc.width();
    int h = doc.height();
    int outW = (opts.outputWidth > 0) ? opts.outputWidth : w;
    int outH = (opts.outputHeight > 0) ? opts.outputHeight : h;

    if (outW == w && outH == h) {
        QImage image(w, h, QImage::Format_ARGB32_Premultiplied);
        image.fill(opts.transparentBg ? Qt::transparent : Qt::white);

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

    // Render at canvas size first, then scale
    QImage fullRes = renderExportFrame(doc, frameIndex,
                                       ExportOptions{0, 0, opts.transparentBg});
    return fullRes.scaled(outW, outH, Qt::IgnoreAspectRatio,
                          Qt::SmoothTransformation);
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

struct VideoFormat {
    const char* codec;
    const char* pixFmt;
    const char* extraArg = nullptr;
    const char* extraVal = nullptr;
    bool needsPad = false;
    bool transparentBg = false;
};

static VideoFormat detectVideoFormat(const QString& path) {
    if (path.endsWith(".mov", Qt::CaseInsensitive)) {
        return {"qtrle", "argb", nullptr, nullptr, false, true};
    }
    if (path.endsWith(".webm", Qt::CaseInsensitive)) {
        return {"libvpx-vp9", "yuva420p", "-lossless", "1", false, true};
    }
    return {"libx264", "yuv420p", nullptr, nullptr, true, false};
}

static const char* audioCodecForContainer(const VideoFormat& vf) {
    if (vf.transparentBg) {
        if (qstrcmp(vf.codec, "qtrle") == 0)     return "pcm_s16le";
        if (qstrcmp(vf.codec, "libvpx-vp9") == 0) return "libopus";
    }
    return "aac";
}

bool exportVideo(const Document& doc,
                 const QString& outputPath,
                 int fps,
                 const ExportOptions& opts) {
    if (!ffmpegAvailable()) {
        qWarning("exportVideo: ffmpeg not found in PATH");
        return false;
    }

    int totalFrames = doc.totalFrames();
    if (totalFrames <= 0) {
        qWarning("exportVideo: document has no frames");
        return false;
    }

    VideoFormat vf = detectVideoFormat(outputPath);

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        qWarning("exportVideo: cannot create temp directory");
        return false;
    }

    QString pattern = tempDir.path() + "/frame_%04d.png";

    for (int f = 0; f < totalFrames; ++f) {
        QImage img = renderExportFrame(doc, f,
            ExportOptions{opts.outputWidth, opts.outputHeight, vf.transparentBg});
        QString framePath = tempDir.path() +
                            QString("/frame_%1.png")
                                .arg(f + 1, 4, 10, QLatin1Char('0'));
        if (!img.save(framePath, "PNG")) {
            qWarning("exportVideo: failed to save frame %d", f);
            return false;
        }
    }

    // Collect non-muted audio tracks with valid files
    std::vector<QString> audioInputs;
    std::vector<float> audioVolumes;
    for (const auto& at : doc.audioTracks()) {
        if (at.muted) continue;
        QString path = QString::fromStdString(at.filepath);
        if (!QFileInfo::exists(path)) {
            qWarning("exportVideo: audio file not found: %s", qPrintable(path));
            continue;
        }
        audioInputs.push_back(path);
        audioVolumes.push_back(static_cast<float>(at.volume) / 100.0f);
    }

    QStringList args;
    args << "-y"
         << "-framerate" << QString::number(fps)
         << "-i" << pattern;

    for (const auto& audioPath : audioInputs) {
        args << "-i" << audioPath;
    }

    args << "-c:v" << vf.codec
         << "-pix_fmt" << vf.pixFmt;

    if (vf.extraArg && vf.extraVal) {
        args << vf.extraArg << vf.extraVal;
    }
    if (vf.needsPad) {
        args << "-vf" << "pad=ceil(iw/2)*2:ceil(ih/2)*2";
    }

    if (!audioInputs.empty()) {
        args << "-c:a" << audioCodecForContainer(vf);

        if (audioInputs.size() == 1) {
            if (audioVolumes[0] < 0.99f || audioVolumes[0] > 1.01f) {
                args << "-filter:a:0"
                     << QString("volume=%1").arg(static_cast<double>(audioVolumes[0]));
            }
            args << "-map" << "0:v:0"
                 << "-map" << "1:a:0";
        } else {
            // Build: [1:a]volume=V1[a0];[2:a]volume=V2[a1];[a0][a1]amix=inputs=N:...[a]
            QStringList filterParts;
            for (size_t i = 0; i < audioInputs.size(); ++i) {
                int inputIdx = static_cast<int>(i) + 1;
                filterParts.append(QString("[%1:a]volume=%2[a%3]")
                    .arg(inputIdx)
                    .arg(static_cast<double>(audioVolumes[i]))
                    .arg(i));
            }
            QString mixInputs;
            for (size_t i = 0; i < audioInputs.size(); ++i) {
                mixInputs += QString("[a%1]").arg(i);
            }
            filterParts.append(QString("%1amix=inputs=%2:duration=longest:dropout_transition=0[a]")
                .arg(mixInputs)
                .arg(audioInputs.size()));

            args << "-filter_complex" << filterParts.join(';');
            args << "-map" << "0:v:0"
                 << "-map" << "[a]";
        }
        args << "-shortest";
    }

    args << outputPath;

    qInfo("exportVideo: running ffmpeg...");
    return executeFFmpeg(args);
}

bool exportGIF(const Document& doc,
               const QString& outputPath,
               int fps,
               const ExportOptions& opts) {
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
        QImage img = renderExportFrame(doc, f, opts);
        QString framePath = tempDir.path() +
                            QString("/frame_%1.png")
                                .arg(f + 1, 4, 10, QLatin1Char('0'));
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
                       ",palettegen=stats_mode=diff"
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
                   "[x];[x][1:v]paletteuse=dither=bayer:bayer_scale=5"
            << outPath;

    qInfo("exportGIF: running ffmpeg...");
    return executeFFmpeg(gifArgs);
}

bool exportPNGSequence(const Document& doc, const QString& outputDir,
                       const ExportOptions& opts) {
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
        QImage img = renderExportFrame(doc, f, opts);
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
