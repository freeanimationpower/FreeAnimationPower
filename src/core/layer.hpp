#pragma once

#include "types.hpp"
#include "engine/vector/bezier_path.hpp"
#include <vector>
#include <memory>
#include <string>
#include <deque>
#include <cstdint>
#include <atomic>
#include <span>
#include <optional>
#include <functional>

namespace fap {

using LayerUid = uint64_t;
LayerUid generateLayerUid();

struct PixelBuffer {
    std::vector<uint32_t> pixels;
};

class Layer : public NonCopyable {
public:
    Layer(const std::string& name, LayerType type);
    virtual ~Layer() = default;

    virtual std::unique_ptr<Layer> clone() const = 0;

    LayerUid uid() const { return uid_; }
    void regenerateUid() { uid_ = generateLayerUid(); }
    const std::string& name() const { return name_; }
    LayerType type() const { return type_; }
    bool visible() const { return visible_; }
    float opacity() const { return opacity_; }
    BlendMode blendMode() const { return blend_mode_; }
    bool locked() const { return locked_; }

    void setName(const std::string& name) { name_ = name; }
    void setVisible(bool v) { visible_ = v; }
    void setOpacity(float o) { opacity_ = std::clamp(o, 0.0f, 1.0f); }
    void setBlendMode(BlendMode mode) { blend_mode_ = mode; }
    void setLocked(bool l) { locked_ = l; }

protected:
    LayerUid uid_;
    std::string name_;
    LayerType type_;
    bool visible_ = true;
    float opacity_ = 1.0f;
    BlendMode blend_mode_ = BlendMode::Normal;
    bool locked_ = false;
};

class RasterLayer : public Layer {
public:
    RasterLayer(const std::string& name, int width, int height);

    std::unique_ptr<Layer> clone() const override;

    int width() const { return width_; }
    int height() const { return height_; }
    int originX() const { return originX_; }
    int originY() const { return originY_; }
    void setOrigin(int x, int y) { originX_ = x; originY_ = y; }

    const uint32_t* pixelData() const { return pixelBuffer_->pixels.data(); }
    uint32_t* pixelData() { return pixelBuffer_->pixels.data(); }
    const std::shared_ptr<PixelBuffer>& pixelBuffer() const { return pixelBuffer_; }
    std::span<const uint32_t> pixelSpan() const { return pixelBuffer_->pixels; }
    std::span<uint32_t> mutablePixelSpan() { return pixelBuffer_->pixels; }
    size_t pixelCount() const { return pixelBuffer_->pixels.size(); }
    uint64_t bufferEpoch() const { return buffer_epoch_; }
    void bufferEpochTick() { ++buffer_epoch_; }

    bool isBufferShared() const { return pixelBuffer_.use_count() > 1; }
    void ensureUnique();

    bool hasContent() const { return hasContent_; }
    void setHasContent(bool v) { hasContent_ = v; }

    Color pixelAt(int x, int y) const;
    void setPixel(int x, int y, const Color& color);
    void blendPixel(int x, int y, const Color& color);
    void clear(const Color& color = Color{0.0f, 0.0f, 0.0f, 0.0f});
    void resize(int width, int height);
    void ensureContains(int x, int y, int w, int h, bool pad = true);

    static constexpr int kMaxDim = 10'000;
    static constexpr int kGrowthPad = 512;
    static constexpr int kGuardBand = 2;

private:
    int width_;
    int height_;
    int originX_ = 0;
    int originY_ = 0;
    std::shared_ptr<PixelBuffer> pixelBuffer_;
    uint64_t buffer_epoch_ = 0;
    bool hasContent_ = false;
    size_t indexAt(int x, int y) const;
    void relocatePixels(std::vector<uint32_t> oldPixels,
                        int oldW, int oldH, int oldOX, int oldOY,
                        int newW, int newH, int newOX, int newOY);
};

class VectorStroke;

class VectorLayer : public Layer {
public:
    VectorLayer(const std::string& name);

    std::unique_ptr<Layer> clone() const override;

    void addStroke(const VectorStroke& stroke);
    void removeStroke(int index);
    void clearStrokes();
    VectorStroke& strokeAt(int index);
    const VectorStroke& strokeAt(int index) const;
    const std::vector<VectorStroke>& strokes() const;
    size_t strokeCount() const;

private:
    std::vector<VectorStroke> strokes_;
};

class GroupLayer : public Layer {
public:
    GroupLayer(const std::string& name);

    std::unique_ptr<Layer> clone() const override;

    void addLayer(std::unique_ptr<Layer> layer);
    void removeLayer(int index);
    void moveLayer(int fromIndex, int toIndex);
    Layer* duplicateLayer(int index);
    Layer* layerAt(int index);
    const Layer* layerAt(int index) const;
    Layer* layerByUid(LayerUid uid);
    const Layer* layerByUid(LayerUid uid) const;
    int indexOfLayer(const Layer* layer) const;
    const std::deque<std::unique_ptr<Layer>>& layers() const { return layers_; }
    size_t layerCount() const { return layers_.size(); }
    std::deque<std::unique_ptr<Layer>>& layers() { return layers_; }

private:
    std::deque<std::unique_ptr<Layer>> layers_;
};

} // namespace fap
