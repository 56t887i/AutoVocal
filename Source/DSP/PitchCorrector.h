#pragma once

#include <JuceHeader.h>
#include <array>
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

    // Pitch analysis runs at one quarter of the audio sample rate.
    // A short low-pass FIR is applied before decimation to suppress
    // harmonics folding into the pitch detector's analysis band.
    static constexpr int analysisSize = 2048;
    static constexpr int decimationFactor = 4;
    static constexpr int analysisWindow = 512;
    static constexpr int analysisHop = 256;
    static constexpr int analysisFirSize = 15;

    std::vector<float> analysisBuffer;
    std::array<float, analysisFirSize> analysisFirHistory {};
    int analysisFirWritePosition = 0;
    int decimationCounter = 0;
    int analysisWritePosition = 0;
    int samplesSinceAnalysis = 0;
    int analysisSamplesWritten = 0;
    int voicedHoldSamples = 0;

    float detectedFrequency = 0.0f;
    float currentSemitoneShift = 0.0f;
    float targetSemitoneShift = 0.0f;

    float pitchPhase = 0.0f;

    bool analysisReady = false;
};
