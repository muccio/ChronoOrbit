#include "RhythmEngine.h"
#include <cmath>
#include <algorithm>

namespace AlgorithmicRhythm
{

//==============================================================================
// ScaleQuantizer Implementation
//==============================================================================

static const int kScaleNotesMajor[]          = { 0, 2, 4, 5, 7, 9, 11 };
static const int kScaleNotesNaturalMinor[]   = { 0, 2, 3, 5, 7, 8, 10 };
static const int kScaleNotesHarmonicMinor[]  = { 0, 2, 3, 5, 7, 8, 11 };
static const int kScaleNotesDorian[]         = { 0, 2, 3, 5, 7, 9, 10 };
static const int kScaleNotesPhrygian[]       = { 0, 1, 3, 5, 7, 8, 10 };
static const int kScaleNotesLydian[]         = { 0, 2, 4, 6, 7, 9, 11 };
static const int kScaleNotesMixolydian[]     = { 0, 2, 4, 5, 7, 9, 10 };
static const int kScaleNotesMinPentatonic[]  = { 0, 3, 5, 7, 10 };
static const int kScaleNotesMajPentatonic[]  = { 0, 2, 4, 7, 9 };
static const int kScaleNotesHirajoshi[]      = { 0, 2, 3, 7, 8 };
static const int kScaleNotesInsen[]          = { 0, 1, 5, 7, 10 };
static const int kScaleNotesWholeTone[]      = { 0, 2, 4, 6, 8, 10 };

struct ScaleDefinition
{
    const char* name;
    const int* intervals;
    int size;
};

static const ScaleDefinition kScaleDefs[] =
{
    { "Chromatic",       nullptr,                    12 },
    { "Major",           kScaleNotesMajor,           7 },
    { "Natural Minor",   kScaleNotesNaturalMinor,    7 },
    { "Harmonic Minor",  kScaleNotesHarmonicMinor,   7 },
    { "Dorian",          kScaleNotesDorian,          7 },
    { "Phrygian",        kScaleNotesPhrygian,        7 },
    { "Lydian",          kScaleNotesLydian,          7 },
    { "Mixolydian",      kScaleNotesMixolydian,      7 },
    { "Minor Pentatonic",kScaleNotesMinPentatonic,   5 },
    { "Major Pentatonic",kScaleNotesMajPentatonic,   5 },
    { "Hirajoshi",       kScaleNotesHirajoshi,       5 },
    { "Insen",           kScaleNotesInsen,           5 },
    { "Whole Tone",      kScaleNotesWholeTone,       6 }
};

int ScaleQuantizer::quantizePitch(int rootNote, int scaleDegreeOffset, ScaleType scale) noexcept
{
    const int scaleIdx = std::clamp(static_cast<int>(scale), 0, static_cast<int>(ScaleType::NumScales) - 1);
    const auto& def = kScaleDefs[scaleIdx];

    if (def.intervals == nullptr || def.size == 12)
    {
        return std::clamp(rootNote + scaleDegreeOffset, 0, 127);
    }

    int octave = scaleDegreeOffset / def.size;
    int degree = scaleDegreeOffset % def.size;

    if (degree < 0)
    {
        degree += def.size;
        octave -= 1;
    }

    int interval = def.intervals[degree];
    int midiPitch = rootNote + octave * 12 + interval;
    return std::clamp(midiPitch, 0, 127);
}

const char* ScaleQuantizer::getScaleName(ScaleType scale) noexcept
{
    const int scaleIdx = std::clamp(static_cast<int>(scale), 0, static_cast<int>(ScaleType::NumScales) - 1);
    return kScaleDefs[scaleIdx].name;
}

//==============================================================================
// RhythmEngine Implementation
//==============================================================================

RhythmEngine::RhythmEngine()
{
    reset();
}

void RhythmEngine::prepare(double sampleRate)
{
    currentSampleRate = (sampleRate > 1000.0) ? sampleRate : 44100.0;
    reset();
}

void RhythmEngine::reset()
{
    for (size_t i = 0; i < kMaxLanes; ++i)
    {
        laneStates[i].lastEvaluatedPpq = -1.0;
        laneStates[i].lastStepIndex = -1;
        laneStates[i].markovState = 0;
        laneStates[i].cachedPattern = 0;
        laneStates[i].lastBarCount = -1;

        telemetry[i].currentStep.store(0, std::memory_order_relaxed);
        telemetry[i].totalSteps.store(16, std::memory_order_relaxed);
        telemetry[i].playheadNorm.store(0.0f, std::memory_order_relaxed);
        telemetry[i].justTriggered.store(false, std::memory_order_relaxed);
        telemetry[i].lastVelocity.store(0, std::memory_order_relaxed);
        telemetry[i].activePatternMask.store(0, std::memory_order_relaxed);
    }
}

uint32_t RhythmEngine::generateEuclideanPattern(int steps, int pulses, int rotation) noexcept
{
    steps = std::clamp(steps, 1, kMaxSteps);
    pulses = std::clamp(pulses, 0, steps);

    if (pulses == 0) return 0;
    if (pulses >= steps) return (steps == 32) ? 0xFFFFFFFFU : ((1U << steps) - 1U);

    uint32_t rawPattern = 0;
    for (int i = 0; i < steps; ++i)
    {
        // Bresenham uniform distribution
        if (((i * pulses) % steps) < pulses)
        {
            rawPattern |= (1U << i);
        }
    }

    // Apply phase rotation
    int rot = ((rotation % steps) + steps) % steps;
    if (rot == 0) return rawPattern;

    uint32_t rotated = 0;
    for (int i = 0; i < steps; ++i)
    {
        int srcIdx = (i - rot + steps) % steps;
        if ((rawPattern & (1U << srcIdx)) != 0)
        {
            rotated |= (1U << i);
        }
    }

    return rotated;
}

uint32_t RhythmEngine::mutatePattern(uint32_t currentPattern, int steps, float mutationRate, FastRandom& rng) noexcept
{
    steps = std::clamp(steps, 1, kMaxSteps);
    if (mutationRate <= 0.001f) return currentPattern;

    uint32_t mutated = currentPattern;
    for (int i = 0; i < steps; ++i)
    {
        if (rng.nextFloat() < mutationRate)
        {
            // Flip the hit bit or shift bit
            mutated ^= (1U << i);
        }
    }
    return mutated;
}

bool RhythmEngine::evaluateMarkovHit(int& currentState, float density, FastRandom& localRng) noexcept
{
    // 4 States: 0=Rest, 1=Hit, 2=Ghost, 3=Accent
    // Density (0..1) biases towards hits & accents
    float pRestToHit = std::clamp(density * 0.9f + 0.1f, 0.05f, 0.95f);
    float pHitToHit  = std::clamp(density * 0.7f + 0.1f, 0.05f, 0.90f);

    float r = localRng.nextFloat();

    switch (currentState)
    {
        case 0: // Rest
            if (r < pRestToHit * 0.6f)       currentState = 1; // Hit
            else if (r < pRestToHit * 0.85f) currentState = 2; // Ghost
            else if (r < pRestToHit)          currentState = 3; // Accent
            else                              currentState = 0; // Remain Rest
            break;

        case 1: // Hit
            if (r < (1.0f - pHitToHit))      currentState = 0; // Fall to Rest
            else if (r < 0.7f)               currentState = 1; // Repeat Hit
            else if (r < 0.85f)              currentState = 2; // Ghost
            else                             currentState = 3; // Accent
            break;

        case 2: // Ghost
            if (r < 0.5f)                    currentState = 0; // Rest
            else if (r < 0.85f)              currentState = 1; // Hit
            else                             currentState = 3; // Accent
            break;

        case 3: // Accent
            if (r < 0.6f)                    currentState = 0; // Rest after accent
            else if (r < 0.9f)               currentState = 1; // Hit
            else                             currentState = 2; // Ghost
            break;

        default:
            currentState = 0;
            break;
    }

    return (currentState != 0);
}

bool RhythmEngine::evaluatePoissonHit(float lambda, FastRandom& localRng) noexcept
{
    // Truncated Poisson approximation for discrete per-step arrival
    // P(X > 0) = 1 - exp(-lambda_norm)
    float lNorm = std::clamp(lambda * 0.25f, 0.05f, 2.5f);
    float pTrigger = 1.0f - std::exp(-lNorm);
    return localRng.nextFloat() < pTrigger;
}

void RhythmEngine::processBlock(const LaneParameters lanes[kMaxLanes],
                                double ppqStart,
                                double ppqEnd,
                                double bpm,
                                int numSamples,
                                int64_t currentBlockStartSample,
                                bool isPlaying,
                                std::vector<ScheduledNote>& outScheduledNotes)
{
    juce::ignoreUnused(currentBlockStartSample);

    if (!isPlaying || bpm <= 1.0 || numSamples <= 0 || ppqEnd <= ppqStart)
    {
        return;
    }

    const double samplesPerQuarter = (60.0 / bpm) * currentSampleRate;
    const double ppqSpan = ppqEnd - ppqStart;

    // Detect bar boundaries (every 4 quarter notes) for stochastic mutations
    const int currentBarCount = static_cast<int>(std::floor(ppqStart / 4.0));

    for (int laneIdx = 0; laneIdx < kMaxLanes; ++laneIdx)
    {
        const auto& params = lanes[laneIdx];
        if (!params.enabled || params.steps <= 0)
            continue;

        auto& state = laneStates[static_cast<size_t>(laneIdx)];
        auto& tele = telemetry[static_cast<size_t>(laneIdx)];

        const int steps = std::clamp(params.steps, 1, kMaxSteps);
        tele.totalSteps.store(steps, std::memory_order_relaxed);

        // Polyrhythm clock ratio: step size in quarter notes
        // Base is 16th note (0.25 ppq)
        const int mult = std::clamp(params.clockMultiplier, 1, 16);
        const int div  = std::clamp(params.clockDivider, 1, 16);
        const double ppqPerStep = (0.25 * static_cast<double>(div)) / static_cast<double>(mult);

        if (ppqPerStep <= 1e-6)
            continue;

        // Total pattern loop duration in quarter notes (Polymetry)
        const double loopPpqDuration = steps * ppqPerStep;

        // Check if bar boundary crossed for mutations
        if (state.lastBarCount != currentBarCount)
        {
            state.lastBarCount = currentBarCount;
            if (params.mutationRate > 0.001f)
            {
                state.cachedPattern = mutatePattern(state.cachedPattern, steps, params.mutationRate, rng);
            }
        }

        // Recompute base pattern if not cached or parameters changed
        uint32_t activePattern = 0;
        if (params.algorithm == AlgorithmMode::Euclidean)
        {
            activePattern = generateEuclideanPattern(steps, params.euclideanPulses, params.euclideanRotation);
            if (params.mutationRate > 0.001f && state.cachedPattern != 0)
            {
                activePattern = state.cachedPattern;
            }
            else
            {
                state.cachedPattern = activePattern;
            }
        }
        else
        {
            activePattern = state.cachedPattern;
        }
        tele.activePatternMask.store(activePattern, std::memory_order_relaxed);

        // Calculate step bounds intersecting this block
        // Find first integer step index k such that k * ppqPerStep >= ppqStart
        const int64_t kStart = static_cast<int64_t>(std::floor(ppqStart / ppqPerStep));
        const int64_t kEnd   = static_cast<int64_t>(std::ceil(ppqEnd / ppqPerStep));

        for (int64_t k = kStart; k <= kEnd; ++k)
        {
            const double nominalStepPpq = static_cast<double> (k) * ppqPerStep;

            // Non-linear microtiming: swing on odd steps + humanize jitter
            const int stepInPattern = static_cast<int>(((k % steps) + steps) % steps);
            double microtimingPpq = 0.0;

            if ((stepInPattern % 2) != 0 && std::abs(params.swing) > 0.001f)
            {
                // Asymmetric swing shifts odd steps forward or backward
                microtimingPpq += ppqPerStep * (params.swing * 0.333);
            }

            if (params.humanize > 0.001f)
            {
                // Micro-jitter deviation
                microtimingPpq += ppqPerStep * (params.humanize * 0.15f * rng.nextBipolar());
            }

            const double effectiveStepPpq = nominalStepPpq + microtimingPpq;

            // Check if this step trigger point falls within the current audio block
            if (effectiveStepPpq >= ppqStart && effectiveStepPpq < ppqEnd)
            {
                // Prevent duplicate trigger of the exact same step index in this block
                if (k == state.lastStepIndex)
                    continue;

                state.lastStepIndex = static_cast<int>(k);

                // Determine whether this step produces a hit
                bool isHit = false;
                int currentVelocity = params.velocity;

                if (params.algorithm == AlgorithmMode::Euclidean)
                {
                    isHit = (activePattern & (1U << stepInPattern)) != 0;
                }
                else if (params.algorithm == AlgorithmMode::Markov)
                {
                    isHit = evaluateMarkovHit(state.markovState, params.markovDensity, rng);
                    if (state.markovState == 2) // Ghost note
                        currentVelocity = std::max(20, currentVelocity / 2);
                    else if (state.markovState == 3) // Accent
                        currentVelocity = std::min(127, static_cast<int>(static_cast<float>(currentVelocity) * 1.3f));
                }
                else if (params.algorithm == AlgorithmMode::PoissonBurst)
                {
                    isHit = evaluatePoissonHit(params.poissonLambda, rng);
                }

                // Stochastic trigger probability filter (0..100%)
                if (isHit && rng.nextFloat() <= params.triggerProbability)
                {
                    // Compute sample offset inside the current audio block
                    const double relativeBlockFraction = (effectiveStepPpq - ppqStart) / ppqSpan;
                    const int sampleOffset = std::clamp(
                        static_cast<int>(std::round(relativeBlockFraction * numSamples)),
                        0,
                        numSamples - 1);

                    // Velocity randomization
                    if (params.velocityRandom > 0)
                    {
                        const int vSpread = static_cast<int>(rng.nextBipolar() * static_cast<float>(params.velocityRandom));
                        currentVelocity = std::clamp(currentVelocity + vSpread, 1, 127);
                    }

                    // Pitch quantization with musical scale & stochastic pitch offset
                    int degreeOffset = 0;
                    if (params.pitchRandomRange > 0)
                    {
                        degreeOffset = rng.nextRange(-params.pitchRandomRange, params.pitchRandomRange);
                    }
                    const int midiNote = ScaleQuantizer::quantizePitch(params.rootNote, degreeOffset, params.scale);

                    // Gate length calculation (in samples)
                    const double stepSamples = ppqPerStep * samplesPerQuarter;
                    float effectiveGateRatio = params.gatePercent;
                    if (params.gateRandom > 0.001f)
                    {
                        effectiveGateRatio += params.gateRandom * rng.nextBipolar() * 0.5f;
                        effectiveGateRatio = std::max(0.05f, effectiveGateRatio);
                    }
                    const int durationSamples = std::max(64, static_cast<int>(effectiveGateRatio * stepSamples));

                    // Schedule note
                    ScheduledNote note;
                    note.laneIndex = laneIdx;
                    note.sampleOffset = sampleOffset;
                    note.midiChannel = std::clamp(params.midiChannel, 1, 16);
                    note.midiNote = midiNote;
                    note.velocity = currentVelocity;
                    note.durationSamples = durationSamples;
                    outScheduledNotes.push_back(note);

                    // Telemetry update
                    tele.justTriggered.store(true, std::memory_order_relaxed);
                    tele.lastVelocity.store(static_cast<uint8_t>(currentVelocity), std::memory_order_relaxed);
                }

                tele.currentStep.store(stepInPattern, std::memory_order_relaxed);
            }
        }

        // Update continuous normalized playhead within pattern loop for orbit GUI
        double normPhase = std::fmod(ppqStart, loopPpqDuration);
        if (normPhase < 0.0) normPhase += loopPpqDuration;
        tele.playheadNorm.store(static_cast<float>(normPhase / loopPpqDuration), std::memory_order_relaxed);
    }
}

} // namespace AlgorithmicRhythm
