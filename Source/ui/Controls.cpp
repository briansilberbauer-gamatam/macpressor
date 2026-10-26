#include "Controls.h"

#include "Theme.h"

#include <cmath>

namespace macpressor::ui
{
ModeSwitch::ModeSwitch (juce::String titleText, juce::String subtitleText, juce::String indexText)
    : title (std::move (titleText)),
      subtitle (std::move (subtitleText)),
      index (std::move (indexText))
{
    setWantsKeyboardFocus (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTitle (title);
    setDescription ("Two-position source selector");
}

void ModeSwitch::setOptions (juce::String leftText, juce::String rightText)
{
    if (options[0] != leftText || options[1] != rightText)
    {
        options[0] = std::move (leftText);
        options[1] = std::move (rightText);
        repaint();
    }
}

void ModeSwitch::setSelectedIndex (int newIndex, bool forceRepaint)
{
    const int clamped = juce::jlimit (0, 1, newIndex);

    if (clamped != selectedIndex || forceRepaint)
    {
        selectedIndex = clamped;
        repaint();
    }
}

void ModeSwitch::setHint (juce::String newHint)
{
    if (hint != newHint)
    {
        hint = std::move (newHint);
        repaint();
    }
}

juce::Rectangle<float> ModeSwitch::getWellBounds() const
{
    return juce::Rectangle<float> (16.0f, 50.0f,
                                   static_cast<float> (getWidth()) - 32.0f,
                                   static_cast<float> (getHeight()) - 82.0f);
}

juce::Rectangle<float> ModeSwitch::getKeyBounds (int optionIndex) const
{
    const auto well = getWellBounds();
    const auto inner = well.reduced (6.0f, 6.0f);
    constexpr float gap = 5.0f;
    constexpr float railHeight = 10.0f;
    const float keyWidth = (inner.getWidth() - gap) * 0.5f;
    const float keyHeight = inner.getHeight() - railHeight - 4.0f;
    const float x = optionIndex == 0 ? inner.getX() : inner.getX() + keyWidth + gap;

    return { x, inner.getY(), keyWidth, keyHeight };
}

int ModeSwitch::optionAt (juce::Point<float> position) const
{
    for (int i = 0; i < 2; ++i)
        if (getKeyBounds (i).expanded (3.0f).contains (position))
            return i;

    return -1;
}

void ModeSwitch::selectIndex (int newIndex)
{
    const int clamped = juce::jlimit (0, 1, newIndex);
    setSelectedIndex (clamped);

    if (onSelect != nullptr)
        onSelect (clamped);
}

void ModeSwitch::paint (juce::Graphics& g)
{
    const auto panel = getLocalBounds().toFloat().reduced (0.5f);
    constexpr float panelCorner = 8.0f;

    // Recessed glass module.
    g.setColour (obsidianScreen.withAlpha (0.92f));
    g.fillRoundedRectangle (panel, panelCorner);

    juce::ColourGradient bodyGradient (juce::Colour (0xff1b1e25), panel.getTopLeft(),
                                       juce::Colour (0xff101216), panel.getBottomRight(), false);
    bodyGradient.addColour (0.48, gunmetalBase);
    g.setGradientFill (bodyGradient);
    g.fillRoundedRectangle (panel.reduced (1.0f), panelCorner - 1.0f);

    g.setColour (glassRimHighlight.withAlpha (0.20f));
    g.drawRoundedRectangle (panel, panelCorner, 0.9f);

    g.setColour (crimsonAccent.withAlpha (0.34f));
    g.drawRoundedRectangle (panel.reduced (1.0f), panelCorner - 1.0f, 0.55f);

    // Upper-left glass catchlight.
    {
        juce::Graphics::ScopedSaveState state (g);
        juce::Path clip;
        clip.addRoundedRectangle (panel, panelCorner);
        g.reduceClipRegion (clip);

        juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.035f), panel.getTopLeft(),
                                    juce::Colours::transparentWhite,
                                    panel.getTopLeft().translated (panel.getWidth() * 0.62f,
                                                                  panel.getHeight() * 0.55f),
                                    true);
        g.setGradientFill (sheen);
        g.fillRect (panel);
    }

    drawEngravedLabel (g, { 16.0f, 13.0f, panel.getWidth() - 32.0f, 28.0f },
                       title, subtitle, index);

    const auto well = getWellBounds();

    // Recessed switch cavity.
    g.setColour (obsidianScreen);
    g.fillRoundedRectangle (well, 6.0f);

    {
        juce::ColourGradient innerShadow (juce::Colours::black.withAlpha (0.55f), well.getTopLeft(),
                                          juce::Colours::transparentBlack, well.getBottomLeft(), false);
        g.setGradientFill (innerShadow);
        g.fillRoundedRectangle (well.reduced (1.0f), 5.0f);
    }

    g.setColour (glassRimHighlight.withAlpha (0.21f));
    g.drawRoundedRectangle (well, 6.0f, 0.9f);

    g.setColour (crimsonAccent.withAlpha (0.18f));
    g.drawRoundedRectangle (well.reduced (1.2f), 5.0f, 0.6f);

    // Two machined latching keys.
    for (int i = 0; i < 2; ++i)
    {
        const auto key = getKeyBounds (i);
        const bool selected = selectedIndex == i;
        const bool hovered = hoveredIndex == i;

        g.setColour (dropShadowDeep.withAlpha (0.55f));
        g.fillRoundedRectangle (key.translated (0.0f, 1.2f), 4.5f);

        if (selected)
        {
            g.setColour (crimsonGlow.withAlpha (0.38f));
            g.drawRoundedRectangle (key.expanded (1.6f), 5.4f, 3.0f);

            juce::ColourGradient activeGradient (
                gunmetalLight.interpolatedWith (crimsonGlow, 0.22f), key.getTopLeft(),
                gunmetalDeep.interpolatedWith (crimsonGlow, 0.10f), key.getBottomRight(), false);
            g.setGradientFill (activeGradient);
            g.fillRoundedRectangle (key, 4.5f);

            g.setColour (crimsonGlow.withAlpha (0.12f));
            g.fillRoundedRectangle (key.reduced (1.0f), 4.0f);

            g.setColour (crimsonAccent);
            g.drawRoundedRectangle (key, 4.5f, 1.15f);

            g.setColour (crimsonBright.withAlpha (0.62f));
            g.drawRoundedRectangle (key.reduced (0.55f), 4.0f, 0.55f);
        }
        else
        {
            juce::ColourGradient inactiveGradient (juce::Colour (0xff1a1d23), key.getTopLeft(),
                                                   juce::Colour (0xff0f1115), key.getBottomRight(), false);
            g.setGradientFill (inactiveGradient);
            g.fillRoundedRectangle (key, 4.5f);

            g.setColour (hovered ? chamferLight.withAlpha (0.85f) : gunmetalLight.withAlpha (0.45f));
            g.drawRoundedRectangle (key, 4.5f, hovered ? 1.1f : 0.8f);

            g.setColour (juce::Colours::white.withAlpha (0.04f));
            g.drawRoundedRectangle (key.reduced (1.4f), 3.4f, 0.5f);
        }

        // Small indicator bead above the legend.
        const auto bead = juce::Point<float> (key.getCentreX(), key.getY() + 10.0f);
        if (selected)
        {
            drawCrimsonDot (g, bead, 1.45f);
        }
        else
        {
            g.setColour (textMuted.withAlpha (hovered ? 0.75f : 0.45f));
            g.fillEllipse (bead.x - 1.15f, bead.y - 1.15f, 2.3f, 2.3f);
        }

        g.setColour (selected ? textPrimary : (hovered ? textSecondary.brighter (0.22f) : textSecondary));
        g.setFont (font (selected ? 12.2f : 11.6f,
                         selected ? FontWeight::semibold : FontWeight::regular,
                         selected ? 0.10f : 0.08f));
        g.drawText (options[i], key.withTrimmedTop (15.0f), juce::Justification::centred, false);
    }

    // Low-profile machined position rail beneath the keys.
    const auto inner = well.reduced (6.0f, 6.0f);
    const auto rail = juce::Rectangle<float> (inner.getX(),
                                              inner.getBottom() - 8.0f,
                                              inner.getWidth(), 5.0f);
    juce::ColourGradient railGradient (gunmetalLight.withAlpha (0.80f), rail.getTopLeft(),
                                       gunmetalDeep, rail.getTopRight(), false);
    g.setGradientFill (railGradient);
    g.fillRoundedRectangle (rail, 2.0f);

    g.setColour (obsidianScreen.withAlpha (0.8f));
    g.fillRect (rail.getX(), rail.getCentreY() - 0.45f, rail.getWidth(), 0.9f);

    const auto activeRail = selectedIndex == 0
                                ? rail.withWidth (rail.getWidth() * 0.5f).reduced (2.0f, 0.8f)
                                : rail.withTrimmedLeft (rail.getWidth() * 0.5f).reduced (2.0f, 0.8f);
    g.setColour (crimsonGlow.withAlpha (0.32f));
    g.fillRoundedRectangle (activeRail.expanded (0.6f, 1.3f), 1.8f);
    g.setColour (crimsonAccent);
    g.fillRoundedRectangle (activeRail, 1.5f);

    g.setColour (textMuted);
    g.setFont (font (5.8f, FontWeight::semibold, 0.17f));
    g.drawText (hint, juce::Rectangle<float> (16.0f, panel.getBottom() - 19.0f, panel.getWidth() - 32.0f, 10.0f),
                juce::Justification::centred, false);
}

void ModeSwitch::mouseMove (const juce::MouseEvent& event)
{
    const int newHover = optionAt (event.position);

    if (newHover != hoveredIndex)
    {
        hoveredIndex = newHover;
        setMouseCursor (hoveredIndex >= 0 ? juce::MouseCursor::PointingHandCursor
                                          : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void ModeSwitch::mouseExit (const juce::MouseEvent&)
{
    if (hoveredIndex != -1)
    {
        hoveredIndex = -1;
        setMouseCursor (juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void ModeSwitch::mouseDown (const juce::MouseEvent& event)
{
    const int option = optionAt (event.position);

    if (option >= 0)
    {
        grabKeyboardFocus();
        selectIndex (option);
    }
}

bool ModeSwitch::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::leftKey || key == juce::KeyPress::upKey)
    {
        selectIndex (0);
        return true;
    }

    if (key == juce::KeyPress::rightKey || key == juce::KeyPress::downKey)
    {
        selectIndex (1);
        return true;
    }

    if (key == juce::KeyPress::spaceKey)
    {
        selectIndex (1 - selectedIndex);
        return true;
    }

    return false;
}

HeaderPillBar::HeaderPillBar()
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTitle ("Voice profile");
    setDescription ("Previous and next voice profile");
}

void HeaderPillBar::setModeText (juce::String titleText, juce::String subtitleText)
{
    if (title != titleText || subtitle != subtitleText)
    {
        title = std::move (titleText);
        subtitle = std::move (subtitleText);
        repaint();
    }
}

juce::Rectangle<float> HeaderPillBar::getPreviousBounds() const
{
    const auto bounds = getLocalBounds().toFloat();
    return bounds.withWidth (bounds.getHeight() * 1.18f);
}

juce::Rectangle<float> HeaderPillBar::getNextBounds() const
{
    const auto bounds = getLocalBounds().toFloat();
    return bounds.withLeft (bounds.getRight() - bounds.getHeight() * 1.18f);
}

void HeaderPillBar::drawChevron (juce::Graphics& g, juce::Rectangle<float> area,
                                 bool pointLeft, bool highlighted) const
{
    const auto centre = area.getCentre();
    const float size = 4.4f;
    const float direction = pointLeft ? -1.0f : 1.0f;

    juce::Path path;
    path.startNewSubPath (centre.x - direction * size * 0.45f, centre.y - size);
    path.lineTo (centre.x + direction * size * 0.55f, centre.y);
    path.lineTo (centre.x - direction * size * 0.45f, centre.y + size);

    if (highlighted)
    {
        g.setColour (crimsonGlow.withAlpha (0.38f));
        g.strokePath (path, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }

    g.setColour (highlighted ? crimsonBright : textSecondary);
    g.strokePath (path, juce::PathStrokeType (highlighted ? 1.7f : 1.35f,
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
}

void HeaderPillBar::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    const float radius = bounds.getHeight() * 0.5f;

    g.setColour (crimsonGlow.withAlpha (0.28f));
    g.drawRoundedRectangle (bounds.expanded (1.4f), radius + 1.4f, 2.2f);

    g.setColour (obsidianScreen);
    g.fillRoundedRectangle (bounds, radius);

    juce::ColourGradient screenGradient (juce::Colour (0xff0c0e13), bounds.getTopLeft(),
                                         juce::Colour (0xff06070a), bounds.getBottomRight(), false);
    g.setGradientFill (screenGradient);
    g.fillRoundedRectangle (bounds.reduced (1.0f), radius - 1.0f);

    g.setColour (crimsonAccent.withAlpha (0.62f));
    g.drawRoundedRectangle (bounds, radius, 1.0f);
    g.setColour (glassRimHighlight.withAlpha (0.20f));
    g.drawRoundedRectangle (bounds.reduced (1.2f), radius - 1.2f, 0.7f);

    const auto previous = getPreviousBounds();
    const auto next = getNextBounds();
    const auto navWidth = previous.getWidth();

    if (hoveredSide == -1)
    {
        g.setColour (crimsonGlow.withAlpha (0.20f));
        g.fillRoundedRectangle (previous.reduced (5.0f, 5.0f), 4.0f);
    }
    else if (hoveredSide == 1)
    {
        g.setColour (crimsonGlow.withAlpha (0.20f));
        g.fillRoundedRectangle (next.reduced (5.0f, 5.0f), 4.0f);
    }

    drawChevron (g, previous, true, hoveredSide == -1);
    drawChevron (g, next, false, hoveredSide == 1);

    g.setColour (obsidianGrid.brighter (0.08f));
    g.fillRect (previous.getRight(), bounds.getY() + 8.0f, 1.0f, bounds.getHeight() - 16.0f);
    g.fillRect (next.getX(), bounds.getY() + 8.0f, 1.0f, bounds.getHeight() - 16.0f);

    const auto textArea = bounds.withTrimmedLeft (navWidth + 6.0f)
                                .withTrimmedRight (navWidth + 6.0f);

    g.setColour (textPrimary);
    g.setFont (font (13.0f, FontWeight::semibold, 0.12f));
    g.drawText (title, textArea.withTrimmedTop (6.0f).withHeight (19.0f),
                juce::Justification::centred, false);

    g.setColour (crimsonAccent.withAlpha (0.88f));
    g.fillEllipse (textArea.getCentreX() - 32.0f, bounds.getBottom() - 15.0f, 2.6f, 2.6f);

    g.setColour (textMuted);
    g.setFont (font (6.0f, FontWeight::regular, 0.18f));
    g.drawText (subtitle, textArea.withTrimmedTop (27.0f).withHeight (11.0f),
                juce::Justification::centred, false);
}

void HeaderPillBar::mouseMove (const juce::MouseEvent& event)
{
    const int side = getPreviousBounds().contains (event.position) ? -1
                   : getNextBounds().contains (event.position) ? 1
                                                                : 0;

    if (side != hoveredSide)
    {
        hoveredSide = side;
        setMouseCursor (side != 0 ? juce::MouseCursor::PointingHandCursor
                                  : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void HeaderPillBar::mouseExit (const juce::MouseEvent&)
{
    if (hoveredSide != 0)
    {
        hoveredSide = 0;
        setMouseCursor (juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void HeaderPillBar::mouseDown (const juce::MouseEvent& event)
{
    if (getPreviousBounds().contains (event.position))
    {
        if (onCycle != nullptr)
            onCycle (-1);
    }
    else if (getNextBounds().contains (event.position))
    {
        if (onCycle != nullptr)
            onCycle (1);
    }
}
} // namespace macpressor::ui
