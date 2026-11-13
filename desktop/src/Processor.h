// SPDX-License-Identifier: GPL-3.0-only
// EPICYKLOS on the computer: the plugin's audio side (el lado de audio). The sound core (app/dsp) is one global
// engine, as on the FM-1, so only the first instance in a process sounds; the others say so and stay silent.
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "epk.h"

struct EpkSnapshot {                       // what a DAW project keeps (lo que guarda el proyecto del DAW)
    bool valid = false;
    int bank = 0, pattern = 0;
    float tempo = 124.0f;
    float p[EPK_TRACKS][EPK_P_COUNT] = {};
    bool mute[EPK_TRACKS] = {};
    float fx[EPK_FX_COUNT] = {};
    float mod[EPK_MOD_SOURCES][EPK_M_COUNT] = {};
};

class EpkProcessor final : public juce::AudioProcessor {
public:
    EpkProcessor();
    ~EpkProcessor() override;

    void prepareToPlay(double sampleRate, int blockSize) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "EPICYKLOS"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& dest) override;
    void setStateInformation(const void* data, int size) override;

    bool isOwner() const { return owner; }
    bool isStandalone() const { return wrapperType == wrapperType_Standalone; }
    std::atomic<int> selectedTrack { 0 };
    std::atomic<bool> hostHasTransport { false };   // a DAW gives tempo and play (el DAW manda)

private:
    void capture(EpkSnapshot& s) const;
    void fillCore(int frames);

    static std::atomic<int> instances;
    bool owner = false;
    double coreRate = 44100.0;
    bool half = false;                     // 88.2 / 96 kHz: the core runs at half and is doubled (a la mitad)
    bool hostPlaying = false;

    juce::SpinLock pendingLock;
    EpkSnapshot pending;
    std::atomic<int> stage { 0 };          // 1: select the pattern; 2: then set everything (en dos pasos)

    float coreL[256] = {}, coreR[256] = {};
    int coreN = 0, corePos = 0;
    float prevL = 0.0f, prevR = 0.0f, curL = 0.0f, curR = 0.0f;
    int phase = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EpkProcessor)
};
