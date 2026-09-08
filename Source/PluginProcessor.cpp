#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
MidiRythmGenProcessor::MidiRythmGenProcessor()
    : AudioProcessor (BusesProperties()), // Pure MIDI processor - no audio I/O buses
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    activeNotes.reserve (128);
    scheduledNotesScratch.reserve (64);
    cacheParamPointers();
}

MidiRythmGenProcessor::~MidiRythmGenProcessor()
{
}

//==============================================================================
const juce::String MidiRythmGenProcessor::getName() const
{
    return JucePlugin_Name;
}

bool MidiRythmGenProcessor::acceptsMidi() const
{
    return true;
}

bool MidiRythmGenProcessor::producesMidi() const
{
    return true;
}

bool MidiRythmGenProcessor::isMidiEffect() const
{
    return true;
}

double MidiRythmGenProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int MidiRythmGenProcessor::getNumPrograms()
{
    return 1;
}

int MidiRythmGenProcessor::getCurrentProgram()
{
    return 0;
}

void MidiRythmGenProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String MidiRythmGenProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void MidiRythmGenProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
bool MidiRythmGenProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Strict MIDI effect / generator: 0 audio ins and 0 audio outs
    return layouts.getMainInputChannels() == 0 && layouts.getMainOutputChannels() == 0;
}

void MidiRythmGenProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    rhythmEngine.prepare (sampleRate);
    activeNotes.clear();
    scheduledNotesScratch.clear();
    wasPlaying = false;
    lastPpqPosition = -1.0;
    sampleCounter = 0;
}

void MidiRythmGenProcessor::releaseResources()
{
    activeNotes.clear();
    scheduledNotesScratch.clear();
    rhythmEngine.reset();
}

void MidiRythmGenProcessor::stopAllActiveNotes (juce::MidiBuffer& midiMessages, int sampleOffset)
{
    sampleOffset = std::max (0, sampleOffset);

    // Send note-off for all currently sounding notes
    for (const auto& note : activeNotes)
    {
        midiMessages.addEvent (juce::MidiMessage::noteOff (note.midiChannel, note.midiNote, 0.0f), sampleOffset);
    }
    activeNotes.clear();

    // Also issue all-notes-off CC on all 16 MIDI channels for absolute cleanliness
    for (int ch = 1; ch <= 16; ++ch)
    {
        midiMessages.addEvent (juce::MidiMessage::allNotesOff (ch), sampleOffset);
        midiMessages.addEvent (juce::MidiMessage::allSoundOff (ch), sampleOffset);
    }
}

void MidiRythmGenProcessor::cacheParamPointers()
{
    globalMutationParam = apvts.getRawParameterValue ("global_mutation");
    globalSwingParam    = apvts.getRawParameterValue ("global_swing");
    globalHumanizeParam = apvts.getRawParameterValue ("global_humanize");

    for (int i = 0; i < AlgorithmicRhythm::kMaxLanes; ++i)
    {
        auto& cp = cachedLaneParams[static_cast<size_t> (i)];
        cp.enabled        = apvts.getRawParameterValue (getParamId (i, "enabled"));
        cp.rootNote       = apvts.getRawParameterValue (getParamId (i, "root_note"));
        cp.midiChannel    = apvts.getRawParameterValue (getParamId (i, "midi_channel"));
        cp.steps          = apvts.getRawParameterValue (getParamId (i, "steps"));
        cp.clockMult      = apvts.getRawParameterValue (getParamId (i, "clock_mult"));
        cp.clockDiv       = apvts.getRawParameterValue (getParamId (i, "clock_div"));
        cp.algo           = apvts.getRawParameterValue (getParamId (i, "algo"));
        cp.pulses         = apvts.getRawParameterValue (getParamId (i, "pulses"));
        cp.rotation       = apvts.getRawParameterValue (getParamId (i, "rotation"));
        cp.markovDensity  = apvts.getRawParameterValue (getParamId (i, "markov_density"));
        cp.poissonLambda  = apvts.getRawParameterValue (getParamId (i, "poisson_lambda"));
        cp.probability    = apvts.getRawParameterValue (getParamId (i, "probability"));
        cp.velocity       = apvts.getRawParameterValue (getParamId (i, "velocity"));
        cp.velocityRnd    = apvts.getRawParameterValue (getParamId (i, "velocity_rnd"));
        cp.gate           = apvts.getRawParameterValue (getParamId (i, "gate"));
        cp.scale          = apvts.getRawParameterValue (getParamId (i, "scale"));
        cp.pitchRnd       = apvts.getRawParameterValue (getParamId (i, "pitch_rnd"));
    }
}

void MidiRythmGenProcessor::readLaneParameters (AlgorithmicRhythm::LaneParameters lanes[AlgorithmicRhythm::kMaxLanes])
{
    const float globalMut = globalMutationParam != nullptr ? globalMutationParam->load (std::memory_order_relaxed) : 0.0f;
    const float globalSwg = globalSwingParam    != nullptr ? globalSwingParam->load (std::memory_order_relaxed) : 0.0f;
    const float globalHum = globalHumanizeParam != nullptr ? globalHumanizeParam->load (std::memory_order_relaxed) : 0.0f;

    for (int i = 0; i < AlgorithmicRhythm::kMaxLanes; ++i)
    {
        const auto& cp = cachedLaneParams[static_cast<size_t> (i)];
        auto& p = lanes[i];

        p.enabled            = (cp.enabled != nullptr) ? (cp.enabled->load (std::memory_order_relaxed) > 0.5f) : true;
        p.rootNote           = (cp.rootNote != nullptr) ? static_cast<int> (cp.rootNote->load (std::memory_order_relaxed)) : 36;
        p.midiChannel        = (cp.midiChannel != nullptr) ? static_cast<int> (cp.midiChannel->load (std::memory_order_relaxed)) : 1;
        p.steps              = (cp.steps != nullptr) ? static_cast<int> (cp.steps->load (std::memory_order_relaxed)) : 16;
        p.clockMultiplier    = (cp.clockMult != nullptr) ? static_cast<int> (cp.clockMult->load (std::memory_order_relaxed)) : 1;
        p.clockDivider       = (cp.clockDiv != nullptr) ? static_cast<int> (cp.clockDiv->load (std::memory_order_relaxed)) : 1;
        p.algorithm          = (cp.algo != nullptr) ? static_cast<AlgorithmicRhythm::AlgorithmMode> (static_cast<int> (cp.algo->load (std::memory_order_relaxed))) : AlgorithmicRhythm::AlgorithmMode::Euclidean;
        p.euclideanPulses    = (cp.pulses != nullptr) ? static_cast<int> (cp.pulses->load (std::memory_order_relaxed)) : 4;
        p.euclideanRotation  = (cp.rotation != nullptr) ? static_cast<int> (cp.rotation->load (std::memory_order_relaxed)) : 0;
        p.markovDensity      = (cp.markovDensity != nullptr) ? cp.markovDensity->load (std::memory_order_relaxed) : 0.5f;
        p.poissonLambda      = (cp.poissonLambda != nullptr) ? cp.poissonLambda->load (std::memory_order_relaxed) : 2.0f;
        p.swing              = globalSwg;
        p.humanize           = globalHum;
        p.triggerProbability = (cp.probability != nullptr) ? cp.probability->load (std::memory_order_relaxed) : 1.0f;
        p.velocity           = (cp.velocity != nullptr) ? static_cast<int> (cp.velocity->load (std::memory_order_relaxed)) : 100;
        p.velocityRandom     = (cp.velocityRnd != nullptr) ? static_cast<int> (cp.velocityRnd->load (std::memory_order_relaxed)) : 0;
        p.gatePercent        = (cp.gate != nullptr) ? cp.gate->load (std::memory_order_relaxed) : 0.8f;
        p.scale              = (cp.scale != nullptr) ? static_cast<AlgorithmicRhythm::ScaleType> (static_cast<int> (cp.scale->load (std::memory_order_relaxed))) : AlgorithmicRhythm::ScaleType::NaturalMinor;
        p.pitchRandomRange   = (cp.pitchRnd != nullptr) ? static_cast<int> (cp.pitchRnd->load (std::memory_order_relaxed)) : 0;
        p.mutationRate       = globalMut;
    }
}

void MidiRythmGenProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    // Clear any audio channels if host allocated a dummy buffer
    buffer.clear();

    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0)
        return;

    auto* playHead = getPlayHead();
    if (playHead == nullptr)
    {
        if (wasPlaying)
        {
            stopAllActiveNotes (midiMessages, 0);
            wasPlaying = false;
        }
        return;
    }

    auto posOpt = playHead->getPosition();
    if (!posOpt.hasValue())
    {
        if (wasPlaying)
        {
            stopAllActiveNotes (midiMessages, 0);
            wasPlaying = false;
        }
        return;
    }

    const auto& pos = *posOpt;
    const bool isPlaying = pos.getIsPlaying();
    const double bpm = pos.getBpm().orFallback (120.0);
    const double ppqPos = pos.getPpqPosition().orFallback (0.0);
    const int64_t currentBlockStartSample = pos.getTimeInSamples().orFallback (sampleCounter);
    sampleCounter = currentBlockStartSample + numSamples;

    // Handle DAW Transport Stop
    if (wasPlaying && !isPlaying)
    {
        stopAllActiveNotes (midiMessages, 0);
        wasPlaying = false;
        lastPpqPosition = -1.0;
        return;
    }

    // Handle DAW Transport Loop jump or reverse seek
    if (isPlaying && lastPpqPosition >= 0.0)
    {
        if (ppqPos < lastPpqPosition || (ppqPos - lastPpqPosition) > 2.0)
        {
            stopAllActiveNotes (midiMessages, 0);
        }
    }
    wasPlaying = isPlaying;
    lastPpqPosition = ppqPos;

    // 1. Process active notes for Note-Off events scheduled in this block
    for (auto it = activeNotes.begin(); it != activeNotes.end(); )
    {
        if (it->offSampleGlobal <= currentBlockStartSample)
        {
            // Note-off is due right at the start of block
            midiMessages.addEvent (juce::MidiMessage::noteOff (it->midiChannel, it->midiNote, 0.0f), 0);
            it = activeNotes.erase (it);
        }
        else if (it->offSampleGlobal < currentBlockStartSample + numSamples)
        {
            // Note-off is due within this block
            const int sampleOffset = static_cast<int> (it->offSampleGlobal - currentBlockStartSample);
            midiMessages.addEvent (juce::MidiMessage::noteOff (it->midiChannel, it->midiNote, 0.0f), sampleOffset);
            it = activeNotes.erase (it);
        }
        else
        {
            ++it;
        }
    }

    if (!isPlaying)
        return;

    // 2. Calculate PPQ window for this audio block
    const double currentSampleRate = getSampleRate() > 1000.0 ? getSampleRate() : 44100.0;
    const double ppqDelta = (static_cast<double> (numSamples) * bpm) / (60.0 * currentSampleRate);
    const double ppqStart = ppqPos;
    const double ppqEnd   = ppqPos + ppqDelta;

    // 3. Extract parameters and run algorithmic engine
    AlgorithmicRhythm::LaneParameters lanes[AlgorithmicRhythm::kMaxLanes];
    readLaneParameters (lanes);

    scheduledNotesScratch.clear();
    rhythmEngine.processBlock (lanes,
                               ppqStart,
                               ppqEnd,
                               bpm,
                               numSamples,
                               currentBlockStartSample,
                               isPlaying,
                               scheduledNotesScratch);

    // 4. Dispatch scheduled notes into the output MidiBuffer
    for (const auto& note : scheduledNotesScratch)
    {
        const int noteSampleOffset = std::clamp (note.sampleOffset, 0, numSamples - 1);

        // Terminate any existing sounding note on the same channel & pitch to prevent overlapping note-ons
        for (auto it = activeNotes.begin(); it != activeNotes.end(); )
        {
            if (it->midiChannel == note.midiChannel && it->midiNote == note.midiNote)
            {
                const int offOffset = std::max (0, noteSampleOffset - 1);
                midiMessages.addEvent (juce::MidiMessage::noteOff (it->midiChannel, it->midiNote, 0.0f), offOffset);
                it = activeNotes.erase (it);
            }
            else
            {
                ++it;
            }
        }

        // Add Note-On event
        midiMessages.addEvent (juce::MidiMessage::noteOn (note.midiChannel,
                                                         note.midiNote,
                                                         static_cast<juce::uint8> (note.velocity)),
                               noteSampleOffset);

        // Register in active notes queue for sample-accurate Note-Off dispatch
        ActiveNote an;
        an.laneIndex = note.laneIndex;
        an.midiChannel = note.midiChannel;
        an.midiNote = note.midiNote;
        an.offSampleGlobal = currentBlockStartSample + noteSampleOffset + note.durationSamples;
        activeNotes.push_back (an);
    }
}

//==============================================================================
juce::AudioProcessorEditor* MidiRythmGenProcessor::createEditor()
{
    return new MidiRythmGenEditor (*this);
}

bool MidiRythmGenProcessor::hasEditor() const
{
    return true;
}

//==============================================================================
void MidiRythmGenProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void MidiRythmGenProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr)
    {
        if (xmlState->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
        }
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout MidiRythmGenProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Global parameters
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("global_mutation", 1),
        "Global Mutation",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("global_swing", 1),
        "Global Swing",
        juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("global_humanize", 1),
        "Global Humanize",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    // Scales array for choice parameter
    juce::StringArray scaleChoices;
    for (int s = 0; s < static_cast<int> (AlgorithmicRhythm::ScaleType::NumScales); ++s)
    {
        scaleChoices.add (AlgorithmicRhythm::ScaleQuantizer::getScaleName (static_cast<AlgorithmicRhythm::ScaleType> (s)));
    }

    juce::StringArray algoChoices = { "Euclidean", "Markov", "Poisson Burst" };

    // Default note presets for standard drum/percussion tracks (General MIDI standard mappings)
    static const int defaultNotes[AlgorithmicRhythm::kMaxLanes] = { 36, 38, 42, 46, 49, 51, 60, 64 };
    static const int defaultSteps[AlgorithmicRhythm::kMaxLanes] = { 16, 16, 16, 12, 8,  7,  11, 14 };
    static const int defaultPulses[AlgorithmicRhythm::kMaxLanes] = { 4,  2,  8,  5,  3,  3,  4,  5 };

    for (int i = 0; i < AlgorithmicRhythm::kMaxLanes; ++i)
    {
        const juce::String prefix = "lane_" + juce::String (i) + "_";
        const juce::String namePrefix = "Track " + juce::String (i + 1) + " ";

        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID (prefix + "enabled", 1),
            namePrefix + "Enabled",
            i < 4)); // First 4 lanes enabled by default

        params.push_back (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID (prefix + "root_note", 1),
            namePrefix + "Root Note",
            0, 127, defaultNotes[i]));

        params.push_back (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID (prefix + "midi_channel", 1),
            namePrefix + "MIDI Channel",
            1, 16, 1));

        params.push_back (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID (prefix + "steps", 1),
            namePrefix + "Steps (Polymetry)",
            1, AlgorithmicRhythm::kMaxSteps, defaultSteps[i]));

        params.push_back (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID (prefix + "clock_mult", 1),
            namePrefix + "Clock Mult",
            1, 16, 1));

        params.push_back (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID (prefix + "clock_div", 1),
            namePrefix + "Clock Div",
            1, 16, 1));

        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID (prefix + "algo", 1),
            namePrefix + "Algorithm",
            algoChoices, 0));

        params.push_back (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID (prefix + "pulses", 1),
            namePrefix + "Euclidean Pulses",
            0, AlgorithmicRhythm::kMaxSteps, defaultPulses[i]));

        params.push_back (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID (prefix + "rotation", 1),
            namePrefix + "Euclidean Rotation",
            0, AlgorithmicRhythm::kMaxSteps - 1, 0));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (prefix + "markov_density", 1),
            namePrefix + "Markov Density",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (prefix + "poisson_lambda", 1),
            namePrefix + "Poisson Lambda",
            juce::NormalisableRange<float> (0.1f, 10.0f, 0.1f), 2.0f));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (prefix + "probability", 1),
            namePrefix + "Probability",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));

        params.push_back (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID (prefix + "velocity", 1),
            namePrefix + "Velocity",
            1, 127, 100));

        params.push_back (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID (prefix + "velocity_rnd", 1),
            namePrefix + "Velocity Random",
            0, 64, 0));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (prefix + "gate", 1),
            namePrefix + "Gate Length",
            juce::NormalisableRange<float> (0.05f, 4.0f, 0.05f), 0.8f));

        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID (prefix + "scale", 1),
            namePrefix + "Quantize Scale",
            scaleChoices, static_cast<int> (AlgorithmicRhythm::ScaleType::NaturalMinor)));

        params.push_back (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID (prefix + "pitch_rnd", 1),
            namePrefix + "Pitch Random Range",
            0, 24, 0));
    }

    return { params.begin(), params.end() };
}

//==============================================================================
// Plugin entry point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MidiRythmGenProcessor();
}
