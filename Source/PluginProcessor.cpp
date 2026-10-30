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
    // Hosts do not agree on when the bus layout becomes visible: some Windows
    // hosts call prepareToPlay while the main bus still reports mono or no
    // channels at all. The engine is stereo-linked, so always prepare it for two
    // channels and derive the channels to process from the actual audio buffer.
    currentVoice = getModeIndex();
    compressor.setVoice (static_cast<macpressor::dsp::Voice> (currentVoice), true);
    compressor.prepare (sampleRate, juce::jmax (1, samplesPerBlock), 2);

    setLatencySamples (0);

    inputLevel.store (0.0f, std::memory_order_relaxed);
    outputLevel.store (0.0f, std::memory_order_relaxed);
    outputLevelLeft.store (0.0f, std::memory_order_relaxed);
    outputLevelRight.store (0.0f, std::memory_order_relaxed);
    gainReductionDb.store (0.0f, std::memory_order_relaxed);
    thresholdDb.store (compressor.getThresholdDb(), std::memory_order_relaxed);

    for (auto& sample : waveformRing)
        sample.store (0.0f, std::memory_order_relaxed);

    waveformValidSamples.store (0, std::memory_order_relaxed);
    waveformWriteIndex.store (0, std::memory_order_release);
    hostBypassed.store (false, std::memory_order_relaxed);
}

void MacPressorAudioProcessor::releaseResources()
{
    compressor.reset();
}

void MacPressorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    hostBypassed.store (false, std::memory_order_relaxed);

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // Anything beyond the stereo buses is not ours; the rest of the buffer is
    // processed exactly as the host delivered it.
    for (int ch = 2; ch < numChannels; ++ch)
        buffer.clear (ch, 0, numSamples);

    if (numSamples <= 0)
        return;

    const int activeChannels = juce::jmin (2, numChannels);

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
    pushWaveformSamples (buffer, activeChannels, numSamples);
    publishMeters (buffer, activeChannels, numSamples, inputPeak, compressor.getGainReductionDb());
}

void MacPressorAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer,
                                                    juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    hostBypassed.store (true, std::memory_order_relaxed);

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int ch = 2; ch < numChannels; ++ch)
        buffer.clear (ch, 0, numSamples);

    if (numSamples <= 0)
        return;

    const int activeChannels = juce::jmin (2, numChannels);

    if (activeChannels <= 0)
        return;

    float inputPeak = 0.0f;
    for (int ch = 0; ch < activeChannels; ++ch)
        inputPeak = std::max (inputPeak, buffer.getMagnitude (ch, 0, numSamples));

    // The host has bypassed the unit, so the audio must stay dry. The scope and
    // meters are still fed, and zero gain reduction is published, so the editor
    // can show "HOST BYPASS" instead of looking like a dead plug-in.
    pushWaveformSamples (buffer, activeChannels, numSamples);
    publishMeters (buffer, activeChannels, numSamples, inputPeak, 0.0f);
}

void MacPressorAudioProcessor::pushWaveformSamples (const juce::AudioBuffer<float>& buffer,
                                                   int activeChannels, int numSamples) noexcept
{
    uint32_t writeIndex = waveformWriteIndex.load (std::memory_order_relaxed)
                        & static_cast<uint32_t> (waveformMask);
    const auto* left = buffer.getReadPointer (0);
    const auto* right = activeChannels > 1 ? buffer.getReadPointer (1) : nullptr;
    uint32_t written = 0;

    for (int i = 0; i < numSamples; i += 2)
    {
        const float mono = right != nullptr ? 0.5f * (left[i] + right[i]) : left[i];

        waveformRing[static_cast<size_t> (writeIndex)]
            .store (std::isfinite (mono) ? juce::jlimit (-1.0f, 1.0f, mono) : 0.0f,
                    std::memory_order_relaxed);

        writeIndex = (writeIndex + 1u) & static_cast<uint32_t> (waveformMask);
        ++written;
    }

    const uint32_t valid = juce::jmin (static_cast<uint32_t> (waveformSize),
                                       waveformValidSamples.load (std::memory_order_relaxed) + written);

    // Publish validity first, then the new write position with release ordering,
    // so a reader that acquires the position also sees a matching valid count.
    waveformValidSamples.store (valid, std::memory_order_relaxed);
    waveformWriteIndex.store (writeIndex, std::memory_order_release);
}

int MacPressorAudioProcessor::copyLatestWaveformSamples (float* destination, int maxSamples) const noexcept
{
    if (destination == nullptr || maxSamples <= 0)
        return 0;

    const uint32_t writeIndex = waveformWriteIndex.load (std::memory_order_acquire);
    const uint32_t valid = waveformValidSamples.load (std::memory_order_relaxed);
    const int count = static_cast<int> (juce::jmin (static_cast<uint32_t> (maxSamples),
                                                    juce::jmin (valid, static_cast<uint32_t> (waveformSize))));

    if (count <= 0)
        return 0;

    const uint32_t start = writeIndex - static_cast<uint32_t> (count);

    for (int i = 0; i < count; ++i)
        destination[i] = waveformRing[static_cast<size_t> ((start + static_cast<uint32_t> (i))
                                                          & static_cast<uint32_t> (waveformMask))]
                             .load (std::memory_order_relaxed);

    return count;
}

void MacPressorAudioProcessor::publishMeters (const juce::AudioBuffer<float>& buffer,
                                              int activeChannels, int numSamples,
                                              float inputPeak, float gainReduction) noexcept
{
    const float outputLeft = buffer.getMagnitude (0, 0, numSamples);
    const float outputRight = activeChannels > 1 ? buffer.getMagnitude (1, 0, numSamples) : outputLeft;
    const float outputPeak = juce::jmax (outputLeft, outputRight);

    inputLevel.store (std::isfinite (inputPeak) ? juce::jlimit (0.0f, 8.0f, inputPeak) : 0.0f,
                      std::memory_order_relaxed);
    outputLevel.store (std::isfinite (outputPeak) ? juce::jlimit (0.0f, 8.0f, outputPeak) : 0.0f,
                       std::memory_order_relaxed);
    outputLevelLeft.store (std::isfinite (outputLeft) ? outputLeft : 0.0f, std::memory_order_relaxed);
    outputLevelRight.store (std::isfinite (outputRight) ? outputRight : 0.0f, std::memory_order_relaxed);
    gainReductionDb.store (std::isfinite (gainReduction)
                               ? juce::jlimit (0.0f, 36.0f, gainReduction)
                               : 0.0f,
                           std::memory_order_relaxed);
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
