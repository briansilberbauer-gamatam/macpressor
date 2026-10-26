#include "dsp/AutoCompressor.h"
#include "dsp/VoiceProfiles.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>
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

struct RunStats
{
    bool finite = true;
    float maxGainReductionDb = 0.0f;
    float inputPeak = 0.0f;
    float outputPeak = 0.0f;
    float lastGainReductionDb = 0.0f;
};

RunStats runVoice (macpressor::dsp::Voice voice, float amplitude, double seconds,
                   double sampleRate, int blockSize = 256)
{
    using namespace macpressor::dsp;

    AutoCompressor compressor;
    compressor.prepare (sampleRate, blockSize, 2);
    compressor.setVoice (voice, true);

    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::int64 offset = 0;
    RunStats stats;

    const auto blocks = static_cast<int> (std::ceil (seconds * sampleRate / static_cast<double> (blockSize)));

    for (int block = 0; block < blocks; ++block)
    {
        fillTone (buffer, offset, amplitude, 220.0, sampleRate);
        stats.inputPeak = juce::jmax (stats.inputPeak, buffer.getMagnitude (0, buffer.getNumSamples()));
        compressor.process (buffer);

        stats.finite = stats.finite && allFinite (buffer);
        stats.maxGainReductionDb = juce::jmax (stats.maxGainReductionDb,
                                               compressor.getGainReductionDb());
        stats.outputPeak = juce::jmax (stats.outputPeak, buffer.getMagnitude (0, buffer.getNumSamples()));
        stats.lastGainReductionDb = compressor.getGainReductionDb();
        offset += blockSize;
    }

    return stats;
}
} // namespace

int main()
{
    using namespace macpressor::dsp;

    std::cout << "MacPressor DSP tests\n";
    std::cout << "--------------------\n";

    {
        AutoCompressor compressor;
        compressor.prepare (48000.0, 512, 2);
        compressor.setVoice (Voice::vocalFemale, true);

        juce::AudioBuffer<float> silence (2, 512);
        silence.clear();
        compressor.process (silence);

        check (allFinite (silence), "silence produces finite samples");
        check (silence.getMagnitude (0, 512) < 1.0e-9f, "silence remains silent");
        check (compressor.getGainReductionDb() < 1.0e-6f, "silence reports no gain reduction");
    }

    for (int voiceIndex = 0; voiceIndex < numVoices; ++voiceIndex)
    {
        const auto voice = static_cast<Voice> (voiceIndex);
        const auto& profile = getVoiceProfile (voice);
        const auto stats = runVoice (voice, 0.62f, 1.2, 48000.0);

        check (stats.finite, std::string ("voice ") + profile.title + " produces finite output");
        check (stats.maxGainReductionDb > 2.0f,
               std::string ("voice ") + profile.title + " applies musical gain reduction");
        check (stats.outputPeak < stats.inputPeak * 0.92f,
               std::string ("voice ") + profile.title + " reduces peak level");
    }

    {
        const auto quiet = runVoice (Voice::vocalFemale, 0.006f, 0.8, 48000.0);
        check (quiet.maxGainReductionDb < 0.25f, "quiet material passes without compression");
    }

    {
        AutoCompressor compressor;
        compressor.prepare (48000.0, 256, 2);
        compressor.setVoice (Voice::guitarOverdrive, true);

        juce::AudioBuffer<float> buffer (2, 256);
        juce::int64 offset = 0;

        for (int block = 0; block < 200; ++block)
        {
            fillTone (buffer, offset, 0.75f, 220.0, 48000.0);
            compressor.process (buffer);
            offset += 256;
        }

        const float heldReduction = compressor.getGainReductionDb();

        for (int block = 0; block < 200; ++block)
        {
            buffer.clear();
            compressor.process (buffer);
        }

        check (heldReduction > 3.0f, "loud material engages the compressor");
        check (compressor.getGainReductionDb() < 0.25f, "gain reduction recovers after the source stops");
    }

    {
        AutoCompressor compressor;
        compressor.prepare (48000.0, 128, 2);
        compressor.setVoice (Voice::vocalMale, true);

        juce::AudioBuffer<float> buffer (2, 128);
        fillTone (buffer, 0, 0.55f, 196.0, 48000.0);
        compressor.process (buffer);

        float maxDifference = 0.0f;
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            maxDifference = juce::jmax (maxDifference,
                                        std::abs (buffer.getSample (0, i) - buffer.getSample (1, i)));

        check (maxDifference < 1.0e-7f, "stereo linking preserves an identical stereo image");
    }

    {
        AutoCompressor compressor;
        compressor.prepare (48000.0, 256, 2);
        compressor.setVoice (Voice::vocalFemale, true);

        juce::AudioBuffer<float> buffer (2, 256);
        juce::int64 offset = 0;
        bool finite = true;
        float maxStep = 0.0f;
        float previous = 0.0f;

        for (int block = 0; block < 240; ++block)
        {
            if (block == 60)
                compressor.setVoice (Voice::guitarOverdrive);
            if (block == 120)
                compressor.setVoice (Voice::vocalMale);
            if (block == 180)
                compressor.setVoice (Voice::guitarClean);

            fillTone (buffer, offset, 0.50f, 220.0, 48000.0);
            compressor.process (buffer);
            finite = finite && allFinite (buffer);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const float sample = buffer.getSample (0, i);
                maxStep = juce::jmax (maxStep, std::abs (sample - previous));
                previous = sample;
            }

            offset += 256;
        }

        check (finite, "live voice switching produces finite output");
        check (maxStep < 0.5f, "voice switching does not click");
    }

    {
        const auto stats44 = runVoice (Voice::guitarClean, 0.55f, 0.8, 44100.0, 128);
        const auto stats96 = runVoice (Voice::guitarClean, 0.55f, 0.8, 96000.0, 512);

        check (stats44.finite && stats96.finite, "44.1 kHz and 96 kHz processing stays finite");
        check (stats44.maxGainReductionDb > 1.0f && stats96.maxGainReductionDb > 1.0f,
               "compression works at 44.1 kHz and 96 kHz");
    }

    {
        AutoCompressor compressor;
        compressor.prepare (48000.0, 256, 2);
        compressor.setVoice (Voice::guitarClean, true);

        juce::AudioBuffer<float> dc (2, 256);

        for (int block = 0; block < 400; ++block)
        {
            for (int ch = 0; ch < dc.getNumChannels(); ++ch)
                juce::FloatVectorOperations::fill (dc.getWritePointer (ch), 1.0f, dc.getNumSamples());

            compressor.process (dc);
        }

        check (dc.getMagnitude (0, 256) < 0.01f, "DC blocker removes a constant input offset");
    }

    if (failures == 0)
        std::cout << "\nAll MacPressor DSP tests passed.\n";
    else
        std::cout << "\n" << failures << " MacPressor DSP test(s) failed.\n";

    return failures == 0 ? 0 : 1;
}
