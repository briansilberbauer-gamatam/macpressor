#include "EnginePanel.h"

#include "PluginProcessor.h"
#include "Theme.h"

#include <cmath>

namespace macpressor::ui
{
namespace
{
float approach (float current, float target, float risePerFrame, float fallPerFrame)
{
    if (target > current)
        return juce::jmin (target, current + risePerFrame);

    return juce::jmax (target, current - fallPerFrame);
}
} // namespace

EnginePanel::EnginePanel (MacPressorAudioProcessor& processorToUse)
    : processor (processorToUse)
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (30);
}

void EnginePanel::timerCallback()
{
    const float inputTarget = juce::jlimit (-60.0f, 6.0f,
        juce::Decibels::gainToDecibels (processor.getInputLevel(), -60.0f));
    const float outputTarget = juce::jlimit (-60.0f, 6.0f,
        juce::Decibels::gainToDecibels (processor.getOutputLevel(), -60.0f));
    const float grTarget = juce::jlimit (0.0f, 24.0f, processor.getGainReductionDb());

    displayInputDb = approach (displayInputDb, inputTarget, 4.5f, 1.4f);
    displayOutputDb = approach (displayOutputDb, outputTarget, 4.5f, 1.4f);
    displayGainReduction = approach (displayGainReduction, grTarget, 6.0f, 0.65f);

    repaint();
}

void EnginePanel::drawMeter (juce::Graphics& g,
                             juce::Rectangle<float> row,
                             const juce::String& label,
                             float normalised,
                             const juce::String& value,
                             juce::Colour fill) const
{
    const auto labelArea = row.withWidth (42.0f);
    const auto valueArea = row.withTrimmedLeft (row.getWidth() - 56.0f);
    const auto track = row.withTrimmedLeft (45.0f).withTrimmedRight (60.0f).reduced (0.0f, 5.5f);

    g.setColour (textMuted);
    g.setFont (font (5.8f, FontWeight::semibold, 0.14f));
    g.drawText (label, labelArea, juce::Justification::centredLeft, false);

    g.setColour (obsidianWell);
    g.fillRoundedRectangle (track.expanded (0.5f), 1.4f);

    constexpr int segments = 18;
    constexpr float gap = 1.15f;
    const float segmentWidth = (track.getWidth() - gap * static_cast<float> (segments - 1))
                             / static_cast<float> (segments);
    const int activeSegments = juce::jlimit (0, segments, juce::roundToInt (normalised * segments));

    for (int i = 0; i < segments; ++i)
    {
        const auto segment = juce::Rectangle<float> (
            track.getX() + static_cast<float> (i) * (segmentWidth + gap),
            track.getY(), segmentWidth, track.getHeight());

        if (i < activeSegments)
        {
            const bool hot = i >= segments - 4;
            g.setColour (fill.withAlpha (hot ? 1.0f : 0.82f));
            g.fillRoundedRectangle (segment, 0.9f);

            if (hot)
            {
                g.setColour (crimsonGlow.withAlpha (0.30f));
                g.fillRoundedRectangle (segment.expanded (0.7f, 1.2f), 1.2f);
            }
        }
        else
        {
            g.setColour (obsidianGrid.withAlpha (0.8f));
            g.fillRoundedRectangle (segment, 0.9f);
        }
    }

    g.setColour (textSecondary);
    g.setFont (font (6.4f, FontWeight::semibold, 0.03f));
    g.drawText (value, valueArea, juce::Justification::centredRight, false);
}

void EnginePanel::paint (juce::Graphics& g)
{
    const auto panel = getLocalBounds().toFloat().reduced (0.5f);

    if (panel.getWidth() < 40.0f || panel.getHeight() < 40.0f)
        return;

    constexpr float corner = 8.0f;

    g.setColour (obsidianScreen.withAlpha (0.92f));
    g.fillRoundedRectangle (panel, corner);

    juce::ColourGradient bodyGradient (juce::Colour (0xff1b1e25), panel.getTopLeft(),
                                       juce::Colour (0xff101216), panel.getBottomRight(), false);
    bodyGradient.addColour (0.48, gunmetalBase);
    g.setGradientFill (bodyGradient);
    g.fillRoundedRectangle (panel.reduced (1.0f), corner - 1.0f);

    g.setColour (glassRimHighlight.withAlpha (0.20f));
    g.drawRoundedRectangle (panel, corner, 0.9f);
    g.setColour (crimsonAccent.withAlpha (0.34f));
    g.drawRoundedRectangle (panel.reduced (1.0f), corner - 1.0f, 0.55f);

    {
        juce::Graphics::ScopedSaveState state (g);
        juce::Path clip;
        clip.addRoundedRectangle (panel, corner);
        g.reduceClipRegion (clip);

        juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.030f), panel.getTopLeft(),
                                    juce::Colours::transparentWhite,
                                    panel.getTopLeft().translated (panel.getWidth() * 0.68f,
                                                                  panel.getHeight() * 0.62f), true);
        g.setGradientFill (sheen);
        g.fillRect (panel);
    }

    drawEngravedLabel (g, { 16.0f, 13.0f, panel.getWidth() - 74.0f, 27.0f },
                       "VOICE ENGINE", "FIXED CURVE / AUTOMATIC MAKE-UP");

    const auto& profile = processor.getActiveProfile();
    const int modeIndex = processor.getModeIndex();

    // Four-position voice indicator.
    for (int i = 0; i < 4; ++i)
    {
        const float x = panel.getRight() - 18.0f - static_cast<float> (3 - i) * 10.0f;
        const juce::Point<float> dot (x, 24.0f);

        if (i == modeIndex)
        {
            drawCrimsonDot (g, dot, 1.35f);
        }
        else
        {
            g.setColour (textMuted.withAlpha (0.55f));
            g.fillEllipse (dot.x - 1.15f, dot.y - 1.15f, 2.3f, 2.3f);
        }
    }

    // Active voice identity.
    g.setColour (textPrimary);
    g.setFont (font (19.5f, FontWeight::black, 0.13f));
    g.drawText (profile.codeName, juce::Rectangle<float> (16.0f, 41.0f, 86.0f, 27.0f),
                juce::Justification::centredLeft, false);

    g.setColour (crimsonAccent.withAlpha (0.80f));
    g.fillRect (103.0f, 43.0f, 1.0f, 26.0f);

    g.setColour (textPrimary);
    g.setFont (font (8.2f, FontWeight::semibold, 0.15f));
    g.drawText (profile.title, juce::Rectangle<float> (111.0f, 43.0f, panel.getWidth() - 127.0f, 12.0f),
                juce::Justification::centredLeft, false);

    g.setColour (textMuted);
    g.setFont (font (6.1f, FontWeight::regular, 0.03f));
    g.drawText (profile.tagline, juce::Rectangle<float> (111.0f, 57.0f, panel.getWidth() - 127.0f, 11.0f),
                juce::Justification::centredLeft, false);

    // Fixed curve stats.
    const auto stats = juce::Rectangle<float> (16.0f, 76.0f, panel.getWidth() - 32.0f, 27.0f);
    g.setColour (obsidianWell);
    g.fillRoundedRectangle (stats, 3.0f);
    g.setColour (glassRimHighlight.withAlpha (0.12f));
    g.drawRoundedRectangle (stats.reduced (0.4f), 3.0f, 0.6f);

    const juce::String statLabels[4] { "ATTACK", "RELEASE", "RATIO", "AUTO GAIN" };
    const juce::String statValues[4] {
        formatMs (profile.attackMs),
        formatMs (profile.releaseMs),
        formatRatio (profile.ratio),
        formatPercent (profile.autoMakeup)
    };

    for (int i = 0; i < 4; ++i)
    {
        const float cellWidth = stats.getWidth() / 4.0f;
        const auto cell = stats.withX (stats.getX() + static_cast<float> (i) * cellWidth)
                               .withWidth (cellWidth);

        if (i > 0)
        {
            g.setColour (obsidianGrid.brighter (0.08f));
            g.fillRect (cell.getX(), stats.getY() + 5.0f, 1.0f, stats.getHeight() - 10.0f);
        }

        g.setColour (textMuted);
        g.setFont (font (5.2f, FontWeight::semibold, 0.16f));
        g.drawText (statLabels[i], cell.withTrimmedTop (4.0f).withHeight (8.0f),
                    juce::Justification::centred, false);

        g.setColour (textSecondary);
        g.setFont (font (6.8f, FontWeight::semibold, 0.02f));
        g.drawText (statValues[i], cell.withTrimmedTop (13.0f).withHeight (11.0f),
                    juce::Justification::centred, false);
    }

    // Live meters.
    const float meterLeft = 16.0f;
    const float meterWidth = panel.getWidth() - 32.0f;
    const float inputNorm = juce::jlimit (0.0f, 1.0f, (displayInputDb + 48.0f) / 48.0f);
    const float outputNorm = juce::jlimit (0.0f, 1.0f, (displayOutputDb + 48.0f) / 48.0f);
    const float grNorm = juce::jlimit (0.0f, 1.0f, displayGainReduction / 18.0f);

    drawMeter (g, { meterLeft, 112.0f, meterWidth, 16.0f }, "INPUT", inputNorm,
               formatDb (displayInputDb), textSecondary.interpolatedWith (crimsonAccent, 0.55f));
    drawMeter (g, { meterLeft, 132.0f, meterWidth, 16.0f }, "GAIN RED.", grNorm,
               displayGainReduction > 0.05f ? "-" + juce::String (displayGainReduction, 1) + " dB" : "0.0 dB",
               crimsonBright);
    drawMeter (g, { meterLeft, 152.0f, meterWidth, 16.0f }, "OUTPUT", outputNorm,
               formatDb (displayOutputDb), crimsonAccent);

    // Footer status.
    g.setColour (obsidianGrid.withAlpha (0.85f));
    g.fillRect (16.0f, 176.0f, panel.getWidth() - 32.0f, 0.7f);

    g.setColour (crimsonAccent);
    g.fillEllipse (17.0f, 183.0f, 2.2f, 2.2f);

    g.setColour (textMuted);
    g.setFont (font (5.5f, FontWeight::regular, 0.13f));
    g.drawText ("VOICE THRESHOLD " + formatDb (processor.getThresholdDb()),
                juce::Rectangle<float> (24.0f, 179.0f, 130.0f, 10.0f),
                juce::Justification::centredLeft, false);

    g.drawText ("STEREO LINKED / ZERO LATENCY",
                juce::Rectangle<float> (panel.getWidth() - 197.0f, 179.0f, 181.0f, 10.0f),
                juce::Justification::centredRight, false);
}
} // namespace macpressor::ui
