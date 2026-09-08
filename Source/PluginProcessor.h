#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "RhythmEngine.h"
#include <vector>
#include <array>

class MidiRythmGenProcessor : public juce::AudioProcessor
{
public:
    MidiRythmGenProcessor();
    ~MidiRythmGenProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    AlgorithmicRhythm::RhythmEngine& getRhythmEngine() noexcept { return rhythmEngine; }
    const AlgorithmicRhythm::RhythmEngine& getRhythmEngine() const noexcept { return rhythmEngine; }

    void setSelectedTrackIndex (int trackIdx) noexcept { selectedTrackIndex.store (trackIdx, std::memory_order_relaxed); }
    int getSelectedTrackIndex() const noexcept { return selectedTrackIndex.load (std::memory_order_relaxed); }

    void toggleLaneStep (int laneIdx, int stepIdx);
    uint32_t getLanePattern (int laneIdx) const;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Parameter ID helper
    static juce::String getParamId (int laneIdx, const juce::String& name)
    {
        return "lane_" + juce::String (laneIdx) + "_" + name;
    }

private:
    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;
    AlgorithmicRhythm::RhythmEngine rhythmEngine;
    std::atomic<int> selectedTrackIndex { 0 };

    // Fast atomic parameter caches
    struct CachedLaneParams
    {
        std::atomic<float>* enabled { nullptr };
        std::atomic<float>* rootNote { nullptr };
        std::atomic<float>* midiChannel { nullptr };
        std::atomic<float>* steps { nullptr };
        std::atomic<float>* clockMult { nullptr };
        std::atomic<float>* clockDiv { nullptr };
        std::atomic<float>* algo { nullptr };
        std::atomic<float>* pulses { nullptr };
        std::atomic<float>* rotation { nullptr };
        std::atomic<float>* markovDensity { nullptr };
        std::atomic<float>* poissonLambda { nullptr };
        std::atomic<float>* probability { nullptr };
        std::atomic<float>* velocity { nullptr };
        std::atomic<float>* velocityRnd { nullptr };
        std::atomic<float>* gate { nullptr };
        std::atomic<float>* timeWarp { nullptr };
        std::atomic<float>* pitchRnd { nullptr };
        std::atomic<float>* customMask { nullptr };
    };

    std::array<CachedLaneParams, AlgorithmicRhythm::kMaxLanes> cachedLaneParams;
    std::atomic<float>* globalMutationParam { nullptr };
    std::atomic<float>* globalSwingParam { nullptr };
    std::atomic<float>* globalHumanizeParam { nullptr };

    // Active note tracker to guarantee zero hanging notes
    struct ActiveNote
    {
        int laneIndex { 0 };
        int midiChannel { 1 };
        int midiNote { 36 };
        int64_t offSampleGlobal { 0 };
    };

    std::vector<ActiveNote> activeNotes;
    std::vector<AlgorithmicRhythm::ScheduledNote> scheduledNotesScratch;

    // Transport sync state
    bool wasPlaying { false };
    double lastPpqPosition { -1.0 };
    int64_t sampleCounter { 0 };

    void stopAllActiveNotes (juce::MidiBuffer& midiMessages, int sampleOffset = 0);
    void cacheParamPointers();
    void readLaneParameters (AlgorithmicRhythm::LaneParameters lanes[AlgorithmicRhythm::kMaxLanes]);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiRythmGenProcessor)
};
