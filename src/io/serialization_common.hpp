#pragma once

#include <QString>
#include "core/types.hpp"

namespace fap {
namespace io {
namespace utils {

QString layerTypeToString(LayerType t);
LayerType layerTypeFromString(const QString& s);
QString blendModeToString(BlendMode m);
BlendMode blendModeFromString(const QString& s);

} // namespace utils
} // namespace io
} // namespace fap
