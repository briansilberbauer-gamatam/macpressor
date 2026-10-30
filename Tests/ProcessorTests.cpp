#include "PluginProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <string>

namespace
{
int failures = 0;

void check (bool condition, const std::string& message)
{
    std::cout << (condition ? "  PASS  " : "  FAIL  ") << message << '\n';

    if (! condition)
        ++failures;
}

void fillTone (juce::AudioBuffer<float>& buffer, juce::int64 sampleOffset,
               float amplitude, double frequency, double sampleRate)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto n = static_cast<double> (sampleOffset + i);
            const double phase = juce::MathConstants<double>::twoPi * frequency * n / sampleRate;
            const float sample = amplitude * static_cast<float> (std::sin (phase)
                                                                + 0.22 * std::sin (2.0 * phase)
                                                                + 0.08 * std::sin (3.0 * phase));
            buffer.setSample (ch, i, sample);
        }
    }
}

bool allFinite (const juce::AudioBuffer<float>& buffer)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (! std::isfinite (buffer.getSample (ch, i)))
                return false;

    return true;
}

float peakOf (const std::array<float, 1024>& samples)
{
    float peak = 0.0f;

    for (const auto sample : samples)
        peak = juce::jmax (peak, std::abs (sample));

    return peak;
}

std::array<float, 1024> readScope (const MacPressorAudioProcessor& processor, int* countOut = nullptr)
{
    std::array<float, 1024> scope {};
    const int count = processor.copyLatestWaveformSamples (scope.data(), static_cast<int> (scope.size()));

    if (countOut != nullptr)
        *countOut = count;

    return scope;
}

struct StereoRun
{
    float inputPeak = 0.0f;
    float outputPeak = 0.0f;
    bool finite = true;
};

StereoRun runTone (MacPressorAudioProcessor& processor, juce::AudioBuffer<float>& buffer,
                   juce::int64& offset, int blocks, float amplitude = 0.62f)
{
    juce::MidiBuffer midi;
    StereoRun run;

    for (int block = 0; block < blocks; ++block)
    {
        fillTone (buffer, offset, amplitude, 220.0, 48000.0);
        run.inputPeak = juce::jmax (run.inputPeak, buffer.getMagnitude (0, buffer.getNumSamples()));

        processor.processBlock (buffer, midi);

        run.finite = run.finite && allFinite (buffer);
        run.outputPeak = juce::jmax (run.outputPeak, buffer.getMagnitude (0, buffer.getNumSamples()));
        offset += buffer.getNumSamples();
    }

    return run;
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    std::cout << "MacPressor processor tests\n";
    std::cout << "--------------------------\n";

    // A fresh instance must not pretend to have a waveform before audio arrives.
    {
        MacPressorAudioProcessor processor;
        processor.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        processor.prepareToPlay (sampleRate, blockSize);

        int count = -1;
        const auto scope = readScope (processor, &count);

        check (count == 0, "scope is empty before the first audio block");
        check (! processor.hasWaveformSamples(), "no valid waveform samples before the first block");
        check (! processor.isHostBypassed(), "plug-in is not host-bypassed on a normal block");
        check (std::abs (scope[0]) < 1.0e-9f, "empty scope reads as silence");
    }

    // Normal stereo operation: the compressor must act, and the scope must show
    // a full window of the processed signal.
    {
        MacPressorAudioProcessor processor;
        processor.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::int64 offset = 0;
        const auto run = runTone (processor, buffer, offset, 160);

        check (run.finite, "stereo processing stays finite");
        check (processor.getGainReductionDb() > 2.0f,
               "processor reports gain reduction for loud material");
        check (run.outputPeak < run.inputPeak * 0.92f,
               "processor reduces the output peak for loud material");

        int count = 0;
        const auto scope = readScope (processor, &count);

        check (count == 1024, "scope returns a full window after audio has run");
        check (peakOf (scope) > 0.02f, "scope carries the processed signal");

        // The UI may poll repeatedly between blocks; the window must stay valid
        // and unchanged instead of collapsing or clearing.
        int countAgain = 0;
        const auto scopeAgain = readScope (processor, &countAgain);

        check (countAgain == count, "scope window stays full between audio blocks");
        check (scope == scopeAgain, "scope window is stable while the audio thread is idle");
    }

    // The ring must keep returning the newest samples after it has wrapped, and
    // must follow the signal rather than replaying stale data.
    {
        MacPressorAudioProcessor processor;
        processor.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::int64 offset = 0;

        runTone (processor, buffer, offset, 40);

        check (processor.getWaveformValidSamples() == static_cast<uint32_t> (MacPressorAudioProcessor::waveformSize),
               "waveform ring reports a full history after wrapping");

        int count = 0;
        const auto loudScope = readScope (processor, &count);
        check (count == 1024, "scope stays full after the ring wraps");
        check (peakOf (loudScope) > 0.02f, "scope still carries audio after the ring wraps");

        juce::MidiBuffer midi;

        for (int block = 0; block < 12; ++block)
        {
            buffer.clear();
            processor.processBlock (buffer, midi);
        }

        const auto silentScope = readScope (processor, &count);
        check (count == 1024, "scope remains a full window when the source stops");
        check (peakOf (silentScope) < 1.0e-4f,
               "scope follows the newest (silent) samples instead of stale audio");
    }

    // Some Windows hosts report a mono main bus when they call prepareToPlay and
    // then hand the plug-in stereo buffers. Neither channel may be silenced and
    // the compressor must still act.
    {
        MacPressorAudioProcessor processor;
        processor.setPlayConfigDetails (1, 1, sampleRate, blockSize);
        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::int64 offset = 0;
        const auto run = runTone (processor, buffer, offset, 80);

        check (run.finite, "stereo buffers are processed after a mono prepare");
        check (buffer.getMagnitude (0, buffer.getNumSamples()) > 0.05f,
               "left channel is not silenced after a mono prepare");
        check (buffer.getMagnitude (1, buffer.getNumSamples()) > 0.05f,
               "right channel is not silenced after a mono prepare");
        check (processor.getOutputLevelRight() > 0.05f,
               "right-channel output meter is fed after a mono prepare");
        check (processor.getGainReductionDb() > 1.0f,
               "compression still engages after a mono prepare");
    }

    // Mono buffers are legal in a mono host and must be processed and scoped.
    {
        MacPressorAudioProcessor processor;
        processor.setPlayConfigDetails (1, 1, sampleRate, blockSize);
        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> buffer (1, blockSize);
        juce::MidiBuffer midi;
        juce::int64 offset = 0;
        bool finite = true;
        float outputPeak = 0.0f;

        for (int block = 0; block < 80; ++block)
        {
            fillTone (buffer, offset, 0.62f, 220.0, sampleRate);
            processor.processBlock (buffer, midi);
            finite = finite && allFinite (buffer);
            outputPeak = juce::jmax (outputPeak, buffer.getMagnitude (0, buffer.getNumSamples()));
            offset += blockSize;
        }

        check (finite, "mono processing stays finite");
        check (outputPeak > 0.05f, "mono audio is passed through");
        check (peakOf (readScope (processor)) > 0.02f, "mono audio feeds the scope");
    }

    // Large host buffers are common on Windows (ASIO 1024/2048 and offline
    // renders). A block larger than the display window must not overflow the ring.
    {
        constexpr int hugeBlock = 8192;

        MacPressorAudioProcessor processor;
        processor.setPlayConfigDetails (2, 2, sampleRate, hugeBlock);
        processor.prepareToPlay (sampleRate, hugeBlock);

        juce::AudioBuffer<float> buffer (2, hugeBlock);
        juce::MidiBuffer midi;
        fillTone (buffer, 0, 0.62f, 220.0, sampleRate);
        processor.processBlock (buffer, midi);

        check (allFinite (buffer), "a full-ring-sized block stays finite");
        check (processor.getWaveformValidSamples() == static_cast<uint32_t> (hugeBlock / 2),
               "a huge block publishes the expected decimated sample count");
        check (peakOf (readScope (processor)) > 0.02f, "a huge block feeds the scope");
    }

    // A host bypass must leave the audio byte-for-byte dry but keep the scope and
    // meters alive, so the editor can explain the bypass instead of looking dead.
    {
        MacPressorAudioProcessor processor;
        processor.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> buffer (2, blockSize);
        fillTone (buffer, 0, 0.62f, 220.0, sampleRate);

        juce::AudioBuffer<float> dryCopy (buffer);

        juce::MidiBuffer midi;
        processor.processBlockBypassed (buffer, midi);

        bool identical = true;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            identical = identical
                        && std::memcmp (buffer.getReadPointer (ch), dryCopy.getReadPointer (ch),
                                        sizeof (float) * static_cast<size_t> (buffer.getNumSamples())) == 0;

        check (identical, "host bypass leaves the audio untouched");
        check (processor.isHostBypassed(), "host bypass is reported to the editor");
        check (processor.getGainReductionDb() < 1.0e-6f, "host bypass reports zero gain reduction");
        check (peakOf (readScope (processor)) > 0.02f, "host bypass still feeds the scope");

        juce::int64 offset = 0;
        runTone (processor, buffer, offset, 4);

        check (! processor.isHostBypassed(), "a normal block clears the host bypass flag");
    }

    // Empty buffers and excessive channel counts must never crash or corrupt the
    // telemetry (some hosts probe plug-ins with zero-length buffers).
    {
        MacPressorAudioProcessor processor;
        processor.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> empty (2, 0);
        juce::MidiBuffer midi;
        processor.processBlock (empty, midi);
        processor.processBlockBypassed (empty, midi);

        check (processor.getWaveformValidSamples() == 0, "zero-length buffers do not publish samples");
        check (processor.getOutputLevel() < 1.0e-9f, "zero-length buffers do not publish output level");

        juce::AudioBuffer<float> surround (4, blockSize);
        fillTone (surround, 0, 0.5f, 220.0, sampleRate);
        processor.processBlock (surround, midi);

        check (surround.getMagnitude (2, 0, blockSize) < 1.0e-9f,
               "channels outside the stereo buses are cleared");
        check (surround.getMagnitude (0, 0, blockSize) > 0.02f,
               "the stereo buses are still processed with extra channels present");
    }

    if (failures == 0)
        std::cout << "\nAll MacPressor processor tests passed.\n";
    else
        std::cout << "\n" << failures << " MacPressor processor test(s) failed.\n";

    return failures == 0 ? 0 : 1;
}
