#include "core/layer.hpp"
#include <atomic>
#include <algorithm>
#include <cstring>

namespace fap {

static std::atomic<LayerUid> g_nextLayerUid{1};

LayerUid generateLayerUid() {
    return g_nextLayerUid.fetch_add(1, std::memory_order_relaxed);
}

Layer::Layer(const std::string& name, LayerType type)
    : uid_(generateLayerUid()), name_(name), type_(type) {
}

RasterLayer::RasterLayer(const std::string& name, int width, int height)
    : Layer(name, LayerType::Raster)
    , width_(width)
    , height_(height)
    , pixelBuffer_(std::make_shared<PixelBuffer>()) {
    pixelBuffer_->pixels.assign(static_cast<size_t>(width) * static_cast<size_t>(height), 0);
}

void RasterLayer::ensureUnique() {
    if (pixelBuffer_.use_count() <= 1) return;
    auto newBuf = std::make_shared<PixelBuffer>(pixelBuffer_->pixels);
    pixelBuffer_ = std::move(newBuf);
}

std::unique_ptr<Layer> RasterLayer::clone() const {
    auto copy = std::make_unique<RasterLayer>(name_, width_, height_);
    copy->pixelBuffer_ = pixelBuffer_;
    copy->originX_ = originX_;
    copy->originY_ = originY_;
    copy->buffer_epoch_ = buffer_epoch_;
    copy->setVisible(visible_);
    copy->setOpacity(opacity_);
    copy->setBlendMode(blend_mode_);
    copy->setLocked(locked_);
    return copy;
}

size_t RasterLayer::indexAt(int x, int y) const {
    int bx = x - originX_;
    int by = y - originY_;
    if (bx < 0 || bx >= width_ || by < 0 || by >= height_) return static_cast<size_t>(-1);
    return static_cast<size_t>(by) * static_cast<size_t>(width_) + static_cast<size_t>(bx);
}

Color RasterLayer::pixelAt(int x, int y) const {
    size_t idx = indexAt(x, y);
    if (idx == static_cast<size_t>(-1)) return {};
    uint32_t p = pixelBuffer_->pixels[idx];
    uint8_t a = static_cast<uint8_t>((p >> 24) & 0xFF);
    if (a == 0) return Color::fromRGBA(0, 0, 0, 0);
    float invA = 255.0f / static_cast<float>(a);
    uint8_t r = static_cast<uint8_t>(std::min(255.0f, static_cast<float>((p >> 16) & 0xFF) * invA + 0.5f));
    uint8_t g = static_cast<uint8_t>(std::min(255.0f, static_cast<float>((p >> 8) & 0xFF) * invA + 0.5f));
    uint8_t b = static_cast<uint8_t>(std::min(255.0f, static_cast<float>(p & 0xFF) * invA + 0.5f));
    return Color::fromRGBA(r, g, b, a);
}

void RasterLayer::setPixel(int x, int y, const Color& color) {
    ensureUnique();
    size_t idx = indexAt(x, y);
    if (idx == static_cast<size_t>(-1)) return;
    float a = std::clamp(color.a, 0.0f, 1.0f);
    uint8_t r = static_cast<uint8_t>(std::clamp(color.r * a, 0.0f, 1.0f) * 255.0f);
    uint8_t g = static_cast<uint8_t>(std::clamp(color.g * a, 0.0f, 1.0f) * 255.0f);
    uint8_t b = static_cast<uint8_t>(std::clamp(color.b * a, 0.0f, 1.0f) * 255.0f);
    uint8_t alpha = static_cast<uint8_t>(a * 255.0f);
    pixelBuffer_->pixels[idx] = (static_cast<uint32_t>(b))
                              | (static_cast<uint32_t>(g) << 8)
                              | (static_cast<uint32_t>(r) << 16)
                              | (static_cast<uint32_t>(alpha) << 24);
    ++buffer_epoch_;
}

void RasterLayer::blendPixel(int x, int y, const Color& color) {
    ensureUnique();
    size_t idx = indexAt(x, y);
    if (idx == static_cast<size_t>(-1)) return;

    uint8_t sr = static_cast<uint8_t>(std::clamp(color.r, 0.0f, 1.0f) * 255.0f);
    uint8_t sg = static_cast<uint8_t>(std::clamp(color.g, 0.0f, 1.0f) * 255.0f);
    uint8_t sb = static_cast<uint8_t>(std::clamp(color.b, 0.0f, 1.0f) * 255.0f);
    uint8_t sa = static_cast<uint8_t>(std::clamp(color.a, 0.0f, 1.0f) * 255.0f);

    float srcA = static_cast<float>(sa) / 255.0f;
    float invSrcA = 1.0f - srcA;

    uint32_t dst = pixelBuffer_->pixels[idx];
    uint8_t db = static_cast<uint8_t>(dst & 0xFF);
    uint8_t dg = static_cast<uint8_t>((dst >> 8) & 0xFF);
    uint8_t dr = static_cast<uint8_t>((dst >> 16) & 0xFF);
    uint8_t da = static_cast<uint8_t>((dst >> 24) & 0xFF);

    uint8_t outB = static_cast<uint8_t>(static_cast<float>(sb) * srcA + static_cast<float>(db) * invSrcA + 0.5f);
    uint8_t outG = static_cast<uint8_t>(static_cast<float>(sg) * srcA + static_cast<float>(dg) * invSrcA + 0.5f);
    uint8_t outR = static_cast<uint8_t>(static_cast<float>(sr) * srcA + static_cast<float>(dr) * invSrcA + 0.5f);
    uint8_t outA = static_cast<uint8_t>(static_cast<float>(sa) + static_cast<float>(da) * invSrcA + 0.5f);

    pixelBuffer_->pixels[idx] = (static_cast<uint32_t>(outB))
                              | (static_cast<uint32_t>(outG) << 8)
                              | (static_cast<uint32_t>(outR) << 16)
                              | (static_cast<uint32_t>(outA) << 24);
    ++buffer_epoch_;
}

void RasterLayer::clear(const Color& color) {
    ensureUnique();
    float a = std::clamp(color.a, 0.0f, 1.0f);
    uint8_t r = static_cast<uint8_t>(std::clamp(color.r * a, 0.0f, 1.0f) * 255.0f);
    uint8_t g = static_cast<uint8_t>(std::clamp(color.g * a, 0.0f, 1.0f) * 255.0f);
    uint8_t b = static_cast<uint8_t>(std::clamp(color.b * a, 0.0f, 1.0f) * 255.0f);
    uint8_t alpha = static_cast<uint8_t>(a * 255.0f);
    uint32_t val = (static_cast<uint32_t>(b))
                 | (static_cast<uint32_t>(g) << 8)
                 | (static_cast<uint32_t>(r) << 16)
                 | (static_cast<uint32_t>(alpha) << 24);
    std::fill(pixelBuffer_->pixels.begin(), pixelBuffer_->pixels.end(), val);
    ++buffer_epoch_;
}

void RasterLayer::resize(int width, int height) {
    if (width <= 0 || height <= 0) return;
    if (width > kMaxDim) width = kMaxDim;
    if (height > kMaxDim) height = kMaxDim;

    ensureUnique();
    std::vector<uint32_t> oldPixels = std::move(pixelBuffer_->pixels);
    int oldW = width_;
    int oldH = height_;

    width_ = width;
    height_ = height;
    size_t newSize = static_cast<size_t>(width) * static_cast<size_t>(height);
    pixelBuffer_->pixels.assign(newSize, 0);

    int copyW = std::min(oldW, width);
    int copyH = std::min(oldH, height);
    for (int y = 0; y < copyH; ++y) {
        const uint32_t* srcRow = oldPixels.data() + static_cast<size_t>(y) * oldW;
        uint32_t* dstRow = pixelBuffer_->pixels.data() + static_cast<size_t>(y) * width;
        std::copy(srcRow, srcRow + copyW, dstRow);
    }
    ++buffer_epoch_;
}

void RasterLayer::relocatePixels(std::vector<uint32_t> oldPixels,
                                 int oldW, int oldH, int oldOX, int oldOY,
                                 int newW, int newH, int newOX, int newOY) {
    int copyLeft = std::max(oldOX, newOX);
    int copyTop = std::max(oldOY, newOY);
    int copyRight = std::min(oldOX + oldW, newOX + newW);
    int copyBottom = std::min(oldOY + oldH, newOY + newH);

    if (copyRight <= copyLeft || copyBottom <= copyTop) return;

    int copyWidth = copyRight - copyLeft;

    for (int cy = copyTop; cy < copyBottom; ++cy) {
        int srcY = cy - oldOY;
        int dstY = cy - newOY;
        int srcStartX = copyLeft - oldOX;
        int dstStartX = copyLeft - newOX;

        const uint32_t* srcRow = oldPixels.data() + static_cast<size_t>(srcY) * oldW + srcStartX;
        uint32_t* dstRow = pixelBuffer_->pixels.data() + static_cast<size_t>(dstY) * newW + dstStartX;
        std::copy(srcRow, srcRow + copyWidth, dstRow);
    }
}

void RasterLayer::ensureContains(int x, int y, int w, int h, bool pad) {
    if (w <= 0 || h <= 0) return;

    int guard = pad ? kGuardBand : 0;
    int grow  = pad ? kGrowthPad : 0;

    x -= guard;
    y -= guard;
    w += guard * 2;
    h += guard * 2;

    int reqLeft = x;
    int reqTop = y;
    int reqRight = x + w;
    int reqBottom = y + h;

    reqLeft = std::max(reqLeft, originX_ - kMaxDim);
    reqTop = std::max(reqTop, originY_ - kMaxDim);
    reqRight = std::min(reqRight, originX_ + kMaxDim);
    reqBottom = std::min(reqBottom, originY_ + kMaxDim);

    if (reqRight <= reqLeft || reqBottom <= reqTop) return;

    int newOriginX = std::min(originX_, reqLeft);
    int newOriginY = std::min(originY_, reqTop);
    int newRight = std::max(originX_ + width_, reqRight);
    int newBottom = std::max(originY_ + height_, reqBottom);

    bool expandedLeft = (newOriginX < originX_);
    bool expandedTop = (newOriginY < originY_);
    bool expandedRight = (newRight > originX_ + width_);
    bool expandedBottom = (newBottom > originY_ + height_);

    if (expandedLeft) newOriginX = std::max(newOriginX - grow, originX_ - kMaxDim);
    if (expandedTop) newOriginY = std::max(newOriginY - grow, originY_ - kMaxDim);
    if (expandedRight) newRight = std::min(newRight + grow, originX_ + kMaxDim);
    if (expandedBottom) newBottom = std::min(newBottom + grow, originY_ + kMaxDim);

    int64_t newW64 = static_cast<int64_t>(newRight) - static_cast<int64_t>(newOriginX);
    int64_t newH64 = static_cast<int64_t>(newBottom) - static_cast<int64_t>(newOriginY);
    if (newW64 <= 0 || newH64 <= 0) return;
    if (newW64 > kMaxDim) newRight = newOriginX + kMaxDim;
    if (newH64 > kMaxDim) newBottom = newOriginY + kMaxDim;

    int newWidth = newRight - newOriginX;
    int newHeight = newBottom - newOriginY;

    if (newOriginX == originX_ && newOriginY == originY_ &&
        newWidth == width_ && newHeight == height_) return;

    size_t newPixels = static_cast<size_t>(newWidth) * static_cast<size_t>(newHeight);
    constexpr size_t kMaxPixels = static_cast<size_t>(kMaxDim) * static_cast<size_t>(kMaxDim);
    if (newPixels == 0 || newPixels > kMaxPixels) return;

    ensureUnique();
    std::vector<uint32_t> oldPixels = std::move(pixelBuffer_->pixels);
    int oldW = width_;
    int oldH = height_;
    int oldOX = originX_;
    int oldOY = originY_;

    width_ = newWidth;
    height_ = newHeight;
    originX_ = newOriginX;
    originY_ = newOriginY;
    pixelBuffer_->pixels.assign(newPixels, 0);

    relocatePixels(std::move(oldPixels), oldW, oldH, oldOX, oldOY,
                   newWidth, newHeight, newOriginX, newOriginY);

    ++buffer_epoch_;
}

VectorLayer::VectorLayer(const std::string& name)
    : Layer(name, LayerType::Vector) {
}

std::unique_ptr<Layer> VectorLayer::clone() const {
    auto copy = std::make_unique<VectorLayer>(name_);
    copy->strokes_ = strokes_;
    copy->setVisible(visible_);
    copy->setOpacity(opacity_);
    copy->setBlendMode(blend_mode_);
    copy->setLocked(locked_);
    return copy;
}

void VectorLayer::addStroke(const VectorStroke& stroke) {
    strokes_.push_back(stroke);
}

void VectorLayer::removeStroke(int index) {
    if (index >= 0 && static_cast<size_t>(index) < strokes_.size()) {
        strokes_.erase(strokes_.begin() + index);
    }
}

void VectorLayer::clearStrokes() {
    strokes_.clear();
}

VectorStroke& VectorLayer::strokeAt(int index) {
    return strokes_.at(index);
}

const VectorStroke& VectorLayer::strokeAt(int index) const {
    return strokes_.at(index);
}

const std::vector<VectorStroke>& VectorLayer::strokes() const {
    return strokes_;
}

size_t VectorLayer::strokeCount() const {
    return strokes_.size();
}

GroupLayer::GroupLayer(const std::string& name)
    : Layer(name, LayerType::Group) {
}

std::unique_ptr<Layer> GroupLayer::clone() const {
    auto copy = std::make_unique<GroupLayer>(name_);
    for (const auto& child : layers_) {
        copy->addLayer(child->clone());
    }
    copy->setVisible(visible_);
    copy->setOpacity(opacity_);
    copy->setBlendMode(blend_mode_);
    copy->setLocked(locked_);
    return copy;
}

void GroupLayer::addLayer(std::unique_ptr<Layer> layer) {
    layers_.push_back(std::move(layer));
}

void GroupLayer::removeLayer(int index) {
    if (index >= 0 && static_cast<size_t>(index) < layers_.size()) {
        layers_.erase(layers_.begin() + index);
    }
}

Layer* GroupLayer::layerAt(int index) {
    if (index >= 0 && static_cast<size_t>(index) < layers_.size()) {
        return layers_[static_cast<size_t>(index)].get();
    }
    return nullptr;
}

const Layer* GroupLayer::layerAt(int index) const {
    if (index >= 0 && static_cast<size_t>(index) < layers_.size()) {
        return layers_[static_cast<size_t>(index)].get();
    }
    return nullptr;
}

Layer* GroupLayer::layerByUid(LayerUid uid) {
    for (auto& l : layers_) {
        if (l->uid() == uid) return l.get();
    }
    return nullptr;
}

const Layer* GroupLayer::layerByUid(LayerUid uid) const {
    for (auto& l : layers_) {
        if (l->uid() == uid) return l.get();
    }
    return nullptr;
}

int GroupLayer::indexOfLayer(const Layer* layer) const {
    for (size_t i = 0; i < layers_.size(); ++i) {
        if (layers_[i].get() == layer) return static_cast<int>(i);
    }
    return -1;
}

void GroupLayer::moveLayer(int fromIndex, int toIndex) {
    int sz = static_cast<int>(layers_.size());
    if (fromIndex < 0 || fromIndex >= sz || toIndex < 0 || toIndex >= sz) return;
    if (fromIndex == toIndex) return;
    auto layer = std::move(layers_[static_cast<size_t>(fromIndex)]);
    layers_.erase(layers_.begin() + fromIndex);
    layers_.insert(layers_.begin() + toIndex, std::move(layer));
}

Layer* GroupLayer::duplicateLayer(int index) {
    if (index < 0 || static_cast<size_t>(index) >= layers_.size()) return nullptr;
    auto clone = layers_[static_cast<size_t>(index)]->clone();
    clone->setName(layers_[static_cast<size_t>(index)]->name() + " copy");
    Layer* ptr = clone.get();
    layers_.insert(layers_.begin() + index + 1, std::move(clone));
    return ptr;
}

} // namespace fap
