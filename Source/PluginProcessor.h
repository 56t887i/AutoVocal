
#pragma once

#include <JuceHeader.h>

#include "Profiles.h"
#include "DSP/VocalCharacter.h"
#include "DSP/DynamicsProcessor.h"
#include "DSP/SpaceProcessor.h"
#include "DSP/PitchCorrector.h"
#include "DSP/NoiseReducer.h"

class AutoVocalAudioProcessor : public juce::AudioProcessor
{
public:
    AutoVocalAudioProcessor();
    ~AutoVocalAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(
        juce::AudioBuffer<float>& buffer,
        juce::MidiBuffer& midiMessages
    ) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;

    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout
        createParameterLayout();

    void applyProfile(int artist, int mood);

    float getInputLevel() const;
    float getOutputLevel() const;

    // NOISE REDUCTION CONTROL

    void startNoiseLearning();
    void stopNoiseLearning();

    float getNoiseLearnProgress() const;
    bool hasNoiseProfile() const;

    juce::AudioProcessorValueTreeState parameters;

private:
    std::atomic<float> inputLevel { 0.0f };
    std::atomic<float> outputLevel { 0.0f };

    // Noise learning requests are transferred safely
    // from the UI thread to the audio thread.
    std::atomic<bool> noiseLearnStartRequested { false };
    std::atomic<bool> noiseLearnStopRequested { false };

    std::atomic<float> noiseLearnProgress { 0.0f };
    std::atomic<bool> noiseProfileReady { false };

    int currentNoiseMode = -1;

    double currentSampleRate = 44100.0;

    // DSP modules
    NoiseReducer noiseReducer;
    PitchCorrector pitchCorrector;
    VocalCharacter vocalCharacter;
    DynamicsProcessor dynamicsProcessor;
    SpaceProcessor spaceProcessor;

    // Preallocated dry signal buffer
    juce::AudioBuffer<float> dryBuffer;

    float getParameterValue(const juce::String& parameterID) const;
    bool getParameterBool(const juce::String& parameterID) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(
        AutoVocalAudioProcessor
    )
};
