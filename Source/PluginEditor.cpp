#include "PluginEditor.h"
#include <cmath>

//==============================================================================
// Lane Colors (Dark Cyberpunk / Modular Aesthetic)
//==============================================================================
const juce::Colour OrbitVisualizerComponent::laneColours[AlgorithmicRhythm::kMaxLanes] =
{
    juce::Colour (0xff00e5ff), // 1: Neon Cyan
    juce::Colour (0xff00ff88), // 2: Electric Mint
    juce::Colour (0xffffbb00), // 3: Warm Amber
    juce::Colour (0xffff0055), // 4: Cyber Crimson
    juce::Colour (0xffaa00ff), // 5: Deep Purple
    juce::Colour (0xff0088ff), // 6: Cobalt Blue
    juce::Colour (0xffff5500), // 7: Blaze Orange
    juce::Colour (0xfff0ff00)  // 8: Neon Yellow
};

//==============================================================================
// OrbitLookAndFeel
//==============================================================================
OrbitLookAndFeel::OrbitLookAndFeel()
{
    setColour (juce::Slider::thumbColourId, juce::Colour (0xff00e5ff));
    setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xff00e5ff));
    setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff252930));
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff14171c));
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff313742));
    setColour (juce::ComboBox::textColourId, juce::Colours::white.withAlpha (0.9f));
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff14171c));
    setColour (juce::PopupMenu::textColourId, juce::Colours::white);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff00e5ff).withAlpha (0.3f));
}

void OrbitLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPosProportional, float rotaryStartAngle,
                                        float rotaryEndAngle, juce::Slider& slider)
{
    juce::ignoreUnused (slider);

    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float centreX = bounds.getCentreX();
    const float centreY = bounds.getCentreY();
    const float rx = centreX - radius;
    const float ry = centreY - radius;
    const float rw = radius * 2.0f;
    const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // Background track arc
    juce::Path backgroundArc;
    backgroundArc.addCentredArc (centreX, centreY, radius - 3.0f, radius - 3.0f,
                                 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour (0xff1c2129));
    g.strokePath (backgroundArc, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Dial background disc
    g.setColour (juce::Colour (0xff12151a));
    g.fillEllipse (rx + 4.0f, ry + 4.0f, rw - 8.0f, rw - 8.0f);
    g.setColour (juce::Colour (0xff2a303c));
    g.drawEllipse (rx + 4.0f, ry + 4.0f, rw - 8.0f, rw - 8.0f, 1.0f);

    // Value active arc
    if (sliderPosProportional > 0.001f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc (centreX, centreY, radius - 3.0f, radius - 3.0f,
                                0.0f, rotaryStartAngle, angle, true);
        g.setColour (juce::Colour (0xff00e5ff));
        g.strokePath (valueArc, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Pointer needle
    juce::Path pointer;
    const float needleLen = radius * 0.65f;
    pointer.addLineSegment (juce::Line<float> (centreX, centreY,
                                               centreX + needleLen * std::sin (angle),
                                               centreY - needleLen * std::cos (angle)), 2.0f);
    g.setColour (juce::Colours::white);
    g.strokePath (pointer, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void OrbitLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                                    int buttonX, int buttonY, int buttonW, int buttonH,
                                    juce::ComboBox& box)
{
    juce::ignoreUnused (isButtonDown, buttonX, buttonY, buttonW, buttonH);

    const auto bounds = juce::Rectangle<float> (0, 0, static_cast<float> (width), static_cast<float> (height));
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (bounds, 4.0f);

    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    // Subtle down arrow
    const float arrowX = static_cast<float> (width) - 18.0f;
    const float arrowY = static_cast<float> (height) * 0.5f - 2.0f;
    juce::Path arrow;
    arrow.startNewSubPath (arrowX, arrowY);
    arrow.lineTo (arrowX + 5.0f, arrowY + 5.0f);
    arrow.lineTo (arrowX + 10.0f, arrowY);
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.strokePath (arrow, juce::PathStrokeType (1.5f));
}

//==============================================================================
// OrbitVisualizerComponent Implementation
//==============================================================================
OrbitVisualizerComponent::OrbitVisualizerComponent (MidiRythmGenProcessor& proc)
    : processor (proc)
{
    triggerFlashIntensity.fill (0.0f);
}

void OrbitVisualizerComponent::mouseDown (const juce::MouseEvent& event)
{
    const float cx = static_cast<float> (getWidth()) * 0.5f;
    const float cy = static_cast<float> (getHeight()) * 0.5f;
    const float dx = event.position.x - cx;
    const float dy = event.position.y - cy;
    const float dist = std::sqrt (dx * dx + dy * dy);

    const float maxRadius = juce::jmin (cx, cy) - 20.0f;
    const float minRadius = maxRadius * 0.22f;

    if (dist >= minRadius - 10.0f && dist <= maxRadius + 10.0f)
    {
        const float norm = (dist - minRadius) / (maxRadius - minRadius);
        const int lane = std::clamp (static_cast<int> (std::round (norm * static_cast<float> (AlgorithmicRhythm::kMaxLanes - 1))),
                                     0, AlgorithmicRhythm::kMaxLanes - 1);
        setSelectedLane (lane);
        if (onLaneSelected)
            onLaneSelected (lane);
    }
}

bool OrbitVisualizerComponent::findNodeAt (float mouseX, float mouseY, int& outLane, int& outStep) const
{
    const float cx = static_cast<float> (getWidth()) * 0.5f;
    const float cy = static_cast<float> (getHeight()) * 0.5f;
    const float maxRadius = juce::jmin (cx, cy) - 24.0f;
    const float minRadius = maxRadius * 0.22f;
    const float radiusStep = (maxRadius - minRadius) / static_cast<float> (AlgorithmicRhythm::kMaxLanes - 1);

    for (int lane = 0; lane < AlgorithmicRhythm::kMaxLanes; ++lane)
    {
        const auto& tele = processor.getRhythmEngine().getTelemetry (lane);
        const int steps = std::clamp (tele.totalSteps.load (std::memory_order_relaxed), 1, AlgorithmicRhythm::kMaxSteps);
        const float swing = tele.swingValue.load (std::memory_order_relaxed);
        const float timeWarp = tele.timeWarpValue.load (std::memory_order_relaxed);
        const float r = minRadius + static_cast<float> (lane) * radiusStep;

        for (int s = 0; s < steps; ++s)
        {
            const float angle = computeStepAngle (s, steps, swing, timeWarp);
            const float nx = cx + r * std::cos (angle);
            const float ny = cy + r * std::sin (angle);

            const float dx = mouseX - nx;
            const float dy = mouseY - ny;
            if ((dx * dx + dy * dy) <= (14.0f * 14.0f))
            {
                outLane = lane;
                outStep = s;
                return true;
            }
        }
    }
    return false;
}

void OrbitVisualizerComponent::mouseDoubleClick (const juce::MouseEvent& event)
{
    int lane = -1, step = -1;
    if (findNodeAt (event.position.x, event.position.y, lane, step))
    {
        setSelectedLane (lane);
        if (onStepToggled)
            onStepToggled (lane, step);
        else
            processor.toggleLaneStep (lane, step);

        if (onLaneSelected)
            onLaneSelected (lane);

        repaint();
    }
}

//==============================================================================
// StepStripComponent Implementation
//==============================================================================
StepStripComponent::StepStripComponent (MidiRythmGenProcessor& proc)
    : processor (proc)
{
}

void StepStripComponent::paint (juce::Graphics& g)
{
    const auto& tele = processor.getRhythmEngine().getTelemetry (currentTrack);
    const int steps = std::clamp (tele.totalSteps.load (std::memory_order_relaxed), 1, AlgorithmicRhythm::kMaxSteps);
    const uint32_t activeMask = tele.activePatternMask.load (std::memory_order_relaxed);
    const int curStep = tele.currentStep.load (std::memory_order_relaxed);
    const juce::Colour trackCol = OrbitVisualizerComponent::laneColours[currentTrack];

    const float width = static_cast<float> (getWidth());
    const float height = static_cast<float> (getHeight());

    g.setColour (juce::Colour (0xff101319));
    g.fillRoundedRectangle (0.0f, 0.0f, width, height, 4.0f);
    g.setColour (juce::Colour (0xff1f2532));
    g.drawRoundedRectangle (0.0f, 0.0f, width, height, 4.0f, 1.0f);

    for (int s = 0; s < steps; ++s)
    {
        const auto padRect = computeStepBounds (s, steps, width, height);
        const bool isHit = (activeMask & (1U << s)) != 0;
        const bool isPlayhead = (s == curStep);

        if (isHit)
        {
            g.setColour (trackCol.withAlpha (0.9f));
            g.fillRoundedRectangle (padRect, 3.0f);
            g.setColour (juce::Colours::black);
        }
        else
        {
            g.setColour (juce::Colour (0xff1a2029));
            g.fillRoundedRectangle (padRect, 3.0f);
            g.setColour (juce::Colours::white.withAlpha (0.4f));
        }

        if (padRect.getWidth() >= 12.0f)
        {
            g.setFont (juce::Font (padRect.getWidth() < 20.0f ? 8.0f : 10.0f, juce::Font::bold));
            g.drawText (juce::String (s + 1), padRect.toNearestInt(), juce::Justification::centred);
        }

        if (isPlayhead)
        {
            g.setColour (juce::Colours::white);
            g.drawRoundedRectangle (padRect, 3.0f, 2.0f);
        }
        else
        {
            g.setColour (juce::Colour (0xff2b3340));
            g.drawRoundedRectangle (padRect, 3.0f, 1.0f);
        }
    }
}

void StepStripComponent::mouseDown (const juce::MouseEvent& event)
{
    const auto& tele = processor.getRhythmEngine().getTelemetry (currentTrack);
    const int steps = std::clamp (tele.totalSteps.load (std::memory_order_relaxed), 1, AlgorithmicRhythm::kMaxSteps);
    const float width = static_cast<float> (getWidth());
    const float height = static_cast<float> (getHeight());
    const float margin = 4.0f;
    const float usableWidth = width - margin * 2.0f;

    if (steps > 0 && event.position.x >= margin && event.position.x <= (width - margin) && event.position.y >= 0.0f && event.position.y <= height)
    {
        const int s = std::clamp (static_cast<int> ((event.position.x - margin) / (usableWidth / static_cast<float> (steps))), 0, steps - 1);
        if (onStepToggled)
            onStepToggled (currentTrack, s);
        else
            processor.toggleLaneStep (currentTrack, s);

        repaint();
    }
}

void OrbitVisualizerComponent::updateAnimation()
{
    for (int i = 0; i < AlgorithmicRhythm::kMaxLanes; ++i)
    {
        auto& tele = processor.getRhythmEngine().getTelemetry (i);
        if (tele.justTriggered.exchange (false, std::memory_order_relaxed))
        {
            triggerFlashIntensity[static_cast<size_t> (i)] = 1.0f;
        }
        else
        {
            triggerFlashIntensity[static_cast<size_t> (i)] *= 0.82f; // Smooth exponential decay
        }
    }
}

void OrbitVisualizerComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0a0c10));

    const float width  = static_cast<float> (getWidth());
    const float height = static_cast<float> (getHeight());
    const float cx = width  * 0.5f;
    const float cy = height * 0.5f;

    const float maxRadius = juce::jmin (cx, cy) - 24.0f;
    const float minRadius = maxRadius * 0.22f;
    const float radiusStep = (maxRadius - minRadius) / static_cast<float> (AlgorithmicRhythm::kMaxLanes - 1);

    // Center core glyph
    g.setColour (juce::Colour (0xff12161f));
    g.fillEllipse (cx - minRadius * 0.8f, cy - minRadius * 0.8f, minRadius * 1.6f, minRadius * 1.6f);
    g.setColour (juce::Colour (0xff00e5ff).withAlpha (0.4f));
    g.drawEllipse (cx - minRadius * 0.8f, cy - minRadius * 0.8f, minRadius * 1.6f, minRadius * 1.6f, 1.0f);

    g.setFont (juce::Font (10.0f, juce::Font::bold));
    g.setColour (juce::Colours::white.withAlpha (0.7f));
    g.drawText ("SYNC", static_cast<int> (cx - 20.0f), static_cast<int> (cy - 8.0f), 40, 16, juce::Justification::centred);

    // Render concentric orbits
    for (int lane = 0; lane < AlgorithmicRhythm::kMaxLanes; ++lane)
    {
        const auto& tele = processor.getRhythmEngine().getTelemetry (lane);
        const int steps = std::clamp (tele.totalSteps.load (std::memory_order_relaxed), 1, AlgorithmicRhythm::kMaxSteps);
        const uint32_t activeMask = tele.activePatternMask.load (std::memory_order_relaxed);
        const float playhead = tele.playheadNorm.load (std::memory_order_relaxed);
        const float swing = tele.swingValue.load (std::memory_order_relaxed);
        const float timeWarp = tele.timeWarpValue.load (std::memory_order_relaxed);
        const float flash = triggerFlashIntensity[static_cast<size_t> (lane)];

        const float r = minRadius + static_cast<float> (lane) * radiusStep;
        const juce::Colour baseColour = laneColours[lane];
        const bool isSelected = (lane == selectedLane);

        // Orbit ring background
        juce::Colour ringColour = isSelected ? baseColour.withAlpha (0.45f)
                                             : juce::Colour (0xff1c212a).withAlpha (0.6f);
        g.setColour (ringColour);
        g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, isSelected ? 1.8f : 1.0f);

        // Flash aura on trigger
        if (flash > 0.05f)
        {
            g.setColour (baseColour.withAlpha (flash * 0.35f));
            g.drawEllipse (cx - r - 2.0f, cy - r - 2.0f, (r + 2.0f) * 2.0f, (r + 2.0f) * 2.0f, 3.0f);
        }

        // Draw step nodes
        for (int s = 0; s < steps; ++s)
        {
            const float angle = computeStepAngle (s, steps, swing, timeWarp);

            const float nx = cx + r * std::cos (angle);
            const float ny = cy + r * std::sin (angle);

            const bool isHit = (activeMask & (1U << s)) != 0;

            if (isHit)
            {
                const float nodeSize = isSelected ? 8.0f : 6.0f;
                g.setColour (baseColour.withAlpha (0.95f));
                g.fillEllipse (nx - nodeSize * 0.5f, ny - nodeSize * 0.5f, nodeSize, nodeSize);

                // Subtle outer glow
                g.setColour (baseColour.withAlpha (0.35f));
                g.drawEllipse (nx - nodeSize * 0.5f - 1.0f, ny - nodeSize * 0.5f - 1.0f, nodeSize + 2.0f, nodeSize + 2.0f, 1.0f);
            }
            else
            {
                const float nodeSize = 3.0f;
                g.setColour (juce::Colour (0xff252a33));
                g.fillEllipse (nx - nodeSize * 0.5f, ny - nodeSize * 0.5f, nodeSize, nodeSize);
            }
        }

        // Draw radial playhead marker (warped along with geometry)
        const double gamma = std::pow (2.0, static_cast<double> (timeWarp * 1.5f));
        const double warpedPlayhead = (std::abs (timeWarp) > 0.001f)
            ? std::clamp (std::pow (static_cast<double> (playhead), gamma), 0.0, 1.0)
            : static_cast<double> (playhead);
        const float playAngle = -juce::MathConstants<float>::halfPi +
                                static_cast<float> (juce::MathConstants<double>::twoPi * warpedPlayhead);
        const float px = cx + r * std::cos (playAngle);
        const float py = cy + r * std::sin (playAngle);

        // Playhead orb
        const float playheadRadius = (flash > 0.1f) ? (6.0f + flash * 4.0f) : 4.5f;
        g.setColour (juce::Colours::white);
        g.fillEllipse (px - playheadRadius * 0.5f, py - playheadRadius * 0.5f, playheadRadius, playheadRadius);

        if (flash > 0.05f)
        {
            g.setColour (baseColour.withAlpha (flash * 0.8f));
            g.drawEllipse (px - playheadRadius, py - playheadRadius, playheadRadius * 2.0f, playheadRadius * 2.0f, 2.0f);
        }
    }
}

//==============================================================================
// DawMidiClipComponent Implementation
//==============================================================================
DawMidiClipComponent::DawMidiClipComponent (MidiRythmGenProcessor& proc)
    : processor (proc)
{
    setRepaintsOnMouseActivity (true);
    setTooltip ("Cubase-Style MIDI Part: Drag & Drop directly into any DAW Track");
}

void DawMidiClipComponent::mouseEnter (const juce::MouseEvent&)
{
    isHovered = true;
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    repaint();
}

void DawMidiClipComponent::mouseExit (const juce::MouseEvent&)
{
    isHovered = false;
    setMouseCursor (juce::MouseCursor::NormalCursor);
    repaint();
}

void DawMidiClipComponent::mouseDown (const juce::MouseEvent& event)
{
    dragStartPos = event.getPosition();
    isDragging = false;
}

void DawMidiClipComponent::mouseDrag (const juce::MouseEvent& event)
{
    if (!isDragging && event.getDistanceFromDragStart() > 4)
    {
        isDragging = true;
        exportAndStartDrag();
    }
}

void DawMidiClipComponent::mouseUp (const juce::MouseEvent&)
{
    isDragging = false;
}

void DawMidiClipComponent::exportAndStartDrag()
{
    auto tempDir = juce::File::getSpecialLocation (juce::File::tempDirectory);
    auto midiFile = tempDir.getChildFile ("ChronoOrbit_Pattern.mid");
    processor.exportPatternToMidiFile (midiFile, 4);

    juce::StringArray files;
    files.add (midiFile.getFullPathName());
    juce::DragAndDropContainer::performExternalDragDropOfFiles (
        files,
        false,
        this,
        [this]() { isDragging = false; });
}

void DawMidiClipComponent::paint (juce::Graphics& g)
{
    const float width  = static_cast<float> (getWidth());
    const float height = static_cast<float> (getHeight());
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, width, height).reduced (1.0f);

    // Clip outer background (Cubase dark theme)
    g.setColour (juce::Colour (0xff0e1118));
    g.fillRoundedRectangle (bounds, 5.0f);

    // Outer border (illuminated on hover)
    g.setColour (isHovered ? juce::Colour (0xff00e5ff) : juce::Colour (0xff212735));
    g.drawRoundedRectangle (bounds, 5.0f, isHovered ? 1.5f : 1.0f);

    // Header banner (Cubase Track Event Header style)
    const float headerH = 22.0f;
    auto headerBounds = bounds.removeFromTop (headerH);
    g.setColour (juce::Colour (0xff171d28));
    g.fillRoundedRectangle (headerBounds.getX(), headerBounds.getY(), headerBounds.getWidth(), headerH, 4.0f);

    // Top neon cyan accent strip
    g.setColour (juce::Colour (0xff00e5ff));
    g.fillRect (headerBounds.getX(), headerBounds.getY(), headerBounds.getWidth(), 2.0f);

    // Title text
    g.setFont (juce::Font (10.5f, juce::Font::bold));
    g.setColour (juce::Colours::white);
    g.drawText ("MIDI CLIP: ChronoOrbit_Pattern.mid",
                juce::Rectangle<float> (headerBounds.getX() + 8.0f, headerBounds.getY(), 240.0f, headerH),
                juce::Justification::centredLeft);

    // Loop info
    g.setFont (juce::Font (9.0f, juce::Font::plain));
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.drawText ("4 BARS • 16 BEATS",
                juce::Rectangle<float> (headerBounds.getRight() - 210.0f, headerBounds.getY(), 100.0f, headerH),
                juce::Justification::centredRight);

    // Drag-to-DAW button badge
    auto badgeRect = juce::Rectangle<float> (headerBounds.getRight() - 102.0f, headerBounds.getY() + 2.5f, 96.0f, 17.0f);
    g.setColour (isHovered ? juce::Colour (0xff00e5ff).withAlpha (0.28f) : juce::Colour (0xff00e5ff).withAlpha (0.12f));
    g.fillRoundedRectangle (badgeRect, 3.0f);
    g.setColour (juce::Colour (0xff00e5ff));
    g.drawRoundedRectangle (badgeRect, 3.0f, 1.0f);
    g.setFont (juce::Font (9.0f, juce::Font::bold));
    g.drawText (isDragging ? "DRAGGING..." : "⤹ DRAG TO DAW", badgeRect, juce::Justification::centred);

    // Piano Roll / Track Lanes
    const float leftMargin = 28.0f;
    const float rightMargin = 6.0f;
    const float topMargin = headerH + 2.0f;
    const float bottomMargin = 4.0f;

    const float rollX = leftMargin;
    const float rollY = topMargin;
    const float rollW = width - leftMargin - rightMargin;
    const float rollH = height - topMargin - bottomMargin;
    const float laneH = rollH / static_cast<float> (AlgorithmicRhythm::kMaxLanes);

    // Read current lane parameters from processor
    AlgorithmicRhythm::LaneParameters lanes[AlgorithmicRhythm::kMaxLanes];
    processor.readLaneParameters (lanes);

    // 1. Draw horizontal track lanes
    for (int i = 0; i < AlgorithmicRhythm::kMaxLanes; ++i)
    {
        const int laneIdx = AlgorithmicRhythm::kMaxLanes - 1 - i; // Track 8 at top, Track 1 at bottom
        const float ly = rollY + static_cast<float> (i) * laneH;

        g.setColour ((i % 2 == 0) ? juce::Colour (0xff0c0f15) : juce::Colour (0xff11151f));
        g.fillRect (rollX, ly, rollW, laneH);

        // Lane separator line
        g.setColour (juce::Colour (0xff1a212c));
        g.drawHorizontalLine (static_cast<int> (ly), 2.0f, width - 2.0f);

        // Track label
        g.setFont (juce::Font (8.5f, juce::Font::bold));
        const bool isTrackEnabled = lanes[laneIdx].enabled;
        const auto laneCol = OrbitVisualizerComponent::laneColours[laneIdx];
        g.setColour (isTrackEnabled ? laneCol : juce::Colour (0xff333a46));
        g.drawText ("T" + juce::String (laneIdx + 1),
                    juce::Rectangle<float> (2.0f, ly, leftMargin - 4.0f, laneH),
                    juce::Justification::centred);
    }

    // 2. Draw vertical bar grid lines (4 bars total = 16 beats)
    for (int b = 0; b <= 4; ++b)
    {
        const float bx = rollX + (static_cast<float> (b) / 4.0f) * rollW;
        g.setColour (juce::Colour (0xff323d4e));
        g.drawVerticalLine (static_cast<int> (bx), rollY, rollY + rollH);

        if (b < 4)
        {
            // Quarter note beat subdivisions
            for (int q = 1; q < 4; ++q)
            {
                const float qx = bx + (static_cast<float> (q) / 16.0f) * rollW;
                g.setColour (juce::Colour (0xff1a202a));
                g.drawVerticalLine (static_cast<int> (qx), rollY, rollY + rollH);
            }
        }
    }

    // 3. Render MIDI Notes for all enabled lanes
    const double totalPpq = 16.0;

    for (int laneIdx = 0; laneIdx < AlgorithmicRhythm::kMaxLanes; ++laneIdx)
    {
        const auto& p = lanes[laneIdx];
        if (!p.enabled || p.steps <= 0)
            continue;

        const int displayRow = AlgorithmicRhythm::kMaxLanes - 1 - laneIdx;
        const float ly = rollY + static_cast<float> (displayRow) * laneH;
        const auto laneCol = OrbitVisualizerComponent::laneColours[laneIdx];

        const int steps = std::clamp (p.steps, 1, AlgorithmicRhythm::kMaxSteps);
        const int mult  = std::clamp (p.clockMultiplier, 1, 16);
        const int div   = std::clamp (p.clockDivider, 1, 16);
        const double ppqPerStep = (0.25 * static_cast<double> (div)) / static_cast<double> (mult);
        if (ppqPerStep <= 1e-6)
            continue;

        const double loopPpqDuration = steps * ppqPerStep;
        const uint32_t pattern = processor.getLanePattern (laneIdx);
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
            const float nx = rollX + static_cast<float> (noteStartPpq / totalPpq) * rollW;
            const float nw = std::max (3.5f, static_cast<float> (gatePpq / totalPpq) * rollW - 1.0f);
            const float ny = ly + 2.0f;
            const float nh = std::max (2.0f, laneH - 4.0f);

            // Velocity shading
            const float velAlpha = 0.65f + 0.35f * (static_cast<float> (std::clamp (p.velocity, 1, 127)) / 127.0f);
            g.setColour (laneCol.withAlpha (velAlpha));
            g.fillRoundedRectangle (nx, ny, nw, nh, 2.0f);
            g.setColour (laneCol.brighter (0.35f).withAlpha (velAlpha));
            g.drawRoundedRectangle (nx, ny, nw, nh, 2.0f, 0.7f);
        }
    }

    // 4. Sweeping playhead cursor
    const double currentPpq = processor.getCurrentPpqPosition();
    const double normPlayhead = std::fmod (std::max (0.0, currentPpq), totalPpq) / totalPpq;
    const float px = rollX + static_cast<float> (normPlayhead) * rollW;

    g.setColour (juce::Colour (0xff00e5ff).withAlpha (0.35f));
    g.fillRect (px - 1.5f, rollY, 3.0f, rollH);
    g.setColour (juce::Colours::white);
    g.drawVerticalLine (static_cast<int> (px), rollY, rollY + rollH);
}

//==============================================================================
// TrackVectorMatrixComponent Implementation
//==============================================================================
TrackVectorMatrixComponent::TrackVectorMatrixComponent (MidiRythmGenProcessor& proc)
    : processor (proc)
{
    matrixLabel.setText ("TRACK ENABLE MATRIX", juce::dontSendNotification);
    matrixLabel.setFont (juce::Font (9.0f, juce::Font::bold));
    matrixLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.55f));
    addAndMakeVisible (matrixLabel);

    for (int i = 0; i < AlgorithmicRhythm::kMaxLanes; ++i)
    {
        auto& btn = vectorButtons[static_cast<size_t> (i)];
        btn.setButtonText (juce::String (i + 1));
        btn.setClickingTogglesState (true);
        btn.setTooltip ("Activate / Mute Track " + juce::String (i + 1));
        addAndMakeVisible (btn);

        vectorAttachments[static_cast<size_t> (i)] =
            std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                processor.getAPVTS(),
                MidiRythmGenProcessor::getParamId (i, "enabled"),
                btn);
    }
}

void TrackVectorMatrixComponent::updateVisuals()
{
    for (int i = 0; i < AlgorithmicRhythm::kMaxLanes; ++i)
    {
        auto& btn = vectorButtons[static_cast<size_t> (i)];
        const bool active = btn.getToggleState();
        const auto laneCol = OrbitVisualizerComponent::laneColours[i];

        btn.setColour (juce::TextButton::buttonColourId,
                       active ? laneCol.withAlpha (0.35f) : juce::Colour (0xff14171f));
        btn.setColour (juce::TextButton::buttonOnColourId,
                       laneCol.withAlpha (0.45f));
        btn.setColour (juce::TextButton::textColourOffId,
                       juce::Colours::white.withAlpha (0.35f));
        btn.setColour (juce::TextButton::textColourOnId,
                       juce::Colours::white);
    }
}

void TrackVectorMatrixComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (juce::Colour (0xff141822));
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (juce::Colour (0xff252c3b));
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
}

void TrackVectorMatrixComponent::resized()
{
    matrixLabel.setBounds (8, 2, 160, 12);
    const int totalW = getWidth() - 12;
    const int gap = 3;
    const int btnW = (totalW - (AlgorithmicRhythm::kMaxLanes - 1) * gap) / AlgorithmicRhythm::kMaxLanes;
    const int startX = 6;
    const int btnY = 14;
    const int btnH = getHeight() - btnY - 4;

    for (int i = 0; i < AlgorithmicRhythm::kMaxLanes; ++i)
    {
        vectorButtons[static_cast<size_t> (i)].setBounds (startX + i * (btnW + gap), btnY, btnW, btnH);
    }
}

//==============================================================================
// Main Editor Implementation
//==============================================================================
MidiRythmGenEditor::MidiRythmGenEditor (MidiRythmGenProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      orbitVisualizer (p),
      dawMidiClip (p),
      trackVectorMatrix (p),
      stepStrip (p)
{
    setLookAndFeel (&customLookAndFeel);

    // Title labels
    titleLabel.setText ("CHRONO-ORBIT", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (18.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xff00e5ff));
    addAndMakeVisible (titleLabel);

    subTitleLabel.setText ("ALGORITHMIC MULTI-TRACK MIDI GENERATOR", juce::dontSendNotification);
    subTitleLabel.setFont (juce::Font (10.0f, juce::Font::plain));
    subTitleLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.5f));
    addAndMakeVisible (subTitleLabel);

    // Audition play/stop button
    playButton.setButtonText ("▶ PLAY");
    playButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff12202c));
    playButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff00e5ff));
    playButton.onClick = [this] {
        const bool nextState = !audioProcessor.isInternalPlaybackActive();
        audioProcessor.setInternalPlayback (nextState);
        playButton.setButtonText (nextState ? "■ STOP" : "▶ PLAY");
        playButton.setColour (juce::TextButton::buttonColourId,
                              nextState ? juce::Colour (0xff00e5ff).withAlpha (0.45f) : juce::Colour (0xff12202c));
    };
    addAndMakeVisible (playButton);

    // Whole-tone aleatoric randomizer button
    randomizeButton.setButtonText ("🎲 RANDOM");
    randomizeButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff251733));
    randomizeButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffff00aa));
    randomizeButton.onClick = [this] {
        audioProcessor.randomizeWholeTonePattern();
        selectTrack (currentTrackIndex);
        orbitVisualizer.repaint();
        stepStrip.repaint();
        dawMidiClip.repaint();
        trackVectorMatrix.updateVisuals();
    };
    addAndMakeVisible (randomizeButton);

    // Track enable vector matrix (top right)
    addAndMakeVisible (trackVectorMatrix);

    // Orbit visualizer
    addAndMakeVisible (orbitVisualizer);
    orbitVisualizer.onLaneSelected = [this] (int lane) { selectTrack (lane); };

    // DAW MIDI clip
    addAndMakeVisible (dawMidiClip);

    // Global controls
    setupKnob (globalMutationSlider, globalMutationLabel, "MUTATION", "%");
    setupKnob (globalSwingSlider,    globalSwingLabel,    "SWING", "%");
    setupKnob (globalHumanizeSlider, globalHumanizeLabel, "HUMANIZE", "%");

    globalMutationAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.getAPVTS(), "global_mutation", globalMutationSlider);
    globalSwingAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.getAPVTS(), "global_swing", globalSwingSlider);
    globalHumanizeAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.getAPVTS(), "global_humanize", globalHumanizeSlider);

    // Track selection tab buttons (1 to 8)
    for (int i = 0; i < AlgorithmicRhythm::kMaxLanes; ++i)
    {
        auto& btn = trackSelectButtons[static_cast<size_t> (i)];
        btn.setButtonText ("T" + juce::String (i + 1));
        btn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff161a22));
        btn.setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.7f));
        btn.onClick = [this, i] { selectTrack (i); };
        addAndMakeVisible (btn);
    }

    // Per-track controls
    trackEnabledToggle.setButtonText ("LANE ACTIVE");
    trackEnabledToggle.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    trackEnabledToggle.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff00e5ff));
    addAndMakeVisible (trackEnabledToggle);

    algoComboBox.addItemList ({ "Euclidean (Bjorklund)", "Markov Chain", "Poisson Burst", "Custom Pattern" }, 1);
    addAndMakeVisible (algoComboBox);
    algoLabel.setText ("ALGORITHM", juce::dontSendNotification);
    algoLabel.setFont (juce::Font (10.0f, juce::Font::bold));
    algoLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
    addAndMakeVisible (algoLabel);

    setupKnob (stepsSlider, stepsLabel, "STEPS", "");
    setupKnob (pulsesSlider, pulsesLabel, "PULSES", "");
    setupKnob (rotationSlider, rotationLabel, "ROTATION", "");
    setupKnob (clockMultSlider, clockMultLabel, "MULT", "x");
    setupKnob (clockDivSlider, clockDivLabel, "DIV", "/");
    setupKnob (probabilitySlider, probabilityLabel, "PROB", "%");
    setupKnob (markovDensitySlider, markovDensityLabel, "DENSITY", "");
    setupKnob (rootNoteSlider, rootNoteLabel, "ROOT", "");
    setupKnob (timeWarpSlider, timeWarpLabel, "TIME WARP", "%");
    setupKnob (pitchRndSlider, pitchRndLabel, "PITCH RND", "st");
    setupKnob (velocitySlider, velocityLabel, "VELOCITY", "");
    setupKnob (velocityRndSlider, velocityRndLabel, "VEL RND", "");
    setupKnob (gateSlider, gateLabel, "GATE", "x");

    // Step sequencer strip setup
    stepStripLabel.setText ("STEP SEQUENCER (CLICK PAD OR DOUBLE-CLICK ORBIT NODE)", juce::dontSendNotification);
    stepStripLabel.setFont (juce::Font (9.0f, juce::Font::bold));
    stepStripLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.5f));
    addAndMakeVisible (stepStripLabel);

    stepStrip.onStepToggled = [this] (int track, int step) {
        audioProcessor.toggleLaneStep (track, step);
        orbitVisualizer.repaint();
        stepStrip.repaint();
    };
    addAndMakeVisible (stepStrip);

    orbitVisualizer.onStepToggled = [this] (int lane, int step) {
        audioProcessor.toggleLaneStep (lane, step);
        orbitVisualizer.repaint();
        stepStrip.repaint();
    };

    // Initialize track 0
    selectTrack (0);

    setSize (960, 660);
    startTimerHz (60); // 60 FPS smooth GUI telemetry update
}

MidiRythmGenEditor::~MidiRythmGenEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MidiRythmGenEditor::setupKnob (juce::Slider& slider, juce::Label& label, const juce::String& text, const juce::String& suffix)
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 16);
    slider.setTextValueSuffix (" " + suffix);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white.withAlpha (0.9f));
    addAndMakeVisible (slider);

    label.setText (text, juce::dontSendNotification);
    label.setFont (juce::Font (10.0f, juce::Font::bold));
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
    addAndMakeVisible (label);
}

void MidiRythmGenEditor::selectTrack (int trackIndex)
{
    currentTrackIndex = std::clamp (trackIndex, 0, AlgorithmicRhythm::kMaxLanes - 1);
    audioProcessor.setSelectedTrackIndex (currentTrackIndex);

    for (int i = 0; i < AlgorithmicRhythm::kMaxLanes; ++i)
    {
        const bool isCur = (i == currentTrackIndex);
        trackSelectButtons[static_cast<size_t> (i)].setColour (
            juce::TextButton::buttonColourId,
            isCur ? OrbitVisualizerComponent::laneColours[i].withAlpha (0.4f) : juce::Colour (0xff161a22));
    }

    orbitVisualizer.setSelectedLane (currentTrackIndex);
    stepStrip.setTrack (currentTrackIndex);
    bindTrackAttachments (currentTrackIndex);
}

void MidiRythmGenEditor::bindTrackAttachments (int trackIndex)
{
    // Reset existing attachments
    trackEnabledAttach.reset();
    algoAttach.reset();
    stepsAttach.reset();
    pulsesAttach.reset();
    rotationAttach.reset();
    clockMultAttach.reset();
    clockDivAttach.reset();
    probAttach.reset();
    markovDensityAttach.reset();
    rootNoteAttach.reset();
    timeWarpAttach.reset();
    pitchRndAttach.reset();
    velocityAttach.reset();
    velocityRndAttach.reset();
    gateAttach.reset();

    auto& apvts = audioProcessor.getAPVTS();

    trackEnabledAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, MidiRythmGenProcessor::getParamId (trackIndex, "enabled"), trackEnabledToggle);

    algoAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, MidiRythmGenProcessor::getParamId (trackIndex, "algo"), algoComboBox);

    stepsAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, MidiRythmGenProcessor::getParamId (trackIndex, "steps"), stepsSlider);

    pulsesAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, MidiRythmGenProcessor::getParamId (trackIndex, "pulses"), pulsesSlider);

    rotationAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, MidiRythmGenProcessor::getParamId (trackIndex, "rotation"), rotationSlider);

    clockMultAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, MidiRythmGenProcessor::getParamId (trackIndex, "clock_mult"), clockMultSlider);

    clockDivAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, MidiRythmGenProcessor::getParamId (trackIndex, "clock_div"), clockDivSlider);

    probAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, MidiRythmGenProcessor::getParamId (trackIndex, "probability"), probabilitySlider);

    markovDensityAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, MidiRythmGenProcessor::getParamId (trackIndex, "markov_density"), markovDensitySlider);

    rootNoteAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, MidiRythmGenProcessor::getParamId (trackIndex, "root_note"), rootNoteSlider);

    timeWarpAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, MidiRythmGenProcessor::getParamId (trackIndex, "time_warp"), timeWarpSlider);

    pitchRndAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, MidiRythmGenProcessor::getParamId (trackIndex, "pitch_rnd"), pitchRndSlider);

    velocityAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, MidiRythmGenProcessor::getParamId (trackIndex, "velocity"), velocitySlider);

    velocityRndAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, MidiRythmGenProcessor::getParamId (trackIndex, "velocity_rnd"), velocityRndSlider);

    gateAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, MidiRythmGenProcessor::getParamId (trackIndex, "gate"), gateSlider);
}

void MidiRythmGenEditor::timerCallback()
{
    orbitVisualizer.updateAnimation();
    orbitVisualizer.repaint();
    stepStrip.repaint();
    dawMidiClip.repaint();
    trackVectorMatrix.updateVisuals();

    const bool isPlaying = audioProcessor.isInternalPlaybackActive();
    playButton.setButtonText (isPlaying ? "■ STOP" : "▶ PLAY");
    playButton.setColour (juce::TextButton::buttonColourId,
                          isPlaying ? juce::Colour (0xff00e5ff).withAlpha (0.45f) : juce::Colour (0xff12202c));
}

void MidiRythmGenEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0d1016));

    // Dividing lines & subtle panels
    g.setColour (juce::Colour (0xff1a1f29));
    g.drawLine (520.0f, 60.0f, 520.0f, static_cast<float> (getHeight()), 1.0f);
    g.drawLine (0.0f, 60.0f, static_cast<float> (getWidth()), 60.0f, 1.0f);

    // Track controls panel header
    g.setFont (juce::Font (12.0f, juce::Font::bold));
    g.setColour (OrbitVisualizerComponent::laneColours[currentTrackIndex]);
    g.drawText ("TRACK " + juce::String (currentTrackIndex + 1) + " CONFIGURATION",
                540, 70, 300, 20, juce::Justification::left);
}

void MidiRythmGenEditor::resized()
{
    // Header Left: Title & Subtitle
    titleLabel.setBounds (16, 8, 175, 22);
    subTitleLabel.setBounds (16, 28, 175, 14);

    // Transport & Randomize
    playButton.setBounds (196, 12, 68, 34);
    randomizeButton.setBounds (268, 12, 90, 34);

    // Global knobs (compacted to fit top header)
    globalMutationLabel.setBounds (364, 6, 48, 12);
    globalMutationSlider.setBounds (364, 16, 48, 38);

    globalSwingLabel.setBounds (414, 6, 48, 12);
    globalSwingSlider.setBounds (414, 16, 48, 38);

    globalHumanizeLabel.setBounds (464, 6, 52, 12);
    globalHumanizeSlider.setBounds (464, 16, 52, 38);

    // Vector track activation matrix (top right in previously empty space!)
    trackVectorMatrix.setBounds (528, 8, 418, 44);

    // Left Column: Orbit visualizer + Cubase-style DAW MIDI Clip below it
    orbitVisualizer.setBounds (15, 68, 490, 388);
    dawMidiClip.setBounds (15, 464, 490, 182);

    // Right Column: Track controls
    const int tabWidth = 46;
    for (int i = 0; i < AlgorithmicRhythm::kMaxLanes; ++i)
    {
        trackSelectButtons[static_cast<size_t> (i)].setBounds (535 + i * (tabWidth + 4), 98, tabWidth, 26);
    }

    trackEnabledToggle.setBounds (535, 134, 120, 24);
    algoLabel.setBounds (665, 126, 90, 14);
    algoComboBox.setBounds (665, 142, 270, 24);

    // Row 1 Knobs: Steps, Pulses, Rotation, Mult, Div
    int kY1 = 180;
    int kW = 72;
    int kH = 70;
    int kSpacing = 78;

    stepsLabel.setBounds (535 + 0 * kSpacing, kY1, kW, 14);
    stepsSlider.setBounds (535 + 0 * kSpacing, kY1 + 14, kW, kH);

    pulsesLabel.setBounds (535 + 1 * kSpacing, kY1, kW, 14);
    pulsesSlider.setBounds (535 + 1 * kSpacing, kY1 + 14, kW, kH);

    rotationLabel.setBounds (535 + 2 * kSpacing, kY1, kW, 14);
    rotationSlider.setBounds (535 + 2 * kSpacing, kY1 + 14, kW, kH);

    clockMultLabel.setBounds (535 + 3 * kSpacing, kY1, kW, 14);
    clockMultSlider.setBounds (535 + 3 * kSpacing, kY1 + 14, kW, kH);

    clockDivLabel.setBounds (535 + 4 * kSpacing, kY1, kW, 14);
    clockDivSlider.setBounds (535 + 4 * kSpacing, kY1 + 14, kW, kH);

    // Row 2 Knobs: Probability, Markov Density, Gate, Velocity, Vel Rnd
    int kY2 = 282;
    probabilityLabel.setBounds (535 + 0 * kSpacing, kY2, kW, 14);
    probabilitySlider.setBounds (535 + 0 * kSpacing, kY2 + 14, kW, kH);

    markovDensityLabel.setBounds (535 + 1 * kSpacing, kY2, kW, 14);
    markovDensitySlider.setBounds (535 + 1 * kSpacing, kY2 + 14, kW, kH);

    gateLabel.setBounds (535 + 2 * kSpacing, kY2, kW, 14);
    gateSlider.setBounds (535 + 2 * kSpacing, kY2 + 14, kW, kH);

    velocityLabel.setBounds (535 + 3 * kSpacing, kY2, kW, 14);
    velocitySlider.setBounds (535 + 3 * kSpacing, kY2 + 14, kW, kH);

    velocityRndLabel.setBounds (535 + 4 * kSpacing, kY2, kW, 14);
    velocityRndSlider.setBounds (535 + 4 * kSpacing, kY2 + 14, kW, kH);

    // Row 3: Root, Time Warp, Pitch Rnd
    int kY3 = 384;
    rootNoteLabel.setBounds (535 + 0 * kSpacing, kY3, kW, 14);
    rootNoteSlider.setBounds (535 + 0 * kSpacing, kY3 + 14, kW, kH);

    timeWarpLabel.setBounds (535 + 2 * kSpacing, kY3, kW, 14);
    timeWarpSlider.setBounds (535 + 2 * kSpacing, kY3 + 14, kW, kH);

    pitchRndLabel.setBounds (535 + 4 * kSpacing, kY3, kW, 14);
    pitchRndSlider.setBounds (535 + 4 * kSpacing, kY3 + 14, kW, kH);

    // Row 4: Step Sequencer Strip
    stepStripLabel.setBounds (535, 484, 400, 14);
    stepStrip.setBounds (535, 502, 400, 42);
}
