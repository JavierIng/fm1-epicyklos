// SPDX-License-Identifier: GPL-3.0-only
// The computer build's extras (EPK_DESKTOP): the notes the sequencer fires come out as events on the right sample,
// a track's voice can be silenced while its notes go on, and an outside sound goes through a track.
#include "epk.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
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
    // 4. The three sources (EUCLID, SEQ, TURING). A TURING loop on the bass: as many hits as HITS says, notes within
    //    the range above the track's note and more than one; still, it repeats while MUTATE is 0; with MUTATE the hits
    //    move but stay as many; PLAY brings the loop back; RANGE 0 is one note; saved and loaded it is the same loop
    {
        auto setup = [&](float mutate) {
            epk_init(SR);
            epk_pattern_select(0, 3);
            epk_render(l, r, 256);                                   // the pattern is in (ya está el patrón)
            epk_events_take(ev, 64);
            epk_param_set(3, EPK_P_LENGTH, 15.0f / 31.0f);           // 16 steps
            epk_param_set(3, EPK_P_HITS, 6.0f / 16.0f);
            epk_param_set(3, EPK_P_MUTATE, mutate);
            epk_param_set(3, EPK_P_EVERY, 0.0f);
            epk_param_set(3, EPK_P_RETURN, 0.0f);
            epk_param_set(3, EPK_P_ABSENCE, 0.0f);
            epk_param_set(3, EPK_P_LFO_AMT, 0.5f);
            epk_source_set(3, EPK_SRC_TURING);
            for (int b = 0; b < 40; b++) { epk_render(l, r, 256); epk_events_take(ev, 64); }
        };
        auto pop = [](uint32_t m) { int n = 0; for (; m; m &= m - 1) n++; return n; };
        setup(0.0f);
        CHECK(epk_source(3) == EPK_SRC_TURING && epk_turing_range_get(3) == 7 && epk_steps_manual(3) == 2,
              "TURING on: source %d range %d", epk_source(3), epk_turing_range_get(3));
        epk_turing_regen(3, 1234);
        for (int b = 0; b < 40; b++) { epk_render(l, r, 256); epk_events_take(ev, 64); }
        const uint32_t loop0 = epk_pattern(3);
        const int base = 24 + (int)(epk_param_get(3, EPK_P_NOTE) * 60.0f + 0.5f);
        CHECK(pop(loop0) == 6, "the loop has %d hits for HITS 6", pop(loop0));
        epk_play(1);
        int lo = 999, hi = 0, kinds = 0, seen[128] = {}, changed = 0, n3 = 0;
        for (int b = 0; b < (int)(SR * 12 / 256); b++) {
            epk_render(l, r, 256);
            const int n = epk_events_take(ev, 64);
            for (int i = 0; i < n; i++)
                if (ev[i].track == 3) { n3++; lo = std::min(lo, (int)ev[i].note); hi = std::max(hi, (int)ev[i].note); if (!seen[ev[i].note]++) kinds++; }
            changed += epk_pattern(3) != loop0;
        }
        CHECK(n3 > 20 && kinds >= 2 && lo >= base && hi <= base + 12, "TURING notes: %d fired, %d different, %d..%d over %d", n3, kinds, lo, hi, base);
        CHECK(changed == 0, "MUTATE 0: the loop holds (changed in %d blocks)", changed);
        // saved and loaded: the same loop and range
        static uint8_t obj[EPK_BANK_OBJ_BYTES];
        std::memset(obj, 0, sizeof obj);
        obj[0] = 'E'; obj[1] = 'P'; obj[2] = 'K'; obj[3] = 'B'; obj[4] = EPK_BANK_VERSION; obj[5] = 5; obj[6] = 0; obj[7] = 1;
        epk_turing_range(3, 11);
        epk_render(l, r, 256);
        epk_pattern_pack_live(obj + EPK_BANK_HEAD);
        epk_init(SR);
        epk_bank_stage_begin(5);
        epk_bank_stage_half(5, 0, obj, (int)sizeof obj);
        epk_pattern_select(5, 0);
        epk_render(l, r, 256);
        CHECK(epk_source(3) == EPK_SRC_TURING && epk_turing_range_get(3) == 11 && epk_pattern(3) == loop0,
              "saved loop back: source %d range %d loop %08x / %08x", epk_source(3), epk_turing_range_get(3), epk_pattern(3), loop0);
        // SEQ keeps the loop's hits as drawn steps; EUCLID spreads them again
        epk_source_set(3, EPK_SRC_SEQ);
        epk_render(l, r, 256);
        CHECK(epk_source(3) == EPK_SRC_SEQ && epk_pattern(3) == loop0, "TURING -> SEQ keeps the steps");
        epk_source_set(3, EPK_SRC_EUCLID);
        epk_render(l, r, 256);
        CHECK(epk_source(3) == EPK_SRC_EUCLID && pop(epk_pattern(3)) == 6, "EUCLID again: %d hits", pop(epk_pattern(3)));
        // MUTATE: the hits move, as many as before; PLAY again starts from the loop as written
        setup(0.9f);
        epk_turing_regen(3, 99);
        for (int b = 0; b < 40; b++) { epk_render(l, r, 256); epk_events_take(ev, 64); }
        const uint32_t loop1 = epk_pattern(3);
        epk_play(1);
        int moved = 0, bad_count = 0;
        for (int b = 0; b < (int)(SR * 16 / 256); b++) {
            epk_render(l, r, 256);
            epk_events_take(ev, 64);
            moved += epk_pattern(3) != loop1;
            bad_count += pop(epk_pattern(3)) != 6;
        }
        CHECK(moved > 100 && bad_count == 0, "MUTATE 90: the loop moved in %d blocks, %d with another count", moved, bad_count);
        epk_play(1);
        epk_render(l, r, 256);
        CHECK(epk_pattern(3) == loop1, "PLAY again: the loop as written (%08x / %08x)", epk_pattern(3), loop1);
        // RANGE 0: one note
        setup(0.0f);
        epk_turing_range(3, 0);
        epk_play(1);
        int one = 0; std::memset(seen, 0, sizeof seen);
        for (int b = 0; b < (int)(SR * 6 / 256); b++) {
            epk_render(l, r, 256);
            const int n = epk_events_take(ev, 64);
            for (int i = 0; i < n; i++) if (ev[i].track == 3 && !seen[ev[i].note]++) one++;
        }
        CHECK(one == 1, "RANGE 0: %d different notes", one);
    }
    std::printf(fails ? "desktop: %d FAILED\n" : "desktop: all passed\n", fails);
    return fails ? 1 : 0;
}
