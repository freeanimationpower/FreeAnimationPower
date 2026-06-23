#include "io/serialization_common.hpp"

namespace fap {
namespace io {
namespace utils {

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
    if (s == QStringLiteral("raster")) return LayerType::Raster;
    if (s == QStringLiteral("vector")) return LayerType::Vector;
    if (s == QStringLiteral("group"))  return LayerType::Group;
    if (s == QStringLiteral("audio"))  return LayerType::Audio;
    if (s == QStringLiteral("camera")) return LayerType::Camera;
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
    if (s == QStringLiteral("multiply"))    return BlendMode::Multiply;
    if (s == QStringLiteral("screen"))      return BlendMode::Screen;
    if (s == QStringLiteral("overlay"))     return BlendMode::Overlay;
    if (s == QStringLiteral("add"))         return BlendMode::Add;
    if (s == QStringLiteral("subtract"))    return BlendMode::Subtract;
    if (s == QStringLiteral("darken"))      return BlendMode::Darken;
    if (s == QStringLiteral("lighten"))     return BlendMode::Lighten;
    if (s == QStringLiteral("color_burn"))  return BlendMode::ColorBurn;
    if (s == QStringLiteral("color_dodge")) return BlendMode::ColorDodge;
    if (s == QStringLiteral("soft_light"))  return BlendMode::SoftLight;
    if (s == QStringLiteral("hard_light"))  return BlendMode::HardLight;
    return BlendMode::Normal;
}

} // namespace utils
} // namespace io
} // namespace fap
