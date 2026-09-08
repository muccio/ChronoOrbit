# Design Document: Rotation Fix, Time Warp Distortion, Scale Removal & MIDI In Root Control

**Date**: 2026-09-08  
**Target**: ChronoOrbit JUCE (C++20) VST3 & Standalone  
**Status**: Approved  

---

## 1. Requirements Summary

1. **Rotation Bug Fix**:
   - `rotation` parameter must rotate active patterns across all algorithms, specifically for `Custom Pattern` mode where rotation was previously ignored.
2. **Timeline Geometric Distortion (Time Warp + Swing)**:
   - Implement prominent, visually distinct distortion of step distribution along the timeline for the relative track.
   - Add a dedicated per-track **TIME WARP** parameter (-100% to +100%) that bends step timing non-linearly ($\tau(t) = t^\gamma$), physically shifting nodes on the circular orbit and pads on the horizontal step strip, perfectly aligned with the audio microtiming engine.
3. **Scale Parameter Removal**:
   - Completely remove the `scale` selector, parameter, and quantizer logic. Replace its UI position with the new `TIME WARP` rotary control.
4. **MIDI In Root Note Control**:
   - Enable `NEEDS_MIDI_INPUT TRUE`.
   - Process incoming MIDI Note-On events to update the `root_note` parameter in real time for the selected track (or track matching MIDI input channel 1-8).

---

## 2. Technical Architecture

### Component 1: Pattern Rotation Across All Algorithms
- Add pure bitwise rotation helper `rotatePattern(uint32_t pattern, int steps, int rotation)`.
- In `RhythmEngine.cpp`: apply `rotatePattern` to `customPatternMask` in both `updateOfflineTelemetry` and `processBlock`:
  ```cpp
  const uint32_t rawMask = customPatternMasks[laneIdx].load(...) & validMask;
  activePattern = rotatePattern(rawMask, steps, params.euclideanRotation);
  ```
- Turning `ROTATION` now immediately rotates the visible nodes on the orbit and pads on the sequencer strip.

### Component 2: Time Warp & Timeline Geometric Distortion
- Parameter: `lane_{i}_time_warp` (Float, range `[-1.0f, 1.0f]`, default `0.0f`, format `"%"`).
- Mathematical Warp & Swing formula:
  $$\gamma = 2^{\text{warp}}$$
  For step $s \in [0, N-1]$:
  $$t_{raw}(s) = \frac{s + ((s \pmod 2 == 1) ? \text{swing} \times 0.5 : 0.0)}{N}$$
  $$T(s) = (t_{raw}(s))^\gamma$$
- Orbit Visualizer: $\theta(s) = -\frac{\pi}{2} + 2\pi \cdot T(s)$.
- Step Strip: pad boundaries $[T(s), T(s+1)]$ stretch and compress smoothly.
- Real-Time Audio Engine: uses $T(s)$ to compute exact sample-accurate microtiming offsets inside the audio block.

### Component 3: Scale Removal & UI Layout Update
- Remove `scaleComboBox`, `scaleLabel`, `scaleAttach`, and `ScaleType` parameter.
- Place `timeWarpSlider` and `timeWarpLabel` ("TIME WARP", "%") in the track parameter panel.
- Pitch calculation simplifies to:
  `midiPitch = std::clamp(params.rootNote + randomOffset, 0, 127);`

### Component 4: MIDI In Root Note Control
- Enable `NEEDS_MIDI_INPUT TRUE` in `CMakeLists.txt`.
- In `PluginProcessor::processBlock`:
  Iterate over incoming `midiMessages` before emitting scheduled notes.
  If `msg.isNoteOn()`:
  - Extract note number ($0..127$).
  - Target lane: if channel $\in [1..8]$, target is `channel - 1`; otherwise target is `selectedLaneIndex`.
  - Update `lane_{target}_root_note` parameter in APVTS.
  - Refresh telemetry so UI knob and display update in real time.

---

## 3. Verification Plan
- Clean compilation of VST3 and Standalone targets.
- Verify that rotating the `ROTATION` knob rotates custom patterns on orbit and step strip.
- Verify that adjusting `TIME WARP` and `SWING` produces clear, pronounced non-linear spacing on the timeline.
- Verify that `scale` is cleanly removed with no compilation warnings.
- Verify that playing incoming MIDI notes sets the root note of the active track.
