#include "svg_export.hpp"

#include "core/document.hpp"
#include "core/sequence.hpp"
#include "core/layer.hpp"
#include "core/types.hpp"
#include "engine/vector/bezier_path.hpp"

#include <QFile>
#include <QTextStream>
#include <QBuffer>
#include <QImage>
#include <QDir>
#include <QDebug>

namespace fap {

static QString colorToSVG(const Color& c) {
    int r = static_cast<int>(std::clamp(c.r, 0.0f, 1.0f) * 255.0f);
    int g = static_cast<int>(std::clamp(c.g, 0.0f, 1.0f) * 255.0f);
    int b = static_cast<int>(std::clamp(c.b, 0.0f, 1.0f) * 255.0f);
    return QString("#%1%2%3")
        .arg(r, 2, 16, QChar('0'))
        .arg(g, 2, 16, QChar('0'))
        .arg(b, 2, 16, QChar('0'));
}

static void rasterToBase64PNG(const RasterLayer& rl, QString& out) {
    QImage img(reinterpret_cast<const uchar*>(rl.pixelData()),
               rl.width(), rl.height(),
               rl.width() * static_cast<int>(sizeof(uint32_t)),
               QImage::Format_ARGB32_Premultiplied);

    QImage converted = img.convertToFormat(QImage::Format_ARGB32);

    QByteArray pngData;
    QBuffer buffer(&pngData);
    buffer.open(QIODevice::WriteOnly);
    converted.save(&buffer, "PNG");
    buffer.close();

    out = QString::fromLatin1(pngData.toBase64());
}

static void writeVectorStroke(QTextStream& s, const VectorStroke& stroke) {
    const auto& segs = stroke.path.segments();
    if (segs.empty()) return;

    s << "    <path d=\"";
    s << "M " << segs[0].p0.x << "," << segs[0].p0.y;

    for (const auto& seg : segs) {
        s << " C " << seg.p1.x << "," << seg.p1.y
          << " "  << seg.p2.x << "," << seg.p2.y
          << " "  << seg.p3.x << "," << seg.p3.y;
    }

    if (segs.size() >= 2) {
        auto& first = segs.front();
        auto& last  = segs.back();
        float dx = last.p3.x - first.p0.x;
        float dy = last.p3.y - first.p0.y;
        if (std::abs(dx) < 0.5f && std::abs(dy) < 0.5f) {
            s << " Z";
        }
    }

    s << "\" ";
    s << "fill=\"none\" ";
    s << "stroke=\"" << colorToSVG(stroke.color) << "\" ";
    s << "stroke-width=\"" << stroke.width << "\" ";

    if (stroke.opacity < 1.0f) {
        s << "stroke-opacity=\"" << stroke.opacity << "\" ";
    }

    s << "/>\n";
}

static void writeLayer(QTextStream& s, const Layer* layer);

static void writeGroupLayer(QTextStream& s, const GroupLayer* group) {
    if (!group) return;
    s << QString("    <g opacity=\"%1\">\n").arg(group->opacity());

    for (size_t i = 0; i < group->layerCount(); ++i) {
        writeLayer(s, group->layerAt(static_cast<int>(i)));
    }

    s << "    </g>\n";
}

static void writeRasterLayer(QTextStream& s, const RasterLayer* rl) {
    if (!rl->hasContent()) return;

    QString b64;
    rasterToBase64PNG(*rl, b64);
    if (b64.isEmpty()) return;

    s << QString("    <image x=\"%1\" y=\"%2\" width=\"%3\" height=\"%4\"\n")
        .arg(rl->originX()).arg(rl->originY())
        .arg(rl->width()).arg(rl->height());
    s << QString("           href=\"data:image/png;base64,%1\"\n").arg(b64);

    if (rl->opacity() < 1.0f) {
        s << QString("           opacity=\"%1\"\n").arg(rl->opacity());
    }
    s << "    />\n";
}

static void writeVectorLayer(QTextStream& s, const VectorLayer* vl) {
    const auto& strokes = vl->strokes();
    if (strokes.empty()) return;

    s << QString("    <g opacity=\"%1\">\n").arg(vl->opacity());
    for (const auto& stroke : strokes) {
        writeVectorStroke(s, stroke);
    }
    s << "    </g>\n";
}

static void writeLayer(QTextStream& s, const Layer* layer) {
    if (!layer || !layer->visible()) return;

    switch (layer->type()) {
    case LayerType::Group:
        writeGroupLayer(s, static_cast<const GroupLayer*>(layer));
        break;
    case LayerType::Raster:
        writeRasterLayer(s, static_cast<const RasterLayer*>(layer));
        break;
    case LayerType::Vector:
        writeVectorLayer(s, static_cast<const VectorLayer*>(layer));
        break;
    }
}

static QString buildSVGFrame(const Document& doc, int frameIndex) {
    QString svg;
    QTextStream s(&svg);

    s << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    s << QString("<svg xmlns=\"http://www.w3.org/2000/svg\" "
                  "xmlns:xlink=\"http://www.w3.org/1999/xlink\" "
                  "width=\"%1\" height=\"%2\" "
                  "viewBox=\"0 0 %1 %2\">\n")
        .arg(doc.width()).arg(doc.height());

    for (size_t si = doc.sequenceCount(); si > 0; --si) {
        const auto& seq = doc.sequenceAt(si - 1);
        if (!seq.visible()) continue;

        const auto* root = seq.peekRootLayerForFrame(frameIndex);
        if (!root) continue;

        s << QString("  <g opacity=\"%1\">\n").arg(seq.opacity());
        for (size_t li = 0; li < root->layerCount(); ++li) {
            writeLayer(s, root->layerAt(static_cast<int>(li)));
        }
        s << "  </g>\n";
    }

    s << "</svg>\n";
    return svg;
}

bool exportSVGFrame(const Document& doc, int frameIndex, const QString& outputPath) {
    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "exportSVGFrame: cannot write" << outputPath;
        return false;
    }

    QString svg = buildSVGFrame(doc, frameIndex);
    file.write(svg.toUtf8());
    file.close();
    return true;
}

bool exportSVGFrames(const Document& doc, const QString& outputDir) {
    QDir dir(outputDir);
    if (!dir.exists() && !dir.mkpath(".")) {
        qWarning() << "exportSVGFrames: cannot create dir" << outputDir;
        return false;
    }

    int totalFrames = doc.totalFrames();
    int saved = 0;

    for (int f = 0; f < totalFrames; ++f) {
        QString path = dir.filePath(QString("frame_%1.svg")
            .arg(f, 4, 10, QChar('0')));
        if (exportSVGFrame(doc, f, path)) {
            ++saved;
        }
    }

    qDebug() << "exportSVGFrames: saved" << saved << "/" << totalFrames;
    return saved == totalFrames;
}

} // namespace fap
