# Interactive Pattern Editing (Orbit Double-Click & Step Strip) Implementation Plan

> **For Antigravity:** REQUIRED WORKFLOW: Use `.agent/workflows/execute-plan.md` to execute this plan in single-flow mode.

**Goal:** Enable direct interactive pattern editing by allowing users to double-click on orbit step nodes (turning on/off steps) and click on a new linear step-sequencer strip, with automatic seamless switching to Custom mode.

**Architecture:** Extend `AlgorithmMode` with `Custom = 3` and add `customPatternMask` to `LaneParameters`. Add `lane_{i}_custom_mask` to APVTS. Implement geometric node collision detection in `OrbitVisualizerComponent::mouseDoubleClick` and build an interactive `StepStripComponent` beneath the track controls.

**Tech Stack:** C++20, JUCE 7 (AudioProcessorValueTreeState, Graphics, MouseEvent, LookAndFeel).

---

### Task 1: Update RhythmEngine Data Model & Logic
**Files:**
- Modify: `Source/RhythmEngine.h`
- Modify: `Source/RhythmEngine.cpp`

**Step 1:** Add `AlgorithmMode::Custom = 3` to `AlgorithmMode` enum.
**Step 2:** Add `uint32_t customPatternMask` to `LaneParameters`.
**Step 3:** In `RhythmEngine::processBlock`, handle `AlgorithmMode::Custom`:
Use `params.customPatternMask` masked to the current number of steps.
Update `cachedPattern` and `telemetry.activePatternMask`.

---

### Task 2: APVTS Parameter & Helper Methods in PluginProcessor
**Files:**
- Modify: `Source/PluginProcessor.h`
- Modify: `Source/PluginProcessor.cpp`

**Step 1:** Add `lane_{i}_custom_mask` (AudioParameterInt) to `createParameterLayout()`.
**Step 2:** Add atomic pointer cache for `customMask` in `CachedLaneParams`.
**Step 3:** Add `toggleLaneStep(int laneIdx, int stepIdx)` method:
- Reads current pattern mask from telemetry or parameter.
- Toggles bit `(1U << stepIdx)`.
- Updates `lane_{i}_custom_mask`.
- Switches `lane_{i}_algo` parameter to `3` (Custom).

---

### Task 3: Orbit Visualizer Double-Click Node Detection
**Files:**
- Modify: `Source/PluginEditor.h`
- Modify: `Source/PluginEditor.cpp`

**Step 1:** Implement node coordinate lookup helper `findStepAt(float x, float y, int& outLane, int& outStep)`.
**Step 2:** In `mouseDoubleClick`:
- Test if double-click hit any step node within radius $\le 12\text{ px}$.
- If hit: call `processor.toggleLaneStep(lane, step)`, select lane, and trigger repaint.
- If not hit: maintain lane selection.

---

### Task 4: Interactive Linear Step-Sequencer Strip
**Files:**
- Modify: `Source/PluginEditor.h`
- Modify: `Source/PluginEditor.cpp`

**Step 1:** Implement `StepStripComponent`:
- Draws up to 32 rectangular step pads matching the track's step count.
- Pads show active/inactive state with the track's neon color.
- Displays current playhead position with a white glowing border.
- Single-click on any pad calls `processor.toggleLaneStep(currentTrack, step)`.
**Step 2:** Integrate `StepStripComponent` into `MidiRythmGenEditor` layout beneath track controls.

---

### Task 5: Compilation, Verification & Git Push
**Files:**
- Build: `build/`
- Test: `cmake --build build --config Release`
- Commit & Push to GitHub repository.
