#pragma once

#include <juce_core/juce_core.h>

#include <array>

namespace macpressor::dsp
{
enum class Voice : int
{
    vocalFemale = 0,
    vocalMale = 1,
    guitarClean = 2,
    guitarOverdrive = 3
};

inline constexpr int numVoices = 4;

struct VoiceProfile
{
    Voice voice = Voice::vocalFemale;

    const char* family = "VOCAL";
    const char* character = "FEMALE";
    const char* title = "VOCAL / FEMALE";
    const char* codeName = "SILK";
    const char* tagline = "Gentle grip with a quick, open release.";

    // Fixed, voice-specific compression curve. These values are intentionally
    // not user-adjustable: the switch is the entire interface.
    float attackMs = 8.0f;
    float releaseMs = 140.0f;
    float ratio = 3.0f;
    float thresholdDb = -26.0f;
    float kneeDb = 8.0f;
    float detectorHpfHz = 110.0f;

    // Parallel blend (1.0 = fully compressed), automatic make-up amount,
    // and a small fixed output trim.
    float parallelMix = 0.92f;
    float autoMakeup = 0.74f;
    float trimDb = 0.0f;

    // Gentle analogue-style harmonic rounding, 0.0 = clean.
    float colour = 0.06f;
};

inline const std::array<VoiceProfile, numVoices>& getVoiceProfiles()
{
    static const std::array<VoiceProfile, numVoices> profiles { {
        { Voice::vocalFemale, "VOCAL", "FEMALE", "VOCAL / FEMALE", "SILK",
          "Gentle grip with a quick, open release.",
          8.0f, 140.0f, 3.0f, -26.0f, 8.0f, 110.0f, 0.92f, 0.74f, 0.0f, 0.06f },

        { Voice::vocalMale, "VOCAL", "MALE", "VOCAL / MALE", "ANCHOR",
          "Firmer, slower grip that steadies the body.",
          13.0f, 180.0f, 3.3f, -25.0f, 7.0f, 85.0f, 0.95f, 0.72f, 0.2f, 0.09f },

        { Voice::guitarClean, "GUITAR", "CLEAN", "GUITAR / CLEAN", "SPRING",
          "Pick attack intact, chords glued evenly.",
          24.0f, 240.0f, 2.4f, -23.0f, 10.0f, 130.0f, 0.88f, 0.68f, 0.0f, 0.05f },

        { Voice::guitarOverdrive, "GUITAR", "OVERDRIVE", "GUITAR / OVERDRIVE", "IRON",
          "Fast, tight leveling for saturated amps.",
          4.0f, 95.0f, 4.0f, -29.0f, 4.5f, 170.0f, 1.0f, 0.78f, -0.3f, 0.14f }
    } };

    return profiles;
}

inline const VoiceProfile& getVoiceProfile (Voice voice)
{
    const auto index = juce::jlimit (0, numVoices - 1, static_cast<int> (voice));
    return getVoiceProfiles()[static_cast<size_t> (index)];
}

inline const VoiceProfile& getVoiceProfile (int index)
{
    return getVoiceProfile (static_cast<Voice> (juce::jlimit (0, numVoices - 1, index)));
}

inline juce::String getVoiceTitle (int index)
{
    return getVoiceProfile (index).title;
}
} // namespace macpressor::dsp
