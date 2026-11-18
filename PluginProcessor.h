#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "DSP/DuckFlowEngine.h"
#include <memory>
#include <mutex>
#include <array>

namespace duckflow {

class DuckFlowAudioProcessor final : public juce::AudioProcessor {
public:
    DuckFlowAudioProcessor();
    ~DuckFlowAudioProcessor() override;

    // AudioProcessor overrides
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;
    void processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DuckFlow 2"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.25; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPresetIndex_; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int /*index*/, const juce::String& /*newName*/) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Parameter tree & Presets
    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts_; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    const std::vector<FactoryPreset>& getFactoryPresets() const noexcept { return presets_; }
    void loadFactoryPreset(int index);

    // Thread-safe Envelope Editing & Undo/Redo API for GUI
    EnvelopeData getBandEnvelope(int bandIndex) const;
    void setBandEnvelope(int bandIndex, const EnvelopeData& env, bool recordUndo = true);
    void setAllBandEnvelopes(const std::array<EnvelopeData, 3>& envs, bool recordUndo = true);

    bool undoEnvelope(int activeBand);
    bool redoEnvelope(int activeBand);
    bool canUndoEnvelope(int activeBand) const;
    bool canRedoEnvelope(int activeBand) const;

    // WAV file & Live Sidechain Trace
    bool traceEnvelopeFromAudioFile(const juce::File& file, int targetBand, bool invertForDucking = true);
    void triggerLiveTrace(double beatsToCapture = 1.0);
    bool checkAndApplyCompletedLiveTrace(int activeBand, bool invertForDucking = true);

    // Sync Group polling (called from GUI timer)
    bool pollSyncGroupUpdates();

    // Persistent UI preferences saved per instance
    bool isSnapToGridEnabled() const noexcept { return snapToGrid_; }
    void setSnapToGridEnabled(bool enabled) noexcept { snapToGrid_ = enabled; }
    int getGridDivisions() const noexcept { return gridDivisions_; }
    void setGridDivisions(int divs) noexcept { gridDivisions_ = std::clamp(divs, 2, 64); }
    int getSavedEditorWidth() const noexcept { return editorWidth_; }
    int getSavedEditorHeight() const noexcept { return editorHeight_; }
    void setSavedEditorSize(int w, int h) noexcept {
        editorWidth_ = std::clamp(w, 760, 1920);
        editorHeight_ = std::clamp(h, 500, 1200);
    }

    DuckFlowEngine& getEngine() noexcept { return *engine_; }
    const DuckFlowEngine& getEngine() const noexcept { return *engine_; }

private:
    DuckParameters readParametersFromAPVTS() const noexcept;
    void pushEnvelopesToEngineAndGroup();

    juce::AudioProcessorValueTreeState apvts_;
    std::unique_ptr<DuckFlowEngine> engine_;
    juce::AudioFormatManager formatManager_;

    std::vector<FactoryPreset> presets_;
    int currentPresetIndex_ = 0;

    mutable std::mutex envMutex_;
    std::array<EnvelopeData, 3> bandEnvelopes_{};
    std::array<EnvelopeUndoHistory, 3> undoHistories_{};
    std::uint64_t syncGroupRevision_ = 0;

    // Pre-allocated MIDI event scratch buffer for audio thread (zero allocation in processBlock)
    std::vector<MidiTriggerEvent> midiScratch_;

    bool snapToGrid_ = true;
    int gridDivisions_ = 8;
    int editorWidth_ = 980;
    int editorHeight_ = 640;
    int lastReportedLatency_ = -1;
    double currentSampleRate_ = 48000.0;
    double lastKnownBpm_ = 120.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DuckFlowAudioProcessor)
};

} // namespace duckflow
