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

    AlgorithmicRhythm::LaneParameters lanes[AlgorithmicRhythm::kMaxLanes];
    readLaneParameters (lanes);
    rhythmEngine.updateOfflineTelemetry (lanes);
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
        cp.timeWarp       = apvts.getRawParameterValue (getParamId (i, "time_warp"));
        cp.pitchRnd       = apvts.getRawParameterValue (getParamId (i, "pitch_rnd"));
        cp.customMask     = apvts.getRawParameterValue (getParamId (i, "custom_mask"));
        cp.pan            = apvts.getRawParameterValue (getParamId (i, "pan"));
        cp.panDepth       = apvts.getRawParameterValue (getParamId (i, "pan_depth"));
        cp.panRate        = apvts.getRawParameterValue (getParamId (i, "pan_rate"));
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
        p.timeWarp           = (cp.timeWarp != nullptr) ? cp.timeWarp->load (std::memory_order_relaxed) : 0.0f;
        p.humanize           = globalHum;
        p.triggerProbability = (cp.probability != nullptr) ? cp.probability->load (std::memory_order_relaxed) : 1.0f;
        p.velocity           = (cp.velocity != nullptr) ? static_cast<int> (cp.velocity->load (std::memory_order_relaxed)) : 100;
        p.velocityRandom     = (cp.velocityRnd != nullptr) ? static_cast<int> (cp.velocityRnd->load (std::memory_order_relaxed)) : 0;
        p.gatePercent        = (cp.gate != nullptr) ? cp.gate->load (std::memory_order_relaxed) : 0.8f;
        p.pitchRandomRange   = (cp.pitchRnd != nullptr) ? static_cast<int> (cp.pitchRnd->load (std::memory_order_relaxed)) : 0;
        p.mutationRate       = globalMut;
        p.customPatternMask  = rhythmEngine.getCustomPatternMask (i);
        p.pan                = (cp.pan != nullptr) ? cp.pan->load (std::memory_order_relaxed) : 0.0f;
        p.panDepth           = (cp.panDepth != nullptr) ? cp.panDepth->load (std::memory_order_relaxed) : 0.0f;
        p.panRateMode        = (cp.panRate != nullptr) ? static_cast<int> (cp.panRate->load (std::memory_order_relaxed)) : 2;
    }
}

void MidiRythmGenProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    // Clear any audio channels if host allocated a dummy buffer
    buffer.clear();

    // 0. Process incoming MIDI Note-On events to update root note
    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
        {
            const int ch = msg.getChannel();
            // If channel matches 1..kMaxLanes, route to track (ch - 1), otherwise route to currently selected track
            int targetLane = selectedTrackIndex.load (std::memory_order_relaxed);
            if (ch >= 1 && ch <= AlgorithmicRhythm::kMaxLanes)
            {
                targetLane = ch - 1;
            }
            targetLane = std::clamp (targetLane, 0, AlgorithmicRhythm::kMaxLanes - 1);

            const int newRoot = msg.getNoteNumber();
            if (auto* rootParam = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter (getParamId (targetLane, "root_note"))))
            {
                *rootParam = newRoot;
            }
        }
    }
    midiMessages.clear();

    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0)
        return;

    AlgorithmicRhythm::LaneParameters lanes[AlgorithmicRhythm::kMaxLanes];
    readLaneParameters (lanes);

    auto* playHead = getPlayHead();
    bool hostIsPlaying = false;
    double bpm = 120.0;
    double ppqPos = 0.0;
    int64_t currentBlockStartSample = sampleCounter;

    if (playHead != nullptr)
    {
        if (auto posOpt = playHead->getPosition())
        {
            hostIsPlaying = posOpt->getIsPlaying();
            bpm = posOpt->getBpm().orFallback (120.0);
            if (hostIsPlaying)
            {
                ppqPos = posOpt->getPpqPosition().orFallback (0.0);
                currentBlockStartSample = posOpt->getTimeInSamples().orFallback (sampleCounter);
            }
        }
    }

    const bool internalActive = internalPlaybackActive.load (std::memory_order_relaxed);
    const bool isPlaying = hostIsPlaying || internalActive;

    const double currentSampleRate = getSampleRate() > 1000.0 ? getSampleRate() : 44100.0;
    const double ppqDelta = (static_cast<double> (numSamples) * bpm) / (60.0 * currentSampleRate);

    if (!hostIsPlaying && internalActive)
    {
        ppqPos = internalPpqPosition;
        internalPpqPosition = std::fmod (internalPpqPosition + ppqDelta, 64.0);
        currentBlockStartSample = sampleCounter;
    }

    sampleCounter = currentBlockStartSample + numSamples;
    currentPpqForGui.store (ppqPos, std::memory_order_relaxed);

    // Handle DAW / Internal Transport Stop
    if (wasPlaying && !isPlaying)
    {
        stopAllActiveNotes (midiMessages, 0);
        wasPlaying = false;
        lastPpqPosition = -1.0;
        internalPpqPosition = 0.0;
        rhythmEngine.reset();
        rhythmEngine.updateOfflineTelemetry (lanes);
        return;
    }

    if (!isPlaying)
    {
        rhythmEngine.updateOfflineTelemetry (lanes);
        return;
    }

    // Handle Transport Loop jump or reverse seek
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
    const double ppqStart = ppqPos;
    const double ppqEnd   = ppqPos + ppqDelta;

    // 3. Run algorithmic engine with current parameters
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

        // Add MIDI CC #10 Pan event immediately before Note-On
        midiMessages.addEvent (juce::MidiMessage::controllerEvent (note.midiChannel,
                                                                   10,
                                                                   note.pan),
                               noteSampleOffset);

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
    // 1. Sync custom masks directly into APVTS state properties
    for (int i = 0; i < AlgorithmicRhythm::kMaxLanes; ++i)
    {
        apvts.state.setProperty ("lane_custom_mask_" + juce::String (i),
                                 static_cast<juce::int64> (rhythmEngine.getCustomPatternMask (i)),
                                 nullptr);
    }

    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml != nullptr)
    {
        auto* patternsElement = xml->createNewChildElement ("CUSTOM_PATTERNS");
        for (int i = 0; i < AlgorithmicRhythm::kMaxLanes; ++i)
        {
            patternsElement->setAttribute ("lane_" + juce::String (i),
                                           juce::String::toHexString (static_cast<juce::int64> (rhythmEngine.getCustomPatternMask (i))));
        }
        copyXmlToBinary (*xml, destData);
    }
}

void MidiRythmGenProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr)
    {
        if (xmlState->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));

            // Layer 1: Dedicated CUSTOM_PATTERNS XML element
            if (auto* patternsElement = xmlState->getChildByName ("CUSTOM_PATTERNS"))
            {
                for (int i = 0; i < AlgorithmicRhythm::kMaxLanes; ++i)
                {
                    auto hexStr = patternsElement->getStringAttribute ("lane_" + juce::String (i));
                    if (hexStr.isNotEmpty())
                    {
                        uint32_t mask = static_cast<uint32_t> (hexStr.getHexValue64());
                        rhythmEngine.setCustomPatternMask (i, mask);
                    }
                }
            }

            // Layer 2: APVTS ValueTree properties (fallback & redundancy)
            for (int i = 0; i < AlgorithmicRhythm::kMaxLanes; ++i)
            {
                const juce::Identifier propId ("lane_custom_mask_" + juce::String (i));
                if (apvts.state.hasProperty (propId))
                {
                    uint32_t mask = static_cast<uint32_t> (static_cast<juce::int64> (apvts.state.getProperty (propId)));
                    rhythmEngine.setCustomPatternMask (i, mask);
                }
            }

            AlgorithmicRhythm::LaneParameters lanes[AlgorithmicRhythm::kMaxLanes];
            readLaneParameters (lanes);
            rhythmEngine.updateOfflineTelemetry (lanes);
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

    juce::StringArray algoChoices = { "Euclidean", "Markov", "Poisson Burst", "Custom Pattern" };

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

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (prefix + "time_warp", 1),
            namePrefix + "Time Warp",
            juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));

        params.push_back (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID (prefix + "pitch_rnd", 1),
            namePrefix + "Pitch Random Range",
            0, 24, 0));

        params.push_back (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID (prefix + "custom_mask", 1),
            namePrefix + "Custom Mask",
            std::numeric_limits<int>::min(),
            std::numeric_limits<int>::max(),
            0));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (prefix + "pan", 1),
            namePrefix + "Pan",
            juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (prefix + "pan_depth", 1),
            namePrefix + "Pan Depth",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));

        juce::StringArray panRateChoices = {
            "1/16", "1/8", "1/4", "1/2", "1 Bar", "2 Bars", "4 Bars", "8 Bars",
            "0.5 Hz", "1.0 Hz", "2.0 Hz", "4.0 Hz",
            "Random (Step S&H)", "Random (Smooth Walk)"
        };

        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID (prefix + "pan_rate", 1),
            namePrefix + "Pan Rate / Mode",
            panRateChoices, 2));
    }

    return { params.begin(), params.end() };
}

void MidiRythmGenProcessor::toggleLaneStep (int laneIdx, int stepIdx)
{
    if (laneIdx < 0 || laneIdx >= AlgorithmicRhythm::kMaxLanes || stepIdx < 0 || stepIdx >= AlgorithmicRhythm::kMaxSteps)
        return;

    auto* stepsParam = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter (getParamId (laneIdx, "steps")));
    const int steps = (stepsParam != nullptr) ? std::clamp (stepsParam->get(), 1, AlgorithmicRhythm::kMaxSteps) : 16;

    auto* rotParam = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter (getParamId (laneIdx, "rotation")));
    const int rotation = (rotParam != nullptr) ? rotParam->get() : 0;
    const int rot = ((rotation % steps) + steps) % steps;

    auto* algoParam = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (getParamId (laneIdx, "algo")));
    const int currentAlgo = (algoParam != nullptr) ? algoParam->getIndex() : 0;

    // Get current active pattern mask currently heard and displayed
    const uint32_t currentActive = rhythmEngine.getTelemetry (laneIdx).activePatternMask.load (std::memory_order_relaxed);

    // Toggle the exact visual step the user clicked
    const uint32_t newActive = currentActive ^ (1U << stepIdx);

    // In Custom mode, RhythmEngine applies rotatePattern(customPatternMask, steps, rot).
    // Therefore, customPatternMask must store newActive un-rotated by rot,
    // so that rotatePattern(customPatternMask, steps, rot) produces newActive with 100% fidelity!
    const uint32_t newCustomMask = AlgorithmicRhythm::RhythmEngine::rotatePattern (newActive, steps, steps - rot);

    // Store in authoritative 32-bit engine storage
    rhythmEngine.setCustomPatternMask (laneIdx, newCustomMask);

    // Switch algorithm to Custom (index 3) if not already Custom
    if (algoParam != nullptr && currentAlgo != 3)
        *algoParam = 3;

    // Persist into APVTS state immediately so it's always ready to save
    apvts.state.setProperty ("lane_custom_mask_" + juce::String (laneIdx),
                             static_cast<juce::int64> (newCustomMask),
                             nullptr);

    // Immediately update telemetry so UI updates without lag
    AlgorithmicRhythm::LaneParameters lanes[AlgorithmicRhythm::kMaxLanes];
    readLaneParameters (lanes);
    rhythmEngine.updateOfflineTelemetry (lanes);
}

uint32_t MidiRythmGenProcessor::getLanePattern (int laneIdx) const
{
    if (laneIdx < 0 || laneIdx >= AlgorithmicRhythm::kMaxLanes)
        return 0;
    return rhythmEngine.getTelemetry (laneIdx).activePatternMask.load (std::memory_order_relaxed);
}

void MidiRythmGenProcessor::setInternalPlayback (bool play) noexcept
{
    internalPlaybackActive.store (play, std::memory_order_relaxed);
    if (!play)
    {
        internalPpqPosition = 0.0;
    }
}

void MidiRythmGenProcessor::randomizeWholeTonePattern()
{
    juce::Random rng (juce::Time::currentTimeMillis());

    // Step lengths suited for polymetric interplay
    static const int musicalSteps[] = { 16, 12, 8, 14, 16, 10, 24, 18, 15, 7, 11 };
    const int numStepOptions = static_cast<int> (sizeof (musicalSteps) / sizeof (musicalSteps[0]));

    for (int i = 0; i < AlgorithmicRhythm::kMaxLanes; ++i)
    {
        // 1. Root Note: Starts from Middle C (60) and ascends by whole tones (+2 semitones per track)
        const int wholeToneNote = 60 + i * 2; // 60, 62, 64, 66, 68, 70, 72, 74

        // 2. Polymetric step size and algorithm
        const int chosenSteps = musicalSteps[rng.nextInt (numStepOptions)];
        const int chosenAlgo  = (rng.nextFloat() < 0.70f) ? 0 : ((rng.nextFloat() < 0.60f) ? 1 : 2); // Mostly Euclidean, then Markov/Poisson

        // 3. Euclidean pulses & rotation
        int maxPulses = std::max (1, chosenSteps - 1);
        int chosenPulses = rng.nextInt (juce::Range<int> (std::max (1, chosenSteps / 4), std::max (2, (chosenSteps * 3) / 4)));
        chosenPulses = std::clamp (chosenPulses, 1, maxPulses);
        const int chosenRotation = rng.nextInt (chosenSteps);

        // 4. Markov / Poisson stochastic parameters
        const float chosenDensity = 0.35f + rng.nextFloat() * 0.45f;
        const float chosenLambda  = 1.5f  + rng.nextFloat() * 3.0f;

        // 5. Dynamics & Clock
        const int chosenMult = (i >= 6 && rng.nextFloat() < 0.35f) ? 2 : 1;
        const int chosenVel = rng.nextInt (juce::Range<int> (85, 118));
        const int chosenVelRnd = rng.nextInt (juce::Range<int> (8, 22));
        const float chosenGate = 0.45f + rng.nextFloat() * 0.50f;
        const float chosenProb = 0.85f + rng.nextFloat() * 0.15f;

        // Apply to APVTS parameters
        if (auto* p = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (getParamId (i, "enabled"))))
            *p = true; // All 8 tracks active

        if (auto* p = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter (getParamId (i, "root_note"))))
            *p = wholeToneNote;

        if (auto* p = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter (getParamId (i, "steps"))))
            *p = chosenSteps;

        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (getParamId (i, "algo"))))
            *p = chosenAlgo;

        if (auto* p = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter (getParamId (i, "pulses"))))
            *p = chosenPulses;

        if (auto* p = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter (getParamId (i, "rotation"))))
            *p = chosenRotation;

        if (auto* p = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (getParamId (i, "markov_density"))))
            *p = chosenDensity;

        if (auto* p = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (getParamId (i, "poisson_lambda"))))
            *p = chosenLambda;

        if (auto* p = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter (getParamId (i, "clock_mult"))))
            *p = chosenMult;

        if (auto* p = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter (getParamId (i, "clock_div"))))
            *p = 1;

        if (auto* p = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter (getParamId (i, "velocity"))))
            *p = chosenVel;

        if (auto* p = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter (getParamId (i, "velocity_rnd"))))
            *p = chosenVelRnd;

        if (auto* p = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (getParamId (i, "gate"))))
            *p = chosenGate;

        if (auto* p = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (getParamId (i, "probability"))))
            *p = chosenProb;
    }

    // Refresh telemetry immediately
    AlgorithmicRhythm::LaneParameters lanes[AlgorithmicRhythm::kMaxLanes];
    readLaneParameters (lanes);
    rhythmEngine.updateOfflineTelemetry (lanes);
}

void MidiRythmGenProcessor::exportPatternToMidiFile (const juce::File& targetFile, int numBars)
{
    juce::MidiFile midiFile;
    const short timeFormat = 960; // 960 ticks per quarter note
    midiFile.setTicksPerQuarterNote (timeFormat);

    AlgorithmicRhythm::LaneParameters lanes[AlgorithmicRhythm::kMaxLanes];
    readLaneParameters (lanes);

    // Track 0: Tempo and Time Signature
    juce::MidiMessageSequence tempoTrack;
    auto timeSig = juce::MidiMessage::timeSignatureMetaEvent (4, 4);
    timeSig.setTimeStamp (0.0);
    tempoTrack.addEvent (timeSig);

    double hostBpm = 120.0;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
            hostBpm = pos->getBpm().orFallback (120.0);
    }
    auto tempoMsg = juce::MidiMessage::tempoMetaEvent (static_cast<int> (std::round (60000000.0 / hostBpm)));
    tempoMsg.setTimeStamp (0.0);
    tempoTrack.addEvent (tempoMsg);

    auto endMeta = juce::MidiMessage::endOfTrack();
    endMeta.setTimeStamp (numBars * 4.0 * timeFormat);
    tempoTrack.addEvent (endMeta);
    midiFile.addTrack (tempoTrack);

    // Track 1: All multi-track notes
    juce::MidiMessageSequence noteSequence;
    const double totalPpq = numBars * 4.0;
    const double ticksPerPpq = static_cast<double> (timeFormat);

    for (int laneIdx = 0; laneIdx < AlgorithmicRhythm::kMaxLanes; ++laneIdx)
    {
        const auto& p = lanes[laneIdx];
        if (!p.enabled || p.steps <= 0)
            continue;

        const int steps = std::clamp (p.steps, 1, AlgorithmicRhythm::kMaxSteps);
        const int mult  = std::clamp (p.clockMultiplier, 1, 16);
        const int div   = std::clamp (p.clockDivider, 1, 16);
        const double ppqPerStep = (0.25 * static_cast<double> (div)) / static_cast<double> (mult);
        if (ppqPerStep <= 1e-6)
            continue;

        const double loopPpqDuration = steps * ppqPerStep;
        const uint32_t pattern = getLanePattern (laneIdx);
        const int totalStepsInExport = static_cast<int> (std::ceil (totalPpq / ppqPerStep));

        for (int s = 0; s < totalStepsInExport; ++s)
        {
            const int stepInPattern = s % steps;
            const int cycle = s / steps;
            const bool isHit = (pattern & (1U << stepInPattern)) != 0;
            if (!isHit)
                continue;

            const float swingShift = (stepInPattern % 2 != 0) ? (p.swing * 0.5f) : 0.0f;
            const double t0 = std::clamp (static_cast<double> (static_cast<float> (stepInPattern) + swingShift) / static_cast<double> (steps), 0.0, 0.999999);
            const double gamma = std::pow (2.0, static_cast<double> (p.timeWarp * 1.5f));
            const double tWarped = (std::abs (p.timeWarp) > 0.001f) ? std::clamp (std::pow (t0, gamma), 0.0, 0.999999) : t0;

            const double noteStartPpq = (static_cast<double> (cycle) + tWarped) * loopPpqDuration;
            if (noteStartPpq >= totalPpq)
                continue;

            const double gatePpq = ppqPerStep * std::clamp (p.gatePercent, 0.1f, 4.0f);
            const double noteEndPpq = std::min (totalPpq, noteStartPpq + gatePpq);

            const double startTick = noteStartPpq * ticksPerPpq;
            const double endTick   = noteEndPpq * ticksPerPpq;

            float noteLfo = 0.0f;
            if (p.panRateMode <= 7)
            {
                static constexpr double kPpqDivs[] = { 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0 };
                const double cyclePpq = kPpqDivs[p.panRateMode];
                double phase = std::fmod (noteStartPpq / cyclePpq, 1.0);
                if (phase < 0.0) phase += 1.0;
                noteLfo = std::sin (static_cast<float> (phase * 6.283185307179586));
            }
            else if (p.panRateMode >= 8 && p.panRateMode <= 11)
            {
                static constexpr float kFreqs[] = { 0.5f, 1.0f, 2.0f, 4.0f };
                const float f = kFreqs[p.panRateMode - 8];
                const double timeSec = (noteStartPpq / hostBpm) * 60.0;
                double phase = std::fmod (timeSec * static_cast<double> (f), 1.0);
                if (phase < 0.0) phase += 1.0;
                noteLfo = std::sin (static_cast<float> (phase * 6.283185307179586));
            }
            else
            {
                const float r = std::sin (static_cast<float> (s * 12.9898 + laneIdx * 78.233)) * 43758.5453f;
                noteLfo = (r - std::floor (r)) * 2.0f - 1.0f;
            }

            const float bipolarPan = std::clamp (p.pan + noteLfo * p.panDepth, -1.0f, 1.0f);
            const int midiPan = std::clamp (static_cast<int> (std::round ((bipolarPan + 1.0f) * 63.5f)), 0, 127);

            auto panMsg = juce::MidiMessage::controllerEvent (p.midiChannel, 10, midiPan);
            panMsg.setTimeStamp (startTick);
            noteSequence.addEvent (panMsg);

            auto noteOn = juce::MidiMessage::noteOn (p.midiChannel, p.rootNote, static_cast<juce::uint8> (std::clamp (p.velocity, 1, 127)));
            noteOn.setTimeStamp (startTick);
            noteSequence.addEvent (noteOn);

            auto noteOff = juce::MidiMessage::noteOff (p.midiChannel, p.rootNote, 0.0f);
            noteOff.setTimeStamp (endTick);
            noteSequence.addEvent (noteOff);
        }
    }

    noteSequence.updateMatchedPairs();
    auto endMeta2 = juce::MidiMessage::endOfTrack();
    endMeta2.setTimeStamp (numBars * 4.0 * timeFormat);
    noteSequence.addEvent (endMeta2);
    midiFile.addTrack (noteSequence);

    if (targetFile.existsAsFile())
        targetFile.deleteFile();

    juce::FileOutputStream outStream (targetFile);
    if (outStream.openedOk())
    {
        midiFile.writeTo (outStream);
    }
}

//==============================================================================
// Plugin entry point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MidiRythmGenProcessor();
}
