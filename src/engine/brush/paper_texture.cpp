#include <vector>
#include <string>
#include <cstdint>
#include <fstream>
#include <cmath>
#include <algorithm>

namespace fap {

class PaperTexture {
public:
    PaperTexture() = default;
    ~PaperTexture() = default;

    PaperTexture(const PaperTexture&) = default;
    PaperTexture& operator=(const PaperTexture&) = default;
    PaperTexture(PaperTexture&&) noexcept = default;
    PaperTexture& operator=(PaperTexture&&) noexcept = default;

    bool load(const std::string& path);
    float sample(float u, float v) const;
    float sample(float x, float y, float scale) const;

    int width() const { return width_; }
    int height() const { return height_; }
    bool loaded() const { return loaded_; }

private:
    bool loadPGM(const std::string& path);
    bool loadBMP(const std::string& path);

    std::vector<uint8_t> pixels_;
    int width_ = 0;
    int height_ = 0;
    bool loaded_ = false;
};

#pragma pack(push, 1)
struct BMPFileHeader {
    uint16_t signature = 0;
    uint32_t fileSize = 0;
    uint16_t reserved1 = 0;
    uint16_t reserved2 = 0;
    uint32_t dataOffset = 0;
};

struct BMPInfoHeader {
    uint32_t size = 0;
    int32_t width = 0;
    int32_t height = 0;
    uint16_t planes = 0;
    uint16_t bitCount = 0;
    uint32_t compression = 0;
    uint32_t imageSize = 0;
    int32_t xPixelsPerMeter = 0;
    int32_t yPixelsPerMeter = 0;
    uint32_t colorsUsed = 0;
    uint32_t colorsImportant = 0;
};
#pragma pack(pop)

bool PaperTexture::loadPGM(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    std::string magic;
    file >> magic;
    if (magic != "P5" && magic != "P2") return false;

    bool binary = (magic == "P5");

    while (file.peek() == '#' || file.peek() == ' ' ||
           file.peek() == '\n' || file.peek() == '\r') {
        if (file.peek() == '#') {
            std::string comment;
            std::getline(file, comment);
        } else {
            file.get();
        }
    }

    int w = 0, h = 0, maxval = 0;
    file >> w;
    while (file.peek() == ' ' || file.peek() == '\n' || file.peek() == '\r') file.get();
    file >> h;
    while (file.peek() == ' ' || file.peek() == '\n' || file.peek() == '\r') file.get();
    file >> maxval;

    if (binary) file.get(); // consume single whitespace before binary data

    if (w <= 0 || h <= 0 || maxval <= 0 || maxval > 65535) return false;

    width_ = w;
    height_ = h;
    pixels_.resize(static_cast<size_t>(w) * static_cast<size_t>(h));

    if (binary) {
        if (maxval <= 255) {
            file.read(reinterpret_cast<char*>(pixels_.data()),
                      static_cast<std::streamsize>(pixels_.size()));
            if (maxval < 255) {
                for (auto& p : pixels_) {
                    p = static_cast<uint8_t>(
                        (static_cast<uint32_t>(p) * 255u) / static_cast<uint32_t>(maxval));
                }
            }
        } else {
            size_t total = pixels_.size();
            for (size_t i = 0; i < total; ++i) {
                uint8_t hi = 0;
                uint8_t lo = 0;
                file.read(reinterpret_cast<char*>(&hi), 1);
                file.read(reinterpret_cast<char*>(&lo), 1);
                uint16_t val = (static_cast<uint16_t>(hi) << 8) | lo;
                pixels_[i] = static_cast<uint8_t>(
                    (static_cast<uint32_t>(val) * 255u) / static_cast<uint32_t>(maxval));
            }
        }
    } else {
        size_t total = pixels_.size();
        for (size_t i = 0; i < total; ++i) {
            int val = 0;
            file >> val;
            pixels_[i] = static_cast<uint8_t>(
                (static_cast<uint32_t>(std::clamp(val, 0, maxval)) * 255u) /
                static_cast<uint32_t>(maxval));
        }
    }

    loaded_ = true;
    return true;
}

bool PaperTexture::loadBMP(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    BMPFileHeader fh{};
    file.read(reinterpret_cast<char*>(&fh), sizeof(fh));
    if (fh.signature != 0x4D42) return false;

    BMPInfoHeader ih{};
    file.read(reinterpret_cast<char*>(&ih), sizeof(ih));
    if (ih.compression != 0) return false;
    if (ih.bitCount != 24 && ih.bitCount != 8) return false;

    int w = ih.width;
    int h = std::abs(ih.height);
    bool topDown = ih.height < 0;

    width_ = w;
    height_ = h;
    pixels_.resize(static_cast<size_t>(w) * static_cast<size_t>(h));

    std::vector<uint8_t> palette;
    if (ih.bitCount == 8) {
        palette.resize(256 * 4);
        file.seekg(static_cast<std::streamoff>(sizeof(fh) + ih.size));
        file.read(reinterpret_cast<char*>(palette.data()), 256 * 4);
    }

    int rowSize = ((static_cast<int>(ih.bitCount) * w + 31) / 32) * 4;
    std::vector<uint8_t> rowBuf(static_cast<size_t>(rowSize));

    for (int y = 0; y < h; ++y) {
        int srcY = topDown ? y : (h - 1 - y);
        file.seekg(fh.dataOffset + static_cast<uint32_t>(srcY) * static_cast<uint32_t>(rowSize));
        file.read(reinterpret_cast<char*>(rowBuf.data()), rowSize);

        auto toGray = [](uint8_t r, uint8_t g, uint8_t b) -> uint8_t {
            return static_cast<uint8_t>(
                (static_cast<uint32_t>(r) * 77u +
                 static_cast<uint32_t>(g) * 150u +
                 static_cast<uint32_t>(b) * 29u) >> 8);
        };

        if (ih.bitCount == 24) {
            for (int x = 0; x < w; ++x) {
                uint8_t b = rowBuf[static_cast<size_t>(x) * 3];
                uint8_t g = rowBuf[static_cast<size_t>(x) * 3 + 1];
                uint8_t r = rowBuf[static_cast<size_t>(x) * 3 + 2];
                pixels_[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] =
                    toGray(r, g, b);
            }
        } else {
            for (int x = 0; x < w; ++x) {
                uint8_t idx = rowBuf[static_cast<size_t>(x)];
                uint8_t b = palette[idx * 4];
                uint8_t g = palette[idx * 4 + 1];
                uint8_t r = palette[idx * 4 + 2];
                pixels_[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] =
                    toGray(r, g, b);
            }
        }
    }

    loaded_ = true;
    return true;
}

bool PaperTexture::load(const std::string& path) {
    loaded_ = false;
    pixels_.clear();
    width_ = 0;
    height_ = 0;

    if (loadBMP(path)) return true;
    if (loadPGM(path)) return true;

    return false;
}

float PaperTexture::sample(float u, float v) const {
    if (!loaded_ || width_ <= 0 || height_ <= 0) return 0.0f;

    auto wrapCoord = [](int coord, int max) -> int {
        coord = coord % max;
        if (coord < 0) coord += max;
        return coord;
    };

    float fx = (u - std::floor(u)) * static_cast<float>(width_) - 0.5f;
    float fy = (v - std::floor(v)) * static_cast<float>(height_) - 0.5f;

    int x0 = static_cast<int>(std::floor(fx));
    int y0 = static_cast<int>(std::floor(fy));
    float tx = fx - static_cast<float>(x0);
    float ty = fy - static_cast<float>(y0);

    x0 = wrapCoord(x0, width_);
    y0 = wrapCoord(y0, height_);
    int x1 = wrapCoord(x0 + 1, width_);
    int y1 = wrapCoord(y0 + 1, height_);

    float p00 = pixels_[static_cast<size_t>(y0) * static_cast<size_t>(width_) +
                        static_cast<size_t>(x0)] / 255.0f;
    float p10 = pixels_[static_cast<size_t>(y0) * static_cast<size_t>(width_) +
                        static_cast<size_t>(x1)] / 255.0f;
    float p01 = pixels_[static_cast<size_t>(y1) * static_cast<size_t>(width_) +
                        static_cast<size_t>(x0)] / 255.0f;
    float p11 = pixels_[static_cast<size_t>(y1) * static_cast<size_t>(width_) +
                        static_cast<size_t>(x1)] / 255.0f;

    float top = p00 + (p10 - p00) * tx;
    float bottom = p01 + (p11 - p01) * tx;
    return top + (bottom - top) * ty;
}

float PaperTexture::sample(float x, float y, float scale) const {
    if (!loaded_ || width_ <= 0 || height_ <= 0) return 0.0f;
    float u = x * scale / static_cast<float>(width_);
    float v = y * scale / static_cast<float>(height_);
    return sample(u, v);
}

} // namespace fap
