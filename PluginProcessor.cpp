#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace duckflow {

DuckFlowAudioProcessor::DuckFlowAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)
          .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts_(*this, nullptr, "DuckFlow2Parameters", createParameterLayout()),
      engine_(std::make_unique<DuckFlowEngine>()),
      presets_(FactoryPresets::getPresets())
{
    formatManager_.registerBasicFormats();
    midiScratch_.reserve(256);

    for (std::size_t b = 0; b < 3; ++b) {
        bandEnvelopes_[b].setDefaultSidechainPump();
        undoHistories_[b].reset(bandEnvelopes_[b]);
    }
    if (!presets_.empty()) {
        loadFactoryPreset(0);
    }
}

DuckFlowAudioProcessor::~DuckFlowAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout DuckFlowAudioProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"bypass", 1}, "Bypass", false));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"triggerMode", 1}, "Trigger Mode",
        juce::StringArray{"Repeat (Beat Sync)", "Free Hz", "MIDI Trigger", "Sidechain Input", "Main Audio Input"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"beatDivision", 1}, "Speed (Beat)",
        juce::StringArray{"1/64", "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8", "1/8D",
                          "1/4T", "1/4", "1/4D", "1/2", "1 Bar", "2 Bars", "4 Bars", "8 Bars"}, 9));

    {
        juce::NormalisableRange<float> hzRange(0.05f, 50.0f, 0.01f, 0.38f);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"freeHz", 1}, "Free Rate (Hz)", hzRange, 4.0f));
    }

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"depth", 1}, "Amount",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 85.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"swing", 1}, "Swing",
        juce::NormalisableRange<float>(-75.0f, 75.0f, 0.1f), 0.0f));

    {
        juce::NormalisableRange<float> smoothRange(0.1f, 50.0f, 0.1f, 0.45f);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"smoothingMs", 1}, "Smoothing (ms)", smoothRange, 4.5f));
    }

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"offsetMs", 1}, "Offset / Lookahead (ms)",
        juce::NormalisableRange<float>(-200.0f, 200.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"offsetInSamples", 1}, "Offset Unit in Samples", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"offsetSamples", 1}, "Offset / Lookahead (spl)",
        juce::NormalisableRange<float>(-9600.0f, 9600.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"bandMode", 1}, "Multiband Mode",
        juce::StringArray{"Full Band (1B)", "2-Band Split", "3-Band Split"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"unlinkBands", 1}, "Unlink Band Envelopes", false));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"linearPhase", 1}, "Linear Phase Crossover", false));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"slope", 1}, "Crossover Slope",
        juce::StringArray{"12 dB/oct", "24 dB/oct", "48 dB/oct"}, 1));

    {
        juce::NormalisableRange<float> lowFreqRange(20.0f, 2000.0f, 1.0f, 0.35f);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"lowCrossHz", 1}, "Low Crossover (Hz)", lowFreqRange, 140.0f));

        juce::NormalisableRange<float> highFreqRange(200.0f, 18000.0f, 1.0f, 0.35f);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"highCrossHz", 1}, "High Crossover (Hz)", highFreqRange, 2600.0f));
    }

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"lowAmount", 1}, "Low Band Amount",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"midAmount", 1}, "Mid Band Amount",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 70.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"highAmount", 1}, "High Band Amount",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 30.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"bandSolo", 1}, "Band Solo",
        juce::StringArray{"Off", "Solo Low", "Solo Mid", "Solo High"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"thresholdDb", 1}, "Trigger Threshold (dB)",
        juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f), -24.0f));

    {
        juce::NormalisableRange<float> holdRange(0.0f, 500.0f, 0.1f, 0.45f);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"holdMs", 1}, "Trigger Hold (ms)", holdRange, 35.0f));

        juce::NormalisableRange<float> hpfRange(20.0f, 2000.0f, 1.0f, 0.35f);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"detHpfHz", 1}, "Detector HPF (Hz)", hpfRange, 30.0f));

        juce::NormalisableRange<float> lpfRange(100.0f, 20000.0f, 1.0f, 0.35f);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"detLpfHz", 1}, "Detector LPF (Hz)", lpfRange, 16000.0f));
    }

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"detFilterOn", 1}, "Detector Filter Enable", true));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"scListen", 1}, "Sidechain Listen", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"midiVelSens", 1}, "MIDI Velocity Sensitivity",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 75.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"loopMode", 1}, "Trigger Loop Mode",
        juce::StringArray{"One-Shot", "Loop", "Loop + Release"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"outputGainDb", 1}, "Output Gain (dB)",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"dryWet", 1}, "Mix (Dry/Wet)",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"syncGroup", 1}, "Sync Group",
        juce::StringArray{"Off", "Group A", "Group B", "Group C", "Group D"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"lockTrigger", 1}, "Lock Trigger Settings", false));

    return {params.begin(), params.end()};
}

void DuckFlowAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    currentSampleRate_ = sampleRate;
    engine_->setParameters(readParametersFromAPVTS());
    engine_->prepare(sampleRate, samplesPerBlock);
    midiScratch_.reserve(512);

    const int lat = engine_->getReportedLatencySamples();
    setLatencySamples(lat);
    lastReportedLatency_ = lat;
}

void DuckFlowAudioProcessor::releaseResources() {
    engine_->reset();
}

bool DuckFlowAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto& mainIn = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo()) {
        return false;
    }
    if (mainIn != mainOut) {
        return false;
    }

    if (layouts.inputBuses.size() > 1) {
        const auto& sc = layouts.inputBuses[1];
        if (!sc.isDisabled() &&
            sc != juce::AudioChannelSet::mono() &&
            sc != juce::AudioChannelSet::stereo())
        {
            return false;
        }
    }
    return true;
}

DuckParameters DuckFlowAudioProcessor::readParametersFromAPVTS() const noexcept {
    DuckParameters p;
    auto load = [this](const char* id) -> float {
        if (auto* v = apvts_.getRawParameterValue(id)) return v->load(std::memory_order_relaxed);
        return 0.0f;
    };

    p.bypass = load("bypass") >= 0.5f;
    p.triggerMode = static_cast<TriggerMode>(std::clamp(static_cast<int>(std::lround(load("triggerMode"))), 0, 4));
    p.beatDivision = static_cast<BeatDivision>(std::clamp(static_cast<int>(std::lround(load("beatDivision"))), 0, static_cast<int>(BeatDivision::Count) - 1));
    p.freeHz = load("freeHz");
    p.globalDepth = load("depth") * 0.01f;
    p.swing = load("swing") * 0.01f;
    p.smoothingMs = load("smoothingMs");
    p.offsetMs = load("offsetMs");
    p.offsetInSamples = load("offsetInSamples") >= 0.5f;
    p.offsetSamples = static_cast<int>(std::lround(load("offsetSamples")));

    p.bandMode = static_cast<BandMode>(std::clamp(static_cast<int>(std::lround(load("bandMode"))), 0, 2));
    p.unlinkBands = load("unlinkBands") >= 0.5f;
    p.linearPhase = load("linearPhase") >= 0.5f;
    p.crossoverSlope = static_cast<CrossoverSlope>(std::clamp(static_cast<int>(std::lround(load("slope"))), 0, 2));
    p.lowCrossHz = load("lowCrossHz");
    p.highCrossHz = std::max(p.lowCrossHz + 40.0f, load("highCrossHz"));
    p.lowAmount = load("lowAmount") * 0.01f;
    p.midAmount = load("midAmount") * 0.01f;
    p.highAmount = load("highAmount") * 0.01f;
    p.bandSolo = std::clamp(static_cast<int>(std::lround(load("bandSolo"))), 0, 3);

    p.thresholdDb = load("thresholdDb");
    p.holdMs = load("holdMs");
    p.detectorHpfHz = load("detHpfHz");
    p.detectorLpfHz = std::max(p.detectorHpfHz + 40.0f, load("detLpfHz"));
    p.detectorFilterOn = load("detFilterOn") >= 0.5f;
    p.sidechainListen = load("scListen") >= 0.5f;
    p.midiVelocitySens = load("midiVelSens") * 0.01f;
    p.loopMode = static_cast<EnvelopeTriggerLoopMode>(std::clamp(static_cast<int>(std::lround(load("loopMode"))), 0, 2));

    p.outputGainDb = load("outputGainDb");
    p.dryWet = load("dryWet") * 0.01f;
    p.syncGroup = std::clamp(static_cast<int>(std::lround(load("syncGroup"))), 0, 4);
    p.lockTriggerSettings = load("lockTrigger") >= 0.5f;
    return p;
}

void DuckFlowAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    (void)noDenormals;

    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0) return;

    const int totalInCh = getTotalNumInputChannels();
    const int totalOutCh = getTotalNumOutputChannels();
    for (int ch = totalInCh; ch < totalOutCh; ++ch) {
        buffer.clear(ch, 0, numSamples);
    }

    const DuckParameters params = readParametersFromAPVTS();
    engine_->setParameters(params);

    const int desiredLatency = engine_->getReportedLatencySamples();
    if (desiredLatency != lastReportedLatency_) {
        setLatencySamples(desiredLatency);
        lastReportedLatency_ = desiredLatency;
    }

    juce::AudioBuffer<float> mainBus = getBusBuffer(buffer, true, 0);
    if (mainBus.getNumChannels() == 0) return;

    float* mainL = mainBus.getWritePointer(0);
    float* mainR = (mainBus.getNumChannels() > 1) ? mainBus.getWritePointer(1) : nullptr;

    const float* scL = nullptr;
    const float* scR = nullptr;
    if (getBusCount(true) > 1) {
        const auto* scBusObj = getBus(true, 1);
        if (scBusObj != nullptr && scBusObj->isEnabled()) {
            juce::AudioBuffer<float> scBus = getBusBuffer(buffer, true, 1);
            if (scBus.getNumChannels() > 0) {
                scL = scBus.getReadPointer(0);
                scR = (scBus.getNumChannels() > 1) ? scBus.getReadPointer(1) : scL;
            }
        }
    }

    HostTransportInfo transport{};
    if (auto* ph = getPlayHead()) {
        if (const auto pos = ph->getPosition()) {
            transport.isPlaying = pos->getIsPlaying();
            if (const auto bpmOpt = pos->getBpm()) {
                transport.bpm = *bpmOpt;
                lastKnownBpm_ = transport.bpm;
            } else {
                transport.bpm = lastKnownBpm_;
            }
            if (const auto ppqOpt = pos->getPpqPosition()) {
                transport.ppqPosition = *ppqOpt;
            }
            if (const auto sigOpt = pos->getTimeSignature()) {
                transport.timeSigNumerator = sigOpt->numerator;
                transport.timeSigDenominator = sigOpt->denominator;
            }
        }
    }

    midiScratch_.clear();
    for (const auto metadata : midiMessages) {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn()) {
            MidiTriggerEvent ev;
            ev.sampleOffset = std::clamp(metadata.samplePosition, 0, numSamples - 1);
            ev.isNoteOn = true;
            ev.noteNumber = msg.getNoteNumber();
            ev.velocity = msg.getFloatVelocity();
            midiScratch_.push_back(ev);
        } else if (msg.isNoteOff()) {
            MidiTriggerEvent ev;
            ev.sampleOffset = std::clamp(metadata.samplePosition, 0, numSamples - 1);
            ev.isNoteOn = false;
            ev.noteNumber = msg.getNoteNumber();
            ev.velocity = 0.0f;
            midiScratch_.push_back(ev);
        }
    }

    engine_->process(
        mainL, mainR, scL, scR, numSamples, transport,
        midiScratch_.empty() ? nullptr : midiScratch_.data(),
        static_cast<int>(midiScratch_.size()));
}

void DuckFlowAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    DuckParameters p = readParametersFromAPVTS();
    p.bypass = true;
    engine_->setParameters(p);
    processBlock(buffer, midiMessages);
}

int DuckFlowAudioProcessor::getNumPrograms() {
    return std::max(1, static_cast<int>(presets_.size()));
}

const juce::String DuckFlowAudioProcessor::getProgramName(int index) {
    if (index >= 0 && index < static_cast<int>(presets_.size())) {
        return presets_[static_cast<std::size_t>(index)].name;
    }
    return "Default";
}

void DuckFlowAudioProcessor::setCurrentProgram(int index) {
    loadFactoryPreset(index);
}

void DuckFlowAudioProcessor::loadFactoryPreset(int index) {
    if (index < 0 || index >= static_cast<int>(presets_.size())) return;
    currentPresetIndex_ = index;
    const auto& pr = presets_[static_cast<std::size_t>(index)];

    auto setParam = [this](const char* id, float plainVal) {
        if (auto* param = apvts_.getParameter(id)) {
            param->setValueNotifyingHost(param->convertTo0to1(plainVal));
        }
    };

    const bool lockTrig = apvts_.getRawParameterValue("lockTrigger")->load() >= 0.5f;
    if (!lockTrig) {
        setParam("triggerMode", static_cast<float>(pr.triggerMode));
        setParam("thresholdDb", pr.thresholdDb);
        setParam("holdMs", pr.holdMs);
        setParam("loopMode", static_cast<float>(pr.loopMode));
    }

    setParam("beatDivision", static_cast<float>(pr.division));
    setParam("freeHz", pr.freeHz);
    setParam("depth", pr.globalDepth * 100.0f);
    setParam("swing", pr.swing * 100.0f);
    setParam("smoothingMs", pr.smoothingMs);
    setParam("offsetMs", pr.offsetMs);
    setParam("bandMode", static_cast<float>(pr.bandMode));
    setParam("unlinkBands", pr.unlinkBands ? 1.0f : 0.0f);
    setParam("linearPhase", pr.linearPhase ? 1.0f : 0.0f);
    setParam("slope", static_cast<float>(pr.slope));
    setParam("lowCrossHz", pr.lowCrossHz);
    setParam("highCrossHz", pr.highCrossHz);
    setParam("lowAmount", pr.lowAmount * 100.0f);
    setParam("midAmount", pr.midAmount * 100.0f);
    setParam("highAmount", pr.highAmount * 100.0f);

    setAllBandEnvelopes(pr.bandEnvelopes, true);
}

EnvelopeData DuckFlowAudioProcessor::getBandEnvelope(int bandIndex) const {
    std::lock_guard<std::mutex> lock(envMutex_);
    return bandEnvelopes_[static_cast<std::size_t>(std::clamp(bandIndex, 0, 2))];
}

void DuckFlowAudioProcessor::setBandEnvelope(int bandIndex, const EnvelopeData& env, bool recordUndo) {
    const int b = std::clamp(bandIndex, 0, 2);
    {
        std::lock_guard<std::mutex> lock(envMutex_);
        EnvelopeData cleaned = env;
        cleaned.sortAndValidate();
        const bool unlinked = apvts_.getRawParameterValue("unlinkBands")->load() >= 0.5f;
        if (!unlinked) {
            for (std::size_t k = 0; k < 3; ++k) {
                bandEnvelopes_[k] = cleaned;
                if (recordUndo) undoHistories_[k].push(cleaned);
            }
        } else {
            bandEnvelopes_[static_cast<std::size_t>(b)] = cleaned;
            if (recordUndo) undoHistories_[static_cast<std::size_t>(b)].push(cleaned);
        }
    }
    pushEnvelopesToEngineAndGroup();
}

void DuckFlowAudioProcessor::setAllBandEnvelopes(const std::array<EnvelopeData, 3>& envs, bool recordUndo) {
    {
        std::lock_guard<std::mutex> lock(envMutex_);
        for (std::size_t b = 0; b < 3; ++b) {
            bandEnvelopes_[b] = envs[b];
            bandEnvelopes_[b].sortAndValidate();
            if (recordUndo) {
                undoHistories_[b].push(bandEnvelopes_[b]);
            }
        }
    }
    pushEnvelopesToEngineAndGroup();
}

void DuckFlowAudioProcessor::pushEnvelopesToEngineAndGroup() {
    std::array<EnvelopeData, 3> copy;
    {
        std::lock_guard<std::mutex> lock(envMutex_);
        copy = bandEnvelopes_;
    }
    engine_->publishEnvelopes(copy);

    const int grp = static_cast<int>(std::lround(apvts_.getRawParameterValue("syncGroup")->load()));
    const bool unlink = apvts_.getRawParameterValue("unlinkBands")->load() >= 0.5f;
    if (grp >= 1 && grp <= SyncGroupHub::kNumGroups) {
        SyncGroupHub::instance().publish(grp, copy, unlink, syncGroupRevision_);
    }
}

bool DuckFlowAudioProcessor::pollSyncGroupUpdates() {
    const int grp = static_cast<int>(std::lround(apvts_.getRawParameterValue("syncGroup")->load()));
    if (grp < 1 || grp > SyncGroupHub::kNumGroups) return false;

    std::array<EnvelopeData, 3> incoming;
    bool incomingUnlink = false;
    if (SyncGroupHub::instance().pollIfUpdated(grp, syncGroupRevision_, incoming, incomingUnlink)) {
        {
            std::lock_guard<std::mutex> lock(envMutex_);
            bandEnvelopes_ = incoming;
        }
        engine_->publishEnvelopes(incoming);
        return true;
    }
    return false;
}

bool DuckFlowAudioProcessor::undoEnvelope(int activeBand) {
    const bool unlinked = apvts_.getRawParameterValue("unlinkBands")->load() >= 0.5f;
    const int b = unlinked ? std::clamp(activeBand, 0, 2) : 0;
    EnvelopeData restored;
    {
        std::lock_guard<std::mutex> lock(envMutex_);
        if (!undoHistories_[static_cast<std::size_t>(b)].undo(restored)) return false;
        if (!unlinked) {
            for (std::size_t k = 0; k < 3; ++k) bandEnvelopes_[k] = restored;
        } else {
            bandEnvelopes_[static_cast<std::size_t>(b)] = restored;
        }
    }
    pushEnvelopesToEngineAndGroup();
    return true;
}

bool DuckFlowAudioProcessor::redoEnvelope(int activeBand) {
    const bool unlinked = apvts_.getRawParameterValue("unlinkBands")->load() >= 0.5f;
    const int b = unlinked ? std::clamp(activeBand, 0, 2) : 0;
    EnvelopeData restored;
    {
        std::lock_guard<std::mutex> lock(envMutex_);
        if (!undoHistories_[static_cast<std::size_t>(b)].redo(restored)) return false;
        if (!unlinked) {
            for (std::size_t k = 0; k < 3; ++k) bandEnvelopes_[k] = restored;
        } else {
            bandEnvelopes_[static_cast<std::size_t>(b)] = restored;
        }
    }
    pushEnvelopesToEngineAndGroup();
    return true;
}

bool DuckFlowAudioProcessor::canUndoEnvelope(int activeBand) const {
    const bool unlinked = apvts_.getRawParameterValue("unlinkBands")->load() >= 0.5f;
    const int b = unlinked ? std::clamp(activeBand, 0, 2) : 0;
    std::lock_guard<std::mutex> lock(envMutex_);
    return undoHistories_[static_cast<std::size_t>(b)].canUndo();
}

bool DuckFlowAudioProcessor::canRedoEnvelope(int activeBand) const {
    const bool unlinked = apvts_.getRawParameterValue("unlinkBands")->load() >= 0.5f;
    const int b = unlinked ? std::clamp(activeBand, 0, 2) : 0;
    std::lock_guard<std::mutex> lock(envMutex_);
    return undoHistories_[static_cast<std::size_t>(b)].canRedo();
}

bool DuckFlowAudioProcessor::traceEnvelopeFromAudioFile(const juce::File& file, int targetBand, bool invertForDucking) {
    std::vector<float> mono;
    double sr = currentSampleRate_;

    if (std::unique_ptr<juce::AudioFormatReader> reader{formatManager_.createReaderFor(file)}) {
        const auto numFrames = static_cast<int>(std::min<juce::int64>(reader->lengthInSamples, 10 * 192000));
        if (numFrames >= 32) {
            juce::AudioBuffer<float> temp(static_cast<int>(reader->numChannels), numFrames);
            reader->read(&temp, 0, numFrames, 0, true, true);
            mono.assign(static_cast<std::size_t>(numFrames), 0.0f);
            const float invCh = 1.0f / static_cast<float>(std::max(1, temp.getNumChannels()));
            for (int c = 0; c < temp.getNumChannels(); ++c) {
                const float* rp = temp.getReadPointer(c);
                for (int i = 0; i < numFrames; ++i) {
                    mono[static_cast<std::size_t>(i)] += rp[i] * invCh;
                }
            }
        }
    } else {
        EnvelopeTracer::loadWavMonoMix(file.getFullPathName().toStdString(), mono, sr);
    }

    if (mono.size() < 32) return false;

    const DuckParameters p = readParametersFromAPVTS();
    // Duck 2 Multiband Trace: when envelopes are unlinked in 2/3-band mode, split WAV through crossover!
    if (p.unlinkBands && p.bandMode != BandMode::FullBand) {
        MultibandCrossover traceXover;
        traceXover.prepare(sr);
        traceXover.setFrequencies(p.lowCrossHz, p.highCrossHz, p.crossoverSlope);

        // Use zero-phase Linear-Phase FIR with exact 127-sample group-delay stripping so Low/Mid/High align at sample 0!
        constexpr std::size_t kFirLat = static_cast<std::size_t>(MultibandCrossover::kLinearPhaseLatency);
        const std::size_t nFrames = mono.size();
        std::vector<float> lowBuf(nFrames), midBuf(nFrames), highBuf(nFrames);
        for (std::size_t i = 0; i < nFrames + kFirLat; ++i) {
            const float x = (i < nFrames) ? mono[i] : 0.0f;
            float l = 0.0f, m = 0.0f, h = 0.0f;
            traceXover.processSample(0, x, p.bandMode, p.crossoverSlope, true, l, m, h);
            if (i >= kFirLat) {
                lowBuf[i - kFirLat]  = l;
                midBuf[i - kFirLat]  = m;
                highBuf[i - kFirLat] = h;
            }
        }
        std::array<EnvelopeData, 3> multiEnvs;
        multiEnvs[0] = EnvelopeTracer::traceFromMonoBuffer(lowBuf.data(), lowBuf.size(), invertForDucking);
        multiEnvs[1] = (p.bandMode == BandMode::ThreeBand)
                     ? EnvelopeTracer::traceFromMonoBuffer(midBuf.data(), midBuf.size(), invertForDucking)
                     : multiEnvs[0];
        multiEnvs[2] = EnvelopeTracer::traceFromMonoBuffer(highBuf.data(), highBuf.size(), invertForDucking);
        setAllBandEnvelopes(multiEnvs, true);
        return true;
    }

    const EnvelopeData traced = EnvelopeTracer::traceFromMonoBuffer(mono.data(), mono.size(), invertForDucking);
    setBandEnvelope(targetBand, traced, true);
    return true;
}

void DuckFlowAudioProcessor::triggerLiveTrace(double beatsToCapture) {
    const double safeBpm = std::clamp(lastKnownBpm_, 30.0, 300.0);
    const int samples = static_cast<int>(std::round((60.0 / safeBpm) * beatsToCapture * currentSampleRate_));
    engine_->startLiveTrace(samples);
}

bool DuckFlowAudioProcessor::checkAndApplyCompletedLiveTrace(int activeBand, bool invertForDucking) {
    EnvelopeData traced;
    if (engine_->pollCompletedLiveTrace(traced, invertForDucking)) {
        setBandEnvelope(activeBand, traced, true);
        return true;
    }
    return false;
}

void DuckFlowAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts_.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (!xml) return;

    xml->setAttribute("presetIndex", currentPresetIndex_);
    xml->setAttribute("snapToGrid", snapToGrid_);
    xml->setAttribute("gridDivisions", gridDivisions_);
    xml->setAttribute("editorWidth", editorWidth_);
    xml->setAttribute("editorHeight", editorHeight_);

    {
        std::lock_guard<std::mutex> lock(envMutex_);
        xml->setAttribute("envBand0", juce::String(bandEnvelopes_[0].serialize()));
        xml->setAttribute("envBand1", juce::String(bandEnvelopes_[1].serialize()));
        xml->setAttribute("envBand2", juce::String(bandEnvelopes_[2].serialize()));
    }

    copyXmlToBinary(*xml, destData);
}

void DuckFlowAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState == nullptr || !xmlState->hasTagName(apvts_.state.getType())) {
        return;
    }

    currentPresetIndex_ = xmlState->getIntAttribute("presetIndex", 0);
    snapToGrid_ = xmlState->getBoolAttribute("snapToGrid", true);
    gridDivisions_ = std::clamp(xmlState->getIntAttribute("gridDivisions", 8), 2, 64);
    editorWidth_ = std::clamp(xmlState->getIntAttribute("editorWidth", 980), 760, 1920);
    editorHeight_ = std::clamp(xmlState->getIntAttribute("editorHeight", 640), 500, 1200);

    std::array<EnvelopeData, 3> loadedEnvs = bandEnvelopes_;
    for (int b = 0; b < 3; ++b) {
        const juce::String key = "envBand" + juce::String(b);
        if (xmlState->hasAttribute(key)) {
            loadedEnvs[static_cast<std::size_t>(b)].deserialize(xmlState->getStringAttribute(key).toStdString());
        }
    }

    apvts_.replaceState(juce::ValueTree::fromXml(*xmlState));
    setAllBandEnvelopes(loadedEnvs, false);
    for (std::size_t b = 0; b < 3; ++b) {
        undoHistories_[b].reset(loadedEnvs[b]);
    }
}

juce::AudioProcessorEditor* DuckFlowAudioProcessor::createEditor() {
    return new DuckFlowAudioProcessorEditor(*this);
}

} // namespace duckflow

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new duckflow::DuckFlowAudioProcessor();
}
