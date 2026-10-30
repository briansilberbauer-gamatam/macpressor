#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Version.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

namespace
{
    struct WaveformVisibility
    {
        int pixelCount = 0;
        int columnCount = 0;
        int top = 0;
        int bottom = 0;

        bool isVisible() const noexcept
        {
            // A live trace covers a large part of the plot. A blank scope only
            // contains the dim centre guide (too dark to match) and the small THR
            // label, so it fails on both the span and the column count.
            return pixelCount >= 1200 && columnCount >= 300 && (bottom - top) >= 60;
        }
    };

    /** Counts the crimson waveform pixels inside the scope plot of a rendered editor. */
    WaveformVisibility analyseWaveformTrace (const juce::Image& image)
    {
        WaveformVisibility result;

        if (! image.isValid())
            return result;

        const juce::Image::BitmapData pixels (image, juce::Image::BitmapData::readOnly);
        const float scaleX = static_cast<float> (image.getWidth()) / 1180.0f;
        const float scaleY = static_cast<float> (image.getHeight()) / 720.0f;

        // The plot area in editor design coordinates (see WaveformDisplay::paint).
        // Deliberately excludes the header readouts and the footer status line.
        const int x0 = juce::jlimit (0, image.getWidth() - 1, juce::roundToInt (120.0f * scaleX));
        const int x1 = juce::jlimit (x0 + 1, image.getWidth(), juce::roundToInt (1090.0f * scaleX));
        const int y0 = juce::jlimit (0, image.getHeight() - 1, juce::roundToInt (168.0f * scaleY));
        const int y1 = juce::jlimit (y0 + 1, image.getHeight(), juce::roundToInt (400.0f * scaleY));

        std::vector<char> columns (static_cast<size_t> (x1 - x0), 0);
        result.top = y1;
        result.bottom = y0;

        for (int y = y0; y < y1; ++y)
        {
            for (int x = x0; x < x1; ++x)
            {
                const auto colour = pixels.getPixelColour (x, y);

                if (colour.getAlpha() < 8)
                    continue;

                const int r = static_cast<int> (colour.getRed());
                const int g = static_cast<int> (colour.getGreen());
                const int b = static_cast<int> (colour.getBlue());

                // Muted crimson trace: red-dominant and bright enough to exclude
                // the dim grid, threshold dashes and idle centre line.
                if (r >= 110 && r >= static_cast<int> (1.7f * static_cast<float> (g))
                    && r >= static_cast<int> (1.5f * static_cast<float> (b)))
                {
                    ++result.pixelCount;
                    columns[static_cast<size_t> (x - x0)] = 1;
                    result.top = juce::jmin (result.top, y);
                    result.bottom = juce::jmax (result.bottom, y);
                }
            }
        }

        for (const auto hit : columns)
            result.columnCount += hit != 0 ? 1 : 0;

        return result;
    }
} // namespace

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
            else if (argument == "--no-verify")
            {
                verifyWaveform = false;
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

            bool success = image.isValid();

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
                    {
                        std::cerr << "Failed to encode PNG.\n";
                        success = false;
                    }
                }
                else
                {
                    std::cerr << "Could not open " << outputFile.getFullPathName() << "\n";
                    success = false;
                }

                // Guards the exact Windows symptom: a scope that paints as an
                // empty screen while the rest of the editor looks fine.
                if (verifyWaveform)
                {
                    const auto trace = analyseWaveformTrace (image);

                    std::cout << "Waveform trace: " << trace.pixelCount << " px / "
                              << trace.columnCount << " columns / "
                              << (trace.bottom - trace.top) << " px vertical span\n";

                    if (trace.isVisible())
                    {
                        std::cout << "Waveform verification passed.\n";
                    }
                    else
                    {
                        std::cerr << "Waveform verification FAILED: the scope is blank or static.\n";
                        success = false;
                    }
                }
            }
            else
            {
                std::cerr << "Editor snapshot was invalid.\n";
                success = false;
            }

            setApplicationReturnValue (success ? 0 : 1);

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
    bool verifyWaveform = true;
};

START_JUCE_APPLICATION (MacPressorRenderApplication)
