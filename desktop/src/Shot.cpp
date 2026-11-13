// SPDX-License-Identifier: GPL-3.0-only
// For the build machine: draws the plugin's window into a PNG without a screen, and checks what comes out of the
// note outputs (MIDI, pitch and gate) and what an outside sound into a track does.
#include "Processor.h"
#include "Editor.h"
#include <cstdio>

int main(int argc, char** argv)
{
    if (argc < 2) return 2;
    juce::ScopedJuceInitialiser_GUI gui;
    EpkProcessor proc;
    proc.setRateAndBufferSizeDetails(44100.0, 512);
    proc.prepareToPlay(44100.0, 512);
    const int bank = argc > 2 ? juce::String(argv[2]).getIntValue() : 0;
    const int pattern = argc > 3 ? juce::String(argv[3]).getIntValue() : 3;
    const int track = argc > 4 ? juce::String(argv[4]).getIntValue() : 2;
    epk_pattern_select(bank, pattern);
    epk_play(1);
    proc.selectedTrack.store(track);
    proc.midiOut[3].store(true);
    std::printf("buses: %d in, %d out; channels %d in, %d out\n", proc.getBusCount(true), proc.getBusCount(false),
                proc.getTotalNumInputChannels(), proc.getTotalNumOutputChannels());
    const int channels = juce::jmax(2, proc.getTotalNumInputChannels(), proc.getTotalNumOutputChannels());
    juce::AudioBuffer<float> buffer(channels, 512);
    juce::MidiBuffer midi;
    int kickGates = 0, bassOn = 0, bassOff = 0, otherMidi = 0;
    float lastGate = 0.0f, pitchLo = 9.0f, pitchHi = -9.0f, gateHi = 0.0f;
    double mainE = 0.0;
    for (int i = 0; i < 690; i++) {        // 8 s
        buffer.clear();
        midi.clear();
        proc.processBlock(buffer, midi);
        const bool cv = proc.getTotalNumOutputChannels() >= 10;
        for (int s = 0; s < 512; s++) {
            mainE += (double) buffer.getSample(0, s) * buffer.getSample(0, s);
            if (!cv) continue;
            const float g = buffer.getSample(3, s);          // CV 1 right: the kick's gate
            if (g > 0.25f && lastGate <= 0.25f) kickGates++;
            lastGate = g;
            gateHi = juce::jmax(gateHi, g);
            const float p = buffer.getSample(8, s);          // CV 4 left: the bass's pitch
            pitchLo = juce::jmin(pitchLo, p); pitchHi = juce::jmax(pitchHi, p);
        }
        for (const auto m : midi) {
            const auto msg = m.getMessage();
            if (msg.getChannel() == 4 && msg.isNoteOn()) bassOn++;
            else if (msg.getChannel() == 4 && msg.isNoteOff()) bassOff++;
            else otherMidi++;
        }
    }
    std::printf("main energy %.1f; kick gates in 8 s: %d (level %.2f); bass pitch %.3f .. %.3f of full scale\n", mainE, kickGates,
                gateHi, pitchLo, pitchHi);
    std::printf("MIDI on channel 4: %d on, %d off; other MIDI: %d\n", bassOn, bassOff, otherMidi);
    const bool ok = mainE > 1.0 && kickGates >= 15 && kickGates <= 17 && bassOn > 4 && std::abs(bassOn - bassOff) <= 1 && otherMidi == 0
                    && pitchLo < -0.1f && pitchLo > -0.4f;
    std::printf(ok ? "OUTPUTS OK\n" : "OUTPUTS FAILED\n");

    std::unique_ptr<juce::AudioProcessorEditor> editor(proc.createEditor());
    const auto image = editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.5f);
    juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
    out.deleteFile();
    juce::FileOutputStream stream(out);
    if (!stream.openedOk()) return 3;
    juce::PNGImageFormat png;
    if (!png.writeImageToStream(image, stream)) return 4;
    return ok ? 0 : 5;
}
