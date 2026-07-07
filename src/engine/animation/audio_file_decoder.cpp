#define DR_WAV_IMPLEMENTATION
#define DR_MP3_IMPLEMENTATION
#define DR_FLAC_IMPLEMENTATION

#include "audio_file_decoder.hpp"

#include "dr_wav.h"
#include "dr_mp3.h"
#include "dr_flac.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace fap {

AudioDecodeResult decodeAudioFile(const std::string& filepath, int targetPeaks)
{
    AudioDecodeResult result;

    auto dotPos = filepath.rfind('.');
    bool isMp3  = false;
    bool isFlac = false;
    bool isOgg  = false;
    if (dotPos != std::string::npos) {
        std::string ext = filepath.substr(dotPos);
        for (auto& c : ext) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        isMp3  = (ext == ".mp3");
        isFlac = (ext == ".flac");
        isOgg  = (ext == ".ogg");
    }

    void* pcm = nullptr;
    unsigned int channels = 0;
    unsigned int sampleRate = 0;
    drwav_uint64 totalFrames = 0;
    bool isMp3Result = false;
    bool isFlacResult = false;

    if (isMp3) {
        drmp3_config config;
        drmp3_uint64 frames64 = 0;
        float* fpcm = drmp3_open_file_and_read_pcm_frames_f32(
            filepath.c_str(), &config, &frames64, nullptr);
        pcm = fpcm;
        if (fpcm) {
            channels = config.channels;
            sampleRate = config.sampleRate;
            totalFrames = frames64;
            isMp3Result = true;
        }
    } else if (isFlac || isOgg) {
        float* fpcm = drflac_open_file_and_read_pcm_frames_f32(
            filepath.c_str(), &channels, &sampleRate, &totalFrames, nullptr);
        pcm = fpcm;
        isFlacResult = (fpcm != nullptr);
    } else {
        float* fpcm = drwav_open_file_and_read_pcm_frames_f32(
            filepath.c_str(), &channels, &sampleRate, &totalFrames, nullptr);
        pcm = fpcm;
    }

    if (!pcm || totalFrames == 0) {
        return result;
    }

    result.channels = static_cast<int>(channels);
    result.sampleRate = static_cast<int>(sampleRate);

    if (sampleRate > 0) {
        result.durationMs = static_cast<int64_t>(
            static_cast<double>(totalFrames) * 1000.0 / sampleRate);
    }

    if (targetPeaks <= 0) targetPeaks = 800;
    drwav_uint64 step = std::max<drwav_uint64>(1, totalFrames / static_cast<drwav_uint64>(targetPeaks));
    result.peaks.reserve(static_cast<size_t>(targetPeaks));

    const float* fpcm = static_cast<const float*>(pcm);
    for (drwav_uint64 i = 0; i < totalFrames; i += step) {
        float peak = 0.0f;
        drwav_uint64 end = std::min(i + step, totalFrames);

        for (drwav_uint64 j = i; j < end; ++j) {
            for (unsigned int ch = 0; ch < channels; ++ch) {
                size_t idx = static_cast<size_t>(j * channels + ch);
                peak = std::max(peak, std::abs(fpcm[idx]));
            }
        }
        result.peaks.push_back(peak);
    }

    if (isMp3Result) {
        drmp3_free(pcm, nullptr);
    } else if (isFlacResult) {
        drflac_free(pcm, nullptr);
    } else {
        drwav_free(pcm, nullptr);
    }

    result.success = true;
    return result;
}

} // namespace fap
