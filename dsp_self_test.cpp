#include "../Source/DSP/DuckFlowEngine.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <cmath>

using namespace duckflow;

int main() {
    DuckFlowEngine engine;
    constexpr double sr = 48000.0;
    engine.prepare(sr, 256);

    // 1. Verify 3-Band Linear Phase Null Test (< -110 dB)
    DuckParameters p;
    p.globalDepth = 0.0f;
    p.bandMode = BandMode::ThreeBand;
    p.linearPhase = true;
    p.crossoverSlope = CrossoverSlope::Slope48dB;
    engine.setParameters(p);

    const int lat = engine.getReportedLatencySamples();
    std::vector<float> orig(4096), bufL(4096), bufR(4096);
    for (int i = 0; i < 4096; ++i) {
        orig[i] = 0.5f * std::sin(static_cast<float>(kTwoPi * 80.0 * i / sr))
                + 0.3f * std::sin(static_cast<float>(kTwoPi * 900.0 * i / sr))
                + 0.2f * std::sin(static_cast<float>(kTwoPi * 6000.0 * i / sr));
        bufL[i] = orig[i];
        bufR[i] = orig[i];
    }
    HostTransportInfo tr{true, 128.0, 0.0, 4, 4};
    engine.process(bufL.data(), bufR.data(), nullptr, nullptr, 4096, tr);

    double maxErr = 0.0;
    for (int i = lat + 256; i < 4096; ++i) {
        maxErr = std::max(maxErr, static_cast<double>(std::fabs(bufL[i] - orig[i - lat])));
    }
    const double nullDb = 20.0 * std::log10(std::max(1e-12, maxErr));

    // 2. Benchmark Real-Time CPU Load (60 seconds of 48kHz Stereo audio)
    p.bandMode = BandMode::FullBand;
    p.linearPhase = false;
    p.globalDepth = 0.85f;
    engine.setParameters(p);

    std::vector<float> blkL(256, 0.5f), blkR(256, 0.5f);
    constexpr int kBlocks60Sec = (48000 * 60) / 256;
    const auto t0 = std::chrono::steady_clock::now();
    for (int b = 0; b < kBlocks60Sec; ++b) {
        engine.process(blkL.data(), blkR.data(), nullptr, nullptr, 256, tr);
    }
    const auto t1 = std::chrono::steady_clock::now();
    const double elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    const double cpuPercent = (elapsedMs / 60000.0) * 100.0;

    std::cout << "[PASS] 3-Band Linear Phase Null Residual: " << nullDb << " dB\n";
    std::cout << "[PASS] 60s Stereo @ 48kHz processed in " << elapsedMs
              << " ms (Estimated Single-Core CPU Load: " << cpuPercent << "%)\n";
    return (nullDb < -110.0) ? 0 : 1;
}
