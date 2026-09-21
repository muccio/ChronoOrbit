#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <vector>
#include <memory>

//==============================================================================
// Modern Minimal Dark LookAndFeel
//==============================================================================
class OrbitLookAndFeel : public juce::LookAndFeel_V4
{
public:
    OrbitLookAndFeel();

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& slider) override;

    void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox& box) override;
};

//==============================================================================
// Concentric Polymetric Orbit Visualizer Component
//==============================================================================
class OrbitVisualizerComponent : public juce::Component
{
public:
    explicit OrbitVisualizerComponent (MidiRythmGenProcessor& proc);
    ~OrbitVisualizerComponent() override = default;

    void paint (juce::Graphics& g) override;
    void updateAnimation();

    int getSelectedLane() const noexcept { return selectedLane; }
    void setSelectedLane (int lane) { selectedLane = lane; repaint(); }

    std::function<void(int)> onLaneSelected;
    std::function<void(int lane, int step)> onStepToggled;

    static const juce::Colour laneColours[AlgorithmicRhythm::kMaxLanes];

    static float computeStepAngle (int step, int totalSteps, float swing, float timeWarp) noexcept
    {
        if (totalSteps <= 0)
            return -juce::MathConstants<float>::halfPi;

        const float swingShift = (step % 2 != 0) ? (swing * 0.5f) : 0.0f;
        const double t0 = std::clamp (static_cast<double> (static_cast<float> (step) + swingShift) / static_cast<double> (totalSteps), 0.0, 0.999999);
        const double gamma = std::pow (2.0, static_cast<double> (timeWarp * 1.5f));
        const double tWarped = (std::abs (timeWarp) > 0.001f) ? std::clamp (std::pow (t0, gamma), 0.0, 0.999999) : t0;

        return -juce::MathConstants<float>::halfPi +
               static_cast<float> (juce::MathConstants<double>::twoPi * tWarped);
    }

    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;
    bool findNodeAt (float x, float y, int& outLane, int& outStep) const;

private:
    MidiRythmGenProcessor& processor;
    int selectedLane { 0 };

    // Trigger flash decay animations
    std::array<float, AlgorithmicRhythm::kMaxLanes> triggerFlashIntensity {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OrbitVisualizerComponent)
};

//==============================================================================
// Interactive Linear Step-Sequencer Strip
//==============================================================================
class StepStripComponent : public juce::Component
{
public:
    explicit StepStripComponent (MidiRythmGenProcessor& proc);
    ~StepStripComponent() override = default;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& event) override;

    static juce::Rectangle<float> computeStepBounds (int step, int totalSteps, float totalWidth, float totalHeight) noexcept
    {
        if (totalSteps <= 0)
            return {};

        const float margin = 4.0f;
        const float usableWidth = totalWidth - margin * 2.0f;
        const float slotWidth = usableWidth / static_cast<float> (totalSteps);
        const float gap = 2.0f;

        const float px = margin + static_cast<float> (step) * slotWidth + gap * 0.5f;
        const float py = 3.0f;
        const float pw = std::max (2.0f, slotWidth - gap);
        const float ph = totalHeight - 6.0f;

        return { px, py, pw, ph };
    }

    void setTrack (int trackIndex) { currentTrack = trackIndex; repaint(); }

    std::function<void(int track, int step)> onStepToggled;

private:
    MidiRythmGenProcessor& processor;
    int currentTrack { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepStripComponent)
};

//==============================================================================
// Cubase-style DAW MIDI Clip Visualizer & External Drag-and-Drop
//==============================================================================
class DawMidiClipComponent : public juce::Component,
                             public juce::SettableTooltipClient
{
public:
    explicit DawMidiClipComponent (MidiRythmGenProcessor& proc);
    ~DawMidiClipComponent() override = default;

    void paint (juce::Graphics& g) override;
    void mouseEnter (const juce::MouseEvent& event) override;
    void mouseExit (const juce::MouseEvent& event) override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;

private:
    MidiRythmGenProcessor& processor;
    bool isHovered { false };
    bool isDragging { false };
    juce::Point<int> dragStartPos;

    void exportAndStartDrag();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DawMidiClipComponent)
};

//==============================================================================
// Vector Control Matrix for 8 Track Activation (Top Right)
//==============================================================================
class TrackVectorMatrixComponent : public juce::Component
{
public:
    explicit TrackVectorMatrixComponent (MidiRythmGenProcessor& proc);
    ~TrackVectorMatrixComponent() override = default;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void updateVisuals();

private:
    MidiRythmGenProcessor& processor;
    juce::Label matrixLabel;
    std::array<juce::TextButton, AlgorithmicRhythm::kMaxLanes> vectorButtons;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, AlgorithmicRhythm::kMaxLanes> vectorAttachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackVectorMatrixComponent)
};

//==============================================================================
// Animated Stereo Pan Meter Component
//==============================================================================
class StereoPanMeterComponent : public juce::Component
{
public:
    explicit StereoPanMeterComponent (MidiRythmGenProcessor& proc);
    ~StereoPanMeterComponent() override = default;

    void paint (juce::Graphics& g) override;
    void setTrack (int trackIndex) { currentTrack = trackIndex; repaint(); }

private:
    MidiRythmGenProcessor& processor;
    int currentTrack { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StereoPanMeterComponent)
};

//==============================================================================
// Main Editor
//==============================================================================
class MidiRythmGenEditor : public juce::AudioProcessorEditor,
                           public juce::Timer
{
public:
    explicit MidiRythmGenEditor (MidiRythmGenProcessor&);
    ~MidiRythmGenEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    MidiRythmGenProcessor& audioProcessor;
    OrbitLookAndFeel customLookAndFeel;

    // Visualizer
    OrbitVisualizerComponent orbitVisualizer;

    // DAW MIDI Clip preview & Drag-and-Drop (below orbit visualizer)
    DawMidiClipComponent dawMidiClip;

    // Track selector buttons
    juce::Label titleLabel;
    juce::Label subTitleLabel;
    std::array<juce::TextButton, AlgorithmicRhythm::kMaxLanes> trackSelectButtons;
    int currentTrackIndex { 0 };

    // Audition transport and generator buttons
    juce::TextButton playButton;
    juce::TextButton randomizeButton;
    juce::TextButton randomizeTrackButton;

    // Vector track activation matrix (top right)
    TrackVectorMatrixComponent trackVectorMatrix;

    // Global controls
    juce::Slider globalMutationSlider;
    juce::Label  globalMutationLabel;
    juce::Slider globalSwingSlider;
    juce::Label  globalSwingLabel;
    juce::Slider globalHumanizeSlider;
    juce::Label  globalHumanizeLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> globalMutationAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> globalSwingAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> globalHumanizeAttach;

    // Track parameter controls (re-attached when track changes)
    juce::ToggleButton trackEnabledToggle;
    juce::ComboBox     algoComboBox;
    juce::Label        algoLabel;

    juce::Slider stepsSlider;
    juce::Label  stepsLabel;
    juce::Slider pulsesSlider;
    juce::Label  pulsesLabel;
    juce::Slider rotationSlider;
    juce::Label  rotationLabel;

    juce::Slider clockMultSlider;
    juce::Label  clockMultLabel;
    juce::Slider clockDivSlider;
    juce::Label  clockDivLabel;

    juce::Slider probabilitySlider;
    juce::Label  probabilityLabel;
    juce::Slider markovDensitySlider;
    juce::Label  markovDensityLabel;

    juce::Slider rootNoteSlider;
    juce::Label  rootNoteLabel;
    juce::Slider timeWarpSlider;
    juce::Label  timeWarpLabel;

    juce::Slider pitchRndSlider;
    juce::Label  pitchRndLabel;
    juce::Slider velocitySlider;
    juce::Label  velocityLabel;
    juce::Slider velocityRndSlider;
    juce::Label  velocityRndLabel;
    juce::Slider gateSlider;
    juce::Label  gateLabel;

    juce::Slider panSlider;
    juce::Label  panLabel;
    juce::Slider panDepthSlider;
    juce::Label  panDepthLabel;

    juce::Label    panRateLabel;
    juce::ComboBox panRateComboBox;

    StereoPanMeterComponent stereoPanMeter;

    // Interactive step strip
    juce::Label stepStripLabel;
    StepStripComponent stepStrip;

    // Dynamic APVTS attachments for the selected track
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> trackEnabledAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> algoAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> stepsAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pulsesAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rotationAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> clockMultAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> clockDivAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> probAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> markovDensityAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rootNoteAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> timeWarpAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pitchRndAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> velocityAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> velocityRndAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gateAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> panAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> panDepthAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> panRateAttach;

    void selectTrack (int trackIndex);
    void bindTrackAttachments (int trackIndex);
    void setupKnob (juce::Slider& slider, juce::Label& label, const juce::String& text, const juce::String& suffix);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiRythmGenEditor)
};
