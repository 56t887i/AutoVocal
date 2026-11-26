
#include "PluginEditor.h"

AutoVocalAudioProcessorEditor::AutoVocalAudioProcessorEditor(
    AutoVocalAudioProcessor& processor)
    : AudioProcessorEditor(&processor),
      audioProcessor(processor)
{
    formatManager.registerBasicFormats();

    setSize(900, 700);
    setResizable(true, true);
    setResizeLimits(760, 600, 1300, 950);

    setupComboBoxes();
    setupButtons();
    setupNoiseControls();
    setupTabs();

    showTab("MAIN");

    startTimerHz(10);
}

AutoVocalAudioProcessorEditor::~AutoVocalAudioProcessorEditor()
{
    stopTimer();
    fileChooser.reset();
}

void AutoVocalAudioProcessorEditor::setupComboBoxes()
{
    artistBox.setTextWhenNothingSelected("Select Artist");

    for (int i = 0; i < AutoVocal::artistCount; ++i)
        artistBox.addItem(
            juce::String(AutoVocal::artistNames[i]),
            i + 1
        );

    moodBox.setTextWhenNothingSelected("Select Mood");

    for (int i = 0; i < AutoVocal::moodCount; ++i)
        moodBox.addItem(
            juce::String(AutoVocal::moodNames[i]),
            i + 1
        );

    for (auto* box : { &artistBox, &moodBox })
    {
        box->setColour(
            juce::ComboBox::backgroundColourId,
            juce::Colour(0xff20202a)
        );

        box->setColour(
            juce::ComboBox::textColourId,
            juce::Colours::white
        );

        box->setColour(
            juce::ComboBox::outlineColourId,
            juce::Colour(0xff454052)
        );

        box->setColour(
            juce::ComboBox::arrowColourId,
            juce::Colour(0xffb99aff)
        );

        addAndMakeVisible(box);
    }
}

void AutoVocalAudioProcessorEditor::styleButton(
    juce::TextButton& button,
    bool primary)
{
    button.setColour(
        juce::TextButton::buttonColourId,
        primary
            ? juce::Colour(0xff7652c8)
            : juce::Colour(0xff252431)
    );

    button.setColour(
        juce::TextButton::buttonOnColourId,
        juce::Colour(0xff906be5)
    );

    button.setColour(
        juce::TextButton::textColourOffId,
        juce::Colours::white
    );

    button.setColour(
        juce::TextButton::textColourOnId,
        juce::Colours::white
    );

    button.setClickingTogglesState(false);

    addAndMakeVisible(button);
}

void AutoVocalAudioProcessorEditor::setupButtons()
{
    styleButton(applyProfileButton, true);
    styleButton(loadReferenceButton);
    styleButton(analyzeReferenceButton, true);

    referenceLabel.setText(
        "No reference loaded",
        juce::dontSendNotification
    );

    referenceLabel.setColour(
        juce::Label::textColourId,
        juce::Colour(0xffa8a5b5)
    );

    referenceLabel.setFont(juce::Font(13.0f));

    addAndMakeVisible(referenceLabel);

    statusLabel.setText(
        "READY",
        juce::dontSendNotification
    );

    statusLabel.setColour(
        juce::Label::textColourId,
        juce::Colour(0xffa98be8)
    );

    statusLabel.setFont(juce::Font(12.0f));

    addAndMakeVisible(statusLabel);

    applyProfileButton.onClick = [this]
    {
        if (artistBox.getSelectedId() == 0 ||
            moodBox.getSelectedId() == 0)
        {
            statusLabel.setText(
                "Select artist and mood first",
                juce::dontSendNotification
            );

            return;
        }

        audioProcessor.applyProfile(
            artistBox.getSelectedId() - 1,
            moodBox.getSelectedId() - 1
        );

        statusLabel.setText(
            "PROFILE APPLIED",
            juce::dontSendNotification
        );
    };

    loadReferenceButton.onClick = [this]
    {
        loadReference();
    };

    analyzeReferenceButton.onClick = [this]
    {
        analyzeReference();
    };
}

void AutoVocalAudioProcessorEditor::setupNoiseControls()
{
    styleButton(noiseLearnButton, true);

    noiseLearnStatus.setText(
        "NOISE PROFILE NOT LEARNED",
        juce::dontSendNotification
    );

    noiseLearnStatus.setColour(
        juce::Label::textColourId,
        juce::Colour(0xffa8a5b5)
    );

    noiseLearnStatus.setFont(juce::Font(12.0f));

    addAndMakeVisible(noiseLearnStatus);

    noiseProgressBar.setColour(
        juce::ProgressBar::backgroundColourId,
        juce::Colour(0xff252431)
    );

    noiseProgressBar.setColour(
        juce::ProgressBar::foregroundColourId,
        juce::Colour(0xff9874e5)
    );

    noiseProgressBar.setColour(
        juce::ProgressBar::textColourId,
        juce::Colours::white
    );

    noiseProgressBar.setPercentageDisplay(true);

    addAndMakeVisible(noiseProgressBar);

    noiseLearnButton.onClick = [this]
    {
        if (!noiseLearningActive)
        {
            audioProcessor.startNoiseLearning();

            noiseLearningActive = true;
            noiseProgress = 0.0;

            noiseLearnButton.setButtonText("STOP LEARNING");

            noiseLearnStatus.setText(
                "LEARNING NOISE... KEEP INPUT SILENT",
                juce::dontSendNotification
            );
        }
        else
        {
            audioProcessor.stopNoiseLearning();

            noiseLearningActive = false;

            noiseLearnButton.setButtonText("LEARN NOISE");

            noiseLearnStatus.setText(
                "LEARNING STOPPED",
                juce::dontSendNotification
            );
        }
    };

    noiseLearnButton.setVisible(false);
    noiseLearnStatus.setVisible(false);
    noiseProgressBar.setVisible(false);
}

void AutoVocalAudioProcessorEditor::setupTabs()
{
    auto configureTab = [this](
        juce::TextButton& button,
        const juce::String& tab)
    {
        button.setClickingTogglesState(true);
        button.setRadioGroupId(1001);

        button.setColour(
            juce::TextButton::buttonColourId,
            juce::Colour(0xff20202a)
        );

        button.setColour(
            juce::TextButton::buttonOnColourId,
            juce::Colour(0xff7652c8)
        );

        button.setColour(
            juce::TextButton::textColourOffId,
            juce::Colour(0xffbdb9ca)
        );

        button.setColour(
            juce::TextButton::textColourOnId,
            juce::Colours::white
        );

        button.onClick = [this, tab]
        {
            showTab(tab);
        };

        addAndMakeVisible(button);
    };

    configureTab(mainTab, "MAIN");
    configureTab(tuneTab, "TUNE");
    configureTab(cleanTab, "CLEAN");
    configureTab(characterTab, "CHARACTER");
    configureTab(dynamicsTab, "DYNAMICS");
    configureTab(spaceTab, "SPACE");
}

void AutoVocalAudioProcessorEditor::addSlider(
    const juce::String& id,
    const juce::String& name)
{
    auto* slider = sliders.add(new juce::Slider());

    slider->setSliderStyle(
        juce::Slider::RotaryHorizontalVerticalDrag
    );

    slider->setTextBoxStyle(
        juce::Slider::TextBoxBelow,
        false,
        85,
        22
    );

    slider->setColour(
        juce::Slider::rotarySliderFillColourId,
        juce::Colour(0xffa783f0)
    );

    slider->setColour(
        juce::Slider::rotarySliderOutlineColourId,
        juce::Colour(0xff383443)
    );

    slider->setColour(
        juce::Slider::thumbColourId,
        juce::Colour(0xffe2d6ff)
    );

    slider->setColour(
        juce::Slider::textBoxTextColourId,
        juce::Colours::white
    );

    slider->setColour(
        juce::Slider::textBoxBackgroundColourId,
        juce::Colour(0xff20202a)
    );

    slider->setColour(
        juce::Slider::textBoxOutlineColourId,
        juce::Colour(0xff454052)
    );

    auto* label = sliderLabels.add(new juce::Label());

    label->setText(
        name,
        juce::dontSendNotification
    );

    label->setJustificationType(
        juce::Justification::centred
    );

    label->setColour(
        juce::Label::textColourId,
        juce::Colour(0xffd9d5e3)
    );

    label->setFont(juce::Font(13.0f));

    addAndMakeVisible(slider);
    addAndMakeVisible(label);

    sliderAttachments.add(
        new SliderAttachment(
            audioProcessor.parameters,
            id,
            *slider
        )
    );
}

void AutoVocalAudioProcessorEditor::addToggle(
    const juce::String& id,
    const juce::String& name)
{
    auto* toggle = toggles.add(
        new juce::ToggleButton(name)
    );

    toggle->setColour(
        juce::ToggleButton::textColourId,
        juce::Colours::white
    );

    toggle->setColour(
        juce::ToggleButton::tickColourId,
        juce::Colour(0xffa783f0)
    );

    addAndMakeVisible(toggle);

    buttonAttachments.add(
        new ButtonAttachment(
            audioProcessor.parameters,
            id,
            *toggle
        )
    );
}

void AutoVocalAudioProcessorEditor::addChoice(
    const juce::String& id,
    const juce::String& name,
    const juce::StringArray& items)
{
    auto* combo = choices.add(new juce::ComboBox());

    combo->setName(name);

    for (int i = 0; i < items.size(); ++i)
        combo->addItem(items[i], i + 1);

    combo->setColour(
        juce::ComboBox::backgroundColourId,
        juce::Colour(0xff20202a)
    );

    combo->setColour(
        juce::ComboBox::textColourId,
        juce::Colours::white
    );

    combo->setColour(
        juce::ComboBox::outlineColourId,
        juce::Colour(0xff454052)
    );

    combo->setColour(
        juce::ComboBox::arrowColourId,
        juce::Colour(0xffb99aff)
    );

    addAndMakeVisible(combo);

    auto* label = choiceLabels.add(new juce::Label());

    label->setText(
        name,
        juce::dontSendNotification
    );

    label->setColour(
        juce::Label::textColourId,
        juce::Colour(0xffd9d5e3)
    );

    label->setFont(juce::Font(13.0f));

    addAndMakeVisible(label);

    comboAttachments.add(
        new ComboAttachment(
            audioProcessor.parameters,
            id,
            *combo
        )
    );
}

void AutoVocalAudioProcessorEditor::showTab(
    const juce::String& tab)
{
    currentTab = tab;

    // Сначала удаляем привязки параметров.
    sliderAttachments.clear();
    buttonAttachments.clear();
    comboAttachments.clear();

    // Затем сами элементы управления.
    sliders.clear();
    sliderLabels.clear();
    choiceLabels.clear();
    toggles.clear();
    choices.clear();

    if (tab == "MAIN")
    {
        addSlider("input", "Input Gain");
        addSlider("output", "Output Gain");
        addSlider("mix", "Global Mix");
    }
    else if (tab == "TUNE")
    {
        addToggle("tuneEnabled", "Enable Auto-Tune");

        addChoice(
            "tuneKey",
            "Key",
            {
                "C", "C#", "D", "D#", "E", "F",
                "F#", "G", "G#", "A", "A#", "B"
            }
        );

        addChoice(
            "tuneScale",
            "Scale",
            {
                "Chromatic", "Major", "Minor"
            }
        );

        addChoice(
            "tuneMode",
            "Mode",
            {
                "Natural", "Hard Tune"
            }
        );

        addSlider("tuneAmount", "Tune Amount");
        addSlider("retuneSpeed", "Retune Speed");
        addSlider("humanize", "Humanize");
        addSlider("flexTune", "Flex Tune");
    }
    else if (tab == "CLEAN")
    {
        addToggle(
            "cleanEnabled",
            "Enable Noise Reduction"
        );

        addChoice(
            "noiseMode",
            "Noise Mode",
            {
                "Adaptive",
                "Profile"
            }
        );

        addSlider(
            "noiseReduction",
            "Noise Reduction"
        );
    }
    else if (tab == "CHARACTER")
    {
        addSlider("drive", "Drive");
        addSlider("saturation", "Saturation");
        addSlider("warmth", "Warmth");
        addSlider("softClip", "Soft Clip");
        addSlider("characterMix", "Character Mix");
    }
    else if (tab == "DYNAMICS")
    {
        addToggle(
            "compressorEnabled",
            "Enable Compressor"
        );

        addSlider("threshold", "Threshold");
        addSlider("ratio", "Ratio");
        addSlider("attack", "Attack");
        addSlider("release", "Release");
        addSlider("makeup", "Makeup Gain");
        addSlider("compressorMix", "Compressor Mix");
    }
    else if (tab == "SPACE")
    {
        addToggle(
            "reverbEnabled",
            "Enable Reverb"
        );

        addChoice(
            "reverbType",
            "Reverb Type",
            {
                "Room", "Hall", "Plate"
            }
        );

        addSlider("reverbMix", "Reverb Mix");
        addSlider("preDelay", "Pre-Delay");

        addToggle(
            "delayEnabled",
            "Enable Delay"
        );

        addChoice(
            "delayType",
            "Delay Type",
            {
                "Slap", "Stereo"
            }
        );

        addSlider("delayTime", "Delay Time");
        addSlider("delayFeedback", "Feedback");
        addSlider("delayMix", "Delay Mix");
        addSlider("stereoWidth", "Stereo Width");
    }

    // Noise learning controls are only visible on CLEAN.
    const bool showNoiseControls = (tab == "CLEAN");

    noiseLearnButton.setVisible(showNoiseControls);
    noiseLearnStatus.setVisible(showNoiseControls);
    noiseProgressBar.setVisible(showNoiseControls);

    mainTab.setToggleState(
        tab == "MAIN",
        juce::dontSendNotification
    );

    tuneTab.setToggleState(
        tab == "TUNE",
        juce::dontSendNotification
    );

    cleanTab.setToggleState(
        tab == "CLEAN",
        juce::dontSendNotification
    );

    characterTab.setToggleState(
        tab == "CHARACTER",
        juce::dontSendNotification
    );

    dynamicsTab.setToggleState(
        tab == "DYNAMICS",
        juce::dontSendNotification
    );

    spaceTab.setToggleState(
        tab == "SPACE",
        juce::dontSendNotification
    );

    resized();
    repaint();
}

void AutoVocalAudioProcessorEditor::timerCallback()
{
    if (noiseLearningActive)
    {
        noiseProgress = juce::jlimit(
            0.0,
            1.0,
            static_cast<double>(
                audioProcessor.getNoiseLearnProgress()
            )
        );

        if (audioProcessor.hasNoiseProfile() ||
            noiseProgress >= 0.999)
        {
            noiseProgress = 1.0;
            noiseLearningActive = false;

            noiseLearnButton.setButtonText("LEARN NOISE");

            noiseLearnStatus.setText(
                "NOISE PROFILE READY",
                juce::dontSendNotification
            );
        }
        else
        {
            const int percent = juce::roundToInt(
                noiseProgress * 100.0
            );

            noiseLearnStatus.setText(
                "LEARNING NOISE... " +
                juce::String(percent) + "%",
                juce::dontSendNotification
            );
        }
    }
    else if (audioProcessor.hasNoiseProfile())
    {
        noiseProgress = 1.0;

        noiseLearnStatus.setText(
            "NOISE PROFILE READY",
            juce::dontSendNotification
        );
    }

    noiseProgressBar.repaint();
}

void AutoVocalAudioProcessorEditor::loadReference()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Select reference audio",
        juce::File{},
        "*.wav;*.aiff;*.aif;*.flac;*.mp3;*.ogg"
    );

    fileChooser->launchAsync(
        juce::FileBrowserComponent::openMode |
        juce::FileBrowserComponent::canSelectFiles,

        [this](const juce::FileChooser& chooser)
        {
            const auto file = chooser.getResult();

            if (file == juce::File{})
                return;

            std::unique_ptr<juce::AudioFormatReader> reader(
                formatManager.createReaderFor(file)
            );

            if (reader == nullptr)
            {
                statusLabel.setText(
                    "Unsupported audio format",
                    juce::dontSendNotification
                );

                return;
            }

            referenceFile = file;

            referenceLabel.setText(
                referenceFile.getFileName(),
                juce::dontSendNotification
            );

            statusLabel.setText(
                "REFERENCE LOADED",
                juce::dontSendNotification
            );
        }
    );
}

void AutoVocalAudioProcessorEditor::analyzeReference()
{
    if (referenceFile == juce::File{})
    {
        statusLabel.setText(
            "Load a reference first",
            juce::dontSendNotification
        );

        return;
    }

    statusLabel.setText(
        "Reference loaded. Analysis engine is the next DSP stage.",
        juce::dontSendNotification
    );
}

void AutoVocalAudioProcessorEditor::paint(juce::Graphics& g)
{
    const juce::Colour background(0xff111117);
    const juce::Colour panel(0xff191922);
    const juce::Colour accent(0xff9874e5);

    g.fillAll(background);

    g.setColour(panel);

    g.fillRoundedRectangle(
        getLocalBounds().toFloat().reduced(14.0f),
        14.0f
    );

    g.setColour(accent);

    g.fillRoundedRectangle(
        juce::Rectangle<float>(
            28.0f,
            27.0f,
            4.0f,
            43.0f
        ),
        2.0f
    );

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(27.0f, juce::Font::bold));

    g.drawText(
        "AUTO VOCAL",
        44,
        25,
        300,
        34,
        juce::Justification::centredLeft
    );

    g.setColour(juce::Colour(0xff918c9e));
    g.setFont(juce::Font(11.0f));

    g.drawText(
        "VOCAL MIXING ENGINE",
        46,
        58,
        260,
        18,
        juce::Justification::centredLeft
    );

    g.setColour(juce::Colour(0xff302b3b));

    g.drawHorizontalLine(
        91,
        28.0f,
        static_cast<float>(getWidth()) - 28.0f
    );

    g.setColour(juce::Colour(0xff22212c));

    g.fillRoundedRectangle(
        juce::Rectangle<float>(
            28.0f,
            105.0f,
            static_cast<float>(getWidth()) - 56.0f,
            115.0f
        ),
        10.0f
    );

    g.setColour(accent);
    g.setFont(juce::Font(11.0f, juce::Font::bold));

    g.drawText(
        "ARTIST PROFILE",
        43,
        115,
        160,
        20,
        juce::Justification::centredLeft
    );

    g.setColour(juce::Colour(0xff22212c));

    g.fillRoundedRectangle(
        juce::Rectangle<float>(
            28.0f,
            235.0f,
            static_cast<float>(getWidth()) - 56.0f,
            120.0f
        ),
        10.0f
    );

    g.setColour(accent);
    g.setFont(juce::Font(11.0f, juce::Font::bold));

    g.drawText(
        "REFERENCE MATCH",
        43,
        245,
        200,
        20,
        juce::Justification::centredLeft
    );

    g.setColour(juce::Colour(0xff302b3b));

    g.drawHorizontalLine(
        getHeight() - 38,
        28.0f,
        static_cast<float>(getWidth()) - 28.0f
    );

    g.setColour(juce::Colour(0xff777283));
    g.setFont(juce::Font(10.0f));

    g.drawText(
        "DIGITAL STUDIO  |  AUTOVOCAL",
        30,
        getHeight() - 30,
        getWidth() - 60,
        16,
        juce::Justification::centredLeft
    );
}

void AutoVocalAudioProcessorEditor::resized()
{
    const int width = getWidth();

    artistBox.setBounds(44, 145, 220, 34);
    moodBox.setBounds(280, 145, 220, 34);

    applyProfileButton.setBounds(
        width - 210,
        145,
        165,
        36
    );

    loadReferenceButton.setBounds(
        44,
        275,
        175,
        36
    );

    analyzeReferenceButton.setBounds(
        230,
        275,
        205,
        36
    );

    referenceLabel.setBounds(
        450,
        275,
        width - 490,
        36
    );

    statusLabel.setBounds(
        44,
        315,
        width - 88,
        24
    );

    const int tabY = 375;
    const int tabW = (width - 88) / 6;

    mainTab.setBounds(
        44 + tabW * 0,
        tabY,
        tabW - 4,
        34
    );

    tuneTab.setBounds(
        44 + tabW * 1,
        tabY,
        tabW - 4,
        34
    );

    cleanTab.setBounds(
        44 + tabW * 2,
        tabY,
        tabW - 4,
        34
    );

    characterTab.setBounds(
        44 + tabW * 3,
        tabY,
        tabW - 4,
        34
    );

    dynamicsTab.setBounds(
        44 + tabW * 4,
        tabY,
        tabW - 4,
        34
    );

    spaceTab.setBounds(
        44 + tabW * 5,
        tabY,
        tabW - 4,
        34
    );

    const int startY = 500;
    const int columns = 4;
    const int cellW = (width - 100) / columns;
    const int cellH = 78;

    // Toggle controls.
    for (int i = 0; i < toggles.size(); ++i)
    {
        const int col = i % 2;

        toggles[i]->setBounds(
            48 + col * ((width - 110) / 2),
            420,
            (width - 110) / 2,
            30
        );
    }

    // Choice labels and ComboBoxes.
    for (int i = 0; i < choices.size(); ++i)
    {
        const int col = i % 3;

        const int x =
            48 + col * ((width - 120) / 3);

        const int choiceW = (width - 140) / 3;

        choices[i]->setBounds(
            x,
            462,
            choiceW,
            30
        );

        if (i < choiceLabels.size())
        {
            choiceLabels[i]->setBounds(
                x,
                440,
                choiceW,
                20
            );
        }
    }

    // Sliders.
    for (int i = 0; i < sliders.size(); ++i)
    {
        const int col = i % columns;
        const int row = i / columns;

        const int x = 44 + col * cellW;
        const int y = startY + row * cellH;

        sliders[i]->setBounds(
            x + 8,
            y + 16,
            cellW - 16,
            60
        );

        sliderLabels[i]->setBounds(
            x,
            y,
            cellW,
            18
        );
    }

    // Noise profile learning section.
    noiseLearnButton.setBounds(
        48,
        585,
        190,
        36
    );

    noiseProgressBar.setBounds(
        48,
        630,
        width - 110,
        18
    );

    noiseLearnStatus.setBounds(
        48,
        650,
        width - 110,
        20
    );
}
