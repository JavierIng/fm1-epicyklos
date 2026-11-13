// SPDX-License-Identifier: GPL-3.0-only
// The computer build's extras (EPK_DESKTOP): the notes the sequencer fires come out as events on the right sample,
// a track's voice can be silenced while its notes go on, and an outside sound goes through a track.
#include "epk.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; std::printf("FAIL: "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)
static float l[256], r[256], ext[256];
int main() {
    const float SR = 44100.0f;
    // 1. ORBITA: the kick is four on the floor: its events fall 21339 samples apart (a quarter note at 124 BPM)
    epk_init(SR);
    epk_pattern_select(0, 3);
    epk_play(1);
    long at = 0, last[EPK_TRACKS], gaps_bad = 0;
    int count[EPK_TRACKS] = {}, notes_ok = 1;
    for (long &v : last) v = -1;
    EpkNoteEvent ev[64];
    for (int b = 0; b < (int)(SR * 8 / 256); b++) {
        epk_render(l, r, 256);
        const int n = epk_events_take(ev, 64);
        for (int i = 0; i < n; i++) {
            const long when = at + ev[i].frame;
            CHECK(ev[i].frame >= 0 && ev[i].frame <= 256 && ev[i].track < EPK_TRACKS, "event out of range: frame %d track %d", (int)ev[i].frame, ev[i].track);
            if (ev[i].track == 0) {
                if (last[0] >= 0 && std::labs(when - last[0] - 21339) > 2) gaps_bad++;
                last[0] = when;
            }
            if (ev[i].note < 24 || ev[i].note > 96) notes_ok = 0;
            count[ev[i].track]++;
        }
        at += 256;
    }
    CHECK(count[0] >= 15 && count[0] <= 17 && gaps_bad == 0, "kick: %d events in 8 s, %ld off the grid", count[0], gaps_bad);
    CHECK(count[3] > 4 && notes_ok, "bass: %d events, notes in range %d", count[3], notes_ok);
    CHECK((uint32_t)count[0] == epk_triggers(0) && (uint32_t)count[3] == epk_triggers(3), "events match the triggers: %d/%u %d/%u",
          count[0], epk_triggers(0), count[3], epk_triggers(3));
    // 2. voice off: the kick alone makes no sound but keeps firing
    epk_init(SR);
    epk_pattern_select(0, 3);
    for (int t = 0; t < EPK_TRACKS; t++) epk_solo(t, t == 0);
    for (int f = 0; f < EPK_FX_COUNT; f++) if (f == EPK_FX_DLY_LEVEL || f == EPK_FX_REV_LEVEL) epk_fx_set(f, 0.0f);
    epk_voice_off(0, 1);
    epk_play(1);
    double e = 0.0; int fired = 0;
    for (int b = 0; b < (int)(SR * 3 / 256); b++) {
        epk_render(l, r, 256);
        fired += epk_events_take(ev, 64);
        for (int i = 0; i < 256; i++) e += (double)l[i] * l[i];
    }
    CHECK(e < 1e-3 && fired > 4, "voice off: energy %.3g, %d notes fired", e, fired);
    // 3. an outside sine into that silent track comes out, and stops when the track is muted
    double in_e = 0.0, out_e = 0.0, muted_e = 0.0; float ph = 0.0f; int bad = 0;
    for (int pass = 0; pass < 2; pass++) {
        epk_mute(0, pass);
        for (int b = 0; b < (int)(SR * 2 / 256); b++) {
            for (int i = 0; i < 256; i++) { ext[i] = 0.3f * std::sin(ph); ph += 6.2831853f * 220.0f / SR; in_e += pass ? 0.0 : (double)ext[i] * ext[i]; }
            epk_ext_in(0, ext);
            epk_render(l, r, 256);
            epk_events_take(ev, 64);
            if (b > 40) for (int i = 0; i < 256; i++) { if (!std::isfinite(l[i])) bad++; (pass ? muted_e : out_e) += (double)l[i] * l[i]; }
        }
    }
    epk_ext_in(0, nullptr);
    CHECK(bad == 0 && out_e > in_e * 0.01 && muted_e < out_e * 0.001, "outside sound: in %.2f, out %.2f, muted %.5f", in_e, out_e, muted_e);
    std::printf(fails ? "desktop: %d FAILED\n" : "desktop: all passed\n", fails);
    return fails ? 1 : 0;
}
