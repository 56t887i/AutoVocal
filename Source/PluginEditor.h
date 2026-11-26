
#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class AutoVocalAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit AutoVocalAudioProcessorEditor(AutoVocalAudioProcessor&);
    ~AutoVocalAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment =
        juce::AudioProcessorValueTreeState::SliderAttachment;

    using ButtonAttachment =
        juce::AudioProcessorValueTreeState::ButtonAttachment;

    using ComboAttachment =
        juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    AutoVocalAudioProcessor& audioProcessor;

    juce::ComboBox artistBox;
    juce::ComboBox moodBox;

    juce::TextButton applyProfileButton { "APPLY PROFILE" };
    juce::TextButton loadReferenceButton { "LOAD REFERENCE" };
    juce::TextButton analyzeReferenceButton { "ANALYZE REFERENCE" };

    juce::Label referenceLabel;
    juce::Label statusLabel;

    // Noise learning controls
    juce::TextButton noiseLearnButton { "LEARN NOISE" };
    juce::Label noiseLearnStatus;

    double noiseProgress = 0.0;
    juce::ProgressBar noiseProgressBar { noiseProgress };

    bool noiseLearningActive = false;

    juce::TextButton mainTab { "MAIN" };
    juce::TextButton tuneTab { "TUNE" };
    juce::TextButton cleanTab { "CLEAN" };
    juce::TextButton characterTab { "CHARACTER" };
    juce::TextButton dynamicsTab { "DYNAMICS" };
    juce::TextButton spaceTab { "SPACE" };

    juce::OwnedArray<juce::Slider> sliders;
    juce::OwnedArray<juce::Label> sliderLabels;
    juce::OwnedArray<juce::Label> choiceLabels;

    juce::OwnedArray<SliderAttachment> sliderAttachments;
    juce::OwnedArray<ButtonAttachment> buttonAttachments;
    juce::OwnedArray<ComboAttachment> comboAttachments;

    juce::OwnedArray<juce::ToggleButton> toggles;
    juce::OwnedArray<juce::ComboBox> choices;

    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::File referenceFile;

    juce::String currentTab = "MAIN";

    struct Control
    {
        juce::String id;
        juce::String name;
        bool toggle = false;
        bool choice = false;
        juce::StringArray choiceItems;
    };

    juce::Array<Control> currentControls;

    void setupComboBoxes();
    void setupButtons();
    void setupNoiseControls();
    void setupTabs();

    void showTab(const juce::String& tab);

    void addSlider(const juce::String& id,
                   const juce::String& name);

    void addToggle(const juce::String& id,
                   const juce::String& name);

    void addChoice(const juce::String& id,
                   const juce::String& name,
                   const juce::StringArray& items);

    void loadReference();
    void analyzeReference();

    void styleButton(juce::TextButton& button,
                     bool primary = false);

    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(
        AutoVocalAudioProcessorEditor
    )
};
