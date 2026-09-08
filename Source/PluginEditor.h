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

    void setTrack (int trackIndex) { currentTrack = trackIndex; repaint(); }

    std::function<void(int track, int step)> onStepToggled;

private:
    MidiRythmGenProcessor& processor;
    int currentTrack { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepStripComponent)
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

    // Track selector buttons
    juce::Label titleLabel;
    juce::Label subTitleLabel;
    std::array<juce::TextButton, AlgorithmicRhythm::kMaxLanes> trackSelectButtons;
    int currentTrackIndex { 0 };

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
    juce::ComboBox scaleComboBox;
    juce::Label  scaleLabel;

    juce::Slider pitchRndSlider;
    juce::Label  pitchRndLabel;
    juce::Slider velocitySlider;
    juce::Label  velocityLabel;
    juce::Slider velocityRndSlider;
    juce::Label  velocityRndLabel;
    juce::Slider gateSlider;
    juce::Label  gateLabel;

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
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scaleAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pitchRndAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> velocityAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> velocityRndAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gateAttach;

    void selectTrack (int trackIndex);
    void bindTrackAttachments (int trackIndex);
    void setupKnob (juce::Slider& slider, juce::Label& label, const juce::String& text, const juce::String& suffix);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiRythmGenEditor)
};
