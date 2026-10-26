#include "PluginEditor.h"

#include "PluginProcessor.h"
#include "Version.h"
#include "dsp/VoiceProfiles.h"
#include "ui/Theme.h"

#include <cmath>

namespace
{
void drawBrandMark (juce::Graphics& g, juce::Point<float> origin)
{
    using namespace macpressor::ui;

    const float heights[4] { 11.0f, 23.0f, 16.0f, 27.0f };
    const float alphas[4] { 0.52f, 0.90f, 0.66f, 1.0f };

    for (int i = 0; i < 4; ++i)
    {
        g.setColour (crimsonAccent.withAlpha (alphas[i]));
        g.fillRoundedRectangle (origin.x + static_cast<float> (i) * 7.0f,
                                origin.y + (27.0f - heights[i]), 3.8f, heights[i], 1.4f);

        g.setColour (crimsonGlow.withAlpha (alphas[i] * 0.28f));
        g.fillRoundedRectangle (origin.x + static_cast<float> (i) * 7.0f - 1.4f,
                                origin.y + (27.0f - heights[i]) - 1.0f,
                                6.6f, heights[i] + 2.0f, 2.2f);
    }
}
} // namespace

MacPressorContent::MacPressorContent (MacPressorAudioProcessor& processorToUse)
    : header(),
      waveform (processorToUse),
      instrumentSwitch ("INSTRUMENT", "SOURCE SELECT", "01"),
      characterSwitch ("CHARACTER", "VOICE CURVE", "02"),
      engine (processorToUse),
      processor (processorToUse)
{
    addAndMakeVisible (header);
    addAndMakeVisible (waveform);
    addAndMakeVisible (instrumentSwitch);
    addAndMakeVisible (characterSwitch);
    addAndMakeVisible (engine);

    instrumentSwitch.setOptions ("VOCAL", "GUITAR");
    instrumentSwitch.setHint ("VOCAL OR GUITAR");
    characterSwitch.setHint ("SOURCE-MATCHED CURVE");
}

void MacPressorContent::setMode (int modeIndex)
{
    using namespace macpressor::dsp;

    const int mode = juce::jlimit (0, numVoices - 1, modeIndex);
    const auto& profile = getVoiceProfile (mode);
    const bool guitar = mode >= static_cast<int> (Voice::guitarClean);

    instrumentSwitch.setSelectedIndex (guitar ? 1 : 0);
    characterSwitch.setOptions (guitar ? "CLEAN" : "FEMALE", guitar ? "OVERDRIVE" : "MALE");
    characterSwitch.setSelectedIndex (mode & 1);

    header.setModeText (profile.title,
                        "MACPRESSOR  //  " + juce::String (profile.codeName)
                            + " ENGINE  /  0" + juce::String (mode + 1) + " OF 04");
}

void MacPressorContent::paint (juce::Graphics& g)
{
    using namespace macpressor::ui;

    drawDesktopHardwareChassis (g, getLocalBounds().toFloat());

        // Top-left brand lock-up.
    drawBrandMark (g, { 96.0f, 58.0f });

    g.setColour (textPrimary);
    g.setFont (font (27.0f, FontWeight::black, 0.16f));
    g.drawText ("MACPRESSOR", juce::Rectangle<float> (128.0f, 48.0f, 270.0f, 34.0f),
                juce::Justification::centredLeft, false);

    g.setColour (textSecondary.withAlpha (0.78f));
    g.setFont (font (6.8f, FontWeight::regular, 0.17f));
    g.drawText ("FOUR-VOICE AUTO COMPRESSOR  /  ZERO SETUP",
                juce::Rectangle<float> (129.0f, 82.0f, 270.0f, 10.0f),
                juce::Justification::centredLeft, false);

    g.setColour (crimsonAccent.withAlpha (0.74f));
    g.fillRect (96.0f, 105.0f, 36.0f, 1.2f);

    // Top-right house mark.
    g.setColour (textSecondary);
    g.setFont (font (7.2f, FontWeight::semibold, 0.21f));
    g.drawText ("BLS AUDIO", juce::Rectangle<float> (900.0f, 50.0f, 192.0f, 11.0f),
                juce::Justification::centredRight, false);

    g.setColour (textMuted);
    g.setFont (font (5.8f, FontWeight::regular, 0.13f));
    g.drawText ("64-BIT VST3 / " + juce::String ("v") + macpressor::version,
                juce::Rectangle<float> (900.0f, 64.0f, 192.0f, 10.0f), juce::Justification::centredRight, false);
    g.drawText ("SERIES MAC / ZERO-SETUP DYNAMICS",
                juce::Rectangle<float> (900.0f, 77.0f, 192.0f, 10.0f), juce::Justification::centredRight, false);

    // Fine divider between the header and the scope.
    g.setColour (glassRimHighlight.withAlpha (0.12f));
    g.fillRect (96.0f, 114.0f, 988.0f, 0.6f);

    // Footer signal path.
    const float footerY = 662.0f;

    g.setColour (glassRimHighlight.withAlpha (0.12f));
    g.fillRect (96.0f, footerY - 10.0f, 988.0f, 0.6f);

    drawSignalChip (g, { 88.0f, footerY, 104.0f, 18.0f }, "AUTO DETECT", true);
    drawSignalChip (g, { 200.0f, footerY, 96.0f, 18.0f }, "VOICE CURVE", true);
    drawSignalChip (g, { 304.0f, footerY, 96.0f, 18.0f }, "AUTO GAIN", true);
    drawSignalChip (g, { 408.0f, footerY, 96.0f, 18.0f }, "SAFE OUT", false);

    g.setColour (textMuted);
    g.setFont (font (5.7f, FontWeight::regular, 0.12f));
    g.drawText ("SIGNAL PATH  /  INPUT  >  VOICE-TUNED DETECTOR  >  VOICE CURVE  >  AUTO MAKE-UP  >  OUTPUT",
                juce::Rectangle<float> (536.0f, footerY + 1.0f, 352.0f, 16.0f),
                juce::Justification::centredLeft, false);

    g.setColour (textMuted);
    g.setFont (font (5.8f, FontWeight::semibold, 0.15f));
    g.drawText ("BLS AUDIO / MACPRESSOR " + juce::String ("v") + macpressor::version,
                juce::Rectangle<float> (904.0f, footerY + 1.0f, 188.0f, 16.0f),
                juce::Justification::centredRight, false);

}

void MacPressorContent::resized()
{
    header.setBounds (398, 55, 384, 48);
    waveform.setBounds (88, 124, 1004, 306);
    instrumentSwitch.setBounds (88, 452, 300, 196);
    characterSwitch.setBounds (404, 452, 300, 196);
    engine.setBounds (720, 452, 372, 196);
}

MacPressorAudioProcessorEditor::MacPressorAudioProcessorEditor (MacPressorAudioProcessor& processorToUse)
    : AudioProcessorEditor (&processorToUse),
      processor (processorToUse),
      lookAndFeel(),
      content (processorToUse)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (content);

    setResizable (true, true);
    setResizeLimits (885, 540, 1770, 1080);

    if (auto* aspectConstrainer = getConstrainer())
        aspectConstrainer->setFixedAspectRatio (static_cast<double> (designWidth)
                                          / static_cast<double> (designHeight));

    setSize (designWidth, designHeight);

    content.instrumentSwitch.onSelect = [this] (int option)
    {
        const int currentMode = processor.getModeIndex();
        selectMode ((option == 0 ? 0 : 2) + (currentMode & 1));
    };

    content.characterSwitch.onSelect = [this] (int option)
    {
        const int currentMode = processor.getModeIndex();
        selectMode ((currentMode >= 2 ? 2 : 0) + option);
    };

    content.header.onCycle = [this] (int delta)
    {
        cycleMode (delta);
    };

    lastKnownMode = processor.getModeIndex();
    content.setMode (lastKnownMode);

    startTimerHz (30);
}

MacPressorAudioProcessorEditor::~MacPressorAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MacPressorAudioProcessorEditor::selectMode (int index)
{
    const int clamped = juce::jlimit (0, macpressor::dsp::numVoices - 1, index);

    if (clamped == processor.getModeIndex())
        return;

    processor.setModeIndex (clamped);
    lastKnownMode = clamped;
    content.setMode (clamped);
}

void MacPressorAudioProcessorEditor::cycleMode (int delta)
{
    const int next = (processor.getModeIndex() + delta + macpressor::dsp::numVoices)
                   % macpressor::dsp::numVoices;
    selectMode (next);
}

void MacPressorAudioProcessorEditor::timerCallback()
{
    const int mode = processor.getModeIndex();

    if (mode != lastKnownMode)
    {
        lastKnownMode = mode;
        content.setMode (mode);
    }
}

void MacPressorAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff05070a));
}

void MacPressorAudioProcessorEditor::resized()
{
    const float scale = juce::jmin (static_cast<float> (getWidth()) / static_cast<float> (designWidth),
                                    static_cast<float> (getHeight()) / static_cast<float> (designHeight));
    const float offsetX = (static_cast<float> (getWidth()) - static_cast<float> (designWidth) * scale) * 0.5f;
    const float offsetY = (static_cast<float> (getHeight()) - static_cast<float> (designHeight) * scale) * 0.5f;

    content.setBounds (0, 0, designWidth, designHeight);
    content.setTransform (juce::AffineTransform::scale (scale).translated (offsetX, offsetY));
}
