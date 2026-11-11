// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Javier Sánchez
// Host tests of the DSP core (pruebas del núcleo DSP en el ordenador): timing to the sample, trigger counts,
// mute/solo, no NaN, levels. Writes tests/dsp/out/groove.wav to listen to.
//   c++ -std=c++14 -O2 -I app/dsp -I base/include tests/dsp/test_engine.cpp app/dsp/engine.cpp -o build/test_engine
#include <cmath>
#include <cstdio>
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

    std::printf(failures ? "dsp: %d FAILED\n" : "dsp: all passed\n", failures);
    return failures ? 1 : 0;
}
