# Design Doc: Interactive Pattern Editing (Orbit Double-Click & Step Strip)

## Date: 2026-09-08
## Status: Approved

## Overview
This feature introduces direct manual editing of rhythmic patterns in ChronoOrbit. Users can modify patterns either by double-clicking on any step node (on or off) in the concentric orbit visualizer or by clicking on pads in a new linear step-sequencer strip under the track controls.

---

## Architectural Changes

### 1. Generative & Data Model (`RhythmEngine`)
- **New Enum Value**: `AlgorithmMode::Custom = 3` ("Custom / Manual").
- **Lane Parameters**:
  - `uint32_t customPatternMask`: 32-bit integer holding the bitmask of active steps (bit $s = 1$ is an active hit, bit $s = 0$ is rest).
- **Processing Logic**:
  - When `params.algorithm == AlgorithmMode::Custom`:
    `activePattern = params.customPatternMask & ((1U << params.steps) - 1U);`
    `isHit = (activePattern & (1U << stepInPattern)) != 0;`
  - When in `Euclidean`, `Markov`, or `Poisson`, the algorithm generates the pattern as usual.
  - If a manual edit is made, the current active pattern is copied to `customPatternMask`, the toggled step bit is flipped, and the track algorithm is switched to `Custom`.

### 2. Audio Processor & Parameter Management (`PluginProcessor`)
- **APVTS Parameter**:
  - `lane_{i}_custom_mask` (AudioParameterInt, range `0` to `0x7FFFFFFF`, storing active pattern bitmask).
- **Helper API**:
  - `void toggleLaneStep(int laneIdx, int stepIdx);`
    - Retrieves current active mask (from telemetry or custom mask).
    - Flips bit `(1U << stepIdx)`.
    - Updates `lane_{i}_custom_mask` in APVTS.
    - Sets `lane_{i}_algo` parameter to `Custom` (index 3).
  - `uint32_t getLanePattern(int laneIdx) const;`

### 3. Orbit Visualizer Interaction (`OrbitVisualizerComponent`)
- `mouseDoubleClick(const juce::MouseEvent& event)`:
  - Calculates distance to all step nodes across all lanes:
    $$d = \sqrt{(x_{click} - nx)^2 + (y_{click} - ny)^2}$$
  - If $d \le 12\text{ px}$:
    - Identifies `(lane, stepIndex)`.
    - Selects the lane.
    - Calls callback `onStepToggled(lane, stepIndex)`.
    - Repaints with immediate visual feedback.
  - If outside any node but within ring radius:
    - Normal lane selection (single or double click).

### 4. Interactive Linear Step Strip (`StepStripComponent`)
- Displayed below track controls in `PluginEditor`.
- Renders up to 32 rectangular step pads matching the track's polymetric step count ($1 \dots 32$).
- Pads indicate:
  - Active hit (bright lane color) vs. Rest (dark charcoal).
  - Step index label ($1 \dots N$).
  - Real-time playhead highlight: white glowing border when `telemetry.currentStep == s`.
- Click on any pad toggles that step immediately and sets the track to `Custom`.

---

## Verification Plan
1. **Compilation Check**: `cmake --build build --config Release` against JUCE 7 and C++20.
2. **Double-Click Verification**: Double-clicking an off node turns it on; double-clicking an on node turns it off; verifies automatic mode switch to `Custom`.
3. **Step Strip Verification**: Clicking step pads updates both the strip and the orbit visualizer simultaneously.
4. **Playback & Audio Check**: Playback sounds the newly edited step on the next loop cycle without audio dropouts or hanging notes.
