#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/AutoCompressor.h"

#include <array>
#include <atomic>
#include <cstdint>

class MacPressorAudioProcessor final : public juce::AudioProcessor
{
public:
    MacPressorAudioProcessor();
    ~MacPressorAudioProcessor() override;

    using juce::AudioProcessor::processBlock;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MacPressor"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Auto Voice"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return apvts; }
    const juce::AudioProcessorValueTreeState& getValueTreeState() const noexcept { return apvts; }

    int getModeIndex() const noexcept
    {
        return modeParam != nullptr
                   ? juce::jlimit (0, macpressor::dsp::numVoices - 1,
                                   juce::roundToInt (modeParam->load (std::memory_order_relaxed)))
                   : 0;
    }

    void setModeIndex (int index);

    const macpressor::dsp::VoiceProfile& getActiveProfile() const noexcept
    {
        return macpressor::dsp::getVoiceProfile (getModeIndex());
    }

    float getInputLevel() const noexcept { return inputLevel.load (std::memory_order_relaxed); }
    float getOutputLevel() const noexcept { return outputLevel.load (std::memory_order_relaxed); }
    float getOutputLevelLeft() const noexcept { return outputLevelLeft.load (std::memory_order_relaxed); }
    float getOutputLevelRight() const noexcept { return outputLevelRight.load (std::memory_order_relaxed); }
    float getGainReductionDb() const noexcept { return gainReductionDb.load (std::memory_order_relaxed); }
    float getThresholdDb() const noexcept { return thresholdDb.load (std::memory_order_relaxed); }

    static constexpr int waveformSize = 8192;
    static constexpr int waveformMask = waveformSize - 1;

    uint32_t getWaveformWriteIndex() const noexcept
    {
        return waveformWriteIndex.load (std::memory_order_acquire);
    }

    float getWaveformSample (uint32_t index) const noexcept
    {
        return waveformRing[static_cast<size_t> (index & static_cast<uint32_t> (waveformMask))]
            .load (std::memory_order_relaxed);
    }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float>* modeParam = nullptr;

    macpressor::dsp::AutoCompressor compressor;

    std::atomic<float> inputLevel { 0.0f };
    std::atomic<float> outputLevel { 0.0f };
    std::atomic<float> outputLevelLeft { 0.0f };
    std::atomic<float> outputLevelRight { 0.0f };
    std::atomic<float> gainReductionDb { 0.0f };
    std::atomic<float> thresholdDb { -26.0f };

    std::array<std::atomic<float>, waveformSize> waveformRing {};
    std::atomic<uint32_t> waveformWriteIndex { 0 };

    int preparedChannels = 2;
    int currentVoice = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MacPressorAudioProcessor)
};
