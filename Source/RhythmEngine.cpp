#include "RhythmEngine.h"
#include <cmath>
#include <algorithm>

namespace AlgorithmicRhythm
{

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
        laneStates[i].lastBarCount = -1;

        telemetry[i].currentStep.store(0, std::memory_order_relaxed);
        telemetry[i].playheadNorm.store(0.0f, std::memory_order_relaxed);
        telemetry[i].justTriggered.store(false, std::memory_order_relaxed);
        telemetry[i].lastVelocity.store(0, std::memory_order_relaxed);
        // Note: activePatternMask and customPatternMasks are persistent configuration data
        // and must NOT be cleared on playback stop.
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

void RhythmEngine::updateOfflineTelemetry(const LaneParameters lanes[kMaxLanes]) noexcept
{
    for (int laneIdx = 0; laneIdx < kMaxLanes; ++laneIdx)
    {
        const auto& params = lanes[laneIdx];
        auto& tele = telemetry[static_cast<size_t>(laneIdx)];
        auto& state = laneStates[static_cast<size_t>(laneIdx)];
        const int steps = std::clamp(params.steps, 1, kMaxSteps);

        tele.totalSteps.store(steps, std::memory_order_relaxed);
        tele.swingValue.store(params.swing, std::memory_order_relaxed);
        tele.timeWarpValue.store(params.timeWarp, std::memory_order_relaxed);

        const uint32_t validMask = (steps == 32) ? 0xFFFFFFFFU : ((1U << steps) - 1U);
        uint32_t activePattern = 0;

        if (params.algorithm == AlgorithmMode::Euclidean)
        {
            activePattern = generateEuclideanPattern(steps, params.euclideanPulses, params.euclideanRotation);
            state.cachedPattern = activePattern;
        }
        else if (params.algorithm == AlgorithmMode::Custom)
        {
            const uint32_t rawMask = customPatternMasks[static_cast<size_t>(laneIdx)].load(std::memory_order_relaxed) & validMask;
            activePattern = rotatePattern(rawMask, steps, params.euclideanRotation);
            state.cachedPattern = activePattern;
        }
        else
        {
            if (state.cachedPattern == 0)
            {
                if (params.algorithm == AlgorithmMode::Markov)
                {
                    int mState = 0;
                    for (int s = 0; s < steps; ++s)
                    {
                        if (evaluateMarkovHit(mState, params.markovDensity, rng))
                            activePattern |= (1U << s);
                    }
                }
                else if (params.algorithm == AlgorithmMode::PoissonBurst)
                {
                    for (int s = 0; s < steps; ++s)
                    {
                        if (evaluatePoissonHit(params.poissonLambda, rng))
                            activePattern |= (1U << s);
                    }
                }
                activePattern = rotatePattern(activePattern, steps, params.euclideanRotation);
                state.cachedPattern = activePattern;
            }
            else
            {
                activePattern = rotatePattern(state.cachedPattern & validMask, steps, params.euclideanRotation);
            }
        }
        tele.activePatternMask.store(activePattern, std::memory_order_relaxed);
    }
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
        tele.swingValue.store(params.swing, std::memory_order_relaxed);
        tele.timeWarpValue.store(params.timeWarp, std::memory_order_relaxed);

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
        const uint32_t validMask = (steps == 32) ? 0xFFFFFFFFU : ((1U << steps) - 1U);
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
        else if (params.algorithm == AlgorithmMode::Custom)
        {
            const uint32_t rawMask = customPatternMasks[static_cast<size_t>(laneIdx)].load(std::memory_order_relaxed) & validMask;
            activePattern = rotatePattern(rawMask, steps, params.euclideanRotation);
            state.cachedPattern = activePattern;
        }
        else
        {
            if (state.cachedPattern == 0)
            {
                if (params.algorithm == AlgorithmMode::Markov)
                {
                    int mState = 0;
                    for (int s = 0; s < steps; ++s)
                    {
                        if (evaluateMarkovHit(mState, params.markovDensity, rng))
                            activePattern |= (1U << s);
                    }
                }
                else if (params.algorithm == AlgorithmMode::PoissonBurst)
                {
                    for (int s = 0; s < steps; ++s)
                    {
                        if (evaluatePoissonHit(params.poissonLambda, rng))
                            activePattern |= (1U << s);
                    }
                }
                activePattern = rotatePattern(activePattern, steps, params.euclideanRotation);
                state.cachedPattern = activePattern;
            }
            else
            {
                activePattern = rotatePattern(state.cachedPattern & validMask, steps, params.euclideanRotation);
            }
        }
        tele.activePatternMask.store(activePattern, std::memory_order_relaxed);

        // Calculate step bounds intersecting this block with safety margins for swing & time warp
        const int64_t kStart = static_cast<int64_t>(std::floor(ppqStart / ppqPerStep)) - 2;
        const int64_t kEnd   = static_cast<int64_t>(std::ceil(ppqEnd / ppqPerStep)) + 2;

        for (int64_t k = kStart; k <= kEnd; ++k)
        {
            const int64_t cycleIndex = (k >= 0) ? (k / steps) : ((k - steps + 1) / steps);
            const int stepInPattern = static_cast<int>(k - cycleIndex * steps);

            // Swing microtiming
            const float swingShift = (stepInPattern % 2 != 0) ? (params.swing * 0.5f) : 0.0f;
            const double t0 = std::clamp(static_cast<double>(stepInPattern + swingShift) / static_cast<double>(steps), 0.0, 0.999999);

            // Time warp geometric non-linear curve: tau(t) = t0^gamma
            const double gamma = std::pow(2.0, static_cast<double>(params.timeWarp * 1.5f));
            const double tWarped = (std::abs(params.timeWarp) > 0.001f) ? std::clamp(std::pow(t0, gamma), 0.0, 0.999999) : t0;

            double effectiveStepPpq = (static_cast<double>(cycleIndex) + tWarped) * loopPpqDuration;

            if (params.humanize > 0.001f)
            {
                // Micro-jitter deviation
                effectiveStepPpq += ppqPerStep * (params.humanize * 0.15f * rng.nextBipolar());
            }

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

                if (params.algorithm == AlgorithmMode::Euclidean || params.algorithm == AlgorithmMode::Custom)
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

                    // Stochastic pitch offset
                    int pitchOffset = 0;
                    if (params.pitchRandomRange > 0)
                    {
                        pitchOffset = rng.nextRange(-params.pitchRandomRange, params.pitchRandomRange);
                    }
                    const int midiNote = std::clamp(params.rootNote + pitchOffset, 0, 127);

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
