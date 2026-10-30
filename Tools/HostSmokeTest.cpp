#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <cmath>
#include <iostream>

namespace
{
int failures = 0;

void check (bool condition, const char* message)
{
    std::cout << (condition ? "  PASS  " : "  FAIL  ") << message << '\n';

    if (! condition)
        ++failures;
}

void fillTone (juce::AudioBuffer<float>& buffer, int offset)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto n = static_cast<double> (offset + i);
            const double phase = juce::MathConstants<double>::twoPi * 196.0 * n / 48000.0;
            buffer.setSample (ch, i, 0.32f * static_cast<float> (std::sin (phase)
                                                                + 0.24 * std::sin (2.0 * phase)));
        }
    }
}

bool allFinite (const juce::AudioBuffer<float>& buffer)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (! std::isfinite (buffer.getReadPointer (ch)[i]))
                return false;

    return true;
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    if (argc < 2)
    {
        std::cerr << "Usage: MacPressorHostSmokeTest /path/to/MacPressor.vst3\n";
        return 2;
    }

    const juce::File pluginFile { juce::String (argv[1]) };

    if (! pluginFile.exists())
    {
        std::cerr << "Plugin bundle not found: " << pluginFile.getFullPathName() << '\n';
        return 2;
    }

    juce::AudioPluginFormatManager formatManager;
    formatManager.addDefaultFormats();

    juce::OwnedArray<juce::PluginDescription> descriptions;

    for (auto* format : formatManager.getFormats())
        if (auto* vst3 = dynamic_cast<juce::VST3PluginFormat*> (format))
            vst3->findAllTypesForFile (descriptions, pluginFile.getFullPathName());

    check (! descriptions.isEmpty(), "VST3 scanner discovers the plugin");

    if (descriptions.isEmpty())
        return 1;

    auto& description = *descriptions[0];

    std::cout << "  INFO  " << description.name << " / " << description.manufacturerName
              << " / " << description.version << '\n';

    juce::String error;
    auto instance = formatManager.createPluginInstance (description, 48000.0, 512, error);

    check (instance != nullptr, "plugin instance can be created");

    if (instance == nullptr)
    {
        std::cerr << "  ERROR " << error << '\n';
        return 1;
    }

    auto& parameters = instance->getParameters();

    std::cout << "  INFO  Host parameter count: " << parameters.size() << '\n';
    for (int i = 0; i < parameters.size(); ++i)
        std::cout << "  INFO    [" << i << "] " << parameters[i]->getName (64)
                  << " value=" << parameters[i]->getValue()
                  << " steps=" << parameters[i]->getNumSteps() << '\n';

    int voiceParameterIndex = -1;
    int bypassParameterCount = 0;

    for (int i = 0; i < parameters.size(); ++i)
    {
        if (parameters[i]->getName (64).containsIgnoreCase ("Voice"))
            voiceParameterIndex = i;
        if (parameters[i]->getName (64).containsIgnoreCase ("Bypass"))
            ++bypassParameterCount;
    }

    // VST3 itself requires a bypass parameter. Apart from that host-level
    // switch, the plugin exposes exactly one control: the voice selector.
    check (parameters.size() == 2 && voiceParameterIndex >= 0 && bypassParameterCount == 1,
           "apart from VST3 bypass, the host sees only the voice switch");
    check (voiceParameterIndex >= 0, "the host parameter list contains the voice selector");
    check (instance->getLatencySamples() == 0, "the compressor reports zero latency");
    check (instance->getName().containsIgnoreCase ("MacPressor"), "plugin name is MacPressor");

    instance->setPlayConfigDetails (2, 2, 48000.0, 512);
    instance->prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    bool finite = true;

    if (voiceParameterIndex >= 0)
    {
        auto& voiceParameter = *parameters[voiceParameterIndex];

        for (int mode = 0; mode < 4; ++mode)
        {
            voiceParameter.setValueNotifyingHost (static_cast<float> (mode) / 3.0f);

            for (int block = 0; block < 12; ++block)
            {
                fillTone (buffer, (mode * 12 + block) * 512);
                instance->processBlock (buffer, midi);
                finite = finite && allFinite (buffer);
            }
        }

        check (finite, "all four voice modes produce finite output through the VST3 wrapper");

        // The DSP tests exercise the compressor class directly. This check goes
        // through the real VST3 wrapper, so a wrapper/host problem that leaves the
        // signal dry (for example a stuck host bypass) fails the release build.
        voiceParameter.setValueNotifyingHost (1.0f);

        float inputPeak = 0.0f;
        float outputPeak = 0.0f;

        for (int block = 0; block < 160; ++block)
        {
            fillTone (buffer, (4 * 12 + block) * 512);
            inputPeak = juce::jmax (inputPeak, buffer.getMagnitude (0, buffer.getNumSamples()));

            instance->processBlock (buffer, midi);

            outputPeak = juce::jmax (outputPeak, buffer.getMagnitude (0, buffer.getNumSamples()));
            finite = finite && allFinite (buffer);
        }

        check (finite, "compression through the VST3 wrapper stays finite");
        check (outputPeak < inputPeak * 0.90f,
               "the compressor reduces peak level through the VST3 wrapper");

        voiceParameter.beginChangeGesture();
        voiceParameter.setValueNotifyingHost (1.0f);
        voiceParameter.endChangeGesture();

        juce::MemoryBlock state;
        instance->getStateInformation (state);
        check (state.getSize() > 0, "state can be serialised for the host");

        voiceParameter.setValueNotifyingHost (0.0f);
        instance->setStateInformation (state.getData(), static_cast<int> (state.getSize()));

        check (voiceParameter.getValue() > 0.95f,
               "state round-trip restores the Guitar Overdrive voice");
    }

    instance->releaseResources();

    std::cout << (failures == 0 ? "\nHost smoke test passed.\n" : "\nHost smoke test failed.\n");
    return failures == 0 ? 0 : 1;
}
