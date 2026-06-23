#pragma once

#include "core/types.hpp"
#include "brush_engine.hpp"
#include <vector>
#include <string>
#include <cstdint>
#include <memory>

namespace fap {

#pragma pack(push, 1)
struct AbrHeader {
    uint16_t version;
    uint16_t subversion;
    int32_t commentOffset;
    int32_t numBrushes;
};

struct AbrBrushHeader {
    int32_t type;
    int32_t extraSize;
    int32_t top;
    int32_t left;
    int32_t bottom;
    int32_t right;
    int32_t depth;
    int8_t compression;
    int32_t dataSize;
};
#pragma pack(pop)

struct ParsedAbrBrush {
    std::string name;
    int width = 0;
    int height = 0;
    std::vector<uint8_t> imageData;
    float spacing = 0.25f;
};

class AbrImporter {
public:
    AbrImporter();

    bool loadFile(const std::string& path);
    const std::vector<ParsedAbrBrush>& brushes() const { return brushes_; }
    size_t brushCount() const { return brushes_.size(); }

    BrushTip convertToBrushTip(int brushIndex) const;
    BrushPreset createPreset(int brushIndex, const std::string& name = {}) const;

    static bool isAbrFile(const std::string& path);
    static constexpr size_t kMaxBrushes = 512;
    static constexpr int kMaxBrushSize = 2500;

private:
    std::vector<ParsedAbrBrush> brushes_;
    bool parseVersion1(const uint8_t* data, size_t size);
    bool parseVersion2(const uint8_t* data, size_t size);
    bool parseVersion6(const uint8_t* data, size_t size);
    bool decompressRle(const uint8_t* src, size_t srcSize,
                       std::vector<uint8_t>& dst, int width, int height);
};

} // namespace fap
