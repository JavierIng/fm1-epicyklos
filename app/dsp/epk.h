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
    /* EVOLUTION (evolución): every EVERY bars the track may change a little and may rest. What changes: hits +-,
     * rotation, tone, decay, character (ACORDE: the chord) and, in the melodic engines, the note. It is an offset
     * over the pattern (desvío): the pattern itself is not touched; a pattern change or PLAY starts from it again */
    EPK_P_MUTATE,     /* how much it changes each time (cuánto muta): 0 nothing */
    EPK_P_EVERY,      /* every 1, 2, 4, 8 or 16 bars (cada cuántos compases) */
    EPK_P_RETURN,     /* how much it goes back towards the pattern each time (vuelta): 0 with MUTATE 0 = frozen */
    EPK_P_ABSENCE,    /* the chance that it rests for that stretch (ausencia) */
    EPK_P_SCOPE,      /* what MUTATE moves (qué muta): EPK_SCOPE_* */
    /* MOVEMENT (movimiento): the track's own LFO, locked to the bar while playing */
    EPK_P_LFO_SHAPE,  /* EPK_TLFO_SHAPE_*: the global LFO's six and DRIFT (deriva: a slow random walk) */
    EPK_P_LFO_RATE,   /* EPK_LFO_RATE_*: 1/16 .. 32 bars a cycle (DRIFT: how fast it wanders) */
    EPK_P_LFO_DEST,   /* EPK_TLFO_*: where it goes (destino) */
    EPK_P_LFO_AMT,    /* how much, 0.5 = none, below = upside down (cantidad bipolar) */
    EPK_P_PATTERN_COUNT, /* the parameters above belong to the pattern (al patrón); the ones below, to the project */
    EPK_P_LEVEL = EPK_P_PATTERN_COUNT,   /* level (nivel) */
    EPK_P_PAN,        /* pan (panorama): 0 left, 0.5 centre, 1 right */
    EPK_P_SEND_A,     /* send to the delay (envío A), after the fader */
    EPK_P_SEND_B,     /* send to the reverb (envío B), after the fader */
    EPK_P_COUNT
};

/* what the evolution moves (qué mueve la evolución): everything, the rhythm (hits, rotation), the sound (tone, decay,
 * character: in ACORDE the chord), the note */
enum { EPK_SCOPE_ALL, EPK_SCOPE_RHYTHM, EPK_SCOPE_SOUND, EPK_SCOPE_NOTE, EPK_SCOPE_COUNT };

/* engines (motores) */
enum { EPK_ENG_TEST, EPK_ENG_GRAVE, EPK_ENG_METAL, EPK_ENG_GOLPE, EPK_ENG_TAMBOR, EPK_ENG_ACORDE, EPK_ENG_DRONE,
       EPK_ENG_COUNT };
/* ACORDE's chord tables (Plaits Palette, MIT): names for the screen (nombres de tablas y acordes) */
int epk_chord_tables(void);
int epk_chord_count(int table);
const char *epk_chord_table_name(int table);
const char *epk_chord_name(int table, int chord);

/* the send effects (efectos de envío), the project's: bus A a delay, bus B the plate reverb PLACA. Each 0..1 */
enum {
    EPK_FX_DLY_TYPE,      /* EPK_DLY_* */
    EPK_FX_DLY_TIME,      /* 1..16 sixteenths (semicorcheas); halved until it fits 1.5 s (0.75 s in PINGPONG) */
    EPK_FX_DLY_FEEDBACK,  /* past 0.9 the delay feeds itself (auto-oscila), held by its saturation */
    EPK_FX_DLY_TONE,      /* the loop's low-pass: each repeat darker (cada repetición más oscura) */
    EPK_FX_DLY_WEAR,      /* wear (desgaste): TAPE wow, flutter, drive; BBD darker and noisier; PINGPONG softer */
    EPK_FX_DLY_TO_REV,    /* the delay into the reverb (el eco a la reverb) */
    EPK_FX_DLY_LEVEL,     /* return level (retorno) */
    EPK_FX_REV_DECAY,     /* how long the plate rings (cuánto dura) */
    EPK_FX_REV_TONE,      /* dark .. bright (oscura .. brillante) */
    EPK_FX_REV_MOD,       /* movement inside the tank (movimiento) */
    EPK_FX_REV_LEVEL,     /* return level (retorno) */
    EPK_FX_DUCK,          /* the kick (track 1) pushes both returns down (el bombo aparta eco y reverb) */
    /* the master (cadena del master), after the returns */
    EPK_FX_FLT,           /* DJ filter: < 0.5 low-pass closing, > 0.5 high-pass opening, 0.5 open (abierto) */
    EPK_FX_FLT_RESO,      /* its resonance */
    EPK_FX_FLT_DOUBLE,    /* a second, parallel peak above the cutoff (doble pico, como un filtro doble) */
    EPK_FX_COMP,          /* bus compressor amount: threshold, ratio and makeup in one (compresor) */
    EPK_FX_SAT,           /* tape saturation (saturación de cinta) */
    EPK_FX_FLT_TYPE,      /* EPK_FLT_*: what the master filter is (tipo de filtro) */
    /* MAREA (marea): a 16-band filterbank moved by a wave, inserted on one track (inspired by Hypnus Records' Slink;
     * the controls follow what its manual says they do, see marea.h) */
    EPK_FX_MAREA_TRACK,   /* 0 none, 1..6 the track it is on (la pista donde está) */
    EPK_FX_MAREA_MIX,     /* dry .. wet (seco .. mojado) */
    EPK_FX_MAREA_SPEED,   /* bipolar: 0.5 frozen, above forwards, below backwards (congelada, adelante, atrás) */
    EPK_FX_MAREA_SYNC,    /* 0 free (Hz), 1 locked to the tempo (al tempo) */
    EPK_FX_MAREA_MULTIPLY,/* how many wave cycles across the bands (ciclos) */
    EPK_FX_MAREA_RIPPLE,  /* a smaller sine ripple on the wave (rizo) */
    EPK_FX_MAREA_OFFSET,  /* the wave's phase, by hand (desfase) */
    EPK_FX_MAREA_INVERT,  /* depth, bipolar: 1 full, 0.5 none, 0 upside down (profundidad, invertida) */
    EPK_FX_MAREA_GRAVITY, /* pulls the wave down: wide valleys, thin crests (gravedad) */
    EPK_FX_MAREA_FROM,    /* the area where the wave acts: from .. (desde) */
    EPK_FX_MAREA_TO,      /* .. to; outside it the bands keep their level (hasta) */
    EPK_FX_MAREA_RESO,    /* wide .. narrow bands (bandas anchas .. finas) */
    EPK_FX_MAREA_WIDTH,   /* the bands alternate left / right; 0 mono (ancho estéreo) */
    EPK_FX_COUNT
};
enum { EPK_DLY_TAPE, EPK_DLY_BBD, EPK_DLY_PINGPONG, EPK_DLY_COUNT };

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

/* the send effects (efectos de envío) */
void epk_fx_set(int param, float value01);
float epk_fx_get(int param);
int epk_fx_steps(int param);                   /* >0: stepped (por pasos) */
float epk_fx_meter(void);                      /* the returns' peak since the last call (medidor de los retornos) */
/* dub gestures (gestos de dub). THROW: the track's send to the delay goes to full while on. FREEZE: the delay stops
 * listening and loops what it holds, without losing it (congelar). Both are the project's and are not saved. */
/* The master filter's types (tipos). DJ: FLT below the middle closes a low-pass, above it opens a high-pass. LP, HP,
 * BP, NOTCH, MORPH: FLT is the cutoff, 30 Hz .. 19 kHz; in MORPH, FLT_DOUBLE morphs LP -> BP -> HP. */
enum { EPK_FLT_DJ, EPK_FLT_LP, EPK_FLT_HP, EPK_FLT_BP, EPK_FLT_NOTCH, EPK_FLT_MORPH, EPK_FLT_COUNT };
/* MAREA for the screen (para la pantalla): a band's level now (0..~1), where a band and an edge sit (0..1 across the
 * screen), and the wave's speed in cycles a second (signed; 0 frozen) */
float epk_marea_band(int b);
float epk_marea_x(float hz);
float epk_marea_hz(int band);
float epk_marea_edge_hz(float v01);
float epk_marea_speed(void);
/* the cutoff of the continuous types for the screen (corte, para la pantalla) */
float epk_master_cutoff(void);

/* modulation (modulación), the project's: sources 0-1 are LFOs locked to the bar, 2-3 envelope followers. Each has two
 * destinations with a bipolar amount (0.5 = none). Settings 0..1 like every parameter. */
#define EPK_MOD_SOURCES 4
enum {
    EPK_M_SHAPE,          /* LFO: EPK_LFO_*. Follower: the track it follows (0..5) */
    EPK_M_RATE,           /* LFO: EPK_LFO_RATE_* (1/16 .. 32 bars). Follower: release 20 ms .. 1.3 s */
    EPK_M_DEST1, EPK_M_AMT1, EPK_M_DEST2, EPK_M_AMT2,
    EPK_M_COUNT
};
enum { EPK_LFO_SINE, EPK_LFO_TRIANGLE, EPK_LFO_SAW, EPK_LFO_SQUARE, EPK_LFO_SH, EPK_LFO_RANDOM, EPK_LFO_SHAPE_COUNT };
#define EPK_LFO_RATE_COUNT 10

/* the track LFO (LFO de pista): its shapes, the global LFO's plus DRIFT, and its destinations */
enum { EPK_TLFO_SHAPE_DRIFT = EPK_LFO_SHAPE_COUNT, EPK_TLFO_SHAPE_COUNT };
enum { EPK_TLFO_TONE, EPK_TLFO_DECAY, EPK_TLFO_CHARACTER, EPK_TLFO_EXTRA1, EPK_TLFO_EXTRA2, EPK_TLFO_LEVEL,
       EPK_TLFO_PAN, EPK_TLFO_SEND_A, EPK_TLFO_SEND_B, EPK_TLFO_HITS, EPK_TLFO_ROTATE, EPK_TLFO_DEST_COUNT };
float epk_track_lfo(int track);                /* its output now with the amount, -1..1 (salida) */

enum {
    EPK_DEST_NONE, EPK_DEST_FLT, EPK_DEST_FLT_RESO, EPK_DEST_DLY_TIME, EPK_DEST_DLY_FEEDBACK, EPK_DEST_DLY_TONE,
    EPK_DEST_DLY_LEVEL, EPK_DEST_REV_DECAY, EPK_DEST_REV_LEVEL,
    EPK_DEST_MAREA_SPEED, EPK_DEST_MAREA_MULTIPLY, EPK_DEST_MAREA_RIPPLE, EPK_DEST_MAREA_OFFSET, EPK_DEST_MAREA_GRAVITY,
    EPK_DEST_MAREA_MIX,
    EPK_DEST_TONE1,                                   /* + track: a track's TONE (tono de la pista) */
    EPK_DEST_LEVEL1 = EPK_DEST_TONE1 + EPK_TRACKS,    /* + track: a track's LEVEL */
    EPK_DEST_COUNT = EPK_DEST_LEVEL1 + EPK_TRACKS
};
void epk_mod_set(int src, int param, float value01);
float epk_mod_get(int src, int param);
int epk_mod_steps(int src, int param);
float epk_mod_out(int src);                    /* what the source puts out now: LFO -1..1, follower 0..1 */
float epk_fx_live(int param);                  /* an effect's value with the modulation on it (con modulación) */
float epk_param_live(int track, int param);    /* the same for a track's parameter */

float epk_comp_reduction(void);                /* the compressor's deepest reduction in dB since the last call */
/* the master filter's state for the screen: 0 open, 1 low-pass, 2 high-pass; *hz its cutoff */
int epk_master_filter(float *hz);
void epk_throw(int track, int on);
int epk_thrown(int track);
void epk_freeze(int on);
int epk_frozen(void);

/* patterns (patrones): sound and rhythm of every track, 16 to a bank, banks A..K. The mix, mutes, solos and tempo
 * stay (son del proyecto). Playing, a change waits for the end of the bar; stopped, it is immediate. Edits stay in
 * the pattern while its bank is the current one (en memoria); bank A is the first project, B..K start as generated
 * patterns (saving banks to the flash: fase 6). */
void epk_pattern_select(int bank, int pattern);
int epk_pattern_current(void);
int epk_pattern_queued(void);                  /* -1 none (ninguno en cola) */
int epk_bank_current(void);
int epk_bank_queued(void);                     /* the queued pattern's bank, -1 none */
const char *epk_pattern_name(int bank, int pattern);   /* main loop only (a static buffer); a saved one ends in '*' */
int epk_every_bars(int track);                 /* EVERY in bars (compases) */
int epk_absent(int track);                     /* resting this stretch (en ausencia) */
/* evolution on or off per track (the project's, like mute): off, the track goes back to its pattern at the next bar
 * and stays there; its knobs are kept. Hold: every track stays as it is now (congelar todo) until released */
void epk_evolve_enable(int track, int on);
int epk_evolve_enabled(int track);
void epk_evolve_hold(int on);
/* the whole-pattern random (aleatorio general): a new minor key each time, unless locked (tonalidad fija) */
void epk_random_key_lock(int on);
int epk_random_key_locked(void);
int epk_key(void);                             /* the key in use: 0 = C .. 11 = B (A and factory: 2, D) */
int epk_evolve_held(void);

/* Saving (guardar): a pattern as it sounds now (with its evolution) goes to a slot of any bank. The core packs it;
 * the UI keeps it in the store (plat_store_*: the flash on the device, the page's storage in the emulator) as two
 * objects a bank, eight slots each. A slot saved overrides the factory or generated one when its bank loads.
 * Record: per track the pattern's parameters (see below) then steps: mask u32 LE, manual u8, meta u8 x 32 */
/* from version 3 a parameter with fixed steps takes one byte (u8 of 255), the rest two (u16 LE); versions 1-2: all
 * two. EPK_PATTERN_PARAM_BYTES: their sum for a track (checked in the core) */
#define EPK_PATTERN_PARAM_BYTES 35
#define EPK_PATTERN_BYTES ((EPK_PATTERN_PARAM_BYTES + 37) * EPK_TRACKS)
#define EPK_BANK_HEAD 8                        /* "EPKB", version, bank, half, slot mask */
#define EPK_BANK_OBJ_BYTES (EPK_BANK_HEAD + 8 * EPK_PATTERN_BYTES)
#define EPK_BANK_VERSION 3                      /* 1: without SCOPE; 2: without the track LFO; both still read */
/* an object of an older version, rewritten as this one (convertido): bytes written to out, 0 if not convertible */
int epk_bank_upgrade(const uint8_t *in, int len, uint8_t *out, int max);
void epk_pattern_pack_live(uint8_t *out);      /* EPK_PATTERN_BYTES: what plays now, its evolution included */
void epk_pattern_keep(int pattern);            /* the same, into that slot of the current bank (en memoria) */
/* before a bank loads: what the store holds for it. stage_begin, then each half's object as loaded, then the
 * bank's change (epk_pattern_select). epk_bank_restage: at boot, the current bank again with what was staged */
void epk_bank_stage_begin(int bank);
void epk_bank_stage_half(int bank, int half, const uint8_t *obj, int len);
void epk_bank_restage(void);
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

/* one level of undo for the current pattern, the pans and the sends (deshacer): save before a change, restore after */
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
