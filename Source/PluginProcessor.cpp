#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

//============================================================
// CONSTRUCTOR
//============================================================

AutoVocalAudioProcessor::AutoVocalAudioProcessor()
    : AudioProcessor(
        BusesProperties()
            .withInput(
                "Input",
                juce::AudioChannelSet::stereo(),
                true
            )
            .withOutput(
                "Output",
                juce::AudioChannelSet::stereo(),
                true
            )
    ),
    parameters(
        *this,
        nullptr,
        "PARAMETERS",
        createParameterLayout()
    )
{
}

//============================================================
// PARAMETERS
//============================================================

juce::AudioProcessorValueTreeState::ParameterLayout
AutoVocalAudioProcessor::createParameterLayout()
{
    using namespace juce;

    AudioProcessorValueTreeState::ParameterLayout layout;

    auto addFloat = [&layout](
        const String& id,
        const String& name,
        float minimum,
        float maximum,
        float defaultValue,
        float interval = 0.01f)
    {
        layout.add(
            std::make_unique<AudioParameterFloat>(
                id,
                name,
                NormalisableRange<float>(
                    minimum,
                    maximum,
                    interval
                ),
                defaultValue
            )
        );
    };

    auto addBool = [&layout](
        const String& id,
        const String& name,
        bool defaultValue)
    {
        layout.add(
            std::make_unique<AudioParameterBool>(
                id,
                name,
                defaultValue
            )
        );
    };

    auto addChoice = [&layout](
        const String& id,
        const String& name,
        StringArray choices,
        int defaultIndex)
    {
        layout.add(
            std::make_unique<AudioParameterChoice>(
                id,
                name,
                choices,
                defaultIndex
            )
        );
    };

    // MAIN

    addFloat("input", "Input", -24.0f, 24.0f, 0.0f);
    addFloat("output", "Output", -24.0f, 12.0f, 0.0f);
    addFloat("mix", "Mix", 0.0f, 100.0f, 100.0f);

    // PITCH

    addBool("tuneEnabled", "Tune Enabled", false);

    addChoice(
        "tuneKey",
        "Key",
        {
            "C", "C#", "D", "D#", "E", "F",
            "F#", "G", "G#", "A", "A#", "B"
        },
        0
    );

    addChoice(
        "tuneScale",
        "Scale",
        {
            "Chromatic",
            "Major",
            "Minor"
        },
        0
    );

    addFloat("tuneAmount", "Tune Amount", 0.0f, 100.0f, 0.0f);
    addFloat("retuneSpeed", "Retune Speed", 1.0f, 100.0f, 20.0f);
    addFloat("humanize", "Humanize", 0.0f, 100.0f, 20.0f);
    addFloat("flexTune", "Flex Tune", 0.0f, 100.0f, 20.0f);

    addChoice(
        "tuneMode",
        "Tune Mode",
        {
            "Natural",
            "Hard Tune"
        },
        0
    );

    // CLEAN

    addBool("cleanEnabled", "Clean Enabled", false);

    addFloat(
        "noiseReduction",
        "Noise Reduction",
        0.0f,
        100.0f,
        0.0f
    );

    addChoice(
        "noiseMode",
        "Noise Mode",
        {
            "Adaptive",
            "Profile"
        },
        0
    );

    // CHARACTER

    addFloat("drive", "Drive", 0.0f, 1.0f, 0.25f);
    addFloat("saturation", "Saturation", 0.0f, 1.0f, 0.25f);
    addFloat("warmth", "Warmth", 0.0f, 1.0f, 0.30f);
    addFloat("softClip", "Soft Clip", 0.0f, 100.0f, 0.0f);
    addFloat("characterMix", "Character Mix", 0.0f, 100.0f, 100.0f);

    // DYNAMICS

    addBool("compressorEnabled", "Compressor Enabled", true);
    addFloat("threshold", "Threshold", -40.0f, 0.0f, -18.0f);
    addFloat("ratio", "Ratio", 1.0f, 12.0f, 3.0f);
    addFloat("attack", "Attack", 0.1f, 100.0f, 10.0f);
    addFloat("release", "Release", 10.0f, 500.0f, 100.0f);
    addFloat("makeup", "Makeup", 0.0f, 18.0f, 0.0f);
    addFloat("compressorMix", "Compressor Mix", 0.0f, 100.0f, 100.0f);

    // SPACE

    addBool("reverbEnabled", "Reverb Enabled", true);

    addChoice(
        "reverbType",
        "Reverb Type",
        {
            "Room",
            "Hall",
            "Plate"
        },
        2
    );

    addFloat("reverbMix", "Reverb Mix", 0.0f, 100.0f, 12.0f);
    addFloat("preDelay", "Pre Delay", 0.0f, 150.0f, 20.0f);

    addBool("delayEnabled", "Delay Enabled", false);

    addChoice(
        "delayType",
        "Delay Type",
        {
            "Slap",
            "Stereo"
        },
        0
    );

    addFloat("delayTime", "Delay Time", 1.0f, 1000.0f, 250.0f);
    addFloat("delayFeedback", "Delay Feedback", 0.0f, 90.0f, 25.0f);
    addFloat("delayMix", "Delay Mix", 0.0f, 100.0f, 0.0f);

    addFloat("stereoWidth", "Stereo Width", 0.0f, 200.0f, 100.0f);

    return layout;
}

//============================================================
// PARAMETER HELPERS
//============================================================

float AutoVocalAudioProcessor::getParameterValue(
    const juce::String& parameterID) const
{
    if (auto* value = parameters.getRawParameterValue(parameterID))
        return value->load();

    return 0.0f;
}

bool AutoVocalAudioProcessor::getParameterBool(
    const juce::String& parameterID) const
{
    return getParameterValue(parameterID) >= 0.5f;
}

//============================================================
// NOISE REDUCTION CONTROL
//============================================================

void AutoVocalAudioProcessor::startNoiseLearning()
{
    if (auto* parameter = parameters.getParameter("cleanEnabled"))
    {
        parameter->setValueNotifyingHost(
            parameter->convertTo0to1(1.0f)
        );
    }

    if (auto* parameter = parameters.getParameter("noiseMode"))
    {
        parameter->setValueNotifyingHost(
            parameter->convertTo0to1(1.0f)
        );
    }

    noiseLearnProgress.store(0.0f);
    noiseProfileReady.store(false);

    noiseLearnStopRequested.store(false);
    noiseLearnStartRequested.store(true);
}

void AutoVocalAudioProcessor::stopNoiseLearning()
{
    noiseLearnStopRequested.store(true);
}

float AutoVocalAudioProcessor::getNoiseLearnProgress() const
{
    return noiseLearnProgress.load();
}

bool AutoVocalAudioProcessor::hasNoiseProfile() const
{
    return noiseProfileReady.load();
}

//============================================================
// PREPARE
//============================================================

void AutoVocalAudioProcessor::prepareToPlay(
    double sampleRate,
    int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    const int channels = juce::jmax(
        1,
        getTotalNumInputChannels()
    );

    noiseReducer.prepare(
        sampleRate,
        samplesPerBlock,
        channels
    );

    pitchCorrector.prepare(
        sampleRate,
        samplesPerBlock,
        channels
    );

    setLatencySamples(
        noiseReducer.getLatencySamples()
        + pitchCorrector.getLatencySamples()
    );

    vocalCharacter.prepare(
        sampleRate,
        samplesPerBlock,
        channels
    );

    dynamicsProcessor.prepare(
        sampleRate,
        samplesPerBlock,
        channels
    );

    spaceProcessor.prepare(
        sampleRate,
        samplesPerBlock,
        channels
    );

    dryBuffer.setSize(
        juce::jmax(2, channels),
        juce::jmax(32768, samplesPerBlock),
        false,
        false,
        true
    );

    dryBuffer.clear();

    noiseLearnStartRequested.store(false);
    noiseLearnStopRequested.store(false);

    noiseLearnProgress.store(0.0f);
    noiseProfileReady.store(noiseReducer.hasLearnedProfile());

    currentNoiseMode = -1;

    inputLevel.store(0.0f);
    outputLevel.store(0.0f);
}

void AutoVocalAudioProcessor::releaseResources()
{
    noiseReducer.reset();
    pitchCorrector.reset();
    vocalCharacter.reset();
    dynamicsProcessor.reset();
    spaceProcessor.reset();

    dryBuffer.clear();
}

//============================================================
// BUS LAYOUT
//============================================================

bool AutoVocalAudioProcessor::isBusesLayoutSupported(
    const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    if (input != output)
        return false;

    return input == juce::AudioChannelSet::mono()
        || input == juce::AudioChannelSet::stereo();
}

//============================================================
// AUDIO PROCESSING
//============================================================

void AutoVocalAudioProcessor::processBlock(
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);

    juce::ScopedNoDenormals noDenormals;

    const int channels = buffer.getNumChannels();
    const int samples = buffer.getNumSamples();

    if (channels <= 0 || samples <= 0)
        return;

    const int inputChannels = getTotalNumInputChannels();
    const int outputChannels = getTotalNumOutputChannels();

    for (int channel = inputChannels; channel < outputChannels; ++channel)
        buffer.clear(channel, 0, samples);

    // Input meter.
    float inputPeak = 0.0f;

    for (int channel = 0; channel < inputChannels; ++channel)
    {
        inputPeak = juce::jmax(
            inputPeak,
            buffer.getMagnitude(channel, 0, samples)
        );
    }

    inputLevel.store(inputPeak);

    const float inputGain =
        juce::Decibels::decibelsToGain(
            getParameterValue("input")
        );

    const float outputGain =
        juce::Decibels::decibelsToGain(
            getParameterValue("output")
        );

    const float globalMix = juce::jlimit(
        0.0f,
        1.0f,
        getParameterValue("mix") * 0.01f
    );

    // Input gain.
    buffer.applyGain(inputGain);

    //========================================================
    // NOISE REDUCTION MODE
    //========================================================

    if (noiseLearnStartRequested.exchange(false))
    {
        noiseReducer.startLearning();

        currentNoiseMode = 1;

        noiseLearnProgress.store(0.0f);
        noiseProfileReady.store(false);
    }
    else
    {
        const int requestedMode = juce::jlimit(
            0,
            1,
            static_cast<int>(
                std::round(getParameterValue("noiseMode"))
            )
        );

        if (requestedMode != currentNoiseMode)
        {
            noiseReducer.setMode(
                requestedMode == 0
                    ? NoiseReducer::Mode::Adaptive
                    : NoiseReducer::Mode::Profile
            );

            currentNoiseMode = requestedMode;
        }
    }

    if (noiseLearnStopRequested.exchange(false))
    {
        noiseReducer.stopLearning();

        currentNoiseMode =
            noiseReducer.getMode() == NoiseReducer::Mode::Adaptive
                ? 0
                : 1;
    }

    //========================================================
    // NOISE REDUCTION
    //========================================================

    const bool cleanEnabled =
        getParameterBool("cleanEnabled");

    const float noiseAmount =
        cleanEnabled
            ? getParameterValue("noiseReduction")
            : 0.0f;

    noiseReducer.process(
        buffer,
        cleanEnabled,
        noiseAmount
    );

    const float learnProgress =
        noiseReducer.getLearnProgress();

    noiseLearnProgress.store(learnProgress);

    if (noiseReducer.getMode() == NoiseReducer::Mode::Learn
        && learnProgress >= 1.0f)
    {
        noiseReducer.stopLearning();

        currentNoiseMode = 1;
    }

    noiseProfileReady.store(
        noiseReducer.hasLearnedProfile()
    );

    //========================================================
    // PITCH CORRECTION
    //========================================================

    pitchCorrector.process(
        buffer,
        getParameterBool("tuneEnabled"),
        static_cast<int>(getParameterValue("tuneKey")),
        static_cast<int>(getParameterValue("tuneScale")),
        getParameterValue("tuneAmount"),
        getParameterValue("retuneSpeed"),
        getParameterValue("humanize"),
        getParameterValue("flexTune"),
        static_cast<int>(getParameterValue("tuneMode"))
    );

    // Preserve the signal after pitch correction.
    const bool canUseDryBuffer =
        channels <= dryBuffer.getNumChannels()
        && samples <= dryBuffer.getNumSamples();

    if (canUseDryBuffer)
    {
        for (int channel = 0; channel < channels; ++channel)
        {
            dryBuffer.copyFrom(
                channel,
                0,
                buffer,
                channel,
                0,
                samples
            );
        }
    }

    //========================================================
    // VOCAL CHARACTER
    //========================================================

    vocalCharacter.process(
        buffer,
        getParameterValue("drive"),
        getParameterValue("saturation"),
        getParameterValue("warmth"),
        getParameterValue("softClip") * 0.01f,
        getParameterValue("characterMix") * 0.01f
    );

    //========================================================
    // DYNAMICS
    //========================================================

    dynamicsProcessor.process(
        buffer,
        getParameterBool("compressorEnabled"),
        getParameterValue("threshold"),
        getParameterValue("ratio"),
        getParameterValue("attack"),
        getParameterValue("release"),
        getParameterValue("makeup"),
        getParameterValue("compressorMix") * 0.01f
    );

    //========================================================
    // SPACE
    //========================================================

    spaceProcessor.process(
        buffer,
        getParameterBool("reverbEnabled"),
        static_cast<int>(getParameterValue("reverbType")),
        getParameterValue("reverbMix") * 0.01f,
        getParameterValue("preDelay"),
        getParameterBool("delayEnabled"),
        static_cast<int>(getParameterValue("delayType")),
        getParameterValue("delayTime"),
        getParameterValue("delayFeedback") * 0.01f,
        getParameterValue("delayMix") * 0.01f
    );

    //========================================================
    // GLOBAL DRY / WET
    //========================================================

    if (canUseDryBuffer && globalMix < 0.9999f)
    {
        for (int channel = 0; channel < channels; ++channel)
        {
            buffer.applyGain(
                channel,
                0,
                samples,
                globalMix
            );

            buffer.addFrom(
                channel,
                0,
                dryBuffer,
                channel,
                0,
                samples,
                1.0f - globalMix
            );
        }
    }

    //========================================================
    // STEREO WIDTH
    //========================================================

    if (channels >= 2)
    {
        const float width = juce::jlimit(
            0.0f,
            2.0f,
            getParameterValue("stereoWidth") * 0.01f
        );

        float* left = buffer.getWritePointer(0);
        float* right = buffer.getWritePointer(1);

        for (int i = 0; i < samples; ++i)
        {
            const float mid = (left[i] + right[i]) * 0.5f;
            const float side = (left[i] - right[i]) * 0.5f;

            left[i] = mid + side * width;
            right[i] = mid - side * width;
        }
    }

    //========================================================
    // OUTPUT GAIN
    //========================================================

    buffer.applyGain(outputGain);

    //========================================================
    // OUTPUT PEAK PROTECTION
    //========================================================

    const float outputCeiling =
        juce::Decibels::decibelsToGain(-1.0f);

    float outputPeak = 0.0f;

    for (int channel = 0; channel < channels; ++channel)
    {
        for (int i = 0; i < samples; ++i)
        {
            float value = buffer.getSample(channel, i);

            if (!std::isfinite(value))
            {
                value = 0.0f;
                buffer.setSample(channel, i, value);
            }

            outputPeak = juce::jmax(
                outputPeak,
                std::abs(value)
            );
        }
    }

    if (outputPeak > outputCeiling)
    {
        const float safetyGain =
            outputCeiling / outputPeak;

        buffer.applyGain(safetyGain);

        outputPeak = outputCeiling;
    }

    outputLevel.store(outputPeak);
}

//============================================================
// PROFILE SYSTEM
//============================================================

void AutoVocalAudioProcessor::applyProfile(
    int artist,
    int mood)
{
    const auto profile =
        AutoVocal::getProfile(artist, mood);

    auto setParameter = [this](
        const juce::String& id,
        float value)
    {
        if (auto* parameter = parameters.getParameter(id))
        {
            parameter->setValueNotifyingHost(
                parameter->convertTo0to1(value)
            );
        }
    };

    // Pitch correction.
    // These profile settings existed but were never applied.
    setParameter("tuneAmount", profile.tuneAmount * 100.0f);
    setParameter("retuneSpeed", profile.retuneSpeed);
    setParameter("humanize", profile.humanize);

    if (auto* parameter = parameters.getParameter("tuneEnabled"))
    {
        parameter->setValueNotifyingHost(
            profile.tuneAmount > 0.001f ? 1.0f : 0.0f
        );
    }

    // Character.
    setParameter("drive", profile.drive);
    setParameter("saturation", profile.saturation);
    setParameter("warmth", profile.warmth);

    setParameter(
        "softClip",
        profile.saturation * 20.0f
    );

    setParameter("characterMix", 100.0f);

    // Dynamics.
    setParameter("threshold", profile.threshold);
    setParameter("ratio", profile.ratio);
    setParameter("attack", profile.attack);
    setParameter("release", profile.release);

    setParameter(
        "compressorMix",
        profile.compression * 100.0f
    );

    setParameter("makeup", 0.0f);

    // Space.
    setParameter(
        "reverbMix",
        profile.reverbMix * 100.0f
    );

    setParameter(
        "delayMix",
        profile.delayMix * 100.0f
    );

    // Stereo.
    setParameter(
        "stereoWidth",
        profile.stereoWidth * 100.0f
    );

    // Output.
    setParameter(
        "output",
        juce::Decibels::gainToDecibels(profile.output)
    );

    if (auto* parameter = parameters.getParameter("compressorEnabled"))
        parameter->setValueNotifyingHost(1.0f);

    if (auto* parameter = parameters.getParameter("reverbEnabled"))
    {
        parameter->setValueNotifyingHost(
            profile.reverbMix > 0.001f ? 1.0f : 0.0f
        );
    }

    if (auto* parameter = parameters.getParameter("delayEnabled"))
    {
        parameter->setValueNotifyingHost(
            profile.delayMix > 0.001f ? 1.0f : 0.0f
        );
    }
}

//============================================================
// METERS
//============================================================

float AutoVocalAudioProcessor::getInputLevel() const
{
    return inputLevel.load();
}

float AutoVocalAudioProcessor::getOutputLevel() const
{
    return outputLevel.load();
}

//============================================================
// EDITOR
//============================================================

bool AutoVocalAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor*
AutoVocalAudioProcessor::createEditor()
{
    return new AutoVocalAudioProcessorEditor(*this);
}

//============================================================
// BASIC PLUGIN INFO
//============================================================

const juce::String AutoVocalAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AutoVocalAudioProcessor::acceptsMidi() const
{
    return false;
}

bool AutoVocalAudioProcessor::producesMidi() const
{
    return false;
}

bool AutoVocalAudioProcessor::isMidiEffect() const
{
    return false;
}

double AutoVocalAudioProcessor::getTailLengthSeconds() const
{
    return 5.0;
}

int AutoVocalAudioProcessor::getNumPrograms()
{
    return 1;
}

int AutoVocalAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AutoVocalAudioProcessor::setCurrentProgram(int)
{
}

const juce::String AutoVocalAudioProcessor::getProgramName(int)
{
    return {};
}

void AutoVocalAudioProcessor::changeProgramName(
    int,
    const juce::String&)
{
}

//============================================================
// SAVE / LOAD STATE
//============================================================

void AutoVocalAudioProcessor::getStateInformation(
    juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();

    std::unique_ptr<juce::XmlElement> xml(
        state.createXml()
    );

    if (xml == nullptr)
    {
        destData.reset();
        return;
    }

    // Save the CLEAN noise profile alongside the parameters.
    juce::MemoryBlock profileData;

    noiseReducer.saveProfile(profileData);

    if (profileData.getSize() > 0)
    {
        auto* profileElement =
            xml->createNewChildElement("NOISE_PROFILE");

        profileElement->addTextElement(
            profileData.toBase64Encoding()
        );
    }

    copyXmlToBinary(*xml, destData);
}

void AutoVocalAudioProcessor::setStateInformation(
    const void* data,
    int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(
        getXmlFromBinary(data, sizeInBytes)
    );

    if (xml == nullptr)
        return;

    if (!xml->hasTagName(parameters.state.getType()))
        return;

    // Clear the previous profile first. This also handles
    // older projects that do not contain a CLEAN profile.
    noiseReducer.clearProfile();

    noiseProfileReady.store(false);
    noiseLearnProgress.store(0.0f);

    noiseLearnStartRequested.store(false);
    noiseLearnStopRequested.store(false);

    // Restore the CLEAN profile if it exists.
    if (auto* profileElement =
        xml->getChildByName("NOISE_PROFILE"))
    {
        const juce::String encodedProfile =
            profileElement->getAllSubText().trim();

        if (encodedProfile.isNotEmpty())
        {
            juce::MemoryBlock profileData;

            const bool decoded =
                profileData.fromBase64Encoding(encodedProfile);

            if (decoded && profileData.getSize() > 0)
            {
                const bool loaded =
                    noiseReducer.loadProfile(
                        profileData.getData(),
                        static_cast<int>(profileData.getSize())
                    );

                noiseProfileReady.store(loaded);
            }
        }
    }

    // Restore ordinary plugin parameters.
    parameters.replaceState(
        juce::ValueTree::fromXml(*xml)
    );

    // Force the audio thread to apply the restored mode
    // on its next processBlock call.
    currentNoiseMode = -1;
}