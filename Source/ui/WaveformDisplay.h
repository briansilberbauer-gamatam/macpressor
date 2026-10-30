#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <cstdint>

class MacPressorAudioProcessor;

namespace macpressor::ui
{
/** Large recessed obsidian scope with a glowing crimson output waveform. */
class WaveformDisplay final : public juce::Component,
                              private juce::Timer
{
public:
    explicit WaveformDisplay (MacPressorAudioProcessor&);
    ~WaveformDisplay() override = default;

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    static constexpr int historySize = 1024;

    MacPressorAudioProcessor& processor;
    std::array<float, historySize> history {};
    int historyCount = 0;
    float displayGainReduction = 0.0f;
    int silentFrames = 0;
    bool hostBypassed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformDisplay)
};
} // namespace macpressor::ui
