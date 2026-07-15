#pragma once

#include <QImage>
#include <QString>
#include <functional>

namespace fap {

class Document;

struct ExportOptions {
    int outputWidth = 0;   // 0 = use canvas width
    int outputHeight = 0;  // 0 = use canvas height
    bool transparentBg = false;
};

QImage renderExportFrame(const Document& doc, int frameIndex,
                          const ExportOptions& opts = {});

// Convenience overload — kept for backward compatibility
inline QImage renderExportFrame(const Document& doc, int frameIndex,
                                bool transparentBg) {
    return renderExportFrame(doc, frameIndex, ExportOptions{0, 0, transparentBg});
}

bool exportVideo(const Document& doc, const QString& outputPath, int fps,
                 const ExportOptions& opts = {});

bool exportGIF(const Document& doc, const QString& outputPath, int fps,
               const ExportOptions& opts = {});

bool exportPNGSequence(const Document& doc, const QString& outputDir,
                       const ExportOptions& opts = {});

} // namespace fap
