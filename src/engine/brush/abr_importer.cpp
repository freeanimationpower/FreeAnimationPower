#include "abr_importer.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <iterator>

namespace fap {

AbrImporter::AbrImporter() = default;

bool AbrImporter::loadFile(const std::string& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size <= 0 || static_cast<size_t>(size) > 100 * 1024 * 1024) return false;

    std::vector<uint8_t> data(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(data.data()), size);
    file.close();

    if (static_cast<size_t>(size) < sizeof(AbrHeader)) return false;

    AbrHeader header;
    std::memcpy(&header, data.data(), sizeof(AbrHeader));

    uint16_t raw = header.version;
    uint16_t version = static_cast<uint16_t>((raw << 8) | (raw >> 8));

    switch (version) {
    case 1:
        return parseVersion1(data.data(), static_cast<size_t>(size));
    case 2:
        return parseVersion2(data.data(), static_cast<size_t>(size));
    case 6:
        return parseVersion6(data.data(), static_cast<size_t>(size));
    default:
        return false;
    }
}

bool AbrImporter::isAbrFile(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    uint32_t magic;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    file.close();

    return (magic == 0x31535042 || magic == 0x32535042);
}

BrushTip AbrImporter::convertToBrushTip(int brushIndex) const
{
    BrushTip tip;
    if (brushIndex < 0 || static_cast<size_t>(brushIndex) >= brushes_.size()) return tip;

    const auto& brush = brushes_[brushIndex];
    tip.name = brush.name;
    tip.spacing = brush.spacing;
    tip.hardness = 0.8f;
    tip.roundness = 1.0f;
    tip.angle = 0.0f;

    return tip;
}

BrushPreset AbrImporter::createPreset(int brushIndex, const std::string& name) const
{
    BrushPreset preset;
    if (brushIndex < 0 || static_cast<size_t>(brushIndex) >= brushes_.size()) return preset;

    const auto& brush = brushes_[brushIndex];
    preset.name = name.empty() ? brush.name : name;
    preset.tip = convertToBrushTip(brushIndex);
    preset.color = { 0.0f, 0.0f, 0.0f, 1.0f };
    preset.size = static_cast<float>(std::max(brush.width, brush.height));
    preset.opacity = 1.0f;
    preset.flow = 0.3f;

    return preset;
}

bool AbrImporter::parseVersion1(const uint8_t* data, size_t size)
{
    if (size < sizeof(AbrHeader)) return false;

    AbrHeader header;
    std::memcpy(&header, data, sizeof(AbrHeader));

    size_t offset = sizeof(AbrHeader);
    const int numBrushes = (header.numBrushes > 0) ? header.numBrushes : 1;

    for (int i = 0; i < numBrushes && i < static_cast<int>(kMaxBrushes); ++i) {
        if (offset + sizeof(AbrBrushHeader) > size) break;

        AbrBrushHeader bh;
        std::memcpy(&bh, data + offset, sizeof(AbrBrushHeader));
        offset += sizeof(AbrBrushHeader);

        ParsedAbrBrush brush;
        brush.width = bh.right - bh.left;
        brush.height = bh.bottom - bh.top;

        if (brush.width <= 0 || brush.width > kMaxBrushSize ||
            brush.height <= 0 || brush.height > kMaxBrushSize) {
            continue;
        }

        size_t dataSize = static_cast<size_t>(std::max(0, bh.dataSize));
        if (offset + dataSize > size) break;

        if (bh.compression == 0) {
            size_t expected = static_cast<size_t>(brush.width) * brush.height;
            if (dataSize >= expected) {
                brush.imageData.assign(data + offset, data + offset + expected);
            }
        } else if (bh.compression == 1) {
            decompressRle(data + offset, dataSize, brush.imageData, brush.width, brush.height);
        }

        offset += dataSize;
        brushes_.push_back(std::move(brush));
    }

    return !brushes_.empty();
}

bool AbrImporter::parseVersion2(const uint8_t* data, size_t size)
{
    return parseVersion1(data, size);
}

bool AbrImporter::parseVersion6(const uint8_t* data, size_t size)
{
    if (size < sizeof(AbrHeader) + 4) return false;

    size_t offset = sizeof(AbrHeader);
    int32_t numBrushes;
    std::memcpy(&numBrushes, data + offset, sizeof(numBrushes));
    offset += 4;

    const int count = (numBrushes > 0) ? numBrushes : 1;
    const int actualCount = std::min(count, static_cast<int>(kMaxBrushes));

    for (int i = 0; i < actualCount; ++i) {
        if (offset + 4 > size) break;

        int32_t dataSize;
        std::memcpy(&dataSize, data + offset, sizeof(dataSize));
        offset += 4;

        if (dataSize <= 0 || offset + static_cast<size_t>(dataSize) > size) continue;

        ParsedAbrBrush brush;
        brush.width = 256;
        brush.height = 256;
        brush.spacing = 0.25f;

        if (!decompressRle(data + offset, static_cast<size_t>(dataSize),
                           brush.imageData, brush.width, brush.height)) {
            offset += dataSize;
            continue;
        }

        offset += dataSize;
        brushes_.push_back(std::move(brush));
    }

    return !brushes_.empty();
}

bool AbrImporter::decompressRle(const uint8_t* src, size_t srcSize,
                                 std::vector<uint8_t>& dst, int width, int height)
{
    const size_t expected = static_cast<size_t>(width) * height;
    dst.reserve(expected);

    size_t pos = 0;
    while (pos < srcSize && dst.size() < expected) {
        int16_t n;
        if (pos + 2 > srcSize) break;
        std::memcpy(&n, src + pos, sizeof(n));
        pos += 2;

        if (n >= 0) {
            int count = n + 1;
            for (int i = 0; i < count && pos < srcSize && dst.size() < expected; ++i) {
                dst.push_back(src[pos++]);
            }
        } else {
            int count = -n + 1;
            uint8_t val = (pos < srcSize) ? src[pos++] : 0;
            for (int i = 0; i < count && dst.size() < expected; ++i) {
                dst.push_back(val);
            }
        }
    }

    dst.resize(expected, 0);
    return !dst.empty();
}

} // namespace fap
