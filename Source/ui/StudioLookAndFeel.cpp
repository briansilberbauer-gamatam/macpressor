#include "StudioLookAndFeel.h"

#include "Theme.h"

namespace macpressor::ui
{
StudioLookAndFeel::StudioLookAndFeel()
{
    setColour (juce::TextButton::buttonColourId, gunmetalBase);
    setColour (juce::TextButton::buttonOnColourId, crimsonGlow);
    setColour (juce::TextButton::textColourOffId, textSecondary);
    setColour (juce::TextButton::textColourOnId, textPrimary);
    setColour (juce::Label::textColourId, textPrimary);
    setColour (juce::PopupMenu::backgroundColourId, obsidianScreen);
    setColour (juce::PopupMenu::textColourId, textPrimary);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, crimsonGlow);
    setColour (juce::PopupMenu::highlightedTextColourId, textPrimary);
}

juce::Font StudioLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return font (juce::jmin (14.0f, static_cast<float> (buttonHeight) * 0.55f),
                 FontWeight::semibold, 0.10f);
}

juce::Font StudioLookAndFeel::getLabelFont (juce::Label&)
{
    return font (11.0f, FontWeight::regular);
}

void StudioLookAndFeel::drawButtonBackground (juce::Graphics& g,
                                              juce::Button& button,
                                              const juce::Colour&,
                                              bool shouldDrawButtonAsHighlighted,
                                              bool shouldDrawButtonAsDown)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    constexpr float corner = 3.5f;
    const bool active = button.getToggleState();

    g.setColour (obsidianWell);
    g.fillRoundedRectangle (bounds.expanded (1.0f), corner + 0.8f);

    juce::ColourGradient capGradient (active ? gunmetalLight.interpolatedWith (crimsonGlow, 0.14f)
                                             : gunmetalBase,
                                      bounds.getTopLeft(),
                                      gunmetalDeep, bounds.getBottomRight(), false);
    g.setGradientFill (capGradient);
    g.fillRoundedRectangle (bounds, corner);

    if (active)
    {
        g.setColour (crimsonGlow.withAlpha (0.36f));
        g.drawRoundedRectangle (bounds.expanded (1.0f), corner + 0.5f, 2.8f);
        g.setColour (crimsonAccent);
        g.drawRoundedRectangle (bounds, corner, 1.2f);
    }
    else
    {
        g.setColour (shouldDrawButtonAsHighlighted ? chamferLight : gunmetalLight.withAlpha (0.65f));
        g.drawRoundedRectangle (bounds, corner, shouldDrawButtonAsDown ? 1.3f : 0.9f);
    }
}

void StudioLookAndFeel::drawToggleButton (juce::Graphics& g,
                                          juce::ToggleButton& button,
                                          bool shouldDrawButtonAsHighlighted,
                                          bool)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (2.0f);
    constexpr float corner = 3.5f;
    const bool active = button.getToggleState();

    g.setColour (obsidianWell);
    g.fillRoundedRectangle (bounds.expanded (1.0f), corner + 0.8f);

    juce::ColourGradient capGradient (active ? gunmetalLight.interpolatedWith (crimsonGlow, 0.14f)
                                             : gunmetalBase,
                                      bounds.getTopLeft(),
                                      gunmetalDeep, bounds.getBottomRight(), false);
    g.setGradientFill (capGradient);
    g.fillRoundedRectangle (bounds, corner);

    if (active)
    {
        g.setColour (crimsonGlow.withAlpha (0.36f));
        g.drawRoundedRectangle (bounds.expanded (1.0f), corner + 0.5f, 2.8f);
        g.setColour (crimsonAccent);
        g.drawRoundedRectangle (bounds, corner, 1.2f);
        g.setColour (crimsonBright.withAlpha (0.75f));
        g.drawRoundedRectangle (bounds.reduced (0.4f), corner, 0.6f);
    }
    else
    {
        g.setColour (shouldDrawButtonAsHighlighted ? chamferLight : gunmetalLight.withAlpha (0.65f));
        g.drawRoundedRectangle (bounds, corner, 1.0f);
    }

    g.setColour (active ? textPrimary : textSecondary);
    g.setFont (font (10.5f, active ? FontWeight::semibold : FontWeight::regular, 0.10f));
    g.drawText (button.getButtonText().toUpperCase(), bounds, juce::Justification::centred, false);
}
} // namespace macpressor::ui
