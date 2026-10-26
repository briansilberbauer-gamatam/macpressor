#include "PluginProcessor.h"

#include "Parameters.h"
#include "PluginEditor.h"

#include <cmath>

namespace
{
using APVTS = juce::AudioProcessorValueTreeState;
}

MacPressorAudioProcessor::MacPressorAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    modeParam = apvts.getRawParameterValue (macpressor::params::mode);
    currentVoice = getModeIndex();
    compressor.setVoice (static_cast<macpressor::dsp::Voice> (currentVoice), true);

    for (auto& sample : waveformRing)
        sample.store (0.0f, std::memory_order_relaxed);
}

MacPressorAudioProcessor::~MacPressorAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout MacPressorAudioProcessor::createParameterLayout()
{
    APVTS::ParameterLayout layout;

    auto source = std::make_unique<juce::AudioProcessorParameterGroup> ("source", "Source", " | ");
    source->addChild (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { macpressor::params::mode, 1 },
        "Voice",
        macpressor::params::getModeNames(),
        0));

    layout.add (std::move (source));
    return layout;
}

void MacPressorAudioProcessor::setModeIndex (int index)
{
    if (auto* parameter = apvts.getParameter (macpressor::params::mode))
    {
        const auto clamped = juce::jlimit (0, macpressor::dsp::numVoices - 1, index);

        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (static_cast<float> (clamped)));
        parameter->endChangeGesture();
    }
}

void MacPressorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    preparedChannels = juce::jlimit (1, 2, juce::jmax (1, juce::jmax (getTotalNumInputChannels(),
                                                                       getTotalNumOutputChannels())));

    currentVoice = getModeIndex();
    compressor.setVoice (static_cast<macpressor::dsp::Voice> (currentVoice), true);
    compressor.prepare (sampleRate, samplesPerBlock, preparedChannels);

    setLatencySamples (0);

    inputLevel.store (0.0f, std::memory_order_relaxed);
    outputLevel.store (0.0f, std::memory_order_relaxed);
    outputLevelLeft.store (0.0f, std::memory_order_relaxed);
    outputLevelRight.store (0.0f, std::memory_order_relaxed);
    gainReductionDb.store (0.0f, std::memory_order_relaxed);
    thresholdDb.store (compressor.getThresholdDb(), std::memory_order_relaxed);

    for (auto& sample : waveformRing)
        sample.store (0.0f, std::memory_order_relaxed);

    waveformWriteIndex.store (0, std::memory_order_release);
}

void MacPressorAudioProcessor::releaseResources()
{
    compressor.reset();
}

void MacPressorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();

    for (int ch = preparedChannels; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    if (numSamples <= 0)
        return;

    const int activeChannels = std::min (preparedChannels, buffer.getNumChannels());

    if (activeChannels <= 0)
        return;

    float inputPeak = 0.0f;
    for (int ch = 0; ch < activeChannels; ++ch)
        inputPeak = std::max (inputPeak, buffer.getMagnitude (ch, 0, numSamples));

    const int requestedVoice = getModeIndex();
    if (requestedVoice != currentVoice)
    {
        currentVoice = requestedVoice;
        compressor.setVoice (static_cast<macpressor::dsp::Voice> (currentVoice), false);
    }

    compressor.process (buffer);

    // Decimated, lock-free waveform telemetry. The UI only ever reads recent
    // samples and never blocks the audio thread.
    {
        uint32_t writeIndex = waveformWriteIndex.load (std::memory_order_relaxed);
        const auto* left = buffer.getReadPointer (0);
        const auto* right = activeChannels > 1 ? buffer.getReadPointer (1) : nullptr;

        for (int i = 0; i < numSamples; i += 2)
        {
            const float mono = right != nullptr
                                   ? 0.5f * (left[i] + right[i])
                                   : left[i];

            waveformRing[static_cast<size_t> (writeIndex & static_cast<uint32_t> (waveformMask))]
                .store (std::isfinite (mono) ? juce::jlimit (-1.0f, 1.0f, mono) : 0.0f,
                        std::memory_order_relaxed);

            writeIndex = (writeIndex + 1u) & static_cast<uint32_t> (waveformMask);
        }

        waveformWriteIndex.store (writeIndex, std::memory_order_release);
    }

    const float outputPeak = buffer.getMagnitude (0, numSamples);
    const float outputLeft = buffer.getMagnitude (0, 0, numSamples);
    const float outputRight = activeChannels > 1 ? buffer.getMagnitude (1, 0, numSamples) : outputLeft;

    inputLevel.store (std::isfinite (inputPeak) ? inputPeak : 0.0f, std::memory_order_relaxed);
    outputLevel.store (std::isfinite (outputPeak) ? outputPeak : 0.0f, std::memory_order_relaxed);
    outputLevelLeft.store (std::isfinite (outputLeft) ? outputLeft : 0.0f, std::memory_order_relaxed);
    outputLevelRight.store (std::isfinite (outputRight) ? outputRight : 0.0f, std::memory_order_relaxed);
    gainReductionDb.store (compressor.getGainReductionDb(), std::memory_order_relaxed);
    thresholdDb.store (compressor.getThresholdDb(), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* MacPressorAudioProcessor::createEditor()
{
    return new MacPressorAudioProcessorEditor (*this);
}

void MacPressorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void MacPressorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto tree = juce::ValueTree::fromXml (*xml);

    if (tree.isValid())
        apvts.replaceState (tree);
}

bool MacPressorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainIn != mainOut)
        return false;

    return mainIn == juce::AudioChannelSet::mono()
        || mainIn == juce::AudioChannelSet::stereo();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MacPressorAudioProcessor();
}
