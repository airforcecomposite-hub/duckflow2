#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <set>

namespace duckflow {

enum class PaintTool : int {
    Pointer = 0,
    Line = 1,
    RampUp = 2,
    RampDown = 3,
    GateStep = 4,
    SineBrush = 5
};

/**
 * Custom Modern Studio Dark-Neon LookAndFeel inspired by Devious Machines Duck 2
 */
class DuckFlowLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    DuckFlowLookAndFeel();

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          juce::Slider::SliderStyle style, juce::Slider& slider) override;

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;
};

/**
 * Interactive Spectrum Analyzer + Draggable Multiband Crossover Split View + Per-Band GR Meters
 */
class MultibandSpectrumView final : public juce::Component {
public:
    explicit MultibandSpectrumView(DuckFlowAudioProcessor& proc);

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;

    void updateSpectrumFrame();
    int getSelectedEditBand() const noexcept { return activeEditBand_; }
    void setSelectedEditBand(int b);

    std::function<void(int)> onBandTabChanged;

private:
    float freqToX(float hz) const noexcept;
    float xToFreq(float x) const noexcept;

    DuckFlowAudioProcessor& processor_;
    std::array<float, SpectrumAnalyzerFifo::kHalfBins> smoothedDb_{};
    int draggingHandle_ = 0; // 0=None, 1=LowCross, 2=HighCross
    int activeEditBand_ = 0; // 0=Low/Full, 1=Mid, 2=High
};

/**
 * Interactive Multi-Segment Envelope Editor + Rolling Pre/Post/Sidechain Oscilloscope + WAV Drop Target
 */
class EnvelopeCanvasComponent final : public juce::Component,
                                      public juce::FileDragAndDropTarget {
public:
    explicit EnvelopeCanvasComponent(DuckFlowAudioProcessor& proc);

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    bool keyPressed(const juce::KeyPress& key) override;

    // FileDragAndDropTarget overrides for Trace WAV
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    void setActiveBand(int band) noexcept { activeBand_ = std::clamp(band, 0, 2); selectedNodes_.clear(); repaint(); }
    int getActiveBand() const noexcept { return activeBand_; }

    void setPaintTool(PaintTool t) noexcept { paintTool_ = t; }
    PaintTool getPaintTool() const noexcept { return paintTool_; }

    void setShowSidechainScope(bool show) noexcept { showSidechainScope_ = show; repaint(); }
    bool getShowSidechainScope() const noexcept { return showSidechainScope_; }

    void copyEnvelopeToClipboard();
    void pasteEnvelopeFromClipboard();

private:
    juce::Rectangle<float> getPlotBounds() const noexcept;
    juce::Point<float> normToScreen(float nx, float ny) const noexcept;
    juce::Point<float> screenToNorm(juce::Point<float> pt) const noexcept;
    float snapXIfEnabled(float nx, bool altOverride) const noexcept;

    int hitTestNode(juce::Point<float> pos, const EnvelopeData& env) const noexcept;
    int hitTestTensionHandle(juce::Point<float> pos, const EnvelopeData& env) const noexcept;
    int hitTestLoopMarker(juce::Point<float> pos, const EnvelopeData& env) const noexcept;
    void showSegmentContextMenu(int segIdx);

    DuckFlowAudioProcessor& processor_;
    int activeBand_ = 0;
    PaintTool paintTool_ = PaintTool::Pointer;
    bool showSidechainScope_ = true;
    bool isFileDragHover_ = false;

    std::set<int> selectedNodes_;
    int draggingNode_ = -1;
    int draggingTensionSeg_ = -1;
    int draggingLoopMarker_ = 0; // 0=none, 1=loopStart, 2=loopEnd
    float dragStartTension_ = 0.0f;
    juce::Point<float> dragStartPos_{};
    EnvelopeData dragStartEnv_{};
    bool isLassoSelecting_ = false;
    juce::Rectangle<float> lassoRect_{};
    bool isPainting_ = false;
};

/**
 * Main Resizable Plugin Editor Window (760x500 to 1680x1080)
 */
class DuckFlowAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                           private juce::Timer {
public:
    explicit DuckFlowAudioProcessorEditor(DuckFlowAudioProcessor& p);
    ~DuckFlowAudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    void timerCallback() override;
    void refreshPresetCombo();
    void applyEnvelopeOperation(int opCode);

    DuckFlowAudioProcessor& processor_;
    DuckFlowLookAndFeel lookAndFeel_;

    // Top Header Controls
    juce::ComboBox presetBox_;
    juce::TextButton prevPresetBtn_{"<"};
    juce::TextButton nextPresetBtn_{">"};
    juce::ToggleButton lockTriggerBtn_{"Lock Trig"};
    juce::TextButton undoBtn_{"Undo"};
    juce::TextButton redoBtn_{"Redo"};
    juce::TextButton copyBtn_{"Copy"};
    juce::TextButton pasteBtn_{"Paste"};
    juce::ComboBox syncGroupBox_;
    juce::ToggleButton bypassBtn_{"Bypass"};

    // Left Multiband Controls
    MultibandSpectrumView spectrumView_;
    juce::ComboBox bandModeBox_;
    juce::ToggleButton unlinkBandsBtn_{"Unlink"};
    juce::ToggleButton linearPhaseBtn_{"Lin Phase"};
    juce::ComboBox slopeBox_;
    juce::ComboBox bandSoloBox_;
    juce::Slider lowAmtSlider_, midAmtSlider_, highAmtSlider_;
    juce::Slider lowFreqSlider_, highFreqSlider_;

    // Center Canvas + Editor Toolbar
    EnvelopeCanvasComponent envelopeCanvas_;
    std::array<juce::TextButton, 6> toolButtons_{
        juce::TextButton{"Ptr"},
        juce::TextButton{"Line"},
        juce::TextButton{"Ramp+"},
        juce::TextButton{"Ramp-"},
        juce::TextButton{"Gate"},
        juce::TextButton{"Sine"}
    };
    juce::ToggleButton snapBtn_{"Snap"};
    juce::ComboBox gridDivBox_;
    juce::ToggleButton scScopeBtn_{"SC Scope"};

    juce::TextButton invBtn_{"Invert"};
    juce::TextButton revBtn_{"Reverse"};
    juce::TextButton rotLBtn_{"< Rot"};
    juce::TextButton rotRBtn_{"Rot >"};
    juce::TextButton dup2xBtn_{"2x"};
    juce::TextButton halfBtn_{"1/2x"};
    juce::TextButton randBtn_{"Random"};
    juce::TextButton traceLiveBtn_{"Trace SC"};
    juce::TextButton loadWavBtn_{"Trace WAV"};

    // Quick Shapes Strip
    std::vector<QuickShape> quickShapes_;
    std::array<juce::TextButton, 8> shapeBtns_;
    juce::TextButton moreShapesBtn_{"Shapes..."};

    // Lower Panel Controls
    juce::ComboBox triggerModeBox_;
    juce::ComboBox loopModeBox_;
    juce::ComboBox beatDivBox_;
    juce::Slider freeHzSlider_;
    juce::Slider swingSlider_;
    juce::Slider depthKnob_;
    juce::Slider smoothSlider_;
    juce::Slider offsetMsSlider_;
    juce::Slider offsetSamplesSlider_;
    juce::ToggleButton offsetUnitSplBtn_{"spl"};
    juce::Slider outGainSlider_;
    juce::Slider dryWetSlider_;

    // Detector Strip Controls
    juce::Slider threshSlider_;
    juce::Slider holdSlider_;
    juce::Slider detHpfSlider_;
    juce::Slider detLpfSlider_;
    juce::ToggleButton detFilterBtn_{"Filt"};
    juce::ToggleButton scListenBtn_{"Listen"};
    juce::Slider midiVelSlider_;

    bool triggerLedGlow_ = false;
    int triggerLedDecayFrames_ = 0;
    std::uint32_t rngSeed_ = 0x9E3779B9u;
    std::unique_ptr<juce::FileChooser> wavFileChooser_;

    // APVTS Parameter Attachments
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<ButtonAtt> bypassAtt_, lockTrigAtt_, unlinkAtt_, linPhaseAtt_;
    std::unique_ptr<ButtonAtt> offsetUnitAtt_, detFilterAtt_, scListenAtt_;
    std::unique_ptr<ComboAtt> syncGroupAtt_, bandModeAtt_, slopeAtt_, bandSoloAtt_;
    std::unique_ptr<ComboAtt> trigModeAtt_, loopModeAtt_, beatDivAtt_;
    std::unique_ptr<SliderAtt> lowAmtAtt_, midAmtAtt_, highAmtAtt_;
    std::unique_ptr<SliderAtt> lowFreqAtt_, highFreqAtt_;
    std::unique_ptr<SliderAtt> freeHzAtt_, swingAtt_, depthAtt_, smoothAtt_;
    std::unique_ptr<SliderAtt> offsetMsAtt_, offsetSplAtt_, outGainAtt_, dryWetAtt_;
    std::unique_ptr<SliderAtt> threshAtt_, holdAtt_, detHpfAtt_, detLpfAtt_, midiVelAtt_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DuckFlowAudioProcessorEditor)
};

} // namespace duckflow
