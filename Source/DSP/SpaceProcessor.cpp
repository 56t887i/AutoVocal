
#include "SpaceProcessor.h"

void SpaceProcessor::prepare(
    double newSampleRate,
    int,
    int channels)
{
    sampleRate = juce::jmax(1.0, newSampleRate);

    maxDelaySamples = static_cast<int>(sampleRate * 2.0);

    delayBuffers.clear();
    delayPositions.clear();

    const int channelCount = juce::jmax(1, channels);

    delayBuffers.resize(static_cast<size_t>(channelCount));
    delayPositions.resize(static_cast<size_t>(channelCount), 0);

    for (auto& channelBuffer : delayBuffers)
    {
        channelBuffer.assign(
            static_cast<size_t>(maxDelaySamples + 1),
            0.0f
        );
    }

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = 32768;
    spec.numChannels = static_cast<juce::uint32>(channelCount);

    reverb.prepare(spec);

    reset();
}

void SpaceProcessor::reset()
{
    for (auto& channelBuffer : delayBuffers)
        std::fill(channelBuffer.begin(), channelBuffer.end(), 0.0f);

    std::fill(delayPositions.begin(), delayPositions.end(), 0);

    reverb.reset();
}

void SpaceProcessor::processDelay(
    juce::AudioBuffer<float>& buffer,
    int delayType,
    float delayTime,
    float feedback,
    float mix)
{
    delayTime = juce::jlimit(1.0f, 2000.0f, delayTime);
    feedback = juce::jlimit(0.0f, 0.90f, feedback);
    mix = juce::jlimit(0.0f, 1.0f, mix);

    const int channels = juce::jmin(
        buffer.getNumChannels(),
        static_cast<int>(delayBuffers.size())
    );

    const int samples = buffer.getNumSamples();

    const int delaySamples = juce::jlimit(
        1,
        maxDelaySamples,
        static_cast<int>(sampleRate * delayTime * 0.001)
    );

    for (int i = 0; i < samples; ++i)
    {
        float delayedSamples[2] = { 0.0f, 0.0f };
        float inputSamples[2] = { 0.0f, 0.0f };

        for (int channel = 0; channel < channels; ++channel)
        {
            inputSamples[channel] = buffer.getSample(channel, i);

            int readPosition =
                delayPositions[static_cast<size_t>(channel)] - delaySamples;

            if (readPosition < 0)
                readPosition += maxDelaySamples + 1;

            delayedSamples[channel] =
                delayBuffers[static_cast<size_t>(channel)]
                            [static_cast<size_t>(readPosition)];
        }

        for (int channel = 0; channel < channels; ++channel)
        {
            float wet = delayedSamples[channel];

            if (delayType == 1 && channels >= 2)
            {
                wet = delayedSamples[channel] * 0.75f
                    + delayedSamples[1 - channel] * 0.25f;
            }

            const float input = inputSamples[channel];

            delayBuffers[static_cast<size_t>(channel)]
                        [static_cast<size_t>(
                            delayPositions[static_cast<size_t>(channel)])] =
                input + delayedSamples[channel] * feedback;

            delayPositions[static_cast<size_t>(channel)]++;

            if (delayPositions[static_cast<size_t>(channel)] >= maxDelaySamples + 1)
                delayPositions[static_cast<size_t>(channel)] = 0;

            buffer.setSample(
                channel,
                i,
                input * (1.0f - mix) + wet * mix
            );
        }
    }
}

void SpaceProcessor::process(
    juce::AudioBuffer<float>& buffer,
    bool reverbEnabled,
    int reverbType,
    float reverbMix,
    float preDelay,
    bool delayEnabled,
    int delayType,
    float delayTime,
    float delayFeedback,
    float delayMix)
{
    const int channels = buffer.getNumChannels();

    if (reverbEnabled && reverbMix > 0.001f)
    {
        juce::dsp::Reverb::Parameters parameters;

        if (reverbType == 0)
        {
            parameters.roomSize = 0.35f;
            parameters.damping = 0.65f;
        }
        else if (reverbType == 1)
        {
            parameters.roomSize = 0.85f;
            parameters.damping = 0.35f;
        }
        else
        {
            parameters.roomSize = 0.65f;
            parameters.damping = 0.50f;
        }

        parameters.wetLevel = juce::jlimit(0.0f, 1.0f, reverbMix);
        parameters.dryLevel = 1.0f - parameters.wetLevel;
        parameters.width = 1.0f;
        parameters.freezeMode = 0.0f;

        reverb.setParameters(parameters);

        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);

        reverb.process(context);
    }

    // Predelay will be implemented as a separate stage later.
    juce::ignoreUnused(preDelay, channels);

    if (delayEnabled && delayMix > 0.001f)
    {
        processDelay(
            buffer,
            delayType,
            delayTime,
            delayFeedback * 0.01f,
            delayMix
        );
    }
}
