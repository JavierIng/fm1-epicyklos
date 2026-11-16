// SPDX-License-Identifier: GPL-3.0-only
// Gain staging check (comprobación de niveles): renders factory patterns and reports how hard the master soft clip
// is driven, and how much of the render time MAREA takes. Build and run (from the repo):
//   c++ -std=c++14 -O2 -fno-exceptions -fno-rtti -DPLAT_HOST -I app/dsp -I base/include tools/levels.cpp \
//       app/dsp/engine.cpp app/dsp/factory.cpp -o build/levels && ./build/levels
#include "epk.h"
#include <cmath>
#include <cstdio>
#include <ctime>

static float L[256], R[256];

struct Stat { double rms, peak, hot, ms; };

static Stat run(int bank, int pat, int marea_on, float seconds) {
    epk_init(44100.0f);
    epk_pattern_select(bank, pat);
    epk_render(L, R, 256);
    epk_fx_set(EPK_FX_ON_MAREA, marea_on ? 1.0f : 0.0f);
    epk_play(1);
    const int blocks = (int)(seconds * 44100.0f / 256.0f);
    double sum = 0, peak = 0, hot = 0, n = 0;
    clock_t t0 = clock();
    for (int b = 0; b < blocks; b++) {
        epk_render(L, R, 256);
        if (b < blocks / 4) continue;                       // skip the first quarter (arranque)
        for (int i = 0; i < 256; i++) {
            const double a = std::fabs(L[i]) > std::fabs(R[i]) ? std::fabs(L[i]) : std::fabs(R[i]);
            sum += 0.5 * ((double)L[i] * L[i] + (double)R[i] * R[i]);
            if (a > peak) peak = a;
            if (a > 0.6) hot += 1;                          // tanh(0.9x) > 0.6: x > 0.77, past ~2 dB of squash
            n += 1;
        }
    }
    const double ms = 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC;
    Stat s = { 10.0 * std::log10(sum / n + 1e-12), 20.0 * std::log10(peak + 1e-12), 100.0 * hot / n, ms };
    return s;
}

int main() {
    double t_on = 0, t_off = 0;
    printf("pattern   rms dBFS  peak dBFS  %%>0.6  | MAREA off: rms  peak  %%>0.6\n");
    for (int bank = 0; bank < 3; bank++)
        for (int p = 0; p < 16; p++) {
            Stat a = run(bank, p, 1, 12.0f), b = run(bank, p, 0, 12.0f);
            t_on += a.ms; t_off += b.ms;
            printf("%c%02d  %8.1f %9.1f %7.1f  | %13.1f %5.1f %6.1f\n", 'A' + bank, p + 1, a.rms, a.peak, a.hot,
                   b.rms, b.peak, b.hot);
        }
    printf("render time: MAREA on %.0f ms, off %.0f ms (MAREA = %.0f %% of the total)\n", t_on, t_off,
           100.0 * (t_on - t_off) / t_on);
    return 0;
}
