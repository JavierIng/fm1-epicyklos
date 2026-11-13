// SPDX-License-Identifier: GPL-3.0-only
// Draws the plugin's window into a PNG without a screen, for the build machine (una captura de la ventana).
#include "Processor.h"
#include "Editor.h"

int main(int argc, char** argv)
{
    if (argc < 2) return 2;
    juce::ScopedJuceInitialiser_GUI gui;
    EpkProcessor proc;
    proc.setPlayConfigDetails(0, 2, 44100.0, 512);
    proc.prepareToPlay(44100.0, 512);
    const int bank = argc > 2 ? juce::String(argv[2]).getIntValue() : 0;
    const int pattern = argc > 3 ? juce::String(argv[3]).getIntValue() : 3;
    const int track = argc > 4 ? juce::String(argv[4]).getIntValue() : 2;
    epk_pattern_select(bank, pattern);
    epk_play(1);
    proc.selectedTrack.store(track);
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    for (int i = 0; i < 400; i++) proc.processBlock(buffer, midi);
    std::unique_ptr<juce::AudioProcessorEditor> editor(proc.createEditor());
    const auto image = editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.5f);
    juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
    out.deleteFile();
    juce::FileOutputStream stream(out);
    if (!stream.openedOk()) return 3;
    juce::PNGImageFormat png;
    return png.writeImageToStream(image, stream) ? 0 : 4;
}
