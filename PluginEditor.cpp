#include "PluginEditor.h"

namespace duckflow {

namespace {
    const juce::Colour kBgDark      {0xff11141c};
    const juce::Colour kPanelBg     {0xff181d28};
    const juce::Colour kCardBg      {0xff202736};
    const juce::Colour kBorderCol   {0xff2d374d};
    const juce::Colour kTextBright  {0xffeef2f9};
    const juce::Colour kTextMuted   {0xff8e9bb5};
    const juce::Colour kAccentCyan  {0xff2ce5d8};
    const juce::Colour kBandLowCol  {0xffffb340};
    const juce::Colour kBandMidCol  {0xff2ce5d8};
    const juce::Colour kBandHighCol {0xffff5e8e};
    const juce::Colour kScGreen     {0xff52e085};

    inline juce::Font makeUiFont(float height, bool bold = false) {
        auto opts = juce::FontOptions(height);
        if (bold) opts = opts.withStyle("Bold");
        return juce::Font(opts);
    }

    inline juce::Colour colourForBand(int band, bool unlinked) {
        if (!unlinked) return kAccentCyan;
        if (band == 0) return kBandLowCol;
        if (band == 1) return kBandMidCol;
        return kBandHighCol;
    }
}

// ==============================================================================
// DuckFlowLookAndFeel
// ==============================================================================
DuckFlowLookAndFeel::DuckFlowLookAndFeel() {
    setColour(juce::ResizableWindow::backgroundColourId, kBgDark);
    setColour(juce::ComboBox::backgroundColourId, kCardBg);
    setColour(juce::ComboBox::outlineColourId, kBorderCol);
    setColour(juce::ComboBox::textColourId, kTextBright);
    setColour(juce::PopupMenu::backgroundColourId, kPanelBg);
    setColour(juce::PopupMenu::textColourId, kTextBright);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, kAccentCyan.withAlpha(0.25f));
    setColour(juce::TextButton::buttonColourId, kCardBg);
    setColour(juce::TextButton::buttonOnColourId, kAccentCyan.withAlpha(0.28f));
    setColour(juce::TextButton::textColourOffId, kTextBright);
    setColour(juce::TextButton::textColourOnId, kAccentCyan);
    setColour(juce::Slider::textBoxTextColourId, kTextBright);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::rotarySliderFillColourId, kAccentCyan);
}

void DuckFlowLookAndFeel::drawRotarySlider(
    juce::Graphics& g, int x, int y, int width, int height,
    float sliderPos, float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
{
    const float radius = static_cast<float>(std::min(width, height)) * 0.42f;
    const float cx = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
    const float cy = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    g.setColour(kCardBg.darker(0.25f));
    g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

    juce::Path bgArc;
    bgArc.addCentredArc(cx, cy, radius, radius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(kBorderCol);
    g.strokePath(bgArc, juce::PathStrokeType(3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const juce::Colour arcCol = slider.findColour(juce::Slider::rotarySliderFillColourId, false);
    const juce::Colour activeCol = (arcCol.getAlpha() > 0) ? arcCol : kAccentCyan;

    juce::Path valArc;
    valArc.addCentredArc(cx, cy, radius, radius, 0.0f, rotaryStartAngle, angle, true);
    g.setColour(activeCol);
    g.strokePath(valArc, juce::PathStrokeType(3.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Pointer tick
    juce::Path tick;
    tick.addRoundedRectangle(-1.5f, -radius + 2.0f, 3.0f, radius * 0.42f, 1.2f);
    g.setColour(kTextBright);
    g.fillPath(tick, juce::AffineTransform::rotation(angle).translated(cx, cy));
}

void DuckFlowLookAndFeel::drawLinearSlider(
    juce::Graphics& g, int x, int y, int width, int height,
    float sliderPos, float minSliderPos, float maxSliderPos,
    juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style == juce::Slider::LinearVertical || style == juce::Slider::LinearHorizontal) {
        const bool isVert = (style == juce::Slider::LinearVertical);
        const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                                   static_cast<float>(width), static_cast<float>(height));
        const float trackThick = 5.0f;
        juce::Rectangle<float> track = isVert
            ? bounds.withSizeKeepingCentre(trackThick, bounds.getHeight() - 8.0f)
            : bounds.withSizeKeepingCentre(bounds.getWidth() - 8.0f, trackThick);

        g.setColour(kBgDark);
        g.fillRoundedRectangle(track, 2.5f);

        const juce::Colour fillCol = slider.isColourSpecified(juce::Slider::trackColourId)
                                   ? slider.findColour(juce::Slider::trackColourId)
                                   : kAccentCyan;
        g.setColour(fillCol);
        if (isVert) {
            const float top = std::clamp(sliderPos, track.getY(), track.getBottom());
            g.fillRoundedRectangle(track.withTop(top), 2.5f);
            g.setColour(kTextBright);
            g.fillEllipse(track.getCentreX() - 5.5f, top - 5.5f, 11.0f, 11.0f);
        } else {
            const float right = std::clamp(sliderPos, track.getX(), track.getRight());
            g.fillRoundedRectangle(track.withRight(right), 2.5f);
            g.setColour(kTextBright);
            g.fillEllipse(right - 5.5f, track.getCentreY() - 5.5f, 11.0f, 11.0f);
        }
    } else {
        LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
    }
}

void DuckFlowLookAndFeel::drawButtonBackground(
    juce::Graphics& g, juce::Button& button, const juce::Colour& /*bgCol*/,
    bool isHighlighted, bool isDown)
{
    const auto r = button.getLocalBounds().toFloat().reduced(1.0f);
    const bool toggled = button.getToggleState();
    juce::Colour fill = toggled ? kAccentCyan.withAlpha(0.24f) : kCardBg;
    if (isDown) fill = fill.brighter(0.18f);
    else if (isHighlighted) fill = fill.brighter(0.08f);

    g.setColour(fill);
    g.fillRoundedRectangle(r, 4.5f);
    g.setColour(toggled ? kAccentCyan : kBorderCol);
    g.drawRoundedRectangle(r, 4.5f, toggled ? 1.4f : 1.0f);
}

// ==============================================================================
// MultibandSpectrumView
// ==============================================================================
MultibandSpectrumView::MultibandSpectrumView(DuckFlowAudioProcessor& proc)
    : processor_(proc)
{
    smoothedDb_.fill(-84.0f);
}

void MultibandSpectrumView::setSelectedEditBand(int b) {
    activeEditBand_ = std::clamp(b, 0, 2);
    if (onBandTabChanged) onBandTabChanged(activeEditBand_);
    repaint();
}

float MultibandSpectrumView::freqToX(float hz) const noexcept {
    const float w = static_cast<float>(std::max(1, getWidth() - 36)); // Reserve 36px on right for GR meters
    const float clamped = std::clamp(hz, 20.0f, 20000.0f);
    const float norm = std::log10(clamped / 20.0f) / 3.0f; // log10(20000/20) = 3.0
    return 4.0f + norm * (w - 8.0f);
}

float MultibandSpectrumView::xToFreq(float x) const noexcept {
    const float w = static_cast<float>(std::max(1, getWidth() - 36));
    const float norm = std::clamp((x - 4.0f) / std::max(1.0f, w - 8.0f), 0.0f, 1.0f);
    return 20.0f * std::pow(10.0f, norm * 3.0f);
}

void MultibandSpectrumView::updateSpectrumFrame() {
    std::array<float, SpectrumAnalyzerFifo::kHalfBins> raw{};
    if (processor_.getEngine().getSpectrumAnalyzer().computeMagnitudesDb(raw)) {
        for (std::size_t i = 0; i < raw.size(); ++i) {
            const float target = raw[i];
            if (target > smoothedDb_[i]) {
                smoothedDb_[i] = 0.45f * smoothedDb_[i] + 0.55f * target;
            } else {
                smoothedDb_[i] = 0.85f * smoothedDb_[i] + 0.15f * target;
            }
        }
    }
    repaint();
}

void MultibandSpectrumView::paint(juce::Graphics& g) {
    const auto full = getLocalBounds().toFloat();
    const auto specArea = full.withTrimmedRight(38.0f).reduced(2.0f);
    const auto grArea = full.withLeft(full.getRight() - 34.0f).reduced(2.0f);

    g.setColour(kBgDark);
    g.fillRoundedRectangle(specArea, 5.0f);

    const auto& apvts = processor_.getAPVTS();
    const int bandMode = static_cast<int>(std::lround(apvts.getRawParameterValue("bandMode")->load()));
    const bool unlinked = apvts.getRawParameterValue("unlinkBands")->load() >= 0.5f;
    const float lowHz = apvts.getRawParameterValue("lowCrossHz")->load();
    const float highHz = apvts.getRawParameterValue("highCrossHz")->load();

    // Draw Band Tints & Active Band Tab Header when Multiband is active
    const float xLow = freqToX(lowHz);
    const float xHigh = freqToX(highHz);

    if (bandMode >= 1) {
        const float split1 = std::clamp(xLow, specArea.getX(), specArea.getRight());
        const float split2 = (bandMode == 2) ? std::clamp(xHigh, split1 + 6.0f, specArea.getRight()) : specArea.getRight();

        g.setColour(kBandLowCol.withAlpha(activeEditBand_ == 0 && unlinked ? 0.16f : 0.06f));
        g.fillRect(juce::Rectangle<float>(specArea.getX(), specArea.getY(), split1 - specArea.getX(), specArea.getHeight()));

        if (bandMode == 2) {
            g.setColour(kBandMidCol.withAlpha(activeEditBand_ == 1 && unlinked ? 0.16f : 0.06f));
            g.fillRect(juce::Rectangle<float>(split1, specArea.getY(), split2 - split1, specArea.getHeight()));
            g.setColour(kBandHighCol.withAlpha(activeEditBand_ == 2 && unlinked ? 0.16f : 0.06f));
            g.fillRect(juce::Rectangle<float>(split2, specArea.getY(), specArea.getRight() - split2, specArea.getHeight()));
        } else {
            g.setColour(kBandHighCol.withAlpha(activeEditBand_ == 2 && unlinked ? 0.16f : 0.06f));
            g.fillRect(juce::Rectangle<float>(split1, specArea.getY(), specArea.getRight() - split1, specArea.getHeight()));
        }
    }

    // Draw FFT Spectrum Curve
    juce::Path specPath;
    const double sr = std::max(8000.0, processor_.getSampleRate());
    bool started = false;
    for (int b = 1; b < SpectrumAnalyzerFifo::kHalfBins; ++b) {
        const float binHz = static_cast<float>(b * sr / static_cast<double>(SpectrumAnalyzerFifo::kFftSize));
        if (binHz < 20.0f || binHz > 20000.0f) continue;
        const float px = freqToX(binHz);
        const float normDb = std::clamp((smoothedDb_[static_cast<std::size_t>(b)] + 78.0f) / 78.0f, 0.0f, 1.0f);
        const float py = specArea.getBottom() - normDb * (specArea.getHeight() - 18.0f);
        if (!started) {
            specPath.startNewSubPath(px, specArea.getBottom());
            specPath.lineTo(px, py);
            started = true;
        } else {
            specPath.lineTo(px, py);
        }
    }
    if (started) {
        juce::Path fillPath = specPath;
        fillPath.lineTo(specArea.getRight(), specArea.getBottom());
        fillPath.closeSubPath();
        g.setColour(kAccentCyan.withAlpha(0.18f));
        g.fillPath(fillPath);
        g.setColour(kAccentCyan.withAlpha(0.85f));
        g.strokePath(specPath, juce::PathStrokeType(1.4f));
    }

    // Draw Draggable Crossover Split Handles
    g.setFont(makeUiFont(10.5f, true));
    if (bandMode >= 1) {
        g.setColour(kBandLowCol);
        g.drawVerticalLine(static_cast<int>(xLow), specArea.getY(), specArea.getBottom());
        g.drawText(juce::String(static_cast<int>(lowHz)) + "Hz",
                   static_cast<int>(xLow) + 3, static_cast<int>(specArea.getBottom()) - 15, 52, 14,
                   juce::Justification::left);
    }
    if (bandMode == 2) {
        g.setColour(kBandHighCol);
        g.drawVerticalLine(static_cast<int>(xHigh), specArea.getY(), specArea.getBottom());
        g.drawText(juce::String(static_cast<int>(highHz)) + "Hz",
                   static_cast<int>(xHigh) - 54, static_cast<int>(specArea.getBottom()) - 15, 52, 14,
                   juce::Justification::right);
    }

    if (unlinked && bandMode >= 1) {
        g.setColour(kTextBright);
        const char* bNames[3] = {"EDIT: LOW", "EDIT: MID", "EDIT: HIGH"};
        g.drawText(bNames[std::clamp(activeEditBand_, 0, 2)],
                   specArea.toNearestInt().reduced(6, 4), juce::Justification::topLeft);
    }

    g.setColour(kBorderCol);
    g.drawRoundedRectangle(specArea, 5.0f, 1.0f);

    // Draw Per-Band Gain Reduction (GR) Vertical Meters on Right
    g.setColour(kBgDark);
    g.fillRoundedRectangle(grArea, 4.0f);
    const float colW = grArea.getWidth() / 3.0f;
    const juce::Colour grCols[3] = {kBandLowCol, kBandMidCol, kBandHighCol};
    for (int b = 0; b < 3; ++b) {
        const float grDb = processor_.getEngine().getBandGainReductionDb(b); // 0 dB down to -96 dB
        const float normDuck = std::clamp(-grDb / 36.0f, 0.0f, 1.0f);
        auto bar = juce::Rectangle<float>(
            grArea.getX() + static_cast<float>(b) * colW + 1.5f,
            grArea.getY() + 12.0f,
            colW - 3.0f,
            (grArea.getHeight() - 14.0f) * normDuck);
        g.setColour(grCols[b]);
        g.fillRoundedRectangle(bar, 1.5f);
    }
    g.setColour(kTextMuted);
    g.setFont(makeUiFont(9.5f, true));
    g.drawText("GR", grArea.toNearestInt().withHeight(12), juce::Justification::centred);
}

void MultibandSpectrumView::mouseMove(const juce::MouseEvent& e) {
    const auto& apvts = processor_.getAPVTS();
    const int bandMode = static_cast<int>(std::lround(apvts.getRawParameterValue("bandMode")->load()));
    if (bandMode == 0) {
        setMouseCursor(juce::MouseCursor::NormalCursor);
        return;
    }
    const float mx = static_cast<float>(e.x);
    const float xLow = freqToX(apvts.getRawParameterValue("lowCrossHz")->load());
    const float xHigh = freqToX(apvts.getRawParameterValue("highCrossHz")->load());
    if (std::fabs(mx - xLow) < 7.0f || (bandMode == 2 && std::fabs(mx - xHigh) < 7.0f)) {
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    } else {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }
}

void MultibandSpectrumView::mouseDown(const juce::MouseEvent& e) {
    auto& apvts = processor_.getAPVTS();
    const int bandMode = static_cast<int>(std::lround(apvts.getRawParameterValue("bandMode")->load()));
    if (bandMode == 0) return;

    const float mx = static_cast<float>(e.x);
    const float xLow = freqToX(apvts.getRawParameterValue("lowCrossHz")->load());
    const float xHigh = freqToX(apvts.getRawParameterValue("highCrossHz")->load());

    if (std::fabs(mx - xLow) < 7.0f) {
        draggingHandle_ = 1;
        return;
    }
    if (bandMode == 2 && std::fabs(mx - xHigh) < 7.0f) {
        draggingHandle_ = 2;
        return;
    }

    // Clicking inside a frequency band region selects that band's envelope for editing!
    if (mx < xLow) {
        setSelectedEditBand(0);
    } else if (bandMode == 2 && mx < xHigh) {
        setSelectedEditBand(1);
    } else {
        setSelectedEditBand(2);
    }
}

void MultibandSpectrumView::mouseDrag(const juce::MouseEvent& e) {
    if (draggingHandle_ == 0) return;
    auto& apvts = processor_.getAPVTS();
    const float hz = xToFreq(static_cast<float>(e.x));
    if (draggingHandle_ == 1) {
        const float maxLow = std::min(2000.0f, apvts.getRawParameterValue("highCrossHz")->load() - 50.0f);
        if (auto* p = apvts.getParameter("lowCrossHz")) {
            p->setValueNotifyingHost(p->convertTo0to1(std::clamp(hz, 20.0f, maxLow)));
        }
    } else if (draggingHandle_ == 2) {
        const float minHigh = std::max(200.0f, apvts.getRawParameterValue("lowCrossHz")->load() + 50.0f);
        if (auto* p = apvts.getParameter("highCrossHz")) {
            p->setValueNotifyingHost(p->convertTo0to1(std::clamp(hz, minHigh, 18000.0f)));
        }
    }
    repaint();
}

void MultibandSpectrumView::mouseUp(const juce::MouseEvent&) {
    draggingHandle_ = 0;
}

// ==============================================================================
// EnvelopeCanvasComponent
// ==============================================================================
EnvelopeCanvasComponent::EnvelopeCanvasComponent(DuckFlowAudioProcessor& proc)
    : processor_(proc)
{
    setWantsKeyboardFocus(true);
}

juce::Rectangle<float> EnvelopeCanvasComponent::getPlotBounds() const noexcept {
    return getLocalBounds().toFloat().reduced(12.0f, 14.0f);
}

juce::Point<float> EnvelopeCanvasComponent::normToScreen(float nx, float ny) const noexcept {
    const auto b = getPlotBounds();
    return {b.getX() + std::clamp(nx, 0.0f, 1.0f) * b.getWidth(),
            b.getBottom() - std::clamp(ny, 0.0f, 1.0f) * b.getHeight()};
}

juce::Point<float> EnvelopeCanvasComponent::screenToNorm(juce::Point<float> pt) const noexcept {
    const auto b = getPlotBounds();
    const float nx = (pt.x - b.getX()) / std::max(1.0f, b.getWidth());
    const float ny = (b.getBottom() - pt.y) / std::max(1.0f, b.getHeight());
    return {std::clamp(nx, 0.0f, 1.0f), std::clamp(ny, 0.0f, 1.0f)};
}

float EnvelopeCanvasComponent::snapXIfEnabled(float nx, bool altOverride) const noexcept {
    if (!processor_.isSnapToGridEnabled() || altOverride) return std::clamp(nx, 0.0f, 1.0f);
    const int divs = std::max(2, processor_.getGridDivisions());
    return std::clamp(std::round(nx * static_cast<float>(divs)) / static_cast<float>(divs), 0.0f, 1.0f);
}

int EnvelopeCanvasComponent::hitTestNode(juce::Point<float> pos, const EnvelopeData& env) const noexcept {
    int best = -1;
    float bestDist = 10.5f;
    for (int i = 0; i < static_cast<int>(env.nodes.size()); ++i) {
        const auto sp = normToScreen(env.nodes[static_cast<std::size_t>(i)].x, env.nodes[static_cast<std::size_t>(i)].y);
        const float d = sp.getDistanceFrom(pos);
        if (d < bestDist) {
            bestDist = d;
            best = i;
        }
    }
    return best;
}

int EnvelopeCanvasComponent::hitTestTensionHandle(juce::Point<float> pos, const EnvelopeData& env) const noexcept {
    for (int i = 0; i + 1 < static_cast<int>(env.nodes.size()); ++i) {
        const auto& n0 = env.nodes[static_cast<std::size_t>(i)];
        const auto& n1 = env.nodes[static_cast<std::size_t>(i + 1)];
        if (n1.x - n0.x < 0.02f) continue;
        const float midX = 0.5f * (n0.x + n1.x);
        const float midY = EnvelopeData::evalSegment(0.5f, n0.y, n1.y, n0.curve, n0.tension);
        const auto sp = normToScreen(midX, midY);
        if (sp.getDistanceFrom(pos) < 9.5f) {
            return i;
        }
    }
    return -1;
}

int EnvelopeCanvasComponent::hitTestLoopMarker(juce::Point<float> pos, const EnvelopeData& env) const noexcept {
    const auto b = getPlotBounds();
    if (pos.y > b.getY() + 18.0f) return 0;
    const float lx1 = normToScreen(env.loopStart, 1.0f).x;
    const float lx2 = normToScreen(env.loopEnd, 1.0f).x;
    if (std::fabs(pos.x - lx1) < 8.0f) return 1;
    if (std::fabs(pos.x - lx2) < 8.0f) return 2;
    return 0;
}

void EnvelopeCanvasComponent::paint(juce::Graphics& g) {
    const auto outer = getLocalBounds().toFloat();
    const auto plot = getPlotBounds();

    g.setColour(kBgDark.darker(0.15f));
    g.fillRoundedRectangle(outer, 7.0f);

    // 1. Musical Grid Subdivisions (with Swing warp preview!)
    const int divs = std::max(2, processor_.getGridDivisions());
    const float swing = processor_.getAPVTS().getRawParameterValue("swing")->load() * 0.01f;
    for (int d = 0; d <= divs; ++d) {
        const float rawX = static_cast<float>(d) / static_cast<float>(divs);
        const float swungX = EnvelopeData::applySwing(rawX, swing);
        const float sx = normToScreen(swungX, 0.0f).x;
        const bool isMajor = (d % 4 == 0);
        g.setColour(isMajor ? kBorderCol.brighter(0.18f) : kBorderCol.withAlpha(0.45f));
        g.drawVerticalLine(static_cast<int>(sx), plot.getY(), plot.getBottom());
    }
    for (int h = 1; h <= 3; ++h) {
        const float sy = normToScreen(0.0f, static_cast<float>(h) * 0.25f).y;
        g.setColour(kBorderCol.withAlpha(0.4f));
        g.drawHorizontalLine(static_cast<int>(sy), plot.getX(), plot.getRight());
    }

    // 2. Rolling Pre-Duck, Post-Duck & Unmodified Sidechain Oscilloscope
    const auto scope = processor_.getEngine().getOscilloscopeSnapshot();
    juce::Path prePath, postPath, scPath;
    prePath.startNewSubPath(plot.getX(), plot.getBottom());
    postPath.startNewSubPath(plot.getX(), plot.getBottom());
    scPath.startNewSubPath(plot.getX(), plot.getBottom());

    for (int p = 0; p < OscilloscopeSnapshot::kPoints; ++p) {
        const int idx = (scope.writeHead + p) % OscilloscopeSnapshot::kPoints;
        const float nx = static_cast<float>(p) / static_cast<float>(OscilloscopeSnapshot::kPoints - 1);
        const float px = plot.getX() + nx * plot.getWidth();

        const float preY = plot.getBottom() - std::clamp(scope.prePeak[static_cast<std::size_t>(idx)], 0.0f, 1.0f) * plot.getHeight() * 0.88f;
        const float postY = plot.getBottom() - std::clamp(scope.postPeak[static_cast<std::size_t>(idx)], 0.0f, 1.0f) * plot.getHeight() * 0.88f;
        const float scY = plot.getBottom() - std::clamp(scope.sidechainPeak[static_cast<std::size_t>(idx)], 0.0f, 1.0f) * plot.getHeight() * 0.80f;

        prePath.lineTo(px, preY);
        postPath.lineTo(px, postY);
        scPath.lineTo(px, scY);
    }
    prePath.lineTo(plot.getRight(), plot.getBottom());
    prePath.closeSubPath();
    postPath.lineTo(plot.getRight(), plot.getBottom());
    postPath.closeSubPath();

    g.setColour(kTextMuted.withAlpha(0.12f));
    g.fillPath(prePath);
    g.setColour(kAccentCyan.withAlpha(0.16f));
    g.fillPath(postPath);

    if (showSidechainScope_) {
        g.setColour(kScGreen.withAlpha(0.45f));
        g.strokePath(scPath, juce::PathStrokeType(1.2f));
    }

    // 3. If Unlinked Multiband is ON, draw ghost curves of inactive bands for alignment reference!
    const bool unlinked = processor_.getAPVTS().getRawParameterValue("unlinkBands")->load() >= 0.5f;
    const int bandMode = static_cast<int>(std::lround(processor_.getAPVTS().getRawParameterValue("bandMode")->load()));
    const int editBand = unlinked ? activeBand_ : 0;

    if (unlinked && bandMode >= 1) {
        for (int b = 0; b < 3; ++b) {
            if (b == editBand) continue;
            if (bandMode == 1 && b == 1) continue;
            const EnvelopeData ghost = processor_.getBandEnvelope(b);
            juce::Path gp;
            constexpr int kSamples = 200;
            for (int s = 0; s <= kSamples; ++s) {
                const float nx = static_cast<float>(s) / static_cast<float>(kSamples);
                const float ny = ghost.evaluate(nx, swing, false);
                const auto sp = normToScreen(nx, ny);
                if (s == 0) gp.startNewSubPath(sp);
                else gp.lineTo(sp);
            }
            g.setColour(colourForBand(b, true).withAlpha(0.28f));
            g.strokePath(gp, juce::PathStrokeType(1.5f));
        }
    }

    // 4. Draw Active Envelope Curve (with Depth scaling shaded region)
    const EnvelopeData env = processor_.getBandEnvelope(editBand);
    const float globalDepth = processor_.getAPVTS().getRawParameterValue("depth")->load() * 0.01f;
    const juce::Colour curveCol = colourForBand(editBand, unlinked);

    juce::Path envStroke;
    juce::Path depthStroke;
    constexpr int kSteps = 320;
    for (int s = 0; s <= kSteps; ++s) {
        const float nx = static_cast<float>(s) / static_cast<float>(kSteps);
        const float rawY = env.evaluate(nx, swing, false);
        const float effY = 1.0f - globalDepth * (1.0f - rawY);
        const auto spRaw = normToScreen(nx, rawY);
        const auto spEff = normToScreen(nx, effY);
        if (s == 0) {
            envStroke.startNewSubPath(spRaw);
            depthStroke.startNewSubPath(spEff);
        } else {
            envStroke.lineTo(spRaw);
            depthStroke.lineTo(spEff);
        }
    }

    juce::Path envFill = depthStroke;
    envFill.lineTo(plot.getRight(), plot.getBottom());
    envFill.lineTo(plot.getX(), plot.getBottom());
    envFill.closeSubPath();
    g.setGradientFill(juce::ColourGradient(
        curveCol.withAlpha(0.24f), plot.getX(), plot.getY(),
        curveCol.withAlpha(0.03f), plot.getX(), plot.getBottom(), false));
    g.fillPath(envFill);

    if (std::fabs(globalDepth - 1.0f) > 0.01f) {
        g.setColour(curveCol.withAlpha(0.35f));
        g.strokePath(envStroke, juce::PathStrokeType(1.2f));
    }
    g.setColour(curveCol);
    g.strokePath(depthStroke, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // 5. Loop + Release Region Markers (shown when Loop+Release mode is selected)
    const int loopMode = static_cast<int>(std::lround(processor_.getAPVTS().getRawParameterValue("loopMode")->load()));
    if (loopMode == static_cast<int>(EnvelopeTriggerLoopMode::LoopRelease)) {
        const float lx1 = normToScreen(env.loopStart, 1.0f).x;
        const float lx2 = normToScreen(env.loopEnd, 1.0f).x;
        g.setColour(kBandLowCol.withAlpha(0.14f));
        g.fillRect(juce::Rectangle<float>(lx1, plot.getY(), lx2 - lx1, 14.0f));
        g.setColour(kBandLowCol);
        g.drawVerticalLine(static_cast<int>(lx1), plot.getY(), plot.getBottom());
        g.drawVerticalLine(static_cast<int>(lx2), plot.getY(), plot.getBottom());
        g.setFont(makeUiFont(10.0f, true));
        g.drawText("SUSTAIN LOOP", static_cast<int>(lx1) + 4, static_cast<int>(plot.getY()),
                   std::max(40, static_cast<int>(lx2 - lx1) - 8), 14, juce::Justification::centred);
    }

    // 6. Segment Midpoint 2D Tension Handles & Control Nodes
    for (int i = 0; i + 1 < static_cast<int>(env.nodes.size()); ++i) {
        const auto& n0 = env.nodes[static_cast<std::size_t>(i)];
        const auto& n1 = env.nodes[static_cast<std::size_t>(i + 1)];
        if (n1.x - n0.x >= 0.025f && n0.curve != CurveType::StepHold) {
            const float midX = 0.5f * (n0.x + n1.x);
            const float midY = EnvelopeData::evalSegment(0.5f, n0.y, n1.y, n0.curve, n0.tension);
            const auto mp = normToScreen(midX, midY);
            g.setColour(kBgDark);
            g.fillEllipse(mp.x - 4.0f, mp.y - 4.0f, 8.0f, 8.0f);
            g.setColour(curveCol.withAlpha(0.85f));
            g.drawEllipse(mp.x - 4.0f, mp.y - 4.0f, 8.0f, 8.0f, 1.4f);
        }
    }

    for (int i = 0; i < static_cast<int>(env.nodes.size()); ++i) {
        const auto& nd = env.nodes[static_cast<std::size_t>(i)];
        const auto np = normToScreen(nd.x, nd.y);
        const bool selected = (selectedNodes_.count(i) > 0);
        const float r = selected ? 6.5f : 5.2f;
        g.setColour(selected ? kTextBright : kBgDark);
        g.fillEllipse(np.x - r, np.y - r, r * 2.0f, r * 2.0f);
        g.setColour(curveCol);
        g.drawEllipse(np.x - r, np.y - r, r * 2.0f, r * 2.0f, 2.2f);
    }

    // 7. Live Playhead Sweep Line & Current Gain Dot
    const float phase = processor_.getEngine().getPlayheadPhase();
    const float playX = normToScreen(phase, 0.0f).x;
    const float playRawY = env.evaluate(phase, swing, false);
    const float playEffY = 1.0f - globalDepth * (1.0f - playRawY);
    const auto playDot = normToScreen(phase, playEffY);

    g.setColour(kTextBright.withAlpha(0.55f));
    g.drawVerticalLine(static_cast<int>(playX), plot.getY(), plot.getBottom());
    g.setColour(kTextBright);
    g.fillEllipse(playDot.x - 4.5f, playDot.y - 4.5f, 9.0f, 9.0f);

    // 8. Multi-Select Lasso Box & WAV Drag-and-Drop Overlay
    if (isLassoSelecting_) {
        g.setColour(kAccentCyan.withAlpha(0.15f));
        g.fillRect(lassoRect_);
        g.setColour(kAccentCyan);
        g.drawRect(lassoRect_, 1.2f);
    }

    if (isFileDragHover_) {
        g.setColour(kBgDark.withAlpha(0.82f));
        g.fillRoundedRectangle(outer, 7.0f);
        g.setColour(kAccentCyan);
        g.drawRoundedRectangle(outer.reduced(4.0f), 7.0f, 2.5f);
        g.setFont(makeUiFont(18.0f, true));
        g.drawText("DROP AUDIO / KICK WAV TO TRACE DUCKING ENVELOPE",
                   getLocalBounds(), juce::Justification::centred);
    } else {
        g.setColour(kBorderCol);
        g.drawRoundedRectangle(outer.reduced(0.5f), 7.0f, 1.2f);
    }
}

void EnvelopeCanvasComponent::showSegmentContextMenu(int segIdx) {
    const bool unlinked = processor_.getAPVTS().getRawParameterValue("unlinkBands")->load() >= 0.5f;
    const int editBand = unlinked ? activeBand_ : 0;
    EnvelopeData env = processor_.getBandEnvelope(editBand);
    if (segIdx < 0 || segIdx + 1 >= static_cast<int>(env.nodes.size())) return;

    juce::PopupMenu menu;
    const CurveType cur = env.nodes[static_cast<std::size_t>(segIdx)].curve;
    menu.addSectionHeader("Segment Curve Shape");
    menu.addItem(1, "Linear", true, cur == CurveType::Linear);
    menu.addItem(2, "2D Variable Bend", true, cur == CurveType::Tension2D);
    menu.addItem(3, "S-Curve Spline", true, cur == CurveType::SCurveSpline);
    menu.addItem(4, "Sine Wobble", true, cur == CurveType::SineWobble);
    menu.addItem(5, "Saw Wobble", true, cur == CurveType::SawWobble);
    menu.addItem(6, "Step / Hard Gate", true, cur == CurveType::StepHold);
    menu.addSeparator();
    menu.addItem(10, "Reset Segment Tension (0.0)");

    menu.showMenuAsync(juce::PopupMenu::Options(), [this, editBand, segIdx](int result) {
        if (result <= 0) return;
        EnvelopeData updated = processor_.getBandEnvelope(editBand);
        if (segIdx < 0 || segIdx + 1 >= static_cast<int>(updated.nodes.size())) return;
        auto& nd = updated.nodes[static_cast<std::size_t>(segIdx)];
        if (result >= 1 && result <= 6) {
            nd.curve = static_cast<CurveType>(result - 1);
        } else if (result == 10) {
            nd.tension = 0.0f;
        }
        processor_.setBandEnvelope(editBand, updated, true);
        repaint();
    });
}

void EnvelopeCanvasComponent::mouseDown(const juce::MouseEvent& e) {
    grabKeyboardFocus();
    const bool unlinked = processor_.getAPVTS().getRawParameterValue("unlinkBands")->load() >= 0.5f;
    const int editBand = unlinked ? activeBand_ : 0;
    EnvelopeData env = processor_.getBandEnvelope(editBand);
    const auto pos = e.position;

    draggingNode_ = -1;
    draggingTensionSeg_ = -1;
    draggingLoopMarker_ = 0;
    isLassoSelecting_ = false;
    isPainting_ = false;

    // Right-click deletes internal node or opens segment curve type context menu
    if (e.mods.isPopupMenu()) {
        const int hitN = hitTestNode(pos, env);
        if (hitN > 0 && hitN + 1 < static_cast<int>(env.nodes.size())) {
            env.nodes.erase(env.nodes.begin() + hitN);
            selectedNodes_.clear();
            processor_.setBandEnvelope(editBand, env, true);
            repaint();
            return;
        }
        const float clickNx = screenToNorm(pos).x;
        for (int i = 0; i + 1 < static_cast<int>(env.nodes.size()); ++i) {
            if (clickNx >= env.nodes[static_cast<std::size_t>(i)].x &&
                clickNx <= env.nodes[static_cast<std::size_t>(i + 1)].x)
            {
                showSegmentContextMenu(i);
                return;
            }
        }
        return;
    }

    // Paint tool mode: stamp pattern across grid cell
    if (paintTool_ != PaintTool::Pointer) {
        isPainting_ = true;
        dragStartPos_ = pos;
        dragStartEnv_ = env;
        return;
    }

    // Loop marker check
    const int loopMode = static_cast<int>(std::lround(processor_.getAPVTS().getRawParameterValue("loopMode")->load()));
    if (loopMode == static_cast<int>(EnvelopeTriggerLoopMode::LoopRelease)) {
        const int lm = hitTestLoopMarker(pos, env);
        if (lm > 0) {
            draggingLoopMarker_ = lm;
            dragStartEnv_ = env;
            return;
        }
    }

    // Node hit check
    const int hitN = hitTestNode(pos, env);
    if (hitN >= 0) {
        draggingNode_ = hitN;
        dragStartPos_ = pos;
        dragStartEnv_ = env;
        if (!e.mods.isShiftDown() && selectedNodes_.count(hitN) == 0) {
            selectedNodes_.clear();
        }
        selectedNodes_.insert(hitN);
        repaint();
        return;
    }

    // Tension handle hit check
    const int hitT = hitTestTensionHandle(pos, env);
    if (hitT >= 0) {
        draggingTensionSeg_ = hitT;
        dragStartPos_ = pos;
        dragStartEnv_ = env;
        dragStartTension_ = env.nodes[static_cast<std::size_t>(hitT)].tension;
        return;
    }

    // Shift-drag starts multi-node lasso selection; plain click inserts a new node
    if (e.mods.isShiftDown()) {
        isLassoSelecting_ = true;
        dragStartPos_ = pos;
        lassoRect_ = {pos.x, pos.y, 0.0f, 0.0f};
        return;
    }

    // Single click on empty curve area adds a new control node cleanly!
    const auto norm = screenToNorm(pos);
    const float nx = snapXIfEnabled(norm.x, e.mods.isAltDown());
    if (nx > 0.002f && nx < 0.998f) {
        env.nodes.push_back({nx, norm.y, 0.0f, CurveType::SCurveSpline});
        env.sortAndValidate();
        processor_.setBandEnvelope(editBand, env, false);
        // Find inserted node index for immediate smooth dragging
        draggingNode_ = hitTestNode(normToScreen(nx, norm.y), env);
        dragStartPos_ = pos;
        dragStartEnv_ = env;
        selectedNodes_.clear();
        if (draggingNode_ >= 0) selectedNodes_.insert(draggingNode_);
        repaint();
    }
}

void EnvelopeCanvasComponent::mouseDrag(const juce::MouseEvent& e) {
    const bool unlinked = processor_.getAPVTS().getRawParameterValue("unlinkBands")->load() >= 0.5f;
    const int editBand = unlinked ? activeBand_ : 0;

    if (isPainting_) {
        EnvelopeData env = dragStartEnv_;
        const auto p0 = screenToNorm(dragStartPos_);
        const auto p1 = screenToNorm(e.position);
        const float x0 = snapXIfEnabled(p0.x, e.mods.isAltDown());
        float x1 = snapXIfEnabled(p1.x, e.mods.isAltDown());
        if (std::fabs(x1 - x0) < 0.01f) {
            const float step = 1.0f / static_cast<float>(std::max(2, processor_.getGridDivisions()));
            x1 = std::min(1.0f, x0 + step);
        }
        switch (paintTool_) {
            case PaintTool::Line:
                env.paintSegment(x0, x1, p0.y, p1.y, CurveType::Linear);
                break;
            case PaintTool::RampUp:
                env.paintSegment(x0, x1, 0.0f, std::max(0.2f, p1.y), CurveType::Tension2D, -0.25f);
                break;
            case PaintTool::RampDown:
                env.paintSegment(x0, x1, std::max(0.2f, p0.y), 0.0f, CurveType::Tension2D, 0.25f);
                break;
            case PaintTool::GateStep:
                env.paintSegment(x0, x1, p1.y, p1.y, CurveType::StepHold);
                break;
            case PaintTool::SineBrush:
                env.paintSegment(x0, x1, p0.y, p1.y, CurveType::SineWobble, 0.0f);
                break;
            default:
                break;
        }
        processor_.setBandEnvelope(editBand, env, false);
        repaint();
        return;
    }

    if (draggingLoopMarker_ > 0) {
        EnvelopeData env = dragStartEnv_;
        const float nx = snapXIfEnabled(screenToNorm(e.position).x, e.mods.isAltDown());
        if (draggingLoopMarker_ == 1) {
            env.loopStart = std::clamp(nx, 0.0f, env.loopEnd - 0.05f);
        } else {
            env.loopEnd = std::clamp(nx, env.loopStart + 0.05f, 1.0f);
        }
        processor_.setBandEnvelope(editBand, env, false);
        repaint();
        return;
    }

    if (draggingNode_ >= 0) {
        EnvelopeData env = dragStartEnv_;
        const auto curNorm = screenToNorm(e.position);
        const auto startNorm = screenToNorm(dragStartPos_);
        const float dx = curNorm.x - startNorm.x;
        const float dy = curNorm.y - startNorm.y;

        for (int idx : selectedNodes_) {
            if (idx < 0 || idx >= static_cast<int>(env.nodes.size())) continue;
            auto& nd = env.nodes[static_cast<std::size_t>(idx)];
            nd.y = std::clamp(nd.y + dy, 0.0f, 1.0f);
            if (idx > 0 && idx + 1 < static_cast<int>(env.nodes.size())) {
                const float minX = env.nodes[static_cast<std::size_t>(idx - 1)].x;
                const float maxX = env.nodes[static_cast<std::size_t>(idx + 1)].x;
                nd.x = std::clamp(snapXIfEnabled(nd.x + dx, e.mods.isAltDown()), minX, maxX);
            }
        }
        env.sortAndValidate();
        processor_.setBandEnvelope(editBand, env, false);
        repaint();
        return;
    }

    if (draggingTensionSeg_ >= 0) {
        EnvelopeData env = dragStartEnv_;
        if (draggingTensionSeg_ < static_cast<int>(env.nodes.size())) {
            const float deltaY = (e.position.y - dragStartPos_.y) / 110.0f;
            auto& seg = env.nodes[static_cast<std::size_t>(draggingTensionSeg_)];
            const float nextY = (draggingTensionSeg_ + 1 < static_cast<int>(env.nodes.size()))
                              ? env.nodes[static_cast<std::size_t>(draggingTensionSeg_ + 1)].y
                              : seg.y;
            const float dir = (nextY >= seg.y) ? 1.0f : -1.0f;
            if (seg.curve == CurveType::Linear) seg.curve = CurveType::Tension2D;
            seg.tension = std::clamp(dragStartTension_ + deltaY * dir, -0.98f, 0.98f);
            processor_.setBandEnvelope(editBand, env, false);
            repaint();
        }
        return;
    }

    if (isLassoSelecting_) {
        lassoRect_ = juce::Rectangle<float>(dragStartPos_, e.position);
        const EnvelopeData env = processor_.getBandEnvelope(editBand);
        selectedNodes_.clear();
        for (int i = 0; i < static_cast<int>(env.nodes.size()); ++i) {
            const auto sp = normToScreen(env.nodes[static_cast<std::size_t>(i)].x,
                                         env.nodes[static_cast<std::size_t>(i)].y);
            if (lassoRect_.contains(sp)) selectedNodes_.insert(i);
        }
        repaint();
    }
}

void EnvelopeCanvasComponent::mouseUp(const juce::MouseEvent&) {
    const bool unlinked = processor_.getAPVTS().getRawParameterValue("unlinkBands")->load() >= 0.5f;
    const int editBand = unlinked ? activeBand_ : 0;

    if (draggingNode_ >= 0 || draggingTensionSeg_ >= 0 || draggingLoopMarker_ > 0 || isPainting_) {
        // Commit final edited envelope state to Undo history
        processor_.setBandEnvelope(editBand, processor_.getBandEnvelope(editBand), true);
    }
    draggingNode_ = -1;
    draggingTensionSeg_ = -1;
    draggingLoopMarker_ = 0;
    isLassoSelecting_ = false;
    isPainting_ = false;
    repaint();
}

void EnvelopeCanvasComponent::mouseDoubleClick(const juce::MouseEvent& e) {
    const bool unlinked = processor_.getAPVTS().getRawParameterValue("unlinkBands")->load() >= 0.5f;
    const int editBand = unlinked ? activeBand_ : 0;
    EnvelopeData env = processor_.getBandEnvelope(editBand);

    const int hitN = hitTestNode(e.position, env);
    if (hitN > 0 && hitN + 1 < static_cast<int>(env.nodes.size())) {
        env.nodes.erase(env.nodes.begin() + hitN);
        selectedNodes_.clear();
        processor_.setBandEnvelope(editBand, env, true);
        repaint();
    }
}

bool EnvelopeCanvasComponent::keyPressed(const juce::KeyPress& key) {
    const bool unlinked = processor_.getAPVTS().getRawParameterValue("unlinkBands")->load() >= 0.5f;
    const int editBand = unlinked ? activeBand_ : 0;

    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) {
        if (!selectedNodes_.empty()) {
            EnvelopeData env = processor_.getBandEnvelope(editBand);
            for (auto it = selectedNodes_.rbegin(); it != selectedNodes_.rend(); ++it) {
                const int idx = *it;
                if (idx > 0 && idx + 1 < static_cast<int>(env.nodes.size())) {
                    env.nodes.erase(env.nodes.begin() + idx);
                }
            }
            selectedNodes_.clear();
            processor_.setBandEnvelope(editBand, env, true);
            repaint();
            return true;
        }
    }
    return false;
}

bool EnvelopeCanvasComponent::isInterestedInFileDrag(const juce::StringArray& files) {
    for (const auto& f : files) {
        if (f.endsWithIgnoreCase(".wav") || f.endsWithIgnoreCase(".aif") ||
            f.endsWithIgnoreCase(".aiff") || f.endsWithIgnoreCase(".flac"))
        {
            return true;
        }
    }
    return false;
}

void EnvelopeCanvasComponent::fileDragEnter(const juce::StringArray&, int, int) {
    isFileDragHover_ = true;
    repaint();
}

void EnvelopeCanvasComponent::fileDragExit(const juce::StringArray&) {
    isFileDragHover_ = false;
    repaint();
}

void EnvelopeCanvasComponent::filesDropped(const juce::StringArray& files, int, int) {
    isFileDragHover_ = false;
    if (files.isEmpty()) return;
    const bool unlinked = processor_.getAPVTS().getRawParameterValue("unlinkBands")->load() >= 0.5f;
    const int editBand = unlinked ? activeBand_ : 0;
    processor_.traceEnvelopeFromAudioFile(juce::File(files[0]), editBand, true);
    repaint();
}

void EnvelopeCanvasComponent::copyEnvelopeToClipboard() {
    const bool unlinked = processor_.getAPVTS().getRawParameterValue("unlinkBands")->load() >= 0.5f;
    const int editBand = unlinked ? activeBand_ : 0;
    const std::string s = "DUCKFLOW2_ENV:" + processor_.getBandEnvelope(editBand).serialize();
    juce::SystemClipboard::copyTextToClipboard(juce::String(s));
}

void EnvelopeCanvasComponent::pasteEnvelopeFromClipboard() {
    const juce::String clip = juce::SystemClipboard::getTextFromClipboard().trim();
    if (!clip.startsWith("DUCKFLOW2_ENV:")) return;
    const std::string payload = clip.substring(14).toStdString();
    EnvelopeData parsed;
    if (parsed.deserialize(payload)) {
        const bool unlinked = processor_.getAPVTS().getRawParameterValue("unlinkBands")->load() >= 0.5f;
        const int editBand = unlinked ? activeBand_ : 0;
        processor_.setBandEnvelope(editBand, parsed, true);
        repaint();
    }
}

// ==============================================================================
// DuckFlowAudioProcessorEditor
// ==============================================================================
DuckFlowAudioProcessorEditor::DuckFlowAudioProcessorEditor(DuckFlowAudioProcessor& p)
    : AudioProcessorEditor(&p),
      processor_(p),
      spectrumView_(p),
      envelopeCanvas_(p),
      quickShapes_(FactoryPresets::getQuickShapes())
{
    setLookAndFeel(&lookAndFeel_);
    setWantsKeyboardFocus(true);

    auto& apvts = processor_.getAPVTS();

    // --- Header Bar ---
    refreshPresetCombo();
    presetBox_.onChange = [this]() {
        const int idx = presetBox_.getSelectedId() - 1;
        if (idx >= 0) {
            processor_.loadFactoryPreset(idx);
            envelopeCanvas_.repaint();
        }
    };
    addAndMakeVisible(presetBox_);

    prevPresetBtn_.onClick = [this]() {
        const int n = static_cast<int>(processor_.getFactoryPresets().size());
        if (n <= 0) return;
        const int next = (processor_.getCurrentProgram() - 1 + n) % n;
        presetBox_.setSelectedId(next + 1, juce::sendNotification);
    };
    nextPresetBtn_.onClick = [this]() {
        const int n = static_cast<int>(processor_.getFactoryPresets().size());
        if (n <= 0) return;
        const int next = (processor_.getCurrentProgram() + 1) % n;
        presetBox_.setSelectedId(next + 1, juce::sendNotification);
    };
    addAndMakeVisible(prevPresetBtn_);
    addAndMakeVisible(nextPresetBtn_);

    addAndMakeVisible(lockTriggerBtn_);
    lockTrigAtt_ = std::make_unique<ButtonAtt>(apvts, "lockTrigger", lockTriggerBtn_);

    undoBtn_.onClick = [this]() {
        processor_.undoEnvelope(envelopeCanvas_.getActiveBand());
        envelopeCanvas_.repaint();
    };
    redoBtn_.onClick = [this]() {
        processor_.redoEnvelope(envelopeCanvas_.getActiveBand());
        envelopeCanvas_.repaint();
    };
    copyBtn_.onClick = [this]() { envelopeCanvas_.copyEnvelopeToClipboard(); };
    pasteBtn_.onClick = [this]() { envelopeCanvas_.pasteEnvelopeFromClipboard(); };
    addAndMakeVisible(undoBtn_);
    addAndMakeVisible(redoBtn_);
    addAndMakeVisible(copyBtn_);
    addAndMakeVisible(pasteBtn_);

    syncGroupBox_.addItemList({"Sync: Off", "Group A", "Group B", "Group C", "Group D"}, 1);
    addAndMakeVisible(syncGroupBox_);
    syncGroupAtt_ = std::make_unique<ComboAtt>(apvts, "syncGroup", syncGroupBox_);

    addAndMakeVisible(bypassBtn_);
    bypassAtt_ = std::make_unique<ButtonAtt>(apvts, "bypass", bypassBtn_);

    // --- Left Multiband Panel ---
    spectrumView_.onBandTabChanged = [this](int band) {
        envelopeCanvas_.setActiveBand(band);
    };
    addAndMakeVisible(spectrumView_);

    bandModeBox_.addItemList({"1-Band Full", "2-Band Split", "3-Band Split"}, 1);
    addAndMakeVisible(bandModeBox_);
    bandModeAtt_ = std::make_unique<ComboAtt>(apvts, "bandMode", bandModeBox_);

    addAndMakeVisible(unlinkBandsBtn_);
    unlinkAtt_ = std::make_unique<ButtonAtt>(apvts, "unlinkBands", unlinkBandsBtn_);

    addAndMakeVisible(linearPhaseBtn_);
    linPhaseAtt_ = std::make_unique<ButtonAtt>(apvts, "linearPhase", linearPhaseBtn_);

    slopeBox_.addItemList({"12 dB/oct", "24 dB/oct", "48 dB/oct"}, 1);
    addAndMakeVisible(slopeBox_);
    slopeAtt_ = std::make_unique<ComboAtt>(apvts, "slope", slopeBox_);

    bandSoloBox_.addItemList({"Solo: Off", "Solo: Low", "Solo: Mid", "Solo: High"}, 1);
    addAndMakeVisible(bandSoloBox_);
    bandSoloAtt_ = std::make_unique<ComboAtt>(apvts, "bandSolo", bandSoloBox_);

    auto setupLinearV = [this](juce::Slider& s, const juce::Colour& col) {
        s.setSliderStyle(juce::Slider::LinearVertical);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 46, 15);
        s.setColour(juce::Slider::trackColourId, col);
        addAndMakeVisible(s);
    };
    setupLinearV(lowAmtSlider_, kBandLowCol);
    setupLinearV(midAmtSlider_, kBandMidCol);
    setupLinearV(highAmtSlider_, kBandHighCol);
    lowAmtAtt_  = std::make_unique<SliderAtt>(apvts, "lowAmount", lowAmtSlider_);
    midAmtAtt_  = std::make_unique<SliderAtt>(apvts, "midAmount", midAmtSlider_);
    highAmtAtt_ = std::make_unique<SliderAtt>(apvts, "highAmount", highAmtSlider_);

    auto setupKnob = [this](juce::Slider& s, const juce::String& suffix = "") {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 58, 15);
        if (suffix.isNotEmpty()) s.setTextValueSuffix(suffix);
        addAndMakeVisible(s);
    };
    setupKnob(lowFreqSlider_, " Hz");
    setupKnob(highFreqSlider_, " Hz");
    lowFreqAtt_  = std::make_unique<SliderAtt>(apvts, "lowCrossHz", lowFreqSlider_);
    highFreqAtt_ = std::make_unique<SliderAtt>(apvts, "highCrossHz", highFreqSlider_);

    // --- Center Envelope Canvas + Paint & Operations Toolbar ---
    addAndMakeVisible(envelopeCanvas_);

    for (std::size_t t = 0; t < toolButtons_.size(); ++t) {
        auto& btn = toolButtons_[t];
        btn.setClickingTogglesState(true);
        btn.setRadioGroupId(101);
        if (t == 0) btn.setToggleState(true, juce::dontSendNotification);
        btn.onClick = [this, t]() {
            envelopeCanvas_.setPaintTool(static_cast<PaintTool>(t));
        };
        addAndMakeVisible(btn);
    }

    snapBtn_.setToggleState(processor_.isSnapToGridEnabled(), juce::dontSendNotification);
    snapBtn_.onClick = [this]() {
        processor_.setSnapToGridEnabled(snapBtn_.getToggleState());
        envelopeCanvas_.repaint();
    };
    addAndMakeVisible(snapBtn_);

    gridDivBox_.addItem("Grid 4", 4);
    gridDivBox_.addItem("Grid 6", 6);
    gridDivBox_.addItem("Grid 8", 8);
    gridDivBox_.addItem("Grid 12", 12);
    gridDivBox_.addItem("Grid 16", 16);
    gridDivBox_.addItem("Grid 24", 24);
    gridDivBox_.addItem("Grid 32", 32);
    gridDivBox_.setSelectedId(processor_.getGridDivisions(), juce::dontSendNotification);
    gridDivBox_.onChange = [this]() {
        processor_.setGridDivisions(gridDivBox_.getSelectedId());
        envelopeCanvas_.repaint();
    };
    addAndMakeVisible(gridDivBox_);

    scScopeBtn_.setToggleState(true, juce::dontSendNotification);
    scScopeBtn_.onClick = [this]() {
        envelopeCanvas_.setShowSidechainScope(scScopeBtn_.getToggleState());
    };
    addAndMakeVisible(scScopeBtn_);

    invBtn_.onClick  = [this]() { applyEnvelopeOperation(1); };
    revBtn_.onClick  = [this]() { applyEnvelopeOperation(2); };
    rotLBtn_.onClick = [this]() { applyEnvelopeOperation(3); };
    rotRBtn_.onClick = [this]() { applyEnvelopeOperation(4); };
    dup2xBtn_.onClick= [this]() { applyEnvelopeOperation(5); };
    halfBtn_.onClick = [this]() { applyEnvelopeOperation(6); };
    randBtn_.onClick = [this]() { applyEnvelopeOperation(7); };
    traceLiveBtn_.onClick = [this]() { processor_.triggerLiveTrace(1.0); };
    loadWavBtn_.onClick = [this]() {
        wavFileChooser_ = std::make_unique<juce::FileChooser>(
            "Select Kick / Audio WAV to Trace Envelope", juce::File{}, "*.wav;*.aif;*.aiff;*.flac");
        wavFileChooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc) {
                const auto f = fc.getResult();
                if (f.existsAsFile()) {
                    const bool unlinked = processor_.getAPVTS().getRawParameterValue("unlinkBands")->load() >= 0.5f;
                    processor_.traceEnvelopeFromAudioFile(f, unlinked ? envelopeCanvas_.getActiveBand() : 0, true);
                    envelopeCanvas_.repaint();
                }
            });
    };
    for (auto* b : {&invBtn_, &revBtn_, &rotLBtn_, &rotRBtn_, &dup2xBtn_, &halfBtn_, &randBtn_, &traceLiveBtn_, &loadWavBtn_}) {
        addAndMakeVisible(*b);
    }

    // --- Quick Shape Strip ---
    for (std::size_t i = 0; i < shapeBtns_.size(); ++i) {
        if (i < quickShapes_.size()) {
            shapeBtns_[i].setButtonText(quickShapes_[i].name);
            shapeBtns_[i].onClick = [this, i]() {
                const bool unlinked = processor_.getAPVTS().getRawParameterValue("unlinkBands")->load() >= 0.5f;
                processor_.setBandEnvelope(unlinked ? envelopeCanvas_.getActiveBand() : 0, quickShapes_[i].envelope, true);
                envelopeCanvas_.repaint();
            };
        }
        addAndMakeVisible(shapeBtns_[i]);
    }

    moreShapesBtn_.onClick = [this]() {
        juce::PopupMenu m;
        m.addSectionHeader("Factory Envelope Shapes (16)");
        for (int i = 0; i < static_cast<int>(quickShapes_.size()); ++i) {
            m.addItem(i + 1, quickShapes_[static_cast<std::size_t>(i)].name);
        }
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&moreShapesBtn_), [this](int res) {
            if (res >= 1 && res <= static_cast<int>(quickShapes_.size())) {
                const bool unlinked = processor_.getAPVTS().getRawParameterValue("unlinkBands")->load() >= 0.5f;
                processor_.setBandEnvelope(unlinked ? envelopeCanvas_.getActiveBand() : 0,
                                           quickShapes_[static_cast<std::size_t>(res - 1)].envelope, true);
                envelopeCanvas_.repaint();
            }
        });
    };
    addAndMakeVisible(moreShapesBtn_);

    // --- Lower Panel Controls ---
    triggerModeBox_.addItemList({"Repeat (Beat)", "Free Hz", "MIDI Trigger", "Sidechain In", "Audio In"}, 1);
    addAndMakeVisible(triggerModeBox_);
    trigModeAtt_ = std::make_unique<ComboAtt>(apvts, "triggerMode", triggerModeBox_);

    loopModeBox_.addItemList({"One-Shot", "Loop", "Loop+Release"}, 1);
    addAndMakeVisible(loopModeBox_);
    loopModeAtt_ = std::make_unique<ComboAtt>(apvts, "loopMode", loopModeBox_);

    beatDivBox_.addItemList({"1/64", "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8", "1/8D",
                             "1/4T", "1/4", "1/4D", "1/2", "1 Bar", "2 Bars", "4 Bars", "8 Bars"}, 1);
    addAndMakeVisible(beatDivBox_);
    beatDivAtt_ = std::make_unique<ComboAtt>(apvts, "beatDivision", beatDivBox_);

    setupKnob(freeHzSlider_, " Hz");
    setupKnob(swingSlider_, " %");
    setupKnob(depthKnob_, " %");
    depthKnob_.setColour(juce::Slider::rotarySliderFillColourId, kAccentCyan);
    setupKnob(smoothSlider_, " ms");
    setupKnob(offsetMsSlider_, " ms");
    setupKnob(offsetSamplesSlider_, " spl");
    setupKnob(outGainSlider_, " dB");
    setupKnob(dryWetSlider_, " %");

    freeHzAtt_    = std::make_unique<SliderAtt>(apvts, "freeHz", freeHzSlider_);
    swingAtt_     = std::make_unique<SliderAtt>(apvts, "swing", swingSlider_);
    depthAtt_     = std::make_unique<SliderAtt>(apvts, "depth", depthKnob_);
    smoothAtt_    = std::make_unique<SliderAtt>(apvts, "smoothingMs", smoothSlider_);
    offsetMsAtt_  = std::make_unique<SliderAtt>(apvts, "offsetMs", offsetMsSlider_);
    offsetSplAtt_ = std::make_unique<SliderAtt>(apvts, "offsetSamples", offsetSamplesSlider_);
    outGainAtt_   = std::make_unique<SliderAtt>(apvts, "outputGainDb", outGainSlider_);
    dryWetAtt_    = std::make_unique<SliderAtt>(apvts, "dryWet", dryWetSlider_);

    addAndMakeVisible(offsetUnitSplBtn_);
    offsetUnitAtt_ = std::make_unique<ButtonAtt>(apvts, "offsetInSamples", offsetUnitSplBtn_);

    // Detector strip
    setupKnob(threshSlider_, " dB");
    setupKnob(holdSlider_, " ms");
    setupKnob(detHpfSlider_, " Hz");
    setupKnob(detLpfSlider_, " Hz");
    setupKnob(midiVelSlider_, " %");
    threshAtt_  = std::make_unique<SliderAtt>(apvts, "thresholdDb", threshSlider_);
    holdAtt_    = std::make_unique<SliderAtt>(apvts, "holdMs", holdSlider_);
    detHpfAtt_  = std::make_unique<SliderAtt>(apvts, "detHpfHz", detHpfSlider_);
    detLpfAtt_  = std::make_unique<SliderAtt>(apvts, "detLpfHz", detLpfSlider_);
    midiVelAtt_ = std::make_unique<SliderAtt>(apvts, "midiVelSens", midiVelSlider_);

    addAndMakeVisible(detFilterBtn_);
    addAndMakeVisible(scListenBtn_);
    detFilterAtt_ = std::make_unique<ButtonAtt>(apvts, "detFilterOn", detFilterBtn_);
    scListenAtt_  = std::make_unique<ButtonAtt>(apvts, "scListen", scListenBtn_);

    // Freely resizable window (Duck 2 style!)
    setResizable(true, true);
    setResizeLimits(760, 500, 1680, 1080);
    setSize(processor_.getSavedEditorWidth(), processor_.getSavedEditorHeight());

    startTimerHz(60);
}

DuckFlowAudioProcessorEditor::~DuckFlowAudioProcessorEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void DuckFlowAudioProcessorEditor::refreshPresetCombo() {
    presetBox_.clear(juce::dontSendNotification);
    const auto& list = processor_.getFactoryPresets();
    juce::String lastCat;
    for (int i = 0; i < static_cast<int>(list.size()); ++i) {
        const auto& pr = list[static_cast<std::size_t>(i)];
        if (lastCat != pr.category) {
            presetBox_.addSectionHeading(pr.category);
            lastCat = pr.category;
        }
        presetBox_.addItem(pr.name, i + 1);
    }
    presetBox_.setSelectedId(processor_.getCurrentProgram() + 1, juce::dontSendNotification);
}

void DuckFlowAudioProcessorEditor::applyEnvelopeOperation(int opCode) {
    const bool unlinked = processor_.getAPVTS().getRawParameterValue("unlinkBands")->load() >= 0.5f;
    const int editBand = unlinked ? envelopeCanvas_.getActiveBand() : 0;
    EnvelopeData env = processor_.getBandEnvelope(editBand);
    const float step = 1.0f / static_cast<float>(std::max(2, processor_.getGridDivisions()));

    switch (opCode) {
        case 1: env.invert(); break;
        case 2: env.reverse(); break;
        case 3: env.rotatePhase(-step); break;
        case 4: env.rotatePhase(+step); break;
        case 5: env.duplicateDoubleTime(); break;
        case 6: env.halveTime(); break;
        case 7: env.randomize(rngSeed_, processor_.getGridDivisions()); break;
        default: break;
    }
    processor_.setBandEnvelope(editBand, env, true);
    envelopeCanvas_.repaint();
}

void DuckFlowAudioProcessorEditor::timerCallback() {
    const int editBand = envelopeCanvas_.getActiveBand();
    undoBtn_.setEnabled(processor_.canUndoEnvelope(editBand));
    redoBtn_.setEnabled(processor_.canRedoEnvelope(editBand));

    const auto& apvts = processor_.getAPVTS();
    const int trigMode = static_cast<int>(std::lround(apvts.getRawParameterValue("triggerMode")->load()));
    const bool isFreeHz = (trigMode == static_cast<int>(TriggerMode::FreeHz));
    beatDivBox_.setVisible(!isFreeHz);
    freeHzSlider_.setVisible(isFreeHz);

    const bool useSamples = apvts.getRawParameterValue("offsetInSamples")->load() >= 0.5f;
    offsetMsSlider_.setVisible(!useSamples);
    offsetSamplesSlider_.setVisible(useSamples);

    if (processor_.getEngine().consumeTriggerLedEvent()) {
        triggerLedGlow_ = true;
        triggerLedDecayFrames_ = 8;
    } else if (triggerLedDecayFrames_ > 0) {
        if (--triggerLedDecayFrames_ == 0) triggerLedGlow_ = false;
    }

    const bool unlinked = apvts.getRawParameterValue("unlinkBands")->load() >= 0.5f;
    processor_.checkAndApplyCompletedLiveTrace(unlinked ? editBand : 0, true);
    processor_.pollSyncGroupUpdates();

    spectrumView_.updateSpectrumFrame();
    envelopeCanvas_.repaint();
    repaint(0, getHeight() - 130, getWidth(), 130);
}

bool DuckFlowAudioProcessorEditor::keyPressed(const juce::KeyPress& key) {
    if (key.getModifiers().isCommandDown()) {
        if (key.getKeyCode() == 'z' || key.getKeyCode() == 'Z') {
            if (key.getModifiers().isShiftDown()) {
                processor_.redoEnvelope(envelopeCanvas_.getActiveBand());
            } else {
                processor_.undoEnvelope(envelopeCanvas_.getActiveBand());
            }
            envelopeCanvas_.repaint();
            return true;
        }
        if (key.getKeyCode() == 'y' || key.getKeyCode() == 'Y') {
            processor_.redoEnvelope(envelopeCanvas_.getActiveBand());
            envelopeCanvas_.repaint();
            return true;
        }
    }
    return false;
}

void DuckFlowAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(kBgDark);

    // Header Bar Background
    const auto full = getLocalBounds();
    auto header = full.withHeight(44).reduced(8, 6);
    g.setColour(kPanelBg);
    g.fillRoundedRectangle(header.toFloat(), 6.0f);

    // Brand Logo
    g.setColour(kAccentCyan);
    g.setFont(makeUiFont(17.5f, true));
    g.drawText("DUCKFLOW 2", 18, 8, 128, 30, juce::Justification::centredLeft);

    // Panel Cards
    const int topY = 50;
    const int bottomH = 148;
    const int midH = full.getHeight() - topY - bottomH - 8;
    const int leftW = std::clamp(full.getWidth() / 4, 215, 310);

    g.setColour(kPanelBg);
    g.fillRoundedRectangle(juce::Rectangle<float>(8.0f, static_cast<float>(topY),
                                                  static_cast<float>(leftW), static_cast<float>(midH)), 6.0f);
    g.fillRoundedRectangle(juce::Rectangle<float>(8.0f, static_cast<float>(full.getHeight() - bottomH - 4),
                                                  static_cast<float>(full.getWidth() - 16), static_cast<float>(bottomH)), 6.0f);

    // Labels for Lower Knobs + Detector Level Meter & Trigger LED
    g.setColour(kTextMuted);
    g.setFont(makeUiFont(10.5f, true));
    auto drawKnobLabel = [&](const juce::Component& c, const char* text) {
        if (!c.isVisible()) return;
        g.drawText(text, c.getX() - 4, c.getY() - 13, c.getWidth() + 8, 13, juce::Justification::centred);
    };

    drawKnobLabel(lowFreqSlider_, "LOW XOVER");
    drawKnobLabel(highFreqSlider_, "HIGH XOVER");
    drawKnobLabel(lowAmtSlider_, "LOW");
    drawKnobLabel(midAmtSlider_, "MID");
    drawKnobLabel(highAmtSlider_, "HIGH");

    drawKnobLabel(freeHzSlider_, "RATE HZ");
    drawKnobLabel(swingSlider_, "SWING");
    drawKnobLabel(depthKnob_, "AMOUNT");
    drawKnobLabel(smoothSlider_, "SMOOTH");
    drawKnobLabel(offsetMsSlider_, "OFFSET");
    drawKnobLabel(offsetSamplesSlider_, "OFFSET");
    drawKnobLabel(outGainSlider_, "OUTPUT");
    drawKnobLabel(dryWetSlider_, "DRY/WET");

    drawKnobLabel(threshSlider_, "THRESH");
    drawKnobLabel(holdSlider_, "HOLD");
    drawKnobLabel(detHpfSlider_, "SC HPF");
    drawKnobLabel(detLpfSlider_, "SC LPF");
    drawKnobLabel(midiVelSlider_, "VEL SENS");

    // Draw Trigger LED next to Threshold knob
    const float ledX = static_cast<float>(threshSlider_.getRight() - 8);
    const float ledY = static_cast<float>(threshSlider_.getY() + 4);
    g.setColour(triggerLedGlow_ ? kScGreen : kCardBg.brighter(0.1f));
    g.fillEllipse(ledX, ledY, 8.0f, 8.0f);
}

void DuckFlowAudioProcessorEditor::resized() {
    processor_.setSavedEditorSize(getWidth(), getHeight());

    // 1. Top Header Bar
    auto hdr = getLocalBounds().withHeight(44).reduced(14, 9);
    hdr.removeFromLeft(128); // Logo space
    prevPresetBtn_.setBounds(hdr.removeFromLeft(24));
    hdr.removeFromLeft(3);
    presetBox_.setBounds(hdr.removeFromLeft(210));
    hdr.removeFromLeft(3);
    nextPresetBtn_.setBounds(hdr.removeFromLeft(24));
    hdr.removeFromLeft(8);
    lockTriggerBtn_.setBounds(hdr.removeFromLeft(78));
    hdr.removeFromLeft(10);
    undoBtn_.setBounds(hdr.removeFromLeft(46));
    hdr.removeFromLeft(4);
    redoBtn_.setBounds(hdr.removeFromLeft(46));
    hdr.removeFromLeft(8);
    copyBtn_.setBounds(hdr.removeFromLeft(46));
    hdr.removeFromLeft(4);
    pasteBtn_.setBounds(hdr.removeFromLeft(46));

    bypassBtn_.setBounds(hdr.removeFromRight(68));
    hdr.removeFromRight(8);
    syncGroupBox_.setBounds(hdr.removeFromRight(92));

    // 2. Main Middle Area (Left Multiband + Center Envelope Editor)
    const int topY = 50;
    const int bottomH = 148;
    const int midH = getHeight() - topY - bottomH - 8;
    const int leftW = std::clamp(getWidth() / 4, 215, 310);

    auto leftArea = juce::Rectangle<int>(14, topY + 6, leftW - 12, midH - 12);
    auto mbRow1 = leftArea.removeFromTop(24);
    bandModeBox_.setBounds(mbRow1.removeFromLeft(mbRow1.getWidth() / 2 - 2));
    mbRow1.removeFromLeft(4);
    slopeBox_.setBounds(mbRow1);
    leftArea.removeFromTop(5);

    auto mbRow2 = leftArea.removeFromTop(24);
    unlinkBandsBtn_.setBounds(mbRow2.removeFromLeft(66));
    mbRow2.removeFromLeft(4);
    linearPhaseBtn_.setBounds(mbRow2.removeFromLeft(78));
    mbRow2.removeFromLeft(4);
    bandSoloBox_.setBounds(mbRow2);
    leftArea.removeFromTop(6);

    const int bottomBandControlsH = 112;
    spectrumView_.setBounds(leftArea.removeFromTop(std::max(80, leftArea.getHeight() - bottomBandControlsH)));
    leftArea.removeFromTop(14);

    auto xoverKnobs = leftArea.removeFromLeft(108);
    lowFreqSlider_.setBounds(xoverKnobs.removeFromLeft(52));
    xoverKnobs.removeFromLeft(4);
    highFreqSlider_.setBounds(xoverKnobs.removeFromLeft(52));

    const int sliderColW = std::max(28, leftArea.getWidth() / 3);
    lowAmtSlider_.setBounds(leftArea.removeFromLeft(sliderColW));
    midAmtSlider_.setBounds(leftArea.removeFromLeft(sliderColW));
    highAmtSlider_.setBounds(leftArea);

    // Center Envelope Area
    auto centerArea = juce::Rectangle<int>(leftW + 16, topY, getWidth() - leftW - 24, midH);
    auto toolRow = centerArea.removeFromTop(26);
    for (auto& tb : toolButtons_) {
        tb.setBounds(toolRow.removeFromLeft(44));
        toolRow.removeFromLeft(3);
    }
    toolRow.removeFromLeft(6);
    snapBtn_.setBounds(toolRow.removeFromLeft(52));
    toolRow.removeFromLeft(4);
    gridDivBox_.setBounds(toolRow.removeFromLeft(76));
    toolRow.removeFromLeft(4);
    scScopeBtn_.setBounds(toolRow.removeFromLeft(74));

    auto opsRow = centerArea.removeFromBottom(26);
    for (auto* opBtn : {&invBtn_, &revBtn_, &rotLBtn_, &rotRBtn_, &dup2xBtn_, &halfBtn_, &randBtn_, &traceLiveBtn_, &loadWavBtn_}) {
        const int bw = (opBtn == &traceLiveBtn_ || opBtn == &loadWavBtn_) ? 68 : 52;
        opBtn->setBounds(opsRow.removeFromLeft(bw));
        opsRow.removeFromLeft(3);
    }

    centerArea.reduce(0, 4);
    envelopeCanvas_.setBounds(centerArea);

    // 3. Bottom Control Panel (Quick Shapes Strip + Trigger / Main Knobs / Detector)
    auto bot = juce::Rectangle<int>(14, getHeight() - bottomH + 2, getWidth() - 28, bottomH - 12);
    auto shapeRow = bot.removeFromTop(24);
    moreShapesBtn_.setBounds(shapeRow.removeFromRight(76));
    shapeRow.removeFromRight(4);
    const int qBtnW = shapeRow.getWidth() / static_cast<int>(shapeBtns_.size());
    for (auto& sb : shapeBtns_) {
        sb.setBounds(shapeRow.removeFromLeft(qBtnW - 3));
        shapeRow.removeFromLeft(3);
    }

    bot.removeFromTop(16);
    auto trigCol = bot.removeFromLeft(134);
    triggerModeBox_.setBounds(trigCol.removeFromTop(24));
    trigCol.removeFromTop(5);
    loopModeBox_.setBounds(trigCol.removeFromTop(24));
    trigCol.removeFromTop(5);
    beatDivBox_.setBounds(trigCol.removeFromTop(24));

    bot.removeFromLeft(8);
    const int knobW = std::clamp((bot.getWidth() - 60) / 11, 50, 72);
    auto placeKnob = [&](juce::Component& c, int w) {
        c.setBounds(bot.removeFromLeft(w));
        bot.removeFromLeft(3);
    };

    freeHzSlider_.setBounds(beatDivBox_.getBounds());
    placeKnob(swingSlider_, knobW);
    placeKnob(depthKnob_, knobW + 14);
    placeKnob(smoothSlider_, knobW);

    const auto offBounds = bot.removeFromLeft(knobW + 8);
    offsetMsSlider_.setBounds(offBounds);
    offsetSamplesSlider_.setBounds(offBounds);
    offsetUnitSplBtn_.setBounds(offBounds.getRight() - 28, offBounds.getY() - 13, 28, 14);
    bot.removeFromLeft(6);

    // Sidechain / MIDI Detector Knobs
    placeKnob(threshSlider_, knobW);
    placeKnob(holdSlider_, knobW);
    placeKnob(detHpfSlider_, knobW);
    placeKnob(detLpfSlider_, knobW);

    auto detBtns = bot.removeFromLeft(48);
    detFilterBtn_.setBounds(detBtns.removeFromTop(24));
    detBtns.removeFromTop(4);
    scListenBtn_.setBounds(detBtns.removeFromTop(24));
    bot.removeFromLeft(4);

    placeKnob(midiVelSlider_, knobW);
    placeKnob(outGainSlider_, knobW);
    dryWetSlider_.setBounds(bot.removeFromLeft(knobW));
}

} // namespace duckflow
