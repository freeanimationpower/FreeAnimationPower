#pragma once

#include <QImage>
#include <QString>
#include <functional>

namespace fap {

class Document;

QImage renderExportFrame(const Document& doc, int frameIndex,
                          bool transparentBg = false);

bool exportVideo(const Document& doc, const QString& outputPath, int fps);

bool exportGIF(const Document& doc, const QString& outputPath, int fps);

bool exportPNGSequence(const Document& doc, const QString& outputDir);

} // namespace fap
