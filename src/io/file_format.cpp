#include "core/document.hpp"
#include "core/layer.hpp"
#include "core/types.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <cstdio>

namespace fap {

namespace {

constexpr int kFormatVersion = 2;

QString layerTypeToString(LayerType t) {
    switch (t) {
        case LayerType::Raster: return QStringLiteral("raster");
        case LayerType::Vector: return QStringLiteral("vector");
        case LayerType::Group:  return QStringLiteral("group");
        case LayerType::Audio:  return QStringLiteral("audio");
        case LayerType::Camera: return QStringLiteral("camera");
    }
    return QStringLiteral("raster");
}

LayerType layerTypeFromString(const QString& s) {
    if (s == "raster") return LayerType::Raster;
    if (s == "vector") return LayerType::Vector;
    if (s == "group")  return LayerType::Group;
    if (s == "audio")  return LayerType::Audio;
    if (s == "camera") return LayerType::Camera;
    return LayerType::Raster;
}

QString blendModeToString(BlendMode m) {
    switch (m) {
        case BlendMode::Normal:     return QStringLiteral("normal");
        case BlendMode::Multiply:   return QStringLiteral("multiply");
        case BlendMode::Screen:     return QStringLiteral("screen");
        case BlendMode::Overlay:    return QStringLiteral("overlay");
        case BlendMode::Add:        return QStringLiteral("add");
        case BlendMode::Subtract:   return QStringLiteral("subtract");
        case BlendMode::Darken:     return QStringLiteral("darken");
        case BlendMode::Lighten:    return QStringLiteral("lighten");
        case BlendMode::ColorBurn:  return QStringLiteral("color_burn");
        case BlendMode::ColorDodge: return QStringLiteral("color_dodge");
        case BlendMode::SoftLight:  return QStringLiteral("soft_light");
        case BlendMode::HardLight:  return QStringLiteral("hard_light");
    }
    return QStringLiteral("normal");
}

BlendMode blendModeFromString(const QString& s) {
    if (s == "multiply")    return BlendMode::Multiply;
    if (s == "screen")      return BlendMode::Screen;
    if (s == "overlay")     return BlendMode::Overlay;
    if (s == "add")         return BlendMode::Add;
    if (s == "subtract")    return BlendMode::Subtract;
    if (s == "darken")      return BlendMode::Darken;
    if (s == "lighten")     return BlendMode::Lighten;
    if (s == "color_burn")  return BlendMode::ColorBurn;
    if (s == "color_dodge") return BlendMode::ColorDodge;
    if (s == "soft_light")  return BlendMode::SoftLight;
    if (s == "hard_light")  return BlendMode::HardLight;
    return BlendMode::Normal;
}

static QJsonObject layerToJson(const Layer& layer) {
    QJsonObject obj;
    obj["name"]       = QString::fromStdString(layer.name());
    obj["type"]       = layerTypeToString(layer.type());
    obj["visible"]    = layer.visible();
    obj["opacity"]    = static_cast<double>(layer.opacity());
    obj["blend_mode"] = blendModeToString(layer.blendMode());
    obj["locked"]     = layer.locked();

    if (layer.type() == LayerType::Raster) {
        const auto& rl = static_cast<const RasterLayer&>(layer);
        obj["width"]       = rl.width();
        obj["height"]      = rl.height();
        obj["origin_x"]    = rl.originX();
        obj["origin_y"]    = rl.originY();
        obj["has_content"] = rl.hasContent();
    }

    if (layer.type() == LayerType::Group) {
        const auto& gl = static_cast<const GroupLayer&>(layer);
        QJsonArray children;
        for (size_t i = 0; i < gl.layerCount(); ++i) {
            children.append(layerToJson(*gl.layers()[i]));
        }
        obj["children"] = children;
    }

    return obj;
}

static std::unique_ptr<Layer> layerFromJson(const QJsonObject& obj) {
    QString name      = obj["name"].toString("Layer");
    LayerType type    = layerTypeFromString(obj["type"].toString("raster"));
    bool visible      = obj["visible"].toBool(true);
    double opacity    = obj["opacity"].toDouble(1.0);
    BlendMode blend   = blendModeFromString(obj["blend_mode"].toString("normal"));
    bool locked       = obj["locked"].toBool(false);

    std::unique_ptr<Layer> layer;

    switch (type) {
        case LayerType::Raster: {
            int w = obj["width"].toInt(1920);
            int h = obj["height"].toInt(1080);
            layer = std::make_unique<RasterLayer>(name.toStdString(), w, h);
            auto* rl = static_cast<RasterLayer*>(layer.get());
            rl->setOrigin(obj["origin_x"].toInt(0), obj["origin_y"].toInt(0));
            rl->setHasContent(obj["has_content"].toBool(false));
            break;
        }
        case LayerType::Vector:
            layer = std::make_unique<VectorLayer>(name.toStdString());
            break;
        case LayerType::Group: {
            auto group = std::make_unique<GroupLayer>(name.toStdString());
            QJsonArray children = obj["children"].toArray();
            for (const auto& child : children) {
                auto childLayer = layerFromJson(child.toObject());
                if (childLayer) {
                    group->addLayer(std::move(childLayer));
                }
            }
            layer = std::move(group);
            break;
        }
        case LayerType::Audio:
        case LayerType::Camera:
            layer = std::make_unique<RasterLayer>(name.toStdString(), 1, 1);
            break;
    }

    if (layer) {
        layer->setVisible(visible);
        layer->setOpacity(static_cast<float>(opacity));
        layer->setBlendMode(blend);
        layer->setLocked(locked);
    }

    return layer;
}

static bool saveFrame(const QImage& image, const QString& path) {
    if (image.isNull()) {
        QImage blank(64, 64, QImage::Format_ARGB32_Premultiplied);
        blank.fill(Qt::transparent);
        return blank.save(path, "PNG");
    }
    return image.save(path, "PNG");
}

} // anonymous namespace

bool loadFAP(const QString& path, Document& doc) {
    QFileInfo fi(path);
    if (!fi.isDir()) {
        qWarning("loadFAP: path is not a directory: %s", qPrintable(path));
        return false;
    }

    QString jsonPath = fi.filePath() + "/document.json";
    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("loadFAP: cannot open document.json: %s", qPrintable(jsonPath));
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError err;
    QJsonDocument jdoc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning("loadFAP: JSON parse error: %s", qPrintable(err.errorString()));
        return false;
    }

    QJsonObject root = jdoc.object();
    int version = root["version"].toInt(1);

    doc.setCanvasSize(root["canvas_w"].toInt(1920), root["canvas_h"].toInt(1080));

    if (version >= 3) {
        QJsonArray seqArr = root["sequences"].toArray();
        for (int si = 0; si < seqArr.size(); ++si) {
            QJsonObject seqObj = seqArr[si].toObject();
            std::string sname = seqObj["name"].toString("Sequence").toStdString();
            int sfps = seqObj["fps"].toInt(24);
            int sframeCount = seqObj["total_frames"].toInt(1);
            auto& seq = (si == 0) ? doc.activeSequence() : doc.addSequence(sname);
            seq.setName(sname);
            seq.setFPS(sfps);
            seq.setTotalFrames(sframeCount);
            seq.setCurrentFrame(seqObj["current_frame"].toInt(0));
            seq.setVisible(seqObj["visible"].toBool(true));
            seq.setOpacity(static_cast<float>(seqObj["opacity"].toDouble(1.0)));
            seq.setLocked(seqObj["locked"].toBool(false));
            seq.setWorkAreaStart(seqObj["work_area_start"].toInt(0));
            seq.setWorkAreaEnd(seqObj["work_area_end"].toInt(0));
            seq.setDurationFrames(seqObj["duration_frames"].toInt(sframeCount));
            seq.setLooping(seqObj["looping"].toBool(false));

            QJsonArray framesArr = seqObj["frames"].toArray();
            for (const auto& frameVal : framesArr) {
                QJsonObject frameObj = frameVal.toObject();
                int frameIdx = frameObj["index"].toInt(0);
                auto& frameRoot = seq.rootLayerForFrame(frameIdx);
                while (frameRoot.layerCount() > 0)
                    frameRoot.removeLayer(0);
                QJsonArray layersArr = frameObj["layers"].toArray();
                int li = 0;
                for (const auto& layerVal : layersArr) {
                    auto layer = layerFromJson(layerVal.toObject());
                    if (layer) {
                        if (layer->type() == LayerType::Raster) {
                            auto* rl = static_cast<RasterLayer*>(layer.get());
                            QString pngName = QString("F%1_S%2_L%3.png")
                                .arg(frameIdx).arg(si).arg(li, 2, 10, QLatin1Char('0'));
                            QString pngPath = fi.filePath() + "/frames/" + pngName;
                            QImage png(pngPath);
                            if (!png.isNull()) {
                                png = png.convertToFormat(QImage::Format_ARGB32_Premultiplied);
                                int copyW = std::min(png.width(), rl->width());
                                int copyH = std::min(png.height(), rl->height());
                                for (int y = 0; y < copyH; ++y) {
                                    const uint32_t* src = reinterpret_cast<const uint32_t*>(png.constScanLine(y));
                                    uint32_t* dst = rl->pixelData() + static_cast<size_t>(y) * rl->width();
                                    if (src && dst) std::copy(src, src + copyW, dst);
                                }
                            }
                        }
                        frameRoot.addLayer(std::move(layer));
                    }
                    ++li;
                }
            }
        }
        int activeIdx = root["active_sequence"].toInt(0);
        doc.setActiveSequence(static_cast<size_t>(activeIdx));
    } else if (version >= 2) {
        auto& seq = doc.activeSequence();
        seq.setFPS(root["fps"].toInt(24));
        seq.setTotalFrames(root["total_frames"].toInt(1));
        QJsonArray framesArr = root["frames"].toArray();
        for (const auto& frameVal : framesArr) {
            QJsonObject frameObj = frameVal.toObject();
            int frameIdx = frameObj["index"].toInt(0);
            auto& frameRoot = seq.rootLayerForFrame(frameIdx);
            while (frameRoot.layerCount() > 0)
                frameRoot.removeLayer(0);
            QJsonArray layersArr = frameObj["layers"].toArray();
            int li = 0;
            for (const auto& layerVal : layersArr) {
                auto layer = layerFromJson(layerVal.toObject());
                if (layer) {
                    if (layer->type() == LayerType::Raster) {
                        auto* rl = static_cast<RasterLayer*>(layer.get());
                        QString pngName = QString("F%1_L%2.png")
                            .arg(frameIdx).arg(li, 2, 10, QLatin1Char('0'));
                        QString pngPath = fi.filePath() + "/frames/" + pngName;
                        QImage png(pngPath);
                        if (!png.isNull()) {
                            png = png.convertToFormat(QImage::Format_ARGB32_Premultiplied);
                            int copyW = std::min(png.width(), rl->width());
                            int copyH = std::min(png.height(), rl->height());
                            for (int y = 0; y < copyH; ++y) {
                                const uint32_t* src = reinterpret_cast<const uint32_t*>(png.constScanLine(y));
                                uint32_t* dst = rl->pixelData() + static_cast<size_t>(y) * rl->width();
                                if (src && dst) std::copy(src, src + copyW, dst);
                            }
                        }
                    }
                    frameRoot.addLayer(std::move(layer));
                }
                ++li;
            }
        }
    } else {
        QJsonArray layersArr = root["layers"].toArray();
        std::vector<std::unique_ptr<Layer>> templateLayers;
        for (const auto& layerVal : layersArr) {
            auto layer = layerFromJson(layerVal.toObject());
            if (layer) templateLayers.push_back(std::move(layer));
        }
        int fps = root["fps"].toInt(24);
        int total = root["total_frames"].toInt(1);
        auto& seq = doc.activeSequence();
        seq.setFPS(fps);
        seq.setTotalFrames(total);
        int w = doc.width(), h = doc.height();
        for (int f = 0; f < total; ++f) {
            auto& frameRoot = seq.rootLayerForFrame(f);
            while (frameRoot.layerCount() > 0)
                frameRoot.removeLayer(0);
            for (auto& tl : templateLayers) {
                auto clone = std::make_unique<RasterLayer>(tl->name(), w, h);
                clone->setVisible(tl->visible());
                clone->setOpacity(tl->opacity());
                clone->setBlendMode(tl->blendMode());
                clone->setLocked(tl->locked());
                QString frameName = QString("L%1_%2.png")
                    .arg(0).arg(f, 4, 10, QLatin1Char('0'));
                QString pngPath = fi.filePath() + "/frames/" + frameName;
                QImage png(pngPath);
                if (!png.isNull()) {
                    for (int y = 0; y < std::min(png.height(), h); ++y) {
                        const uint32_t* src = reinterpret_cast<const uint32_t*>(png.constScanLine(y));
                        if (src) {
                            uint32_t* dst = clone->pixelData() + static_cast<size_t>(y) * w;
                            std::copy(src, src + std::min(png.width(), w), dst);
                        }
                    }
                }
                frameRoot.addLayer(std::move(clone));
            }
        }
    }

    doc.setModified(false);
    return true;
}

bool saveFAP(const QString& path, const Document& doc) {
    QDir dir(path);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qWarning("saveFAP: cannot create directory: %s", qPrintable(path));
            return false;
        }
    }

    QJsonObject root;
    root["version"]          = 3;
    root["canvas_w"]         = doc.width();
    root["canvas_h"]         = doc.height();
    root["active_sequence"]  = static_cast<int>(doc.activeSequenceIndex());

    QString framesDirPath = dir.filePath("frames");
    QDir framesDir(framesDirPath);
    if (!framesDir.exists()) framesDir.mkpath(".");

    QJsonArray seqArr;
    for (size_t si = 0; si < doc.sequenceCount(); ++si) {
        const auto& seq = doc.sequenceAt(si);
        QJsonObject seqObj;
        seqObj["name"]            = QString::fromStdString(seq.name());
        seqObj["fps"]             = seq.fps();
        seqObj["total_frames"]    = seq.totalFrames();
        seqObj["current_frame"]   = seq.currentFrame();
        seqObj["visible"]         = seq.visible();
        seqObj["opacity"]         = static_cast<double>(seq.opacity());
        seqObj["locked"]          = seq.locked();
        seqObj["work_area_start"] = seq.workAreaStart();
        seqObj["work_area_end"]   = seq.workAreaEnd();
        seqObj["duration_frames"] = seq.durationFrames();
        seqObj["looping"]         = seq.looping();

        QJsonArray framesArr;
        int total = seq.totalFrames();

        for (int frame = 0; frame < total; ++frame) {
            const auto* root = seq.peekRootLayerForFrame(frame);
            if (!root) continue;

            QJsonObject frameObj;
            frameObj["index"] = frame;
            QJsonArray layersArr;
            for (size_t li = 0; li < root->layerCount(); ++li) {
                const Layer* layer = root->layers()[li].get();
                layersArr.append(layerToJson(*layer));
                if (layer->type() == LayerType::Raster) {
                    const auto& rl = static_cast<const RasterLayer&>(*layer);
                    QImage layerImage(reinterpret_cast<const uchar*>(rl.pixelData()),
                                      rl.width(), rl.height(),
                                      rl.width() * static_cast<int>(sizeof(uint32_t)),
                                      QImage::Format_ARGB32_Premultiplied);
                    QString frameName = QString("F%1_S%2_L%3.png")
                        .arg(frame).arg(si).arg(li, 2, 10, QLatin1Char('0'));
                    saveFrame(layerImage.copy(), framesDir.filePath(frameName));
                }
            }
            frameObj["layers"] = layersArr;
            framesArr.append(frameObj);
        }
        seqObj["frames"] = framesArr;
        seqArr.append(seqObj);
    }
    root["sequences"] = seqArr;

    QJsonDocument jdoc(root);
    QString jsonPath = dir.filePath("document.json");
    QFile file(jsonPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning("saveFAP: cannot write document.json: %s", qPrintable(jsonPath));
        return false;
    }
    file.write(jdoc.toJson(QJsonDocument::Indented));
    file.close();

    return true;
}

} // namespace fap
