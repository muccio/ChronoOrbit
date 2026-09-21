#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>
#include <atomic>
#include <cstdint>
#include <random>

namespace AlgorithmicRhythm
{

static constexpr int kMaxLanes = 8;
static constexpr int kMaxSteps = 32;

enum class AlgorithmMode : int
{
    Euclidean = 0,
    Markov = 1,
    PoissonBurst = 2,
    Custom = 3
};

// Fast non-allocating Xorshift32 PRNG suitable for real-time audio threads
class FastRandom
{
public:
    explicit FastRandom(uint32_t seed = 881726454U) : state(seed == 0 ? 1U : seed) {}

    inline uint32_t nextInt() noexcept
    {
        uint32_t x = state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        state = x;
        return x;
    }

    inline float nextFloat() noexcept // [0.0f, 1.0f)
    {
        return static_cast<float>(nextInt()) / 4294967296.0f;
    }

    inline float nextBipolar() noexcept // [-1.0f, 1.0f)
    {
        return nextFloat() * 2.0f - 1.0f;
    }

    inline int nextRange(int minVal, int maxVal) noexcept
    {
        if (minVal >= maxVal) return minVal;
        return minVal + static_cast<int>(nextInt() % static_cast<uint32_t>(maxVal - minVal + 1));
    }

private:
    uint32_t state;
};

// Parameters for one rhythmic lane
struct LaneParameters
{
    bool enabled { true };
    int rootNote { 36 };             // MIDI Note (e.g. 36 = C1 / Kick)
    int midiChannel { 1 };           // 1-16
    int steps { 16 };                // Polymetric steps: 1 to 32
    int clockMultiplier { 1 };       // Polyrhythmic multiplier (1..16)
    int clockDivider { 1 };          // Polyrhythmic divider (1..16)
    AlgorithmMode algorithm { AlgorithmMode::Euclidean };
    int euclideanPulses { 4 };       // Hits in pattern
    int euclideanRotation { 0 };     // Phase rotation offset
    float markovDensity { 0.5f };    // Density / transition weight for Markov
    float poissonLambda { 2.0f };    // Average events per window for Poisson
    float swing { 0.0f };            // -1.0 to 1.0 (MPC/asymmetric swing)
    float timeWarp { 0.0f };         // -1.0 to 1.0 (non-linear metric time warp)
    float humanize { 0.0f };         // 0.0 to 1.0 (microtiming clock jitter)
    float triggerProbability { 1.0f }; // 0.0 to 1.0
    int velocity { 100 };            // 1..127
    int velocityRandom { 0 };        // ± random velocity spread
    float gatePercent { 0.8f };      // 0.05 to 4.0 of step length
    float gateRandom { 0.0f };       // 0.0 to 1.0
    int pitchRandomRange { 0 };      // 0 to 24 semitones
    float mutationRate { 0.0f };     // 0.0 to 1.0 per bar mutation chance
    uint32_t customPatternMask { 0 }; // 32-bit custom edited bitmask
    float pan { 0.0f };              // -1.0 to 1.0 (Manual base stereo position: -1=L, 0=C, +1=R)
    float panDepth { 0.0f };         // 0.0 to 1.0 (LFO modulation depth: 0% to 100%)
    int panRateMode { 2 };           // LFO frequency or random mode (0..13)
};

// Lock-free telemetry exported to the UI
struct LaneVisualTelemetry
{
    std::atomic<int> currentStep { 0 };
    std::atomic<int> totalSteps { 16 };
    std::atomic<float> playheadNorm { 0.0f }; // Normalized playhead [0..1]
    std::atomic<bool> justTriggered { false };
    std::atomic<uint8_t> lastVelocity { 0 };
    std::atomic<uint32_t> activePatternMask { 0 }; // Bitmask of active hits
    std::atomic<float> swingValue { 0.0f };        // Swing percentage [-1..1]
    std::atomic<float> timeWarpValue { 0.0f };     // Time warp percentage [-1..1]
    std::atomic<float> currentPan { 0.5f };        // Normalized pan [0..1] for GUI stereo meter
};

// Rhythmic event emitted to the audio processor MIDI scheduler
struct ScheduledNote
{
    int laneIndex { 0 };
    int sampleOffset { 0 };          // Offset within the current audio block [0, numSamples-1]
    int midiChannel { 1 };
    int midiNote { 36 };
    int velocity { 100 };
    int durationSamples { 2000 };    // Gate length in samples
    int pan { 64 };                  // MIDI CC #10 value (0..127, 64 = Center)
};

// Dedicated generative engine for multi-track polymetric/polyrhythmic generation
class RhythmEngine
{
public:
    RhythmEngine();
    ~RhythmEngine() = default;

    void prepare(double sampleRate);
    void reset();

    // Generate Bjorklund Euclidean bit pattern
    static uint32_t generateEuclideanPattern(int steps, int pulses, int rotation) noexcept;

    // Circular phase rotation for any 32-bit bitmask pattern
    static inline uint32_t rotatePattern(uint32_t pattern, int steps, int rotation) noexcept
    {
        if (steps <= 1) return pattern;
        int rot = ((rotation % steps) + steps) % steps;
        if (rot == 0) return pattern;
        uint32_t result = 0;
        for (int i = 0; i < steps; ++i)
        {
            int srcIdx = (i - rot + steps) % steps;
            if ((pattern & (1U << srcIdx)) != 0)
                result |= (1U << i);
        }
        return result;
    }

    // Mutate pattern stochastically
    static uint32_t mutatePattern(uint32_t currentPattern, int steps, float mutationRate, FastRandom& rng) noexcept;

    // Evaluate rhythmic generation for the current block
    // Returns scheduled notes within this block
    void processBlock(const LaneParameters lanes[kMaxLanes],
                      double ppqStart,
                      double ppqEnd,
                      double bpm,
                      int numSamples,
                      int64_t currentBlockStartSample,
                      bool isPlaying,
                      std::vector<ScheduledNote>& outScheduledNotes);

    // Update telemetry patterns even when DAW transport is stopped
    void updateOfflineTelemetry(const LaneParameters lanes[kMaxLanes]) noexcept;

    // Authoritative 32-bit custom pattern mask accessors (thread-safe, zero precision loss)
    void setCustomPatternMask(int laneIdx, uint32_t mask) noexcept
    {
        if (laneIdx >= 0 && laneIdx < kMaxLanes)
            customPatternMasks[static_cast<size_t>(laneIdx)].store(mask, std::memory_order_relaxed);
    }

    uint32_t getCustomPatternMask(int laneIdx) const noexcept
    {
        if (laneIdx >= 0 && laneIdx < kMaxLanes)
            return customPatternMasks[static_cast<size_t>(laneIdx)].load(std::memory_order_relaxed);
        return 0;
    }

    // Read telemetry for GUI
    LaneVisualTelemetry& getTelemetry(int laneIndex) noexcept
    {
        return telemetry[static_cast<size_t>(laneIndex)];
    }

    const LaneVisualTelemetry& getTelemetry(int laneIndex) const noexcept
    {
        return telemetry[static_cast<size_t>(laneIndex)];
    }

private:
    double currentSampleRate { 44100.0 };
    FastRandom rng { 0xC001CAFE };

    std::array<std::atomic<uint32_t>, kMaxLanes> customPatternMasks;

    // Per-lane internal state
    struct LaneState
    {
        double lastEvaluatedPpq { -1.0 };
        int lastStepIndex { -1 };
        int markovState { 0 }; // 0 = Rest, 1 = Hit, 2 = Ghost, 3 = Accent
        uint32_t cachedPattern { 0 };
        int lastBarCount { -1 };
        float lastRandomPan { 0.0f };
        float smoothRandomPan { 0.0f };
    };

    std::array<LaneState, kMaxLanes> laneStates;
    std::array<LaneVisualTelemetry, kMaxLanes> telemetry;

    // Markov transition resolver
    bool evaluateMarkovHit(int& currentState, float density, FastRandom& rng) noexcept;

    // Poisson burst step evaluator
    bool evaluatePoissonHit(float lambda, FastRandom& rng) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RhythmEngine)
};

} // namespace AlgorithmicRhythm
