// SPDX-License-Identifier: GPL-3.0-only
// EPICYKLOS on the computer: the window (la ventana). Not a picture of the FM-1: the orbit of the six tracks, a
// lane of steps for each, and the knobs of the selected track and of the master.
#pragma once
#include "Processor.h"

class EpkLook final : public juce::LookAndFeel_V4 {
public:
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height, float pos, float startAngle,
                          float endAngle, juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height, float sliderPos, float minPos,
                          float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour& background, bool highlighted,
                              bool down) override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;
};

class EpkEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit EpkEditor(EpkProcessor&);
    ~EpkEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    struct Knob {
        juce::Slider slider;
        juce::String name;
        int param = -1;                    // a track parameter, or -1
        int group = 0;
        std::function<float()> get;
        std::function<void(float)> set;
    };
    void timerCallback() override;
    Knob* addKnob(juce::OwnedArray<Knob>& into, const juce::String& name, int group, std::function<float()> get,
                  std::function<void(float)> set);
    void drawOrbit(juce::Graphics&, juce::Rectangle<float>);
    void drawLane(juce::Graphics&, int track);
    void drawGroups(juce::Graphics&);
    juce::Rectangle<float> cell(int track, int step, int length) const;
    static juce::Colour trackColour(int track);

    EpkProcessor& proc;
    EpkLook look;
    juce::TextButton play { "PLAY" }, random { "ALEATORIO" }, undo { "DESHACER" };
    juce::ComboBox scope;
    juce::Slider tempo;
    juce::Label notice;
    juce::OwnedArray<juce::TextButton> banks, patterns, mutes, solos, voices, midis;
    juce::OwnedArray<Knob> levels, sound, fx;
    juce::Rectangle<int> header, orbitArea, laneArea[EPK_TRACKS], stepsArea[EPK_TRACKS], soundArea, fxArea;
    uint32_t lastTrig[EPK_TRACKS] = {};
    float flash[EPK_TRACKS] = {};
    int lastSel = -1, lastEngine = -1;
    uint32_t seed = 0x9E3779B9u;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EpkEditor)
};
