/**
 * DuckFlow 2 - Standalone Zero-Dependency Steinberg VST3 3.7 C++ COM ABI Wrapper
 *
 * Wraps `duckflow::DuckFlowEngine` directly into a binary-compatible VST3 SingleComponentEffect
 * (implementing FUnknown, IPluginBase, IComponent, IAudioProcessor, IEditController, IPluginFactory2).
 *
 * Note: For the full custom resizable GUI (Spectrum Analyzer, Multi-Segment Curve Editor, Paint Tools),
 * build the primary JUCE 8 CMake target (`cmake -B build && cmake --build build --config Release`).
 * This standalone VST3 target provides a rock-solid zero-dependency VST3 binary that works in any
 * DAW's native generic parameter view and enables offline binary ABI validation.
 */

#include "../Source/DSP/DuckFlowEngine.h"

#include <atomic>
#include <cstring>
#include <cwchar>
#include <memory>
#include <vector>
#include <string>
#include <sstream>

#if defined(_WIN32)
  #define VST3_API __stdcall
  #define VST3_EXPORT extern "C" __declspec(dllexport)
#else
  #define VST3_API
  #define VST3_EXPORT extern "C" __attribute__((visibility("default")))
#endif

namespace vst3abi {

using int8    = std::int8_t;
using uint8   = std::uint8_t;
using int16   = std::int16_t;
using uint16  = std::uint16_t;
using int32   = std::int32_t;
using uint32  = std::uint32_t;
using int64   = std::int64_t;
using uint64  = std::uint64_t;
using tresult = int32;
using TBool   = uint8;
using char8   = char;
using char16  = char16_t;
using FIDString = const char8*;
using ParamID = uint32;
using ParamValue = double;
using Sample32 = float;
using Sample64 = double;
using SampleRate = double;
using SpeakerArrangement = uint64;
using TUID = char[16];

constexpr tresult kResultOk    = 0;
constexpr tresult kResultTrue  = 0;
constexpr tresult kResultFalse = 1;
constexpr tresult kNoInterface = -1;
constexpr tresult kInvalidArgument = -2;
constexpr tresult kNotImplemented  = -4;

constexpr SpeakerArrangement kSpeakerEmpty  = 0ULL;
constexpr SpeakerArrangement kSpeakerMono   = (1ULL << 2);            // M
constexpr SpeakerArrangement kSpeakerStereo = (1ULL << 0) | (1ULL << 1); // L | R

// Official Steinberg VST3 Interface IIDs (big-endian COM layout on Non-Windows, COM layout on Windows)
inline void makeUID(TUID out, uint32 l1, uint32 l2, uint32 l3, uint32 l4) noexcept {
#if defined(_WIN32)
    out[0]  = static_cast<char>((l1 & 0x000000FFu));
    out[1]  = static_cast<char>((l1 & 0x0000FF00u) >> 8);
    out[2]  = static_cast<char>((l1 & 0x00FF0000u) >> 16);
    out[3]  = static_cast<char>((l1 & 0xFF000000u) >> 24);
    out[4]  = static_cast<char>((l2 & 0x00FF0000u) >> 16);
    out[5]  = static_cast<char>((l2 & 0xFF000000u) >> 24);
    out[6]  = static_cast<char>((l2 & 0x000000FFu));
    out[7]  = static_cast<char>((l2 & 0x0000FF00u) >> 8);
#else
    out[0]  = static_cast<char>((l1 & 0xFF000000u) >> 24);
    out[1]  = static_cast<char>((l1 & 0x00FF0000u) >> 16);
    out[2]  = static_cast<char>((l1 & 0x0000FF00u) >> 8);
    out[3]  = static_cast<char>((l1 & 0x000000FFu));
    out[4]  = static_cast<char>((l2 & 0xFF000000u) >> 24);
    out[5]  = static_cast<char>((l2 & 0x00FF0000u) >> 16);
    out[6]  = static_cast<char>((l2 & 0x0000FF00u) >> 8);
    out[7]  = static_cast<char>((l2 & 0x000000FFu));
#endif
    out[8]  = static_cast<char>((l3 & 0xFF000000u) >> 24);
    out[9]  = static_cast<char>((l3 & 0x00FF0000u) >> 16);
    out[10] = static_cast<char>((l3 & 0x0000FF00u) >> 8);
    out[11] = static_cast<char>((l3 & 0x000000FFu));
    out[12] = static_cast<char>((l4 & 0xFF000000u) >> 24);
    out[13] = static_cast<char>((l4 & 0x00FF0000u) >> 16);
    out[14] = static_cast<char>((l4 & 0x0000FF00u) >> 8);
    out[15] = static_cast<char>((l4 & 0x000000FFu));
}

inline bool uidEquals(const TUID a, uint32 l1, uint32 l2, uint32 l3, uint32 l4) noexcept {
    TUID b{};
    makeUID(b, l1, l2, l3, l4);
    return std::memcmp(a, b, 16) == 0;
}

inline void copyAsciiToStr128(char16 dest[128], const char* src) noexcept {
    std::size_t i = 0;
    while (src[i] != '\0' && i < 127) {
        dest[i] = static_cast<char16>(static_cast<unsigned char>(src[i]));
        ++i;
    }
    dest[i] = 0;
}

inline void copyAsciiFixed(char* dest, std::size_t cap, const char* src) noexcept {
    if (cap == 0) return;
    std::strncpy(dest, src, cap - 1);
    dest[cap - 1] = '\0';
}

// --- Core COM Interfaces matching exact Steinberg VST3 vtable layouts ---
struct FUnknown {
    virtual tresult VST3_API queryInterface(const TUID _iid, void** obj) = 0;
    virtual uint32  VST3_API addRef() = 0;
    virtual uint32  VST3_API release() = 0;
};

struct IBStream : public FUnknown {
    enum IStreamSeekMode { kIBSeekSet = 0, kIBSeekCur, kIBSeekEnd };
    virtual tresult VST3_API read(void* buffer, int32 numBytes, int32* numBytesRead) = 0;
    virtual tresult VST3_API write(void* buffer, int32 numBytes, int32* numBytesWritten) = 0;
    virtual tresult VST3_API seek(int64 pos, int32 mode, int64* result) = 0;
    virtual tresult VST3_API tell(int64* pos) = 0;
};

struct IPluginBase : public FUnknown {
    virtual tresult VST3_API initialize(FUnknown* context) = 0;
    virtual tresult VST3_API terminate() = 0;
};

struct BusInfo {
    int32 mediaType;    // 0 = kAudio, 1 = kEvent
    int32 direction;    // 0 = kInput, 1 = kOutput
    int32 channelCount;
    char16 name[128];
    int32 busType;      // 0 = kMain, 1 = kAux
    uint32 flags;       // 1 = kDefaultActive
};

struct RoutingInfo {
    int32 mediaType;
    int32 busIndex;
    int32 channel;
};

struct IComponent : public IPluginBase {
    virtual tresult VST3_API getControllerClassId(TUID classId) = 0;
    virtual tresult VST3_API setIoMode(int32 mode) = 0;
    virtual int32   VST3_API getBusCount(int32 type, int32 dir) = 0;
    virtual tresult VST3_API getBusInfo(int32 type, int32 dir, int32 index, BusInfo& bus) = 0;
    virtual tresult VST3_API getRoutingInfo(RoutingInfo& inInfo, RoutingInfo& outInfo) = 0;
    virtual tresult VST3_API activateBus(int32 type, int32 dir, int32 index, TBool state) = 0;
    virtual tresult VST3_API setActive(TBool state) = 0;
    virtual tresult VST3_API setState(IBStream* state) = 0;
    virtual tresult VST3_API getState(IBStream* state) = 0;
};

struct ProcessSetup {
    int32 processMode;
    int32 symbolicSampleSize; // 0 = kSample32, 1 = kSample64
    int32 maxSamplesPerBlock;
    SampleRate sampleRate;
};

struct AudioBusBuffers {
    int32 numChannels;
    uint64 silenceFlags;
    union {
        Sample32** channelBuffers32;
        Sample64** channelBuffers64;
    };
};

struct IParamValueQueue : public FUnknown {
    virtual ParamID VST3_API getParameterId() = 0;
    virtual int32   VST3_API getPointCount() = 0;
    virtual tresult VST3_API getPoint(int32 index, int32& sampleOffset, ParamValue& value) = 0;
    virtual tresult VST3_API addPoint(int32 sampleOffset, ParamValue value, int32& index) = 0;
};

struct IParameterChanges : public FUnknown {
    virtual int32             VST3_API getParameterCount() = 0;
    virtual IParamValueQueue* VST3_API getParameterData(int32 index) = 0;
    virtual IParamValueQueue* VST3_API addParameterData(const ParamID& id, int32& index) = 0;
};

struct NoteOnEvent {
    int16 channel;
    int16 pitch;
    float tuning;
    float velocity;
    int32 length;
    int32 noteId;
};

struct NoteOffEvent {
    int16 channel;
    int16 pitch;
    float velocity;
    int32 noteId;
    float tuning;
};

struct Event {
    int32 busIndex;
    int32 sampleOffset;
    double ppqPosition;
    uint16 flags;
    uint16 type; // 0 = kNoteOnEvent, 1 = kNoteOffEvent
    union {
        NoteOnEvent noteOn;
        NoteOffEvent noteOff;
        uint8 rawPad[32];
    };
};

struct IEventList : public FUnknown {
    virtual int32   VST3_API getEventCount() = 0;
    virtual tresult VST3_API getEvent(int32 index, Event& e) = 0;
    virtual tresult VST3_API addEvent(Event& e) = 0;
};

struct FrameRate {
    uint32 framesPerSecond;
    uint32 flags;
};

struct Chord {
    uint8 keyNote;
    uint8 rootNote;
    int16 chordMask;
};

struct ProcessContext {
    enum StatesAndFlags : uint32 {
        kPlaying          = 1u << 1,
        kTempoValid       = 1u << 10,
        kProjectTimeMusicValid = 1u << 9,
        kTimeSigValid     = 1u << 13
    };
    uint32 state;
    double sampleRate;
    int64 projectTimeSamples;
    int64 systemTime;
    int64 continousTimeSamples;
    double projectTimeMusic;
    double barPositionMusic;
    double cycleStartMusic;
    double cycleEndMusic;
    double tempo;
    int32 timeSigNumerator;
    int32 timeSigDenominator;
    Chord chord;
    int32 smpteOffsetSubframes;
    FrameRate frameRate;
    int32 samplesToNextClock;
};

struct ProcessData {
    int32 processMode;
    int32 symbolicSampleSize;
    int32 numSamples;
    int32 numInputs;
    int32 numOutputs;
    AudioBusBuffers* inputs;
    AudioBusBuffers* outputs;
    IParameterChanges* inputParameterChanges;
    IParameterChanges* outputParameterChanges;
    IEventList* inputEvents;
    IEventList* outputEvents;
    ProcessContext* processContext;
};

struct IAudioProcessor : public FUnknown {
    virtual tresult VST3_API setBusArrangements(SpeakerArrangement* inputs, int32 numIns,
                                                SpeakerArrangement* outputs, int32 numOuts) = 0;
    virtual tresult VST3_API getBusArrangement(int32 dir, int32 index, SpeakerArrangement& arr) = 0;
    virtual tresult VST3_API canProcessSampleSize(int32 symbolicSampleSize) = 0;
    virtual uint32  VST3_API getLatencySamples() = 0;
    virtual tresult VST3_API setupProcessing(ProcessSetup& setup) = 0;
    virtual tresult VST3_API setProcessing(TBool state) = 0;
    virtual tresult VST3_API process(ProcessData& data) = 0;
    virtual uint32  VST3_API getTailSamples() = 0;
};

struct ParameterInfo {
    ParamID id;
    char16 title[128];
    char16 shortTitle[128];
    char16 units[128];
    int32 stepCount;
    ParamValue defaultNormalizedValue;
    int32 unitId;
    int32 flags; // 1 = kCanAutomate, (1<<15) = kIsProgramChange, (1<<16) = kIsBypass
};

struct IComponentHandler : public FUnknown {
    virtual tresult VST3_API beginEdit(ParamID id) = 0;
    virtual tresult VST3_API performEdit(ParamID id, ParamValue valueNormalized) = 0;
    virtual tresult VST3_API endEdit(ParamID id) = 0;
    virtual tresult VST3_API restartComponent(int32 flags) = 0;
};

struct IPlugView;

struct IEditController : public IPluginBase {
    virtual tresult    VST3_API setComponentState(IBStream* state) = 0;
    virtual tresult    VST3_API setState(IBStream* state) = 0;
    virtual tresult    VST3_API getState(IBStream* state) = 0;
    virtual int32      VST3_API getParameterCount() = 0;
    virtual tresult    VST3_API getParameterInfo(int32 paramIndex, ParameterInfo& info) = 0;
    virtual tresult    VST3_API getParamStringByValue(ParamID id, ParamValue valueNormalized, char16 string[128]) = 0;
    virtual tresult    VST3_API getParamValueByString(ParamID id, char16* string, ParamValue& valueNormalized) = 0;
    virtual ParamValue VST3_API normalizedParamToPlain(ParamID id, ParamValue valueNormalized) = 0;
    virtual ParamValue VST3_API plainParamToNormalized(ParamID id, ParamValue plainValue) = 0;
    virtual ParamValue VST3_API getParamNormalized(ParamID id) = 0;
    virtual tresult    VST3_API setParamNormalized(ParamID id, ParamValue value) = 0;
    virtual tresult    VST3_API setComponentHandler(IComponentHandler* handler) = 0;
    virtual IPlugView* VST3_API createView(FIDString name) = 0;
};

struct PFactoryInfo {
    char8 vendor[64];
    char8 url[256];
    char8 email[128];
    int32 flags;
};

struct PClassInfo {
    TUID cid;
    int32 cardinality;
    char8 category[32];
    char8 name[64];
};

struct PClassInfo2 {
    TUID cid;
    int32 cardinality;
    char8 category[32];
    char8 name[64];
    uint32 classFlags;
    char8 subCategories[128];
    char8 vendor[64];
    char8 version[64];
    char8 sdkVersion[64];
};

struct IPluginFactory : public FUnknown {
    virtual tresult VST3_API getFactoryInfo(PFactoryInfo* info) = 0;
    virtual int32   VST3_API countClasses() = 0;
    virtual tresult VST3_API getClassInfo(int32 index, PClassInfo* info) = 0;
    virtual tresult VST3_API createInstance(FIDString cid, FIDString _iid, void** obj) = 0;
};

struct IPluginFactory2 : public IPluginFactory {
    virtual tresult VST3_API getClassInfo2(int32 index, PClassInfo2* info) = 0;
};

// ==============================================================================
// DuckFlow 2 Parameter IDs & Metadata
// ==============================================================================
enum ParamIds : ParamID {
    kParamBypass = 0,
    kParamPreset,
    kParamTriggerMode,
    kParamBeatDivision,
    kParamFreeHz,
    kParamDepth,
    kParamSwing,
    kParamSmoothingMs,
    kParamOffsetMs,
    kParamBandMode,
    kParamUnlinkBands,
    kParamLinearPhase,
    kParamSlope,
    kParamLowCrossHz,
    kParamHighCrossHz,
    kParamLowAmount,
    kParamMidAmount,
    kParamHighAmount,
    kParamThresholdDb,
    kParamHoldMs,
    kParamDetHpfHz,
    kParamDetLpfHz,
    kParamMidiVelSens,
    kParamLoopMode,
    kParamOutputGainDb,
    kParamDryWet,
    kNumParams
};

struct ParamSpec {
    ParamID id;
    const char* title;
    const char* units;
    int32 stepCount;
    double minPlain;
    double maxPlain;
    double defaultPlain;
    int32 flags;
};

static const ParamSpec kSpecs[kNumParams] = {
    {kParamBypass,       "Bypass",            "",    1,  0.0,   1.0,     0.0,   1 | (1 << 16)},
    {kParamPreset,       "Factory Preset",    "",   23,  0.0,  23.0,     0.0,   1 | (1 << 15)},
    {kParamTriggerMode,  "Trigger Mode",      "",    4,  0.0,   4.0,     0.0,   1},
    {kParamBeatDivision, "Speed (Beat)",      "",   15,  0.0,  15.0,     9.0,   1},
    {kParamFreeHz,       "Free Rate",         "Hz",  0,  0.05, 50.0,     4.0,   1},
    {kParamDepth,        "Amount",            "%",   0,  0.0, 100.0,    85.0,   1},
    {kParamSwing,        "Swing",             "%",   0,-75.0,  75.0,     0.0,   1},
    {kParamSmoothingMs,  "Smoothing",         "ms",  0,  0.1,  50.0,     4.5,   1},
    {kParamOffsetMs,     "Offset/Lookahead",  "ms",  0,-200.0,200.0,     0.0,   1},
    {kParamBandMode,     "Multiband Mode",    "",    2,  0.0,   2.0,     0.0,   1},
    {kParamUnlinkBands,  "Unlink Envelopes",  "",    1,  0.0,   1.0,     0.0,   1},
    {kParamLinearPhase,  "Linear Phase",      "",    1,  0.0,   1.0,     0.0,   1},
    {kParamSlope,        "Crossover Slope",   "",    2,  0.0,   2.0,     1.0,   1},
    {kParamLowCrossHz,   "Low Crossover",     "Hz",  0, 20.0,2000.0,   140.0,   1},
    {kParamHighCrossHz,  "High Crossover",    "Hz",  0,200.0,18000.0, 2600.0,   1},
    {kParamLowAmount,    "Low Band Amount",   "%",   0,  0.0, 100.0,   100.0,   1},
    {kParamMidAmount,    "Mid Band Amount",   "%",   0,  0.0, 100.0,    70.0,   1},
    {kParamHighAmount,   "High Band Amount",  "%",   0,  0.0, 100.0,    30.0,   1},
    {kParamThresholdDb,   "Trigger Threshold", "dB",  0,-60.0,   0.0,   -24.0,   1},
    {kParamHoldMs,       "Trigger Hold",      "ms",  0,  0.0, 500.0,    35.0,   1},
    {kParamDetHpfHz,     "Sidechain HPF",     "Hz",  0, 20.0,2000.0,    30.0,   1},
    {kParamDetLpfHz,     "Sidechain LPF",     "Hz",  0,100.0,20000.0,16000.0,   1},
    {kParamMidiVelSens,  "MIDI Vel Sens",     "%",   0,  0.0, 100.0,    75.0,   1},
    {kParamLoopMode,     "Trigger Loop Mode", "",    2,  0.0,   2.0,     0.0,   1},
    {kParamOutputGainDb, "Output Gain",       "dB",  0,-24.0,  12.0,     0.0,   1},
    {kParamDryWet,       "Dry/Wet Mix",       "%",   0,  0.0, 100.0,   100.0,   1}
};

// Unique VST3 Processor/SingleComponent Class UID for DuckFlow 2
static constexpr uint32 kDuckFlowCid1 = 0x446B4632u; // 'DkF2'
static constexpr uint32 kDuckFlowCid2 = 0x56535433u; // 'VST3'
static constexpr uint32 kDuckFlowCid3 = 0x91A2B3C4u;
static constexpr uint32 kDuckFlowCid4 = 0x55667788u;

// ==============================================================================
// DuckFlow2SingleComponentEffect (Implements IComponent + IAudioProcessor + IEditController)
// ==============================================================================
class DuckFlow2SingleComponent final : public IComponent,
                                       public IAudioProcessor,
                                       public IEditController {
public:
    DuckFlow2SingleComponent()
        : engine_(std::make_unique<duckflow::DuckFlowEngine>()),
          presets_(duckflow::FactoryPresets::getPresets())
    {
        for (uint32 i = 0; i < kNumParams; ++i) {
            const auto& sp = kSpecs[i];
            normValues_[i] = (sp.defaultPlain - sp.minPlain) / std::max(1e-9, sp.maxPlain - sp.minPlain);
        }
        midiScratch_.reserve(256);
        applyFactoryPresetInternal(0);
    }

    // --- FUnknown disambiguation ---
    tresult VST3_API queryInterface(const TUID _iid, void** obj) override {
        if (!obj) return kInvalidArgument;
        // FUnknown: 00000000-0000-0000-C000-000000000046
        // IPluginBase: 22888DDB-156E-45AE-8358-B34808190625
        // IComponent: E831FF31-F2D5-4301-928E-BBEE25697802
        if (uidEquals(_iid, 0x00000000u, 0x00000000u, 0xC0000000u, 0x00000046u) ||
            uidEquals(_iid, 0x22888DDBu, 0x156E45AEu, 0x8358B348u, 0x08190625u) ||
            uidEquals(_iid, 0xE831FF31u, 0xF2D54301u, 0x928EBBEEu, 0x25697802u))
        {
            addRef();
            *obj = static_cast<IComponent*>(this);
            return kResultTrue;
        }
        // IAudioProcessor: 42043F99-B7DA-453C-A569-E79D9AAEC33D
        if (uidEquals(_iid, 0x42043F99u, 0xB7DA453Cu, 0xA569E79Du, 0x9AAEC33Du)) {
            addRef();
            *obj = static_cast<IAudioProcessor*>(this);
            return kResultTrue;
        }
        // IEditController: DCD7BBE3-7742-448D-A874-AACC979C759E
        if (uidEquals(_iid, 0xDCD7BBE3u, 0x7742448Du, 0xA874AACCu, 0x979C759Eu)) {
            addRef();
            *obj = static_cast<IEditController*>(this);
            return kResultTrue;
        }
        *obj = nullptr;
        return kNoInterface;
    }

    uint32 VST3_API addRef() override {
        return ++refCount_;
    }

    uint32 VST3_API release() override {
        const uint32 r = --refCount_;
        if (r == 0) delete this;
        return r;
    }

    // --- IPluginBase ---
    tresult VST3_API initialize(FUnknown*) override { return kResultOk; }
    tresult VST3_API terminate() override { handler_ = nullptr; return kResultOk; }

    // --- IComponent ---
    tresult VST3_API getControllerClassId(TUID classId) override {
        // Single-component effect: controller shares same class ID
        makeUID(classId, kDuckFlowCid1, kDuckFlowCid2, kDuckFlowCid3, kDuckFlowCid4);
        return kResultTrue;
    }
    tresult VST3_API setIoMode(int32) override { return kResultOk; }

    int32 VST3_API getBusCount(int32 type, int32 dir) override {
        if (type == 0) { // kAudio
            return (dir == 0) ? 2 : 1; // 2 Audio Inputs (Main + Aux Sidechain), 1 Audio Output (Main)
        }
        if (type == 1) { // kEvent (MIDI)
            return (dir == 0) ? 1 : 0; // 1 MIDI Event Input for MIDI Trigger!
        }
        return 0;
    }

    tresult VST3_API getBusInfo(int32 type, int32 dir, int32 index, BusInfo& bus) override {
        std::memset(&bus, 0, sizeof(BusInfo));
        bus.mediaType = type;
        bus.direction = dir;
        if (type == 0 && dir == 0) {
            if (index == 0) {
                bus.channelCount = (mainInArr_ == kSpeakerMono) ? 1 : 2;
                copyAsciiToStr128(bus.name, "Main Input");
                bus.busType = 0; // kMain
                bus.flags = 1;   // kDefaultActive
                return kResultTrue;
            }
            if (index == 1) {
                bus.channelCount = (scInArr_ == kSpeakerMono) ? 1 : 2;
                copyAsciiToStr128(bus.name, "Sidechain");
                bus.busType = 1; // kAux
                bus.flags = 0;
                return kResultTrue;
            }
        } else if (type == 0 && dir == 1 && index == 0) {
            bus.channelCount = (mainOutArr_ == kSpeakerMono) ? 1 : 2;
            copyAsciiToStr128(bus.name, "Main Output");
            bus.busType = 0;
            bus.flags = 1;
            return kResultTrue;
        } else if (type == 1 && dir == 0 && index == 0) {
            bus.channelCount = 16;
            copyAsciiToStr128(bus.name, "MIDI Trigger In");
            bus.busType = 0;
            bus.flags = 1;
            return kResultTrue;
        }
        return kInvalidArgument;
    }

    tresult VST3_API getRoutingInfo(RoutingInfo& inInfo, RoutingInfo& outInfo) override {
        if (inInfo.mediaType == 0 && inInfo.busIndex == 0) {
            outInfo = inInfo;
            return kResultTrue;
        }
        return kNotImplemented;
    }

    tresult VST3_API activateBus(int32 type, int32 dir, int32 index, TBool state) override {
        if (type == 0 && dir == 0 && index == 1) {
            sidechainActive_ = (state != 0);
            return kResultTrue;
        }
        return kResultOk;
    }

    tresult VST3_API setActive(TBool state) override {
        if (state) engine_->reset();
        return kResultOk;
    }

    tresult VST3_API setState(IBStream* state) override {
        if (!state) return kInvalidArgument;
        int32 nRead = 0;
        uint32 magic = 0;
        if (state->read(&magic, sizeof(magic), &nRead) != kResultOk || magic != 0x444B4632u) {
            return kResultFalse;
        }
        double loaded[kNumParams]{};
        if (state->read(loaded, sizeof(loaded), &nRead) != kResultOk || nRead != sizeof(loaded)) {
            return kResultFalse;
        }
        for (uint32 i = 0; i < kNumParams; ++i) {
            normValues_[i] = std::clamp(loaded[i], 0.0, 1.0);
        }
        for (std::size_t b = 0; b < 3; ++b) {
            uint32 len = 0;
            if (state->read(&len, sizeof(len), &nRead) == kResultOk && len > 0 && len < 65536) {
                std::string s(len, '\0');
                if (state->read(s.data(), static_cast<int32>(len), &nRead) == kResultOk) {
                    bandEnvs_[b].deserialize(s);
                }
            }
        }
        syncEngineFromParams();
        engine_->publishEnvelopes(bandEnvs_);
        return kResultOk;
    }

    tresult VST3_API getState(IBStream* state) override {
        if (!state) return kInvalidArgument;
        int32 nWritten = 0;
        uint32 magic = 0x444B4632u; // 'DKF2'
        state->write(&magic, sizeof(magic), &nWritten);
        state->write(normValues_.data(), static_cast<int32>(sizeof(normValues_)), &nWritten);
        for (std::size_t b = 0; b < 3; ++b) {
            const std::string s = bandEnvs_[b].serialize();
            uint32 len = static_cast<uint32>(s.size());
            state->write(&len, sizeof(len), &nWritten);
            if (len > 0) {
                state->write(const_cast<char*>(s.data()), static_cast<int32>(len), &nWritten);
            }
        }
        return kResultOk;
    }

    // --- IAudioProcessor ---
    tresult VST3_API setBusArrangements(SpeakerArrangement* inputs, int32 numIns,
                                        SpeakerArrangement* outputs, int32 numOuts) override
    {
        if (numOuts != 1 || numIns < 1 || numIns > 2) return kResultFalse;
        if (outputs[0] != kSpeakerStereo && outputs[0] != kSpeakerMono) return kResultFalse;
        if (inputs[0] != outputs[0]) return kResultFalse;

        if (numIns == 2) {
            if (inputs[1] != kSpeakerEmpty && inputs[1] != kSpeakerMono && inputs[1] != kSpeakerStereo) {
                return kResultFalse;
            }
            scInArr_ = inputs[1];
        }
        mainInArr_ = inputs[0];
        mainOutArr_ = outputs[0];
        return kResultTrue;
    }

    tresult VST3_API getBusArrangement(int32 dir, int32 index, SpeakerArrangement& arr) override {
        if (dir == 0 && index == 0) { arr = mainInArr_; return kResultTrue; }
        if (dir == 0 && index == 1) { arr = scInArr_; return kResultTrue; }
        if (dir == 1 && index == 0) { arr = mainOutArr_; return kResultTrue; }
        return kInvalidArgument;
    }

    tresult VST3_API canProcessSampleSize(int32 symbolicSampleSize) override {
        return (symbolicSampleSize == 0) ? kResultTrue : kResultFalse; // 32-bit float
    }

    uint32 VST3_API getLatencySamples() override {
        syncEngineFromParams();
        return static_cast<uint32>(engine_->getReportedLatencySamples());
    }

    tresult VST3_API setupProcessing(ProcessSetup& setup) override {
        sampleRate_ = setup.sampleRate;
        syncEngineFromParams();
        engine_->prepare(setup.sampleRate, setup.maxSamplesPerBlock);
        return kResultOk;
    }

    tresult VST3_API setProcessing(TBool state) override {
        if (state) engine_->reset();
        return kResultOk;
    }

    tresult VST3_API process(ProcessData& data) override {
        // 1. Apply incoming parameter automation changes
        if (data.inputParameterChanges) {
            const int32 numChanges = data.inputParameterChanges->getParameterCount();
            for (int32 i = 0; i < numChanges; ++i) {
                if (IParamValueQueue* q = data.inputParameterChanges->getParameterData(i)) {
                    const ParamID pid = q->getParameterId();
                    const int32 pts = q->getPointCount();
                    if (pid < kNumParams && pts > 0) {
                        int32 offset = 0;
                        ParamValue val = 0.0;
                        if (q->getPoint(pts - 1, offset, val) == kResultTrue) {
                            setParamNormalized(pid, val);
                        }
                    }
                }
            }
        }
        syncEngineFromParams();

        if (data.numSamples <= 0 || data.numOutputs < 1 || !data.outputs) {
            return kResultOk;
        }

        AudioBusBuffers& outBus = data.outputs[0];
        if (outBus.numChannels < 1 || !outBus.channelBuffers32) return kResultOk;

        // Copy Main Input -> Main Output if not in-place
        if (data.numInputs > 0 && data.inputs && data.inputs[0].channelBuffers32) {
            const AudioBusBuffers& inBus = data.inputs[0];
            for (int32 ch = 0; ch < outBus.numChannels; ++ch) {
                const int32 srcCh = std::min(ch, inBus.numChannels - 1);
                if (srcCh >= 0 && inBus.channelBuffers32[srcCh] && outBus.channelBuffers32[ch] &&
                    inBus.channelBuffers32[srcCh] != outBus.channelBuffers32[ch])
                {
                    std::memcpy(outBus.channelBuffers32[ch], inBus.channelBuffers32[srcCh],
                                static_cast<std::size_t>(data.numSamples) * sizeof(float));
                }
            }
        }

        float* mainL = outBus.channelBuffers32[0];
        float* mainR = (outBus.numChannels > 1) ? outBus.channelBuffers32[1] : nullptr;

        const float* scL = nullptr;
        const float* scR = nullptr;
        if (sidechainActive_ && data.numInputs > 1 && data.inputs[1].numChannels > 0 && data.inputs[1].channelBuffers32) {
            scL = data.inputs[1].channelBuffers32[0];
            scR = (data.inputs[1].numChannels > 1) ? data.inputs[1].channelBuffers32[1] : scL;
        }

        // 2. Extract Transport Info
        duckflow::HostTransportInfo transport{};
        if (data.processContext) {
            const auto& ctx = *data.processContext;
            transport.isPlaying = (ctx.state & ProcessContext::kPlaying) != 0;
            if (ctx.state & ProcessContext::kTempoValid) transport.bpm = ctx.tempo;
            if (ctx.state & ProcessContext::kProjectTimeMusicValid) transport.ppqPosition = ctx.projectTimeMusic;
            if (ctx.state & ProcessContext::kTimeSigValid) {
                transport.timeSigNumerator = ctx.timeSigNumerator;
                transport.timeSigDenominator = ctx.timeSigDenominator;
            }
        }

        // 3. Extract MIDI Note-On / Note-Off Events
        midiScratch_.clear();
        if (data.inputEvents) {
            const int32 evCount = data.inputEvents->getEventCount();
            Event ev{};
            for (int32 e = 0; e < evCount; ++e) {
                if (data.inputEvents->getEvent(e, ev) == kResultOk) {
                    if (ev.type == 0) { // kNoteOnEvent
                        midiScratch_.push_back({std::clamp(ev.sampleOffset, 0, data.numSamples - 1),
                                                true, ev.noteOn.pitch, ev.noteOn.velocity});
                    } else if (ev.type == 1) { // kNoteOffEvent
                        midiScratch_.push_back({std::clamp(ev.sampleOffset, 0, data.numSamples - 1),
                                                false, ev.noteOff.pitch, 0.0f});
                    }
                }
            }
        }

        engine_->process(
            mainL, mainR, scL, scR, data.numSamples, transport,
            midiScratch_.empty() ? nullptr : midiScratch_.data(),
            static_cast<int>(midiScratch_.size()));

        return kResultOk;
    }

    uint32 VST3_API getTailSamples() override {
        return static_cast<uint32>(sampleRate_ * 0.1);
    }

    // --- IEditController ---
    tresult VST3_API setComponentState(IBStream* state) override {
        return setState(state);
    }
    int32 VST3_API getParameterCount() override {
        return static_cast<int32>(kNumParams);
    }
    tresult VST3_API getParameterInfo(int32 paramIndex, ParameterInfo& info) override {
        if (paramIndex < 0 || paramIndex >= static_cast<int32>(kNumParams)) return kInvalidArgument;
        std::memset(&info, 0, sizeof(ParameterInfo));
        const auto& sp = kSpecs[paramIndex];
        info.id = sp.id;
        copyAsciiToStr128(info.title, sp.title);
        copyAsciiToStr128(info.shortTitle, sp.title);
        copyAsciiToStr128(info.units, sp.units);
        info.stepCount = sp.stepCount;
        info.defaultNormalizedValue = (sp.defaultPlain - sp.minPlain) / std::max(1e-9, sp.maxPlain - sp.minPlain);
        info.unitId = 0;
        info.flags = sp.flags;
        return kResultOk;
    }
    tresult VST3_API getParamStringByValue(ParamID id, ParamValue valueNormalized, char16 string[128]) override {
        if (id >= kNumParams) return kInvalidArgument;
        const double plain = normalizedParamToPlain(id, valueNormalized);
        std::ostringstream oss;
        if (id == kParamPreset) {
            const int idx = std::clamp(static_cast<int>(std::lround(plain)), 0, static_cast<int>(presets_.size()) - 1);
            oss << presets_[static_cast<std::size_t>(idx)].name;
        } else if (id == kParamBeatDivision) {
            const auto div = static_cast<duckflow::BeatDivision>(std::clamp(static_cast<int>(std::lround(plain)), 0, 15));
            oss << duckflow::beatDivisionName(div);
        } else {
            oss.precision(2);
            oss << std::fixed << plain;
        }
        copyAsciiToStr128(string, oss.str().c_str());
        return kResultOk;
    }
    tresult VST3_API getParamValueByString(ParamID id, char16* string, ParamValue& valueNormalized) override {
        if (id >= kNumParams || !string) return kInvalidArgument;
        std::string ascii;
        for (int i = 0; i < 127 && string[i] != 0; ++i) {
            ascii.push_back(static_cast<char>(string[i] & 0x7Fu));
        }
        try {
            const double plain = std::stod(ascii);
            valueNormalized = plainParamToNormalized(id, plain);
            return kResultOk;
        } catch (...) {
            return kResultFalse;
        }
    }
    ParamValue VST3_API normalizedParamToPlain(ParamID id, ParamValue valueNormalized) override {
        if (id >= kNumParams) return 0.0;
        const auto& sp = kSpecs[id];
        const double n = std::clamp(valueNormalized, 0.0, 1.0);
        if (sp.stepCount > 0) {
            return std::round(n * static_cast<double>(sp.stepCount));
        }
        return sp.minPlain + n * (sp.maxPlain - sp.minPlain);
    }
    ParamValue VST3_API plainParamToNormalized(ParamID id, ParamValue plainValue) override {
        if (id >= kNumParams) return 0.0;
        const auto& sp = kSpecs[id];
        return std::clamp((plainValue - sp.minPlain) / std::max(1e-9, sp.maxPlain - sp.minPlain), 0.0, 1.0);
    }
    ParamValue VST3_API getParamNormalized(ParamID id) override {
        if (id >= kNumParams) return 0.0;
        return normValues_[id];
    }
    tresult VST3_API setParamNormalized(ParamID id, ParamValue value) override {
        if (id >= kNumParams) return kInvalidArgument;
        const double clamped = std::clamp(value, 0.0, 1.0);
        normValues_[id] = clamped;
        if (id == kParamPreset) {
            const int idx = std::clamp(static_cast<int>(std::lround(normalizedParamToPlain(id, clamped))),
                                       0, static_cast<int>(presets_.size()) - 1);
            if (idx != activePreset_) {
                applyFactoryPresetInternal(idx);
            }
        }
        return kResultOk;
    }
    tresult VST3_API setComponentHandler(IComponentHandler* handler) override {
        handler_ = handler;
        return kResultOk;
    }
    IPlugView* VST3_API createView(FIDString) override {
        return nullptr; // Host renders generic VST3 parameter editor in minimal mode
    }

private:
    void applyFactoryPresetInternal(int idx) {
        if (idx < 0 || idx >= static_cast<int>(presets_.size())) return;
        activePreset_ = idx;
        const auto& pr = presets_[static_cast<std::size_t>(idx)];
        normValues_[kParamPreset]       = plainParamToNormalized(kParamPreset, idx);
        normValues_[kParamTriggerMode]  = plainParamToNormalized(kParamTriggerMode, static_cast<double>(pr.triggerMode));
        normValues_[kParamBeatDivision] = plainParamToNormalized(kParamBeatDivision, static_cast<double>(pr.division));
        normValues_[kParamFreeHz]       = plainParamToNormalized(kParamFreeHz, pr.freeHz);
        normValues_[kParamDepth]        = plainParamToNormalized(kParamDepth, pr.globalDepth * 100.0);
        normValues_[kParamSwing]        = plainParamToNormalized(kParamSwing, pr.swing * 100.0);
        normValues_[kParamSmoothingMs]  = plainParamToNormalized(kParamSmoothingMs, pr.smoothingMs);
        normValues_[kParamOffsetMs]     = plainParamToNormalized(kParamOffsetMs, pr.offsetMs);
        normValues_[kParamBandMode]     = plainParamToNormalized(kParamBandMode, static_cast<double>(pr.bandMode));
        normValues_[kParamUnlinkBands]  = pr.unlinkBands ? 1.0 : 0.0;
        normValues_[kParamLinearPhase]  = pr.linearPhase ? 1.0 : 0.0;
        normValues_[kParamSlope]        = plainParamToNormalized(kParamSlope, static_cast<double>(pr.slope));
        normValues_[kParamLowCrossHz]   = plainParamToNormalized(kParamLowCrossHz, pr.lowCrossHz);
        normValues_[kParamHighCrossHz]  = plainParamToNormalized(kParamHighCrossHz, pr.highCrossHz);
        normValues_[kParamLowAmount]    = plainParamToNormalized(kParamLowAmount, pr.lowAmount * 100.0);
        normValues_[kParamMidAmount]    = plainParamToNormalized(kParamMidAmount, pr.midAmount * 100.0);
        normValues_[kParamHighAmount]   = plainParamToNormalized(kParamHighAmount, pr.highAmount * 100.0);
        normValues_[kParamThresholdDb]  = plainParamToNormalized(kParamThresholdDb, pr.thresholdDb);
        normValues_[kParamHoldMs]       = plainParamToNormalized(kParamHoldMs, pr.holdMs);
        normValues_[kParamLoopMode]     = plainParamToNormalized(kParamLoopMode, static_cast<double>(pr.loopMode));

        bandEnvs_ = pr.bandEnvelopes;
        engine_->publishEnvelopes(bandEnvs_);
        syncEngineFromParams();
    }

    void syncEngineFromParams() {
        duckflow::DuckParameters p;
        auto plain = [this](ParamID id) -> float {
            return static_cast<float>(normalizedParamToPlain(id, normValues_[id]));
        };
        p.bypass          = plain(kParamBypass) >= 0.5f;
        p.triggerMode     = static_cast<duckflow::TriggerMode>(std::clamp(static_cast<int>(plain(kParamTriggerMode)), 0, 4));
        p.beatDivision    = static_cast<duckflow::BeatDivision>(std::clamp(static_cast<int>(plain(kParamBeatDivision)), 0, 15));
        p.freeHz          = plain(kParamFreeHz);
        p.globalDepth     = plain(kParamDepth) * 0.01f;
        p.swing           = plain(kParamSwing) * 0.01f;
        p.smoothingMs     = plain(kParamSmoothingMs);
        p.offsetMs        = plain(kParamOffsetMs);
        p.bandMode        = static_cast<duckflow::BandMode>(std::clamp(static_cast<int>(plain(kParamBandMode)), 0, 2));
        p.unlinkBands     = plain(kParamUnlinkBands) >= 0.5f;
        p.linearPhase     = plain(kParamLinearPhase) >= 0.5f;
        p.crossoverSlope  = static_cast<duckflow::CrossoverSlope>(std::clamp(static_cast<int>(plain(kParamSlope)), 0, 2));
        p.lowCrossHz      = plain(kParamLowCrossHz);
        p.highCrossHz     = std::max(p.lowCrossHz + 40.0f, plain(kParamHighCrossHz));
        p.lowAmount       = plain(kParamLowAmount) * 0.01f;
        p.midAmount       = plain(kParamMidAmount) * 0.01f;
        p.highAmount      = plain(kParamHighAmount) * 0.01f;
        p.thresholdDb     = plain(kParamThresholdDb);
        p.holdMs          = plain(kParamHoldMs);
        p.detectorHpfHz   = plain(kParamDetHpfHz);
        p.detectorLpfHz   = std::max(p.detectorHpfHz + 40.0f, plain(kParamDetLpfHz));
        p.midiVelocitySens= plain(kParamMidiVelSens) * 0.01f;
        p.loopMode        = static_cast<duckflow::EnvelopeTriggerLoopMode>(std::clamp(static_cast<int>(plain(kParamLoopMode)), 0, 2));
        p.outputGainDb    = plain(kParamOutputGainDb);
        p.dryWet          = plain(kParamDryWet) * 0.01f;
        engine_->setParameters(p);
    }

    std::atomic<uint32> refCount_{1};
    std::unique_ptr<duckflow::DuckFlowEngine> engine_;
    std::vector<duckflow::FactoryPreset> presets_;
    std::array<double, kNumParams> normValues_{};
    std::array<duckflow::EnvelopeData, 3> bandEnvs_{};
    std::vector<duckflow::MidiTriggerEvent> midiScratch_;

    SpeakerArrangement mainInArr_ = kSpeakerStereo;
    SpeakerArrangement scInArr_   = kSpeakerStereo;
    SpeakerArrangement mainOutArr_= kSpeakerStereo;
    bool sidechainActive_ = true;
    double sampleRate_ = 48000.0;
    int activePreset_ = -1;
    IComponentHandler* handler_ = nullptr;
};

// ==============================================================================
// DuckFlow2Factory (Implements IPluginFactory2)
// ==============================================================================
class DuckFlow2Factory final : public IPluginFactory2 {
public:
    tresult VST3_API queryInterface(const TUID _iid, void** obj) override {
        if (!obj) return kInvalidArgument;
        // FUnknown, IPluginFactory (7A4D811C-5211-4A1F-AED9-D2EE0B43BF9F),
        // IPluginFactory2 (0007B650-F24B-4C0B-A464-EDB9F00B2ABB)
        if (uidEquals(_iid, 0x00000000u, 0x00000000u, 0xC0000000u, 0x00000046u) ||
            uidEquals(_iid, 0x7A4D811Cu, 0x52114A1Fu, 0xAED9D2EEu, 0x0B43BF9Fu) ||
            uidEquals(_iid, 0x0007B650u, 0xF24B4C0Bu, 0xA464EDB9u, 0xF00B2ABBu))
        {
            addRef();
            *obj = static_cast<IPluginFactory2*>(this);
            return kResultTrue;
        }
        *obj = nullptr;
        return kNoInterface;
    }

    uint32 VST3_API addRef() override { return ++refCount_; }
    uint32 VST3_API release() override { return --refCount_; }

    tresult VST3_API getFactoryInfo(PFactoryInfo* info) override {
        if (!info) return kInvalidArgument;
        std::memset(info, 0, sizeof(PFactoryInfo));
        copyAsciiFixed(info->vendor, sizeof(info->vendor), "DuckFlow Audio");
        copyAsciiFixed(info->url, sizeof(info->url), "https://github.com/duckflow-audio/duckflow2");
        copyAsciiFixed(info->email, sizeof(info->email), "support@duckflow.audio");
        info->flags = 0x10; // kUnicode
        return kResultOk;
    }

    int32 VST3_API countClasses() override { return 1; }

    tresult VST3_API getClassInfo(int32 index, PClassInfo* info) override {
        if (index != 0 || !info) return kInvalidArgument;
        std::memset(info, 0, sizeof(PClassInfo));
        makeUID(info->cid, kDuckFlowCid1, kDuckFlowCid2, kDuckFlowCid3, kDuckFlowCid4);
        info->cardinality = 0x7FFFFFFF; // kManyInstances
        copyAsciiFixed(info->category, sizeof(info->category), "Audio Module Class");
        copyAsciiFixed(info->name, sizeof(info->name), "DuckFlow 2");
        return kResultOk;
    }

    tresult VST3_API getClassInfo2(int32 index, PClassInfo2* info) override {
        if (index != 0 || !info) return kInvalidArgument;
        std::memset(info, 0, sizeof(PClassInfo2));
        makeUID(info->cid, kDuckFlowCid1, kDuckFlowCid2, kDuckFlowCid3, kDuckFlowCid4);
        info->cardinality = 0x7FFFFFFF;
        copyAsciiFixed(info->category, sizeof(info->category), "Audio Module Class");
        copyAsciiFixed(info->name, sizeof(info->name), "DuckFlow 2");
        info->classFlags = 0;
        copyAsciiFixed(info->subCategories, sizeof(info->subCategories), "Fx|Dynamics|Modulation");
        copyAsciiFixed(info->vendor, sizeof(info->vendor), "DuckFlow Audio");
        copyAsciiFixed(info->version, sizeof(info->version), "2.0.0");
        copyAsciiFixed(info->sdkVersion, sizeof(info->sdkVersion), "VST 3.7.9");
        return kResultOk;
    }

    tresult VST3_API createInstance(FIDString cid, FIDString _iid, void** obj) override {
        if (!cid || !_iid || !obj) return kInvalidArgument;
        if (uidEquals(cid, kDuckFlowCid1, kDuckFlowCid2, kDuckFlowCid3, kDuckFlowCid4)) {
            auto* inst = new DuckFlow2SingleComponent();
            const tresult res = inst->queryInterface(_iid, obj);
            inst->release();
            return res;
        }
        *obj = nullptr;
        return kNoInterface;
    }

private:
    std::atomic<uint32> refCount_{1};
};

} // namespace vst3abi

// ==============================================================================
// Platform VST3 Module Entry / Exit & Factory Exports
// ==============================================================================
VST3_EXPORT vst3abi::IPluginFactory* VST3_API GetPluginFactory() {
    static vst3abi::DuckFlow2Factory gFactory;
    gFactory.addRef();
    return &gFactory;
}

#if defined(_WIN32)
VST3_EXPORT bool InitDll() { return true; }
VST3_EXPORT bool ExitDll() { return true; }
#elif defined(__APPLE__)
VST3_EXPORT bool bundleEntry(void*) { return true; }
VST3_EXPORT bool bundleExit() { return true; }
#else
VST3_EXPORT bool ModuleEntry(void*) { return true; }
VST3_EXPORT bool ModuleExit() { return true; }
#endif
