#pragma once

#include "MultiSegmentEnvelope.h"
#include "MultibandCrossover.h"
#include "TriggerDetector.h"
#include <string>
#include <vector>
#include <array>

namespace duckflow {

enum class BeatDivision : int {
    Div_1_64 = 0,
    Div_1_32,
    Div_1_16T,
    Div_1_16,
    Div_1_16D,
    Div_1_8T,
    Div_1_8,
    Div_1_8D,
    Div_1_4T,
    Div_1_4,     // Default: Quarter note (4-on-the-floor kick pump)
    Div_1_4D,
    Div_1_2,
    Div_1_Bar,
    Div_2_Bars,
    Div_4_Bars,
    Div_8_Bars,
    Count
};

inline double beatDivisionToQuarterNotes(BeatDivision div) noexcept {
    switch (div) {
        case BeatDivision::Div_1_64:  return 0.0625;
        case BeatDivision::Div_1_32:  return 0.125;
        case BeatDivision::Div_1_16T: return 1.0 / 6.0;
        case BeatDivision::Div_1_16:  return 0.25;
        case BeatDivision::Div_1_16D: return 0.375;
        case BeatDivision::Div_1_8T:  return 1.0 / 3.0;
        case BeatDivision::Div_1_8:   return 0.5;
        case BeatDivision::Div_1_8D:  return 0.75;
        case BeatDivision::Div_1_4T:  return 2.0 / 3.0;
        case BeatDivision::Div_1_4:   return 1.0;
        case BeatDivision::Div_1_4D:  return 1.5;
        case BeatDivision::Div_1_2:   return 2.0;
        case BeatDivision::Div_1_Bar: return 4.0;
        case BeatDivision::Div_2_Bars:return 8.0;
        case BeatDivision::Div_4_Bars:return 16.0;
        case BeatDivision::Div_8_Bars:return 32.0;
        default:                      return 1.0;
    }
}

inline const char* beatDivisionName(BeatDivision div) noexcept {
    switch (div) {
        case BeatDivision::Div_1_64:  return "1/64";
        case BeatDivision::Div_1_32:  return "1/32";
        case BeatDivision::Div_1_16T: return "1/16T";
        case BeatDivision::Div_1_16:  return "1/16";
        case BeatDivision::Div_1_16D: return "1/16D";
        case BeatDivision::Div_1_8T:  return "1/8T";
        case BeatDivision::Div_1_8:   return "1/8";
        case BeatDivision::Div_1_8D:  return "1/8D";
        case BeatDivision::Div_1_4T:  return "1/4T";
        case BeatDivision::Div_1_4:   return "1/4";
        case BeatDivision::Div_1_4D:  return "1/4D";
        case BeatDivision::Div_1_2:   return "1/2";
        case BeatDivision::Div_1_Bar: return "1 Bar";
        case BeatDivision::Div_2_Bars:return "2 Bars";
        case BeatDivision::Div_4_Bars:return "4 Bars";
        case BeatDivision::Div_8_Bars:return "8 Bars";
        default:                      return "1/4";
    }
}

struct QuickShape {
    std::string name;
    EnvelopeData envelope;
};

struct FactoryPreset {
    std::string category;
    std::string name;
    TriggerMode triggerMode = TriggerMode::RepeatSync;
    BeatDivision division = BeatDivision::Div_1_4;
    float freeHz = 4.0f;
    float globalDepth = 1.0f;
    float swing = 0.0f;
    float smoothingMs = 4.5f;
    float offsetMs = 0.0f;
    BandMode bandMode = BandMode::FullBand;
    bool unlinkBands = false;
    bool linearPhase = false;
    CrossoverSlope slope = CrossoverSlope::Slope24dB;
    float lowCrossHz = 140.0f;
    float highCrossHz = 2600.0f;
    float lowAmount = 1.0f;
    float midAmount = 0.65f;
    float highAmount = 0.25f;
    float thresholdDb = -24.0f;
    float holdMs = 35.0f;
    EnvelopeTriggerLoopMode loopMode = EnvelopeTriggerLoopMode::OneShot;
    std::array<EnvelopeData, 3> bandEnvelopes{};
};

class FactoryPresets {
public:
    static std::vector<QuickShape> getQuickShapes() {
        std::vector<QuickShape> shapes;

        auto makeShape = [&](const char* name, std::initializer_list<EnvelopeNode> list, float ls = 0.1f, float le = 0.6f) {
            QuickShape qs;
            qs.name = name;
            qs.envelope.nodes.assign(list.begin(), list.end());
            qs.envelope.loopStart = ls;
            qs.envelope.loopEnd = le;
            qs.envelope.sortAndValidate();
            shapes.push_back(std::move(qs));
        };

        // 1. Classic Pump
        makeShape("Classic Pump", {
            {0.0f, 0.0f, 0.0f, CurveType::Linear},
            {0.05f, 0.0f, -0.30f, CurveType::SCurveSpline},
            {0.58f, 1.0f, 0.0f, CurveType::Linear},
            {1.0f, 1.0f, 0.0f, CurveType::Linear}
        });
        // 2. Tight Kick Pocket
        makeShape("Tight Kick", {
            {0.0f, 0.0f, 0.0f, CurveType::Linear},
            {0.03f, 0.0f, -0.45f, CurveType::Tension2D},
            {0.30f, 1.0f, 0.0f, CurveType::Linear},
            {1.0f, 1.0f, 0.0f, CurveType::Linear}
        });
        // 3. Deep House Suck
        makeShape("House Suck", {
            {0.0f, 0.0f, 0.45f, CurveType::Tension2D},
            {0.82f, 1.0f, -0.35f, CurveType::SCurveSpline},
            {1.0f, 1.0f, 0.0f, CurveType::Linear}
        });
        // 4. Sub-Only Clean Pocket
        makeShape("Sub Pocket", {
            {0.0f, 0.0f, 0.0f, CurveType::StepHold},
            {0.12f, 0.0f, -0.20f, CurveType::SCurveSpline},
            {0.42f, 1.0f, 0.0f, CurveType::Linear},
            {1.0f, 1.0f, 0.0f, CurveType::Linear}
        });
        // 5. Trance Gate 4-Step
        makeShape("Trance Gate", {
            {0.0f, 1.0f, 0.0f, CurveType::StepHold},
            {0.25f, 0.0f, 0.0f, CurveType::StepHold},
            {0.50f, 1.0f, 0.0f, CurveType::StepHold},
            {0.75f, 0.15f, 0.0f, CurveType::StepHold},
            {1.0f, 1.0f, 0.0f, CurveType::Linear}
        });
        // 6. Offbeat Skank
        makeShape("Offbeat", {
            {0.0f, 0.0f, 0.0f, CurveType::StepHold},
            {0.48f, 0.0f, -0.4f, CurveType::SCurveSpline},
            {0.52f, 1.0f, 0.0f, CurveType::StepHold},
            {0.95f, 1.0f, 0.3f, CurveType::SCurveSpline},
            {1.0f, 0.0f, 0.0f, CurveType::Linear}
        });
        // 7. Sine Tremolo
        makeShape("Sine Tremolo", {
            {0.0f, 1.0f, 0.0f, CurveType::SCurveSpline},
            {0.50f, 0.0f, 0.0f, CurveType::SCurveSpline},
            {1.0f, 1.0f, 0.0f, CurveType::Linear}
        });
        // 8. Wobble Flutter
        makeShape("Sine Wobble", {
            {0.0f, 0.0f, 0.15f, CurveType::SineWobble},
            {0.75f, 1.0f, 0.0f, CurveType::SCurveSpline},
            {1.0f, 1.0f, 0.0f, CurveType::Linear}
        });
        // 9. Saw Chopper
        makeShape("Saw Chopper", {
            {0.0f, 0.2f, 0.25f, CurveType::SawWobble},
            {1.0f, 1.0f, 0.0f, CurveType::Linear}
        });
        // 10. Transient Tamer (De-Click / Soft Attack)
        makeShape("Soft Attack", {
            {0.0f, 0.0f, 0.35f, CurveType::SCurveSpline},
            {0.22f, 1.0f, 0.0f, CurveType::Linear},
            {1.0f, 1.0f, 0.0f, CurveType::Linear}
        });
        // 11. Kick Tail Cleaner (Drum Gate / Shaper)
        makeShape("Tail Cleaner", {
            {0.0f, 1.0f, 0.0f, CurveType::Linear},
            {0.28f, 1.0f, 0.25f, CurveType::Tension2D},
            {0.68f, 0.0f, 0.0f, CurveType::Linear},
            {1.0f, 0.0f, 0.0f, CurveType::Linear}
        });
        // 12. MIDI Pluck ADSR
        makeShape("MIDI Pluck", {
            {0.0f, 1.0f, 0.45f, CurveType::Tension2D},
            {0.45f, 0.25f, 0.0f, CurveType::Linear},
            {0.65f, 0.25f, 0.30f, CurveType::Tension2D},
            {1.0f, 0.0f, 0.0f, CurveType::Linear}
        }, 0.45f, 0.65f);
        // 13. Reverse Swell
        makeShape("Reverse Swell", {
            {0.0f, 0.0f, 0.62f, CurveType::Tension2D},
            {0.96f, 1.0f, -0.5f, CurveType::SCurveSpline},
            {1.0f, 0.0f, 0.0f, CurveType::Linear}
        });
        // 14. Double Bounce
        makeShape("Double Bounce", {
            {0.0f, 0.0f, -0.25f, CurveType::SCurveSpline},
            {0.35f, 1.0f, 0.0f, CurveType::SCurveSpline},
            {0.50f, 0.30f, -0.25f, CurveType::SCurveSpline},
            {0.85f, 1.0f, 0.0f, CurveType::Linear},
            {1.0f, 1.0f, 0.0f, CurveType::Linear}
        });
        // 15. Euclidean Stutter
        makeShape("Stutter 3-3-2", {
            {0.0f, 1.0f, 0.0f, CurveType::StepHold},
            {0.1875f, 0.0f, 0.0f, CurveType::StepHold},
            {0.375f, 1.0f, 0.0f, CurveType::StepHold},
            {0.5625f, 0.0f, 0.0f, CurveType::StepHold},
            {0.75f, 1.0f, 0.0f, CurveType::StepHold},
            {0.875f, 0.0f, 0.0f, CurveType::StepHold},
            {1.0f, 1.0f, 0.0f, CurveType::Linear}
        });
        // 16. Subtle Mix Glue
        makeShape("Mix Glue", {
            {0.0f, 0.45f, -0.2f, CurveType::SCurveSpline},
            {0.35f, 1.0f, 0.0f, CurveType::Linear},
            {1.0f, 1.0f, 0.0f, CurveType::Linear}
        });

        return shapes;
    }

    static std::vector<FactoryPreset> getPresets() {
        const auto shapes = getQuickShapes();
        auto findShape = [&](const std::string& name) -> EnvelopeData {
            for (const auto& s : shapes) {
                if (s.name == name) return s.envelope;
            }
            return EnvelopeData{};
        };

        std::vector<FactoryPreset> list;

        // --- Category 1: Sidechain Pump ---
        {
            FactoryPreset p;
            p.category = "Sidechain Pump";
            p.name = "01 Classic EDM Pump";
            p.bandEnvelopes = {findShape("Classic Pump"), findShape("Classic Pump"), findShape("Classic Pump")};
            p.globalDepth = 0.85f;
            list.push_back(p);
        }
        {
            FactoryPreset p;
            p.category = "Sidechain Pump";
            p.name = "02 Tight Techno Kick Pocket";
            p.bandEnvelopes = {findShape("Tight Kick"), findShape("Tight Kick"), findShape("Tight Kick")};
            p.globalDepth = 0.95f;
            p.smoothingMs = 3.0f;
            list.push_back(p);
        }
        {
            FactoryPreset p;
            p.category = "Sidechain Pump";
            p.name = "03 Deep House Breathing";
            p.bandEnvelopes = {findShape("House Suck"), findShape("House Suck"), findShape("House Suck")};
            p.globalDepth = 0.78f;
            p.swing = 0.14f;
            list.push_back(p);
        }
        {
            FactoryPreset p;
            p.category = "Sidechain Pump";
            p.name = "04 Sidechain Input Duck (Lookahead)";
            p.triggerMode = TriggerMode::SidechainAudio;
            p.bandEnvelopes = {findShape("Tight Kick"), findShape("Tight Kick"), findShape("Tight Kick")};
            p.offsetMs = 5.0f; // 5ms true lookahead!
            p.thresholdDb = -24.0f;
            p.holdMs = 40.0f;
            p.globalDepth = 0.90f;
            list.push_back(p);
        }

        // --- Category 2: Multiband Mix ---
        {
            FactoryPreset p;
            p.category = "Multiband Mix";
            p.name = "05 Sub-Bass Only Duck (2-Band LR4)";
            p.bandMode = BandMode::TwoBand;
            p.lowCrossHz = 135.0f;
            p.lowAmount = 1.0f;
            p.highAmount = 0.08f;
            p.bandEnvelopes = {findShape("Sub Pocket"), findShape("Sub Pocket"), findShape("Sub Pocket")};
            list.push_back(p);
        }
        {
            FactoryPreset p;
            p.category = "Multiband Mix";
            p.name = "06 3-Band Linear Phase Mastering Duck";
            p.bandMode = BandMode::ThreeBand;
            p.unlinkBands = true;
            p.linearPhase = true;
            p.slope = CrossoverSlope::Slope48dB;
            p.lowCrossHz = 120.0f;
            p.highCrossHz = 2800.0f;
            p.lowAmount = 1.0f;
            p.midAmount = 0.55f;
            p.highAmount = 0.15f;
            p.bandEnvelopes = {findShape("Sub Pocket"), findShape("Tight Kick"), findShape("Mix Glue")};
            list.push_back(p);
        }
        {
            FactoryPreset p;
            p.category = "Multiband Mix";
            p.name = "07 Drum & Bass Breakbeat SC (3-Band)";
            p.triggerMode = TriggerMode::SidechainAudio;
            p.division = BeatDivision::Div_1_8;
            p.bandMode = BandMode::ThreeBand;
            p.unlinkBands = true;
            p.linearPhase = true;
            p.offsetMs = 4.0f;
            p.lowCrossHz = 150.0f;
            p.highCrossHz = 3200.0f;
            p.lowAmount = 1.0f;
            p.midAmount = 0.70f;
            p.highAmount = 0.20f;
            p.bandEnvelopes = {findShape("Tight Kick"), findShape("Mix Glue"), findShape("Soft Attack")};
            list.push_back(p);
        }
        {
            FactoryPreset p;
            p.category = "Multiband Mix";
            p.name = "08 Vocal Space Maker (Mid-Band Focus)";
            p.triggerMode = TriggerMode::SidechainAudio;
            p.bandMode = BandMode::ThreeBand;
            p.linearPhase = true;
            p.lowCrossHz = 220.0f;
            p.highCrossHz = 4800.0f;
            p.lowAmount = 0.0f;
            p.midAmount = 0.75f;
            p.highAmount = 0.20f;
            p.bandEnvelopes = {findShape("Mix Glue"), findShape("Classic Pump"), findShape("Mix Glue")};
            list.push_back(p);
        }

        // --- Category 3: Rhythmic Gate ---
        {
            FactoryPreset p;
            p.category = "Rhythmic Gate";
            p.name = "09 Classic Trance Pad Gate";
            p.division = BeatDivision::Div_1_4;
            p.bandEnvelopes = {findShape("Trance Gate"), findShape("Trance Gate"), findShape("Trance Gate")};
            p.smoothingMs = 2.5f;
            list.push_back(p);
        }
        {
            FactoryPreset p;
            p.category = "Rhythmic Gate";
            p.name = "10 Euclidean 3-3-2 Groove";
            p.division = BeatDivision::Div_1_2;
            p.bandEnvelopes = {findShape("Stutter 3-3-2"), findShape("Stutter 3-3-2"), findShape("Stutter 3-3-2")};
            p.swing = 0.12f;
            p.smoothingMs = 2.2f;
            list.push_back(p);
        }
        {
            FactoryPreset p;
            p.category = "Rhythmic Gate";
            p.name = "11 UK Garage Swung Chop";
            p.division = BeatDivision::Div_1_4;
            p.swing = 0.28f;
            p.bandEnvelopes = {findShape("Double Bounce"), findShape("Double Bounce"), findShape("Double Bounce")};
            list.push_back(p);
        }
        {
            FactoryPreset p;
            p.category = "Rhythmic Gate";
            p.name = "12 Offbeat Reggae / House Skank";
            p.division = BeatDivision::Div_1_4;
            p.bandEnvelopes = {findShape("Offbeat"), findShape("Offbeat"), findShape("Offbeat")};
            list.push_back(p);
        }

        // --- Category 4: Tremolo & LFO ---
        {
            FactoryPreset p;
            p.category = "Tremolo & LFO";
            p.name = "13 Vintage Surf Tremolo (5.5 Hz)";
            p.triggerMode = TriggerMode::FreeHz;
            p.freeHz = 5.5f;
            p.globalDepth = 0.65f;
            p.bandEnvelopes = {findShape("Sine Tremolo"), findShape("Sine Tremolo"), findShape("Sine Tremolo")};
            list.push_back(p);
        }
        {
            FactoryPreset p;
            p.category = "Tremolo & LFO";
            p.name = "14 Polyrhythmic Multiband Shimmer";
            p.bandMode = BandMode::ThreeBand;
            p.unlinkBands = true;
            p.division = BeatDivision::Div_1_2;
            p.lowAmount = 0.35f;
            p.midAmount = 0.75f;
            p.highAmount = 0.95f;
            p.bandEnvelopes = {findShape("Sine Tremolo"), findShape("Sine Wobble"), findShape("Saw Chopper")};
            list.push_back(p);
        }
        {
            FactoryPreset p;
            p.category = "Tremolo & LFO";
            p.name = "15 Helicopter Fast Chopper (14 Hz)";
            p.triggerMode = TriggerMode::FreeHz;
            p.freeHz = 14.0f;
            p.globalDepth = 0.90f;
            p.smoothingMs = 1.8f;
            p.bandEnvelopes = {findShape("Trance Gate"), findShape("Trance Gate"), findShape("Trance Gate")};
            list.push_back(p);
        }
        {
            FactoryPreset p;
            p.category = "Tremolo & LFO";
            p.name = "16 Dubstep LFO Wobble Pump";
            p.division = BeatDivision::Div_1_2;
            p.bandEnvelopes = {findShape("Sine Wobble"), findShape("Sine Wobble"), findShape("Sine Wobble")};
            list.push_back(p);
        }

        // --- Category 5: Transient Shaper ---
        {
            FactoryPreset p;
            p.category = "Transient Shaper";
            p.name = "17 Noisy Kick Tail Cleaner";
            p.triggerMode = TriggerMode::MainAudio;
            p.division = BeatDivision::Div_1_4;
            p.thresholdDb = -20.0f;
            p.holdMs = 60.0f;
            p.bandEnvelopes = {findShape("Tail Cleaner"), findShape("Tail Cleaner"), findShape("Tail Cleaner")};
            list.push_back(p);
        }
        {
            FactoryPreset p;
            p.category = "Transient Shaper";
            p.name = "18 Chord Stab Decay Sculptor";
            p.triggerMode = TriggerMode::MainAudio;
            p.division = BeatDivision::Div_1_2;
            p.bandMode = BandMode::TwoBand;
            p.unlinkBands = true;
            p.lowCrossHz = 350.0f;
            p.bandEnvelopes = {findShape("Tail Cleaner"), findShape("MIDI Pluck"), findShape("MIDI Pluck")};
            list.push_back(p);
        }
        {
            FactoryPreset p;
            p.category = "Transient Shaper";
            p.name = "19 Soften Harsh Pick Attack";
            p.triggerMode = TriggerMode::MainAudio;
            p.division = BeatDivision::Div_1_8;
            p.offsetMs = 3.0f;
            p.globalDepth = 0.70f;
            p.bandEnvelopes = {findShape("Soft Attack"), findShape("Soft Attack"), findShape("Soft Attack")};
            list.push_back(p);
        }
        {
            FactoryPreset p;
            p.category = "Transient Shaper";
            p.name = "20 MIDI Velocity Synth Envelope (ADSR)";
            p.triggerMode = TriggerMode::MidiTrigger;
            p.loopMode = EnvelopeTriggerLoopMode::LoopRelease;
            p.division = BeatDivision::Div_1_2;
            p.bandEnvelopes = {findShape("MIDI Pluck"), findShape("MIDI Pluck"), findShape("MIDI Pluck")};
            list.push_back(p);
        }

        // --- Category 6: Creative FX ---
        {
            FactoryPreset p;
            p.category = "Creative FX";
            p.name = "21 Reverse Tape Swell";
            p.division = BeatDivision::Div_1_2;
            p.bandEnvelopes = {findShape("Reverse Swell"), findShape("Reverse Swell"), findShape("Reverse Swell")};
            p.smoothingMs = 6.0f;
            list.push_back(p);
        }
        {
            FactoryPreset p;
            p.category = "Creative FX";
            p.name = "22 Rising Saw Flutter";
            p.division = BeatDivision::Div_1_Bar;
            p.bandEnvelopes = {findShape("Saw Chopper"), findShape("Saw Chopper"), findShape("Saw Chopper")};
            list.push_back(p);
        }
        {
            FactoryPreset p;
            p.category = "Creative FX";
            p.name = "23 3-Band Spectral Wave Cascade";
            p.bandMode = BandMode::ThreeBand;
            p.unlinkBands = true;
            p.linearPhase = true;
            p.division = BeatDivision::Div_1_Bar;
            p.bandEnvelopes = {findShape("Reverse Swell"), findShape("Double Bounce"), findShape("Sine Wobble")};
            list.push_back(p);
        }
        {
            FactoryPreset p;
            p.category = "Creative FX";
            p.name = "24 Subtle Bus Glue (25% Depth)";
            p.division = BeatDivision::Div_1_4;
            p.globalDepth = 0.35f;
            p.bandEnvelopes = {findShape("Mix Glue"), findShape("Mix Glue"), findShape("Mix Glue")};
            list.push_back(p);
        }

        return list;
    }
};

} // namespace duckflow
