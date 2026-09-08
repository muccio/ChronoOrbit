# Design Document: Pattern Retention Fix & Swing Timeline Distortion (Approach A)

**Date**: 2026-09-08  
**Target**: ChronoOrbit JUCE (C++20) VST3 & Standalone  
**Status**: Approved (Approach A)

---

## 1. Problem Statement

1. **Pattern Reset / Loss of Custom Steps**:
   - Custom pattern steps toggled by the user frequently reset or disappear when changing track configuration, adjusting sliders, or re-selecting tracks.
   - **Root Cause 1**: APVTS parameter storage for `custom_mask` maps through `std::atomic<float>`, which only possesses 24 bits of mantissa precision. Storing a 32-bit integer bitmask causes upper bits (> bit 23) to lose precision and lower bits to round or zero out.
   - **Root Cause 2**: In `RhythmEngine`, pattern computation was executed only inside `processBlock` while `isPlaying == true`. When the DAW transport is paused or stopped, telemetry patterns were stale or zero. Toggling steps while stopped operated on a zero mask.
   - **Root Cause 3**: Changing step length or switching algorithms could overwrite or fail to preserve the underlying 32-bit user bitmask.

2. **Visual Rhythm Distortion on the Timeline**:
   - Currently, nodes on the orbit rings and pads on the step sequencer strip are placed with strict equidistant spacing.
   - The user requested visual distortion of point distribution along the timeline for the relative track to visually communicate microtiming/swing groove dynamics.

---

## 2. Proposed Architecture & Solution

### Component A: Robust 32-Bit Pattern Retention Engine

1. **Direct 32-Bit Thread-Safe Storage**:
   - Maintain `std::atomic<uint32_t> customPatternMasks[kMaxLanes]` in `RhythmEngine` (and accessors in `PluginProcessor`).
   - Eliminate float32 quantization by keeping the authoritative custom bitmask as a pure `uint32_t`.
   - Update APVTS `custom_mask` as two 16-bit safe integers (or keep direct XML synchronization) to ensure zero float mantissa truncation.

2. **Offline & Synchronous Pattern Calculation**:
   - Extract pattern calculation into a method `recalculatePatterns(int laneIdx = -1)` callable both in `processBlock` and whenever parameters change while stopped.
   - When the DAW is stopped, tweaking `pulses`, `rotation`, `steps`, or `algo` immediately refreshes `tele.activePatternMask`.
   - In `toggleLaneStep(lane, step)`: if the lane is currently in Euclidean, Markov, or Poisson mode, snapshot the current active pattern mask directly into `customPatternMasks[lane]`, flip the toggled bit, and switch mode to `Custom`.

3. **DAW State Serialization (XML Persistence)**:
   - In `getStateInformation()` and `setStateInformation()`:
     Save each lane's 32-bit custom mask as a hex string or uint in an XML child element `<CUSTOM_PATTERNS>` inside the APVTS state.
     On recall, restore all 8 masks losslessly.

---

## 3. Component B: Geometric Swing Timeline Distortion

1. **Mathematical Model for Swing Displacement**:
   - Let $N$ be the number of steps in the track ($1 \le N \le 32$).
   - Let $S \in [0.0, 1.0]$ be the track's swing percentage.
   - In `RhythmEngine`, odd steps ($s \pmod 2 == 1$) are shifted by:
     $$\Delta t_{swing} = S \times 0.333 \times \frac{1}{N}$$
   - Normalized timeline position $T(s) \in [0.0, 1.0)$ for step $s$:
     $$T(s) = \frac{s + ((s \pmod 2 == 1) ? S \times 0.333 : 0.0)}{N}$$

2. **Orbit Visualizer Distortion (`OrbitVisualizerComponent`)**:
   - Radial step nodes are rendered at angle:
     $$\theta(s) = -\frac{\pi}{2} + 2\pi \cdot T(s)$$
   - When swing increases, odd nodes visibly advance along the orbit circumference closer to their even partners, physically illustrating the swung rhythm.
   - Collision detection `findNodeAt()` uses $\theta(s)$ with the lane's actual swing value, ensuring double-click detection matches the visible distorted positions with pixel accuracy.

3. **Step Strip Distortion (`StepStripComponent`)**:
   - On the horizontal 32-pad timeline strip, the horizontal center and bounds of each pad are distorted according to $T(s)$.
   - Pad boundaries stretch and compress proportionally: odd pads slide closer to the next step, visually demonstrating swing elasticity.
   - Pad click detection (`mouseDown`) uses the distorted pad coordinates.

---

## 4. Verification Plan

1. **Compilation**: Clean build of VST3 and Standalone targets via `cmake --build build --config Release`.
2. **Bitmask Integrity Verification**:
   - Set custom bits across steps 0, 7, 15, 23, 24, 30, 31.
   - Change track parameters (steps, sliders, track switching) and confirm no bits are lost.
   - Stop and start playback to confirm custom patterns survive DAW transport transitions.
3. **Visual Swing Distortion Verification**:
   - Increase `Global Swing` from 0% to 100%.
   - Confirm odd nodes on orbit rings visibly shift along the circumference.
   - Confirm step strip pads compress/expand according to swing.
   - Confirm clicking shifted pads and double-clicking shifted nodes correctly toggles the step at its exact visual location.
