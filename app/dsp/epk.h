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
#define EPK_PATTERNS 16       /* patterns per bank (patrones por banco) */
#define EPK_BANKS 11          /* banks A..K (bancos): the black keys */
#define EPK_BAR_STEPS 16      /* a pattern change waits for the end of a bar of 16 sixteenths (compás) */

/* per-track parameters (parámetros por pista), each 0..1 at this interface. What NOTE..EXTRA2 do depends on the
 * engine (motor): see voices.h */
enum {
    EPK_P_NOTE,       /* note (nota): 0..1 -> MIDI 24..84 */
    EPK_P_CHARACTER,  /* TEST: sine/saw/noise. GRAVE: drive. METAL: noise..metal. GOLPE: clap..snare..rim */
    EPK_P_TONE,       /* TEST: cutoff. GRAVE: click. METAL: brightness */
    EPK_P_DECAY,      /* decay (caída) */
    EPK_P_ENGINE,     /* the engine (motor): EPK_ENG_* */
    EPK_P_EXTRA1,     /* GRAVE: pitch sweep (barrido). METAL: ring (resonancia) */
    EPK_P_EXTRA2,     /* GRAVE: rumble. METAL: vary (variación de la caída) */
    EPK_P_ATTACK,     /* attack (ataque): how slowly the sound comes in; the drum engines ignore it */
    EPK_P_LENGTH,     /* pattern length (longitud): 1..32 steps */
    EPK_P_HITS,       /* euclidean hits (golpes) */
    EPK_P_ROTATE,     /* rotation (rotación) */
    EPK_P_DIVISION,   /* step division (división): see EPK_DIV_* */
    EPK_P_SWING,      /* swing: the odd steps play late (los pasos impares se atrasan), up to 0.4 of a step */
    EPK_P_SHIFT,      /* the whole track early or late (adelanta o atrasa la pista): 0.5 = on the grid, +-0.45 step */
    EPK_P_PATTERN_COUNT, /* the parameters above belong to the pattern (al patrón); the ones below, to the project */
    EPK_P_LEVEL = EPK_P_PATTERN_COUNT,   /* level (nivel) */
    EPK_P_PAN,        /* pan (panorama): 0 left, 0.5 centre, 1 right */
    EPK_P_COUNT
};

/* engines (motores) */
enum { EPK_ENG_TEST, EPK_ENG_GRAVE, EPK_ENG_METAL, EPK_ENG_GOLPE, EPK_ENG_COUNT };

/* a step's chance (probabilidad) and condition (condición): N:M plays on the Nth of every M cycles */
enum { EPK_PROB_100, EPK_PROB_75, EPK_PROB_50, EPK_PROB_25, EPK_PROB_COUNT };
enum { EPK_COND_NONE, EPK_COND_1_2, EPK_COND_2_2, EPK_COND_1_4, EPK_COND_2_4, EPK_COND_3_4, EPK_COND_4_4,
       EPK_COND_COUNT };

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
int epk_engine(int track);                     /* EPK_ENG_* */
void epk_mute(int track, int on);
int epk_muted(int track);
void epk_solo(int track, int on);
int epk_soloed(int track);
int epk_audible(int track);                    /* after mute and solo (tras mute y solo) */

/* patterns (patrones): sound and rhythm of every track, 16 to a bank, banks A..K. The mix, mutes, solos and tempo
 * stay (son del proyecto). Playing, a change waits for the end of the bar; stopped, it is immediate. Edits stay in
 * the pattern while its bank is the current one (en memoria); bank A is the first project, B..K start as generated
 * patterns (saving banks to the flash: fase 6). */
void epk_pattern_select(int bank, int pattern);
int epk_pattern_current(void);
int epk_pattern_queued(void);                  /* -1 none (ninguno en cola) */
int epk_bank_current(void);
int epk_bank_queued(void);                     /* the queued pattern's bank, -1 none */
const char *epk_pattern_name(int bank, int pattern);   /* main loop only (a static buffer) */
int epk_bar_step(void);                        /* 0..15 within the bar, -1 stopped (paso del compás) */
void epk_pattern_randomize(uint32_t seed);     /* a whole new pattern in the current slot (aleatorio general) */

/* steps (pasos): a track's hits are euclidean (from HITS) until a step is edited by hand; then they are the
 * player's (manual). ROTATE turns both. Chance, condition and micro-timing belong to each step: micro is -3..3
 * eighths of a step, early or late (antes o después). SWING, SHIFT and micro add up, within +-0.9 of a step. */
void epk_step_toggle(int track, int step);
void epk_step_set(int track, int step, int prob, int cond, int micro);   /* EPK_PROB_*, EPK_COND_*, -3..3 */
int epk_step_prob(int track, int step);
int epk_step_cond(int track, int step);
int epk_step_micro(int track, int step);
int epk_steps_manual(int track);
void epk_steps_euclid(int track);              /* back to the euclidean hits (vuelve al euclidiano) */

/* one level of undo for the current pattern and the pans (deshacer): save before a change, restore after */
void epk_undo_save(void);
void epk_undo_restore(void);
int epk_undo_ready(void);

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
