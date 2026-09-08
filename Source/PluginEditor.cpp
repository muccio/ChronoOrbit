#include "PluginEditor.h"
#include <cmath>

//==============================================================================
// Lane Colors (Dark Cyberpunk / Modular Aesthetic)
//==============================================================================
const juce::Colour OrbitVisualizerComponent::laneColours[AlgorithmicRhythm::kMaxLanes] =
{
    juce::Colour (0xff00e5ff), // 1: Neon Cyan
    juce::Colour (0xff00ff88), // 2: Electric Mint
    juce::Colour (0xffffbb00), // 3: Warm Amber
    juce::Colour (0xffff0055), // 4: Cyber Crimson
    juce::Colour (0xffaa00ff), // 5: Deep Purple
    juce::Colour (0xff0088ff), // 6: Cobalt Blue
    juce::Colour (0xffff5500), // 7: Blaze Orange
    juce::Colour (0xfff0ff00)  // 8: Neon Yellow
};

//==============================================================================
// OrbitLookAndFeel
//==============================================================================
OrbitLookAndFeel::OrbitLookAndFeel()
{
    setColour (juce::Slider::thumbColourId, juce::Colour (0xff00e5ff));
    setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xff00e5ff));
    setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff252930));
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff14171c));
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff313742));
    setColour (juce::ComboBox::textColourId, juce::Colours::white.withAlpha (0.9f));
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff14171c));
    setColour (juce::PopupMenu::textColourId, juce::Colours::white);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff00e5ff).withAlpha (0.3f));
}

void OrbitLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPosProportional, float rotaryStartAngle,
                                        float rotaryEndAngle, juce::Slider& slider)
{
    juce::ignoreUnused (slider);

    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float centreX = bounds.getCentreX();
    const float centreY = bounds.getCentreY();
    const float rx = centreX - radius;
    const float ry = centreY - radius;
    const float rw = radius * 2.0f;
    const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // Background track arc
    juce::Path backgroundArc;
    backgroundArc.addCentredArc (centreX, centreY, radius - 3.0f, radius - 3.0f,
                                 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour (0xff1c2129));
    g.strokePath (backgroundArc, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Dial background disc
    g.setColour (juce::Colour (0xff12151a));
    g.fillEllipse (rx + 4.0f, ry + 4.0f, rw - 8.0f, rw - 8.0f);
    g.setColour (juce::Colour (0xff2a303c));
    g.drawEllipse (rx + 4.0f, ry + 4.0f, rw - 8.0f, rw - 8.0f, 1.0f);

    // Value active arc
    if (sliderPosProportional > 0.001f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc (centreX, centreY, radius - 3.0f, radius - 3.0f,
                                0.0f, rotaryStartAngle, angle, true);
        g.setColour (juce::Colour (0xff00e5ff));
        g.strokePath (valueArc, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Pointer needle
    juce::Path pointer;
    const float needleLen = radius * 0.65f;
    pointer.addLineSegment (juce::Line<float> (centreX, centreY,
                                               centreX + needleLen * std::sin (angle),
                                               centreY - needleLen * std::cos (angle)), 2.0f);
    g.setColour (juce::Colours::white);
    g.strokePath (pointer, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void OrbitLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                                    int buttonX, int buttonY, int buttonW, int buttonH,
                                    juce::ComboBox& box)
{
    juce::ignoreUnused (isButtonDown, buttonX, buttonY, buttonW, buttonH);

    const auto bounds = juce::Rectangle<float> (0, 0, static_cast<float> (width), static_cast<float> (height));
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (bounds, 4.0f);

    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    // Subtle down arrow
    const float arrowX = static_cast<float> (width) - 18.0f;
    const float arrowY = static_cast<float> (height) * 0.5f - 2.0f;
    juce::Path arrow;
    arrow.startNewSubPath (arrowX, arrowY);
    arrow.lineTo (arrowX + 5.0f, arrowY + 5.0f);
    arrow.lineTo (arrowX + 10.0f, arrowY);
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.strokePath (arrow, juce::PathStrokeType (1.5f));
}

//==============================================================================
// OrbitVisualizerComponent Implementation
//==============================================================================
OrbitVisualizerComponent::OrbitVisualizerComponent (MidiRythmGenProcessor& proc)
    : processor (proc)
{
    triggerFlashIntensity.fill (0.0f);
}

void OrbitVisualizerComponent::mouseDown (const juce::MouseEvent& event)
{
    const float cx = static_cast<float> (getWidth()) * 0.5f;
    const float cy = static_cast<float> (getHeight()) * 0.5f;
    const float dx = event.position.x - cx;
    const float dy = event.position.y - cy;
    const float dist = std::sqrt (dx * dx + dy * dy);

    const float maxRadius = juce::jmin (cx, cy) - 20.0f;
    const float minRadius = maxRadius * 0.22f;

    if (dist >= minRadius - 10.0f && dist <= maxRadius + 10.0f)
    {
        const float norm = (dist - minRadius) / (maxRadius - minRadius);
        const int lane = std::clamp (static_cast<int> (std::round (norm * static_cast<float> (AlgorithmicRhythm::kMaxLanes - 1))),
                                     0, AlgorithmicRhythm::kMaxLanes - 1);
        setSelectedLane (lane);
        if (onLaneSelected)
            onLaneSelected (lane);
    }
}

void OrbitVisualizerComponent::updateAnimation()
{
    for (int i = 0; i < AlgorithmicRhythm::kMaxLanes; ++i)
    {
        auto& tele = processor.getRhythmEngine().getTelemetry (i);
        if (tele.justTriggered.exchange (false, std::memory_order_relaxed))
        {
            triggerFlashIntensity[static_cast<size_t> (i)] = 1.0f;
        }
        else
        {
            triggerFlashIntensity[static_cast<size_t> (i)] *= 0.82f; // Smooth exponential decay
        }
    }
}

void OrbitVisualizerComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0a0c10));

    const float width  = static_cast<float> (getWidth());
    const float height = static_cast<float> (getHeight());
    const float cx = width  * 0.5f;
    const float cy = height * 0.5f;

    const float maxRadius = juce::jmin (cx, cy) - 24.0f;
    const float minRadius = maxRadius * 0.22f;
    const float radiusStep = (maxRadius - minRadius) / static_cast<float> (AlgorithmicRhythm::kMaxLanes - 1);

    // Center core glyph
    g.setColour (juce::Colour (0xff12161f));
    g.fillEllipse (cx - minRadius * 0.8f, cy - minRadius * 0.8f, minRadius * 1.6f, minRadius * 1.6f);
    g.setColour (juce::Colour (0xff00e5ff).withAlpha (0.4f));
    g.drawEllipse (cx - minRadius * 0.8f, cy - minRadius * 0.8f, minRadius * 1.6f, minRadius * 1.6f, 1.0f);

    g.setFont (juce::Font (10.0f, juce::Font::bold));
    g.setColour (juce::Colours::white.withAlpha (0.7f));
    g.drawText ("SYNC", static_cast<int> (cx - 20.0f), static_cast<int> (cy - 8.0f), 40, 16, juce::Justification::centred);

    // Render concentric orbits
    for (int lane = 0; lane < AlgorithmicRhythm::kMaxLanes; ++lane)
    {
        const auto& tele = processor.getRhythmEngine().getTelemetry (lane);
        const int steps = std::clamp (tele.totalSteps.load (std::memory_order_relaxed), 1, AlgorithmicRhythm::kMaxSteps);
        const uint32_t activeMask = tele.activePatternMask.load (std::memory_order_relaxed);
        const float playhead = tele.playheadNorm.load (std::memory_order_relaxed);
        const float flash = triggerFlashIntensity[static_cast<size_t> (lane)];

        const float r = minRadius + lane * radiusStep;
        const juce::Colour baseColour = laneColours[lane];
        const bool isSelected = (lane == selectedLane);

        // Orbit ring background
        juce::Colour ringColour = isSelected ? baseColour.withAlpha (0.45f)
                                             : juce::Colour (0xff1c212a).withAlpha (0.6f);
        g.setColour (ringColour);
        g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, isSelected ? 1.8f : 1.0f);

        // Flash aura on trigger
        if (flash > 0.05f)
        {
            g.setColour (baseColour.withAlpha (flash * 0.35f));
            g.drawEllipse (cx - r - 2.0f, cy - r - 2.0f, (r + 2.0f) * 2.0f, (r + 2.0f) * 2.0f, 3.0f);
        }

        // Draw step nodes
        for (int s = 0; s < steps; ++s)
        {
            const float angle = -juce::MathConstants<float>::halfPi +
                                (juce::MathConstants<float>::twoPi * static_cast<float> (s)) / static_cast<float> (steps);

            const float nx = cx + r * std::cos (angle);
            const float ny = cy + r * std::sin (angle);

            const bool isHit = (activeMask & (1U << s)) != 0;

            if (isHit)
            {
                const float nodeSize = isSelected ? 8.0f : 6.0f;
                g.setColour (baseColour.withAlpha (0.95f));
                g.fillEllipse (nx - nodeSize * 0.5f, ny - nodeSize * 0.5f, nodeSize, nodeSize);

                // Subtle outer glow
                g.setColour (baseColour.withAlpha (0.35f));
                g.drawEllipse (nx - nodeSize * 0.5f - 1.0f, ny - nodeSize * 0.5f - 1.0f, nodeSize + 2.0f, nodeSize + 2.0f, 1.0f);
            }
            else
            {
                const float nodeSize = 3.0f;
                g.setColour (juce::Colour (0xff252a33));
                g.fillEllipse (nx - nodeSize * 0.5f, ny - nodeSize * 0.5f, nodeSize, nodeSize);
            }
        }

        // Draw radial playhead marker
        const float playAngle = -juce::MathConstants<float>::halfPi +
                                juce::MathConstants<float>::twoPi * playhead;
        const float px = cx + r * std::cos (playAngle);
        const float py = cy + r * std::sin (playAngle);

        // Playhead orb
        const float playheadRadius = (flash > 0.1f) ? (6.0f + flash * 4.0f) : 4.5f;
        g.setColour (juce::Colours::white);
        g.fillEllipse (px - playheadRadius * 0.5f, py - playheadRadius * 0.5f, playheadRadius, playheadRadius);

        if (flash > 0.05f)
        {
            g.setColour (baseColour.withAlpha (flash * 0.8f));
            g.drawEllipse (px - playheadRadius, py - playheadRadius, playheadRadius * 2.0f, playheadRadius * 2.0f, 2.0f);
        }
    }
}

//==============================================================================
// Main Editor Implementation
//==============================================================================
MidiRythmGenEditor::MidiRythmGenEditor (MidiRythmGenProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      orbitVisualizer (p)
{
    setLookAndFeel (&customLookAndFeel);

    // Title labels
    titleLabel.setText ("CHRONO-ORBIT", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (18.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xff00e5ff));
    addAndMakeVisible (titleLabel);

    subTitleLabel.setText ("ALGORITHMIC MULTI-TRACK MIDI GENERATOR", juce::dontSendNotification);
    subTitleLabel.setFont (juce::Font (10.0f, juce::Font::plain));
    subTitleLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.5f));
    addAndMakeVisible (subTitleLabel);

    // Orbit visualizer
    addAndMakeVisible (orbitVisualizer);
    orbitVisualizer.onLaneSelected = [this] (int lane) { selectTrack (lane); };

    // Global controls
    setupKnob (globalMutationSlider, globalMutationLabel, "MUTATION", "%");
    setupKnob (globalSwingSlider,    globalSwingLabel,    "SWING", "%");
    setupKnob (globalHumanizeSlider, globalHumanizeLabel, "HUMANIZE", "%");

    globalMutationAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.getAPVTS(), "global_mutation", globalMutationSlider);
    globalSwingAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.getAPVTS(), "global_swing", globalSwingSlider);
    globalHumanizeAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.getAPVTS(), "global_humanize", globalHumanizeSlider);

    // Track selection tab buttons (1 to 8)
    for (int i = 0; i < AlgorithmicRhythm::kMaxLanes; ++i)
    {
        auto& btn = trackSelectButtons[static_cast<size_t> (i)];
        btn.setButtonText ("T" + juce::String (i + 1));
        btn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff161a22));
        btn.setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.7f));
        btn.onClick = [this, i] { selectTrack (i); };
        addAndMakeVisible (btn);
    }

    // Per-track controls
    trackEnabledToggle.setButtonText ("LANE ACTIVE");
    trackEnabledToggle.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    trackEnabledToggle.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff00e5ff));
    addAndMakeVisible (trackEnabledToggle);

    algoComboBox.addItemList ({ "Euclidean (Bjorklund)", "Markov Chain", "Poisson Burst" }, 1);
    addAndMakeVisible (algoComboBox);
    algoLabel.setText ("ALGORITHM", juce::dontSendNotification);
    algoLabel.setFont (juce::Font (10.0f, juce::Font::bold));
    algoLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
    addAndMakeVisible (algoLabel);

    setupKnob (stepsSlider, stepsLabel, "STEPS", "");
    setupKnob (pulsesSlider, pulsesLabel, "PULSES", "");
    setupKnob (rotationSlider, rotationLabel, "ROTATION", "");
    setupKnob (clockMultSlider, clockMultLabel, "MULT", "x");
    setupKnob (clockDivSlider, clockDivLabel, "DIV", "/");
    setupKnob (probabilitySlider, probabilityLabel, "PROB", "%");
    setupKnob (markovDensitySlider, markovDensityLabel, "DENSITY", "");
    setupKnob (rootNoteSlider, rootNoteLabel, "ROOT", "");

    juce::StringArray scaleList;
    for (int s = 0; s < static_cast<int> (AlgorithmicRhythm::ScaleType::NumScales); ++s)
    {
        scaleList.add (AlgorithmicRhythm::ScaleQuantizer::getScaleName (static_cast<AlgorithmicRhythm::ScaleType> (s)));
    }
    scaleComboBox.addItemList (scaleList, 1);
    addAndMakeVisible (scaleComboBox);
    scaleLabel.setText ("SCALE", juce::dontSendNotification);
    scaleLabel.setFont (juce::Font (10.0f, juce::Font::bold));
    scaleLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
    addAndMakeVisible (scaleLabel);

    setupKnob (pitchRndSlider, pitchRndLabel, "PITCH RND", "st");
    setupKnob (velocitySlider, velocityLabel, "VELOCITY", "");
    setupKnob (velocityRndSlider, velocityRndLabel, "VEL RND", "");
    setupKnob (gateSlider, gateLabel, "GATE", "x");

    // Initialize track 0
    selectTrack (0);

    setSize (920, 560);
    startTimerHz (60); // 60 FPS smooth GUI telemetry update
}

MidiRythmGenEditor::~MidiRythmGenEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MidiRythmGenEditor::setupKnob (juce::Slider& slider, juce::Label& label, const juce::String& text, const juce::String& suffix)
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 16);
    slider.setTextValueSuffix (" " + suffix);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white.withAlpha (0.9f));
    addAndMakeVisible (slider);

    label.setText (text, juce::dontSendNotification);
    label.setFont (juce::Font (10.0f, juce::Font::bold));
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
    addAndMakeVisible (label);
}

void MidiRythmGenEditor::selectTrack (int trackIndex)
{
    currentTrackIndex = std::clamp (trackIndex, 0, AlgorithmicRhythm::kMaxLanes - 1);

    for (int i = 0; i < AlgorithmicRhythm::kMaxLanes; ++i)
    {
        const bool isCur = (i == currentTrackIndex);
        trackSelectButtons[static_cast<size_t> (i)].setColour (
            juce::TextButton::buttonColourId,
            isCur ? OrbitVisualizerComponent::laneColours[i].withAlpha (0.4f) : juce::Colour (0xff161a22));
    }

    orbitVisualizer.setSelectedLane (currentTrackIndex);
    bindTrackAttachments (currentTrackIndex);
}

void MidiRythmGenEditor::bindTrackAttachments (int trackIndex)
{
    // Reset existing attachments
    trackEnabledAttach.reset();
    algoAttach.reset();
    stepsAttach.reset();
    pulsesAttach.reset();
    rotationAttach.reset();
    clockMultAttach.reset();
    clockDivAttach.reset();
    probAttach.reset();
    markovDensityAttach.reset();
    rootNoteAttach.reset();
    scaleAttach.reset();
    pitchRndAttach.reset();
    velocityAttach.reset();
    velocityRndAttach.reset();
    gateAttach.reset();

    auto& apvts = audioProcessor.getAPVTS();

    trackEnabledAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, MidiRythmGenProcessor::getParamId (trackIndex, "enabled"), trackEnabledToggle);

    algoAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, MidiRythmGenProcessor::getParamId (trackIndex, "algo"), algoComboBox);

    stepsAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, MidiRythmGenProcessor::getParamId (trackIndex, "steps"), stepsSlider);

    pulsesAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, MidiRythmGenProcessor::getParamId (trackIndex, "pulses"), pulsesSlider);

    rotationAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, MidiRythmGenProcessor::getParamId (trackIndex, "rotation"), rotationSlider);

    clockMultAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, MidiRythmGenProcessor::getParamId (trackIndex, "clock_mult"), clockMultSlider);

    clockDivAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, MidiRythmGenProcessor::getParamId (trackIndex, "clock_div"), clockDivSlider);

    probAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, MidiRythmGenProcessor::getParamId (trackIndex, "probability"), probabilitySlider);

    markovDensityAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, MidiRythmGenProcessor::getParamId (trackIndex, "markov_density"), markovDensitySlider);

    rootNoteAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, MidiRythmGenProcessor::getParamId (trackIndex, "root_note"), rootNoteSlider);

    scaleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, MidiRythmGenProcessor::getParamId (trackIndex, "scale"), scaleComboBox);

    pitchRndAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, MidiRythmGenProcessor::getParamId (trackIndex, "pitch_rnd"), pitchRndSlider);

    velocityAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, MidiRythmGenProcessor::getParamId (trackIndex, "velocity"), velocitySlider);

    velocityRndAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, MidiRythmGenProcessor::getParamId (trackIndex, "velocity_rnd"), velocityRndSlider);

    gateAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, MidiRythmGenProcessor::getParamId (trackIndex, "gate"), gateSlider);
}

void MidiRythmGenEditor::timerCallback()
{
    orbitVisualizer.updateAnimation();
    orbitVisualizer.repaint();
}

void MidiRythmGenEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0d1016));

    // Dividing lines & subtle panels
    g.setColour (juce::Colour (0xff1a1f29));
    g.drawLine (520.0f, 0.0f, 520.0f, static_cast<float> (getHeight()), 1.0f);
    g.drawLine (0.0f, 60.0f, static_cast<float> (getWidth()), 60.0f, 1.0f);

    // Track controls panel header
    g.setFont (juce::Font (12.0f, juce::Font::bold));
    g.setColour (OrbitVisualizerComponent::laneColours[currentTrackIndex]);
    g.drawText ("TRACK " + juce::String (currentTrackIndex + 1) + " CONFIGURATION",
                540, 70, 300, 20, juce::Justification::left);
}

void MidiRythmGenEditor::resized()
{
    // Header
    titleLabel.setBounds (20, 10, 200, 24);
    subTitleLabel.setBounds (20, 32, 280, 16);

    // Global knobs (top right)
    globalMutationLabel.setBounds (290, 8, 70, 14);
    globalMutationSlider.setBounds (290, 20, 70, 36);

    globalSwingLabel.setBounds (365, 8, 70, 14);
    globalSwingSlider.setBounds (365, 20, 70, 36);

    globalHumanizeLabel.setBounds (440, 8, 70, 14);
    globalHumanizeSlider.setBounds (440, 20, 70, 36);

    // Orbit visualizer (central left)
    orbitVisualizer.setBounds (15, 75, 490, 465);

    // Track tabs (right top)
    const int tabWidth = 42;
    for (int i = 0; i < AlgorithmicRhythm::kMaxLanes; ++i)
    {
        trackSelectButtons[static_cast<size_t> (i)].setBounds (535 + i * (tabWidth + 4), 98, tabWidth, 26);
    }

    trackEnabledToggle.setBounds (535, 134, 120, 24);
    algoLabel.setBounds (665, 126, 90, 14);
    algoComboBox.setBounds (665, 142, 230, 24);

    // Row 1 Knobs: Steps, Pulses, Rotation, Mult, Div
    int kY1 = 180;
    int kW = 68;
    int kH = 68;
    int kSpacing = 72;

    stepsLabel.setBounds (535 + 0 * kSpacing, kY1, kW, 14);
    stepsSlider.setBounds (535 + 0 * kSpacing, kY1 + 14, kW, kH);

    pulsesLabel.setBounds (535 + 1 * kSpacing, kY1, kW, 14);
    pulsesSlider.setBounds (535 + 1 * kSpacing, kY1 + 14, kW, kH);

    rotationLabel.setBounds (535 + 2 * kSpacing, kY1, kW, 14);
    rotationSlider.setBounds (535 + 2 * kSpacing, kY1 + 14, kW, kH);

    clockMultLabel.setBounds (535 + 3 * kSpacing, kY1, kW, 14);
    clockMultSlider.setBounds (535 + 3 * kSpacing, kY1 + 14, kW, kH);

    clockDivLabel.setBounds (535 + 4 * kSpacing, kY1, kW, 14);
    clockDivSlider.setBounds (535 + 4 * kSpacing, kY1 + 14, kW, kH);

    // Row 2 Knobs: Probability, Markov Density, Gate, Velocity, Vel Rnd
    int kY2 = 280;
    probabilityLabel.setBounds (535 + 0 * kSpacing, kY2, kW, 14);
    probabilitySlider.setBounds (535 + 0 * kSpacing, kY2 + 14, kW, kH);

    markovDensityLabel.setBounds (535 + 1 * kSpacing, kY2, kW, 14);
    markovDensitySlider.setBounds (535 + 1 * kSpacing, kY2 + 14, kW, kH);

    gateLabel.setBounds (535 + 2 * kSpacing, kY2, kW, 14);
    gateSlider.setBounds (535 + 2 * kSpacing, kY2 + 14, kW, kH);

    velocityLabel.setBounds (535 + 3 * kSpacing, kY2, kW, 14);
    velocitySlider.setBounds (535 + 3 * kSpacing, kY2 + 14, kW, kH);

    velocityRndLabel.setBounds (535 + 4 * kSpacing, kY2, kW, 14);
    velocityRndSlider.setBounds (535 + 4 * kSpacing, kY2 + 14, kW, kH);

    // Row 3: Root, Scale, Pitch Rnd
    int kY3 = 380;
    rootNoteLabel.setBounds (535 + 0 * kSpacing, kY3, kW, 14);
    rootNoteSlider.setBounds (535 + 0 * kSpacing, kY3 + 14, kW, kH);

    scaleLabel.setBounds (535 + 1 * kSpacing, kY3 + 6, 120, 14);
    scaleComboBox.setBounds (535 + 1 * kSpacing, kY3 + 24, 180, 24);

    pitchRndLabel.setBounds (535 + 4 * kSpacing, kY3, kW, 14);
    pitchRndSlider.setBounds (535 + 4 * kSpacing, kY3 + 14, kW, kH);
}
