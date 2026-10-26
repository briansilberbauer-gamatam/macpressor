#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include <algorithm>
#include <array>
#include <cmath>

#include "VoiceProfiles.h"

namespace macpressor::dsp
{
/**
    A stereo-linked, fully automatic compressor.

    There is deliberately no threshold, ratio, attack, release or make-up
    control. The selected VoiceProfile supplies a fixed, musical curve for the
    source; switching voices crossfades the static curve and ballistics so the
    change never clicks. Automatic make-up gain tracks the gain reduction.
*/
class AutoCompressor
{
public:
    AutoCompressor() = default;

    void prepare (double newSampleRate, int /*maximumBlockSize*/, int newNumChannels)
    {
        sampleRate = std::max (8000.0, newSampleRate);
        numChannels = juce::jlimit (1, 2, newNumChannels);

        threshold.reset (sampleRate, 0.055);
        ratio.reset (sampleRate, 0.055);
        knee.reset (sampleRate, 0.055);
        parallel.reset (sampleRate, 0.075);
        autoMakeup.reset (sampleRate, 0.075);
        trim.reset (sampleRate, 0.075);
        colour.reset (sampleRate, 0.075);

        makeupCoeff = onePoleCoeffAtRate (250.0f);
        dcCoeff = static_cast<float> (std::exp (-juce::MathConstants<double>::twoPi * 10.0 / sampleRate));

        setVoice (voice, true);
        reset();
    }

    void reset()
    {
        detectorEnvelope = 0.0f;
        makeupDb = 0.0f;
        lastThresholdDb = threshold.getCurrentValue();
        lastGainReductionDb = 0.0f;

        for (int ch = 0; ch < 2; ++ch)
        {
            hpLp1[static_cast<size_t> (ch)] = 0.0f;
            hpLp2[static_cast<size_t> (ch)] = 0.0f;
            dcX[static_cast<size_t> (ch)] = 0.0f;
            dcY[static_cast<size_t> (ch)] = 0.0f;
        }
    }

    void setVoice (Voice newVoice, bool immediate = false)
    {
        voice = newVoice;
        const auto& profile = getVoiceProfile (voice);

        auto set = [immediate] (auto& smoother, float value)
        {
            if (immediate)
                smoother.setCurrentAndTargetValue (value);
            else
                smoother.setTargetValue (value);
        };

        set (threshold, profile.thresholdDb);
        set (ratio, profile.ratio);
        set (knee, profile.kneeDb);
        set (parallel, profile.parallelMix);
        set (autoMakeup, profile.autoMakeup);
        set (trim, profile.trimDb);
        set (colour, profile.colour);

        attackCoeff = onePoleCoeffAtRate (profile.attackMs);
        releaseCoeff = onePoleCoeffAtRate (profile.releaseMs);
        hpfCoeff = onePoleCutoffCoeffAtRate (profile.detectorHpfHz);

        if (immediate)
            lastThresholdDb = profile.thresholdDb;
    }

    Voice getVoice() const noexcept { return voice; }
    float getGainReductionDb() const noexcept { return lastGainReductionDb; }
    float getThresholdDb() const noexcept { return lastThresholdDb; }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const int activeChannels = std::min (numChannels, buffer.getNumChannels());

        if (numSamples <= 0 || activeChannels <= 0)
            return;

        auto* left = buffer.getWritePointer (0);
        auto* right = activeChannels > 1 ? buffer.getWritePointer (1) : nullptr;

        float blockGainReduction = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            const float dryL = left[i];
            const float dryR = right != nullptr ? right[i] : dryL;

            // A per-channel detector high-pass keeps bass energy from pumping
            // the compressor, and max(L,R) linking preserves the stereo image.
            const float detL = highPass (dryL, 0);
            const float detR = highPass (dryR, 1);
            const float rectified = std::max (std::abs (detL), std::abs (detR));

            const float envelopeCoeff = rectified > detectorEnvelope ? attackCoeff : releaseCoeff;
            detectorEnvelope += envelopeCoeff * (rectified - detectorEnvelope);

            const float levelDb = juce::Decibels::gainToDecibels (detectorEnvelope, -100.0f);
            const float currentThreshold = threshold.getNextValue();
            lastThresholdDb = currentThreshold;

            const float staticGrDb = computeGainReductionDb (levelDb,
                                                            currentThreshold,
                                                            knee.getNextValue(),
                                                            ratio.getNextValue());

            const float makeupTarget = -staticGrDb * autoMakeup.getNextValue()
                                      + trim.getNextValue();
            makeupDb += makeupCoeff * (makeupTarget - makeupDb);
            makeupDb = juce::jlimit (-6.0f, 18.0f, makeupDb);

            const float gain = juce::Decibels::decibelsToGain (staticGrDb + makeupDb);
            const float mix = juce::jlimit (0.0f, 1.0f, parallel.getNextValue());
            const float saturation = juce::jlimit (0.0f, 0.5f, colour.getNextValue());

            float wetL = dryL * gain;
            float wetR = dryR * gain;

            if (saturation > 0.0005f)
            {
                const float drive = 1.0f + saturation * 1.8f;
                wetL = std::tanh (wetL * drive) / drive;
                wetR = std::tanh (wetR * drive) / drive;
            }

            float outL = dryL + mix * (wetL - dryL);
            float outR = dryR + mix * (wetR - dryR);

            outL = dcBlock (outL, 0);
            outR = dcBlock (outR, 1);

            left[i] = softLimit (outL);
            if (right != nullptr)
                right[i] = softLimit (outR);

            blockGainReduction = std::max (blockGainReduction, -staticGrDb);
        }

        lastGainReductionDb = juce::jlimit (0.0f, 36.0f, blockGainReduction);
    }

private:
    float onePoleCoeffAtRate (float timeMs) const
    {
        const double seconds = std::max (0.00001, static_cast<double> (timeMs) * 0.001);
        return static_cast<float> (1.0 - std::exp (-1.0 / (seconds * sampleRate)));
    }

    float onePoleCutoffCoeffAtRate (float hz) const
    {
        const double cutoff = std::clamp (static_cast<double> (hz), 1.0, sampleRate * 0.45);
        return static_cast<float> (1.0 - std::exp (-juce::MathConstants<double>::twoPi * cutoff / sampleRate));
    }

    static float computeGainReductionDb (float inputDb,
                                         float thresholdDb,
                                         float kneeDb,
                                         float ratioValue)
    {
        const float safeRatio = std::max (1.0f, ratioValue);
        const float safeKnee = std::max (0.1f, kneeDb);
        const float over = inputDb - thresholdDb;
        const float slope = 1.0f / safeRatio - 1.0f;

        float gainReduction = 0.0f;

        if (over > safeKnee * 0.5f)
        {
            gainReduction = slope * over;
        }
        else if (over > -safeKnee * 0.5f)
        {
            const float kneePosition = over + safeKnee * 0.5f;
            gainReduction = slope * (kneePosition * kneePosition) / (2.0f * safeKnee);
        }

        return juce::jlimit (-30.0f, 0.0f, gainReduction);
    }

    static float softLimit (float sample)
    {
        constexpr float knee = 0.96f;
        constexpr float range = 1.0f - knee;

        if (sample > knee)
            return knee + range * std::tanh ((sample - knee) / range);

        if (sample < -knee)
            return -knee - range * std::tanh ((-sample - knee) / range);

        return sample;
    }

    float highPass (float input, int channel)
    {
        auto& lp1 = hpLp1[static_cast<size_t> (channel)];
        auto& lp2 = hpLp2[static_cast<size_t> (channel)];

        lp1 += hpfCoeff * (input - lp1);
        const float firstStage = input - lp1;
        lp2 += hpfCoeff * (firstStage - lp2);
        return firstStage - lp2;
    }

    float dcBlock (float input, int channel)
    {
        auto& xPrev = dcX[static_cast<size_t> (channel)];
        auto& yPrev = dcY[static_cast<size_t> (channel)];

        const float output = input - xPrev + dcCoeff * yPrev;
        xPrev = input;
        yPrev = output;
        return output;
    }

    double sampleRate = 48000.0;
    int numChannels = 2;
    Voice voice = Voice::vocalFemale;

    float attackCoeff = 0.003f;
    float releaseCoeff = 0.0002f;
    float hpfCoeff = 0.014f;
    float makeupCoeff = 0.00008f;
    float dcCoeff = 0.9987f;

    float detectorEnvelope = 0.0f;
    float makeupDb = 0.0f;
    float lastThresholdDb = -26.0f;
    float lastGainReductionDb = 0.0f;

    std::array<float, 2> hpLp1 {};
    std::array<float, 2> hpLp2 {};
    std::array<float, 2> dcX {};
    std::array<float, 2> dcY {};

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> threshold { -26.0f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> ratio { 3.0f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> knee { 8.0f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> parallel { 0.92f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> autoMakeup { 0.74f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> trim { 0.0f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> colour { 0.06f };

    JUCE_LEAK_DETECTOR (AutoCompressor)
};
} // namespace macpressor::dsp
