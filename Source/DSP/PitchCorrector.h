#pragma once

#include <JuceHeader.h>
#include <vector>

class PitchCorrector
{
public:
    void prepare(double sampleRate, int samplesPerBlock, int channels);
    void reset();

    void process(
        juce::AudioBuffer<float>& buffer,
        bool enabled,
        int key,
        int scale,
        float amount,
        float retuneSpeed,
        float humanize,
        float flexTune,
        int mode
    );

    int getLatencySamples() const;

private:
    float detectPitch();
    int findTargetNote(float midiNote, int key, int scale) const;
    float readDelayLine(const std::vector<float>& data, float delay) const;

    double currentSampleRate = 44100.0;

    int channelCount = 2;
    int writePosition = 0;

    int latencySamples = 1323;
    int minimumDelay = 500;
    int maximumDelay = 2200;

    std::vector<std::vector<float>> delayBuffers;

    std::vector<float> analysisBuffer;
    int analysisWritePosition = 0;
    int samplesSinceAnalysis = 0;

    float detectedFrequency = 0.0f;
    float currentSemitoneShift = 0.0f;
    float targetSemitoneShift = 0.0f;

    float pitchPhase = 0.0f;

    bool analysisReady = false;
};