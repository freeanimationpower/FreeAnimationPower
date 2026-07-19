#pragma once

#include <QtGui/QImage>
#include <cstdint>

namespace fap {

void applyLineBoil(QImage& image, int frame, float strength, int seed);

} // namespace fap
