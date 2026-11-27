
#include "VocalCharacter.h"

#include <cmath>

void VocalCharacter::prepare(
    double newSampleRate,
    int,
    int channels)
{
    sampleRate = juce::jmax(1.0, newSampleRate);

    const int safeChannels = juce::jmax(1, channels);

    lowFrequencyState.setSize(safeChannels, 1);
    previousSaturationInput.setSize(safeChannels, 1);

    reset();
}

void VocalCharacter::reset()
{
    lowFrequencyState.clear();
    previousSaturationInput.clear();
}

float VocalCharacter::processSample(
    float input,
    float drive,
    float saturation,
    float warmth,
    float softClip,
    float& lowState,
    float& previousDrivenInput)
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

    // ADAA (antiderivative anti-aliasing) soft saturation.
    // This substantially reduces the harsh fold-back harmonics that
    // a direct tanh/rational waveshaper creates at high drive.
    const float driveScale =
        1.0f + drive * 5.0f + saturation * 8.0f + softClip * 3.0f;

    const float saturationAmount =
        juce::jlimit(
            0.0f,
            1.0f,
            saturation + drive * 0.35f + softClip * 0.35f
        );

    const float drivenInput = warmSignal * driveScale;
    const float previousDriven = previousDrivenInput;

    const auto logCosh = [](float value)
    {
        const float magnitude = std::abs(value);

        return magnitude
            + std::log1p(std::exp(-2.0f * magnitude))
            - 0.69314718056f;
    };

    const float difference = drivenInput - previousDriven;
    float saturated = 0.0f;

    if (std::abs(difference) > 0.0001f)
    {
        // First-order ADAA: divided difference of log(cosh).
        saturated =
            (logCosh(drivenInput) - logCosh(previousDriven))
            / (driveScale * difference);
    }
    else
    {
        const float midpoint = 0.5f * (drivenInput + previousDriven);
        saturated = std::tanh(midpoint) / driveScale;
    }

    previousDrivenInput = drivenInput;

    // Keep the clean component intact and blend in controlled harmonics.
    const float result = juce::jmap(
        saturationAmount,
        warmSignal,
        saturated
    );

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

        float previousDrivenInput =
            previousSaturationInput.getSample(channel, 0);

        for (int i = 0; i < samples; ++i)
        {
            const float dry = buffer.getSample(channel, i);

            const float wet = processSample(
                dry,
                drive,
                saturation,
                warmth,
                softClip,
                lowState,
                previousDrivenInput
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

        previousSaturationInput.setSample(
            channel,
            0,
            previousDrivenInput
        );
    }
}
