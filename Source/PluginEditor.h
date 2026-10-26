#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "ui/Controls.h"
#include "ui/EnginePanel.h"
#include "ui/StudioLookAndFeel.h"
#include "ui/WaveformDisplay.h"

class MacPressorAudioProcessor;

class MacPressorContent final : public juce::Component
{
public:
    explicit MacPressorContent (MacPressorAudioProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

    void setMode (int modeIndex);

    macpressor::ui::HeaderPillBar header;
    macpressor::ui::WaveformDisplay waveform;
    macpressor::ui::ModeSwitch instrumentSwitch;
    macpressor::ui::ModeSwitch characterSwitch;
    macpressor::ui::EnginePanel engine;

private:
    MacPressorAudioProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MacPressorContent)
};

class MacPressorAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                             private juce::Timer
{
public:
    explicit MacPressorAudioProcessorEditor (MacPressorAudioProcessor&);
    ~MacPressorAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int designWidth = 1180;
    static constexpr int designHeight = 720;

private:
    void timerCallback() override;
    void selectMode (int index);
    void cycleMode (int delta);

    MacPressorAudioProcessor& processor;
    macpressor::ui::StudioLookAndFeel lookAndFeel;
    MacPressorContent content;

    int lastKnownMode = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MacPressorAudioProcessorEditor)
};
