
#pragma once

#include <JuceHeader.h>

class DynamicsProcessor
{
public:
    void prepare(double sampleRate, int samplesPerBlock, int channels);
    void reset();

    void process(
        juce::AudioBuffer<float>& buffer,
        bool enabled,
        float threshold,
        float ratio,
        float attack,
        float release,
        float makeup,
        float mix
    );

private:
    double sampleRate = 44100.0;

    float envelope = 0.0f;
    float currentGain = 1.0f;

    float attackCoefficient = 0.0f;
    float releaseCoefficient = 0.0f;
};
