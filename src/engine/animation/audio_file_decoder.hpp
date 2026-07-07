#pragma once

#include <vector>
#include <string>
#include <cstdint>

namespace fap {

struct AudioDecodeResult {
    bool success = false;
    std::vector<float> peaks;
    int sampleRate = 0;
    int channels = 0;
    int64_t durationMs = 0;
};

AudioDecodeResult decodeAudioFile(const std::string& filepath, int targetPeaks = 800);

} // namespace fap
