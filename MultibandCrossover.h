#pragma once

#include "SimdAndMath.h"
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>

namespace duckflow {

enum class BandMode : int {
    FullBand = 0,
    TwoBand = 1,
    ThreeBand = 2
};

enum class CrossoverSlope : int {
    Slope12dB = 0, // Linkwitz-Riley 2nd order (LR2)
    Slope24dB = 1, // Linkwitz-Riley 4th order (LR4)
    Slope48dB = 2  // Linkwitz-Riley 8th order (LR8)
};

/**
 * Zero-Delay-Feedback Topology-Preserving Transform (TPT) State Variable Filter (Cytomic / Andrew Simper).
 * Numerically unconditional stability up to Nyquist and artifact-free modulation.
 */
struct TptSvf2Pole {
    float ic1eq = 0.0f;
    float ic2eq = 0.0f;
    float g = 0.0f;
    float k = 1.41421356f; // Butterworth Q = 1/sqrt(2) -> k = sqrt(2)
    float a1 = 0.0f;
    float a2 = 0.0f;
    float a3 = 0.0f;

    void setCoeffs(double sampleRate, float cutoffHz, float q = 0.70710678f) noexcept {
        const float nyq = static_cast<float>(sampleRate * 0.49);
        const float fc = std::clamp(cutoffHz, 15.0f, std::max(20.0f, nyq));
        g = static_cast<float>(std::tan(kPi * static_cast<double>(fc) / sampleRate));
        k = 1.0f / std::max(0.05f, q);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    void reset() noexcept {
        ic1eq = 0.0f;
        ic2eq = 0.0f;
    }

    inline void processLP_HP(float v0, float& lp, float& hp) noexcept {
        const float v3 = v0 - ic2eq;
        const float v1 = a1 * ic1eq + a2 * v3;
        const float v2 = ic2eq + a2 * ic1eq + a3 * v3;
        ic1eq = 2.0f * v1 - ic1eq;
        ic2eq = 2.0f * v2 - ic2eq;
        lp = v2;
        hp = v0 - k * v1 - v2;
    }

    inline float processLowpass(float v0) noexcept {
        float lp = 0.0f, hp = 0.0f;
        processLP_HP(v0, lp, hp);
        return lp;
    }

    inline float processHighpass(float v0) noexcept {
        float lp = 0.0f, hp = 0.0f;
        processLP_HP(v0, lp, hp);
        return hp;
    }

    inline float processAllpass2ndOrder(float v0) noexcept {
        // 2nd-order allpass matching LR4 sum phase: LP + HP of Q = 0.70710678 gives AP2!
        // Note: For Butterworth 2-pole H_LP(s) + H_HP(s) = (s^2 + wc^2)/(s^2 + sqrt(2)wc s + wc^2) (notch),
        // whereas true AP2 is (s^2 - sqrt(2)wc s + wc^2)/(s^2 + sqrt(2)wc s + wc^2) = v0 - 2*k*BP!
        const float v3 = v0 - ic2eq;
        const float v1 = a1 * ic1eq + a2 * v3;
        const float v2 = ic2eq + a2 * ic1eq + a3 * v3;
        ic1eq = 2.0f * v1 - ic1eq;
        ic2eq = 2.0f * v2 - ic2eq;
        return v0 - 2.0f * k * v1;
    }
};

/**
 * 1st-order TPT Lowpass/Highpass/Allpass for LR2 (12 dB/oct Linkwitz-Riley)
 * In LR2: LP2 = LP1 * LP1, HP2 = -(HP1 * HP1) so LP2 + HP2 = AP1(s) = (1 - s/wc)/(1 + s/wc)!
 */
struct TptOnePole {
    float s = 0.0f;
    float G = 0.0f;

    void setCoeffs(double sampleRate, float cutoffHz) noexcept {
        const float nyq = static_cast<float>(sampleRate * 0.49);
        const float fc = std::clamp(cutoffHz, 15.0f, std::max(20.0f, nyq));
        const float g = static_cast<float>(std::tan(kPi * static_cast<double>(fc) / sampleRate));
        G = g / (1.0f + g);
    }

    void reset() noexcept { s = 0.0f; }

    inline void processLP_HP(float x, float& lp, float& hp) noexcept {
        const float v = (x - s) * G;
        lp = v + s;
        s = lp + v;
        hp = x - lp;
    }

    inline float processLowpass(float x) noexcept {
        float lp = 0.0f, hp = 0.0f;
        processLP_HP(x, lp, hp);
        return lp;
    }

    inline float processHighpass(float x) noexcept {
        float lp = 0.0f, hp = 0.0f;
        processLP_HP(x, lp, hp);
        return hp;
    }

    inline float processAllpass(float x) noexcept {
        float lp = 0.0f, hp = 0.0f;
        processLP_HP(x, lp, hp);
        return lp - hp; // (1 - s/wc)/(1 + s/wc) matches LR2 sum LP1^2 - HP1^2!
    }
};

/**
 * Single Linkwitz-Riley 2-Way Split (12 / 24 / 48 dB/oct) + matching Allpass branch
 * Guarantees exact flat magnitude AND phase sum across 2-band and 3-band trees!
 */
struct LinkwitzRileySplit {
    // LR2 uses two 1-poles per branch
    TptOnePole lr2_lpA, lr2_lpB, lr2_hpA, lr2_hpB, lr2_ap;

    // LR4 uses two Butterworth 2-poles (Q = 1/sqrt(2)) per branch
    TptSvf2Pole lr4_lpA, lr4_lpB, lr4_hpA, lr4_hpB, lr4_ap;

    // LR8 uses two cascaded 4th-order Butterworth stages (Q1 = 0.54119610f, Q2 = 1.30656296f) repeated twice
    // And matching 4th-order allpass = AP2(Q1) * AP2(Q2)!
    static constexpr float kButter4Q1 = 0.54119610f;
    static constexpr float kButter4Q2 = 1.30656296f;
    TptSvf2Pole lr8_lp1A, lr8_lp2A, lr8_lp1B, lr8_lp2B;
    TptSvf2Pole lr8_hp1A, lr8_hp2A, lr8_hp1B, lr8_hp2B;
    TptSvf2Pole lr8_ap1, lr8_ap2;

    void setCoeffs(double sampleRate, float freqHz) noexcept {
        lr2_lpA.setCoeffs(sampleRate, freqHz);
        lr2_lpB.setCoeffs(sampleRate, freqHz);
        lr2_hpA.setCoeffs(sampleRate, freqHz);
        lr2_hpB.setCoeffs(sampleRate, freqHz);
        lr2_ap.setCoeffs(sampleRate, freqHz);

        lr4_lpA.setCoeffs(sampleRate, freqHz, 0.70710678f);
        lr4_lpB.setCoeffs(sampleRate, freqHz, 0.70710678f);
        lr4_hpA.setCoeffs(sampleRate, freqHz, 0.70710678f);
        lr4_hpB.setCoeffs(sampleRate, freqHz, 0.70710678f);
        lr4_ap.setCoeffs(sampleRate, freqHz, 0.70710678f);

        lr8_lp1A.setCoeffs(sampleRate, freqHz, kButter4Q1);
        lr8_lp2A.setCoeffs(sampleRate, freqHz, kButter4Q2);
        lr8_lp1B.setCoeffs(sampleRate, freqHz, kButter4Q1);
        lr8_lp2B.setCoeffs(sampleRate, freqHz, kButter4Q2);

        lr8_hp1A.setCoeffs(sampleRate, freqHz, kButter4Q1);
        lr8_hp2A.setCoeffs(sampleRate, freqHz, kButter4Q2);
        lr8_hp1B.setCoeffs(sampleRate, freqHz, kButter4Q1);
        lr8_hp2B.setCoeffs(sampleRate, freqHz, kButter4Q2);

        lr8_ap1.setCoeffs(sampleRate, freqHz, kButter4Q1);
        lr8_ap2.setCoeffs(sampleRate, freqHz, kButter4Q2);
    }

    void reset() noexcept {
        lr2_lpA.reset(); lr2_lpB.reset(); lr2_hpA.reset(); lr2_hpB.reset(); lr2_ap.reset();
        lr4_lpA.reset(); lr4_lpB.reset(); lr4_hpA.reset(); lr4_hpB.reset(); lr4_ap.reset();
        lr8_lp1A.reset(); lr8_lp2A.reset(); lr8_lp1B.reset(); lr8_lp2B.reset();
        lr8_hp1A.reset(); lr8_hp2A.reset(); lr8_hp1B.reset(); lr8_hp2B.reset();
        lr8_ap1.reset(); lr8_ap2.reset();
    }

    inline void split(float x, CrossoverSlope slope, float& low, float& high) noexcept {
        switch (slope) {
            case CrossoverSlope::Slope12dB:
                low = lr2_lpB.processLowpass(lr2_lpA.processLowpass(x));
                // Invert HP2 polarity so LP2 + HP2 sums flat (AP1) without a notch at fc!
                high = -lr2_hpB.processHighpass(lr2_hpA.processHighpass(x));
                break;

            case CrossoverSlope::Slope24dB:
                low = lr4_lpB.processLowpass(lr4_lpA.processLowpass(x));
                high = lr4_hpB.processHighpass(lr4_hpA.processHighpass(x));
                break;

            case CrossoverSlope::Slope48dB: {
                const float l4 = lr8_lp2A.processLowpass(lr8_lp1A.processLowpass(x));
                low = lr8_lp2B.processLowpass(lr8_lp1B.processLowpass(l4));
                const float h4 = lr8_hp2A.processHighpass(lr8_hp1A.processHighpass(x));
                high = lr8_hp2B.processHighpass(lr8_hp1B.processHighpass(h4));
                break;
            }
        }
    }

    inline float allpass(float x, CrossoverSlope slope) noexcept {
        switch (slope) {
            case CrossoverSlope::Slope12dB:
                return lr2_ap.processAllpass(x);
            case CrossoverSlope::Slope24dB:
                return lr4_ap.processAllpass2ndOrder(x);
            case CrossoverSlope::Slope48dB:
                return lr8_ap2.processAllpass2ndOrder(lr8_ap1.processAllpass2ndOrder(x));
        }
        return x;
    }
};

/**
 * Symmetric Windowed-Sinc Linear-Phase FIR Crossover (255 taps, Blackman-Harris window).
 * Exact complementary subtraction guarantees Low + Mid + High === Delayed Input to < -120 dB
 * with ZERO phase shift across all frequencies!
 */
class LinearPhaseFirCrossoverChannel {
public:
    static constexpr int kNumTaps = 255;       // Odd length -> integer group delay = 127 samples
    static constexpr int kLatencySamples = (kNumTaps - 1) / 2; // 127 samples (~2.65 ms @ 48 kHz)
    static constexpr int kRingSize = 512;      // Power of 2 >= kNumTaps
    static constexpr int kRingMask = kRingSize - 1;

    void reset() noexcept {
        ring_.fill(0.0f);
        writePos_ = 0;
    }

    inline void processSample(
        float x,
        BandMode mode,
        const std::array<float, kNumTaps>& lpLowKernel,
        const std::array<float, kNumTaps>& lpHighKernel,
        float& outLow, float& outMid, float& outHigh) noexcept
    {
        ring_[writePos_] = x;

        const float delayedCenter = ring_[(writePos_ - kLatencySamples) & kRingMask];

        if (mode == BandMode::FullBand) {
            outLow = delayedCenter;
            outMid = 0.0f;
            outHigh = 0.0f;
            writePos_ = (writePos_ + 1) & kRingMask;
            return;
        }

        // Exploit even symmetry of Type-I linear phase FIR: h[k] == h[N - 1 - k] -> halves MAC count!
        constexpr int half = kLatencySamples;
        float sumLow = lpLowKernel[half] * delayedCenter;

        if (mode == BandMode::TwoBand) {
            for (int k = 0; k < half; ++k) {
                const float a = ring_[(writePos_ - k) & kRingMask];
                const float b = ring_[(writePos_ - (kNumTaps - 1 - k)) & kRingMask];
                sumLow += lpLowKernel[k] * (a + b);
            }
            outLow = sumLow;
            outMid = 0.0f;
            outHigh = delayedCenter - sumLow;
        } else {
            float sumHighLP = lpHighKernel[half] * delayedCenter;
            for (int k = 0; k < half; ++k) {
                const float s = ring_[(writePos_ - k) & kRingMask] +
                                ring_[(writePos_ - (kNumTaps - 1 - k)) & kRingMask];
                sumLow += lpLowKernel[k] * s;
                sumHighLP += lpHighKernel[k] * s;
            }
            outLow = sumLow;
            outMid = sumHighLP - sumLow;
            outHigh = delayedCenter - sumHighLP;
        }

        writePos_ = (writePos_ + 1) & kRingMask;
    }

private:
    std::array<float, kRingSize> ring_{};
    int writePos_ = 0;
};

/**
 * Stereo Multiband Crossover supporting both IIR Linkwitz-Riley (12/24/48 dB/oct)
 * and Linear-Phase FIR (zero phase distortion, exact complementary null).
 */
class MultibandCrossover {
public:
    static constexpr int kLinearPhaseLatency = LinearPhaseFirCrossoverChannel::kLatencySamples;

    void prepare(double sampleRate) noexcept {
        sampleRate_ = std::max(8000.0, sampleRate);
        lastLowFreq_ = -1.0f;
        lastHighFreq_ = -1.0f;
        lastSlope_ = CrossoverSlope::Slope24dB;
        setFrequencies(120.0f, 2500.0f, CrossoverSlope::Slope24dB);
        reset();
    }

    void reset() noexcept {
        for (int ch = 0; ch < 2; ++ch) {
            lowSplit_[ch].reset();
            highSplit_[ch].reset();
            firCh_[ch].reset();
        }
    }

    void setFrequencies(float lowCrossHz, float highCrossHz, CrossoverSlope slope) noexcept {
        const float nyqLimit = static_cast<float>(sampleRate_ * 0.46);
        lowCrossHz = std::clamp(lowCrossHz, 20.0f, std::min(4000.0f, nyqLimit * 0.5f));
        highCrossHz = std::clamp(highCrossHz, std::max(lowCrossHz + 40.0f, 120.0f), std::min(18000.0f, nyqLimit));

        const bool changed = (std::fabs(lowCrossHz - lastLowFreq_) > 0.25f) ||
                             (std::fabs(highCrossHz - lastHighFreq_) > 0.5f) ||
                             (slope != lastSlope_);
        if (!changed) return;

        lastLowFreq_ = lowCrossHz;
        lastHighFreq_ = highCrossHz;
        lastSlope_ = slope;

        for (int ch = 0; ch < 2; ++ch) {
            lowSplit_[ch].setCoeffs(sampleRate_, lowCrossHz);
            highSplit_[ch].setCoeffs(sampleRate_, highCrossHz);
        }

        buildLinearPhaseKernel(lowCrossHz, slope, firLowKernel_);
        buildLinearPhaseKernel(highCrossHz, slope, firHighKernel_);
    }

    int latencySamples(BandMode mode, bool linearPhase) const noexcept {
        if (linearPhase && mode != BandMode::FullBand) {
            return kLinearPhaseLatency;
        }
        return 0;
    }

    inline void processSample(
        int ch,
        float input,
        BandMode mode,
        CrossoverSlope slope,
        bool linearPhase,
        float& outLow,
        float& outMid,
        float& outHigh) noexcept
    {
        ch = std::clamp(ch, 0, 1);

        if (mode == BandMode::FullBand) {
            outLow = input;
            outMid = 0.0f;
            outHigh = 0.0f;
            return;
        }

        if (linearPhase) {
            firCh_[ch].processSample(input, mode, firLowKernel_, firHighKernel_, outLow, outMid, outHigh);
            return;
        }

        if (mode == BandMode::TwoBand) {
            lowSplit_[ch].split(input, slope, outLow, outHigh);
            outMid = 0.0f;
        } else {
            // 3-Band Allpass-Compensated Linkwitz-Riley Tree:
            // Step 1: Split input at fLow into Low_raw and MidHigh
            // Step 2: Split MidHigh at fHigh into Mid and High (their sum equals AP_high(MidHigh))
            // Step 3: Pass Low_raw through AP_high so Low + Mid + High = AP_high(Low_raw + MidHigh) = AP_high(AP_low(input)) -> 100% flat magnitude!
            float lowRaw = 0.0f, midHigh = 0.0f;
            lowSplit_[ch].split(input, slope, lowRaw, midHigh);
            outLow = highSplit_[ch].allpass(lowRaw, slope);
            highSplit_[ch].split(midHigh, slope, outMid, outHigh);
        }
    }

private:
    void buildLinearPhaseKernel(
        float cutoffHz,
        CrossoverSlope slope,
        std::array<float, LinearPhaseFirCrossoverChannel::kNumTaps>& kernel) noexcept
    {
        constexpr int N = LinearPhaseFirCrossoverChannel::kNumTaps;
        constexpr int M = N - 1;
        const double fcNorm = std::clamp(static_cast<double>(cutoffHz) / sampleRate_, 0.001, 0.48);

        // Adjust window steepness cleanly for 12/24/48 dB feel while preserving zero-phase symmetry
        const double windowAlpha = (slope == CrossoverSlope::Slope12dB) ? 0.35 :
                                   (slope == CrossoverSlope::Slope24dB) ? 0.68 : 1.0;
        // Effective active span narrows slightly for gentler transition band on 12/24 dB slopes
        const int activeHalfSpan = (slope == CrossoverSlope::Slope12dB) ? 40 :
                                   (slope == CrossoverSlope::Slope24dB) ? 80 : (M / 2);

        double sum = 0.0;
        const int center = M / 2;
        kernel.fill(0.0f);

        for (int n = center - activeHalfSpan; n <= center + activeHalfSpan; ++n) {
            const int d = n - center;
            double sinc = 2.0 * fcNorm;
            if (d != 0) {
                const double x = kTwoPi * fcNorm * static_cast<double>(d);
                sinc = std::sin(x) / (kPi * static_cast<double>(d));
            }
            const double wPos = static_cast<double>(n - (center - activeHalfSpan)) /
                                static_cast<double>(2 * activeHalfSpan);
            // 4-term Blackman-Harris window (>92 dB stopband attenuation) blended with Hann for gentle slopes
            const double bh = 0.35875
                            - 0.48829 * std::cos(kTwoPi * wPos)
                            + 0.14128 * std::cos(2.0 * kTwoPi * wPos)
                            - 0.01168 * std::cos(3.0 * kTwoPi * wPos);
            const double hann = 0.5 - 0.5 * std::cos(kTwoPi * wPos);
            const double win = windowAlpha * bh + (1.0 - windowAlpha) * hann;
            const float val = static_cast<float>(sinc * win);
            kernel[static_cast<std::size_t>(n)] = val;
            sum += static_cast<double>(val);
        }

        if (std::fabs(sum) > 1.0e-12) {
            const float invSum = static_cast<float>(1.0 / sum);
            for (auto& v : kernel) v *= invSum;
        }
    }

    double sampleRate_ = 48000.0;
    float lastLowFreq_ = -1.0f;
    float lastHighFreq_ = -1.0f;
    CrossoverSlope lastSlope_ = CrossoverSlope::Slope24dB;

    std::array<LinkwitzRileySplit, 2> lowSplit_{};
    std::array<LinkwitzRileySplit, 2> highSplit_{};
    std::array<LinearPhaseFirCrossoverChannel, 2> firCh_{};
    std::array<float, LinearPhaseFirCrossoverChannel::kNumTaps> firLowKernel_{};
    std::array<float, LinearPhaseFirCrossoverChannel::kNumTaps> firHighKernel_{};
};

} // namespace duckflow
