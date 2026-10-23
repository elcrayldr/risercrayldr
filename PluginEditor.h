#pragma once

#include "PluginProcessor.h"
#include <array>

class RiserLabEditor : public juce::AudioProcessorEditor,
                       private juce::Timer
{
public:
    explicit RiserLabEditor (RiserLabProcessor&);
    ~RiserLabEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct CaptureControl : public juce::Button
    {
        CaptureControl() : juce::Button ("Capture") {}
        void paintButton (juce::Graphics&, bool highlighted, bool pressed) override;
        bool active = false;
    };

    struct DragArea : public juce::Component
    {
        void setData (std::vector<float> newPeaks, const juce::File& newFile);
        void paint (juce::Graphics&) override;
        void mouseDrag (const juce::MouseEvent&) override;

        std::vector<float> peaks;
        juce::File file;
    };

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void timerCallback() override;
    void onCaptureClicked();
    void onGenerateClicked();
    void onPreviewClicked();

    RiserLabProcessor& proc;

    CaptureControl captureButton;
    juce::TextButton generateButton, previewButton;
    juce::Label statusLabel;
    juce::ComboBox presetBox, reverbTypeBox;
    juce::ToggleButton reverbToggle, cutToggle;
    std::unique_ptr<ButtonAttachment> reverbAttachment, cutAttachment;
    std::unique_ptr<ComboBoxAttachment> reverbTypeAttachment;
    std::array<Knob, 11> knobs;
    DragArea dragArea;
    juce::Image logoImage;

    juce::Rectangle<int> titleArea, logoArea;
    juce::String lastMessage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RiserLabEditor)
};
