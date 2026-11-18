#pragma once

#include "SimdAndMath.h"
#include "MultiSegmentEnvelope.h"
#include "MultibandCrossover.h"
#include "TriggerDetector.h"
#include "EnvelopeTracer.h"
#include "SyncGroupHub.h"
#include "FactoryPresets.h"

#include <array>
#include <vector>
#include <memory>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace duckflow {

struct RealtimeEnvelopeBank {
    static constexpr int kMaxNodes = 128;

    struct PrecomputedNode {
        float x = 0.0f;
        float y = 1.0f;
        float tension = 0.0f;
        float bendPower = 1.0f; // Precomputed pow(10, |k| * scale) to avoid pow(10, ...) per audio sample!
        int wobbleCycles = 1;
        CurveType curve = CurveType::SCurveSpline;
    };

    struct FixedEnv {
        std::array<PrecomputedNode, kMaxNodes> nodes{};
        int count = 2;
        float loopStart = 0.25f;
        float loopEnd = 0.75f;

        void fromEnvelopeData(const EnvelopeData& src) noexcept {
            count = static_cast<int>(std::min<std::size_t>(src.nodes.size(), kMaxNodes));
            for (int i = 0; i < count; ++i) {
                const auto& sn = src.nodes[static_cast<std::size_t>(i)];
                auto& dn = nodes[static_cast<std::size_t>(i)];
                dn.x = sn.x;
                dn.y = sn.y;
                dn.tension = sn.tension;
                dn.curve = sn.curve;
                if (sn.curve == CurveType::Tension2D) {
                    const float k = std::clamp(sn.tension, -0.98f, 0.98f);
                    dn.bendPower = (std::fabs(k) <= 1.0e-4f) ? 1.0f : std::pow(10.0f, std::fabs(k) * 1.15f);
                } else if (sn.curve == CurveType::SCurveSpline) {
                    const float k = std::clamp(sn.tension, -0.95f, 0.95f);
                    dn.bendPower = (std::fabs(k) <= 1.0e-4f) ? 1.0f : std::pow(10.0f, std::fabs(k) * 0.65f);
                } else {
                    dn.bendPower = 1.0f;
                }
                dn.wobbleCycles = 1 + static_cast<int>(std::round((sn.tension * 0.5f + 0.5f) * 7.0f));
            }
            if (count >= 2) {
                nodes[0].x = 0.0f;
                nodes[static_cast<std::size_t>(count - 1)].x = 1.0f;
            }
            loopStart = src.loopStart;
            loopEnd = src.loopEnd;
        }

        inline float evaluate(float rawPhase, float swing, bool exactEndInOneShot) const noexcept {
            if (count <= 0) return 1.0f;
            if (count == 1) return nodes[0].y;
            if (exactEndInOneShot && rawPhase >= 1.0f) {
                return nodes[static_cast<std::size_t>(count - 1)].y;
            }
            const float x = EnvelopeData::applySwing(std::clamp(rawPhase, 0.0f, 1.0f), swing);
            if (x <= nodes[0].x) return nodes[0].y;
            if (x >= nodes[static_cast<std::size_t>(count - 1)].x) {
                return nodes[static_cast<std::size_t>(count - 1)].y;
            }

            int idx = 0;
            for (int i = 0; i + 1 < count; ++i) {
                const auto ui = static_cast<std::size_t>(i);
                if (x >= nodes[ui].x && x <= nodes[ui + 1].x) {
                    idx = i;
                    if (nodes[ui + 1].x > x) break;
                }
            }
            const auto& n0 = nodes[static_cast<std::size_t>(idx)];
            const auto& n1 = nodes[static_cast<std::size_t>(idx + 1)];
            const float dx = n1.x - n0.x;
            if (dx <= 1.0e-6f) return n1.y;
            const float t = std::clamp((x - n0.x) / dx, 0.0f, 1.0f);

            switch (n0.curve) {
                case CurveType::Linear:
                    return n0.y + (n1.y - n0.y) * t;
                case CurveType::StepHold:
                    return (t < 1.0f) ? n0.y : n1.y;
                case CurveType::Tension2D: {
                    float shaped = t;
                    if (n0.bendPower != 1.0f) {
                        shaped = (n0.tension >= 0.0f) ? std::pow(t, n0.bendPower)
                                                      : (1.0f - std::pow(1.0f - t, n0.bendPower));
                    }
                    return std::clamp(n0.y + (n1.y - n0.y) * shaped, 0.0f, 1.0f);
                }
                case CurveType::SCurveSpline: {
                    float tb = t;
                    if (n0.bendPower != 1.0f) {
                        tb = (n0.tension >= 0.0f) ? std::pow(t, n0.bendPower)
                                                  : (1.0f - std::pow(1.0f - t, n0.bendPower));
                    }
                    const float s = tb * tb * tb * (tb * (tb * 6.0f - 15.0f) + 10.0f);
                    return std::clamp(n0.y + (n1.y - n0.y) * s, 0.0f, 1.0f);
                }
                default:
                    return std::clamp(
                        EnvelopeData::evalSegment(t, n0.y, n1.y, n0.curve, n0.tension),
                        0.0f, 1.0f);
            }
        }
    };

    std::array<FixedEnv, 3> bands{};
};

struct DuckParameters {
    bool bypass = false;
    TriggerMode triggerMode = TriggerMode::RepeatSync;
    BeatDivision beatDivision = BeatDivision::Div_1_4;
    float freeHz = 4.0f;
    float globalDepth = 1.0f;
    float swing = 0.0f;
    float smoothingMs = 4.5f;
    float offsetMs = 0.0f;
    bool offsetInSamples = false;
    int offsetSamples = 0;

    BandMode bandMode = BandMode::FullBand;
    bool unlinkBands = false;
    bool linearPhase = false;
    CrossoverSlope crossoverSlope = CrossoverSlope::Slope24dB;
    float lowCrossHz = 140.0f;
    float highCrossHz = 2600.0f;
    float lowAmount = 1.0f;
    float midAmount = 0.7f;
    float highAmount = 0.3f;
    int bandSolo = 0;

    float thresholdDb = -24.0f;
    float holdMs = 35.0f;
    float detectorHpfHz = 30.0f;
    float detectorLpfHz = 16000.0f;
    bool detectorFilterOn = true;
    bool sidechainListen = false;
    float midiVelocitySens = 0.75f;
    EnvelopeTriggerLoopMode loopMode = EnvelopeTriggerLoopMode::OneShot;

    float outputGainDb = 0.0f;
    float dryWet = 1.0f;
    int syncGroup = 0;
    bool lockTriggerSettings = false;
};

struct HostTransportInfo {
    bool isPlaying = false;
    double bpm = 120.0;
    double ppqPosition = 0.0;
    int timeSigNumerator = 4;
    int timeSigDenominator = 4;
};

class SpectrumAnalyzerFifo {
public:
    static constexpr int kFftSize = 1024;
    static constexpr int kHalfBins = kFftSize / 2;

    SpectrumAnalyzerFifo()
        : fifo_(kFftSize, 0.0f),
          latestFrame_(kFftSize, 0.0f) {}

    void reset() noexcept {
        std::fill(fifo_.begin(), fifo_.end(), 0.0f);
        std::fill(latestFrame_.begin(), latestFrame_.end(), 0.0f);
        writeIdx_ = 0;
        frameReady_.store(false, std::memory_order_relaxed);
    }

    inline void pushSample(float monoSample) noexcept {
        fifo_[static_cast<std::size_t>(writeIdx_)] = monoSample;
        if (++writeIdx_ >= kFftSize) {
            writeIdx_ = 0;
            if (!frameReady_.load(std::memory_order_relaxed)) {
                std::copy(fifo_.begin(), fifo_.end(), latestFrame_.begin());
                frameReady_.store(true, std::memory_order_release);
            }
        }
    }

    bool computeMagnitudesDb(std::array<float, kHalfBins>& outDb) noexcept {
        if (!frameReady_.load(std::memory_order_acquire)) {
            return false;
        }
        std::array<float, kFftSize> re{};
        std::array<float, kFftSize> im{};
        for (int n = 0; n < kFftSize; ++n) {
            const float hann = 0.5f * (1.0f - std::cos(static_cast<float>(kTwoPi) * static_cast<float>(n) / static_cast<float>(kFftSize - 1)));
            re[static_cast<std::size_t>(n)] = latestFrame_[static_cast<std::size_t>(n)] * hann;
        }
        frameReady_.store(false, std::memory_order_release);

        int j = 0;
        for (int i = 1; i < kFftSize; ++i) {
            int bit = kFftSize >> 1;
            while (j & bit) {
                j ^= bit;
                bit >>= 1;
            }
            j ^= bit;
            if (i < j) {
                std::swap(re[static_cast<std::size_t>(i)], re[static_cast<std::size_t>(j)]);
            }
        }

        for (int len = 2; len <= kFftSize; len <<= 1) {
            const double ang = -kTwoPi / static_cast<double>(len);
            const float wlenRe = static_cast<float>(std::cos(ang));
            const float wlenIm = static_cast<float>(std::sin(ang));
            for (int i = 0; i < kFftSize; i += len) {
                float wRe = 1.0f;
                float wIm = 0.0f;
                const int half = len >> 1;
                for (int k = 0; k < half; ++k) {
                    const auto uIdx = static_cast<std::size_t>(i + k);
                    const auto vIdx = static_cast<std::size_t>(i + k + half);
                    const float uRe = re[uIdx];
                    const float uIm = im[uIdx];
                    const float vRe = re[vIdx] * wRe - im[vIdx] * wIm;
                    const float vIm = re[vIdx] * wIm + im[vIdx] * wRe;
                    re[uIdx] = uRe + vRe;
                    im[uIdx] = uIm + vIm;
                    re[vIdx] = uRe - vRe;
                    im[vIdx] = uIm - vIm;
                    const float nextWRe = wRe * wlenRe - wIm * wlenIm;
                    wIm = wRe * wlenIm + wIm * wlenRe;
                    wRe = nextWRe;
                }
            }
        }

        const float norm = 2.0f / static_cast<float>(kFftSize);
        for (int b = 0; b < kHalfBins; ++b) {
            const auto ub = static_cast<std::size_t>(b);
            const float mag = std::sqrt(re[ub] * re[ub] + im[ub] * im[ub]) * norm;
            outDb[ub] = std::clamp(gainToDb(mag), -90.0f, 6.0f);
        }
        return true;
    }

private:
    std::vector<float> fifo_;
    std::vector<float> latestFrame_;
    int writeIdx_ = 0;
    std::atomic<bool> frameReady_{false};
};

struct OscilloscopeSnapshot {
    static constexpr int kPoints = 512;
    std::array<float, kPoints> prePeak{};
    std::array<float, kPoints> postPeak{};
    std::array<float, kPoints> sidechainPeak{};
    std::array<float, kPoints> gainCurve{};
    int writeHead = 0;
};

class DuckFlowEngine {
public:
    static constexpr int kDelayRingSize = 65536;
    static constexpr int kDelayRingMask = kDelayRingSize - 1;
    static constexpr int kMaxTraceSamples = 192000;

    DuckFlowEngine()
        : envDoubleBuffer_(std::make_unique<std::array<RealtimeEnvelopeBank, 2>>()),
          traceCaptureBuffer_(kMaxTraceSamples, 0.0f)
    {
        for (int ch = 0; ch < 2; ++ch) {
            for (int b = 0; b < 3; ++b) {
                bandLookaheadRing_[ch][b].assign(kDelayRingSize, 0.0f);
            }
            dryCompensationRing_[ch].assign(kDelayRingSize, 0.0f);
            triggerAlignRing_[ch].assign(kDelayRingSize, 0.0f);
            scScopeAlignRing_[ch].assign(kDelayRingSize, 0.0f);
        }
        EnvelopeData def;
        publishEnvelopes({def, def, def});
    }

    void prepare(double sampleRate, int /*maxBlockSize*/) {
        sampleRate_ = std::clamp(sampleRate, 8000.0, 384000.0);
        crossover_.prepare(sampleRate_);
        detector_.prepare(sampleRate_);
        for (auto& s : bandSmoothers_) {
            s.prepare(sampleRate_, params_.smoothingMs);
        }
        grMeterReleaseCoeff_ = static_cast<float>(1.0 - std::exp(-1.0 / (0.080 * sampleRate_)));
        spectrumFifo_.reset();
        reset();
    }

    void reset() noexcept {
        crossover_.reset();
        detector_.reset();
        for (auto& s : bandSmoothers_) {
            s.reset(1.0f);
        }
        grMeterBallisticLin_ = {1.0f, 1.0f, 1.0f};
        for (int ch = 0; ch < 2; ++ch) {
            for (int b = 0; b < 3; ++b) {
                std::fill(bandLookaheadRing_[ch][b].begin(), bandLookaheadRing_[ch][b].end(), 0.0f);
            }
            std::fill(dryCompensationRing_[ch].begin(), dryCompensationRing_[ch].end(), 0.0f);
            std::fill(triggerAlignRing_[ch].begin(), triggerAlignRing_[ch].end(), 0.0f);
            std::fill(scScopeAlignRing_[ch].begin(), scScopeAlignRing_[ch].end(), 0.0f);
        }
        delayWritePos_ = 0;

        freeRunningPhase_ = 0.0;
        triggeredPhase_ = 1.0;
        triggeredActive_ = false;
        triggerGateHeld_ = false;
        currentVelocityDepthScale_ = 1.0f;

        scopeDecimCounter_ = 0;
        scopeWriteIdx_ = 0;
        scopePreAcc_ = 0.0f;
        scopePostAcc_ = 0.0f;
        scopeScAcc_ = 0.0f;

        isTracingLive_.store(false, std::memory_order_relaxed);
        traceWritePos_ = 0;
    }

    void setParameters(const DuckParameters& p) noexcept { params_ = p; }
    const DuckParameters& getParameters() const noexcept { return params_; }

    void publishEnvelopes(const std::array<EnvelopeData, 3>& envs) noexcept {
        const int nextSlot = 1 - activeEnvSlot_.load(std::memory_order_relaxed);
        for (std::size_t b = 0; b < 3; ++b) {
            (*envDoubleBuffer_)[static_cast<std::size_t>(nextSlot)].bands[b].fromEnvelopeData(envs[b]);
        }
        activeEnvSlot_.store(nextSlot, std::memory_order_release);
    }

    int getReportedLatencySamples() const noexcept {
        const int firLat = crossover_.latencySamples(params_.bandMode, params_.linearPhase);
        int lookaheadLat = 0;
        if (isTriggeredMode(params_.triggerMode)) {
            const int offSamp = computeOffsetSamples();
            if (offSamp > 0) lookaheadLat = offSamp;
        }
        return std::clamp(firLat + lookaheadLat, 0, kDelayRingSize - 512);
    }

    void startLiveTrace(int durationSamples) noexcept {
        traceTargetSamples_ = std::clamp(durationSamples, 512, kMaxTraceSamples);
        traceWritePos_ = 0;
        traceReady_.store(false, std::memory_order_relaxed);
        isTracingLive_.store(true, std::memory_order_release);
    }

    bool pollCompletedLiveTrace(EnvelopeData& outTraced, bool invertForDucking = true) {
        if (!traceReady_.load(std::memory_order_acquire)) return false;
        outTraced = EnvelopeTracer::traceFromMonoBuffer(
            traceCaptureBuffer_.data(), static_cast<std::size_t>(traceTargetSamples_), invertForDucking);
        traceReady_.store(false, std::memory_order_release);
        return true;
    }

    void process(
        float* mainL,
        float* mainR,
        const float* scL,
        const float* scR,
        int numSamples,
        const HostTransportInfo& transport,
        const MidiTriggerEvent* midiEvents = nullptr,
        int numMidiEvents = 0) noexcept
    {
        if (mainL == nullptr || numSamples <= 0) return;

        ScopedDenormalFlush denormalGuard;

        const bool hasRightMain = (mainR != nullptr && mainR != mainL);
        const bool hasScL = (scL != nullptr);
        const bool hasScR = (scR != nullptr);
        const bool isFullBand = (params_.bandMode == BandMode::FullBand);

        if (!isFullBand) {
            crossover_.setFrequencies(params_.lowCrossHz, params_.highCrossHz, params_.crossoverSlope);
        }
        detector_.setFilter(params_.detectorHpfHz, params_.detectorLpfHz);
        for (auto& sm : bandSmoothers_) {
            sm.setTimeMs(params_.smoothingMs);
        }

        const int firLat = crossover_.latencySamples(params_.bandMode, params_.linearPhase);
        const int rawOffsetSamples = computeOffsetSamples();
        const bool triggeredMode = isTriggeredMode(params_.triggerMode);

        const int lookaheadSamples = (triggeredMode && rawOffsetSamples > 0)
                                   ? std::min(rawOffsetSamples, kDelayRingSize - 1024) : 0;
        const int extraTriggerDelay = (triggeredMode && rawOffsetSamples < 0)
                                    ? std::min(-rawOffsetSamples, kDelayRingSize - 1024) : 0;
        const int totalMainLatency = std::min(firLat + lookaheadSamples, kDelayRingSize - 512);
        const int triggerDelaySamples = std::min(firLat + extraTriggerDelay, kDelayRingSize - 512);

        const auto& envBank = (*envDoubleBuffer_)[static_cast<std::size_t>(activeEnvSlot_.load(std::memory_order_acquire))];

        const double safeBpm = std::clamp(transport.bpm, 20.0, 400.0);
        const double quarterNotesPerCycle = beatDivisionToQuarterNotes(params_.beatDivision);
        const double cycleDurationSamples = (params_.triggerMode == TriggerMode::FreeHz)
            ? (sampleRate_ / static_cast<double>(std::clamp(params_.freeHz, 0.05f, 50.0f)))
            : ((60.0 / safeBpm) * quarterNotesPerCycle * sampleRate_);
        const double phaseIncPerSample = 1.0 / std::max(1.0, cycleDurationSamples);

        const float outGainLin = dbToGain(params_.outputGainDb);
        const float wetMix = std::clamp(params_.dryWet, 0.0f, 1.0f);
        const float dryMix = 1.0f - wetMix;
        const int scopeDecim = std::max(16, static_cast<int>(cycleDurationSamples / 128.0));

        int midiIdx = 0;
        bool blockTriggeredLed = false;

        for (int i = 0; i < numSamples; ++i) {
            const float inL = sanitizeSample(mainL[i]);
            const float inR = hasRightMain ? sanitizeSample(mainR[i]) : inL;

            const float rawScL = hasScL ? sanitizeSample(scL[i]) : 0.0f;
            const float rawScR = hasScR ? sanitizeSample(scR[i]) : rawScL;

            spectrumFifo_.pushSample(0.5f * (inL + inR));

            if (isTracingLive_.load(std::memory_order_relaxed)) {
                const float traceSrc = (params_.triggerMode == TriggerMode::SidechainAudio && hasScL)
                                     ? (0.5f * (rawScL + rawScR)) : (0.5f * (inL + inR));
                if (traceWritePos_ < traceTargetSamples_) {
                    traceCaptureBuffer_[static_cast<std::size_t>(traceWritePos_++)] = traceSrc;
                }
                if (traceWritePos_ >= traceTargetSamples_) {
                    isTracingLive_.store(false, std::memory_order_relaxed);
                    traceReady_.store(true, std::memory_order_release);
                }
            }

            const auto wIdx = static_cast<std::size_t>(delayWritePos_);
            dryCompensationRing_[0][wIdx] = inL;
            dryCompensationRing_[1][wIdx] = inR;
            scScopeAlignRing_[0][wIdx] = rawScL;
            scScopeAlignRing_[1][wIdx] = rawScR;

            const float trigSrcL = (params_.triggerMode == TriggerMode::SidechainAudio) ? rawScL : inL;
            const float trigSrcR = (params_.triggerMode == TriggerMode::SidechainAudio) ? rawScR : inR;
            triggerAlignRing_[0][wIdx] = trigSrcL;
            triggerAlignRing_[1][wIdx] = trigSrcR;

            const auto dryReadIdx = static_cast<std::size_t>((delayWritePos_ - totalMainLatency) & kDelayRingMask);
            const float alignedDryL = dryCompensationRing_[0][dryReadIdx];
            const float alignedDryR = dryCompensationRing_[1][dryReadIdx];
            const float alignedScScope = 0.5f * (scScopeAlignRing_[0][dryReadIdx] + scScopeAlignRing_[1][dryReadIdx]);

            float bandL[3]{}, bandR[3]{};
            const auto lookReadIdx = static_cast<std::size_t>((delayWritePos_ - lookaheadSamples) & kDelayRingMask);

            if (isFullBand) {
                // Ultra-fast FullBand fast path (skips multiband crossover & bands 1..2 ring writes!)
                bandLookaheadRing_[0][0][wIdx] = inL;
                bandLookaheadRing_[1][0][wIdx] = inR;
                bandL[0] = bandLookaheadRing_[0][0][lookReadIdx];
                bandR[0] = bandLookaheadRing_[1][0][lookReadIdx];
            } else {
                float splitLow[2]{}, splitMid[2]{}, splitHigh[2]{};
                crossover_.processSample(0, inL, params_.bandMode, params_.crossoverSlope, params_.linearPhase,
                                         splitLow[0], splitMid[0], splitHigh[0]);
                if (hasRightMain) {
                    crossover_.processSample(1, inR, params_.bandMode, params_.crossoverSlope, params_.linearPhase,
                                             splitLow[1], splitMid[1], splitHigh[1]);
                } else {
                    splitLow[1] = splitLow[0];
                    splitMid[1] = splitMid[0];
                    splitHigh[1] = splitHigh[0];
                }
                const float curSplitsL[3] = {splitLow[0], splitMid[0], splitHigh[0]};
                const float curSplitsR[3] = {splitLow[1], splitMid[1], splitHigh[1]};
                for (int b = 0; b < 3; ++b) {
                    bandLookaheadRing_[0][b][wIdx] = curSplitsL[b];
                    bandLookaheadRing_[1][b][wIdx] = curSplitsR[b];
                    bandL[b] = bandLookaheadRing_[0][b][lookReadIdx];
                    bandR[b] = bandLookaheadRing_[1][b][lookReadIdx];
                }
            }

            const auto trigReadIdx = static_cast<std::size_t>((delayWritePos_ - triggerDelaySamples) & kDelayRingMask);
            const auto audioTrig = detector_.processAudioSample(
                triggerAlignRing_[0][trigReadIdx], triggerAlignRing_[1][trigReadIdx],
                params_.thresholdDb, params_.holdMs, params_.detectorFilterOn);
            detectorMeterLinear_.store(audioTrig.detectorLinear, std::memory_order_relaxed);

            bool midiRetrig = false;
            while (midiEvents != nullptr && midiIdx < numMidiEvents && midiEvents[midiIdx].sampleOffset <= i) {
                const auto mRes = detector_.handleMidiEvent(midiEvents[midiIdx], params_.midiVelocitySens);
                if (mRes.retriggered) {
                    midiRetrig = true;
                    currentVelocityDepthScale_ = mRes.triggerVelocityGain;
                }
                triggerGateHeld_ = mRes.gateHeld;
                ++midiIdx;
            }

            float evalPhase = 0.0f;
            bool exactOneShotEnd = false;

            if (params_.triggerMode == TriggerMode::RepeatSync) {
                if (transport.isPlaying) {
                    const double samplePpq = transport.ppqPosition +
                        (static_cast<double>(i - firLat) / sampleRate_) * (safeBpm / 60.0);
                    const double phaseShift = static_cast<double>(rawOffsetSamples) * phaseIncPerSample;
                    double cyc = std::fmod((samplePpq / quarterNotesPerCycle) + phaseShift, 1.0);
                    if (cyc < 0.0) cyc += 1.0;
                    evalPhase = static_cast<float>(cyc);
                    freeRunningPhase_ = cyc;
                } else {
                    double cyc = std::fmod(freeRunningPhase_ + static_cast<double>(rawOffsetSamples) * phaseIncPerSample, 1.0);
                    if (cyc < 0.0) cyc += 1.0;
                    evalPhase = static_cast<float>(cyc);
                    freeRunningPhase_ = std::fmod(freeRunningPhase_ + phaseIncPerSample, 1.0);
                }
                currentVelocityDepthScale_ = 1.0f;
            } else if (params_.triggerMode == TriggerMode::FreeHz) {
                double cyc = std::fmod(freeRunningPhase_ + static_cast<double>(rawOffsetSamples) * phaseIncPerSample, 1.0);
                if (cyc < 0.0) cyc += 1.0;
                evalPhase = static_cast<float>(cyc);
                freeRunningPhase_ = std::fmod(freeRunningPhase_ + phaseIncPerSample, 1.0);
                currentVelocityDepthScale_ = 1.0f;
            } else {
                bool retrigNow = false;
                if (params_.triggerMode == TriggerMode::MidiTrigger) {
                    retrigNow = midiRetrig;
                    triggerGateHeld_ = detector_.isMidiGateHeld();
                } else {
                    retrigNow = audioTrig.retriggered;
                    triggerGateHeld_ = audioTrig.gateHeld;
                    currentVelocityDepthScale_ = 1.0f;
                }

                if (retrigNow) {
                    triggeredPhase_ = 0.0;
                    triggeredActive_ = true;
                    blockTriggeredLed = true;
                }

                if (!triggeredActive_) {
                    evalPhase = 1.0f;
                    exactOneShotEnd = true;
                } else {
                    evalPhase = static_cast<float>(std::clamp(triggeredPhase_, 0.0, 1.0));
                    exactOneShotEnd = (params_.loopMode == EnvelopeTriggerLoopMode::OneShot && triggeredPhase_ >= 1.0);

                    const double nextP = triggeredPhase_ + phaseIncPerSample;
                    const float ls = envBank.bands[0].loopStart;
                    const float le = std::max(ls + 0.02f, envBank.bands[0].loopEnd);

                    if (params_.loopMode == EnvelopeTriggerLoopMode::OneShot) {
                        if (nextP >= 1.0) { triggeredPhase_ = 1.0; triggeredActive_ = false; }
                        else triggeredPhase_ = nextP;
                    } else if (params_.loopMode == EnvelopeTriggerLoopMode::Loop) {
                        if (triggerGateHeld_) triggeredPhase_ = std::fmod(nextP, 1.0);
                        else if (nextP >= 1.0) { triggeredPhase_ = 1.0; triggeredActive_ = false; }
                        else triggeredPhase_ = nextP;
                    } else {
                        if (triggerGateHeld_ && triggeredPhase_ < static_cast<double>(le)) {
                            if (nextP >= static_cast<double>(le)) {
                                triggeredPhase_ = static_cast<double>(ls) +
                                    std::fmod(nextP - static_cast<double>(ls), static_cast<double>(le - ls));
                            } else triggeredPhase_ = nextP;
                        } else {
                            if (nextP >= 1.0) { triggeredPhase_ = 1.0; triggeredActive_ = false; }
                            else triggeredPhase_ = nextP;
                        }
                    }
                }
            }

            playheadPhase_.store(evalPhase, std::memory_order_relaxed);

            const float effGlobalDepth = std::clamp(params_.globalDepth * currentVelocityDepthScale_, 0.0f, 1.0f);
            float smoothedGain[3]{1.0f, 1.0f, 1.0f};

            if (isFullBand) {
                // Evaluate & smooth only 1 curve in FullBand mode (65% CPU reduction!)
                const float rawCurveY = envBank.bands[0].evaluate(evalPhase, params_.swing, exactOneShotEnd);
                const float targetGain = 1.0f - effGlobalDepth * (1.0f - rawCurveY);
                const float g0 = bandSmoothers_[0].process(targetGain);
                smoothedGain[0] = g0;
                smoothedGain[1] = g0;
                smoothedGain[2] = g0;
            } else {
                const float bandAmts[3] = {
                    std::clamp(params_.lowAmount, 0.0f, 1.0f),
                    std::clamp(params_.midAmount, 0.0f, 1.0f),
                    std::clamp(params_.highAmount, 0.0f, 1.0f)
                };
                const float sharedCurve0 = envBank.bands[0].evaluate(evalPhase, params_.swing, exactOneShotEnd);
                for (int b = 0; b < 3; ++b) {
                    const float rawCurveY = (params_.unlinkBands && b > 0)
                        ? envBank.bands[static_cast<std::size_t>(b)].evaluate(evalPhase, params_.swing, exactOneShotEnd)
                        : sharedCurve0;
                    const float amt = (params_.bandMode == BandMode::TwoBand)
                        ? ((b == 0) ? bandAmts[0] : bandAmts[2])
                        : bandAmts[b];
                    const float targetGain = 1.0f - (effGlobalDepth * amt) * (1.0f - rawCurveY);
                    smoothedGain[b] = bandSmoothers_[static_cast<std::size_t>(b)].process(targetGain);
                }
            }

            // Sample-accurate ballistic GR meter (immune to FL Studio 1-sample micro-buffer jitter!)
            for (int b = 0; b < 3; ++b) {
                const auto ub = static_cast<std::size_t>(b);
                if (smoothedGain[b] < grMeterBallisticLin_[ub]) {
                    grMeterBallisticLin_[ub] = smoothedGain[b];
                } else {
                    grMeterBallisticLin_[ub] += grMeterReleaseCoeff_ * (smoothedGain[b] - grMeterBallisticLin_[ub]);
                }
            }

            float wetL = 0.0f, wetR = 0.0f;
            if (params_.sidechainListen) {
                wetL = audioTrig.filteredL;
                wetR = audioTrig.filteredR;
            } else if (isFullBand) {
                wetL = bandL[0] * smoothedGain[0];
                wetR = bandR[0] * smoothedGain[0];
            } else if (params_.bandMode == BandMode::TwoBand) {
                if (params_.bandSolo == 1) {
                    wetL = bandL[0] * smoothedGain[0];
                    wetR = bandR[0] * smoothedGain[0];
                } else if (params_.bandSolo == 3) {
                    wetL = bandL[2] * smoothedGain[2];
                    wetR = bandR[2] * smoothedGain[2];
                } else {
                    wetL = bandL[0] * smoothedGain[0] + bandL[2] * smoothedGain[2];
                    wetR = bandR[0] * smoothedGain[0] + bandR[2] * smoothedGain[2];
                }
            } else {
                if (params_.bandSolo >= 1 && params_.bandSolo <= 3) {
                    const int sb = params_.bandSolo - 1;
                    wetL = bandL[sb] * smoothedGain[sb];
                    wetR = bandR[sb] * smoothedGain[sb];
                } else {
                    wetL = bandL[0] * smoothedGain[0] + bandL[1] * smoothedGain[1] + bandL[2] * smoothedGain[2];
                    wetR = bandR[0] * smoothedGain[0] + bandR[1] * smoothedGain[1] + bandR[2] * smoothedGain[2];
                }
            }

            float outL = alignedDryL, outR = alignedDryR;
            if (!params_.bypass) {
                outL = (wetL * wetMix + alignedDryL * dryMix) * outGainLin;
                outR = (wetR * wetMix + alignedDryR * dryMix) * outGainLin;
            }

            outL = sanitizeSample(outL);
            outR = sanitizeSample(outR);
            mainL[i] = outL;
            if (hasRightMain) mainR[i] = outR;

            scopePreAcc_  = std::max(scopePreAcc_,  0.5f * (std::fabs(alignedDryL) + std::fabs(alignedDryR)));
            scopePostAcc_ = std::max(scopePostAcc_, 0.5f * (std::fabs(outL) + std::fabs(outR)));
            scopeScAcc_   = std::max(scopeScAcc_,   std::fabs(alignedScScope));

            if (++scopeDecimCounter_ >= scopeDecim) {
                scopeDecimCounter_ = 0;
                const auto sw = static_cast<std::size_t>(scopeWriteIdx_);
                scopeData_.prePeak[sw] = std::min(scopePreAcc_, 1.5f);
                scopeData_.postPeak[sw] = std::min(scopePostAcc_, 1.5f);
                scopeData_.sidechainPeak[sw] = std::min(scopeScAcc_, 1.5f);
                scopeData_.gainCurve[sw] = smoothedGain[0];
                scopeWriteIdx_ = (scopeWriteIdx_ + 1) % OscilloscopeSnapshot::kPoints;
                scopeData_.writeHead = scopeWriteIdx_;
                scopePreAcc_ = 0.0f;
                scopePostAcc_ = 0.0f;
                scopeScAcc_ = 0.0f;
            }

            delayWritePos_ = (delayWritePos_ + 1) & kDelayRingMask;
        }

        for (int b = 0; b < 3; ++b) {
            bandGainReductionDb_[static_cast<std::size_t>(b)].store(
                gainToDb(grMeterBallisticLin_[static_cast<std::size_t>(b)]), std::memory_order_relaxed);
        }
        if (blockTriggeredLed) {
            triggerLedFlag_.store(true, std::memory_order_relaxed);
        }
    }

    float getPlayheadPhase() const noexcept { return playheadPhase_.load(std::memory_order_relaxed); }
    float getBandGainReductionDb(int band) const noexcept {
        return bandGainReductionDb_[static_cast<std::size_t>(std::clamp(band, 0, 2))].load(std::memory_order_relaxed);
    }
    float getDetectorLevelDb() const noexcept {
        return gainToDb(detectorMeterLinear_.load(std::memory_order_relaxed));
    }
    bool consumeTriggerLedEvent() noexcept {
        return triggerLedFlag_.exchange(false, std::memory_order_relaxed);
    }
    SpectrumAnalyzerFifo& getSpectrumAnalyzer() noexcept { return spectrumFifo_; }
    OscilloscopeSnapshot getOscilloscopeSnapshot() const noexcept { return scopeData_; }

private:
    static bool isTriggeredMode(TriggerMode m) noexcept {
        return m == TriggerMode::MidiTrigger ||
               m == TriggerMode::SidechainAudio ||
               m == TriggerMode::MainAudio;
    }

    int computeOffsetSamples() const noexcept {
        if (params_.offsetInSamples) {
            return std::clamp(params_.offsetSamples, -38400, 38400);
        }
        const float clampedMs = std::clamp(params_.offsetMs, -200.0f, 200.0f);
        return static_cast<int>(std::llround(static_cast<double>(clampedMs) * 0.001 * sampleRate_));
    }

    double sampleRate_ = 48000.0;
    DuckParameters params_{};

    std::unique_ptr<std::array<RealtimeEnvelopeBank, 2>> envDoubleBuffer_;
    std::atomic<int> activeEnvSlot_{0};

    MultibandCrossover crossover_{};
    TriggerDetector detector_{};
    std::array<CriticallyDampedSmoother, 3> bandSmoothers_{};
    std::array<float, 3> grMeterBallisticLin_{1.0f, 1.0f, 1.0f};
    float grMeterReleaseCoeff_ = 0.0005f;

    std::vector<float> bandLookaheadRing_[2][3];
    std::vector<float> dryCompensationRing_[2];
    std::vector<float> triggerAlignRing_[2];
    std::vector<float> scScopeAlignRing_[2];
    int delayWritePos_ = 0;

    double freeRunningPhase_ = 0.0;
    double triggeredPhase_ = 1.0;
    bool triggeredActive_ = false;
    bool triggerGateHeld_ = false;
    float currentVelocityDepthScale_ = 1.0f;

    std::vector<float> traceCaptureBuffer_;
    int traceTargetSamples_ = 48000;
    int traceWritePos_ = 0;
    std::atomic<bool> isTracingLive_{false};
    std::atomic<bool> traceReady_{false};

    SpectrumAnalyzerFifo spectrumFifo_{};
    OscilloscopeSnapshot scopeData_{};
    int scopeDecimCounter_ = 0;
    int scopeWriteIdx_ = 0;
    float scopePreAcc_ = 0.0f;
    float scopePostAcc_ = 0.0f;
    float scopeScAcc_ = 0.0f;

    std::atomic<float> playheadPhase_{0.0f};
    std::array<std::atomic<float>, 3> bandGainReductionDb_{0.0f, 0.0f, 0.0f};
    std::atomic<float> detectorMeterLinear_{0.0f};
    std::atomic<bool> triggerLedFlag_{false};
};

} // namespace duckflow
