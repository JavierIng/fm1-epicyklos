// SPDX-License-Identifier: GPL-3.0-only
// EPICYKLOS on the computer: the window (la ventana). A first working panel, not the final design: transport,
// banks and patterns, the six tracks' mixer, the selected track's sound and a few effects.
#pragma once
#include "Processor.h"

class EpkEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit EpkEditor(EpkProcessor&);
    ~EpkEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct Knob {
        juce::Slider slider;
        juce::Label label;
        std::function<float()> get;
        std::function<void(float)> set;
    };
    void timerCallback() override;
    Knob* addKnob(juce::OwnedArray<Knob>& into, const juce::String& name, std::function<float()> get,
                  std::function<void(float)> set, bool vertical = false);
    static juce::Colour trackColour(int track);

    EpkProcessor& proc;
    juce::TextButton play { "PLAY" }, random { "ALEATORIO" }, undo { "DESHACER" };
    juce::ComboBox scope;
    juce::Slider tempo;
    juce::Label tempoLabel, title, notice, patternTitle, trackTitle, fxTitle;
    juce::OwnedArray<juce::TextButton> banks, patterns, selects, mutes, solos;
    juce::OwnedArray<Knob> levels, sound, fx;
    juce::Rectangle<int> stripArea[EPK_TRACKS];
    uint32_t lastTrig[EPK_TRACKS] = {};
    float flash[EPK_TRACKS] = {};
    int shownBank = -1;
    uint32_t seed = 0x9E3779B9u;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EpkEditor)
};
