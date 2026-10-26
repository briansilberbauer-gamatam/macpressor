#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace macpressor::ui
{
/** A two-position, flush-mounted hardware source switch.

    The whole component paints a recessed module; the two large machined keys
    are the only hit targets. Selection is reported through onSelect.
*/
class ModeSwitch final : public juce::Component
{
public:
    ModeSwitch (juce::String titleText, juce::String subtitleText, juce::String indexText);

    std::function<void (int)> onSelect;

    void setOptions (juce::String leftText, juce::String rightText);
    void setSelectedIndex (int index, bool forceRepaint = false);
    int getSelectedIndex() const noexcept { return selectedIndex; }

    void setHint (juce::String newHint);

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    juce::Rectangle<float> getWellBounds() const;
    juce::Rectangle<float> getKeyBounds (int index) const;
    int optionAt (juce::Point<float> position) const;
    void selectIndex (int index);

    juce::String title;
    juce::String subtitle;
    juce::String index;
    juce::String options[2] { "A", "B" };
    juce::String hint;

    int selectedIndex = 0;
    int hoveredIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModeSwitch)
};

/** Slender top-centre voice readout with functional mode chevrons. */
class HeaderPillBar final : public juce::Component
{
public:
    HeaderPillBar();

    std::function<void (int)> onCycle;

    void setModeText (juce::String titleText, juce::String subtitleText);

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;

    int getPreferredHeight() const noexcept { return 48; }

private:
    juce::Rectangle<float> getPreviousBounds() const;
    juce::Rectangle<float> getNextBounds() const;
    void drawChevron (juce::Graphics&, juce::Rectangle<float>, bool pointLeft, bool highlighted) const;

    juce::String title;
    juce::String subtitle;
    int hoveredSide = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeaderPillBar)
};
} // namespace macpressor::ui
