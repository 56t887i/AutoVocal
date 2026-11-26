
#pragma once

#include <JuceHeader.h>
#include <vector>

class SpaceProcessor
{
public:
    void prepare(double sampleRate, int samplesPerBlock, int channels);
    void reset();

    void process(
        juce::AudioBuffer<float>& buffer,
        bool reverbEnabled,
        int reverbType,
        float reverbMix,
        float preDelay,
        bool delayEnabled,
        int delayType,
        float delayTime,
        float delayFeedback,
        float delayMix
    );

private:
    juce::dsp::Reverb reverb;

    double sampleRate = 44100.0;

    std::vector<std::vector<float>> delayBuffers;
    std::vector<int> delayPositions;

    // Separate wet path: pre-delay must never shift the dry vocal.
    juce::AudioBuffer<float> reverbBuffer;
    std::vector<std::vector<float>> preDelayBuffers;
    std::vector<int> preDelayPositions;

    int maxDelaySamples = 0;
    int maxPreDelaySamples = 0;

    void processDelay(
        juce::AudioBuffer<float>& buffer,
        int delayType,
        float delayTime,
        float feedback,
        float mix
    );
};
