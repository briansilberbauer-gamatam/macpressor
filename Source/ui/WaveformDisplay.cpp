#include "WaveformDisplay.h"

#include "PluginProcessor.h"
#include "Theme.h"

#include <cmath>

namespace macpressor::ui
{
WaveformDisplay::WaveformDisplay (MacPressorAudioProcessor& processorToUse)
    : processor (processorToUse)
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (30);
}

void WaveformDisplay::timerCallback()
{
    const float targetReduction = processor.getGainReductionDb();

    if (targetReduction > displayGainReduction)
        displayGainReduction = targetReduction;
    else
        displayGainReduction = juce::jmax (0.0f, displayGainReduction - 0.65f);

    pullNewSamples();
    repaint();
}

void WaveformDisplay::pullNewSamples()
{
    const uint32_t writeIndex = processor.getWaveformWriteIndex();

    if (writeIndex == lastWriteIndex)
        return;

    if (writeIndex < lastWriteIndex)
        historyCount = 0;

    lastWriteIndex = writeIndex;

    const int available = static_cast<int> (writeIndex);
    const int count = juce::jlimit (0, historySize, available);

    if (count <= 0)
    {
        historyCount = 0;
        return;
    }

    const uint32_t start = writeIndex - static_cast<uint32_t> (count);

    for (int i = 0; i < count; ++i)
        history[static_cast<size_t> (i)] = processor.getWaveformSample (start + static_cast<uint32_t> (i));

    historyCount = count;
}

void WaveformDisplay::paint (juce::Graphics& g)
{
    const auto screen = getLocalBounds().toFloat().reduced (1.0f);

    if (screen.getWidth() < 20.0f || screen.getHeight() < 20.0f)
        return;

    constexpr float corner = 7.0f;

    // Recessed screen under the glass.
    g.setColour (dropShadowDeep.withAlpha (0.70f));
    g.fillRoundedRectangle (screen.translated (0.0f, 2.0f), corner + 1.0f);

    g.setColour (obsidianScreen);
    g.fillRoundedRectangle (screen, corner);

    juce::ColourGradient screenGradient (juce::Colour (0xff0b0d11), screen.getTopLeft(),
                                         juce::Colour (0xff06070a), screen.getBottomRight(), false);
    g.setGradientFill (screenGradient);
    g.fillRoundedRectangle (screen.reduced (1.2f), corner - 1.0f);

    g.setColour (crimsonAccent.withAlpha (0.47f));
    g.drawRoundedRectangle (screen, corner, 1.15f);
    g.setColour (glassRimHighlight.withAlpha (0.16f));
    g.drawRoundedRectangle (screen.reduced (1.2f), corner - 1.2f, 0.7f);

    const auto inner = screen.reduced (3.0f);
    const auto header = inner.withHeight (42.0f);
    const auto footer = inner.withTop (inner.getBottom() - 27.0f);
    auto plot = inner.withTrimmedTop (46.0f).withTrimmedBottom (32.0f);
    const auto scaleArea = plot.removeFromLeft (43.0f).reduced (4.0f, 0.0f);
    plot = plot.withTrimmedRight (8.0f);

    constexpr float minDb = -36.0f;
    constexpr float maxDb = 6.0f;
    const float dbRange = maxDb - minDb;

    auto dbToY = [&] (float db)
    {
        const float normalised = juce::jlimit (0.0f, 1.0f, (maxDb - db) / dbRange);
        return plot.getY() + normalised * plot.getHeight();
    };

    {
        juce::Graphics::ScopedSaveState state (g);
        juce::Path screenClip;
        screenClip.addRoundedRectangle (screen.reduced (1.0f), corner - 1.0f);
        g.reduceClipRegion (screenClip);

        // Grid.
        static constexpr float dbLines[] = { 6.0f, 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -30.0f, -36.0f };
        g.setFont (font (6.0f, FontWeight::regular, 0.06f));

        for (float db : dbLines)
        {
            const float y = dbToY (db);
            g.setColour (juce::approximatelyEqual (db, 0.0f) ? obsidianGrid.brighter (0.20f) : obsidianGrid);
            g.fillRect (plot.getX(), y, plot.getWidth(), 1.0f);

            g.setColour (textMuted);
            g.drawText (db > 0.0f ? "+6" : juce::String (juce::roundToInt (db)),
                        scaleArea.withTop (y - 6.0f).withHeight (12.0f),
                        juce::Justification::centredRight, false);
        }

        constexpr int verticalDivisions = 10;
        for (int i = 1; i < verticalDivisions; ++i)
        {
            const float x = plot.getX() + (static_cast<float> (i) / static_cast<float> (verticalDivisions))
                                              * plot.getWidth();
            g.setColour (obsidianGrid.withAlpha (i == verticalDivisions / 2 ? 0.90f : 0.56f));
            g.fillRect (x, plot.getY(), 1.0f, plot.getHeight());
        }

        // Auto-threshold reference line.
        const float thresholdDb = juce::jlimit (minDb, maxDb, processor.getThresholdDb());
        const float thresholdY = dbToY (thresholdDb);
        g.setColour (crimsonAccent.withAlpha (0.24f));
        for (float x = plot.getX(); x < plot.getRight(); x += 11.0f)
            g.fillRect (x, thresholdY, 5.0f, 0.8f);

        g.setColour (crimsonAccent.withAlpha (0.55f));
        g.setFont (font (5.6f, FontWeight::semibold, 0.14f));
        g.drawText ("THR " + formatDb (thresholdDb),
                    juce::Rectangle<float> (plot.getRight() - 82.0f, thresholdY - 10.0f, 74.0f, 9.0f),
                    juce::Justification::centredRight, false);

        // Waveform.
        const float midY = plot.getCentreY();
        const float halfHeight = plot.getHeight() * 0.455f;

        // The scope auto-scales so quiet and loud sources both read clearly.
        float displayPeak = 0.0f;
        for (int i = 0; i < historyCount; ++i)
            displayPeak = juce::jmax (displayPeak, std::abs (history[static_cast<size_t> (i)]));

        const float displayGain = juce::jlimit (1.2f, 5.0f, 0.82f / juce::jmax (0.015f, displayPeak));

        if (historyCount > 1)
        {
            const int numColumns = juce::jlimit (2, 220, historyCount);
            std::array<float, 220> maxSamples {};
            std::array<float, 220> minSamples {};

            for (int column = 0; column < numColumns; ++column)
            {
                const int start = (column * historyCount) / numColumns;
                const int end = juce::jmax (start + 1, ((column + 1) * historyCount) / numColumns);

                const auto firstSample = history[static_cast<size_t> (juce::jlimit (0, historyCount - 1, start))];
                float minimum = firstSample;
                float maximum = firstSample;

                for (int i = start + 1; i < end; ++i)
                {
                    const float sample = history[static_cast<size_t> (juce::jlimit (0, historyCount - 1, i))];
                    minimum = juce::jmin (minimum, sample);
                    maximum = juce::jmax (maximum, sample);
                }

                maxSamples[static_cast<size_t> (column)] = juce::jlimit (-1.0f, 1.0f, maximum * displayGain);
                minSamples[static_cast<size_t> (column)] = juce::jlimit (-1.0f, 1.0f, minimum * displayGain);
            }

            juce::Path fillPath;
            juce::Path topTrace;
            juce::Path bottomTrace;

            for (int column = 0; column < numColumns; ++column)
            {
                const float x = plot.getX()
                              + (static_cast<float> (column) / static_cast<float> (numColumns - 1))
                                    * plot.getWidth();
                const float yMax = midY - maxSamples[static_cast<size_t> (column)] * halfHeight;
                const float yMin = midY - minSamples[static_cast<size_t> (column)] * halfHeight;

                if (column == 0)
                {
                    fillPath.startNewSubPath (x, yMax);
                    topTrace.startNewSubPath (x, yMax);
                    bottomTrace.startNewSubPath (x, yMin);
                }
                else
                {
                    fillPath.lineTo (x, yMax);
                    topTrace.lineTo (x, yMax);
                    bottomTrace.lineTo (x, yMin);
                }
            }

            for (int column = numColumns - 1; column >= 0; --column)
            {
                const float x = plot.getX()
                              + (static_cast<float> (column) / static_cast<float> (numColumns - 1))
                                    * plot.getWidth();
                const float yMin = midY - minSamples[static_cast<size_t> (column)] * halfHeight;
                fillPath.lineTo (x, yMin);
            }

            fillPath.closeSubPath();

            juce::ColourGradient envelopeGradient (crimsonAccent.withAlpha (0.22f),
                                                   plot.getX(), midY - halfHeight,
                                                   crimsonGlow.withAlpha (0.035f),
                                                   plot.getX(), midY + halfHeight, false);
            g.setGradientFill (envelopeGradient);
            g.fillPath (fillPath);

            g.setColour (crimsonGlow.withAlpha (0.25f));
            g.strokePath (topTrace, juce::PathStrokeType (5.5f, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));
            g.strokePath (bottomTrace, juce::PathStrokeType (5.5f, juce::PathStrokeType::curved,
                                                             juce::PathStrokeType::rounded));

            g.setColour (crimsonAccent);
            g.strokePath (topTrace, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));
            g.strokePath (bottomTrace, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved,
                                                             juce::PathStrokeType::rounded));

            g.setColour (crimsonBright);
            g.strokePath (topTrace, juce::PathStrokeType (0.9f, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));
        }
        else
        {
            g.setColour (crimsonSubtle.withAlpha (0.55f));
            g.fillRect (plot.getX(), midY, plot.getWidth(), 0.8f);
        }

        // Glass reflection across the scope.
        juce::Path sheen;
        sheen.startNewSubPath (inner.getX(), inner.getY());
        sheen.lineTo (inner.getX() + inner.getWidth() * 0.41f, inner.getY());
        sheen.lineTo (inner.getX() + inner.getWidth() * 0.17f, inner.getBottom());
        sheen.lineTo (inner.getX(), inner.getBottom());
        sheen.closeSubPath();
        g.setColour (juce::Colours::white.withAlpha (0.022f));
        g.fillPath (sheen);
    }

    // Header readouts.
    g.setColour (crimsonAccent);
    g.fillRect (header.getX() + 10.0f, header.getY() + 14.0f, 2.0f, 9.0f);

    g.setColour (textPrimary);
    g.setFont (font (7.2f, FontWeight::semibold, 0.19f));
    g.drawText ("LIVE OUTPUT WAVEFORM", header.withTrimmedLeft (18.0f).withTrimmedTop (9.0f).withHeight (15.0f),
                juce::Justification::centredLeft, false);

    g.setColour (textMuted);
    g.setFont (font (5.8f, FontWeight::regular, 0.15f));
    g.drawText ("POST-DYNAMICS / MONO SUM", header.withTrimmedLeft (18.0f).withTrimmedTop (24.0f).withHeight (10.0f),
                juce::Justification::centredLeft, false);

    const auto rightHeader = header.withTrimmedRight (10.0f);

    g.setColour (textMuted);
    g.setFont (font (5.7f, FontWeight::semibold, 0.15f));
    g.drawText ("VOICE THRESHOLD", rightHeader.withTrimmedLeft (rightHeader.getWidth() - 330.0f)
                                      .withWidth (94.0f).withTrimmedTop (9.0f).withHeight (10.0f),
                juce::Justification::centredRight, false);
    g.setColour (textSecondary);
    g.setFont (font (8.3f, FontWeight::semibold, 0.04f));
    g.drawText (formatDb (processor.getThresholdDb()), rightHeader.withTrimmedLeft (rightHeader.getWidth() - 330.0f)
                                                       .withWidth (94.0f).withTrimmedTop (21.0f).withHeight (13.0f),
                juce::Justification::centredRight, false);

    g.setColour (textMuted);
    g.setFont (font (5.7f, FontWeight::semibold, 0.15f));
    g.drawText ("GAIN REDUCTION", rightHeader.withTrimmedLeft (rightHeader.getWidth() - 218.0f)
                                       .withWidth (110.0f).withTrimmedTop (9.0f).withHeight (10.0f),
                juce::Justification::centredRight, false);
    g.setColour (displayGainReduction > 0.25f ? crimsonBright : textSecondary);
    g.setFont (font (8.3f, FontWeight::semibold, 0.04f));
    g.drawText ("-" + juce::String (displayGainReduction, 1) + " dB",
                rightHeader.withTrimmedLeft (rightHeader.getWidth() - 218.0f)
                    .withWidth (110.0f).withTrimmedTop (21.0f).withHeight (13.0f),
                juce::Justification::centredRight, false);

    // Footer strip.
    g.setColour (obsidianGrid.withAlpha (0.80f));
    g.fillRect (footer.getX() + 10.0f, footer.getY() + 1.0f, footer.getWidth() - 20.0f, 0.7f);

    g.setColour (textMuted);
    g.setFont (font (5.8f, FontWeight::regular, 0.15f));
    g.drawText ("AUTOMATIC MAKE-UP / STEREO LINKED",
                footer.withTrimmedLeft (12.0f).withTrimmedTop (7.0f),
                juce::Justification::centredLeft, false);

    const double rate = processor.getSampleRate();
    const juce::String rateText = rate > 0.0 ? juce::String (rate / 1000.0, 1) + " kHz  /  "
                                                                   + juce::String (processor.getTotalNumOutputChannels())
                                                                   + " CH"
                                             : "NO AUDIO";

    g.drawText (rateText, footer.withTrimmedRight (12.0f).withTrimmedTop (7.0f),
                juce::Justification::centredRight, false);
}
} // namespace macpressor::ui
