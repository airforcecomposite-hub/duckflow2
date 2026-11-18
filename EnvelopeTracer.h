#pragma once

#include "MultiSegmentEnvelope.h"
#include <vector>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <fstream>

namespace duckflow {

/**
 * EnvelopeTracer
 * Implements Duck 2's "Trace Envelope" feature:
 *  - Extracts a smooth amplitude contour from an audio buffer or dropped WAV file
 *  - Supports inverted ducking trace (loud kick -> deep duck) or follower trace
 *  - Simplifies dense amplitude frames via Ramer-Douglas-Peucker into clean,
 *    editable MultiSegmentEnvelope nodes (typically 6-18 nodes)
 *  - Built-in minimal RIFF/WAVE PCM/IEEE-Float parser so WAV import works even without external codecs
 */
class EnvelopeTracer {
public:
    static EnvelopeData traceFromMonoBuffer(
        const float* samples,
        std::size_t numSamples,
        bool invertForDucking = true,
        float tolerance = 0.028f,
        std::size_t maxNodes = 20)
    {
        EnvelopeData out;
        if (samples == nullptr || numSamples < 32) {
            return out;
        }

        constexpr std::size_t kNumBins = 128;
        std::array<float, kNumBins> contour{};
        float peakOverall = 1.0e-6f;

        for (std::size_t b = 0; b < kNumBins; ++b) {
            const std::size_t i0 = (b * numSamples) / kNumBins;
            const std::size_t i1 = std::max(i0 + 1, ((b + 1) * numSamples) / kNumBins);
            double sumSq = 0.0;
            float localPeak = 0.0f;
            for (std::size_t i = i0; i < std::min(i1, numSamples); ++i) {
                const float a = std::fabs(samples[i]);
                sumSq += static_cast<double>(a * a);
                localPeak = std::max(localPeak, a);
            }
            const float rms = static_cast<float>(std::sqrt(sumSq / static_cast<double>(i1 - i0)));
            // Hybrid 65% peak + 35% RMS captures both punchy transient and body decay
            contour[b] = 0.65f * localPeak + 0.35f * rms;
            peakOverall = std::max(peakOverall, contour[b]);
        }

        // Normalize and apply mild 3-point moving average + perceptual power curve
        std::array<float, kNumBins> smoothed{};
        for (std::size_t b = 0; b < kNumBins; ++b) {
            const float prev = contour[b > 0 ? b - 1 : 0];
            const float curr = contour[b];
            const float next = contour[b + 1 < kNumBins ? b + 1 : kNumBins - 1];
            float norm = std::clamp(((0.25f * prev + 0.5f * curr + 0.25f * next) / peakOverall), 0.0f, 1.0f);
            norm = std::pow(norm, 0.7f); // Perceptual loudness curve
            smoothed[b] = invertForDucking ? (1.0f - norm) : norm;
        }

        // Ramer-Douglas-Peucker simplification
        std::vector<bool> keep(kNumBins, false);
        keep.front() = true;
        keep.back() = true;
        rdpSimplify(smoothed, 0, kNumBins - 1, std::clamp(tolerance, 0.005f, 0.15f), keep);

        // Enforce maxNodes cap if signal is very spiky
        std::vector<std::size_t> indices;
        for (std::size_t b = 0; b < kNumBins; ++b) {
            if (keep[b]) indices.push_back(b);
        }
        while (indices.size() > std::max<std::size_t>(4, maxNodes)) {
            // Remove internal node with smallest triangle area
            float minArea = 1e9f;
            std::size_t removeIdx = 1;
            for (std::size_t k = 1; k + 1 < indices.size(); ++k) {
                const float x0 = static_cast<float>(indices[k - 1]) / static_cast<float>(kNumBins - 1);
                const float y0 = smoothed[indices[k - 1]];
                const float x1 = static_cast<float>(indices[k]) / static_cast<float>(kNumBins - 1);
                const float y1 = smoothed[indices[k]];
                const float x2 = static_cast<float>(indices[k + 1]) / static_cast<float>(kNumBins - 1);
                const float y2 = smoothed[indices[k + 1]];
                const float area = std::fabs(x0 * (y1 - y2) + x1 * (y2 - y0) + x2 * (y0 - y1));
                if (area < minArea) {
                    minArea = area;
                    removeIdx = k;
                }
            }
            indices.erase(indices.begin() + static_cast<std::ptrdiff_t>(removeIdx));
        }

        out.nodes.clear();
        for (std::size_t idx : indices) {
            const float x = static_cast<float>(idx) / static_cast<float>(kNumBins - 1);
            const float y = std::clamp(smoothed[idx], 0.0f, 1.0f);
            out.nodes.push_back({x, y, 0.0f, CurveType::SCurveSpline});
        }
        out.nodes.front().x = 0.0f;
        out.nodes.back().x = 1.0f;
        out.sortAndValidate();
        return out;
    }

    /**
     * Minimal standalone 16/24/32-bit PCM & 32-bit IEEE Float WAV reader for Drag & Drop Trace WAV
     */
    static bool loadWavMonoMix(const std::string& path, std::vector<float>& outMono, double& outSampleRate) {
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs) return false;

        char riff[4]{};
        std::uint32_t riffSize = 0;
        char wave[4]{};
        ifs.read(riff, 4);
        ifs.read(reinterpret_cast<char*>(&riffSize), 4);
        ifs.read(wave, 4);
        if (std::memcmp(riff, "RIFF", 4) != 0 || std::memcmp(wave, "WAVE", 4) != 0) {
            return false;
        }

        std::uint16_t audioFormat = 0;
        std::uint16_t numChannels = 0;
        std::uint32_t sampleRate = 44100;
        std::uint16_t bitsPerSample = 16;
        std::vector<std::uint8_t> rawData;

        while (ifs && ifs.good()) {
            char chunkId[4]{};
            std::uint32_t chunkSize = 0;
            if (!ifs.read(chunkId, 4)) break;
            if (!ifs.read(reinterpret_cast<char*>(&chunkSize), 4)) break;

            if (std::memcmp(chunkId, "fmt ", 4) == 0 && chunkSize >= 16) {
                std::uint32_t byteRate = 0;
                std::uint16_t blockAlign = 0;
                ifs.read(reinterpret_cast<char*>(&audioFormat), 2);
                ifs.read(reinterpret_cast<char*>(&numChannels), 2);
                ifs.read(reinterpret_cast<char*>(&sampleRate), 4);
                ifs.read(reinterpret_cast<char*>(&byteRate), 4);
                ifs.read(reinterpret_cast<char*>(&blockAlign), 2);
                ifs.read(reinterpret_cast<char*>(&bitsPerSample), 2);
                if (chunkSize > 16) {
                    ifs.seekg(static_cast<std::streamoff>(chunkSize - 16), std::ios::cur);
                }
            } else if (std::memcmp(chunkId, "data", 4) == 0) {
                // Cap at 10 seconds to keep trace instant and safe
                const std::size_t maxBytes = 10u * 192000u * 8u;
                const std::size_t toRead = std::min<std::size_t>(chunkSize, maxBytes);
                rawData.resize(toRead);
                ifs.read(reinterpret_cast<char*>(rawData.data()), static_cast<std::streamsize>(toRead));
                break;
            } else {
                ifs.seekg(static_cast<std::streamoff>(chunkSize + (chunkSize & 1u)), std::ios::cur);
            }
        }

        if (rawData.empty() || numChannels == 0 || (audioFormat != 1 && audioFormat != 3)) {
            return false;
        }
        const std::size_t bytesPerSample = bitsPerSample / 8u;
        if (bytesPerSample == 0) return false;
        const std::size_t frameSize = bytesPerSample * numChannels;
        const std::size_t numFrames = rawData.size() / frameSize;
        if (numFrames < 32) return false;

        outMono.assign(numFrames, 0.0f);
        outSampleRate = static_cast<double>(sampleRate);
        const float invCh = 1.0f / static_cast<float>(numChannels);

        for (std::size_t f = 0; f < numFrames; ++f) {
            float sum = 0.0f;
            for (std::uint16_t ch = 0; ch < numChannels; ++ch) {
                const std::uint8_t* p = rawData.data() + f * frameSize + ch * bytesPerSample;
                float s = 0.0f;
                if (audioFormat == 1 && bitsPerSample == 16) {
                    std::int16_t v = static_cast<std::int16_t>(p[0] | (static_cast<std::uint16_t>(p[1]) << 8));
                    s = static_cast<float>(v) / 32768.0f;
                } else if (audioFormat == 1 && bitsPerSample == 24) {
                    std::int32_t v = static_cast<std::int32_t>(p[0]) |
                                     (static_cast<std::int32_t>(p[1]) << 8) |
                                     (static_cast<std::int32_t>(static_cast<std::int8_t>(p[2])) << 16);
                    s = static_cast<float>(v) / 8388608.0f;
                } else if (audioFormat == 1 && bitsPerSample == 32) {
                    std::int32_t v = 0;
                    std::memcpy(&v, p, 4);
                    s = static_cast<float>(static_cast<double>(v) / 2147483648.0);
                } else if (audioFormat == 3 && bitsPerSample == 32) {
                    std::memcpy(&s, p, 4);
                }
                sum += sanitizeSample(s);
            }
            outMono[f] = sum * invCh;
        }
        return true;
    }

private:
    static void rdpSimplify(
        const std::array<float, 128>& pts,
        std::size_t first,
        std::size_t last,
        float eps,
        std::vector<bool>& keep)
    {
        if (last <= first + 1) return;

        const float x1 = static_cast<float>(first) / 127.0f;
        const float y1 = pts[first];
        const float x2 = static_cast<float>(last) / 127.0f;
        const float y2 = pts[last];
        const float dx = x2 - x1;
        const float dy = y2 - y1;
        const float len = std::sqrt(dx * dx + dy * dy);

        float maxDist = 0.0f;
        std::size_t maxIdx = first;

        for (std::size_t i = first + 1; i < last; ++i) {
            const float px = static_cast<float>(i) / 127.0f;
            const float py = pts[i];
            float dist = 0.0f;
            if (len > 1.0e-6f) {
                dist = std::fabs(dy * px - dx * py + x2 * y1 - y2 * x1) / len;
            } else {
                dist = std::fabs(py - y1);
            }
            if (dist > maxDist) {
                maxDist = dist;
                maxIdx = i;
            }
        }

        if (maxDist > eps && maxIdx > first && maxIdx < last) {
            keep[maxIdx] = true;
            rdpSimplify(pts, first, maxIdx, eps, keep);
            rdpSimplify(pts, maxIdx, last, eps, keep);
        }
    }
};

} // namespace duckflow
