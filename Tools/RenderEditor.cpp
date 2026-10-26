#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Version.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <cmath>
#include <iostream>
#include <memory>

class MacPressorRenderApplication final : public juce::JUCEApplication,
                                           private juce::Timer
{
public:
    const juce::String getApplicationName() override { return "MacPressor Screenshot"; }
    const juce::String getApplicationVersion() override { return macpressor::version; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void anotherInstanceStarted (const juce::String&) override {}
    void systemRequestedQuit() override {}
    void shutdown() override {}

    void initialise (const juce::String& commandLine) override
    {
        outputFile = juce::File::getCurrentWorkingDirectory().getChildFile ("macpressor-ui.png");

        for (const auto& argument : juce::StringArray::fromTokens (commandLine, true))
        {
            if (argument.startsWith ("--out="))
            {
                const auto path = argument.substring (6);
                outputFile = juce::File::isAbsolutePath (path)
                                 ? juce::File (path)
                                 : juce::File::getCurrentWorkingDirectory().getChildFile (path);
            }
            else if (argument.startsWith ("--mode="))
            {
                requestedMode = juce::jlimit (0, 3, argument.substring (7).getIntValue());
            }
        }

        outputFile = outputFile.getFullPathName();
        outputFile.getParentDirectory().createDirectory();

        processor = std::make_unique<MacPressorAudioProcessor>();

        if (auto* parameter = processor->getValueTreeState().getParameter ("mode"))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (static_cast<float> (requestedMode)));

        editor = std::unique_ptr<juce::AudioProcessorEditor> (processor->createEditor());

        if (editor == nullptr)
        {
            std::cerr << "Could not create the plugin editor.\n";
            setApplicationReturnValue (1);
            quit();
            return;
        }

        editor->setSize (1180, 720);
        editor->addToDesktop (0);
        editor->setTopLeftPosition (30, 30);
        editor->setVisible (true);
        editor->toFront (true);

        juce::Desktop::getInstance().getMainMouseSource().setScreenPosition ({ 3000.0f, 3000.0f });

        processor->setPlayConfigDetails (2, 2, 48000.0, 512);
        processor->prepareToPlay (48000.0, 512);
        testBuffer.setSize (2, 512);
        startTimerHz (30);

        juce::Timer::callAfterDelay (2400, [this]
        {
            stopTimer();
            juce::Desktop::getInstance().getMainMouseSource().setScreenPosition ({ 3000.0f, 3000.0f });

            const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);

            if (image.isValid())
            {
                outputFile.deleteFile();

                if (auto stream = std::unique_ptr<juce::FileOutputStream> (outputFile.createOutputStream()))
                {
                    juce::PNGImageFormat png;
                    const bool written = png.writeImageToStream (image, *stream);
                    stream->flush();

                    if (written)
                        std::cout << "Rendered " << outputFile.getFullPathName() << "\n";
                    else
                        std::cerr << "Failed to encode PNG.\n";
                }
                else
                {
                    std::cerr << "Could not open " << outputFile.getFullPathName() << "\n";
                }
            }
            else
            {
                std::cerr << "Editor snapshot was invalid.\n";
            }

            processor->releaseResources();
            editor->removeFromDesktop();
            editor.reset();
            processor.reset();
            quit();
        });
    }

private:
    void timerCallback() override
    {
        if (processor == nullptr)
            return;

        for (int i = 0; i < testBuffer.getNumSamples(); ++i)
        {
            const double t = static_cast<double> (sampleCounter + i) / 48000.0;
            const double phrase = 0.50 + 0.10 * std::sin (juce::MathConstants<double>::twoPi * 0.85 * t)
                                       + 0.04 * std::sin (juce::MathConstants<double>::twoPi * 2.3 * t);

            auto tone = [&] (double frequency, double phase)
            {
                return std::sin (juce::MathConstants<double>::twoPi * frequency * t + phase);
            };

            const double wave = 0.32 * tone (196.0, 0.0)
                              + 0.20 * tone (261.63, 0.7)
                              + 0.14 * tone (329.63, 1.3)
                              + 0.10 * tone (392.0, 2.1)
                              + 0.06 * tone (523.25, 0.3)
                              + 0.04 * tone (784.0, 1.7)
                              + 0.03 * tone (2470.0, 0.4)
                              + 0.02 * tone (3910.0, 2.0);

            const float signal = static_cast<float> (phrase * wave);

            for (int ch = 0; ch < testBuffer.getNumChannels(); ++ch)
                testBuffer.setSample (ch, i, ch == 0 ? signal : signal * 0.94f);
        }

        sampleCounter += testBuffer.getNumSamples();
        juce::MidiBuffer midi;
        processor->processBlock (testBuffer, midi);
    }

    std::unique_ptr<MacPressorAudioProcessor> processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    juce::File outputFile;
    juce::AudioBuffer<float> testBuffer;
    juce::int64 sampleCounter = 0;
    int requestedMode = 0;
};

START_JUCE_APPLICATION (MacPressorRenderApplication)
