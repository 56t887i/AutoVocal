#include "NoiseReducer.h"

#include <cmath>

//============================================================
// CONSTRUCTOR
//============================================================

NoiseReducer::NoiseReducer()
{
    for (auto& value : noiseProfile)
        value.store(0.0f, std::memory_order_relaxed);
}

//============================================================
// RING BUFFER INDEX
//============================================================

int NoiseReducer::getRingIndex(int64_t position)
{
    return static_cast<int>(
        static_cast<uint64_t>(position)
        & static_cast<uint64_t>(ringMask)
    );
}

//============================================================
// PREPARE
//============================================================

void NoiseReducer::prepare(
    double sampleRate,
    int samplesPerBlock,
    int channels)
{
    juce::ignoreUnused(samplesPerBlock);

    currentSampleRate = juce::jmax(8000.0, sampleRate);
    channelCount = juce::jlimit(1, maxChannels, channels);

    fft = std::make_unique<juce::dsp::FFT>(fftOrder);

    for (int i = 0; i < fftSize; ++i)
    {
        const float position =
            static_cast<float>(i)
            / static_cast<float>(fftSize);

        const float hann =
            0.5f - 0.5f * std::cos(
                juce::MathConstants<float>::twoPi * position
            );

        window[static_cast<size_t>(i)] =
            std::sqrt(juce::jmax(0.0f, hann));
    }

    adaptiveFrameThreshold =
        juce::Decibels::decibelsToGain(-36.0f);

    const double attackTimeSeconds = 0.025;
    const double releaseTimeSeconds = 0.120;

    gainAttackCoefficient =
        static_cast<float>(
            1.0 - std::exp(
                -static_cast<double>(hopSize)
                / (currentSampleRate * attackTimeSeconds)
            )
        );

    gainReleaseCoefficient =
        static_cast<float>(
            1.0 - std::exp(
                -static_cast<double>(hopSize)
                / (currentSampleRate * releaseTimeSeconds)
            )
        );

    reset();
}

//============================================================
// RESET
//============================================================

void NoiseReducer::reset()
{
    sampleCounter = 0;

    for (auto& channel : inputRing)
        channel.fill(0.0f);

    for (auto& channel : outputRing)
        channel.fill(0.0f);

    for (auto& channel : fftBuffers)
        channel.fill(0.0f);

    adaptiveNoise.fill(0.000001f);

    binGains.fill(1.0f);
    smoothedGains.fill(1.0f);

    currentReduction = 50.0f;

    // Keep the learned profile when the audio engine resets.
}

//============================================================
// MODE CONTROL
//============================================================

void NoiseReducer::setMode(Mode newMode)
{
    if (newMode == Mode::Profile
        && !profileAvailable.load(std::memory_order_acquire))
    {
        newMode = Mode::Adaptive;
    }

    mode = newMode;
}

NoiseReducer::Mode NoiseReducer::getMode() const
{
    return mode;
}

//============================================================
// LEARNING
//============================================================

void NoiseReducer::startLearning()
{
    // Invalidate any state snapshot that may be in progress.
    profileRevision.fetch_add(1, std::memory_order_acq_rel);

    profileAvailable.store(false, std::memory_order_release);

    for (auto& value : noiseProfile)
        value.store(0.0f, std::memory_order_relaxed);

    learnedFrames = 0;
    mode = Mode::Learn;

    smoothedGains.fill(1.0f);
    binGains.fill(1.0f);
}

void NoiseReducer::stopLearning()
{
    if (learnedFrames > 0)
    {
        profileRevision.fetch_add(1, std::memory_order_acq_rel);

        profileAvailable.store(true, std::memory_order_release);
        mode = Mode::Profile;
    }
    else
    {
        mode = Mode::Adaptive;
    }
}

bool NoiseReducer::hasLearnedProfile() const
{
    return profileAvailable.load(std::memory_order_acquire);
}

float NoiseReducer::getLearnProgress() const
{
    return juce::jlimit(
        0.0f,
        1.0f,
        static_cast<float>(learnedFrames)
            / static_cast<float>(learningFramesTarget)
    );
}

int NoiseReducer::getLatencySamples() const
{
    return latencySamples;
}

//============================================================
// CLEAR PROFILE
//============================================================

void NoiseReducer::clearProfile()
{
    profileRevision.fetch_add(1, std::memory_order_acq_rel);

    profileAvailable.store(false, std::memory_order_release);

    for (auto& value : noiseProfile)
        value.store(0.0f, std::memory_order_relaxed);

    profileRevision.fetch_add(1, std::memory_order_acq_rel);
}

//============================================================
// PROFILE SERIALIZATION
//============================================================

void NoiseReducer::saveProfile(
    juce::MemoryBlock& destination) const
{
    destination.reset();

    constexpr int maximumAttempts = 4;

    std::array<float, binCount> snapshot {};

    for (int attempt = 0; attempt < maximumAttempts; ++attempt)
    {
        const uint64_t revisionBefore =
            profileRevision.load(std::memory_order_acquire);

        if (!profileAvailable.load(std::memory_order_acquire))
            return;

        for (int i = 0; i < binCount; ++i)
        {
            snapshot[static_cast<size_t>(i)] =
                noiseProfile[static_cast<size_t>(i)]
                    .load(std::memory_order_relaxed);
        }

        const bool availableAfter =
            profileAvailable.load(std::memory_order_acquire);

        const uint64_t revisionAfter =
            profileRevision.load(std::memory_order_acquire);

        if (!availableAfter || revisionBefore != revisionAfter)
            continue;

        bool valid = true;

        for (const float value : snapshot)
        {
            if (!std::isfinite(value)
                || value < 0.0f
                || value > 1000000.0f)
            {
                valid = false;
                break;
            }
        }

        if (!valid)
            return;

        juce::MemoryOutputStream stream(destination, false);

        stream.writeInt(profileMagic);
        stream.writeInt(profileVersion);
        stream.writeInt(binCount);

        for (const float value : snapshot)
            stream.writeFloat(value);

        return;
    }

    destination.reset();
}

bool NoiseReducer::loadProfile(
    const void* data,
    int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return false;

    constexpr int expectedSize =
        static_cast<int>(
            sizeof(int) * 3
            + sizeof(float) * binCount
        );

    if (sizeInBytes != expectedSize)
        return false;

    juce::MemoryInputStream stream(
        data,
        static_cast<size_t>(sizeInBytes),
        false
    );

    const int magic = stream.readInt();
    const int version = stream.readInt();
    const int storedBinCount = stream.readInt();

    if (magic != profileMagic
        || version != profileVersion
        || storedBinCount != binCount)
    {
        return false;
    }

    std::array<float, binCount> loadedProfile {};

    for (int i = 0; i < binCount; ++i)
    {
        const float value = stream.readFloat();

        if (!std::isfinite(value)
            || value < 0.0f
            || value > 1000000.0f)
        {
            return false;
        }

        loadedProfile[static_cast<size_t>(i)] = value;
    }

    profileRevision.fetch_add(1, std::memory_order_acq_rel);

    profileAvailable.store(false, std::memory_order_release);

    for (int i = 0; i < binCount; ++i)
    {
        noiseProfile[static_cast<size_t>(i)].store(
            loadedProfile[static_cast<size_t>(i)],
            std::memory_order_relaxed
        );
    }

    profileRevision.fetch_add(1, std::memory_order_acq_rel);

    profileAvailable.store(true, std::memory_order_release);

    return true;
}

//============================================================
// NOISE MAGNITUDE
//============================================================

float NoiseReducer::getNoiseMagnitude(int bin) const
{
    const auto index = static_cast<size_t>(bin);

    if (mode == Mode::Profile
        && profileAvailable.load(std::memory_order_acquire))
    {
        return noiseProfile[index].load(std::memory_order_relaxed);
    }

    return adaptiveNoise[index];
}

//============================================================
// NOISE ESTIMATION
//============================================================

void NoiseReducer::updateNoiseEstimate(
    const std::array<float, binCount>& magnitudes,
    float frameRms)
{
    if (mode == Mode::Learn)
    {
        if (learnedFrames < learningFramesTarget)
        {
            ++learnedFrames;

            const float inverseCount =
                1.0f / static_cast<float>(learnedFrames);

            for (int bin = 0; bin < binCount; ++bin)
            {
                const auto index = static_cast<size_t>(bin);

                const float previous =
                    noiseProfile[index].load(std::memory_order_relaxed);

                const float updated =
                    previous
                    + (magnitudes[index] - previous) * inverseCount;

                noiseProfile[index].store(
                    updated,
                    std::memory_order_relaxed
                );
            }
        }

        if (learnedFrames >= learningFramesTarget)
        {
            profileRevision.fetch_add(1, std::memory_order_acq_rel);

            profileAvailable.store(true, std::memory_order_release);

            mode = Mode::Profile;
        }

        return;
    }

    if (mode != Mode::Adaptive)
        return;

    if (frameRms < adaptiveFrameThreshold)
    {
        constexpr float adaptationSpeed = 0.08f;

        for (int bin = 0; bin < binCount; ++bin)
        {
            const auto index = static_cast<size_t>(bin);

            const float target = magnitudes[index];

            adaptiveNoise[index] +=
                (target - adaptiveNoise[index])
                * adaptationSpeed;
        }
    }
}

//============================================================
// FFT FRAME PROCESSING
//============================================================

void NoiseReducer::processFrame(int64_t frameStart)
{
    if (fft == nullptr)
        return;

    std::array<float, binCount> magnitudes {};

    float totalEnergy = 0.0f;

    for (int channel = 0; channel < channelCount; ++channel)
    {
        auto& fftData =
            fftBuffers[static_cast<size_t>(channel)];

        const auto& input =
            inputRing[static_cast<size_t>(channel)];

        for (int i = 0; i < fftSize; ++i)
        {
            const int64_t absolutePosition = frameStart + i;

            const int ringIndex = getRingIndex(absolutePosition);

            const float inputSample =
                input[static_cast<size_t>(ringIndex)];

            totalEnergy += inputSample * inputSample;

            fftData[static_cast<size_t>(i)] =
                inputSample * window[static_cast<size_t>(i)];
        }

        fft->performRealOnlyForwardTransform(
            fftData.data(),
            false
        );
    }

    const float frameRms = std::sqrt(
        totalEnergy /
        static_cast<float>(fftSize * channelCount)
    );

    // Linked stereo magnitudes.

    for (int bin = 0; bin < binCount; ++bin)
    {
        float magnitudeSum = 0.0f;

        for (int channel = 0; channel < channelCount; ++channel)
        {
            const auto& fftData =
                fftBuffers[static_cast<size_t>(channel)];

            const int index = bin * 2;

            const float real =
                fftData[static_cast<size_t>(index)];

            const float imaginary =
                fftData[static_cast<size_t>(index + 1)];

            magnitudeSum += std::sqrt(
                real * real + imaginary * imaginary
            );
        }

        magnitudes[static_cast<size_t>(bin)] =
            magnitudeSum / static_cast<float>(channelCount);
    }

    updateNoiseEstimate(magnitudes, frameRms);

    // Spectral gain targets.

    const float amount = currentReduction * 0.01f;
    const float subtractionStrength = amount * 1.65f;
    const float minimumGain = 1.0f - amount * 0.88f;

    if (mode == Mode::Learn)
        smoothedGains.fill(1.0f);

    for (int bin = 0; bin < binCount; ++bin)
    {
        float targetGain = 1.0f;

        if (mode != Mode::Learn && amount > 0.001f)
        {
            const float magnitude =
                magnitudes[static_cast<size_t>(bin)];

            const float noise = getNoiseMagnitude(bin);

            const float safeMagnitude =
                juce::jmax(magnitude, 0.000001f);

            const float estimatedNoise =
                noise * subtractionStrength;

            targetGain = 1.0f - estimatedNoise / safeMagnitude;

            targetGain = juce::jlimit(
                minimumGain,
                1.0f,
                targetGain
            );
        }

        binGains[static_cast<size_t>(bin)] = targetGain;
    }

    // Frequency and temporal smoothing.

    for (int bin = 0; bin < binCount; ++bin)
    {
        const int previous = juce::jmax(0, bin - 1);
        const int next = juce::jmin(binCount - 1, bin + 1);

        const float center = binGains[static_cast<size_t>(bin)];
        const float left = binGains[static_cast<size_t>(previous)];
        const float right = binGains[static_cast<size_t>(next)];

        const float frequencySmoothed =
            center * 0.60f + left * 0.20f + right * 0.20f;

        const auto index = static_cast<size_t>(bin);

        const float previousGain = smoothedGains[index];

        const float coefficient =
            frequencySmoothed < previousGain
                ? gainAttackCoefficient
                : gainReleaseCoefficient;

        smoothedGains[index] =
            previousGain
            + (frequencySmoothed - previousGain) * coefficient;
    }

    // Apply gains and rebuild spectrum.

    for (int channel = 0; channel < channelCount; ++channel)
    {
        auto& fftData = fftBuffers[static_cast<size_t>(channel)];

        for (int bin = 0; bin < binCount; ++bin)
        {
            const float gain = smoothedGains[static_cast<size_t>(bin)];

            const int index = bin * 2;

            fftData[static_cast<size_t>(index)] *= gain;
            fftData[static_cast<size_t>(index + 1)] *= gain;
        }

        fftData[1] = 0.0f;
        fftData[static_cast<size_t>(fftSize + 1)] = 0.0f;

        for (int bin = 1; bin < fftSize / 2; ++bin)
        {
            const int source = bin * 2;
            const int destination = (fftSize - bin) * 2;

            fftData[static_cast<size_t>(destination)] =
                fftData[static_cast<size_t>(source)];

            fftData[static_cast<size_t>(destination + 1)] =
                -fftData[static_cast<size_t>(source + 1)];
        }

        fft->performRealOnlyInverseTransform(fftData.data());

        // Windowed overlap-add.

        auto& output = outputRing[static_cast<size_t>(channel)];

        for (int i = 0; i < fftSize; ++i)
        {
            const int64_t absolutePosition = frameStart + i;
            const int ringIndex = getRingIndex(absolutePosition);

            const float reconstructed =
                fftData[static_cast<size_t>(i)];

            const float synthesisWindow =
                window[static_cast<size_t>(i)];

            const float normalizedOutput =
                reconstructed * synthesisWindow * 0.5f;

            output[static_cast<size_t>(ringIndex)] += normalizedOutput;
        }
    }
}

//============================================================
// MAIN PROCESS
//============================================================

void NoiseReducer::process(
    juce::AudioBuffer<float>& buffer,
    bool enabled,
    float reduction)
{
    const int samples = buffer.getNumSamples();

    if (samples <= 0 || fft == nullptr)
        return;

    currentReduction = juce::jlimit(
        0.0f,
        100.0f,
        reduction
    );

    for (int sample = 0; sample < samples; ++sample)
    {
        const int64_t currentPosition = sampleCounter;

        const int inputRingIndex = getRingIndex(currentPosition);

        // Write input.

        for (int channel = 0; channel < channelCount; ++channel)
        {
            float input = 0.0f;

            if (channel < buffer.getNumChannels())
                input = buffer.getSample(channel, sample);

            inputRing[static_cast<size_t>(channel)]
                     [static_cast<size_t>(inputRingIndex)] = input;
        }

        // Process FFT frames.

        const int64_t frameStart =
            currentPosition - (fftSize - 1);

        if ((frameStart % hopSize) == 0)
            processFrame(frameStart);

        // Read delayed output.

        const int64_t outputPosition =
            currentPosition - latencySamples;

        const int outputRingIndex = getRingIndex(outputPosition);

        for (int channel = 0; channel < channelCount; ++channel)
        {
            float output = 0.0f;

            if (currentPosition >= latencySamples)
            {
                if (enabled)
                {
                    output =
                        outputRing[static_cast<size_t>(channel)]
                                  [static_cast<size_t>(outputRingIndex)];
                }
                else
                {
                    output =
                        inputRing[static_cast<size_t>(channel)]
                                 [static_cast<size_t>(outputRingIndex)];
                }
            }

            outputRing[static_cast<size_t>(channel)]
                      [static_cast<size_t>(outputRingIndex)] = 0.0f;

            if (channel < buffer.getNumChannels())
                buffer.setSample(channel, sample, output);
        }

        ++sampleCounter;
    }
}