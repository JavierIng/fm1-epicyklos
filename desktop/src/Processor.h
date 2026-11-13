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
    bool producesMidi() const override { return true; }
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
    // per track: its notes also go out as MIDI on its channel (por pista: sus notas salen por MIDI)
    std::atomic<bool> midiOut[EPK_TRACKS];
    // the voltage of a full-scale sample at the converter, to turn notes into 1 V/octave (voltios a fondo de escala)
    std::atomic<float> cvVolts { 10.0f };

private:
    void capture(EpkSnapshot& s) const;
    void fillCore(int frames, int outIndex, int n);
    void noteAt(int frame, const EpkNoteEvent& e, int n);
    static BusesProperties makeBuses();

    static std::atomic<int> instances;
    bool owner = false;
    double coreRate = 44100.0;
    bool half = false;                     // 88.2 / 96 kHz: the core runs at half and is doubled (a la mitad)
    bool hostPlaying = false;

    juce::SpinLock pendingLock;
    EpkSnapshot pending;
    std::atomic<int> stage { 0 };          // 1: select the pattern; 2: then set everything (en dos pasos)

    struct TrackOut { int note = 60, gate = 0, midiNote = -1, midiLeft = 0; } out[EPK_TRACKS];
    struct Fired { int frame; EpkNoteEvent e; } fired[512], carry[64];
    int nFired = 0, nCarry = 0;
    std::vector<float> ext[EPK_TRACKS];    // what came in for each track this block (lo que entra)
    bool haveExt[EPK_TRACKS] = {};
    float extCore[EPK_TRACKS][256] = {};
    double outRate = 44100.0;

    float coreL[256] = {}, coreR[256] = {};
    int coreN = 0, corePos = 0;
    float prevL = 0.0f, prevR = 0.0f, curL = 0.0f, curR = 0.0f;
    int phase = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EpkProcessor)
};
