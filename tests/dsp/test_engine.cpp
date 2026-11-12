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
    for (int b = 0; b < (int)(4.0f * SR / 256); b++) {
        epk_render(l, r, 256);
        if (b > (int)(3.0f * SR / 256)) for (int i = 0; i < 256; i++) tail = std::fmax(tail, std::fabs(l[i]));
    }
    CHECK(tail < 0.001f, "3 s after stop the output is %.5f (expected silence)", tail);
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
