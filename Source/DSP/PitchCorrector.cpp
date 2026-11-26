#include "PitchCorrector.h"

#include <cmath>
#include <algorithm>
#include <array>

namespace
{
    constexpr float pi = 3.14159265358979323846f;

    // 15-tap Blackman-windowed low-pass FIR, cutoff ~= 0.105 cycles/sample.
    // The cutoff is below the new Nyquist limit after 4:1 decimation.
    constexpr std::array<float, 15> analysisFir
    {{
         0.0f, -0.0008394524f, -0.0010081886f,
         0.0101542522f, 0.0500432395f, 0.1231756670f,
         0.2009594300f, 0.2350301050f, 0.2009594300f,
         0.1231756670f, 0.0500432395f, 0.0101542522f,
        -0.0010081886f, -0.0008394524f, 0.0f
    }};

    float midiToFrequency(float midi)
    {
        return 440.0f * std::pow(2.0f, (midi - 69.0f) / 12.0f);
    }

    float frequencyToMidi(float frequency)
    {
        return 69.0f + 12.0f * std::log2(frequency / 440.0f);
    }
}

void PitchCorrector::prepare(
    double sampleRate,
    int samplesPerBlock,
    int channels)
{
    juce::ignoreUnused(samplesPerBlock);

    currentSampleRate = juce::jmax(8000.0, sampleRate);
    channelCount = juce::jlimit(1, 2, channels);

    latencySamples = static_cast<int>(currentSampleRate * 0.030);
    minimumDelay = static_cast<int>(currentSampleRate * 0.012);
    maximumDelay = static_cast<int>(currentSampleRate * 0.050);

    const int bufferSize =
        static_cast<int>(currentSampleRate * 0.080) + 8;

    delayBuffers.clear();
    delayBuffers.resize(static_cast<size_t>(channelCount));

    for (auto& channel : delayBuffers)
        channel.assign(static_cast<size_t>(bufferSize), 0.0f);

    analysisBuffer.assign(analysisSize, 0.0f);

    reset();
}

void PitchCorrector::reset()
{
    for (auto& channel : delayBuffers)
        std::fill(channel.begin(), channel.end(), 0.0f);

    std::fill(analysisBuffer.begin(), analysisBuffer.end(), 0.0f);
    analysisFirHistory.fill(0.0f);

    writePosition = 0;
    analysisFirWritePosition = 0;
    decimationCounter = 0;
    analysisWritePosition = 0;
    samplesSinceAnalysis = 0;

    detectedFrequency = 0.0f;
    currentSemitoneShift = 0.0f;
    targetSemitoneShift = 0.0f;

    pitchPhase = 0.0f;
    analysisReady = false;
}

int PitchCorrector::getLatencySamples() const
{
    return latencySamples;
}

float PitchCorrector::detectPitch()
{
    if (analysisBuffer.size() < analysisSize)
        return 0.0f;

    const float analysisRate =
        static_cast<float>(currentSampleRate / decimationFactor);

    const int minimumLag = juce::jmax(
        2,
        static_cast<int>(analysisRate / 1000.0f)
    );

    const int maximumLag = juce::jmin(
        analysisWindow / 2,
        static_cast<int>(analysisRate / 65.0f)
    );

    std::array<float, analysisWindow / 2 + 1> correlations {};
    float bestCorrelation = 0.0f;
    int bestLag = 0;

    for (int lag = minimumLag; lag <= maximumLag; ++lag)
    {
        double crossCorrelation = 0.0;
        double energyA = 0.0;
        double energyB = 0.0;

        for (int i = 0; i < analysisWindow; ++i)
        {
            int positionA = analysisWritePosition - 1 - i;
            while (positionA < 0)
                positionA += analysisSize;

            int positionB = positionA - lag;
            while (positionB < 0)
                positionB += analysisSize;

            const float a = analysisBuffer[static_cast<size_t>(positionA)];
            const float b = analysisBuffer[static_cast<size_t>(positionB)];

            crossCorrelation += static_cast<double>(a) * b;
            energyA += static_cast<double>(a) * a;
            energyB += static_cast<double>(b) * b;
        }

        const double denominator =
            std::sqrt(energyA * energyB) + 1.0e-12;

        const float correlation =
            static_cast<float>(crossCorrelation / denominator);

        correlations[static_cast<size_t>(lag)] = correlation;

        if (correlation > bestCorrelation)
        {
            bestCorrelation = correlation;
            bestLag = lag;
        }
    }

    if (bestLag <= 0 || bestCorrelation < 0.60f)
        return 0.0f;

    // Parabolic peak interpolation gives sub-sample period estimates,
    // reducing the stepped tuning movement caused by integer lags.
    float refinedLag = static_cast<float>(bestLag);

    if (bestLag > minimumLag && bestLag < maximumLag)
    {
        const float left = correlations[static_cast<size_t>(bestLag - 1)];
        const float centre = correlations[static_cast<size_t>(bestLag)];
        const float right = correlations[static_cast<size_t>(bestLag + 1)];
        const float curvature = left - 2.0f * centre + right;

        if (std::abs(curvature) > 1.0e-8f)
        {
            const float offset =
                0.5f * (left - right) / curvature;

            refinedLag += juce::jlimit(-0.5f, 0.5f, offset);
        }
    }

    const float frequency = analysisRate / refinedLag;

    if (frequency < 65.0f || frequency > 1000.0f)
        return 0.0f;

    return frequency;
}

int PitchCorrector::findTargetNote(
    float midiNote,
    int key,
    int scale) const
{
    static constexpr int majorScale[7] =
    {
        0, 2, 4, 5, 7, 9, 11
    };

    static constexpr int minorScale[7] =
    {
        0, 2, 3, 5, 7, 8, 10
    };

    key = juce::jlimit(0, 11, key);
    scale = juce::jlimit(0, 2, scale);

    const int nearestMidi = static_cast<int>(std::round(midiNote));

    if (scale == 0)
        return nearestMidi;

    int bestNote = nearestMidi;
    float smallestDistance = 1000.0f;

    for (int note = nearestMidi - 12; note <= nearestMidi + 12; ++note)
    {
        int pitchClass = note % 12;

        if (pitchClass < 0)
            pitchClass += 12;

        const int relativePitch = (pitchClass - key + 12) % 12;
        bool allowed = false;

        for (int i = 0; i < 7; ++i)
        {
            const int scaleNote =
                scale == 1 ? majorScale[i] : minorScale[i];

            if (relativePitch == scaleNote)
            {
                allowed = true;
                break;
            }
        }

        if (!allowed)
            continue;

        const float distance =
            std::abs(midiNote - static_cast<float>(note));

        if (distance < smallestDistance)
        {
            smallestDistance = distance;
            bestNote = note;
        }
    }

    return bestNote;
}

float PitchCorrector::readDelayLine(
    const std::vector<float>& data,
    float delay) const
{
    if (data.empty())
        return 0.0f;

    const int size = static_cast<int>(data.size());

    delay = juce::jlimit(
        2.0f,
        static_cast<float>(size - 3),
        delay
    );

    float readPosition = static_cast<float>(writePosition) - delay;

    while (readPosition < 0.0f)
        readPosition += static_cast<float>(size);

    while (readPosition >= static_cast<float>(size))
        readPosition -= static_cast<float>(size);

    const int indexA = static_cast<int>(readPosition);
    const int indexB = (indexA + 1) % size;
    const float fraction = readPosition - static_cast<float>(indexA);

    return data[static_cast<size_t>(indexA)]
        + (data[static_cast<size_t>(indexB)]
           - data[static_cast<size_t>(indexA)]) * fraction;
}

void PitchCorrector::process(
    juce::AudioBuffer<float>& buffer,
    bool enabled,
    int key,
    int scale,
    float amount,
    float retuneSpeed,
    float humanize,
    float flexTune,
    int mode)
{
    const int channels = juce::jmin(buffer.getNumChannels(), channelCount);
    const int samples = buffer.getNumSamples();

    if (channels <= 0 || samples <= 0 || delayBuffers.empty())
        return;

    amount = juce::jlimit(0.0f, 100.0f, amount);
    retuneSpeed = juce::jlimit(1.0f, 100.0f, retuneSpeed);
    humanize = juce::jlimit(0.0f, 100.0f, humanize);
    flexTune = juce::jlimit(0.0f, 100.0f, flexTune);

    const bool correctionActive = enabled && amount > 0.01f;
    const float amountNormalised = amount * 0.01f;
    const float speedNormalised = (retuneSpeed - 1.0f) / 99.0f;

    float timeConstant =
        0.45f * (1.0f - speedNormalised) + 0.008f;

    if (mode == 1)
        timeConstant = 0.0025f + (1.0f - speedNormalised) * 0.012f;

    timeConstant *= 1.0f + humanize * 0.01f * 0.65f;

    const float smoothingCoefficient =
        1.0f - std::exp(
            -1.0f / static_cast<float>(currentSampleRate * timeConstant)
        );

    const int bufferSize = static_cast<int>(delayBuffers[0].size());

    for (int sample = 0; sample < samples; ++sample)
    {
        float monoSample = 0.0f;

        for (int channel = 0; channel < channels; ++channel)
            monoSample += buffer.getSample(channel, sample);

        monoSample /= static_cast<float>(channels);

        analysisFirHistory[static_cast<size_t>(analysisFirWritePosition)] =
            monoSample;

        analysisFirWritePosition =
            (analysisFirWritePosition + 1) % analysisFirSize;

        ++decimationCounter;

        if (decimationCounter >= decimationFactor)
        {
            decimationCounter = 0;

            float filteredSample = 0.0f;

            for (int tap = 0; tap < analysisFirSize; ++tap)
            {
                int historyPosition =
                    analysisFirWritePosition - 1 - tap;

                while (historyPosition < 0)
                    historyPosition += analysisFirSize;

                filteredSample +=
                    analysisFir[static_cast<size_t>(tap)]
                    * analysisFirHistory[static_cast<size_t>(historyPosition)];
            }

            analysisBuffer[static_cast<size_t>(analysisWritePosition)] =
                filteredSample;

            analysisWritePosition =
                (analysisWritePosition + 1) % analysisSize;
        }

        ++samplesSinceAnalysis;

        if (samplesSinceAnalysis >= analysisHop)
        {
            samplesSinceAnalysis = 0;
            analysisReady = true;

            const float frequency = detectPitch();

            if (frequency > 0.0f)
            {
                detectedFrequency = frequency;

                if (correctionActive)
                {
                    const float midiNote = frequencyToMidi(frequency);
                    const int targetNote = findTargetNote(midiNote, key, scale);
                    float difference =
                        static_cast<float>(targetNote) - midiNote;

                    const float flexThreshold = flexTune * 0.005f;

                    if (std::abs(difference) < flexThreshold)
                        difference = 0.0f;

                    targetSemitoneShift = difference * amountNormalised;
                }
                else
                {
                    targetSemitoneShift = 0.0f;
                }
            }
            else
            {
                targetSemitoneShift = 0.0f;
            }
        }

        if (!analysisReady || !correctionActive)
            targetSemitoneShift = 0.0f;

        currentSemitoneShift +=
            (targetSemitoneShift - currentSemitoneShift)
            * smoothingCoefficient;

        currentSemitoneShift = juce::jlimit(
            -12.0f, 12.0f, currentSemitoneShift
        );

        const float pitchRatio =
            correctionActive
                ? std::pow(2.0f, currentSemitoneShift / 12.0f)
                : 1.0f;

        for (int channel = 0; channel < channels; ++channel)
        {
            delayBuffers[static_cast<size_t>(channel)]
                [static_cast<size_t>(writePosition)] =
                    buffer.getSample(channel, sample);
        }

        const float neutralDelay = static_cast<float>(latencySamples);

        // Keep the neutral path stable around zero correction. This avoids
        // the old hard 0.1% ratio switch, which could click as tuning began.
        if (correctionActive && std::abs(pitchRatio - 1.0f) > 0.00001f)
        {
            const float delayRange =
                static_cast<float>(maximumDelay - minimumDelay);

            const float phaseOffset = std::abs(pitchRatio - 1.0f);
            pitchPhase += phaseOffset / delayRange;

            while (pitchPhase >= 1.0f)
                pitchPhase -= 1.0f;

            const float phaseA = pitchPhase;
            float phaseB = phaseA + 0.5f;

            if (phaseB >= 1.0f)
                phaseB -= 1.0f;

            const float delayA =
                pitchRatio > 1.0f
                    ? static_cast<float>(maximumDelay) - phaseA * delayRange
                    : static_cast<float>(minimumDelay) + phaseA * delayRange;

            const float delayB =
                pitchRatio > 1.0f
                    ? static_cast<float>(maximumDelay) - phaseB * delayRange
                    : static_cast<float>(minimumDelay) + phaseB * delayRange;

            const float weightA =
                0.5f + 0.5f * std::cos(2.0f * pi * phaseA);
            const float weightB = 1.0f - weightA;

            for (int channel = 0; channel < channels; ++channel)
            {
                const auto& delayLine =
                    delayBuffers[static_cast<size_t>(channel)];

                const float shifted =
                    readDelayLine(delayLine, delayA) * weightA
                    + readDelayLine(delayLine, delayB) * weightB;

                buffer.setSample(channel, sample, shifted);
            }
        }
        else
        {
            for (int channel = 0; channel < channels; ++channel)
            {
                buffer.setSample(
                    channel,
                    sample,
                    readDelayLine(
                        delayBuffers[static_cast<size_t>(channel)],
                        neutralDelay
                    )
                );
            }
        }

        writePosition = (writePosition + 1) % bufferSize;
    }
}
