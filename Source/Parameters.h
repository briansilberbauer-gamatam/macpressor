#pragma once

#include <juce_core/juce_core.h>

#include <array>

namespace macpressor::params
{
inline constexpr const char* mode = "mode";
inline constexpr int numModes = 4;

inline const juce::StringArray& getModeNames()
{
    static const juce::StringArray names {
        "Vocal - Female",
        "Vocal - Male",
        "Guitar - Clean",
        "Guitar - Overdrive"
    };
    return names;
}

inline juce::String getModeName (int index)
{
    return getModeNames()[juce::jlimit (0, numModes - 1, index)];
}
} // namespace macpressor::params
