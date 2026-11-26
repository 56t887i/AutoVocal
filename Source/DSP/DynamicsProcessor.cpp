#include "DynamicsProcessor.h"

#include <cmath>

void DynamicsProcessor::prepare(
    double newSampleRate,
    int,
    int)
{
    sampleRate = juce::jmax(1.0, newSampleRate);
    reset();
}

void DynamicsProcessor::reset()
{
    peakEnvelope = 0.0f;
    rmsEnvelopeSquared = 0.0f;
    currentGain = 1.0f;
    currentMix = 0.0f;

    attackCoefficient = 0.0f;
    releaseCoefficient = 0.0f;
    rmsCoefficient = 0.0f;
    mixCoefficient = 0.0f;
}

void DynamicsProcessor::process(
    juce::AudioBuffer<float>& buffer,
    bool enabled,
    float threshold,
    float ratio,
    float attack,
    float release,
    float makeup,
    float mix)
{
    threshold = juce::jlimit(-60.0f, 0.0f, threshold);
    ratio = juce::jlimit(1.0f, 20.0f, ratio);
    attack = juce::jlimit(0.1f, 200.0f, attack);
    release = juce::jlimit(5.0f, 1000.0f, release);
    makeup = juce::jlimit(0.0f, 18.0f, makeup);
    mix = juce::jlimit(0.0f, 1.0f, mix);

    const float rate = static_cast<float>(sampleRate);
    const float attackSeconds = attack * 0.001f;
    const float releaseSeconds = release * 0.001f;

    attackCoefficient = std::exp(-1.0f / (rate * attackSeconds));
    releaseCoefficient = std::exp(-1.0f / (rate * releaseSeconds));

    // A 10 ms RMS window steadies the detector across voiced syllables,
    // while the peak path keeps consonants and fast transients controlled.
    rmsCoefficient = std::exp(-1.0f / (rate * 0.010f));

    // Smooth bypass and wet/dry automation to prevent abrupt gain jumps.
    mixCoefficient = std::exp(-1.0f / (rate * 0.010f));

    const float targetMix = enabled ? mix : 0.0f;
    const float makeupGain = juce::Decibels::decibelsToGain(makeup);

    const int channels = buffer.getNumChannels();
    const int samples = buffer.getNumSamples();

    if (channels <= 0 || samples <= 0)
        return;

    for (int i = 0; i < samples; ++i)
    {
        float peak = 0.0f;
        float meanSquare = 0.0f;

        // Linked detection prevents the stereo image from moving when
        // one side of a stereo vocal/effect is louder than the other.
        for (int channel = 0; channel < channels; ++channel)
        {
            const float value = buffer.getSample(channel, i);
            peak = juce::jmax(peak, std::abs(value));
            meanSquare += value * value;
        }

        meanSquare /= static_cast<float>(channels);

        const float peakCoefficient =
            peak > peakEnvelope ? attackCoefficient : releaseCoefficient;

        peakEnvelope =
            peakCoefficient * peakEnvelope
            + (1.0f - peakCoefficient) * peak;

        rmsEnvelopeSquared =
            rmsCoefficient * rmsEnvelopeSquared
            + (1.0f - rmsCoefficient) * meanSquare;

        const float rmsLevel = std::sqrt(
            juce::jmax(0.0f, rmsEnvelopeSquared)
        );

        // Peak-forward hybrid detection keeps punch without letting every
        // tiny consonant spike dictate the entire gain envelope.
        const float detectorLevel =
            0.70f * peakEnvelope + 0.30f * rmsLevel;

        float targetGainDb = 0.0f;

        if (enabled)
        {
            const float detectorDb =
                juce::Decibels::gainToDecibels(
                    detectorLevel + 0.000001f
                );

            constexpr float kneeWidthDb = 6.0f;
            constexpr float halfKneeDb = kneeWidthDb * 0.5f;

            if (detectorDb > threshold + halfKneeDb)
            {
                targetGainDb =
                    threshold + (detectorDb - threshold) / ratio
                    - detectorDb;
            }
            else if (detectorDb > threshold - halfKneeDb)
            {
                const float kneePosition =
                    detectorDb - threshold + halfKneeDb;

                targetGainDb =
                    (1.0f / ratio - 1.0f)
                    * kneePosition * kneePosition
                    / (2.0f * kneeWidthDb);
            }
        }

        const float targetGain =
            juce::Decibels::decibelsToGain(targetGainDb);

        const float gainCoefficient =
            targetGain < currentGain
                ? attackCoefficient
                : releaseCoefficient;

        currentGain =
            gainCoefficient * currentGain
            + (1.0f - gainCoefficient) * targetGain;

        currentMix =
            mixCoefficient * currentMix
            + (1.0f - mixCoefficient) * targetMix;

        const float wetGain = currentGain * makeupGain;

        for (int channel = 0; channel < channels; ++channel)
        {
            const float dry = buffer.getSample(channel, i);
            const float wet = dry * wetGain;

            buffer.setSample(
                channel,
                i,
                dry + (wet - dry) * currentMix
            );
        }
    }
}
