#pragma once

#include "SimdAndMath.h"
#include "MultibandCrossover.h"
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace duckflow {

enum class TriggerMode : int {
    RepeatSync = 0,     // DAW Tempo / Beat-synced repeating LFO (runs even when DAW stopped)
    FreeHz = 1,         // Unsynced Hz rate for tremolo & sound design
    MidiTrigger = 2,    // MIDI Note-On / Note-Off with velocity sensitivity & Loop+Release
    SidechainAudio = 3, // External Sidechain Bus transient / threshold trigger
    MainAudio = 4       // Self-triggering from Main Audio Input transients
};

struct MidiTriggerEvent {
    int sampleOffset = 0;
    bool isNoteOn = false;
    int noteNumber = 60;
    float velocity = 1.0f; // [0.0, 1.0]
};

/**
 * Audio & MIDI Trigger Detector
 * Matches Duck 2's Trigger section:
 *  - Dedicated HPF (20 Hz - 2 kHz) and LPF (100 Hz - 20 kHz) 12dB/oct detector filter
 *  - Sidechain Filter Audition ("Listen") stereo signal output
 *  - Instantaneous 0-sample transient peak capture + smooth exponential release with 3 dB hysteresis
 *    (catches 1-sample ghost trigger clicks with zero lag!)
 *  - Hold time down to 0.0 ms (matching Duck 2 v2.0.29!)
 *  - MIDI Note polyphonic ref-counted gate + velocity depth modulation
 */
class TriggerDetector {
public:
    void prepare(double sampleRate) noexcept {
        sampleRate_ = std::max(8000.0, sampleRate);
        lastHpfHz_ = -1.0f;
        lastLpfHz_ = -1.0f;
        setFilter(30.0f, 16000.0f);
        releaseCoeff_ = static_cast<float>(1.0 - std::exp(-1.0 / (0.028 * sampleRate_)));
        reset();
    }

    void reset() noexcept {
        for (int ch = 0; ch < 2; ++ch) {
            hpf_[ch].reset();
            lpf_[ch].reset();
        }
        envLevel_ = 0.0f;
        audioGateOpen_ = false;
        holdSamplesRemaining_ = 0;
        activeMidiNotes_ = 0;
        lastVelocity_ = 1.0f;
    }

    void setFilter(float hpfHz, float lpfHz) noexcept {
        hpfHz = std::clamp(hpfHz, 20.0f, 4000.0f);
        lpfHz = std::clamp(lpfHz, std::max(hpfHz + 40.0f, 80.0f), 20000.0f);
        if (std::fabs(hpfHz - lastHpfHz_) > 0.25f || std::fabs(lpfHz - lastLpfHz_) > 0.5f) {
            lastHpfHz_ = hpfHz;
            lastLpfHz_ = lpfHz;
            for (int ch = 0; ch < 2; ++ch) {
                hpf_[ch].setCoeffs(sampleRate_, hpfHz, 0.70710678f);
                lpf_[ch].setCoeffs(sampleRate_, lpfHz, 0.70710678f);
            }
        }
    }

    struct TriggerResult {
        bool retriggered = false;
        bool gateHeld = false;
        float filteredL = 0.0f;
        float filteredR = 0.0f;
        float detectorLinear = 0.0f;
        float triggerVelocityGain = 1.0f;
    };

    inline TriggerResult processAudioSample(
        float inL,
        float inR,
        float thresholdDb,
        float holdTimeMs,
        bool filterEnabled) noexcept
    {
        TriggerResult res{};

        float fL = inL;
        float fR = inR;
        if (filterEnabled) {
            fL = lpf_[0].processLowpass(hpf_[0].processHighpass(inL));
            fR = lpf_[1].processLowpass(hpf_[1].processHighpass(inR));
        }
        res.filteredL = fL;
        res.filteredR = fR;

        // Instantaneous peak attack ensures 0-sample transient alignment even on 1-sample ghost clicks
        const float peak = std::max(std::fabs(fL), std::fabs(fR));
        if (peak >= envLevel_) {
            envLevel_ = peak;
        } else {
            envLevel_ += releaseCoeff_ * (peak - envLevel_);
        }
        res.detectorLinear = envLevel_;

        const float threshOpen = dbToGain(std::clamp(thresholdDb, -60.0f, 0.0f));
        const float threshClose = threshOpen * 0.70794578f; // 3 dB hysteresis
        const int holdSamples = static_cast<int>(std::round(
            std::clamp(holdTimeMs, 0.0f, 500.0f) * 0.001f * static_cast<float>(sampleRate_)));

        if (!audioGateOpen_) {
            if (envLevel_ >= threshOpen && holdSamplesRemaining_ <= 0) {
                audioGateOpen_ = true;
                res.retriggered = true;
                holdSamplesRemaining_ = holdSamples;
            } else if (holdSamplesRemaining_ > 0) {
                --holdSamplesRemaining_;
            }
        } else {
            if (holdSamplesRemaining_ > 0) {
                --holdSamplesRemaining_;
            }
            if (envLevel_ < threshClose && holdSamplesRemaining_ <= 0) {
                audioGateOpen_ = false;
            }
        }

        res.gateHeld = audioGateOpen_;
        res.triggerVelocityGain = 1.0f;
        return res;
    }

    inline TriggerResult handleMidiEvent(const MidiTriggerEvent& ev, float velocitySensitivity) noexcept {
        TriggerResult res{};
        if (ev.isNoteOn && ev.velocity > 0.001f) {
            ++activeMidiNotes_;
            lastVelocity_ = std::clamp(ev.velocity, 0.0f, 1.0f);
            res.retriggered = true;
        } else {
            activeMidiNotes_ = std::max(0, activeMidiNotes_ - 1);
        }
        res.gateHeld = (activeMidiNotes_ > 0);
        const float sens = std::clamp(velocitySensitivity, 0.0f, 1.0f);
        res.triggerVelocityGain = (1.0f - sens) + sens * lastVelocity_;
        return res;
    }

    bool isMidiGateHeld() const noexcept { return activeMidiNotes_ > 0; }
    float currentMidiVelocityGain(float velocitySensitivity) const noexcept {
        const float sens = std::clamp(velocitySensitivity, 0.0f, 1.0f);
        return (1.0f - sens) + sens * lastVelocity_;
    }

private:
    double sampleRate_ = 48000.0;
    float lastHpfHz_ = -1.0f;
    float lastLpfHz_ = -1.0f;
    float releaseCoeff_ = 0.001f;
    float envLevel_ = 0.0f;
    bool audioGateOpen_ = false;
    int holdSamplesRemaining_ = 0;

    int activeMidiNotes_ = 0;
    float lastVelocity_ = 1.0f;

    TptSvf2Pole hpf_[2]{};
    TptSvf2Pole lpf_[2]{};
};

} // namespace duckflow
