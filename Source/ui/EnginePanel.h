#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class MacPressorAudioProcessor;

namespace macpressor::ui
{
/** Voice-specific fixed engine readout plus input / GR / output telemetry. */
class EnginePanel final : public juce::Component,
                          private juce::Timer
{
public:
    explicit EnginePanel (MacPressorAudioProcessor&);
    ~EnginePanel() override = default;

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    void drawMeter (juce::Graphics&, juce::Rectangle<float>, const juce::String& label,
                    float normalised, const juce::String& value, juce::Colour fill) const;

    MacPressorAudioProcessor& processor;

    float displayInputDb = -60.0f;
    float displayOutputDb = -60.0f;
    float displayGainReduction = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnginePanel)
};
} // namespace macpressor::ui
