
#pragma once

#include <JuceHeader.h>

namespace AutoVocal
{
    enum class Artist
    {
        MADK1D,
        DarkPrince,
        OGBuda,
        BigBabyTape,
        MAYOT,
        DORA,
        KAIANGEL,
        MONA
    };

    enum class Mood
    {
        Aggressive,
        Melodic,
        Emotional,
        Dark,
        Chill,
        Club
    };

    enum class EvidenceLevel
    {
        Reconstructed,
        PartialEvidence,
        PubliclyDescribed
    };

    struct Profile
    {
        float drive;
        float saturation;
        float warmth;

        float compression;
        float threshold;
        float ratio;
        float attack;
        float release;

        float tuneAmount;
        float retuneSpeed;
        float humanize;

        float reverbMix;
        float delayMix;
        float stereoWidth;

        float output;
    };

    inline constexpr int artistCount = 8;
    inline constexpr int moodCount = 6;

    inline constexpr const char* artistNames[artistCount] =
    {
        "MADK1D",
        "Dark Prince",
        "OG Buda",
        "Big Baby Tape",
        "MAYOT",
        "DORA",
        "KAI ANGEL",
        "MONA"
    };

    inline constexpr const char* moodNames[moodCount] =
    {
        "Aggressive",
        "Melodic",
        "Emotional",
        "Dark",
        "Chill",
        "Club"
    };

    // Evidence level describes how much public information
    // is available about the artist's vocal production.
    //
    // It does NOT mean that the actual studio settings
    // have been recovered.

    inline constexpr EvidenceLevel evidenceLevels[artistCount] =
    {
        EvidenceLevel::PartialEvidence, // MADK1D
        EvidenceLevel::Reconstructed,  // Dark Prince
        EvidenceLevel::PartialEvidence, // OG Buda
        EvidenceLevel::Reconstructed,  // Big Baby Tape
        EvidenceLevel::Reconstructed,  // MAYOT
        EvidenceLevel::Reconstructed,  // DORA
        EvidenceLevel::Reconstructed,  // KAI ANGEL
        EvidenceLevel::PartialEvidence  // MONA
    };

    inline constexpr const char* evidenceNotes[artistCount] =
    {
        "Third-party vocal breakdowns describe pitch processing, "
        "parallel compression, vocal layers and reverb.",

        "No verified original vocal chain found. "
        "Sound-oriented reconstruction.",

        "Artist has publicly discussed learning Auto-Tune. "
        "Exact processing values are not publicly established.",

        "Sound-oriented reconstruction. "
        "Exact artist settings are not verified.",

        "Third-party tutorials demonstrate similar Auto-Tune "
        "production, not confirmed personal settings.",

        "Sound-oriented reconstruction. "
        "Exact artist settings are not verified.",

        "Third-party New Jazz vocal production breakdowns. "
        "Not an original artist session.",

        "Public description mentions minimal Auto-Tune use. "
        "Exact processing values are not verified."
    };

    // All values below are starting points for reconstruction.
    // They are NOT claimed to be the artists' actual plugin settings.

    inline constexpr Profile artistBases[artistCount] =
    {
        // MADK1D
        // Inspired by public breakdowns of the vocal sound:
        // pitch treatment, layered vocals, parallel compression.
        {
            0.30f, 0.28f, 0.25f,
            0.48f, -18.0f, 3.0f, 8.0f, 90.0f,
            0.90f, 8.0f, 12.0f,
            0.08f, 0.06f, 0.20f,
            1.0f
        },

        // Dark Prince
        {
            0.25f, 0.24f, 0.48f,
            0.50f, -19.0f, 3.2f, 10.0f, 110.0f,
            0.85f, 12.0f, 20.0f,
            0.12f, 0.08f, 0.18f,
            1.0f
        },

        // OG Buda
        // Auto-Tune is part of the artist's publicly discussed
        // production history, but exact values are unknown.
        {
            0.22f, 0.22f, 0.38f,
            0.45f, -18.0f, 2.8f, 12.0f, 115.0f,
            0.78f, 18.0f, 25.0f,
            0.10f, 0.09f, 0.24f,
            1.0f
        },

        // Big Baby Tape
        {
            0.30f, 0.30f, 0.22f,
            0.50f, -19.0f, 3.2f, 8.0f, 95.0f,
            0.92f, 7.0f, 10.0f,
            0.08f, 0.07f, 0.25f,
            1.0f
        },

        // MAYOT
        {
            0.25f, 0.23f, 0.32f,
            0.46f, -18.0f, 3.0f, 10.0f, 105.0f,
            0.88f, 10.0f, 18.0f,
            0.11f, 0.09f, 0.22f,
            1.0f
        },

        // DORA
        {
            0.12f, 0.15f, 0.45f,
            0.35f, -17.0f, 2.2f, 18.0f, 150.0f,
            0.68f, 25.0f, 55.0f,
            0.19f, 0.12f, 0.28f,
            1.0f
        },

        // KAI ANGEL
        // New Jazz inspired reconstruction.
        {
            0.32f, 0.30f, 0.25f,
            0.52f, -19.0f, 3.4f, 7.0f, 85.0f,
            0.95f, 5.0f, 8.0f,
            0.07f, 0.06f, 0.28f,
            1.0f
        },

        // MONA
        // Less aggressive pitch correction.
        {
            0.15f, 0.17f, 0.48f,
            0.38f, -17.0f, 2.3f, 20.0f, 160.0f,
            0.20f, 40.0f, 65.0f,
            0.20f, 0.14f, 0.30f,
            1.0f
        }
    };

    inline Profile getProfile(int artist, int mood)
    {
        artist = juce::jlimit(0, artistCount - 1, artist);
        mood = juce::jlimit(0, moodCount - 1, mood);

        Profile p = artistBases[artist];

        switch (mood)
        {
            case 0: // Aggressive
                p.drive += 0.08f;
                p.saturation += 0.07f;
                p.compression += 0.08f;
                p.threshold -= 1.0f;
                p.ratio += 0.4f;
                p.retuneSpeed -= 2.0f;
                p.humanize -= 5.0f;
                p.reverbMix *= 0.65f;
                break;

            case 1: // Melodic
                p.drive -= 0.04f;
                p.retuneSpeed += 4.0f;
                p.humanize += 15.0f;
                p.reverbMix += 0.05f;
                p.delayMix += 0.04f;
                break;

            case 2: // Emotional
                p.drive -= 0.07f;
                p.compression -= 0.06f;
                p.retuneSpeed += 10.0f;
                p.humanize += 25.0f;
                p.reverbMix += 0.08f;
                p.delayMix += 0.06f;
                break;

            case 3: // Dark
                p.warmth += 0.10f;
                p.saturation += 0.04f;
                p.reverbMix += 0.03f;
                p.stereoWidth -= 0.05f;
                break;

            case 4: // Chill
                p.drive -= 0.08f;
                p.compression -= 0.08f;
                p.retuneSpeed += 12.0f;
                p.humanize += 30.0f;
                p.reverbMix += 0.07f;
                p.delayMix += 0.07f;
                break;

            case 5: // Club
                p.drive += 0.05f;
                p.compression += 0.07f;
                p.ratio += 0.3f;
                p.stereoWidth += 0.08f;
                p.delayMix += 0.04f;
                break;
        }

        p.drive = juce::jlimit(0.0f, 0.50f, p.drive);
        p.saturation = juce::jlimit(0.0f, 0.45f, p.saturation);
        p.warmth = juce::jlimit(0.0f, 1.0f, p.warmth);

        p.compression = juce::jlimit(0.0f, 0.65f, p.compression);
        p.threshold = juce::jlimit(-30.0f, -8.0f, p.threshold);
        p.ratio = juce::jlimit(1.5f, 5.0f, p.ratio);
        p.attack = juce::jlimit(2.0f, 40.0f, p.attack);
        p.release = juce::jlimit(40.0f, 250.0f, p.release);

        p.tuneAmount = juce::jlimit(0.0f, 1.0f, p.tuneAmount);
        p.retuneSpeed = juce::jlimit(1.0f, 100.0f, p.retuneSpeed);
        p.humanize = juce::jlimit(0.0f, 100.0f, p.humanize);

        p.reverbMix = juce::jlimit(0.0f, 0.35f, p.reverbMix);
        p.delayMix = juce::jlimit(0.0f, 0.25f, p.delayMix);
        p.stereoWidth = juce::jlimit(0.0f, 0.65f, p.stereoWidth);

        p.output = 1.0f;

        return p;
    }
}
