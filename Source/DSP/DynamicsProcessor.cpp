
#include "DynamicsProcessor.h"

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
    envelope = 0.0f;
    currentGain = 1.0f;
    attackCoefficient = 0.0f;
    releaseCoefficient = 0.0f;
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
    if (!enabled)
        return;

    threshold = juce::jlimit(-60.0f, 0.0f, threshold);
    ratio = juce::jlimit(1.0f, 20.0f, ratio);
    attack = juce::jlimit(0.1f, 200.0f, attack);
    release = juce::jlimit(5.0f, 1000.0f, release);
    makeup = juce::jlimit(0.0f, 18.0f, makeup);
    mix = juce::jlimit(0.0f, 1.0f, mix);

    const float attackSeconds = attack * 0.001f;
    const float releaseSeconds = release * 0.001f;

    attackCoefficient = std::exp(
        -1.0f /
        (static_cast<float>(sampleRate) * attackSeconds)
    );

    releaseCoefficient = std::exp(
        -1.0f /
        (static_cast<float>(sampleRate) * releaseSeconds)
    );

    const float makeupGain =
        juce::Decibels::decibelsToGain(makeup);

    const int channels = buffer.getNumChannels();
    const int samples = buffer.getNumSamples();

    for (int i = 0; i < samples; ++i)
    {
        // Linked stereo peak detector.
        float peak = 0.0f;

        for (int channel = 0; channel < channels; ++channel)
        {
            peak = juce::jmax(
                peak,
                std::abs(buffer.getSample(channel, i))
            );
        }

        // Envelope follower.
        const float envelopeCoefficient =
            peak > envelope
                ? attackCoefficient
                : releaseCoefficient;

        envelope =
            envelopeCoefficient * envelope
            + (1.0f - envelopeCoefficient) * peak;

        const float envelopeDb =
            juce::Decibels::gainToDecibels(
                envelope + 0.000001f
            );

        // Smooth 6 dB soft knee avoids an abrupt change in tone
        // when vocal syllables cross the compression threshold.
        constexpr float kneeWidthDb = 6.0f;
        const float halfKnee = kneeWidthDb * 0.5f;
        float targetGainDb = 0.0f;

        if (envelopeDb > threshold + halfKnee)
        {
            targetGainDb =
                (threshold + (envelopeDb - threshold) / ratio)
                - envelopeDb;
        }
        else if (envelopeDb > threshold - halfKnee)
        {
            const float kneePosition =
                envelopeDb - threshold + halfKnee;

            targetGainDb =
                (1.0f / ratio - 1.0f)
                * kneePosition * kneePosition
                / (2.0f * kneeWidthDb);
        }

        const float targetGain =
            juce::Decibels::decibelsToGain(targetGainDb);

        // Smooth gain reduction and recovery separately.
        const float gainCoefficient =
            targetGain < currentGain
                ? attackCoefficient
                : releaseCoefficient;

        currentGain =
            gainCoefficient * currentGain
            + (1.0f - gainCoefficient) * targetGain;

        const float wetGain = currentGain * makeupGain;

        for (int channel = 0; channel < channels; ++channel)
        {
            const float dry = buffer.getSample(channel, i);
            const float wet = dry * wetGain;

            buffer.setSample(
                channel,
                i,
                dry + (wet - dry) * mix
            );
        }
    }
}
