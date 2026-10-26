#include "Theme.h"

#include <BinaryData.h>

#include <cmath>

namespace macpressor::ui
{
namespace
{
juce::Typeface::Ptr makeTypeface (const void* data, int size)
{
    return juce::Typeface::createSystemTypefaceFor (data, static_cast<size_t> (size));
}

const juce::Typeface::Ptr& regularTypeface()
{
    static const auto typeface = makeTypeface (BinaryData::LatoRegular_ttf, BinaryData::LatoRegular_ttfSize);
    return typeface;
}

const juce::Typeface::Ptr& semiboldTypeface()
{
    static const auto typeface = makeTypeface (BinaryData::LatoSemibold_ttf, BinaryData::LatoSemibold_ttfSize);
    return typeface;
}

const juce::Typeface::Ptr& blackTypeface()
{
    static const auto typeface = makeTypeface (BinaryData::LatoBlack_ttf, BinaryData::LatoBlack_ttfSize);
    return typeface;
}
} // namespace

juce::Font font (float height, FontWeight weight, float tracking)
{
    const auto& typeface = weight == FontWeight::black ? blackTypeface()
                         : weight == FontWeight::semibold ? semiboldTypeface()
                                                          : regularTypeface();

    juce::Font result { juce::FontOptions (typeface).withHeight (height) };

    if (! juce::approximatelyEqual (tracking, 0.0f))
        result = result.withExtraKerningFactor (tracking);

    return result;
}

juce::String formatDb (float value, int decimals)
{
    if (std::abs (value) < 0.05f)
        return "0.0 dB";

    const int places = juce::jlimit (0, 2, decimals);
    return (value > 0.0f ? "+" : "") + juce::String (value, places) + " dB";
}

juce::String formatMs (float value)
{
    if (value < 10.0f)
        return juce::String (value, 1) + " ms";

    return juce::String (juce::roundToInt (value)) + " ms";
}

juce::String formatRatio (float value)
{
    return juce::String (value, 1) + ":1";
}

juce::String formatPercent (float value)
{
    return juce::String (juce::roundToInt (juce::jlimit (0.0f, 1.0f, value) * 100.0f)) + "%";
}

void drawDesktopHardwareChassis (juce::Graphics& g, juce::Rectangle<float> fullBounds)
{
    // 1. Shadowed studio desk with an upper-left soft-box falloff.
    juce::ColourGradient studioGradient (studioSurfaceLight, fullBounds.getTopLeft(),
                                         studioSurfaceDark, fullBounds.getBottomRight(), true);
    studioGradient.addColour (0.55, juce::Colour (0xff0b0d12));
    g.setGradientFill (studioGradient);
    g.fillRect (fullBounds);

    // Faint pool of light on the desk surface, upper-left.
    {
        juce::ColourGradient pool (juce::Colours::white.withAlpha (0.035f),
                                   fullBounds.getTopLeft().translated (fullBounds.getWidth() * 0.18f,
                                                                       -fullBounds.getHeight() * 0.10f),
                                   juce::Colours::transparentWhite,
                                   fullBounds.getTopLeft().translated (fullBounds.getWidth() * 0.72f,
                                                                       fullBounds.getHeight() * 0.78f),
                                   true);
        g.setGradientFill (pool);
        g.fillRect (fullBounds);
    }

    // 2. Desktop unit inset from every edge.
    const auto unitBounds = fullBounds.reduced (16.0f, 12.0f);
    constexpr float outerCorner = 16.0f;

    // Layered cast shadow toward the lower-right.
    for (int i = 6; i >= 1; --i)
    {
        const float spread = static_cast<float> (i) * 3.4f;
        const float offset = static_cast<float> (i) * 2.5f;
        g.setColour (dropShadowDeep.withAlpha (0.14f));
        g.fillRoundedRectangle (unitBounds.expanded (spread * 0.35f).translated (offset, offset * 1.15f),
                                outerCorner + spread * 0.22f);
    }

    // 3. Dimensional lower-right chamfered base extrusion.
    const auto extrudedBase = unitBounds.translated (4.0f, 5.5f);
    juce::ColourGradient skirtGradient (chamferLight, extrudedBase.getTopLeft(),
                                        chamferDark, extrudedBase.getBottomRight(), false);
    g.setGradientFill (skirtGradient);
    g.fillRoundedRectangle (extrudedBase, outerCorner);

    // 4. Smoothly chamfered matte metal rim.
    juce::ColourGradient chamferGradient (chamferLight, unitBounds.getTopLeft(),
                                          chamferDark, unitBounds.getBottomRight(), false);
    chamferGradient.addColour (0.38, juce::Colour (0xff242932));
    g.setGradientFill (chamferGradient);
    g.fillRoundedRectangle (unitBounds, outerCorner);

    // 5. Brushed gunmetal base with fine horizontal grain.
    const auto metalBase = unitBounds.reduced (4.0f);
    constexpr float metalCorner = 12.5f;

    juce::ColourGradient metalGradient (gunmetalLight, metalBase.getTopLeft(),
                                        gunmetalDeep, metalBase.getBottomRight(), false);
    metalGradient.addColour (0.42, gunmetalBase);
    g.setGradientFill (metalGradient);
    g.fillRoundedRectangle (metalBase, metalCorner);

    {
        juce::Graphics::ScopedSaveState state (g);
        juce::Path clip;
        clip.addRoundedRectangle (metalBase, metalCorner);
        g.reduceClipRegion (clip);

        const int yStart = static_cast<int> (metalBase.getY());
        const int yEnd = static_cast<int> (metalBase.getBottom());
        const float x1 = metalBase.getX();
        const float w = metalBase.getWidth();

        for (int y = yStart; y < yEnd; y += 2)
        {
            const float fy = static_cast<float> (y);
            g.setColour (juce::Colours::white.withAlpha ((y % 6 == 0) ? 0.034f : 0.015f));
            g.fillRect (x1, fy, w, 1.0f);
            g.setColour (juce::Colours::black.withAlpha (0.075f));
            g.fillRect (x1, fy + 1.0f, w, 1.0f);
        }

        // Every 48px, a slightly stronger machining line.
        for (int y = yStart; y < yEnd; y += 48)
        {
            g.setColour (juce::Colours::white.withAlpha (0.025f));
            g.fillRect (x1, static_cast<float> (y), w, 0.7f);
        }
    }

    // Top chamfer catchlight.
    g.setColour (chamferLight.withAlpha (0.72f));
    g.drawLine (metalBase.getX() + 18.0f, metalBase.getY() + 0.6f,
                metalBase.getRight() - 18.0f, metalBase.getY() + 0.6f, 0.9f);

    // 6. Thick, seamless transparent glass overlay. No fasteners anywhere.
    const auto glassPanel = metalBase.reduced (7.0f);
    constexpr float glassCorner = 8.5f;

    juce::ColourGradient glassBodyGradient (glassTintTop, glassPanel.getTopLeft(),
                                            glassTintBottom, glassPanel.getBottomRight(), false);
    glassBodyGradient.addColour (0.58, juce::Colour (0x15000000));
    g.setGradientFill (glassBodyGradient);
    g.fillRoundedRectangle (glassPanel, glassCorner);

    // 7. Crimson LED edge-glow and inward refraction.
    for (int ring = 5; ring >= 1; --ring)
    {
        const float inset = static_cast<float> (ring) * 2.1f;
        const float alpha = static_cast<float> (6 - ring) * 0.032f;
        g.setColour (crimsonGlow.withAlpha (alpha));
        g.drawRoundedRectangle (glassPanel.reduced (inset),
                                juce::jmax (2.0f, glassCorner - inset * 0.45f), 2.0f);
    }

    // Crisp low-profile light-pipe along the glass perimeter.
    juce::ColourGradient edgeGlowGradient (crimsonBright.withAlpha (0.82f), glassPanel.getTopLeft(),
                                           crimsonAccent.withAlpha (0.52f), glassPanel.getBottomRight(), false);
    edgeGlowGradient.addColour (0.48, crimsonAccent.withAlpha (0.88f));
    g.setGradientFill (edgeGlowGradient);
    g.drawRoundedRectangle (glassPanel, glassCorner, 1.35f);

    // Polished top-left glass bevel.
    g.setColour (glassRimHighlight);
    g.drawRoundedRectangle (glassPanel.reduced (1.4f), glassCorner - 1.0f, 0.75f);

    // 8. Upper-left studio soft-box reflection sweeping across the glass.
    {
        juce::Graphics::ScopedSaveState state (g);
        juce::Path glassClip;
        glassClip.addRoundedRectangle (glassPanel, glassCorner);
        g.reduceClipRegion (glassClip);

        juce::Path sheen;
        sheen.startNewSubPath (glassPanel.getX(), glassPanel.getY());
        sheen.lineTo (glassPanel.getX() + glassPanel.getWidth() * 0.54f, glassPanel.getY());
        sheen.lineTo (glassPanel.getX() + glassPanel.getWidth() * 0.21f, glassPanel.getBottom());
        sheen.lineTo (glassPanel.getX(), glassPanel.getBottom());
        sheen.closeSubPath();

        juce::ColourGradient sheenGradient (juce::Colours::white.withAlpha (0.062f),
                                            glassPanel.getTopLeft(),
                                            juce::Colours::transparentWhite,
                                            glassPanel.getCentre(), false);
        g.setGradientFill (sheenGradient);
        g.fillPath (sheen);

        // A narrow diagonally reflected highlight.
        juce::Path reflection;
        reflection.startNewSubPath (glassPanel.getX() + glassPanel.getWidth() * 0.30f, glassPanel.getY());
        reflection.lineTo (glassPanel.getX() + glassPanel.getWidth() * 0.36f, glassPanel.getY());
        reflection.lineTo (glassPanel.getX() + glassPanel.getWidth() * 0.16f, glassPanel.getBottom());
        reflection.lineTo (glassPanel.getX() + glassPanel.getWidth() * 0.10f, glassPanel.getBottom());
        reflection.closeSubPath();
        g.setColour (juce::Colours::white.withAlpha (0.022f));
        g.fillPath (reflection);
    }
}

void drawGlassWell (juce::Graphics& g, juce::Rectangle<float> bounds, float corner)
{
    g.setColour (obsidianWell.withAlpha (0.96f));
    g.fillRoundedRectangle (bounds, corner);

    // Soft top inner shadow.
    {
        juce::ColourGradient innerShadow (juce::Colours::black.withAlpha (0.42f),
                                          bounds.getTopLeft(),
                                          juce::Colours::transparentBlack,
                                          bounds.getBottomLeft(), false);
        g.setGradientFill (innerShadow);
        g.fillRoundedRectangle (bounds.reduced (0.8f), juce::jmax (1.0f, corner - 0.8f));
    }

    g.setColour (glassRimHighlight.withAlpha (0.18f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), corner, 0.7f);

    g.setColour (crimsonAccent.withAlpha (0.15f));
    g.drawRoundedRectangle (bounds.reduced (1.5f), juce::jmax (1.0f, corner - 1.2f), 0.7f);
}

void drawEngravedLabel (juce::Graphics& g, juce::Rectangle<float> bounds,
                        const juce::String& title, const juce::String& subtitle,
                        const juce::String& index)
{
    g.setColour (crimsonAccent.withAlpha (0.88f));
    g.fillRect (bounds.getX(), bounds.getY() + 2.0f, 2.0f, 8.0f);

    g.setColour (textSecondary);
    g.setFont (font (8.5f, FontWeight::semibold, 0.18f));
    g.drawText (title, bounds.withTrimmedLeft (7.0f).withHeight (12.0f),
                juce::Justification::centredLeft, false);

    g.setColour (textMuted);
    g.setFont (font (6.3f, FontWeight::regular, 0.14f));
    g.drawText (subtitle, bounds.withTrimmedLeft (7.0f).withTrimmedTop (13.0f).withHeight (10.0f),
                juce::Justification::centredLeft, false);

    if (index.isNotEmpty())
    {
        g.setColour (textMuted.withAlpha (0.82f));
        g.setFont (font (6.0f, FontWeight::semibold, 0.16f));
        g.drawText (index, bounds.removeFromTop (11.0f).removeFromRight (42.0f),
                    juce::Justification::centredRight, false);
    }
}

void drawSignalChip (juce::Graphics& g, juce::Rectangle<float> bounds, const juce::String& text,
                     bool active)
{
    g.setColour (obsidianScreen.withAlpha (0.90f));
    g.fillRoundedRectangle (bounds, 2.0f);

    g.setColour (active ? crimsonAccent.withAlpha (0.62f) : gunmetalLight.withAlpha (0.38f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 2.0f, 0.7f);

    if (active)
    {
        g.setColour (crimsonGlow.withAlpha (0.34f));
        g.fillRoundedRectangle (bounds.reduced (1.2f), 1.4f);
    }

    g.setColour (crimsonAccent.withAlpha (active ? 0.60f : 0.26f));
    g.fillRect (bounds.getX() + 4.0f, bounds.getBottom() - 1.4f, bounds.getWidth() - 8.0f, 0.8f);

    g.setColour (active ? textPrimary : textSecondary);
    g.setFont (font (5.9f, FontWeight::semibold, 0.16f));
    g.drawText (text, bounds, juce::Justification::centred, false);
}

void drawCrimsonDot (juce::Graphics& g, juce::Point<float> centre, float radius)
{
    g.setColour (crimsonGlow.withAlpha (0.28f));
    g.fillEllipse (centre.x - radius * 2.4f, centre.y - radius * 2.4f,
                   radius * 4.8f, radius * 4.8f);
    g.setColour (crimsonAccent);
    g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour (crimsonBright);
    g.fillEllipse (centre.x - radius * 0.45f, centre.y - radius * 0.45f,
                   radius * 0.9f, radius * 0.9f);
}

void drawMicroLabel (juce::Graphics& g, juce::Rectangle<float> bounds, const juce::String& text,
                     juce::Justification justification, juce::Colour colour)
{
    g.setColour (colour);
    g.setFont (font (6.2f, FontWeight::semibold, 0.16f));
    g.drawText (text, bounds, justification, false);
}
} // namespace macpressor::ui
