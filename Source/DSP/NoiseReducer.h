#pragma once

#include <JuceHeader.h>
#include <array>
#include <memory>
#include <cstdint>
#include <atomic>

class NoiseReducer
{
public:
    enum class Mode
    {
        Adaptive = 0,
        Learn,
        Profile
    };

    static constexpr int fftOrder = 10;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int hopSize = fftSize / 4;
    static constexpr int latencySamples = fftSize - 1;

    NoiseReducer();

    void prepare(
        double sampleRate,
        int samplesPerBlock,
        int channels
    );

    void reset();

    void process(
        juce::AudioBuffer<float>& buffer,
        bool enabled,
        float reduction
    );

    void setMode(Mode newMode);
    Mode getMode() const;

    void startLearning();
    void stopLearning();

    bool hasLearnedProfile() const;
    float getLearnProgress() const;

    int getLatencySamples() const;

    // Save/load the learned noise profile.
    void saveProfile(juce::MemoryBlock& destination) const;
    bool loadProfile(const void* data, int sizeInBytes);
    void clearProfile();

private:
    static constexpr int maxChannels = 2;
    static constexpr int ringSize = 8192;
    static constexpr int ringMask = ringSize - 1;
    static constexpr int binCount = fftSize / 2 + 1;
    static constexpr int learningFramesTarget = 150;

    static constexpr int profileMagic = 0x41564E52;
    static constexpr int profileVersion = 1;

    using FftBuffer = std::array<float, fftSize * 2>;
    using RingBuffer = std::array<float, ringSize>;

    static int getRingIndex(int64_t position);

    void processFrame(int64_t frameStart);

    void updateNoiseEstimate(
        const std::array<float, binCount>& magnitudes,
        float frameRms
    );

    float getNoiseMagnitude(int bin) const;

    double currentSampleRate = 44100.0;

    int channelCount = 2;

    std::unique_ptr<juce::dsp::FFT> fft;

    std::array<FftBuffer, maxChannels> fftBuffers;

    std::array<RingBuffer, maxChannels> inputRing;
    std::array<RingBuffer, maxChannels> outputRing;

    std::array<float, fftSize> window {};

    // Atomic profile values allow the state thread to save or
    // load data without a C++ data race with the audio thread.
    std::array<std::atomic<float>, binCount> noiseProfile {};

    std::array<float, binCount> adaptiveNoise {};
    std::array<float, binCount> binGains {};
    std::array<float, binCount> smoothedGains {};

    std::atomic<bool> profileAvailable { false };

    // Changes whenever the profile starts or finishes being
    // replaced. State saving uses it to reject inconsistent copies.
    std::atomic<uint64_t> profileRevision { 0 };

    int64_t sampleCounter = 0;

    int learnedFrames = 0;

    Mode mode = Mode::Adaptive;

    float currentReduction = 50.0f;

    float adaptiveFrameThreshold = 0.015f;

    float gainAttackCoefficient = 0.0f;
    float gainReleaseCoefficient = 0.0f;
};