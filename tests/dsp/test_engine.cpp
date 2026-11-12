// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Javier Sánchez
// Host tests of the DSP core (pruebas del núcleo DSP en el ordenador): timing to the sample, trigger counts,
// mute/solo, no NaN, levels. Writes tests/dsp/out/groove.wav to listen to.
//   c++ -std=c++14 -O2 -DPLAT_HOST -I app/dsp -I base/include tests/dsp/test_engine.cpp app/dsp/engine.cpp app/dsp/factory.cpp -o build/test_engine
#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "epk.h"

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { std::printf("FAIL %s:%d: ", __FILE__, __LINE__); \
    std::printf(__VA_ARGS__); std::printf("\n"); failures++; } } while (0)

static const float SR = 44100.0f;

static void write_wav(const char *path, const std::vector<float> &l, const std::vector<float> &r) {
    FILE *f = std::fopen(path, "wb");
    if (!f) return;
    const uint32_t n = (uint32_t)l.size(), bytes = n * 4;
    const uint32_t hdr[] = {0x46464952u, 36 + bytes, 0x45564157u, 0x20746D66u, 16, 0x00020001u, 44100, 44100 * 4,
                            0x00100004u, 0x61746164u, bytes};
    std::fwrite(hdr, 4, 11, f);
    for (uint32_t i = 0; i < n; i++) {
        int16_t s[2] = {(int16_t)std::lrint(l[i] * 32000.0f), (int16_t)std::lrint(r[i] * 32000.0f)};
        std::fwrite(s, 2, 2, f);
    }
    std::fclose(f);
}

// Sample index of every trigger of a track, rendering one frame at a time (índice de cada disparo)
static std::vector<long> trigger_times(int track, float seconds) {
    std::vector<long> t;
    float l, r;
    uint32_t last = epk_triggers(track);
    for (long i = 0; i < (long)(seconds * SR); i++) {
        epk_render(&l, &r, 1);
        uint32_t now = epk_triggers(track);
        if (now != last) { t.push_back(i); last = now; }
    }
    return t;
}

int main() {
    // 1. Timing: track 1, 1/16 at 124 BPM, 4 euclidean hits in 16 -> one hit every 4 steps = one beat
    epk_init(SR);
    epk_pattern_select(0, 3);                                       // ORBITA: kick 4 in 16, stab 3 in 12
    epk_play(1);
    std::vector<long> t = trigger_times(0, 4.0f);
    const double beat = 60.0 / 124.0 * SR;                       // samples per beat
    CHECK(t.size() >= 8 && t.size() <= 9, "track 1: %zu triggers in 4 s (expected 8-9)", t.size());
    CHECK(!t.empty() && t[0] == 0, "the first step plays at sample 0 (got %ld)", t.empty() ? -1L : t[0]);
    for (size_t i = 0; i < t.size(); i++) {
        double err = (double)t[i] - (double)i * beat;
        CHECK(std::fabs(err) <= 1.0, "trigger %zu at %ld, %.2f samples off the grid", i, t[i], err);
    }

    // 2. Polymetry: track 3 has 12 steps, track 1 has 16; after 48 steps both are back at step 0 together
    epk_init(SR);
    epk_pattern_select(0, 3);
    epk_play(1);
    float l[256], r[256];
    const long steps48 = (long)std::lround(48.0 * 60.0 / 124.0 / 4.0 * SR);
    long done = 0;
    while (done < steps48 - 128) { uint32_t n = steps48 - 128 - done > 256 ? 256 : (uint32_t)(steps48 - 128 - done);
        epk_render(l, r, n); done += n; }
    CHECK(epk_step(0) == 15 && epk_step(2) == 11, "before step 48: tracks at %d and %d (expected 15 and 11)",
          epk_step(0), epk_step(2));
    epk_render(l, r, 256);
    CHECK(epk_step(0) == 0 && epk_step(2) == 0, "at step 48: tracks at %d and %d (expected 0 and 0)",
          epk_step(0), epk_step(2));

    // 3. Mute and solo (silencio y solo)
    epk_init(SR);
    epk_play(1);
    for (int i = 0; i < EPK_TRACKS; i++) epk_solo(i, i == 0);
    CHECK(epk_audible(0) && !epk_audible(1), "solo on track 1: track 1 audible, track 2 not");
    epk_solo(0, 0);
    epk_mute(1, 1);
    CHECK(!epk_audible(1) && epk_audible(0), "mute on track 2");
    epk_mute(1, 0);

    // 4. A groove, 8 s: no NaN, a sane level; written out to listen to (para escuchar)
    epk_init(SR);
    epk_pattern_select(0, 3);
    epk_play(1);
    std::vector<float> L, R;
    float peak = 0.0f, sum2 = 0.0f;
    int nan = 0;
    for (int b = 0; b < (int)(8.0f * SR / 256); b++) {
        epk_render(l, r, 256);
        for (int i = 0; i < 256; i++) {
            if (!std::isfinite(l[i]) || !std::isfinite(r[i])) nan++;
            peak = std::fmax(peak, std::fmax(std::fabs(l[i]), std::fabs(r[i])));
            sum2 += l[i] * l[i];
            L.push_back(l[i]); R.push_back(r[i]);
        }
    }
    const float rms = std::sqrt(sum2 / (float)L.size());
    CHECK(nan == 0, "%d non-finite samples", nan);
    CHECK(peak > 0.2f && peak < 1.0f, "peak %.3f (expected 0.2..1)", peak);
    CHECK(rms > 0.02f, "rms %.4f: too quiet", rms);
    std::printf("groove: peak %.3f, rms %.4f, triggers per track:", peak, rms);
    for (int i = 0; i < EPK_TRACKS; i++) std::printf(" %u", epk_triggers(i));
    std::printf("\n");
    write_wav("tests/dsp/out/groove.wav", L, R);

    // 5. Stop: silence returns (vuelve el silencio)
    epk_play(0);
    float tail = 0.0f;
    for (int b = 0; b < (int)(7.0f * SR / 256); b++) {              // the plate rings a few seconds (la placa suena)
        epk_render(l, r, 256);
        if (b > (int)(6.0f * SR / 256)) for (int i = 0; i < 256; i++) tail = std::fmax(tail, std::fabs(l[i]));
    }
    CHECK(tail < 0.001f, "6 s after stop the output is %.5f (expected silence)", tail);
    CHECK(epk_step(0) == -1, "stopped: no step");

    // 6. Patterns (patrones): a change waits for the bar; mutes stay; edits stay in their pattern
    epk_init(SR);
    epk_pattern_select(0, 3);
    epk_play(1);
    epk_render(l, r, 1);
    CHECK(epk_pattern_current() == 3, "stopped then play: pattern 4 now (got %d)", epk_pattern_current() + 1);
    const long sixteenth = (long)std::lround(60.0 / 124.0 / 4.0 * SR);
    for (long i = 1; i < 5 * sixteenth; i++) epk_render(l, r, 1);  // five sixteenths into the bar
    epk_mute(0, 1);
    epk_pattern_select(0, 7);                                       // APOGEO: no kick
    long switched = -1;
    for (long i = 5 * sixteenth; i < 20 * sixteenth; i++) {
        epk_render(l, r, 1);
        if (switched < 0 && epk_pattern_current() == 7) switched = i;
    }
    CHECK(epk_pattern_queued() == -1, "the queue is empty after the change");
    const long bar = (long)std::lround(16.0 * 60.0 / 124.0 / 4.0 * SR);   // the exact bar (el compás exacto)
    CHECK(std::labs(switched - bar) <= 1, "the change came at sample %ld, the bar starts at %ld", switched, bar);
    CHECK(epk_muted(0), "track 1 is still muted after the pattern change (el mute es del proyecto)");
    CHECK(epk_step(2) >= 0 && epk_step(2) <= 4, "the new pattern restarted its steps (track 3 at %d)", epk_step(2));
    epk_param_set(2, EPK_P_TONE, 0.123f);                        // an edit on APOGEO
    epk_play(0);
    epk_pattern_select(0, 4);
    epk_render(l, r, 1);
    CHECK(epk_pattern_current() == 4, "stopped: the change is immediate");
    epk_pattern_select(0, 7);
    epk_render(l, r, 1);
    CHECK(std::fabs(epk_param_get(2, EPK_P_TONE) - 0.123f) < 1e-6f, "the edit stayed in its pattern (%.3f)",
          epk_param_get(2, EPK_P_TONE));
    CHECK(std::strcmp(epk_pattern_name(0, 0), "NEBULA") == 0, "pattern names");

    // 7. Every factory pattern plays without NaN or overload (los 16 patrones suenan)
    for (int pt = 0; pt < EPK_PATTERNS; pt++) {
        epk_init(SR);
        epk_pattern_select(0, pt);
        epk_play(1);
        float pk = 0.0f;
        int bad = 0;
        for (int b = 0; b < (int)(4.0f * SR / 256); b++) {
            epk_render(l, r, 256);
            for (int i = 0; i < 256; i++) {
                if (!std::isfinite(l[i])) bad++;
                pk = std::fmax(pk, std::fabs(l[i]));
            }
        }
        CHECK(bad == 0 && pk > 0.05f && pk < 1.0f, "pattern %d %s: peak %.3f, %d bad", pt + 1, epk_pattern_name(0, pt),
              pk, bad);
    }

    // 8. Steps by hand (pasos a mano): a toggle makes the track manual; ROTATE still turns it; euclid comes back
    epk_init(SR);
    epk_pattern_select(0, 3);
    epk_render(l, r, 1);
    const uint32_t eu = epk_pattern(0);                           // kick: 4 in 16 -> steps 0 4 8 12
    CHECK(eu == 0x1111u, "kick mask %x (expected 1111)", eu);
    epk_step_toggle(0, 2);
    epk_step_toggle(0, 4);
    epk_render(l, r, 1);
    CHECK(epk_steps_manual(0) && epk_pattern(0) == 0x1105u, "after toggles: manual %d mask %x (expected 1105)",
          epk_steps_manual(0), epk_pattern(0));
    epk_param_set(0, EPK_P_ROTATE, 1.0f / 31.0f);
    CHECK(epk_pattern(0) == 0x220Au, "rotated by one: %x (expected 220a)", epk_pattern(0));
    epk_step_set(0, 3, EPK_PROB_50, EPK_COND_1_2, 0);                // step 3 as it plays = index 2 before rotation
    epk_render(l, r, 1);
    CHECK(epk_step_prob(0, 3) == EPK_PROB_50 && epk_step_cond(0, 3) == EPK_COND_1_2, "step chance and condition");
    epk_param_set(0, EPK_P_ROTATE, 0.0f);
    CHECK(epk_step_prob(0, 2) == EPK_PROB_50, "the step's chance turns with the pattern");
    epk_steps_euclid(0);
    epk_render(l, r, 1);
    CHECK(!epk_steps_manual(0) && epk_pattern(0) == 0x1111u, "back to euclid: %x", epk_pattern(0));

    // 9. Conditions and chance (condiciones y azar): track 1 alone, one hit on step 0, 16 cycles
    {
        const long cyc = (long)std::lround(16.0 * 60.0 / 124.0 / 4.0 * SR);
        struct { int prob, cond; unsigned lo, hi; } cases[] = {
            {EPK_PROB_100, EPK_COND_NONE, 16, 16}, {EPK_PROB_100, EPK_COND_1_2, 8, 8}, {EPK_PROB_100, EPK_COND_2_2, 8, 8},
            {EPK_PROB_100, EPK_COND_3_4, 4, 4}, {EPK_PROB_50, EPK_COND_NONE, 3, 13}, {EPK_PROB_25, EPK_COND_NONE, 1, 9}};
        for (auto &c : cases) {
            epk_init(SR);
            epk_pattern_select(0, 3);
            epk_render(l, r, 1);
            epk_param_set(0, EPK_P_HITS, 1.0f / 16.0f);
            epk_step_set(0, 0, c.prob, c.cond, 0);
            epk_play(1);
            for (long i = 0; i + 256 < 16 * cyc; i += 256) epk_render(l, r, 256);
            const unsigned n = epk_triggers(0);
            CHECK(n >= c.lo && n <= c.hi, "prob %d cond %d: %u triggers in 16 cycles (expected %u..%u)", c.prob,
                  c.cond, n, c.lo, c.hi);
        }
    }

    // 10. Random and undo (aleatorio y deshacer); banks (bancos)
    epk_init(SR);
    epk_pattern_select(0, 3);
    epk_render(l, r, 1);
    float before[EPK_TRACKS][EPK_P_PATTERN_COUNT];
    for (int t = 0; t < EPK_TRACKS; t++) for (int q = 0; q < EPK_P_PATTERN_COUNT; q++) before[t][q] = epk_param_get(t, q);
    epk_undo_save();
    epk_pattern_randomize(12345);
    epk_render(l, r, 1);
    int changed = 0;
    for (int t = 0; t < EPK_TRACKS; t++) for (int q = 0; q < EPK_P_PATTERN_COUNT; q++) changed += epk_param_get(t, q) != before[t][q];
    CHECK(changed > 20, "the random pattern changed %d parameters", changed);
    CHECK(epk_engine(0) == EPK_ENG_GRAVE && epk_engine(1) == EPK_ENG_METAL, "the tracks keep their engines");
    CHECK(epk_undo_ready(), "undo is ready");
    epk_undo_restore();
    epk_render(l, r, 1);
    changed = 0;
    for (int t = 0; t < EPK_TRACKS; t++) for (int q = 0; q < EPK_P_PATTERN_COUNT; q++) changed += epk_param_get(t, q) != before[t][q];
    CHECK(changed == 0 && !epk_undo_ready(), "undo brought the pattern back (%d differ)", changed);
    epk_mute(0, 1);
    epk_pattern_select(3, 6);                                    // bank D, pattern 7
    epk_render(l, r, 1);
    CHECK(epk_bank_current() == 3 && epk_pattern_current() == 6 && epk_muted(0), "bank D pattern 7, mute kept");
    CHECK(std::strcmp(epk_pattern_name(3, 6), "GEN D07") == 0, "generated name: %s", epk_pattern_name(3, 6));
    const float d7 = epk_param_get(2, EPK_P_NOTE);
    epk_pattern_select(0, 3);
    epk_render(l, r, 1);
    epk_pattern_select(3, 6);
    epk_render(l, r, 1);
    CHECK(epk_param_get(2, EPK_P_NOTE) == d7, "a generated bank is the same every time");
    // every generated pattern plays clean (los 160 generados suenan limpios)
    {
        int bad = 0, quiet = 0;
        float worst = 0.0f;
        for (int b = 1; b < EPK_BANKS; b++)
            for (int pt = 0; pt < EPK_PATTERNS; pt++) {
                epk_init(SR);
                epk_pattern_select(b, pt);
                epk_play(1);
                float pk = 0.0f;
                for (int k = 0; k < (int)(2.0f * SR / 256); k++) {
                    epk_render(l, r, 256);
                    for (int i = 0; i < 256; i++) { if (!std::isfinite(l[i])) bad++; pk = std::fmax(pk, std::fabs(l[i])); }
                }
                quiet += pk < 0.03f;
                worst = std::fmax(worst, pk);
            }
        CHECK(bad == 0 && quiet == 0 && worst < 1.0f, "generated banks: %d bad samples, %d silent patterns, peak %.3f",
              bad, quiet, worst);
    }

    // 12. Off the grid (fuera de la rejilla): SHIFT moves the whole track, micro one step, SWING the odd steps
    {
        const double step = 60.0 / 124.0 / 4.0 * SR;                 // samples per sixteenth
        auto fresh = [&]() {
            epk_init(SR);
            epk_pattern_select(0, 3);                                // kick 4 in 16: steps 0, 4, 8, 12
            epk_render(l, r, 1);
        };
        fresh();                                                     // late by 0.225 of a step
        epk_param_set(0, EPK_P_SHIFT, 0.75f);
        epk_play(1);
        std::vector<long> a = trigger_times(0, 2.0f);
        CHECK(a.size() >= 4, "shift late: %zu triggers", a.size());
        for (size_t i = 0; i < a.size(); i++)
            CHECK(std::fabs((double)a[i] - ((double)i * 4.0 + 0.225) * step) <= 1.5, "late: trigger %zu at %ld", i, a[i]);
        fresh();                                                     // early by 0.225: the first plays at 0
        epk_param_set(0, EPK_P_SHIFT, 0.25f);
        epk_play(1);
        a = trigger_times(0, 2.0f);
        CHECK(a.size() >= 4 && a[0] == 0, "shift early: %zu triggers, first at %ld", a.size(), a.empty() ? -1L : a[0]);
        for (size_t i = 1; i < a.size(); i++)
            CHECK(std::fabs((double)a[i] - ((double)i * 4.0 - 0.225) * step) <= 1.5, "early: trigger %zu at %ld", i, a[i]);
        fresh();                                                     // micro: step 4 early by 2/8, step 8 late by 3/8
        epk_step_set(0, 4, EPK_PROB_100, EPK_COND_NONE, -2);
        epk_step_set(0, 8, EPK_PROB_100, EPK_COND_NONE, 3);
        epk_render(l, r, 1);
        CHECK(epk_step_micro(0, 4) == -2 && epk_step_micro(0, 8) == 3 && !epk_steps_manual(0),
              "micro stored: %d %d", epk_step_micro(0, 4), epk_step_micro(0, 8));
        epk_play(1);
        a = trigger_times(0, 1.9f);
        const double want[4] = {0.0, 3.75, 8.375, 12.0};
        CHECK(a.size() >= 4, "micro: %zu triggers", a.size());
        for (size_t i = 0; i < 4 && i < a.size(); i++)
            CHECK(std::fabs((double)a[i] - want[i] * step) <= 1.5, "micro: trigger %zu at %ld (want %.0f)", i, a[i],
                  want[i] * step);
        fresh();                                                     // swing: hats on every step, the odd ones late
        epk_param_set(1, EPK_P_LENGTH, 15.0f / 31.0f);
        epk_param_set(1, EPK_P_HITS, 1.0f);
        epk_param_set(1, EPK_P_ROTATE, 0.0f);
        epk_param_set(1, EPK_P_SWING, 0.5f);
        epk_play(1);
        a = trigger_times(1, 1.0f);
        CHECK(a.size() >= 8, "swing: %zu triggers", a.size());
        for (size_t i = 0; i < a.size(); i++)
            CHECK(std::fabs((double)a[i] - ((double)i + (i & 1 ? 0.2 : 0.0)) * step) <= 1.5, "swing: trigger %zu at %ld",
                  i, a[i]);
    }

    // 13. GOLPE: clap, snare and rim sound, without NaN (palma, caja y aro suenan)
    for (float type : {0.0f, 0.5f, 1.0f}) {
        epk_init(SR);
        for (int i = 0; i < EPK_TRACKS; i++) epk_solo(i, i == 5);
        epk_param_set(5, EPK_P_CHARACTER, type);
        epk_note_on(5, 52, 1.0f);
        float pk = 0.0f;
        int bad = 0;
        for (int k = 0; k < 40; k++) {
            epk_render(l, r, 256);
            for (int i = 0; i < 256; i++) { bad += !std::isfinite(l[i]); pk = std::fmax(pk, std::fabs(l[i])); }
        }
        CHECK(epk_engine(5) == EPK_ENG_GOLPE && bad == 0 && pk > 0.05f && pk < 1.0f, "GOLPE type %.1f: engine %d, peak %.3f",
              type, epk_engine(5), pk);
        std::printf("GOLPE type %.1f: peak %.3f\n", type, pk);
    }

    // 14. GOLPE's knobs change the sound (los knobs cambian el sonido), and TAMBOR sounds: the energy of a hit
    // must move by at least 25 % between a knob's ends, for each type.
    {
        auto energy = [&](int eng, float type, int param, float v) {
            epk_init(SR);
            for (int i = 0; i < EPK_TRACKS; i++) epk_solo(i, i == 5);
            epk_param_set(5, EPK_P_ENGINE, (float)eng / (float)(EPK_ENG_COUNT - 1));
            epk_param_set(5, EPK_P_CHARACTER, type);
            if (param >= 0) epk_param_set(5, param, v);
            epk_note_on(5, 52, 1.0f);
            double e = 0.0, hi = 0.0;
            float prev = 0.0f;
            for (int k = 0; k < 120; k++) {
                epk_render(l, r, 256);
                for (int i = 0; i < 256; i++) { e += (double)l[i] * l[i]; hi += (double)(l[i] - prev) * (l[i] - prev); prev = l[i]; }
            }
            return std::make_pair(e, hi / (e + 1e-12));              // energy, and how bright (brillo)
        };
        static const int kKnob[4] = {EPK_P_TONE, EPK_P_DECAY, EPK_P_EXTRA1, EPK_P_EXTRA2};
        static const char *const kKnobName[4] = {"TONE", "DECAY", "EXTRA1", "EXTRA2"};
        for (float type : {0.0f, 0.5f, 1.0f})
            for (int k = 0; k < 4; k++) {
                const auto a = energy(EPK_ENG_GOLPE, type, kKnob[k], 0.05f), b = energy(EPK_ENG_GOLPE, type, kKnob[k], 0.95f);
                const double de = std::fabs(a.first - b.first) / std::fmax(a.first, b.first);
                const double db = std::fabs(a.second - b.second) / std::fmax(a.second, b.second);
                std::printf("GOLPE type %.1f %-6s: energy moves %3.0f %%, brightness %3.0f %%\n", type, kKnobName[k], de * 100, db * 100);
                CHECK(de > 0.25 || db > 0.25, "GOLPE type %.1f: %s changes the sound by %.0f %% / %.0f %%", type,
                      kKnobName[k], de * 100, db * 100);
            }
        for (float type : {0.0f, 0.5f, 1.0f}) {
            const auto t0 = energy(EPK_ENG_TAMBOR, type, -1, 0.0f);
            CHECK(epk_engine(5) == EPK_ENG_TAMBOR && t0.first > 1.0 && std::isfinite(t0.first), "TAMBOR type %.1f: energy %.2f",
                  type, t0.first);
        }
    }

    // 15. The sends (envíos): the echo comes back on time, PINGPONG alternates sides, the reverb leaves a tail,
    // and nothing blows up with the feedback and the decay at their ends (nada se dispara).
    {
        auto hit = [&](int type, float fb, float send_a, float send_b, float decay, float seconds, std::vector<float> &L2,
                       std::vector<float> &R2) {
            epk_init(SR);
            for (int i = 0; i < EPK_TRACKS; i++) epk_solo(i, i == 5);
            epk_param_set(5, EPK_P_PAN, 0.5f);
            epk_param_set(5, EPK_P_SEND_A, send_a);
            epk_param_set(5, EPK_P_SEND_B, send_b);
            epk_param_set(5, EPK_P_DECAY, 0.1f);
            epk_fx_set(EPK_FX_DLY_TYPE, (float)type / (float)(EPK_DLY_COUNT - 1));
            epk_fx_set(EPK_FX_DLY_FEEDBACK, fb);
            epk_fx_set(EPK_FX_DLY_TO_REV, 0.0f);
            epk_fx_set(EPK_FX_DLY_WEAR, 0.0f);
            epk_fx_set(EPK_FX_REV_DECAY, decay);
            epk_render(l, r, 256);                                   // the delay settles on its time
            epk_note_on(5, 52, 1.0f);
            L2.clear(); R2.clear();
            for (int k = 0; k < (int)(seconds * SR / 256); k++) {
                epk_render(l, r, 256);
                L2.insert(L2.end(), l, l + 256);
                R2.insert(R2.end(), r, r + 256);
            }
        };
        auto rms = [&](const std::vector<float> &v, double from_s, double to_s) {
            double e = 0.0;
            const size_t a = (size_t)(from_s * SR), b = std::min(v.size(), (size_t)(to_s * SR));
            for (size_t i = a; i < b; i++) e += (double)v[i] * v[i];
            return std::sqrt(e / (double)(b > a ? b - a : 1));
        };
        std::vector<float> A, B;
        const double echo = 3.0 * 15.0 / 124.0;                      // 3 sixteenths at 124 BPM = 0.363 s
        for (int type = 0; type < EPK_DLY_COUNT; type++) {
            hit(type, 0.5f, 1.0f, 0.0f, 0.5f, 2.0f, A, B);
            const double gap = rms(A, echo - 0.10, echo - 0.02), first = rms(A, echo, echo + 0.08);
            int bad = 0;
            for (float v : A) bad += !std::isfinite(v);
            CHECK(bad == 0 && first > 0.01 && first > gap * 4.0, "delay type %d: echo %.4f against %.4f before it", type,
                  first, gap);
            if (type == EPK_DLY_PINGPONG) {
                const double l1 = rms(A, echo, echo + 0.08), r1 = rms(B, echo, echo + 0.08);
                const double l2 = rms(A, 2 * echo, 2 * echo + 0.08), r2 = rms(B, 2 * echo, 2 * echo + 0.08);
                CHECK(l1 > r1 * 3.0 && r2 > l2 * 3.0, "pingpong: first echo L %.4f R %.4f, second L %.4f R %.4f", l1, r1, l2, r2);
            }
            hit(type, 1.0f, 1.0f, 1.0f, 1.0f, 20.0f, A, B);          // everything at the top, 20 s
            float pk = 0.0f;
            bad = 0;
            for (float v : A) { bad += !std::isfinite(v); pk = std::fmax(pk, std::fabs(v)); }
            CHECK(bad == 0 && pk <= 1.0f, "delay type %d at full feedback and decay: %d bad samples, peak %.3f", type, bad, pk);
        }
        hit(0, 0.0f, 0.0f, 1.0f, 0.9f, 4.0f, A, B);                  // the plate alone (solo la placa)
        const double early = rms(A, 0.3, 0.6), late = rms(A, 2.0, 2.5), side = rms(B, 0.3, 0.6);
        CHECK(early > 0.003 && late > 0.0003 && late < early && side > 0.003, "plate: tail %.5f at 0.3 s, %.5f at 2 s, right %.5f",
              early, late, side);
        double diff = 0.0;
        for (size_t i = (size_t)(0.3 * SR); i < (size_t)(0.6 * SR); i++) diff += std::fabs(A[i] - B[i]);
        CHECK(diff > 1.0, "plate: left and right differ (stereo)");
        hit(0, 0.0f, 0.0f, 1.0f, 0.0f, 4.0f, A, B);
        CHECK(rms(A, 3.0, 3.5) < 0.0002, "plate at the shortest decay: %.6f after 3 s", rms(A, 3.0, 3.5));
        std::printf("plate: tail %.4f at 0.3 s, %.4f at 2 s (decay 0.9)\n", early, late);
    }

    // 16. Dub gestures (gestos de dub): THROW sends a dry track to the delay; FREEZE holds the loop and stops
    // listening; DUCK pushes the returns down under the kick.
    {
        auto setup = [&]() {
            epk_init(SR);
            for (int i = 0; i < EPK_TRACKS; i++) epk_solo(i, i == 5);
            epk_param_set(5, EPK_P_SEND_A, 0.0f);
            epk_param_set(5, EPK_P_SEND_B, 0.0f);
            epk_param_set(5, EPK_P_DECAY, 0.1f);
            epk_fx_set(EPK_FX_DLY_TO_REV, 0.0f);
            epk_fx_set(EPK_FX_DLY_FEEDBACK, 0.3f);
            epk_fx_set(EPK_FX_DUCK, 0.0f);
            epk_render(l, r, 256);
        };
        auto returns = [&](float seconds) {                          // the returns' peak, block by block, summed
            double sum = 0.0;
            epk_fx_meter();
            for (int k = 0; k < (int)(seconds * SR / 256); k++) { epk_render(l, r, 256); sum += epk_fx_meter(); }
            return sum;
        };
        setup();                                                     // no send: no echo
        epk_note_on(5, 52, 1.0f);
        const double dry = returns(1.0f);
        setup();                                                     // the same, thrown
        epk_throw(5, 1);
        epk_note_on(5, 52, 1.0f);
        const double thrown = returns(1.0f);
        CHECK(epk_thrown(5) && dry < 0.01 && thrown > 0.05, "throw: returns %.3f dry, %.3f thrown", dry, thrown);
        epk_throw(5, 0);
        CHECK(!epk_thrown(5), "throw released");

        setup();                                                     // freeze: what is in the loop stays there
        epk_param_set(5, EPK_P_SEND_A, 1.0f);
        epk_note_on(5, 52, 1.0f);
        returns(0.2f);                                               // the hit is in the line, not out yet
        epk_freeze(1);
        const double loop1 = returns(1.45f);                         // 4 turns of the 0.363 s loop
        returns(4.0f);
        const double loop2 = returns(1.45f);
        epk_freeze(0);
        returns(4.0f);
        const double late = returns(1.45f);
        CHECK(loop1 > 0.05 && loop2 > loop1 * 0.6, "freeze: returns %.3f, then %.3f 4 s later", loop1, loop2);
        CHECK(late < loop2 * 0.2, "freeze off: the loop dies away (%.4f against %.3f)", late, loop2);
        setup();                                                     // frozen and empty: a new hit stays out
        epk_param_set(5, EPK_P_SEND_A, 1.0f);
        epk_freeze(1);
        epk_note_on(5, 52, 1.0f);
        const double empty = returns(1.0f);
        CHECK(epk_frozen() && empty < 0.01, "frozen: a new hit does not get in (%.4f)", empty);
        epk_freeze(0);

        auto duck_run = [&](float duck) {                            // kick 4 in 16, sending to the reverb
            epk_init(SR);
            for (int i = 0; i < EPK_TRACKS; i++) epk_solo(i, i == 0);
            epk_param_set(0, EPK_P_SEND_B, 1.0f);
            epk_fx_set(EPK_FX_DUCK, duck);
            epk_pattern_select(0, 3);
            epk_render(l, r, 1);
            epk_play(1);
            returns(2.0f);
            return returns(4.0f);
        };
        const double open = duck_run(0.0f), ducked = duck_run(1.0f);
        CHECK(ducked < open * 0.75, "duck: returns %.1f open, %.1f ducked", open, ducked);
        std::printf("duck: returns %.1f open, %.1f ducked; freeze %.3f then %.3f\n", open, ducked, loop1, loop2);
    }

    // 17. The master (cadena del master): the filter darkens below the middle and thins above it, the middle is
    // transparent, the compressor narrows the dynamics, and nothing blows up with everything at the top.
    {
        auto run = [&](float flt, float reso, float dbl, float comp, float sat, double &energy, double &bright,
                       double &crest) {
            epk_init(SR);
            epk_fx_set(EPK_FX_FLT, flt);
            epk_fx_set(EPK_FX_FLT_RESO, reso);
            epk_fx_set(EPK_FX_FLT_DOUBLE, dbl);
            epk_fx_set(EPK_FX_COMP, comp);
            epk_fx_set(EPK_FX_SAT, sat);
            epk_pattern_select(0, 8);                                // ECLIPSE: kick, hats, chord, perc
            epk_render(l, r, 1);
            epk_play(1);
            double e = 0.0, h = 0.0, w = 0.0;
            float prev = 0.0f;
            int bad = 0;
            std::vector<double> windows;                             // 256-sample loudness after the first second
            for (int k = 0; k < (int)(4.0f * SR / 256); k++) {
                epk_render(l, r, 256);
                w = 0.0;
                for (int i = 0; i < 256; i++) {
                    bad += !std::isfinite(l[i]);
                    e += (double)l[i] * l[i];
                    w += (double)l[i] * l[i];
                    h += (double)(l[i] - prev) * (l[i] - prev);
                    prev = l[i];
                }
                if (k > (int)(SR / 256)) windows.push_back(std::sqrt(w / 256.0));
            }
            const double n = 4.0 * SR;
            energy = e / n;
            bright = h / (e + 1e-12);
            std::sort(windows.begin(), windows.end());               // dynamics: loud windows against quiet ones
            crest = windows[windows.size() * 9 / 10] / (windows[windows.size() / 4] + 1e-9);
            return bad;
        };
        double e0, b0, c0, e1, b1, c1, e2, b2, c2, e3, b3, c3;
        run(0.5f, 0.3f, 0.0f, 0.0f, 0.0f, e0, b0, c0);               // flat (plano)
        run(0.15f, 0.3f, 0.0f, 0.0f, 0.0f, e1, b1, c1);              // low-pass closed
        run(0.85f, 0.3f, 0.0f, 0.0f, 0.0f, e2, b2, c2);              // high-pass open
        CHECK(b1 < b0 * 0.5 && b2 > b0 * 1.5 && e2 < e0 * 0.7, "filter: brightness %.4f flat, %.4f LP, %.4f HP; energy HP %.0f %%",
              b0, b1, b2, 100.0 * e2 / e0);
        epk_init(SR);                                                // the middle is transparent: same as before it
        run(0.5f, 1.0f, 1.0f, 0.0f, 0.0f, e3, b3, c3);
        CHECK(std::fabs(e3 / e0 - 1.0) < 0.05, "filter open (even with RESO and DOBLE up): energy %.1f %% of flat", 100.0 * e3 / e0);
        run(0.5f, 0.3f, 0.0f, 1.0f, 0.0f, e3, b3, c3);
        CHECK(c3 < c0 * 0.85 && epk_comp_reduction() > 3.0f, "compressor: loud / quiet %.2f against %.2f flat", c3, c0);
        const double c_comp = c3;
        int bad = run(0.05f, 1.0f, 1.0f, 1.0f, 1.0f, e3, b3, c3);
        bad += run(0.95f, 1.0f, 1.0f, 1.0f, 1.0f, e3, b3, c3);
        CHECK(bad == 0, "master at the extremes: %d bad samples", bad);
        std::printf("master: brightness %.4f flat, %.4f LP, %.4f HP; loud/quiet %.2f flat, %.2f compressed\n", b0, b1, b2, c0, c_comp);
        float hz = 0.0f;
        epk_fx_set(EPK_FX_FLT, 0.2f);
        CHECK(epk_master_filter(&hz) == 1 && hz > 100.0f && hz < 2000.0f, "filter for the screen: LP at %.0f Hz", hz);
    }

    // 18. Modulation (modulación): an LFO locked to the bar sweeps its destination over exactly its cycle; a follower
    // rises with the kick and falls between hits; a track's TONE and LEVEL can be destinations.
    {
        auto setdest = [&](int src, int slot, int dest, float amount) {   // amount -1..1
            epk_mod_set(src, slot == 0 ? EPK_M_DEST1 : EPK_M_DEST2, (float)dest / (float)(EPK_DEST_COUNT - 1));
            epk_mod_set(src, slot == 0 ? EPK_M_AMT1 : EPK_M_AMT2, 0.5f + 0.5f * amount);
        };
        epk_init(SR);
        epk_mod_set(0, EPK_M_SHAPE, 0.0f);                              // sine
        epk_mod_set(0, EPK_M_RATE, 4.0f / 9.0f);                        // 1 bar
        setdest(0, 0, EPK_DEST_FLT, 0.4f);
        setdest(1, 0, EPK_DEST_NONE, 0.0f);
        epk_pattern_select(0, 3);
        epk_render(l, r, 1);
        epk_play(1);
        const int bar = (int)(16.0 * 15.0 / 124.0 * SR);                // samples per bar at 124 BPM
        float lo = 1.0f, hi = 0.0f, at_quarter = 0.0f, at_three = 0.0f;
        for (int done = 0; done < bar; done += 64) {
            epk_render(l, r, 64);
            const float v = epk_fx_live(EPK_FX_FLT);
            lo = std::fmin(lo, v); hi = std::fmax(hi, v);
            if (std::abs(done - bar / 4) < 64) at_quarter = v;
            if (std::abs(done - 3 * bar / 4) < 64) at_three = v;
        }
        CHECK(lo < 0.13f && hi > 0.87f && epk_fx_get(EPK_FX_FLT) == 0.5f, "LFO on the filter: %.2f .. %.2f (knob 0.50)", lo, hi);
        CHECK(at_quarter > 0.85f && at_three < 0.15f, "LFO locked to the bar: %.2f at 1/4, %.2f at 3/4", at_quarter, at_three);

        epk_init(SR);                                                    // follower 1 on the kick, into track 3's TONE
        for (int i = 1; i < EPK_MOD_SOURCES; i++) setdest(i, 0, EPK_DEST_NONE, 0.0f);
        setdest(0, 0, EPK_DEST_NONE, 0.0f);
        epk_mod_set(2, EPK_M_SHAPE, 0.0f);
        epk_mod_set(2, EPK_M_RATE, 0.2f);
        setdest(2, 0, EPK_DEST_TONE1 + 2, -0.5f);
        setdest(2, 1, EPK_DEST_LEVEL1 + 4, 0.3f);
        epk_pattern_select(0, 3);
        epk_render(l, r, 1);
        const float tone = epk_param_get(2, EPK_P_TONE);
        epk_play(1);
        float f_hi = 0.0f, f_lo = 1.0f, t_lo = 1.0f;
        for (int done = 0; done < bar; done += 64) {
            epk_render(l, r, 64);
            const float o = epk_mod_out(2);
            if (done > bar / 8) { f_hi = std::fmax(f_hi, o); f_lo = std::fmin(f_lo, o); }
            t_lo = std::fmin(t_lo, epk_param_live(2, EPK_P_TONE));
        }
        CHECK(f_hi > 0.8f && f_lo < 0.2f, "follower: %.2f on a hit, %.2f between", f_hi, f_lo);
        CHECK(t_lo < tone - 0.3f && epk_param_get(2, EPK_P_TONE) == tone, "follower into TONE 3: down to %.2f from %.2f", t_lo, tone);
        CHECK(epk_param_live(4, EPK_P_LEVEL) >= epk_param_get(4, EPK_P_LEVEL), "second destination: LEVEL 5 up");
        std::printf("modulation: LFO %.2f..%.2f, follower %.2f..%.2f\n", lo, hi, f_lo, f_hi);
    }

    // 19. The filter's types (tipos de filtro): each one does what its name says on a full mix, MORPH goes from dark
    // to thin, and none blows up.
    {
        auto run = [&](int type, float cut, float reso, float dbl, double &energy, double &bright, double &sway) {
            epk_init(SR);
            epk_fx_set(EPK_FX_FLT_TYPE, (float)type / (float)(EPK_FLT_COUNT - 1));
            epk_fx_set(EPK_FX_FLT, cut);
            epk_fx_set(EPK_FX_FLT_RESO, reso);
            epk_fx_set(EPK_FX_FLT_DOUBLE, dbl);
            epk_fx_set(EPK_FX_COMP, 0.0f);
            epk_fx_set(EPK_FX_SAT, 0.0f);
            epk_pattern_select(0, 8);
            epk_render(l, r, 1);
            epk_play(1);
            double e = 0.0, h = 0.0, bmin = 1e9, bmax = 0.0;
            float prev = 0.0f;
            int bad = 0;
            for (int k = 0; k < 8; k++) {                            // eight half-second windows (ventanas)
                double we = 0.0, wh = 0.0;
                for (int b = 0; b < (int)(0.5f * SR / 256); b++) {
                    epk_render(l, r, 256);
                    for (int i = 0; i < 256; i++) {
                        bad += !std::isfinite(l[i]) || std::fabs(l[i]) > 1.0f;
                        we += (double)l[i] * l[i];
                        wh += (double)(l[i] - prev) * (l[i] - prev);
                        prev = l[i];
                    }
                }
                e += we; h += wh;
                const double wb = wh / (we + 1e-12);
                bmin = std::fmin(bmin, wb); bmax = std::fmax(bmax, wb);
            }
            energy = e; bright = h / (e + 1e-12); sway = bmax / (bmin + 1e-12);
            return bad;
        };
        double e[EPK_FLT_COUNT], b[EPK_FLT_COUNT], w[EPK_FLT_COUNT], ef, bf, wf;
        int bad = run(EPK_FLT_DJ, 0.5f, 0.3f, 0.0f, ef, bf, wf);    // flat (plano)
        for (int t = EPK_FLT_LP; t < EPK_FLT_COUNT; t++) bad += run(t, 0.55f, 0.3f, 0.5f, e[t], b[t], w[t]);
        CHECK(b[EPK_FLT_LP] < bf * 0.5 && b[EPK_FLT_HP] > bf * 1.5, "LP / HP at 1 kHz: brightness %.4f / %.4f, flat %.4f",
              b[EPK_FLT_LP], b[EPK_FLT_HP], bf);
        CHECK(e[EPK_FLT_BP] < ef * 0.6 && e[EPK_FLT_NOTCH] < ef && e[EPK_FLT_NOTCH] > e[EPK_FLT_BP],
              "BP keeps a band (%.0f %%), NOTCH takes one out (%.0f %%)", 100 * e[EPK_FLT_BP] / ef, 100 * e[EPK_FLT_NOTCH] / ef);
        double e0, b0, w0, e1, b1, w1;
        bad += run(EPK_FLT_MORPH, 0.55f, 0.3f, 0.0f, e0, b0, w0);
        bad += run(EPK_FLT_MORPH, 0.55f, 0.3f, 1.0f, e1, b1, w1);
        CHECK(b0 < b[EPK_FLT_MORPH] && b[EPK_FLT_MORPH] < b1, "MORPH: brightness %.4f (LP) < %.4f (BP) < %.4f (HP)", b0,
              b[EPK_FLT_MORPH], b1);
        bad += run(EPK_FLT_NOTCH, 0.0f, 1.0f, 1.0f, e1, b1, w1);
        bad += run(EPK_FLT_BP, 1.0f, 1.0f, 1.0f, e1, b1, w1);
        CHECK(bad == 0, "filter types: %d bad samples", bad);
        std::printf("filters: LP %.4f HP %.4f flat %.4f\n", b[EPK_FLT_LP], b[EPK_FLT_HP], bf);
    }

    // 20. MAREA (marea), the track insert: only its track changes; dry at MIX 0; with SPEED the colour moves over time
    // and still it holds; STEREO makes the sides differ; nothing blows up at the extremes.
    {
        // track 3 alone, a held saw, so the colour comes from MAREA and not from the notes
        auto run = [&](float on_track, float mix, float speed, float shape, float stereo, double &sway, double &side,
                       double &energy) {
            epk_init(SR);
            for (int i = 0; i < EPK_TRACKS; i++) epk_solo(i, i == 2);
            epk_param_set(2, EPK_P_SEND_A, 0.0f);
            epk_param_set(2, EPK_P_SEND_B, 0.0f);
            epk_param_set(2, EPK_P_CHARACTER, 0.5f);                 // saw (sierra)
            epk_param_set(2, EPK_P_TONE, 1.0f);
            epk_param_set(2, EPK_P_DECAY, 1.0f);
            epk_fx_set(EPK_FX_COMP, 0.0f);
            epk_fx_set(EPK_FX_SAT, 0.0f);
            epk_fx_set(EPK_FX_MAREA_TRACK, on_track / 6.0f);
            epk_fx_set(EPK_FX_MAREA_MIX, mix);
            epk_fx_set(EPK_FX_MAREA_SPEED, speed);
            epk_fx_set(EPK_FX_MAREA_SHAPE, shape);
            epk_fx_set(EPK_FX_MAREA_STEREO, stereo);
            epk_fx_set(EPK_FX_MAREA_DEPTH, 1.0f);
            for (int i = 0; i < 4; i++) epk_mod_set(i, EPK_M_AMT1, 0.5f), epk_mod_set(i, EPK_M_AMT2, 0.5f);
            epk_note_on(2, 45, 1.0f);
            double e = 0.0, d = 0.0, bmin = 1e9, bmax = 0.0;
            float prev = 0.0f;
            int bad = 0;
            for (int k = 0; k < 8; k++) {
                double we = 0.0, wh = 0.0;
                for (int b = 0; b < (int)(0.5f * SR / 256); b++) {
                    epk_render(l, r, 256);
                    for (int i = 0; i < 256; i++) {
                        bad += !std::isfinite(l[i]) || std::fabs(l[i]) > 1.0f;
                        we += (double)l[i] * l[i];
                        wh += (double)(l[i] - prev) * (l[i] - prev);
                        d += (double)(l[i] - r[i]) * (l[i] - r[i]);
                        prev = l[i];
                    }
                }
                e += we;
                const double wb = wh / (we + 1e-12);
                bmin = std::fmin(bmin, wb); bmax = std::fmax(bmax, wb);
            }
            sway = bmax / (bmin + 1e-12); side = d / (e + 1e-12); energy = e;
            return bad;
        };
        double sw0, sd0, e0, sw1, sd1, e1, sw2, sd2, e2, sw3, sd3, e3;
        int bad = run(0.0f, 1.0f, 0.5f, 0.5f, 0.0f, sw0, sd0, e0);   // MAREA elsewhere: the plain voice
        bad += run(3.0f, 0.0f, 0.5f, 0.5f, 0.0f, sw1, sd1, e1);      // on track 3, dry
        CHECK(std::fabs(e1 / e0 - 1.0) < 0.05, "MAREA at MIX 0 is dry: energy %.1f %% of the plain voice", 100 * e1 / e0);
        bad += run(3.0f, 1.0f, 0.0f, 0.5f, 0.0f, sw2, sd2, e2);      // still water
        bad += run(3.0f, 1.0f, 0.55f, 0.8f, 0.0f, sw3, sd3, e3);     // moving water
        CHECK(sw3 > sw2 * 1.3 && sw3 > 1.3, "MAREA: the colour sways x%.2f moving, x%.2f still", sw3, sw2);
        CHECK(e3 > e0 * 0.1 && e3 < e0 * 2.0, "MAREA wet keeps a sensible level: %.0f %% of dry", 100 * e3 / e0);
        double sw4, sd4, e4;
        bad += run(3.0f, 1.0f, 0.55f, 0.8f, 1.0f, sw4, sd4, e4);     // stereo
        CHECK(sd4 > sd3 * 5.0 && sd4 > 0.05, "MAREA stereo: sides differ %.3f, mono %.4f", sd4, sd3);
        double sw5, sd5, e5;
        bad += run(1.0f, 1.0f, 0.55f, 0.8f, 1.0f, sw5, sd5, e5);     // on track 1: track 3 untouched
        CHECK(std::fabs(e5 / e0 - 1.0) < 0.05, "MAREA on another track leaves track 3 alone (%.1f %%)", 100 * e5 / e0);
        bad += run(3.0f, 1.0f, 1.0f, 1.0f, 1.0f, sw5, sd5, e5);
        CHECK(bad == 0, "MAREA: %d bad samples", bad);
        float bl, br;
        epk_marea_band(7, &bl, &br);
        CHECK(bl >= 0.0f && bl <= 1.0f && br >= 0.0f && br <= 1.0f, "MAREA band levels for the screen: %.2f %.2f", bl, br);
        std::printf("marea: sway x%.2f moving, x%.2f still; stereo %.3f; wet %.0f %% of dry\n", sw3, sw2, sd4, 100 * e3 / e0);
    }

    // 11. Relative cost of the engines (coste relativo): not the FM-1's CPU, only how they compare
    {
        static const char *const kName[EPK_ENG_COUNT] = {"TEST", "GRAVE", "METAL", "GOLPE", "TAMBOR"};
        double base = 0.0;
        for (int e = 0; e < EPK_ENG_COUNT; e++) {
            epk_init(SR);
            for (int t = 0; t < EPK_TRACKS; t++) {
                epk_param_set(t, EPK_P_ENGINE, (float)e / (float)(EPK_ENG_COUNT - 1));
                epk_param_set(t, EPK_P_HITS, 0.5f);
            }
            epk_play(1);
            const auto t0 = std::chrono::steady_clock::now();
            for (int k = 0; k < (int)(20.0f * SR / 256); k++) epk_render(l, r, 256);
            const double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() /
                              (20.0 * SR) / EPK_TRACKS;
            if (e == 0) base = ns;
            std::printf("cost %-5s %6.1f ns per sample per voice on this machine, x%.2f of TEST\n", kName[e], ns, ns / base);
        }
    }

    std::printf(failures ? "dsp: %d FAILED\n" : "dsp: all passed\n", failures);
    return failures ? 1 : 0;
}
