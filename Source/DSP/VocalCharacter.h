
#pragma once

#include <JuceHeader.h>

class VocalCharacter
{
public:
    void prepare(double sampleRate, int samplesPerBlock, int channels);
    void reset();

    void process(
        juce::AudioBuffer<float>& buffer,
        float drive,
        float saturation,
        float warmth,
        float softClip,
        float mix
    );

private:
    double sampleRate = 44100.0;

    juce::AudioBuffer<float> lowFrequencyState;
    juce::AudioBuffer<float> previousSaturationInput;

    float processSample(
        float input,
        float drive,
        float saturation,
        float warmth,
        float softClip,
        float& lowState,
        float& previousDrivenInput
    );
};
