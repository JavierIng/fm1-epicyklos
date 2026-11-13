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
    // and frozen it holds; reverse runs the wave the other way; WIDTH makes the sides differ; outside the area the
    // bands keep their level; INVERT at the middle flattens it; nothing blows up at the extremes.
    {
        // track 3 alone, a held saw, so the colour comes from MAREA and not from the notes
        auto run = [&](float on_track, float mix, float speed, float width, double &sway, double &side, double &energy) {
            epk_init(SR);
            for (int i = 0; i < EPK_TRACKS; i++) epk_solo(i, i == 2);
            epk_param_set(2, EPK_P_SEND_A, 0.0f);
            epk_param_set(2, EPK_P_SEND_B, 0.0f);
            epk_param_set(2, EPK_P_CHARACTER, 0.5f);                 // saw (sierra)
            epk_param_set(2, EPK_P_TONE, 1.0f);
            epk_param_set(2, EPK_P_DECAY, 1.0f);
            epk_param_set(2, EPK_P_PAN, 0.5f);
            epk_fx_set(EPK_FX_COMP, 0.0f);
            epk_fx_set(EPK_FX_SAT, 0.0f);
            epk_fx_set(EPK_FX_MAREA_TRACK, on_track / 6.0f);
            epk_fx_set(EPK_FX_MAREA_MIX, mix);
            epk_fx_set(EPK_FX_MAREA_SPEED, speed);
            epk_fx_set(EPK_FX_MAREA_WIDTH, width);
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
        int bad = run(0.0f, 1.0f, 0.8f, 0.8f, sw0, sd0, e0);        // MAREA elsewhere: the plain voice
        bad += run(3.0f, 0.0f, 0.8f, 0.8f, sw1, sd1, e1);           // on track 3, dry
        CHECK(std::fabs(e1 / e0 - 1.0) < 0.05, "MAREA at MIX 0 is dry: energy %.1f %% of the plain voice", 100 * e1 / e0);
        bad += run(3.0f, 1.0f, 0.5f, 0.0f, sw2, sd2, e2);           // frozen (congelada)
        bad += run(3.0f, 1.0f, 0.8f, 0.0f, sw3, sd3, e3);           // moving (en marcha)
        CHECK(sw3 > sw2 * 1.3 && sw3 > 1.3, "MAREA: the colour sways x%.2f moving, x%.2f frozen", sw3, sw2);
        CHECK(e3 > e0 * 0.3 && e3 < e0 * 2.0, "MAREA wet keeps a sensible level: %.0f %% of dry", 100 * e3 / e0);
        CHECK(sd3 < 1e-6, "MAREA at WIDTH 0 is mono (%.2g)", sd3);
        double sw4, sd4, e4;
        bad += run(3.0f, 1.0f, 0.8f, 1.0f, sw4, sd4, e4);           // wide (ancho)
        CHECK(sd4 > 0.05, "MAREA WIDTH: the sides differ %.3f", sd4);
        double sw5, sd5, e5;
        bad += run(1.0f, 1.0f, 0.8f, 1.0f, sw5, sd5, e5);           // on track 1: track 3 untouched
        CHECK(std::fabs(e5 / e0 - 1.0) < 0.05, "MAREA on another track leaves track 3 alone (%.1f %%)", 100 * e5 / e0);
        // the speed's sign is the direction; the middle stops it (parada)
        epk_fx_set(EPK_FX_MAREA_SPEED, 0.5f);
        epk_render(l, r, 1);
        const float stop = epk_marea_speed();
        epk_fx_set(EPK_FX_MAREA_SPEED, 0.2f);
        epk_render(l, r, 1);
        const float back = epk_marea_speed();
        epk_fx_set(EPK_FX_MAREA_SYNC, 1.0f);
        epk_fx_set(EPK_FX_MAREA_SPEED, 1.0f);
        epk_render(l, r, 1);
        const float synced = epk_marea_speed();                      // a quarter bar a cycle at 124 BPM
        CHECK(stop == 0.0f && back < 0.0f && std::fabs(synced - 124.0f / 60.0f) < 0.01f,
              "MAREA speed: %.3f in the middle, %.3f below it, %.3f Hz synced at the top", stop, back, synced);
        // INVERT in the middle flattens the wave; outside the area the bands stay at 1
        epk_init(SR);
        epk_fx_set(EPK_FX_MAREA_INVERT, 0.5f);
        epk_fx_set(EPK_FX_MAREA_FROM, 0.0f);
        epk_fx_set(EPK_FX_MAREA_TO, 1.0f);
        epk_fx_set(EPK_FX_MAREA_TRACK, 3.0f / 6.0f);
        for (int k = 0; k < 40; k++) epk_render(l, r, 256);
        float flat_dev = 0.0f;
        for (int b = 0; b < 16; b++) flat_dev = std::fmax(flat_dev, std::fabs(epk_marea_band(b) - 1.0f));
        epk_init(SR);
        epk_fx_set(EPK_FX_MAREA_INVERT, 1.0f);
        epk_fx_set(EPK_FX_MAREA_GRAVITY, 1.0f);
        epk_fx_set(EPK_FX_MAREA_FROM, 0.6f);                          // the area starts at ~1 kHz
        epk_fx_set(EPK_FX_MAREA_TO, 1.0f);
        epk_fx_set(EPK_FX_MAREA_TRACK, 3.0f / 6.0f);
        float low_dev = 0.0f, high_dev = 0.0f;
        for (int k = 0; k < 160; k++) {
            epk_render(l, r, 256);
            low_dev = std::fmax(low_dev, std::fabs(epk_marea_band(1) - 1.0f));
            high_dev = std::fmax(high_dev, std::fabs(epk_marea_band(13) - 1.0f));
        }
        CHECK(flat_dev < 0.01f, "MAREA INVERT in the middle: flat (%.3f)", flat_dev);
        CHECK(low_dev < 0.01f && high_dev > 0.3f, "MAREA area: a band below it stays (%.3f), one inside moves (%.3f)", low_dev,
              high_dev);
        double s6, d6, e6;
        epk_fx_set(EPK_FX_MAREA_GRAVITY, 1.0f);
        bad += run(3.0f, 1.0f, 1.0f, 1.0f, s6, d6, e6);
        bad += run(3.0f, 1.0f, 0.0f, 1.0f, s6, d6, e6);
        CHECK(bad == 0, "MAREA: %d bad samples", bad);
        std::printf("marea: sway x%.2f moving, x%.2f frozen; width %.3f; wet %.0f %% of dry\n", sw3, sw2, sd4, 100 * e3 / e0);
    }

    // 21. ACORDE DUB and DRONE: they sound; a chord is four notes (more pitch content than one); every table and
    // chord is clean; the drone holds past a second and fades after the sequencer stops.
    {
        auto play = [&](int track, float ch, float table, float seconds, double &energy, double &cross) {
            epk_init(SR);
            for (int i = 0; i < EPK_TRACKS; i++) epk_solo(i, i == track);
            epk_fx_set(EPK_FX_MAREA_TRACK, 0.0f);
            epk_param_set(track, EPK_P_SEND_A, 0.0f);
            epk_param_set(track, EPK_P_SEND_B, 0.0f);
            epk_param_set(track, EPK_P_CHARACTER, ch);
            epk_param_set(track, EPK_P_EXTRA1, table);
            epk_param_set(track, EPK_P_DECAY, 0.8f);
            epk_param_set(track, EPK_P_TONE, 0.7f);
            epk_param_set(track, EPK_P_ATTACK, 0.0f);
            epk_note_on(track, 50, 1.0f);
            double e = 0.0; long zc = 0; float prev = 0.0f; int bad = 0;
            for (int k = 0; k < (int)(seconds * SR / 256); k++) {
                epk_render(l, r, 256);
                for (int i = 0; i < 256; i++) {
                    bad += !std::isfinite(l[i]);
                    e += (double)l[i] * l[i];
                    zc += (l[i] > 0.0f) != (prev > 0.0f);
                    prev = l[i];
                }
            }
            energy = e; cross = (double)zc / seconds;
            return bad;
        };
        CHECK(epk_engine(2) == EPK_ENG_ACORDE && epk_engine(4) == EPK_ENG_DRONE, "engines: track 3 %d, track 5 %d",
              epk_engine(2), epk_engine(4));
        double e1, c1, e2, c2;
        int bad = play(2, 0.0f, 0.0f, 0.5f, e1, c1);                  // OCTAVA (Original, chord 0)
        bad += play(2, 0.5f, 0.0f, 0.5f, e2, c2);                    // MIN9 (Original, chord 5)
        CHECK(e1 > 10.0 && e2 > 10.0 && std::fabs(c2 - c1) > 50.0, "ACORDE sounds (%.0f, %.0f) and the chord changes it (%.0f / %.0f crossings a second)",
              e1, e2, c1, c2);
        for (int t = 0; t < epk_chord_tables(); t++)
            for (int c = 0; c < epk_chord_count(t); c++) {
                double e, x;
                bad += play(2, (float)c / (float)(epk_chord_count(t) - 1), (float)t / (float)(epk_chord_tables() - 1), 0.05f, e, x);
            }
        CHECK(bad == 0 && epk_chord_count(3) == 24 && epk_chord_name(0, 5)[0] != 0,
              "every chord of every table is clean (%d bad); %s has %d chords", bad, epk_chord_table_name(3), epk_chord_count(3));
        // the drone: still there after 2 s, then the sequencer starts and stops: it fades
        double ed, cd;
        bad = play(4, 0.4f, 0.4f, 2.0f, ed, cd);
        float late = 0.0f;
        for (int k = 0; k < 20; k++) { epk_render(l, r, 256); for (int i = 0; i < 256; i++) late = std::fmax(late, std::fabs(l[i])); }
        CHECK(bad == 0 && late > 0.05f, "DRONE holds: peak %.3f after 2 s", late);
        epk_play(1);
        epk_render(l, r, 256);
        epk_play(0);
        float tail = 0.0f;
        for (int k = 0; k < (int)(5.0f * SR / 256); k++) {
            epk_render(l, r, 256);
            if (k > (int)(4.0f * SR / 256)) for (int i = 0; i < 256; i++) tail = std::fmax(tail, std::fabs(l[i]));
        }
        CHECK(tail < 0.002f, "DRONE fades after stop: %.4f at 4-5 s", tail);
        std::printf("acorde: %.0f / %.0f crossings a second (octave / min9); drone peak %.3f at 2 s\n", c1, c2, late);
    }

    // 22. EVOLUTION (evolución): still without MUTATE; with it the rhythm and the sound change but stay near the
    //     pattern; RETURN brings it back; ABSENCE rests the track; a pattern change starts from the pattern again
    {
        const long bar = (long)std::lround(16.0 * 60.0 / 124.0 / 4.0 * SR);
        auto run_bars = [&](int bars) { for (long d = 0; d < bars * bar; d += 256) epk_render(l, r, 256); };
        auto quiet_all = [&]() { for (int t = 0; t < EPK_TRACKS; t++) epk_param_set(t, EPK_P_MUTATE, 0.0f); };
        // still (quieto): MUTATE 0, RETURN 0 everywhere: 16 bars, the same masks and tones
        epk_init(SR);
        epk_pattern_select(0, 0);
        epk_play(1);
        epk_render(l, r, 256);
        quiet_all();
        uint32_t m0[EPK_TRACKS];
        for (int t = 0; t < EPK_TRACKS; t++) m0[t] = epk_pattern(t);
        int moved = 0;
        for (int b = 0; b < 16; b++) {
            run_bars(1);
            for (int t = 0; t < EPK_TRACKS; t++) moved += epk_pattern(t) != m0[t];
        }
        CHECK(moved == 0, "MUTATE 0: the pattern moved %d times in 16 bars", moved);
        // moving (mueve): track 2 (hats) MUTATE 1 every bar: the mask and the tone change, the hits never reach 0
        epk_param_set(1, EPK_P_MUTATE, 1.0f);
        epk_param_set(1, EPK_P_EVERY, 0.0f);
        epk_param_set(1, EPK_P_RETURN, 0.2f);
        CHECK(epk_every_bars(1) == 1, "EVERY 0 = 1 bar (got %d)", epk_every_bars(1));
        int changes = 0, empty = 0, tone_moves = 0;
        uint32_t prev = epk_pattern(1);
        float tone0 = epk_param_live(1, EPK_P_TONE);
        for (int b = 0; b < 32; b++) {
            run_bars(1);
            changes += epk_pattern(1) != prev;
            prev = epk_pattern(1);
            empty += epk_pattern(1) == 0;
            tone_moves += std::fabs(epk_param_live(1, EPK_P_TONE) - tone0) > 0.02f;
        }
        CHECK(changes >= 8, "MUTATE 1: the hats' rhythm changed only %d times in 32 bars", changes);
        CHECK(empty == 0, "MUTATE never empties the track (%d empty bars)", empty);
        CHECK(tone_moves >= 8, "MUTATE 1: the tone moved only %d times", tone_moves);
        CHECK(epk_param_get(1, EPK_P_TONE) == epk_param_get(1, EPK_P_TONE), "the pattern's own value is untouched");
        // back (vuelta): MUTATE 0, RETURN 1: within a few bars it is the pattern again
        epk_param_set(1, EPK_P_MUTATE, 0.0f);
        epk_param_set(1, EPK_P_RETURN, 1.0f);
        run_bars(3);
        CHECK(epk_pattern(1) == m0[1], "RETURN 1: back to the pattern (mask %08x, expected %08x)", epk_pattern(1), m0[1]);
        CHECK(std::fabs(epk_param_live(1, EPK_P_TONE) - epk_param_get(1, EPK_P_TONE)) < 1e-4f, "RETURN 1: the tone back");
        // frozen (congelado): mutate a while, then MUTATE 0 and RETURN 0: it holds where it is
        epk_param_set(1, EPK_P_MUTATE, 1.0f);
        epk_param_set(1, EPK_P_RETURN, 0.0f);
        run_bars(6);
        epk_param_set(1, EPK_P_MUTATE, 0.0f);
        const uint32_t held = epk_pattern(1);
        const float held_tone = epk_param_live(1, EPK_P_TONE);
        run_bars(8);
        CHECK(epk_pattern(1) == held && std::fabs(epk_param_live(1, EPK_P_TONE) - held_tone) < 1e-6f,
              "MUTATE 0 + RETURN 0: frozen where it was");
        // a pattern change starts from the written pattern (desde el patrón escrito)
        epk_pattern_select(0, 0);
        run_bars(1);
        CHECK(epk_pattern(1) == m0[1], "after a pattern change the evolution starts again from the pattern");
        // absence (ausencia): ABSENCE 1 every bar: after the first bar, no triggers; ABSENCE 0: triggers
        epk_init(SR);
        epk_pattern_select(0, 0);
        epk_play(1);
        quiet_all();
        epk_param_set(1, EPK_P_EVERY, 0.0f);
        epk_param_set(1, EPK_P_ABSENCE, 1.0f);
        run_bars(1);
        uint32_t before = epk_triggers(1);
        run_bars(4);
        CHECK(epk_triggers(1) == before, "ABSENCE 1: %u triggers while resting", epk_triggers(1) - before);
        CHECK(epk_absent(1) == 1, "ABSENCE 1: reported as resting");
        epk_param_set(1, EPK_P_ABSENCE, 0.0f);
        run_bars(1);
        before = epk_triggers(1);
        run_bars(2);
        CHECK(epk_triggers(1) > before, "ABSENCE 0: the hats play again");
        // the factory patterns evolve by themselves: CYKLOS over 32 bars is not one loop
        epk_init(SR);
        epk_pattern_select(0, 15);
        epk_play(1);
        epk_render(l, r, 256);
        int differ = 0;
        uint32_t first[EPK_TRACKS];
        for (int t = 0; t < EPK_TRACKS; t++) first[t] = epk_pattern(t);
        for (int b = 0; b < 32; b++) {
            run_bars(1);
            for (int t = 0; t < EPK_TRACKS; t++) differ += epk_pattern(t) != first[t];
        }
        CHECK(differ > 10, "CYKLOS: only %d track-bars differ from the first bar in 32 bars", differ);
        CHECK(epk_pattern(0) == first[0], "the kick holds (el bombo no se mueve)");
        std::printf("evolution: %d rhythm changes in 32 bars at MUTATE 1; CYKLOS %d track-bars differ\n", changes, differ);
    }

    // 24. What evolves (qué evoluciona): SCOPE RHYTHM keeps the sound, SOUND keeps the rhythm; a track turned off
    //     goes back to its pattern and stays; HOLD freezes every track; in bank A the drums do not evolve
    {
        const long bar = (long)std::lround(16.0 * 60.0 / 124.0 / 4.0 * SR);
        auto run_bars = [&](int bars) { for (long d = 0; d < bars * bar; d += 256) epk_render(l, r, 256); };
        epk_init(SR);
        epk_pattern_select(0, 0);
        epk_play(1);
        epk_render(l, r, 256);
        for (int t = 0; t < EPK_TRACKS; t++) epk_param_set(t, EPK_P_MUTATE, 0.0f);
        const uint32_t m1 = epk_pattern(1);
        const float tone1 = epk_param_get(1, EPK_P_TONE);
        epk_param_set(1, EPK_P_MUTATE, 1.0f);
        epk_param_set(1, EPK_P_EVERY, 0.0f);
        epk_param_set(1, EPK_P_RETURN, 0.1f);
        epk_param_set(1, EPK_P_SCOPE, (float)EPK_SCOPE_RHYTHM / (EPK_SCOPE_COUNT - 1));
        int rh = 0, tn = 0;
        for (int b = 0; b < 16; b++) {
            run_bars(1);
            rh += epk_pattern(1) != m1;
            tn += std::fabs(epk_param_live(1, EPK_P_TONE) - tone1) > 1e-5f;
        }
        CHECK(rh >= 4 && tn == 0, "SCOPE RITMO: rhythm moved %d bars, tone %d (expected >=4 and 0)", rh, tn);
        epk_param_set(1, EPK_P_SCOPE, (float)EPK_SCOPE_SOUND / (EPK_SCOPE_COUNT - 1));
        run_bars(1);                                                 // out of scope goes back to the pattern
        rh = 0; tn = 0;
        for (int b = 0; b < 16; b++) {
            run_bars(1);
            rh += epk_pattern(1) != m1;
            tn += std::fabs(epk_param_live(1, EPK_P_TONE) - tone1) > 0.02f;
        }
        CHECK(rh == 0 && tn >= 4, "SCOPE SONIDO: rhythm moved %d bars (expected 0), tone %d", rh, tn);
        // off (apagada): next bar the pattern, and it stays; the knobs are kept
        epk_param_set(1, EPK_P_SCOPE, 0.0f);
        run_bars(4);
        epk_evolve_enable(1, 0);
        run_bars(1);
        int moved = 0;
        for (int b = 0; b < 8; b++) {
            run_bars(1);
            moved += epk_pattern(1) != m1 || std::fabs(epk_param_live(1, EPK_P_TONE) - tone1) > 1e-5f;
        }
        CHECK(!epk_evolve_enabled(1) && moved == 0, "evolution off: moved %d bars", moved);
        CHECK(epk_param_get(1, EPK_P_MUTATE) == 1.0f, "off keeps MUTATE as it was");
        epk_evolve_enable(1, 1);
        // hold (congelar todo): two tracks moving, then held: nothing changes for 8 bars; released, it moves again
        epk_param_set(2, EPK_P_MUTATE, 1.0f);
        epk_param_set(2, EPK_P_EVERY, 0.0f);
        run_bars(4);
        epk_evolve_hold(1);
        const uint32_t h1 = epk_pattern(1), h2 = epk_pattern(2);
        const float ht = epk_param_live(2, EPK_P_TONE);
        moved = 0;
        for (int b = 0; b < 8; b++) {
            run_bars(1);
            moved += epk_pattern(1) != h1 || epk_pattern(2) != h2 || epk_param_live(2, EPK_P_TONE) != ht;
        }
        CHECK(moved == 0, "HOLD: moved %d bars", moved);
        epk_evolve_hold(0);
        moved = 0;
        for (int b = 0; b < 8; b++) {
            run_bars(1);
            moved += epk_param_live(2, EPK_P_TONE) != ht;
        }
        CHECK(moved > 0, "released, it moves again");
        // bank A: the drums hold (la batería quieta), the synths move
        for (int pt = 0; pt < EPK_PATTERNS; pt++) {
            epk_init(SR);
            epk_pattern_select(0, pt);
            epk_render(l, r, 256);
            CHECK(epk_param_get(0, EPK_P_MUTATE) == 0.0f && epk_param_get(1, EPK_P_MUTATE) == 0.0f &&
                  epk_param_get(5, EPK_P_MUTATE) == 0.0f && epk_param_get(5, EPK_P_ABSENCE) == 0.0f,
                  "pattern %d: the drums evolve by default", pt + 1);
            CHECK(epk_param_get(2, EPK_P_MUTATE) > 0.0f, "pattern %d: the chord does not evolve by default", pt + 1);
        }
        // an object of version 1 (sin QUÉ) converts: same values, SCOPE TODO
        static uint8_t v1[EPK_BANK_HEAD + 8 * ((EPK_P_PATTERN_COUNT - 5) * 2 + 37) * EPK_TRACKS], v2[EPK_BANK_OBJ_BYTES];
        std::memset(v1, 0, sizeof v1);
        v1[0] = 'E'; v1[1] = 'P'; v1[2] = 'K'; v1[3] = 'B'; v1[4] = 1; v1[5] = 2; v1[6] = 0; v1[7] = 1;
        uint8_t *rec = v1 + EPK_BANK_HEAD;
        for (int t = 0; t < EPK_TRACKS; t++) {
            for (int q = 0; q < EPK_P_PATTERN_COUNT - 5; q++) { *rec++ = 0x00; *rec++ = 0x80; }   // 0.5 everywhere
            *rec++ = 0xFF; rec += 36;                                // mask byte 0, the rest zero
        }
        const int n2 = epk_bank_upgrade(v1, (int)sizeof v1, v2, (int)sizeof v2);
        CHECK(n2 == EPK_BANK_OBJ_BYTES && v2[4] == EPK_BANK_VERSION, "version 1 converts (%d bytes)", n2);
        epk_init(SR);
        epk_bank_stage_begin(2);
        epk_bank_stage_half(2, 0, v2, n2);
        epk_pattern_select(2, 0);
        epk_render(l, r, 256);
        CHECK(std::fabs(epk_param_get(3, EPK_P_TONE) - 0.5f) < 1e-3f && std::fabs(epk_param_get(3, EPK_P_ABSENCE) - 0.5f) < 1e-3f &&
              epk_param_get(3, EPK_P_SCOPE) == 0.0f, "converted values: tone %.3f absence %.3f scope %.3f",
              epk_param_get(3, EPK_P_TONE), epk_param_get(3, EPK_P_ABSENCE), epk_param_get(3, EPK_P_SCOPE));
    }

    // 25. The whole-pattern random (aleatorio general): many kicks, not mostly four on the floor; a new key each
    //     time unless locked; banks keep one key each, A in D
    {
        epk_init(SR);
        uint32_t seeds = 0x1F123BB5u;
        uint32_t kicks[64];
        int nk = 0, four = 0, keyseen = 0;
        for (int i = 0; i < 200; i++) {
            seeds ^= seeds << 13; seeds ^= seeds >> 17; seeds ^= seeds << 5;
            epk_pattern_randomize(seeds);
            epk_render(l, r, 256);
            const uint32_t m = epk_pattern(0);
            four += m == 0x1111u;
            int seen = 0;
            for (int k = 0; k < nk; k++) seen |= kicks[k] == m;
            if (!seen && nk < 64) kicks[nk++] = m;
            keyseen |= 1 << epk_key();
        }
        int nkeys = 0;
        for (int k = 0; k < 12; k++) nkeys += keyseen >> k & 1;
        CHECK(nk >= 12, "random: only %d different kicks in 200", nk);
        CHECK(four < 100, "random: four on the floor %d of 200 (at most half)", four);
        CHECK(nkeys >= 10, "random: only %d keys in 200", nkeys);
        epk_random_key_lock(1);
        const int k0 = epk_key();
        int moved = 0;
        for (int i = 0; i < 30; i++) {
            epk_pattern_randomize(0x1234567u * (uint32_t)(i + 1));
            epk_render(l, r, 256);
            moved += epk_key() != k0;
        }
        CHECK(moved == 0, "key locked: changed %d times", moved);
        epk_random_key_lock(0);
        epk_pattern_select(0, 3);
        epk_render(l, r, 256);
        CHECK(epk_key() == 2, "bank A is in D (got %d)", epk_key());
        std::printf("random: %d kicks, four on the floor %d/200, %d keys\n", nk, four, nkeys);
    }

    // 26. The track LFO (LFO de pista, página MOVIMIENTO): amount 0 changes nothing; a 1-bar sine on TONE peaks at a
    //     quarter bar; a negative amount turns it upside down; HITS and ROTATE move the rhythm; DRIFT wanders
    //     within its range without repeating; a version-2 save converts with the LFO still
    {
        const long bar = (long)std::lround(16.0 * 60.0 / 124.0 / 4.0 * SR);
        auto setup = [&]() {
            epk_init(SR);
            epk_pattern_select(0, 0);
            epk_render(l, r, 256);
            for (int t = 0; t < EPK_TRACKS; t++) { epk_param_set(t, EPK_P_LFO_AMT, 0.5f); epk_param_set(t, EPK_P_MUTATE, 0.0f); }
            epk_play(1);
        };
        setup();
        epk_param_set(2, EPK_P_TONE, 0.5f);
        float diff = 0.0f;
        for (long d = 0; d < 2 * bar; d += 256) {
            epk_render(l, r, 256);
            diff = std::fmax(diff, std::fabs(epk_param_live(2, EPK_P_TONE) - 0.5f));
        }
        CHECK(diff < 1e-6f, "amount 0: the tone moved %.4f", diff);
        // a 1-bar sine to TONE at +100: near 1.0 at a quarter bar, near 0.0 at three quarters
        setup();
        epk_param_set(2, EPK_P_TONE, 0.5f);
        epk_param_set(2, EPK_P_LFO_SHAPE, 0.0f);
        epk_param_set(2, EPK_P_LFO_RATE, 4.0f / 9.0f);               // 1 bar
        epk_param_set(2, EPK_P_LFO_DEST, (float)EPK_TLFO_TONE / (EPK_TLFO_DEST_COUNT - 1));
        epk_param_set(2, EPK_P_LFO_AMT, 1.0f);
        for (long d = 0; d < bar / 4; d += 256) epk_render(l, r, 256);
        const float q1 = epk_param_live(2, EPK_P_TONE);
        for (long d = 0; d < bar / 2; d += 256) epk_render(l, r, 256);
        const float q3 = epk_param_live(2, EPK_P_TONE);
        CHECK(q1 > 0.95f && q3 < 0.05f, "sine to TONE: %.3f at 1/4 bar, %.3f at 3/4 (expected ~1 and ~0)", q1, q3);
        epk_param_set(2, EPK_P_LFO_AMT, 0.0f);                       // -100: upside down (al revés)
        for (long d = 0; d < bar; d += 256) epk_render(l, r, 256);
        const float q3n = epk_param_live(2, EPK_P_TONE);
        CHECK(q3n > 0.95f, "amount -100 turns it over: %.3f at 3/4 bar", q3n);
        // HITS and ROTATE (golpes y rotación): the rhythm breathes
        setup();
        epk_param_set(1, EPK_P_LFO_RATE, 4.0f / 9.0f);
        epk_param_set(1, EPK_P_LFO_DEST, (float)EPK_TLFO_HITS / (EPK_TLFO_DEST_COUNT - 1));
        epk_param_set(1, EPK_P_LFO_AMT, 1.0f);
        uint32_t seen[16];
        int ns = 0;
        for (long d = 0; d < 2 * bar; d += 256) {
            epk_render(l, r, 256);
            const uint32_t m = epk_pattern(1);
            int k = 0;
            while (k < ns && seen[k] != m) k++;
            if (k == ns && ns < 16) seen[ns++] = m;
        }
        CHECK(ns >= 4, "LFO to HITS: only %d rhythms in 2 bars", ns);
        epk_param_set(1, EPK_P_LFO_DEST, (float)EPK_TLFO_ROTATE / (EPK_TLFO_DEST_COUNT - 1));
        ns = 0;
        for (long d = 0; d < 2 * bar; d += 256) {
            epk_render(l, r, 256);
            const uint32_t m = epk_pattern(1);
            int k = 0;
            while (k < ns && seen[k] != m) k++;
            if (k == ns && ns < 16) seen[ns++] = m;
        }
        CHECK(ns >= 4, "LFO to ROTATE: only %d rhythms in 2 bars", ns);
        // DRIFT (deriva): moves, stays inside, not the same over two cycles
        setup();
        epk_param_set(3, EPK_P_LFO_SHAPE, 1.0f);
        epk_param_set(3, EPK_P_LFO_RATE, 4.0f / 9.0f);
        epk_param_set(3, EPK_P_LFO_AMT, 1.0f);
        float lo = 9.0f, hi = -9.0f, a1[16], a2[16];
        for (int c = 0; c < 2; c++)
            for (int k = 0; k < 16; k++) {
                for (long d = 0; d < bar / 16; d += 256) epk_render(l, r, 256);
                const float v = epk_track_lfo(3);
                (c ? a2 : a1)[k] = v;
                lo = std::fmin(lo, v); hi = std::fmax(hi, v);
            }
        float same = 0.0f;
        for (int k = 0; k < 16; k++) same += std::fabs(a1[k] - a2[k]);
        CHECK(lo >= -1.0f && hi <= 1.0f && hi - lo > 0.3f, "DRIFT: range %.2f..%.2f", lo, hi);
        CHECK(same > 0.5f, "DRIFT repeats itself every cycle (diff %.3f)", same);
        // a version-2 save (sin LFO): converts, the LFO still (amount 0), SCOPE kept
        static uint8_t v2[EPK_BANK_HEAD + 8 * ((EPK_P_PATTERN_COUNT - 4) * 2 + 37) * EPK_TRACKS], v3[EPK_BANK_OBJ_BYTES];
        std::memset(v2, 0, sizeof v2);
        v2[0] = 'E'; v2[1] = 'P'; v2[2] = 'K'; v2[3] = 'B'; v2[4] = 2; v2[5] = 5; v2[6] = 0; v2[7] = 1;
        uint8_t *rec = v2 + EPK_BANK_HEAD;
        for (int t = 0; t < EPK_TRACKS; t++) {
            for (int q = 0; q < EPK_P_PATTERN_COUNT - 4; q++) {
                const uint32_t v = q == EPK_P_SCOPE ? 65535u : 21845u;  // SCOPE NOTE (1.0), the rest 1/3
                *rec++ = (uint8_t)v; *rec++ = (uint8_t)(v >> 8);
            }
            rec += 37;
        }
        const int n3 = epk_bank_upgrade(v2, (int)sizeof v2, v3, (int)sizeof v3);
        CHECK(n3 == EPK_BANK_OBJ_BYTES, "version 2 converts (%d bytes)", n3);
        epk_init(SR);
        epk_bank_stage_begin(5);
        epk_bank_stage_half(5, 0, v3, n3);
        epk_pattern_select(5, 0);
        epk_render(l, r, 256);
        CHECK(epk_param_get(2, EPK_P_LFO_AMT) > 0.49f && epk_param_get(2, EPK_P_LFO_AMT) < 0.51f &&
              epk_param_get(2, EPK_P_SCOPE) > 0.99f && std::fabs(epk_param_get(2, EPK_P_TONE) - 1.0f / 3.0f) < 1e-3f,
              "version 2: LFO amount %.3f, scope %.3f, tone %.3f", epk_param_get(2, EPK_P_LFO_AMT),
              epk_param_get(2, EPK_P_SCOPE), epk_param_get(2, EPK_P_TONE));
        std::printf("track LFO: sine %.2f/%.2f, drift %.2f..%.2f\n", q1, q3, lo, hi);
    }

    // 27. FLOW: a block off is not heard (and not computed); the filter's place; which of delay and reverb feeds the
    //     other; compressor and tape in either order; MAREA off sounds as no MAREA
    {
        auto run = [&](float sec, double *energy) {                  // render, energy of the last half (energía)
            const long total = (long)(sec * SR);
            double e = 0.0;
            for (long d = 0; d < total; d += 256) {
                epk_render(l, r, 256);
                if (d > total / 2) for (int i = 0; i < 256; i++) e += (double)l[i] * l[i] + (double)r[i] * r[i];
            }
            if (energy) *energy = e;
        };
        auto fresh = [&]() {
            epk_init(SR);
            epk_pattern_select(0, 3);
            epk_render(l, r, 256);
            for (int t = 0; t < EPK_TRACKS; t++) { epk_param_set(t, EPK_P_MUTATE, 0.0f); epk_param_set(t, EPK_P_LFO_AMT, 0.5f); }
            epk_play(1);
        };
        // delay and reverb off: no return at all (sin retorno)
        fresh();
        for (int t = 0; t < EPK_TRACKS; t++) { epk_param_set(t, EPK_P_SEND_A, 0.8f); epk_param_set(t, EPK_P_SEND_B, 0.8f); }
        run(1.0f, nullptr);
        epk_fx_meter();
        run(0.5f, nullptr);
        const float with_fx = epk_fx_meter();
        epk_fx_set(EPK_FX_ON_DLY, 0.0f);
        epk_fx_set(EPK_FX_ON_REV, 0.0f);
        run(0.1f, nullptr);
        epk_fx_meter();
        run(0.5f, nullptr);
        const float without = epk_fx_meter();
        CHECK(with_fx > 0.01f && without == 0.0f, "ECO and REVERB off: return %.4f (on: %.4f)", without, with_fx);
        epk_fx_set(EPK_FX_ON_DLY, 1.0f);
        run(0.5f, nullptr);
        epk_fx_meter();
        run(0.5f, nullptr);
        CHECK(epk_fx_meter() > 0.01f, "ECO back on: it returns again");
        // the filter's place (dónde va el filtro): a closed low-pass on everything is darker than on the returns only
        double e_all, e_fx, e_dry;
        fresh();
        epk_fx_set(EPK_FX_FLT_TYPE, (float)EPK_FLT_LP / (EPK_FLT_COUNT - 1));
        epk_fx_set(EPK_FX_FLT, 0.05f);
        run(2.0f, &e_all);
        fresh();
        epk_fx_set(EPK_FX_FLT_TYPE, (float)EPK_FLT_LP / (EPK_FLT_COUNT - 1));
        epk_fx_set(EPK_FX_FLT, 0.05f);
        epk_fx_set(EPK_FX_FLT_PLACE, 1.0f);                         // on the returns only (en efectos)
        run(2.0f, &e_fx);
        fresh();
        epk_fx_set(EPK_FX_FLT_TYPE, (float)EPK_FLT_LP / (EPK_FLT_COUNT - 1));
        epk_fx_set(EPK_FX_FLT, 0.05f);
        epk_fx_set(EPK_FX_FLT_PLACE, 0.5f);                         // on the tracks (seco), the returns off
        epk_fx_set(EPK_FX_ON_DLY, 0.0f);
        epk_fx_set(EPK_FX_ON_REV, 0.0f);
        run(2.0f, &e_dry);
        CHECK(e_fx > 2.0 * e_all, "filter on the returns: the tracks pass open (%.1f vs %.1f on everything)", e_fx, e_all);
        CHECK(e_dry < 1.6 * e_all && e_dry > 0.3 * e_all, "filter on the tracks with no returns ~ on everything (%.2f)",
              e_dry / e_all);
        // the space's order (orden del espacio): only bus B fed, reverb return at 0, cross at 1: the delay sounds
        // only when the reverb feeds it
        double e_dr, e_rd;
        for (int order = 0; order < 2; order++) {
            fresh();
            for (int t = 0; t < EPK_TRACKS; t++) { epk_param_set(t, EPK_P_SEND_A, 0.0f); epk_param_set(t, EPK_P_SEND_B, 0.9f);
                                                   epk_param_set(t, EPK_P_LEVEL, 0.0f); }
            epk_fx_set(EPK_FX_REV_LEVEL, 0.0f);
            epk_fx_set(EPK_FX_DLY_TO_REV, 1.0f);
            epk_fx_set(EPK_FX_SPACE_ORDER, (float)order);
            run(2.0f, order ? &e_rd : &e_dr);
        }
        CHECK(e_dr < 1e-6 && e_rd > 1e-3, "REVERB > ECO feeds the delay (%.6f) and ECO > REVERB does not (%.6f)", e_rd, e_dr);
        // drive order: different (distinto) with both on, the same with the compressor off
        double e_cs, e_sc;
        for (int order = 0; order < 2; order++) {
            fresh();
            epk_fx_set(EPK_FX_COMP, 0.9f);
            epk_fx_set(EPK_FX_SAT, 0.9f);
            epk_fx_set(EPK_FX_DRIVE_ORDER, (float)order);
            run(1.5f, order ? &e_sc : &e_cs);
        }
        CHECK(std::fabs(e_cs - e_sc) > 0.01 * e_cs, "COMP>SAT and SAT>COMP sound the same (%.2f / %.2f)", e_cs, e_sc);
        // MAREA off sounds as no MAREA at all (track 3 sending nothing, so the tails match)
        static float ref[22050];
        fresh();
        for (int t = 0; t < EPK_TRACKS; t++) { epk_param_set(t, EPK_P_SEND_A, 0.0f); epk_param_set(t, EPK_P_SEND_B, 0.0f); }
        epk_fx_set(EPK_FX_MAREA_TRACK, 0.0f);
        run(1.0f, nullptr);
        for (int k = 0; k < 22050; k += 256) { epk_render(l, r, 256); for (int i = 0; i < 256 && k + i < 22050; i++) ref[k + i] = l[i]; }
        fresh();
        for (int t = 0; t < EPK_TRACKS; t++) { epk_param_set(t, EPK_P_SEND_A, 0.0f); epk_param_set(t, EPK_P_SEND_B, 0.0f); }
        epk_fx_set(EPK_FX_ON_MAREA, 0.0f);
        run(1.0f, nullptr);
        float worst = 0.0f;
        for (int k = 0; k < 22050; k += 256) {
            epk_render(l, r, 256);
            for (int i = 0; i < 256 && k + i < 22050; i++) worst = std::fmax(worst, std::fabs(l[i] - ref[k + i]));
        }
        CHECK(worst < 1e-3f, "MAREA off differs from no MAREA by %.5f", worst);
        std::printf("FLOW: returns %.3f -> %.3f off; filter on returns x%.1f; REV>ECO %.4f; MAREA off diff %.6f\n", with_fx,
                    without, e_fx / e_all, e_rd, worst);
    }

    // 28. Pre-fader sends, DIGITAL (bits, rate) and SCENES (escenas)
    {
        const long bar = (long)std::lround(16.0 * 60.0 / 124.0 / 4.0 * SR);
        auto fresh = [&]() {
            epk_init(SR);
            epk_pattern_select(0, 3);
            epk_render(l, r, 256);
            for (int t = 0; t < EPK_TRACKS; t++) { epk_param_set(t, EPK_P_MUTATE, 0.0f); epk_param_set(t, EPK_P_LFO_AMT, 0.5f); }
            epk_play(1);
        };
        auto fx_peak = [&](float sec) {
            epk_fx_meter();
            for (long d = 0; d < (long)(sec * SR); d += 256) epk_render(l, r, 256);
            return epk_fx_meter();
        };
        // pre-fader: the level at 0 and the send still feeds the delay; post-fader it does not
        float post, pre;
        fresh();
        for (int t = 0; t < EPK_TRACKS; t++) { epk_param_set(t, EPK_P_SEND_A, 0.8f); epk_param_set(t, EPK_P_SEND_B, 0.0f);
                                               epk_param_set(t, EPK_P_LEVEL, 0.0f); }
        fx_peak(6.0f);                                               // what was sent before dies out (se apaga)
        post = fx_peak(1.0f);
        for (int t = 0; t < EPK_TRACKS; t++) epk_param_set(t, EPK_P_SEND_PRE, 1.0f);
        fx_peak(1.0f);
        pre = fx_peak(1.0f);
        CHECK(post < 1e-4f && pre > 0.02f, "pre-fader: level 0 sends %.4f (post-fader %.4f)", pre, post);
        // bits at the top: few distinct levels; rate at the top: each sample held for many
        fresh();
        epk_fx_set(EPK_FX_BITS, 1.0f);
        epk_fx_set(EPK_FX_SRR, 1.0f);
        for (long d = 0; d < SR; d += 256) epk_render(l, r, 256);
        int held = 0, changes = 0;
        for (int k = 0; k < 40; k++) {
            epk_render(l, r, 256);
            for (int i = 1; i < 256; i++) { if (l[i] == l[i - 1]) held++; else changes++; }
        }
        CHECK(held > 20 * changes, "rate /32: samples held %d, changes %d", held, changes);
        // scenes: SECO (5) held takes both returns to 0 at once; let go, back; the knobs untouched
        fresh();
        const float dly = epk_fx_get(EPK_FX_DLY_LEVEL);
        epk_scene_hold(5, 1);
        epk_render(l, r, 256);
        CHECK(epk_fx_live(EPK_FX_DLY_LEVEL) == 0.0f && epk_fx_live(EPK_FX_REV_LEVEL) == 0.0f, "SECO held: returns %.2f %.2f",
              epk_fx_live(EPK_FX_DLY_LEVEL), epk_fx_live(EPK_FX_REV_LEVEL));
        CHECK(epk_fx_get(EPK_FX_DLY_LEVEL) == dly, "a scene does not move the knob");
        epk_scene_hold(5, 0);
        epk_render(l, r, 256);
        CHECK(std::fabs(epk_fx_live(EPK_FX_DLY_LEVEL) - dly) < 1e-6f, "SECO let go: back to %.2f", dly);
        // SUMERGIR (0) fades in over 1 bar: about halfway at half a bar, all in at one
        epk_scene_hold(0, 1);
        for (long d = 0; d < bar / 2; d += 256) epk_render(l, r, 256);
        const float half = epk_scene_amount(0);
        for (long d = 0; d < bar / 2 + 512; d += 256) epk_render(l, r, 256);
        CHECK(half > 0.4f && half < 0.6f && epk_scene_amount(0) == 1.0f, "fade of 1 bar: %.2f at half, %.2f at one", half,
              epk_scene_amount(0));
        CHECK(std::fabs(epk_fx_live(EPK_FX_FLT) - 0.12f) < 1e-4f, "SUMERGIR in: filter %.3f", epk_fx_live(EPK_FX_FLT));
        epk_scene_hold(0, 0);
        // a track's parameter: ECO INFINITO (7) sets every track's send A to 0
        epk_scene_latch(7, 1);
        epk_render(l, r, 256);
        int zero = 0;
        for (int t = 0; t < EPK_TRACKS; t++) zero += epk_param_live(t, EPK_P_SEND_A) == 0.0f;
        CHECK(zero == EPK_TRACKS && epk_scene_latched(7), "ECO INFINITO latched: %d tracks' send A at 0", zero);
        epk_scene_latch(7, 0);
        // making one: 8 targets at most; empty it
        epk_scene_clear(2);
        int ok = 0;
        for (int i = 0; i < 9; i++) ok += epk_scene_lock(2, EPK_SCENE_TRACK + (i % 6) * 32 + (i < 6 ? EPK_P_TONE : EPK_P_DECAY), 0.3f) >= 0;
        CHECK(ok == 8 && epk_scene_count(2) == 8, "a scene holds 8 targets (took %d)", ok);
        CHECK(epk_scene_lock(2, EPK_SCENE_TRACK + 0 * 32 + EPK_P_TONE, 0.9f) >= 0 && epk_scene_value(2, 0) > 0.89f,
              "changing a target already there works when full");
        epk_scene_clear(2);
        CHECK(epk_scene_count(2) == 0, "cleared");
        std::printf("pre-fader send %.3f; fade half %.2f\n", pre, half);
    }

    // 29. NUBE (nube de granos), bus B's second type: it sounds; freeze keeps it going with nothing new in; density
    //     at the centre makes no new grains; extremes stay finite; back to PLACA works
    {
        auto fresh = [&]() {
            epk_init(SR);
            epk_pattern_select(0, 3);
            epk_render(l, r, 256);
            for (int t = 0; t < EPK_TRACKS; t++) {
                epk_param_set(t, EPK_P_MUTATE, 0.0f); epk_param_set(t, EPK_P_LFO_AMT, 0.5f);
                epk_param_set(t, EPK_P_SEND_A, 0.0f); epk_param_set(t, EPK_P_SEND_B, 0.7f);
            }
            epk_fx_set(EPK_FX_REV_TYPE, 1.0f);
            epk_fx_set(EPK_FX_REV_LEVEL, 1.0f);
            epk_play(1);
        };
        auto fx_peak = [&](float sec) {
            epk_fx_meter();
            for (long d = 0; d < (long)(sec * SR); d += 256) epk_render(l, r, 256);
            return epk_fx_meter();
        };
        fresh();
        const float on = fx_peak(3.0f);
        CHECK(on > 0.02f, "NUBE sounds (returns peak %.3f)", on);
        // freeze, then silence the sends: the cloud keeps going from what it holds
        epk_freeze(1);
        for (int t = 0; t < EPK_TRACKS; t++) epk_param_set(t, EPK_P_SEND_B, 0.0f);
        fx_peak(0.5f);
        const float frozen = fx_peak(2.0f);
        CHECK(frozen > 0.01f, "frozen, nothing in: still grains (peak %.3f)", frozen);
        // density at the centre: no new grains; after the last ones end, silence (its own reverb off for this)
        epk_fx_set(EPK_FX_NUBE_VERB, 0.0f);
        epk_fx_set(EPK_FX_NUBE_DENSITY, 0.5f);
        fx_peak(1.0f);
        const float none = fx_peak(1.0f);
        CHECK(none < 1e-4f, "density at the centre: silent (peak %.6f)", none);
        epk_freeze(0);
        // extremes (extremos): everything up, pitch at both ends, long grains; finite, not runaway
        fresh();
        epk_fx_set(EPK_FX_REV_DECAY, 1.0f);
        epk_fx_set(EPK_FX_NUBE_SIZE, 1.0f);
        epk_fx_set(EPK_FX_NUBE_DENSITY, 1.0f);
        int bad = 0;
        float peak = 0.0f;
        for (int k = 0; k < 2; k++) {
            epk_fx_set(EPK_FX_NUBE_PITCH, (float)k);
            epk_fx_set(EPK_FX_REV_TONE, (float)k);
            for (long d = 0; d < (long)(4.0f * SR); d += 256) {
                epk_render(l, r, 256);
                for (int i = 0; i < 256; i++) {
                    bad += !std::isfinite(l[i]) || !std::isfinite(r[i]);
                    peak = std::fmax(peak, std::fabs(l[i]));
                }
            }
        }
        CHECK(bad == 0 && peak <= 1.0f, "NUBE at the extremes: %d bad samples, peak %.3f", bad, peak);
        // the plate again (otra vez la placa)
        epk_fx_set(EPK_FX_REV_TYPE, 0.0f);
        epk_fx_set(EPK_FX_REV_DECAY, 0.7f);
        const float plate = fx_peak(2.0f);
        CHECK(plate > 0.02f, "back to PLACA it sounds (peak %.3f)", plate);
        // its own reverb (su reverb): no new grains, the cloud's tail lasts longer with it than without
        auto cloud_tail = [&](float verb) {
            fresh();
            epk_fx_set(EPK_FX_NUBE_VERB, verb);
            epk_fx_set(EPK_FX_NUBE_VERB_TIME, 0.9f);
            fx_peak(3.0f);
            epk_fx_set(EPK_FX_NUBE_DENSITY, 0.5f);
            for (int t = 0; t < EPK_TRACKS; t++) epk_param_set(t, EPK_P_SEND_B, 0.0f);
            fx_peak(1.2f);                                           // the last grains end (acaban)
            return fx_peak(0.5f);
        };
        const float dry_tail = cloud_tail(0.0f), wet_tail = cloud_tail(0.8f);
        CHECK(dry_tail < 1e-4f && wet_tail > 0.005f, "NUBE's reverb rings after the grains (%.5f without, %.4f with)",
              dry_tail, wet_tail);
        std::printf("nube peak %.3f, frozen %.3f, plate after %.3f; cloud tail %.4f\n", on, frozen, plate, wet_tail);
    }

    // 30. PLACA the way Plateau plays it: pre-delay holds the tail back; freeze keeps it with nothing coming in;
    //     size moved while it plays and every extreme stay finite
    {
        // only the returns: every level at 0, the B sends pre-fader (solo retornos)
        auto fresh = [&](float predelay) {
            epk_init(SR);
            epk_pattern_select(0, 3);
            epk_render(l, r, 256);
            for (int t = 0; t < EPK_TRACKS; t++) {
                epk_param_set(t, EPK_P_MUTATE, 0.0f); epk_param_set(t, EPK_P_LFO_AMT, 0.5f);
                epk_param_set(t, EPK_P_LEVEL, 0.0f); epk_param_set(t, EPK_P_SEND_PRE, 1.0f);
                epk_param_set(t, EPK_P_SEND_A, 0.0f); epk_param_set(t, EPK_P_SEND_B, t == 0 ? 0.9f : 0.0f);
            }
            epk_fx_set(EPK_FX_REV_PREDELAY, predelay);
            epk_fx_set(EPK_FX_REV_LEVEL, 1.0f);
            for (long d = 0; d < (long)(4.0f * SR); d += 256) epk_render(l, r, 256);   // levels glide to 0, tails die
            epk_play(1);
        };
        auto onset = [&]() {                                         // first sample of the tail (primera muestra)
            for (long d = 0; d < (long)(1.0f * SR); d += 256) {
                epk_render(l, r, 256);
                for (int i = 0; i < 256; i++)
                    if (std::fabs(l[i]) + std::fabs(r[i]) > 1e-3f) return (float)(d + i) / SR;
            }
            return 1.0f;
        };
        fresh(0.0f);
        const float t0 = onset();
        fresh(1.0f);
        const float t1 = onset();
        CHECK(t1 - t0 > 0.45f && t1 - t0 < 0.55f, "pre-delay at the top holds the tail ~500 ms (%.0f ms)",
              (t1 - t0) * 1000.0f);
        // freeze: the kick's send off, the tail stays; without freeze it dies
        float before = 0.0f;
        auto tail = [&](int frz) {
            fresh(0.0f);
            epk_fx_set(EPK_FX_REV_DECAY, 0.3f);
            for (long d = 0; d < (long)(1.0f * SR); d += 256) epk_render(l, r, 256);
            epk_fx_meter();
            for (long d = 0; d < (long)(1.0f * SR); d += 256) epk_render(l, r, 256);
            before = epk_fx_meter();
            epk_freeze(frz);
            epk_param_set(0, EPK_P_SEND_B, 0.0f);
            for (long d = 0; d < (long)(4.0f * SR); d += 256) epk_render(l, r, 256);
            epk_fx_meter();
            for (long d = 0; d < (long)(1.0f * SR); d += 256) epk_render(l, r, 256);
            const float pk = epk_fx_meter();
            epk_freeze(0);
            return pk;
        };
        const float held = tail(1);
        const float held_before = before;
        const float gone = tail(0);
        CHECK(held < held_before * 1.5f, "frozen plate does not grow (%.3f, before %.3f)", held, held_before);
        CHECK(held > 0.02f && gone < held * 0.05f, "frozen plate holds (%.3f), free one dies (%.5f)", held, gone);
        // size swept while playing, diffusion and pre-delay at both ends: finite, no runaway
        fresh(0.5f);
        epk_param_set(0, EPK_P_SEND_B, 1.0f);
        epk_fx_set(EPK_FX_REV_DECAY, 1.0f);
        int bad = 0;
        float peak = 0.0f;
        for (int k = 0; k < 40; k++) {
            epk_fx_set(EPK_FX_REV_SIZE, (k % 2) ? (float)(k % 7) / 6.0f : 1.0f - (float)(k % 5) / 4.0f);
            epk_fx_set(EPK_FX_REV_DIFFUSION, (float)(k % 3) / 2.0f);
            epk_fx_set(EPK_FX_REV_PREDELAY, (float)(k % 4) / 3.0f);
            for (long d = 0; d < (long)(0.25f * SR); d += 256) {
                epk_render(l, r, 256);
                for (int i = 0; i < 256; i++) {
                    bad += !std::isfinite(l[i]) || !std::isfinite(r[i]);
                    peak = std::fmax(peak, std::fabs(l[i]));
                }
            }
        }
        CHECK(bad == 0 && peak <= 1.0f, "PLACA swept: %d bad samples, peak %.3f", bad, peak);
        // shimmer (+12): the tail gets brighter (more zero crossings); flat out with decay up it stays bounded
        auto bright = [&](float shim) {
            fresh(0.0f);
            epk_fx_set(EPK_FX_REV_DECAY, 0.85f);
            epk_fx_set(EPK_FX_REV_SHIMMER, shim);
            for (long d = 0; d < (long)(3.0f * SR); d += 256) epk_render(l, r, 256);
            long zc = 0, n = 0;
            float prev = 0.0f;
            for (long d = 0; d < (long)(2.0f * SR); d += 256) {
                epk_render(l, r, 256);
                for (int i = 0; i < 256; i++) { zc += (l[i] > 0.0f) != (prev > 0.0f); prev = l[i]; n++; }
            }
            return (float)zc / ((float)n / SR) * 0.5f;              // a rough "frequency" (Hz)
        };
        const float plain = bright(0.0f), shimmered = bright(1.0f);
        CHECK(shimmered > plain * 1.3f, "shimmer brightens the tail (%.0f -> %.0f Hz)", plain, shimmered);
        fresh(0.0f);
        epk_param_set(0, EPK_P_SEND_B, 1.0f);
        epk_fx_set(EPK_FX_REV_DECAY, 1.0f);
        epk_fx_set(EPK_FX_REV_SHIMMER, 1.0f);
        epk_fx_set(EPK_FX_REV_SIZE, 1.0f);
        bad = 0; peak = 0.0f;
        for (long d = 0; d < (long)(12.0f * SR); d += 256) {
            epk_render(l, r, 256);
            for (int i = 0; i < 256; i++) { bad += !std::isfinite(l[i]); peak = std::fmax(peak, std::fabs(l[i])); }
        }
        CHECK(bad == 0 && peak <= 1.0f, "shimmer flat out, 12 s: %d bad, peak %.3f", bad, peak);
        // width 0: mono (l == r) from the reverb
        fresh(0.0f);
        epk_fx_set(EPK_FX_REV_WIDTH, 0.0f);
        for (long d = 0; d < (long)(2.0f * SR); d += 256) epk_render(l, r, 256);
        float side = 0.0f;
        for (int i = 0; i < 256; i++) side = std::fmax(side, std::fabs(l[i] - r[i]));
        CHECK(side < 1e-4f, "width 0: mono returns (side %.6f)", side);
        std::printf("plate pre-delay %.0f ms; frozen %.3f, free %.5f; shimmer %.0f -> %.0f Hz\n", (t1 - t0) * 1000.0f,
                    held, gone, plain, shimmered);
    }

    // 23. Saving (guardar): a pattern as it sounds, packed, staged into another bank's slot, loads back the same;
    //     its name gets '*'; a slot kept in the current bank is the pattern from then on
    {
        static uint8_t obj[EPK_BANK_OBJ_BYTES];
        epk_init(SR);
        epk_pattern_select(0, 15);
        epk_play(1);
        epk_param_set(2, EPK_P_MUTATE, 1.0f);
        epk_param_set(2, EPK_P_EVERY, 0.0f);
        epk_param_set(2, EPK_P_RETURN, 0.0f);
        epk_render(l, r, 256);                                       // the pattern in first (primero el patrón)
        for (int t = 0; t < EPK_TRACKS; t++) epk_param_set(t, EPK_P_LFO_AMT, 0.5f);   // live = saved (sin LFO)
        const long bar = (long)std::lround(16.0 * 60.0 / 124.0 / 4.0 * SR);
        for (long d = 0; d < 6 * bar; d += 256) epk_render(l, r, 256);
        float live[EPK_TRACKS][3];
        uint32_t mask[EPK_TRACKS];
        for (int t = 0; t < EPK_TRACKS; t++) {
            live[t][0] = epk_param_live(t, EPK_P_TONE);
            live[t][1] = epk_param_live(t, EPK_P_NOTE);
            live[t][2] = epk_param_live(t, EPK_P_CHARACTER);
            mask[t] = epk_pattern(t);
        }
        std::memset(obj, 0, sizeof obj);
        obj[0] = 'E'; obj[1] = 'P'; obj[2] = 'K'; obj[3] = 'B'; obj[4] = EPK_BANK_VERSION;
        obj[5] = 3; obj[6] = 1; obj[7] = 1u << 4;                    // bank D, second half, slot 13 (index 12)
        epk_pattern_pack_live(obj + EPK_BANK_HEAD + 4 * EPK_PATTERN_BYTES);
        epk_play(0);
        epk_render(l, r, 256);
        epk_bank_stage_begin(3);
        epk_bank_stage_half(3, 1, obj, (int)sizeof obj);
        epk_pattern_select(3, 12);
        epk_render(l, r, 256);
        CHECK(epk_bank_current() == 3 && epk_pattern_current() == 12, "bank D, pattern 13 (got %d, %d)",
              epk_bank_current(), epk_pattern_current() + 1);
        const char *nm = epk_pattern_name(3, 12);
        CHECK(nm[std::strlen(nm) - 1] == '*', "a saved pattern's name ends in '*' (got %s)", nm);
        CHECK(std::strchr(epk_pattern_name(3, 11), '*') == nullptr, "the slot beside it is not saved");
        for (int t = 0; t < EPK_TRACKS; t++) {
            CHECK(std::fabs(epk_param_get(t, EPK_P_TONE) - live[t][0]) < 1e-4f, "track %d tone %.4f, saved %.4f", t + 1,
                  epk_param_get(t, EPK_P_TONE), live[t][0]);
            CHECK(std::lround(epk_param_get(t, EPK_P_NOTE) * 60.0f) == std::lround(live[t][1] * 60.0f), "track %d note",
                  t + 1);                                            // one byte: the same note (la misma nota)
            CHECK(std::fabs(epk_param_get(t, EPK_P_CHARACTER) - live[t][2]) < 1e-4f, "track %d character", t + 1);
            CHECK(epk_pattern(t) == mask[t], "track %d steps %08x, saved %08x", t + 1, epk_pattern(t), mask[t]);
        }
        // a wrong object is refused (rechazado): another bank's, another version
        epk_bank_stage_begin(4);
        obj[4] = EPK_BANK_VERSION + 1;
        epk_bank_stage_half(4, 1, obj, (int)sizeof obj);
        epk_pattern_select(4, 12);
        epk_render(l, r, 256);
        CHECK(std::strchr(epk_pattern_name(4, 12), '*') == nullptr, "another version is not loaded");
        // kept in the current bank (en memoria): pattern 2 of bank E becomes what pattern 13 sounds like
        const float tone13 = epk_param_get(2, EPK_P_TONE);
        epk_pattern_keep(1);
        epk_render(l, r, 256);
        epk_pattern_select(4, 1);
        epk_render(l, r, 256);
        CHECK(std::strchr(epk_pattern_name(4, 1), '*') != nullptr, "kept: marked");
        CHECK(std::fabs(epk_param_get(2, EPK_P_TONE) - tone13) < 1e-4f, "kept: pattern 2 now has pattern 13's chord tone");
    }

    // 31. MACRO: track 4 plays it; each of the six models sounds, stays finite and under full scale, and its two
    //     knobs change the sound; the low-pass gate closes the note; a version 3 bank keeps its engines; banks B and
    //     C have names and sound; the drone no longer buries the chord (la mezcla)
    {
        epk_init(SR);
        CHECK(epk_engine(3) == EPK_ENG_MACRO, "track 4 plays MACRO (got %d)", epk_engine(3));
        auto play = [&](int model, float timbre, float harm, float tone, int note, double *bright) {
            epk_init(SR);
            for (int t = 0; t < EPK_TRACKS; t++) epk_solo(t, t == 3);
            for (int f = 0; f < EPK_FX_COUNT; f++)
                if (f == EPK_FX_DLY_LEVEL || f == EPK_FX_REV_LEVEL) epk_fx_set(f, 0.0f);
            epk_param_set(3, EPK_P_ENGINE, 1.0f);
            epk_param_set(3, EPK_P_EXTRA1, (float)model / (float)(EPK_MACRO_MODELS - 1));
            epk_param_set(3, EPK_P_CHARACTER, timbre);
            epk_param_set(3, EPK_P_EXTRA2, harm);
            epk_param_set(3, EPK_P_TONE, tone);
            epk_param_set(3, EPK_P_DECAY, 0.5f);
            epk_param_set(3, EPK_P_MUTATE, 0.0f);
            epk_param_set(3, EPK_P_LFO_AMT, 0.5f);
            epk_param_set(3, EPK_P_NOTE, (float)(note - 24) / 60.0f);
            epk_param_set(3, EPK_P_LENGTH, 15.0f / 31.0f);
            epk_param_set(3, EPK_P_HITS, 0.25f);
            epk_play(1);
            double e = 0.0, d = 0.0;
            float pk = 0.0f, prev = 0.0f;
            int bad = 0;
            for (int k = 0; k < (int)(4.0f * SR / 256); k++) {
                epk_render(l, r, 256);
                for (int i = 0; i < 256; i++) {
                    if (!std::isfinite(l[i]) || !std::isfinite(r[i])) bad++;
                    const float m = 0.5f * (l[i] + r[i]);
                    e += (double)m * m; d += (double)(m - prev) * (m - prev); prev = m;
                    pk = std::max(pk, std::fabs(l[i]));
                }
            }
            if (bright) *bright = d / (e + 1e-12);                   // more highs, larger (más agudos, mayor)
            CHECK(bad == 0 && pk < 1.0f, "MACRO %s note %d: %d bad samples, peak %.3f", epk_macro_name(model), note, bad, pk);
            return e;
        };
        for (int m = 0; m < EPK_MACRO_MODELS; m++) {
            double b0, b1, b2, bc;
            const double e0 = play(m, 0.2f, 0.3f, 0.8f, 38, &b0);
            const double e1 = play(m, 0.8f, 0.3f, 0.8f, 38, &b1);
            const double e2 = play(m, 0.2f, 0.8f, 0.8f, 38, &b2);
            double bo;
            play(m, 0.8f, 0.3f, 0.9f, 50, &bo);
            const double ec = play(m, 0.8f, 0.3f, 0.05f, 50, &bc);
            play(m, 0.5f, 0.5f, 0.6f, 24, nullptr);                  // the lowest and the highest note
            play(m, 1.0f, 1.0f, 1.0f, 84, nullptr);
            play(m, 0.0f, 0.0f, 0.0f, 60, nullptr);
            CHECK(e0 > 5.0 && e1 > 5.0 && e2 > 5.0, "MACRO %s sounds: %.1f %.1f %.1f", epk_macro_name(m), e0, e1, e2);
            CHECK(std::fabs(b1 / b0 - 1.0) > 0.05, "MACRO %s: TIMBRE changes it (%.4f -> %.4f)", epk_macro_name(m), b0, b1);
            CHECK(std::fabs(b2 / b0 - 1.0) > 0.05 || std::fabs(e2 / e0 - 1.0) > 0.05, "MACRO %s: ARMON changes it (%.4f -> %.4f)",
                  epk_macro_name(m), b0, b2);
            CHECK(bc < bo * 0.9, "MACRO %s: TONE down is darker (%.4f -> %.4f)", epk_macro_name(m), bo, bc);
            (void)ec;
        }
        // a bank saved with seven engines (version 3): ACORDE stays ACORDE, DRONE stays DRONE
        static uint8_t v3[EPK_BANK_OBJ_BYTES], v4[EPK_BANK_OBJ_BYTES];
        epk_init(SR);
        epk_render(l, r, 256);
        std::memset(v3, 0, sizeof v3);
        v3[0] = 'E'; v3[1] = 'P'; v3[2] = 'K'; v3[3] = 'B'; v3[4] = EPK_BANK_VERSION; v3[5] = 6; v3[6] = 0; v3[7] = 1;
        epk_pattern_pack_live(v3 + EPK_BANK_HEAD);
        int at = 4 * 2;                                              // NOTE one byte, three of two, then ENGINE
        at = 1 + 3 * 2;
        static const int kOld[EPK_TRACKS] = {1, 2, 5, 0, 6, 3};      // what version 3 would have stored, of 6
        for (int t = 0; t < EPK_TRACKS; t++)
            v3[EPK_BANK_HEAD + t * (EPK_PATTERN_PARAM_BYTES + 37) + at] = (uint8_t)((kOld[t] * 255 + 3) / 6);
        v3[4] = 3;
        const int n4 = epk_bank_upgrade(v3, (int)sizeof v3, v4, (int)sizeof v4);
        CHECK(n4 == EPK_BANK_OBJ_BYTES && v4[4] == EPK_BANK_VERSION, "version 3 converts (%d bytes)", n4);
        epk_init(SR);
        epk_bank_stage_begin(6);
        epk_bank_stage_half(6, 0, v4, n4);
        epk_pattern_select(6, 0);
        epk_render(l, r, 256);
        CHECK(epk_engine(0) == EPK_ENG_GRAVE && epk_engine(1) == EPK_ENG_METAL && epk_engine(2) == EPK_ENG_ACORDE &&
              epk_engine(3) == EPK_ENG_TEST && epk_engine(4) == EPK_ENG_DRONE && epk_engine(5) == EPK_ENG_GOLPE,
              "version 3 engines kept: %d %d %d %d %d %d", epk_engine(0), epk_engine(1), epk_engine(2), epk_engine(3),
              epk_engine(4), epk_engine(5));
        // banks B and C: named, and every pattern sounds
        CHECK(std::strcmp(epk_pattern_name(1, 0), "UMBRAL") == 0 && std::strcmp(epk_pattern_name(2, 15), "OCASO") == 0 &&
              std::strncmp(epk_pattern_name(3, 0), "GEN D", 5) == 0, "names: %s, %s, %s", epk_pattern_name(1, 0),
              epk_pattern_name(2, 15), epk_pattern_name(3, 0));
        for (int bank = 1; bank <= 2; bank++) {
            int quiet = 0, bad = 0, kicks = 0;
            float worst = 0.0f;
            for (int pt = 0; pt < EPK_PATTERNS; pt++) {
                epk_init(SR);
                epk_pattern_select(bank, pt);
                epk_play(1);
                float pk = 0.0f;
                for (int k = 0; k < (int)(8.0f * SR / 256); k++) {
                    epk_render(l, r, 256);
                    for (int i = 0; i < 256; i++) {
                        if (!std::isfinite(l[i])) bad++;
                        pk = std::max(pk, std::fabs(l[i]));
                    }
                }
                if (pk < 0.02f) quiet++;
                if (epk_param_get(0, EPK_P_HITS) > 0.0f) kicks++;
                worst = std::max(worst, pk);
            }
            CHECK(bad == 0 && quiet == 0 && worst < 1.0f, "bank %c: %d bad samples, %d silent patterns, peak %.3f", 'A' + bank,
                  bad, quiet, worst);
            CHECK(bank == 1 ? kicks >= 10 : kicks <= 4, "bank %c: %d patterns with a kick", 'A' + bank, kicks);
        }
        // the mix (ORBITA): the chord and the bass within 6 dB of the drone or above it
        double lvl[EPK_TRACKS];
        for (int t = 0; t < EPK_TRACKS; t++) {
            epk_init(SR);
            epk_pattern_select(0, 3);
            for (int k = 0; k < EPK_TRACKS; k++) epk_solo(k, k == t);
            epk_play(1);
            double e = 0.0;
            for (int k = 0; k < (int)(12.0f * SR / 256); k++) {
                epk_render(l, r, 256);
                for (int i = 0; i < 256; i++) e += (double)l[i] * l[i] + (double)r[i] * r[i];
            }
            lvl[t] = 10.0 * std::log10(e + 1e-12);
        }
        CHECK(lvl[2] > lvl[4] - 6.0 && lvl[3] > lvl[4] - 3.0 && lvl[0] > lvl[3],
              "mix: kick %.1f, chord %.1f, bass %.1f, drone %.1f dB", lvl[0], lvl[2], lvl[3], lvl[4]);
    }

    // 11. Relative cost of the engines (coste relativo): not the FM-1's CPU, only how they compare
    {
        static const char *const kName[EPK_ENG_COUNT] = {"TEST", "GRAVE", "METAL", "GOLPE", "TAMBOR", "ACORDE", "DRONE", "MACRO"};
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
