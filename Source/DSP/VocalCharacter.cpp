
#include "VocalCharacter.h"

void VocalCharacter::prepare(
    double newSampleRate,
    int,
    int channels)
{
    sampleRate = juce::jmax(1.0, newSampleRate);

    lowFrequencyState.setSize(
        juce::jmax(1, channels),
        1
    );

    reset();
}

void VocalCharacter::reset()
{
    lowFrequencyState.clear();
}

float VocalCharacter::processSample(
    float input,
    float drive,
    float saturation,
    float warmth,
    float softClip,
    float& lowState)
{
    drive = juce::jlimit(0.0f, 1.0f, drive);
    saturation = juce::jlimit(0.0f, 1.0f, saturation);
    warmth = juce::jlimit(0.0f, 1.0f, warmth);
    softClip = juce::jlimit(0.0f, 1.0f, softClip);

    // Controlled warmth.
    const float cutoff = 180.0f + warmth * 420.0f;

    const float coefficient =
        1.0f - std::exp(
            -2.0f * juce::MathConstants<float>::pi
            * cutoff / static_cast<float>(sampleRate)
        );

    lowState += coefficient * (input - lowState);

    const float warmSignal =
        input + lowState * warmth * 0.12f;

    // Drive and saturation work together.
    // No large input gain is applied.
    const float driveAmount =
        drive * 2.5f + saturation * 1.5f;

    const float saturationAmount =
        juce::jlimit(
            0.0f,
            1.0f,
            saturation + drive * 0.35f
        );

    // Smooth rational saturation curve.
    // It does not create uncontrolled output gain.
    const float saturated =
        warmSignal /
        (1.0f + driveAmount * std::abs(warmSignal));

    float result = juce::jmap(
        saturationAmount,
        warmSignal,
        saturated
    );

    // Additional gentle soft clipping.
    if (softClip > 0.001f)
    {
        const float clipped =
            result /
            (1.0f + softClip * 2.0f * std::abs(result));

        result = juce::jmap(
            softClip,
            result,
            clipped
        );
    }

    return result;
}

void VocalCharacter::process(
    juce::AudioBuffer<float>& buffer,
    float drive,
    float saturation,
    float warmth,
    float softClip,
    float mix)
{
    drive = juce::jlimit(0.0f, 1.0f, drive);
    saturation = juce::jlimit(0.0f, 1.0f, saturation);
    warmth = juce::jlimit(0.0f, 1.0f, warmth);
    softClip = juce::jlimit(0.0f, 1.0f, softClip);
    mix = juce::jlimit(0.0f, 1.0f, mix);

    const int channels = juce::jmin(
        buffer.getNumChannels(),
        lowFrequencyState.getNumChannels()
    );

    const int samples = buffer.getNumSamples();

    for (int channel = 0; channel < channels; ++channel)
    {
        float lowState =
            lowFrequencyState.getSample(channel, 0);

        for (int i = 0; i < samples; ++i)
        {
            const float dry = buffer.getSample(channel, i);

            const float wet = processSample(
                dry,
                drive,
                saturation,
                warmth,
                softClip,
                lowState
            );

            buffer.setSample(
                channel,
                i,
                dry + (wet - dry) * mix
            );
        }

        lowFrequencyState.setSample(
            channel,
            0,
            lowState
        );
    }
}
