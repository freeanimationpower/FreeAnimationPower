#pragma once

#include <QString>

namespace fap {

class Document;

bool exportSVGFrame(const Document& doc, int frameIndex, const QString& outputPath);
bool exportSVGFrames(const Document& doc, const QString& outputDir);

} // namespace fap
