/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Javier Sánchez */
/* EPICYKLOS DSP core: the C interface (interfaz C del núcleo DSP). The core is C++ (app/dsp, built as o2
 * units); the UI (app/app.c, C) talks to it only through these functions.
 * Threads (hilos): epk_render runs in the audio interrupt; everything else is called from the main loop. Parameter
 * writes are single 32-bit stores; notes and transport go through a small lock-free queue (cola sin bloqueo). */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EPK_TRACKS 6          /* tracks (pistas) */
#define EPK_MAX_STEPS 32      /* steps per pattern (pasos por patrón) */
#define EPK_PATTERNS 16       /* patterns per project (patrones por proyecto) */
#define EPK_BAR_STEPS 16      /* a pattern change waits for the end of a bar of 16 sixteenths (compás) */

/* per-track parameters (parámetros por pista), each 0..1 at this interface */
enum {
    EPK_P_NOTE,       /* base note (nota base): 0..1 -> MIDI 24..84 */
    EPK_P_CHARACTER,  /* test voice character (carácter): 0 kick-like sine, 0.5 saw, 1 noise */
    EPK_P_TONE,       /* low-pass cutoff (tono / corte) */
    EPK_P_DECAY,      /* amplitude decay (caída) */
    EPK_P_LENGTH,     /* pattern length (longitud): 1..32 steps */
    EPK_P_HITS,       /* euclidean hits (golpes) */
    EPK_P_ROTATE,     /* rotation (rotación) */
    EPK_P_DIVISION,   /* step division (división): see EPK_DIV_* */
    EPK_P_PATTERN_COUNT, /* the parameters above belong to the pattern (al patrón); the ones below, to the project */
    EPK_P_LEVEL = EPK_P_PATTERN_COUNT,   /* level (nivel) */
    EPK_P_PAN,        /* pan (panorama): 0 left, 0.5 centre, 1 right */
    EPK_P_COUNT
};

/* step divisions (divisiones), in order of EPK_P_DIVISION */
enum { EPK_DIV_1_32, EPK_DIV_1_16T, EPK_DIV_1_16, EPK_DIV_1_8T, EPK_DIV_1_8, EPK_DIV_1_4, EPK_DIV_COUNT };

void epk_init(float sample_rate);
void epk_render(float *left, float *right, uint32_t frames);   /* audio interrupt only (solo en el audio) */
void epk_panic(void);

/* transport and clock (transporte y reloj) */
void epk_play(int on);
int epk_playing(void);
void epk_set_tempo(float bpm);                 /* 40..200 */
float epk_tempo(void);

/* tracks (pistas) */
void epk_param_set(int track, int param, float value01);
float epk_param_get(int track, int param);
int epk_param_steps(int param);                /* >0: the parameter is stepped, with this many positions */
void epk_mute(int track, int on);
int epk_muted(int track);
void epk_solo(int track, int on);
int epk_soloed(int track);
int epk_audible(int track);                    /* after mute and solo (tras mute y solo) */

/* patterns (patrones): sound and rhythm of every track. The mix, mutes, solos and tempo stay (son del proyecto).
 * Playing, a change waits for the end of the bar; stopped, it is immediate. Edits stay in the pattern (en memoria). */
void epk_pattern_select(int pattern);
int epk_pattern_current(void);
int epk_pattern_queued(void);                  /* -1 none (ninguno en cola) */
const char *epk_pattern_name(int pattern);
int epk_bar_step(void);                        /* 0..15 within the bar, -1 stopped (paso del compás) */

/* the keys (teclado): a note on the track, now */
void epk_note_on(int track, int note, float velocity);
void epk_note_off(int track, int note);

/* what the screen shows (lo que muestra la pantalla) */
int epk_step(int track);                       /* the step playing, -1 stopped (paso actual) */
int epk_length(int track);
uint32_t epk_pattern(int track);               /* bit s: step s triggers (máscara de pasos) */
float epk_meter(int track);                    /* peak since the last call, 0..1 (medidor) */
float epk_meter_master(void);
uint32_t epk_triggers(int track);              /* triggers since boot: a counter for tests (contador) */

#ifdef __cplusplus
}
#endif
